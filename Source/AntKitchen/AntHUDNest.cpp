#include "AntHUD.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"

// The nest between raids: one screen, drawn on the Canvas, worked with the mouse (and [Enter]).
// Left: what the last raid brought, the Queen and her orders, the brood. Middle: the six branches. Right: the next raid -
// the threat, this player's squad and pockets, one-raid boosts, who is ready. Everything a button does goes to the server
// as AAntPlayerController::ServerRaid; the screen only shows what the replicated AAntRaid says.

FVector2D AAntHUD::DevClickAt(0.f, 0.f);
int32 AAntHUD::DevClickFrames = 0;

namespace
{
	const FLinearColor NGold(1.f, 0.8f, 0.2f);
	const FLinearColor NText(0.93f, 0.91f, 0.86f);
	const FLinearColor NDim(0.64f, 0.61f, 0.55f);
	const FLinearColor NGood(0.45f, 0.95f, 0.45f);
	const FLinearColor NBad(1.f, 0.42f, 0.32f);
	const FLinearColor NJelly(0.78f, 0.62f, 1.f);
	const FLinearColor NBack(0.006f, 0.005f, 0.004f, 0.98f);
	const FLinearColor NPanel(0.022f, 0.017f, 0.012f, 1.f);
	const FLinearColor NRow(0.036f, 0.028f, 0.019f, 1.f);
	const FLinearColor NRowHot(0.075f, 0.056f, 0.032f, 1.f);
	const FLinearColor NBtn(0.17f, 0.105f, 0.02f, 1.f);
	const FLinearColor NBtnHot(0.36f, 0.23f, 0.04f, 1.f);
	const FLinearColor NBtnOn(0.05f, 0.2f, 0.045f, 1.f);
	const FLinearColor NBtnOff(0.03f, 0.029f, 0.027f, 1.f);
	const FLinearColor NEdge(0.3f, 0.21f, 0.07f, 1.f);

	const TCHAR* RomanN(int32 N)
	{
		static const TCHAR* R[] = { TEXT("0"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV") };
		return R[FMath::Clamp(N, 0, 4)];
	}
}

bool AAntHUD::NestHover(float X, float Y, float BW, float BH) const
{
	return NestMouse.X >= X && NestMouse.X < X + BW && NestMouse.Y >= Y && NestMouse.Y < Y + BH;
}

bool AAntHUD::NestBtn(const FString& Label, float X, float Y, float BW, float BH, bool bEnabled, const FString& Tip, bool bOn, float TextScale)
{
	const bool bHot = NestHover(X, Y, BW, BH);
	const FLinearColor Fill = !bEnabled ? NBtnOff : bOn ? (bHot ? FLinearColor(0.09f, 0.32f, 0.07f, 1.f) : NBtnOn) : bHot ? NBtnHot : NBtn;
	DrawRect(bEnabled ? NEdge : FLinearColor(0.07f, 0.065f, 0.06f, 1.f), X, Y, BW, BH);
	DrawRect(Fill, X + 2.f, Y + 2.f, BW - 4.f, BH - 4.f);
	UFont* F = GEngine->GetSmallFont();
	float TW = 0.f, TH = 0.f;
	float Sc = TextScale * NestFs;
	GetTextSize(Label, TW, TH, F, Sc);
	if (TW > BW - 10.f) { Sc *= (BW - 10.f) / TW; GetTextSize(Label, TW, TH, F, Sc); }
	Txt(Label, X + BW * 0.5f, Y + (BH - TH) * 0.5f, bEnabled ? NText : NDim, F, Sc, true);
	if (bHot && !Tip.IsEmpty()) { NestTip = Tip; }
	return bHot && bEnabled && bNestClick;
}

float AAntHUD::WrapTxt(const FString& Str, float X, float Y, float MaxW, const FLinearColor& C, UFont* F, float Scale, int32 MaxLines)
{
	TArray<FString> Words;
	Str.ParseIntoArray(Words, TEXT(" "));
	FString Line;
	float LH = 0.f, TW = 0.f;
	GetTextSize(TEXT("Ау"), TW, LH, F, Scale);
	LH *= 1.12f;
	int32 Lines = 0;
	for (int32 i = 0; i <= Words.Num(); ++i)
	{
		const FString Try = i < Words.Num() ? (Line.IsEmpty() ? Words[i] : Line + TEXT(" ") + Words[i]) : Line;
		float W2 = 0.f, H2 = 0.f;
		GetTextSize(Try, W2, H2, F, Scale);
		if (i == Words.Num() || (W2 > MaxW && !Line.IsEmpty()))
		{
			if (!Line.IsEmpty())
			{
				const bool bLast = Lines + 1 >= MaxLines && i < Words.Num();
				Txt(bLast ? Line + TEXT("...") : Line, X, Y + Lines * LH, C, F, Scale);
				++Lines;
				if (bLast) { break; }
			}
			Line = i < Words.Num() ? Words[i] : FString();
		}
		else
		{
			Line = Try;
		}
	}
	return Lines * LH;
}

void AAntHUD::DrawNest(AAntRaid* Raid, AAntGameState* GS)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner);
	const AAntPlayerState* MyPS = PC ? PC->GetPlayerState<AAntPlayerState>() : nullptr;
	const FAntNestState& N = Raid->Nest;
	const bool bHost = GetWorld()->GetNetMode() != NM_Client;
	const float Now = GetWorld()->GetTimeSeconds();

	// A 1920x1080 sheet, fitted into the screen. Text is never smaller than 0.72 of its design size.
	const float U = FMath::Min(W / 1920.f, H / 1080.f);
	const float OX = (W - 1920.f * U) * 0.5f, OY = (H - 1080.f * U) * 0.5f;
	NestFs = FMath::Max(0.72f, U);
	const float Fs = NestFs;
	auto PX = [&](float X) { return OX + X * U; };
	auto PY = [&](float Y) { return OY + Y * U; };

	// The mouse. (Tests without a window click through "-AntDo nest click X Y".)
	float MX = -1.f, MY = -1.f;
	bNestClick = false;
	if (PC)
	{
		PC->GetMousePosition(MX, MY);
		bNestClick = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
	}
	if (DevClickAt.X > 0.f || DevClickAt.Y > 0.f)
	{
		MX = DevClickAt.X * W;
		MY = DevClickAt.Y * H;
		if (DevClickFrames > 0) { bNestClick = (DevClickFrames == 2); --DevClickFrames; }
	}
	NestMouse = FVector2D(MX, MY);
	NestTip.Reset();
	auto Send = [&](uint8 Cmd, int32 A, int32 B) { if (PC) { PC->ServerRaid(Cmd, A, B); } };
	auto Panel = [&](float X, float Y, float PW, float PH, const TCHAR* Title)
	{
		DrawRect(NPanel, PX(X), PY(Y), PW * U, PH * U);
		DrawRect(NEdge, PX(X), PY(Y), PW * U, 3.f * U);
		Txt(Title, PX(X + 16.f), PY(Y + 12.f), NGold, Small, 1.3f * Fs);
	};
	const TCHAR* HostOnly = TEXT("Колония хоста: строит и решает он");

	DrawRect(NBack, 0.f, 0.f, W, H);

	// ------------------------------------------------------------------ the top: the stores
	Txt(TEXT("ГНЕЗДО"), PX(34.f), PY(14.f), NGold, Big, 1.75f * Fs);
	Txt(FString::Printf(TEXT("рейдов %d  ·  тортов дома %d  ·  лучшая добыча %d"), N.Raids, N.CakesHome, N.BestHaul), PX(38.f), PY(70.f), NDim, Small, 1.1f * Fs);
	{
		struct FChip { const TCHAR* Label; FString Value; FLinearColor C; const TCHAR* Tip; };
		const FChip Chips[4] =
		{
			{ TEXT("ЕДА"), FString::Printf(TEXT("%d"), N.Food), NGold, TEXT("Еда: всё, что донесли до входа. На неё строят, растят личинок и рабочих, берут инструменты") },
			{ TEXT("МОЛОЧКО"), FString::Printf(TEXT("%d"), N.Jelly), NJelly, TEXT("Маточное молочко: торт, целые трофеи, особые находки, приказы Королевы. На него строят II и III ярусы") },
			{ TEXT("ЛИЧИНКИ"), FString::Printf(TEXT("%d / %d"), N.Larvae, AntNest::MaxLarvae), N.Larvae > 1 ? NText : NBad, TEXT("Личинки — общие жизни в рейде: каждая смерть игрока тратит одну") },
			{ TEXT("РАБОЧИЕ"), FString::Printf(TEXT("%d / %d"), N.Workers, AntNest::MaxWorkers), NText, TEXT("Рабочие идут в отряды игроков. Кто не вернулся из рейда — того больше нет") },
		};
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = 600.f + 250.f * i;
			DrawRect(NPanel, PX(X), PY(14.f), 232.f * U, 76.f * U);
			Txt(Chips[i].Label, PX(X + 14.f), PY(20.f), NDim, Small, 1.05f * Fs);
			Txt(Chips[i].Value, PX(X + 14.f), PY(42.f), Chips[i].C, Med, 1.6f * Fs);
			if (NestHover(PX(X), PY(14.f), 232.f * U, 76.f * U)) { NestTip = Chips[i].Tip; }
		}
	}
	if (bHost)
	{
		const bool bArmed = Now - NewColonyArmedAt < 4.f;
		if (NestBtn(bArmed ? TEXT("Точно? Ещё раз") : TEXT("Новая колония"), PX(1680.f), PY(30.f), 206.f * U, 44.f * U, true,
			TEXT("Начать колонию с нуля. Нынешняя останется на диске рядом с сохранением как colony.old"), false, 1.1f))
		{
			if (bArmed) { Send(AntRaidCmd::NewColony, 7777, 0); NewColonyArmedAt = -100.f; }
			else { NewColonyArmedAt = Now; }
		}
	}

	// ------------------------------------------------------------------ left: the last raid
	Panel(30.f, 104.f, 590.f, 226.f, TEXT("ПРОШЛЫЙ РЕЙД"));
	{
		const FAntRaidResult& R = Raid->Result;
		float Y = 146.f;
		if (R.RaidNo <= 0)
		{
			WrapTxt(N.Raids > 0 ? TEXT("Колония ждёт. Выберите угрозу, соберите отряд — и в рейд.")
				: TEXT("Колония только основана. Задача рейда — найти кусок именинного торта и донести его до входа в гнездо. Всё, что вынесли из гнезда (отряд, инструменты), на кону: не вернулись — пропало."),
				PX(46.f), PY(Y), 560.f * U, NText, Small, 1.15f * Fs, 6);
		}
		else
		{
			const bool bWiped = R.End == AntRaidEnd::Wiped;
			static const TCHAR* CakeText[] = { TEXT("остался в квартире"), TEXT("бросили по дороге"), TEXT("остался у красных"), TEXT("дома"), TEXT("съеден в красном холме"), TEXT("потерян"), TEXT("разбит") };
			Txt(FString::Printf(TEXT("Рейд %d, угроза %s: %s"), R.RaidNo, RomanN(R.Tier), R.End == AntRaidEnd::Left ? TEXT("ушли с добычей") : bWiped ? TEXT("отряд погиб") : TEXT("время вышло")),
				PX(46.f), PY(Y), R.End == AntRaidEnd::Left ? NGood : NBad, Small, 1.25f * Fs);
			Y += 32.f;
			Txt(FString::Printf(TEXT("Еда +%d (Королева ждала %d)   Молочко +%d"), R.Kept, R.Ask, R.Jelly), PX(46.f), PY(Y), NText, Small, 1.15f * Fs);
			Y += 27.f;
			Txt(FString::Printf(TEXT("Торт: %s"), (R.Cake == AntCake::Home && bWiped) ? TEXT("был дома — растащили") : CakeText[FMath::Min<int32>(R.Cake, 6)]), PX(46.f), PY(Y),
				(R.Cake == AntCake::Home && !bWiped) ? NGood : NBad, Small, 1.15f * Fs);
			Y += 27.f;
			Txt(FString::Printf(TEXT("Погибло %d   Отряды: вернулось %d из %d"), R.Deaths + R.Lost, R.SquadBack, R.SquadOut), PX(46.f), PY(Y), NText, Small, 1.15f * Fs);
			Y += 27.f;
			const int32 D = R.MoodAfter - R.MoodBefore;
			Txt(FString::Printf(TEXT("Королева %s%d: +%d личинок, +%d рабочих%s"), D >= 0 ? TEXT("+") : TEXT(""), D, R.LarvaeLaid, R.WorkersHatched,
				R.QueenAte > 0 ? *FString::Printf(TEXT(", съела %d еды"), R.QueenAte) : TEXT("")), PX(46.f), PY(Y), D >= 0 ? NText : NBad, Small, 1.15f * Fs);
			Y += 27.f;
			if (R.NewTier > 0) { Txt(FString::Printf(TEXT("Открыта угроза %s «%s»"), RomanN(R.NewTier), AntTier::Def(R.NewTier).Name), PX(46.f), PY(Y), NGold, Small, 1.15f * Fs); }
		}
	}

	// ------------------------------------------------------------------ left: the Queen and her orders
	Panel(30.f, 340.f, 590.f, 372.f, TEXT("КОРОЛЕВА"));
	{
		const int32 Band = AntNest::MoodBand(N.Mood);
		const FLinearColor MoodC = Band >= 2 ? NGood : Band == 1 ? NGold : NBad;
		Txt(FString::Printf(TEXT("%s (%d из 100)"), AntNest::MoodName(N.Mood), N.Mood), PX(190.f), PY(352.f), MoodC, Small, 1.25f * Fs);
		const float BX = PX(46.f), BY = PY(388.f), BW = 558.f * U, BH = 16.f * U;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), BX, BY, BW, BH);
		DrawRect(MoodC, BX + 2.f, BY + 2.f, (BW - 4.f) * N.Mood / 100.f, BH - 4.f);
		for (const int32 Mark : { 26, 60, 85 }) { DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f), BX + BW * Mark / 100.f, BY - 3.f * U, 2.f, BH + 6.f * U); }
		static const TCHAR* BandText[] =
		{
			TEXT("В ярости: после рейда съест 15% еды и не отложит ни одной личинки. Несите торт и разное"),
			TEXT("Спокойна: после рейда +1 рабочий. Довольная (60) кладёт личинки и разрешает III ярус построек"),
			TEXT("Довольна: после рейда +1 личинка и +3 рабочих. III ярус построек открыт"),
			TEXT("В восторге: после рейда +2 личинки и +6 рабочих"),
		};
		WrapTxt(BandText[Band], PX(46.f), PY(414.f), 558.f * U, NDim, Small, 1.05f * Fs, 2);
		if (NestHover(PX(30.f), PY(340.f), 590.f * U, 124.f * U))
		{
			NestTip = TEXT("Настроение растёт от торта дома, перевыполненного запроса, разной еды, целых трофеев, особых находок и выполненных приказов. Падает от смертей, от потерянного торта и от гибели отряда");
		}
		Txt(bHost ? TEXT("ПРИКАЗЫ НА РЕЙД — возьмите до двух") : TEXT("ПРИКАЗЫ НА РЕЙД — выбирает хост"), PX(46.f), PY(472.f), NGold, Small, 1.12f * Fs);
		const int32 Bonus = FMath::RoundToInt(40.f * Raid->TierDef().LootMul);
		for (int32 i = 0; i < Raid->ContractOffer.Num() && i < 3; ++i)
		{
			const float RY = 502.f + 68.f * i;
			const bool bOn = (Raid->ContractPick & (1 << i)) != 0;
			const bool bHot = NestHover(PX(42.f), PY(RY), 566.f * U, 62.f * U);
			DrawRect(bOn ? FLinearColor(0.03f, 0.1f, 0.03f, 1.f) : bHot ? NRowHot : NRow, PX(42.f), PY(RY), 566.f * U, 62.f * U);
			DrawRect(NEdge, PX(54.f), PY(RY + 17.f), 28.f * U, 28.f * U);
			DrawRect(bOn ? NGood : FLinearColor(0.03f, 0.03f, 0.03f, 1.f), PX(58.f), PY(RY + 21.f), 20.f * U, 20.f * U);
			Txt(AntOrder::Name(Raid->ContractOffer[i]), PX(96.f), PY(RY + 7.f), bOn ? NText : NDim, Small, 1.18f * Fs);
			Txt(FString::Printf(TEXT("+1 молочко, +%d еды, Королева +4"), Bonus), PX(96.f), PY(RY + 34.f), bOn ? NJelly : NDim, Small, 1.0f * Fs);
			if (bHot)
			{
				NestTip = bHost ? TEXT("Приказ Королевы: выполните в рейде — получите награду. Не выполните — ничего не теряете, кроме её настроения") : HostOnly;
				if (bNestClick && bHost) { Send(AntRaidCmd::Contract, i, 0); }
			}
		}
	}

	// ------------------------------------------------------------------ left: the brood
	Panel(30.f, 722.f, 590.f, 178.f, TEXT("ВЫВОДОК"));
	{
		const bool bL = bHost && N.Food >= AntNest::LarvaCost && N.Larvae < AntNest::MaxLarvae;
		const bool bW = bHost && N.Food >= AntNest::WorkerPackCost && N.Workers < AntNest::MaxWorkers;
		if (NestBtn(FString::Printf(TEXT("Вырастить личинку — %d еды"), AntNest::LarvaCost), PX(46.f), PY(766.f), 274.f * U, 50.f * U, bL,
			bHost ? TEXT("Личинка — ещё одна жизнь на всех в рейде. Смерть с нулём личинок — навсегда до конца рейда") : HostOnly, false, 1.1f)) { Send(AntRaidCmd::Larva, 0, 0); }
		if (NestBtn(FString::Printf(TEXT("+%d рабочих — %d еды"), AntNest::WorkerPack, AntNest::WorkerPackCost), PX(332.f), PY(766.f), 274.f * U, 50.f * U, bW,
			bHost ? TEXT("Рабочие для отрядов. Отряд несёт, держит двери, строит мосты и дерётся — и гибнет, если не вернулся его командир") : HostOnly, false, 1.1f)) { Send(AntRaidCmd::Workers, 0, 0); }
		WrapTxt(TEXT("Личинки — жизни на рейд. Рабочие — ваши отряды. И то и другое растёт из еды, а довольная Королева добавляет после каждого рейда."),
			PX(46.f), PY(828.f), 558.f * U, NDim, Small, 1.02f * Fs, 3);
	}

	// ------------------------------------------------------------------ middle: the six branches
	Panel(636.f, 104.f, 660.f, 796.f, TEXT("ПОСТРОЙКИ"));
	Txt(TEXT("каждый ярус меняет следующий рейд"), PX(820.f), PY(118.f), NDim, Small, 1.0f * Fs);
	for (int32 b = 0; b < AntUp::Count; ++b)
	{
		const float RY = 150.f + 124.f * b;
		const int32 Have = N.Tier_((uint8)b);
		const bool bHot = NestHover(PX(646.f), PY(RY), 640.f * U, 118.f * U);
		DrawRect(bHot ? NRowHot : NRow, PX(646.f), PY(RY), 640.f * U, 118.f * U);
		Txt(AntUp::BranchName((uint8)b), PX(660.f), PY(RY + 8.f), NGold, Small, 1.3f * Fs);
		for (int32 t = 0; t < AntUp::MaxTier; ++t)
		{
			DrawRect(NEdge, PX(858.f + 30.f * t), PY(RY + 13.f), 22.f * U, 18.f * U);
			DrawRect(t < Have ? NGood : FLinearColor(0.03f, 0.03f, 0.03f, 1.f), PX(860.f + 30.f * t), PY(RY + 15.f), 18.f * U, 14.f * U);
		}
		if (Have > 0)
		{
			const AntUp::FDef& D = AntUp::Def((uint8)b, Have);
			float TW = 0.f, TH = 0.f;
			const FString Line = FString::Printf(TEXT("Есть: %s"), D.Name);
			GetTextSize(Line, TW, TH, Small, 1.0f * Fs);
			Txt(Line, PX(960.f), PY(RY + 12.f), NGood, Small, 1.0f * Fs * FMath::Min(1.f, 318.f * U / FMath::Max(1.f, TW)));
		}
		if (Have < AntUp::MaxTier)
		{
			const AntUp::FDef& D = AntUp::Def((uint8)b, Have + 1);
			FString Why;
			const bool bCan = AntNest::CanBuy(N, (uint8)b, &Why);
			Txt(FString::Printf(TEXT("%s. %s"), RomanN(Have + 1), D.Name), PX(660.f), PY(RY + 42.f), NText, Small, 1.15f * Fs);
			WrapTxt(D.Text, PX(660.f), PY(RY + 68.f), 440.f * U, NDim, Small, 1.0f * Fs, 2);
			const FString Cost = D.Jelly > 0 ? FString::Printf(TEXT("%d еды + %d мол."), D.Food, D.Jelly) : FString::Printf(TEXT("%d еды"), D.Food);
			if (NestBtn(Cost, PX(1114.f), PY(RY + 40.f), 162.f * U, 46.f * U, bCan && bHost,
				!bHost ? FString(HostOnly) : bCan ? FString::Printf(TEXT("Построить «%s»: %s"), D.Name, D.Text) : FString::Printf(TEXT("«%s»: %s"), D.Name, *Why), false, 1.1f))
			{
				Send(AntRaidCmd::Buy, b, 0);
			}
			if (!bCan) { Txt(Why, PX(1195.f), PY(RY + 90.f), NBad, Small, 0.92f * Fs, true); }
		}
		else
		{
			Txt(TEXT("Построено всё"), PX(660.f), PY(RY + 52.f), NGood, Small, 1.15f * Fs);
			WrapTxt(AntUp::Def((uint8)b, AntUp::MaxTier).Text, PX(660.f), PY(RY + 78.f), 600.f * U, NDim, Small, 1.0f * Fs, 1);
		}
		if (bHot && NestTip.IsEmpty())
		{
			FString All;
			for (int32 t = 1; t <= AntUp::MaxTier; ++t)
			{
				const AntUp::FDef& D = AntUp::Def((uint8)b, t);
				All += FString::Printf(TEXT("%s%s. %s: %s"), t > 1 ? TEXT("   ") : TEXT(""), RomanN(t), D.Name, D.Text);
			}
			NestTip = All;
		}
	}

	// ------------------------------------------------------------------ right: the threat
	Panel(1312.f, 104.f, 578.f, 232.f, TEXT("УГРОЗА"));
	{
		for (int32 t = 1; t <= AntTier::Max; ++t)
		{
			const bool bOpen = t <= N.MaxTier;
			const FString Tip = !bOpen ? FString::Printf(TEXT("Закрыто: принесите торт домой на угрозе %s (ещё %d)"), RomanN(N.MaxTier), FMath::Max(1, AntTier::CakesToUnlock(N.MaxTier) - N.CakesAtMax))
				: !bHost ? FString(HostOnly) : FString::Printf(TEXT("%s: %s"), AntTier::Def(t).Name, AntTier::Def(t).Text);
			if (NestBtn(RomanN(t), PX(1470.f + 104.f * (t - 1)), PY(112.f), 94.f * U, 40.f * U, bOpen && (bHost || t == Raid->Tier), Tip, t == Raid->Tier, 1.25f) && bHost && t != Raid->Tier)
			{
				Send(AntRaidCmd::Tier, t, 0);
			}
		}
		const AntTier::FDef& T = Raid->TierDef();
		Txt(FString::Printf(TEXT("%s. «%s»"), RomanN(Raid->Tier), T.Name), PX(1328.f), PY(162.f), NText, Small, 1.3f * Fs);
		const float Used = WrapTxt(T.Text, PX(1328.f), PY(194.f), 546.f * U, NDim, Small, 1.05f * Fs, 2);
		const int32 Reds = T.Scouts + T.Haulers + T.Escorts + T.Ambushers;
		float Y = 194.f + Used / U + 6.f;
		Txt(FString::Printf(TEXT("Рейд %d:%02d  ·  торт %d еды и %d мол.  ·  еда ×%.1f"), (int32)T.Length / 60, (int32)T.Length % 60, T.CakeValue, T.CakeJelly, T.LootMul), PX(1328.f), PY(Y), NGold, Small, 1.05f * Fs);
		Y += 25.f;
		Txt(FString::Printf(TEXT("Человек выходит через %d с  ·  красных %d  ·  находок %d"), (int32)T.Quiet, Reds, T.Specials), PX(1328.f), PY(Y), NText, Small, 1.05f * Fs);
		if (N.MaxTier < AntTier::Max)
		{
			Y += 25.f;
			Txt(FString::Printf(TEXT("До угрозы %s: торт домой на %s, ещё %d"), RomanN(N.MaxTier + 1), RomanN(N.MaxTier), FMath::Max(1, AntTier::CakesToUnlock(N.MaxTier) - N.CakesAtMax)), PX(1328.f), PY(Y), NDim, Small, 1.0f * Fs);
		}
	}

	// ------------------------------------------------------------------ right: my squad and my pockets
	Panel(1312.f, 346.f, 578.f, 252.f, TEXT("МОЙ ОТРЯД И КАРМАНЫ"));
	if (MyPS)
	{
		const int32 Cap = AntNest::SquadCap(N);
		const int32 Want = MyPS->SquadWant == 255 ? Cap : FMath::Min<int32>(MyPS->SquadWant, Cap);
		const int32 Get = FMath::Min(Want, N.Workers);
		Txt(TEXT("Отряд"), PX(1328.f), PY(396.f), NText, Small, 1.2f * Fs);
		if (NestBtn(TEXT("-"), PX(1420.f), PY(388.f), 44.f * U, 40.f * U, Want > 0, TEXT("Меньше рабочих — меньше потеряете, но слабее отряд"), false, 1.4f)) { Send(AntRaidCmd::Squad, Want - 1, 0); }
		Txt(FString::Printf(TEXT("%d"), Want), PX(1500.f), PY(390.f), NGold, Med, 1.5f * Fs, true);
		if (NestBtn(TEXT("+"), PX(1536.f), PY(388.f), 44.f * U, 40.f * U, Want < Cap, TEXT("Больше рабочих: сильнее несут, дольше держатся. Все на кону"), false, 1.4f)) { Send(AntRaidCmd::Squad, Want + 1, 0); }
		Txt(FString::Printf(TEXT("из %d   сила %d"), Cap, FMath::RoundToInt(Get * AntNest::WorkerStrength(N))), PX(1596.f), PY(396.f), NDim, Small, 1.1f * Fs);
		Txt(Get < Want ? FString::Printf(TEXT("В гнезде только %d рабочих: пойдут %d"), N.Workers, Get) : TEXT("Погиб командир и тело не донесли — отряд пропал"), PX(1328.f), PY(436.f), Get < Want ? NBad : NDim, Small, 1.0f * Fs);

		for (int32 p = 0; p < AntTool::Pockets; ++p)
		{
			const uint8 Tool = p == 0 ? MyPS->Tool0 : MyPS->Tool1;
			const float BX = 1328.f + 278.f * p;
			// A click takes the next tool the nest has unlocked (or empties the pocket).
			uint8 Next = Tool;
			for (int32 k = 0; k < AntTool::Count; ++k)
			{
				Next = (uint8)((Next + 1) % AntTool::Count);
				if (Next == AntTool::None || AntNest::ToolUnlocked(N, Next)) { break; }
			}
			const bool bAny = N.Tier_(AntUp::Tools) >= 1;
			const FString Label = Tool == AntTool::None ? FString::Printf(TEXT("[%d] пусто"), 5 + p) : FString::Printf(TEXT("[%d] %s — %d"), 5 + p, AntTool::Name(Tool), AntTool::Cost(Tool));
			const FString Tip = !bAny ? FString(TEXT("Инструменты открывает ветка «Арсенал»"))
				: Tool == AntTool::None ? FString(TEXT("Карман пуст. Клик — положить инструмент (2 заряда; нетронутый карман вернётся в кладовую)"))
				: FString::Printf(TEXT("%s: %s. 2 заряда за %d еды; нетронутый карман вернётся в кладовую. Клик — сменить"), AntTool::Name(Tool), AntTool::Text(Tool), AntTool::Cost(Tool));
			if (NestBtn(Label, PX(BX), PY(474.f), 268.f * U, 46.f * U, bAny, Tip, Tool != AntTool::None, 1.05f)) { Send(AntRaidCmd::Tool, p, Next); }
			if (Tool != AntTool::None) { WrapTxt(AntTool::Text(Tool), PX(BX + 4.f), PY(528.f), 262.f * U, NDim, Small, 0.95f * Fs, 3); }
		}
		if (N.Tier_(AntUp::Tools) < 1) { Txt(TEXT("Карманы пусты, пока не построен «Арсенал»"), PX(1328.f), PY(532.f), NDim, Small, 1.0f * Fs); }
	}

	// ------------------------------------------------------------------ right: one-raid boosts
	Panel(1312.f, 608.f, 578.f, 150.f, TEXT("НА ЭТОТ РЕЙД"));
	for (int32 m = 0; m < AntMod::Count; ++m)
	{
		const bool bOn = (Raid->Mods & (1 << m)) != 0;
		const float BX = 1328.f + 278.f * (m % 2), BY = 648.f + 52.f * (m / 2);
		if (NestBtn(FString::Printf(TEXT("%s — %d"), AntMod::Name((uint8)m), AntMod::Cost((uint8)m)), PX(BX), PY(BY), 268.f * U, 44.f * U, bHost || bOn,
			bHost ? FString::Printf(TEXT("%s: %s. %d еды, на один рейд"), AntMod::Name((uint8)m), AntMod::Text((uint8)m), AntMod::Cost((uint8)m)) : FString(HostOnly), bOn, 1.05f) && bHost)
		{
			Send(AntRaidCmd::Mod, m, 0);
		}
	}

	// ------------------------------------------------------------------ right: who goes
	Panel(1312.f, 768.f, 578.f, 132.f, TEXT("КТО ИДЁТ"));
	{
		int32 Row = 0;
		for (APlayerState* P : GS->PlayerArray)
		{
			const AAntPlayerState* A = Cast<AAntPlayerState>(P);
			if (!A || A->bIsHuman || A->IsABot() || Row >= 4) { continue; }
			const int32 Cap = AntNest::SquadCap(N);
			const int32 Want = A->SquadWant == 255 ? Cap : FMath::Min<int32>(A->SquadWant, Cap);
			const float X = 1328.f + 278.f * (Row % 2), Y = 806.f + 42.f * (Row / 2);
			DrawRect(A->bNestReady ? NGood : FLinearColor(0.3f, 0.3f, 0.3f, 1.f), PX(X), PY(Y + 6.f), 14.f * U, 14.f * U);
			float TW = 0.f, TH = 0.f;
			const FString Line = FString::Printf(TEXT("%s · отряд %d%s"), *A->GetPlayerName(), Want, (A->Tool0 || A->Tool1) ? TEXT(" · с инструм.") : TEXT(""));
			GetTextSize(Line, TW, TH, Small, 1.08f * Fs);
			Txt(Line, PX(X + 22.f), PY(Y), A->bNestReady ? NText : NDim, Small, 1.08f * Fs * FMath::Min(1.f, 244.f * U / FMath::Max(1.f, TW)));
			++Row;
		}
	}

	// ------------------------------------------------------------------ the bottom: what the mouse is on, and GO
	DrawRect(NPanel, PX(30.f), PY(912.f), 1266.f * U, 140.f * U);
	if (!NestTip.IsEmpty())
	{
		WrapTxt(NestTip, PX(46.f), PY(926.f), 1236.f * U, NText, Small, 1.15f * Fs, 4);
	}
	else
	{
		if (!Raid->NestNote.IsEmpty()) { Txt(Raid->NestNote, PX(46.f), PY(924.f), NBad, Small, 1.1f * Fs); }
		WrapTxt(TEXT("Рейд: вынести торт и всё, что успеете, до входа в гнездо. Считается только донесённое. Уход — удерживать [L] у входа. Красный клан со двора тоже идёт за тортом: унесут в холм — есть минуты, чтобы отбить. Наведите мышь на что угодно — здесь будет объяснение."),
			PX(46.f), PY(Raid->NestNote.IsEmpty() ? 926.f : 954.f), 1236.f * U, NDim, Small, 1.1f * Fs, 4);
	}
	{
		const float Left = Raid->StartEnds > 0.f ? Raid->StartEnds - GS->GetServerWorldTimeSeconds() : -1.f;
		const bool bReady = MyPS && MyPS->bNestReady;
		const FString Label = Left >= 0.f ? FString::Printf(TEXT("ВЫХОДИМ ЧЕРЕЗ %d"), FMath::Max(0, FMath::CeilToInt(Left)))
			: bHost ? FString(TEXT("В РЕЙД  [Enter]")) : bReady ? FString(TEXT("ГОТОВ — ждём хоста")) : FString(TEXT("ГОТОВ  [Enter]"));
		const bool bGo = NestBtn(Label, PX(1312.f), PY(912.f), 578.f * U, 104.f * U, true,
			bHost ? TEXT("Начать рейд. С несколькими игроками: 3 с, если все готовы, иначе 20 с. Ещё раз — отменить") : TEXT("Отметить, что готов. Рейд начинает хост"), bReady && !bHost, 1.9f);
		const bool bEnter = PC && PC->WasInputKeyJustPressed(EKeys::Enter);
		if (bGo || bEnter)
		{
			if (bHost) { Send(AntRaidCmd::Ready, 1, 0); Send(AntRaidCmd::Start, 0, 0); }
			else { Send(AntRaidCmd::Ready, bReady ? 0 : 1, 0); }
		}
		Txt(Raid->StartCost > 0 ? FString::Printf(TEXT("У двери спишется %d еды (усиления и инструменты)%s"), Raid->StartCost, Raid->StartCost > N.Food ? TEXT(" — не хватает, лишнее останется дома") : TEXT(""))
			: FString(TEXT("Усилений и инструментов не взято: у двери ничего не спишется")), PX(1601.f), PY(1024.f), Raid->StartCost > N.Food ? NBad : NDim, Small, 1.0f * Fs, true);
	}

	// The mouse of a test without a window: a mark where it "is".
	if (DevClickAt.X > 0.f || DevClickAt.Y > 0.f)
	{
		DrawRect(FLinearColor::White, MX - 9.f, MY - 1.f, 18.f, 3.f);
		DrawRect(FLinearColor::White, MX - 1.f, MY - 9.f, 3.f, 18.f);
	}
}
