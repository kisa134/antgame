#include "AntHUD.h"
#include "AntTutorial.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "AntRaid.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "AntArmy.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "AntTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

namespace
{
	const FLinearColor QWhite(1.f, 1.f, 1.f);
	const FLinearColor QGrey(0.72f, 0.72f, 0.72f);
	const FLinearColor QDim(0.5f, 0.5f, 0.5f);
	const FLinearColor QGold(1.f, 0.8f, 0.2f);
	const FLinearColor QRed(1.f, 0.3f, 0.25f);
	const FLinearColor QGreen(0.45f, 1.f, 0.45f);
	const FLinearColor QFire(1.f, 0.4f, 0.2f);
	const FLinearColor QJelly(0.85f, 0.65f, 1.f);
	const FLinearColor QPanel(0.03f, 0.025f, 0.02f, 0.86f);

	const TCHAR* Roman(int32 N)
	{
		static const TCHAR* R[] = { TEXT("0"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V") };
		return R[FMath::Clamp(N, 0, 5)];
	}
}

// ---------------------------------------------------------------------------------------------
// The top of the screen during a raid: the clock, what has been banked against what the Queen asked for,
// the larvae, what the house is doing, and one line about the cake.
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawRaidTop(AAntRaid* Raid, AAntGameState* GS)
{
	if (!Raid->InRaid() || GS->Phase != 0)
	{
		SeenShift = -1;   // the next raid announces itself again
		return;
	}
	const float W = Canvas->ClipX;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Dt = GetWorld()->GetDeltaSeconds();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();

	const int32 Score = GS->TeamScore, Target = FMath::Max(1, GS->TargetScore), Larvae = GS->Larvae;
	const int32 Stage = FMath::Min<int32>(GS->ShiftStage, 3);
	const int32 TimeLeft = FMath::Max(0, FMath::CeilToInt(GS->TimeRemaining));
	const bool bAsk = Score >= Target;

	// --- What changed since the last frame: the big lines ---
	if (SeenShift != Raid->RaidNo)
	{
		SeenShift = Raid->RaidNo;
		SeenStage = Stage;
		SeenScore = Score;
		SeenLarvae = Larvae;
		SeenCake = Raid->Cake;
		if (Stage == 0)
		{
			Announce(FString::Printf(TEXT("РЕЙД %d"), Raid->RaidNo), TEXT("Цель — кусок именинного торта. Уйти домой: удерживай [L] у входа в гнездо"), QGold);
		}
	}
	if (Stage != SeenStage)
	{
		SeenStage = Stage;
		if (Stage == 1) { Announce(TEXT("ЧЕЛОВЕК НА КУХНЕ"), TEXT("Теперь осторожно: он топчет всё, что шевелится"), FLinearColor(1.f, 0.55f, 0.25f)); }
		else if (Stage == 2) { Announce(TEXT("ВЕСЬ ДОМ НА НОГАХ!"), TEXT("Человек с дихлофосом, кошка, Бадрик, бабушка и малыш ищут вас. Пора домой"), QRed); }
		else if (Stage == 3) { Announce(TEXT("ПОСЛЕДНИЕ 30 СЕКУНД"), TEXT("Кто не в гнезде, когда выйдет время, — пропал вместе с грузом"), QGold); }
	}
	if (Raid->Cake != SeenCake)
	{
		SeenCake = Raid->Cake;
		switch (Raid->Cake)
		{
		case AntCake::Ours: Announce(TEXT("ТОРТ У НАС"), TEXT("Несите к гнезду или к южному входу. Уронить с высоты — разбить"), QGold); break;
		case AntCake::Clan: Announce(TEXT("КРАСНЫЕ ТАЩАТ ТОРТ!"), TEXT("Отбейте, пока не унесли в свой холм"), QFire); break;
		case AntCake::Home: Announce(TEXT("ТОРТ ДОМА!"), TEXT("Можно уходить. Остаться за добавкой — жадность растёт, дом злее"), QGold); break;
		case AntCake::InHill: Announce(TEXT("ТОРТ В КРАСНОМ ХОЛМЕ"), TEXT("Его едят. Набег на холм во дворе — и он снова ваш"), QFire); break;
		case AntCake::Eaten: Announce(TEXT("ТОРТ ПОТЕРЯН"), TEXT("Берите, что осталось, и уходите живыми"), QRed); break;
		case AntCake::Broken: Announce(TEXT("ТОРТ РАЗБИТ"), TEXT("Куски — обычная еда. Королева будет недовольна"), QRed); break;
		default: break;
		}
	}
	if (SeenScore >= 0 && Score > SeenScore)
	{
		ScoreGain = Score - SeenScore;
		GainFromFrac = FMath::Clamp(SeenScore / (float)Target, 0.f, 1.f);
		ScorePulse = 1.f;
	}
	SeenScore = Score;
	if (SeenLarvae >= 0 && Larvae < SeenLarvae)
	{
		LarvaPop = 1.f;
		LarvaPopCount = SeenLarvae - Larvae;
	}
	SeenLarvae = Larvae;
	ScorePulse = FMath::Max(0.f, ScorePulse - Dt * 1.3f);
	LarvaPop = FMath::Max(0.f, LarvaPop - Dt * 1.6f);

	// --- Row 1: the clock, the raid, the jelly ---
	const float CX = W * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.42f), CX - 330.f * S, 0.f, 660.f * S, 150.f * S);
	const bool bLastCall = TimeLeft <= 30;
	const float ClockPulse = bLastCall ? 1.f + 0.12f * FMath::Abs(FMath::Sin(Now * 6.f)) : 1.f;
	Txt(FString::Printf(TEXT("%d:%02d"), TimeLeft / 60, TimeLeft % 60), CX, 6.f * S, (Stage >= 2) ? QRed : QWhite, Med, 1.45f * S * ClockPulse, true);
	TxtRight(FString::Printf(TEXT("РЕЙД %d  ·  УГРОЗА %s"), Raid->RaidNo, Roman(Raid->Tier)), CX - 76.f * S, 13.f * S, QGold, Small, 1.2f * S);
	Txt(FString::Printf(TEXT("МОЛОЧКО +%d"), Raid->RaidJelly), CX + 76.f * S, 13.f * S, Raid->RaidJelly > 0 ? QJelly : QDim, Small, 1.15f * S);

	// --- Row 2: banked against what the Queen asked for ---
	const float Frac = FMath::Clamp(Score / (float)Target, 0.f, 1.f);
	const float Kick = FMath::Square(ScorePulse);
	const float Grow = Kick * 5.f * S;
	const float BW = 440.f * S + Grow * 2.f, BH = 22.f * S + Grow * 2.f;
	const float BX = CX - BW * 0.5f, BY = 42.f * S - Grow;
	const FLinearColor Base = bAsk ? FLinearColor(0.5f, 1.f, 0.45f) : QGold;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), BX - 2.f, BY - 2.f, BW + 4.f, BH + 4.f);
	DrawRect(FMath::Lerp(Base, QWhite, Kick * 0.6f), BX, BY, BW * Frac, BH);
	if (ScorePulse > 0.f && Frac > GainFromFrac)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, ScorePulse), BX + BW * GainFromFrac, BY, BW * (Frac - GainFromFrac), BH);
	}
	{
		const FString Line = bAsk ? FString::Printf(TEXT("В ГНЕЗДЕ  %d  ·  запрос Королевы закрыт"), Score) : FString::Printf(TEXT("В ГНЕЗДЕ  %d  ·  Королева ждёт %d"), Score, Target);
		if (Frac > 0.42f)
		{
			float QW = 0.f, QH = 0.f;
			GetTextSize(Line, QW, QH, Small, 1.1f * S);
			DrawText(Line, FLinearColor(0.05f, 0.04f, 0.02f), CX - QW * 0.5f, BY + BH * 0.5f - 10.f * S, Small, 1.1f * S);
		}
		else
		{
			Txt(Line, CX, BY + BH * 0.5f - 10.f * S, QWhite, Small, 1.1f * S, true);
		}
	}
	if (ScorePulse > 0.f && ScoreGain > 0)
	{
		Txt(FString::Printf(TEXT("+%d"), ScoreGain), BX + BW + 14.f * S, BY - (1.f - ScorePulse) * 22.f * S, FLinearColor(1.f, 0.9f, 0.35f, FMath::Min(1.f, ScorePulse * 2.f)), Med, 1.25f * S);
	}

	// --- Row 3: larvae = lives of the whole team; they come out of the nest's own stock ---
	const float LY = 84.f * S;
	auto Egg = [this](float X, float Y, float K, const FLinearColor& C)
	{
		DrawRect(C, X - 3.f * S * K, Y - 7.f * S * K, 6.f * S * K, 14.f * S * K);
		DrawRect(C, X - 5.f * S * K, Y - 4.5f * S * K, 10.f * S * K, 9.f * S * K);
	};
	if (Larvae <= 0)
	{
		Txt(TEXT("ЛИЧИНОК НЕТ — погибший не вернётся, пока тело не принесут в гнездо"), CX, LY - 9.f * S, QRed, Small, 1.1f * S, true);
	}
	else
	{
		const int32 Shown = FMath::Clamp(Larvae, 0, 12);
		const float Gap = 17.f * S;
		float LW = 0.f, LH = 0.f;
		const FString Label = TEXT("ЛИЧИНКИ");
		GetTextSize(Label, LW, LH, Small, 1.05f * S);
		const float Total = LW + 10.f * S + Shown * Gap;
		float X = CX - Total * 0.5f;
		Txt(Label, X, LY - 9.f * S, QGrey, Small, 1.05f * S);
		X += LW + 10.f * S + Gap * 0.5f;
		for (int32 i = 0; i < Shown; ++i)
		{
			Egg(X + i * Gap, LY, 1.f, Larvae == 1 ? QRed : FLinearColor(1.f, 0.93f, 0.72f));
		}
		if (LarvaPop > 0.f)
		{
			for (int32 i = 0; i < FMath::Min(LarvaPopCount, 3) && Shown + i < 13; ++i)
			{
				Egg(X + (Shown + i) * Gap, LY - (1.f - LarvaPop) * 6.f * S, 1.f + (1.f - LarvaPop) * 1.6f, FLinearColor(1.f, 0.25f, 0.2f, LarvaPop));
			}
		}
	}

	// --- Row 4: the house ---
	static const TCHAR* StageName[] = { TEXT("Человек залип в телефон: дом спит"), TEXT("Человек на кухне"), TEXT("ВЕСЬ ДОМ НА НОГАХ: пора домой"), TEXT("ПОСЛЕДНИЕ СЕКУНДЫ: в гнездо!") };
	FString Line = StageName[Stage];
	if (Stage >= 1 && Stage < 3 && GS->HumanGoal != AntHumanGoal::Idle && !GS->HumanGoalText.IsEmpty())
	{
		Line = FString::Printf(TEXT("%s: %s"), Stage == 2 ? TEXT("ВЕСЬ ДОМ НА НОГАХ. Человек") : TEXT("Человек"), *GS->HumanGoalText);
	}
	if (GS->HumanRage > 0 && Stage < 2)
	{
		Line += FString::Printf(TEXT("  ·  ярость %d/3"), GS->HumanRage);
	}
	Txt(Line, CX, 104.f * S, Stage == 3 ? QGold : Stage == 2 ? QRed : Stage == 1 ? FLinearColor(1.f, 0.7f, 0.45f) : QGrey, Small, 1.15f * S, true);

	// --- Row 5: the cake ---
	FString CakeLine;
	FLinearColor CakeColor = QGold;
	const float ServerNow = GS->GetServerWorldTimeSeconds();
	switch (Raid->Cake)
	{
	case AntCake::Lying:
		CakeLine = Raid->bCakeKnown ? FString::Printf(TEXT("ТОРТ: %s"), *Raid->CakeHint) : FString(TEXT("ТОРТ: ищите — журнальный стол, тумба ТВ или ящик холодильника"));
		break;
	case AntCake::Ours: CakeLine = TEXT("ТОРТ У НАС: к гнезду или к южному входу"); break;
	case AntCake::Clan: CakeLine = TEXT("ТОРТ ТАЩАТ КРАСНЫЕ — отбейте!"); CakeColor = QFire; break;
	case AntCake::Home: CakeLine = TEXT("ТОРТ ДОМА  ·  [L] у входа — уйти"); CakeColor = QGreen; break;
	case AntCake::InHill:
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(Raid->CakeWindowEnds - ServerNow));
		CakeLine = FString::Printf(TEXT("ТОРТ В КРАСНОМ ХОЛМЕ: осталось %d%%, доедят через %d:%02d"), (int32)Raid->CakeLeft, Left / 60, Left % 60);
		CakeColor = QFire;
		break;
	}
	case AntCake::Eaten: CakeLine = TEXT("Торт потерян"); CakeColor = QDim; break;
	default: CakeLine = TEXT("Торт разбит: куски — обычная еда"); CakeColor = QDim; break;
	}
	Txt(CakeLine, CX, 126.f * S, CakeColor, Small, 1.15f * S, true);
}

// ---------------------------------------------------------------------------------------------
// Everything else of the raid: the cake's mark, leaving, the results, the nest
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawRaid(AAntRaid* Raid, AAntGameState* GS, APawn* Me)
{
	if (!Raid || !GS || !Canvas)
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Dt = GetWorld()->GetDeltaSeconds();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner);
	const AAntCharacter* Ant = Cast<AAntCharacter>(Me);
	const float ServerNow = GS->GetServerWorldTimeSeconds();

	// The nest screen takes the mouse; a raid gives it back. (The Esc menu has its own input mode: wait for it to close.)
	if (PC && !PC->MenuWidget.IsValid())
	{
		const bool bWantUi = Raid->Phase == AntRaidPhase::Nest;
		if (bWantUi && (!bNestUi || !PC->bShowMouseCursor))
		{
			bNestUi = true;
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
			NestFrozenPawn = nullptr;
		}
		else if (!bWantUi && bNestUi)
		{
			bNestUi = false;
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
			if (Me) { Me->EnableInput(PC); }
			NestFrozenPawn = nullptr;
		}
		if (bNestUi && Me && NestFrozenPawn.Get() != Me)
		{
			NestFrozenPawn = Me;   // the ant stands in the nest while its player looks at the screen
			Me->DisableInput(PC);
		}
	}

	if (Raid->Phase == AntRaidPhase::Nest)
	{
		DrawNest(Raid, GS);
		return;
	}
	if (Raid->Phase == AntRaidPhase::Results)
	{
		DrawRaidResults(Raid, GS);
		return;
	}
	if (!Ant || Ant->Team != 0 || AntTut::OwnsRaid(Raid))
	{
		return;   // (the training draws its own)
	}

	// --- Where the cake is, once the team knows ---
	if (Raid->bCakeKnown && (Raid->Cake == AntCake::Lying || Raid->Cake == AntCake::Clan || Raid->Cake == AntCake::InHill) && !Ant->bDead)
	{
		const FVector At = FVector(Raid->CakeLoc) + FVector(0.f, 0.f, 140.f);
		if (FVector::Dist(At, Ant->GetActorLocation()) > 1600.f)   // closer than that the trophy's own label does the talking
		{
			const int32 Metres = FMath::RoundToInt(FVector::Dist(At, Ant->GetActorLocation()) / 1900.f * 10.f);
			Marker(At, FString::Printf(TEXT("%s  %d.%d м"), Raid->Cake == AntCake::Lying ? TEXT("ТОРТ") : Raid->Cake == AntCake::Clan ? TEXT("ТОРТ У КРАСНЫХ") : TEXT("ТОРТ В ХОЛМЕ"), Metres / 10, Metres % 10),
				Raid->Cake == AntCake::Lying ? QGold : QFire, true);
		}
	}

	// --- My squad: how many are left, what they are doing, the keys ---
	{
		const APlayerState* MyPS = PlayerOwner ? PlayerOwner->PlayerState : nullptr;
		const AAntArmy* Sq = nullptr;
		for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
		{
			if (It->bSquad && (It->Commander == Ant || (MyPS && It->OwnerPS == MyPS))) { Sq = *It; break; }
		}
		const float SX = 30.f * S, SY = H - 232.f * S;
		if (Sq && !Ant->bDead)
		{
			FString What;
			FLinearColor C = QGold;
			if (Sq->bOrphan) { What = TEXT("ждёт у твоего тела: донеси тело в гнездо"); C = QRed; }
			else if (Sq->Cargo && Sq->bCargoHeld) { What = FString::Printf(TEXT("несут «%s» %s"), *Sq->Cargo->LootName, Sq->bCargoHome ? TEXT("домой сами") : TEXT("рядом с тобой")); C = QGreen; }
			else if (Sq->Cargo) { What = FString::Printf(TEXT("идут за «%s»"), *Sq->Cargo->LootName); }
			else if (Sq->Mode == EArmyMode::Attack) { What = TEXT("В АТАКЕ"); C = QFire; }
			else if (Sq->Mode == EArmyMode::Hold) { What = TEXT("держат место"); }
			else if (Sq->bForaging) { What = TEXT("вольно: ищут еду"); }
			else { What = TEXT("за тобой"); }
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), SX - 8.f * S, SY - 6.f * S, 470.f * S, 60.f * S);
			Txt(FString::Printf(TEXT("ОТРЯД %d"), Sq->Count), SX, SY, C, Med, 0.95f * S);
			Txt(What, SX + 150.f * S, SY + 7.f * S, C, Small, 1.15f * S);
			Txt(TEXT("[Z] за мной  [X] фас  [C] вольно  [Q] нести  [B] держать  [G] мост"), SX, SY + 32.f * S, FLinearColor(1.f, 1.f, 1.f, 0.6f), Small, 0.98f * S);
			// Where they are, when they are not at my side.
			if (FVector::Dist(Sq->GetActorLocation(), Ant->GetActorLocation()) > 450.f)
			{
				Marker(Sq->GetActorLocation() + FVector(0.f, 0.f, 50.f), FString::Printf(TEXT("ОТРЯД %d"), Sq->Count), FLinearColor(C.R, C.G, C.B, 0.9f), true);
			}
			if (Sq->Mode == EArmyMode::Attack)
			{
				Marker(FVector(Sq->Target) + FVector(0.f, 0.f, 40.f), TEXT("цель отряда"), QFire, false);
			}
		}
		else if (Sq && Ant->bDead)
		{
			Txt(FString::Printf(TEXT("Отряд: %d рабочих ждут у твоего тела"), Sq->Count), W * 0.5f, H * 0.36f + 124.f * S, QGold, Small, 1.15f * S, true);
		}
		else if (!Ant->bDead)
		{
			Txt(TEXT("ОТРЯДА НЕТ"), SX, SY + 8.f * S, QDim, Small, 1.1f * S);
		}
	}

	// --- The rival clan: what it is up to, and the ambush tell ---
	if (Raid->ClanAlert != SeenClan)
	{
		if (Raid->ClanAlert == 1 && SeenClan == 0) { Announce(TEXT("КРАСНЫЕ ВЫШЛИ НА РАЗВЕДКУ"), TEXT("Их холм во дворе за гостиной. Они тоже ищут торт"), QFire); }
		else if (Raid->ClanAlert == 2 && SeenClan < 2) { Announce(TEXT("КРАСНЫЕ НАШЛИ ТОРТ"), TEXT("Их отряд идёт за ним: успейте раньше или отбейте по дороге"), QFire); }
		SeenClan = Raid->ClanAlert;
	}
	if (Raid->AmbushState != SeenAmbush)
	{
		SeenAmbush = Raid->AmbushState;
		if (SeenAmbush == 3 && !Ant->bDead)
		{
			Announce(TEXT("ДОЗОРНЫЕ: ВПЕРЕДИ ЗАСАДА"), TEXT("Красные сидят у отмеченного места. Обойдите — или ударьте первыми"), QFire);
		}
		else if (!Ant->bDead && FVector::Dist(FVector(Raid->AmbushLoc), Ant->GetActorLocation()) < 4500.f)
		{
			if (SeenAmbush == 1)
			{
				Announce(TEXT("ШОРОХ ВПЕРЕДИ"), TEXT("Красные в засаде: сейчас кинутся. Бросай груз и дерись — или назад"), QFire);
				AntSfx::Play2D(this, AntSfx::Hiss, 0.7f, 0.8f);
			}
			else if (SeenAmbush == 2)
			{
				Announce(TEXT("ЗАСАДА!"), TEXT("Они бьют рывком и отнимают груз. Отряд на них — [X]"), QRed);
			}
		}
	}
	if (Raid->AmbushState != 0 && !Ant->bDead)
	{
		const float Beat = 0.6f + 0.4f * FMath::Abs(FMath::Sin(Now * 7.f));
		Marker(FVector(Raid->AmbushLoc), Raid->AmbushState == 1 ? TEXT("ШОРОХ: ЗАСАДА") : Raid->AmbushState == 3 ? TEXT("ЗАСАДА (дозорные)") : TEXT("ЗАСАДА КРАСНЫХ"),
			FLinearColor(1.f, 0.35f, 0.15f, Raid->AmbushState == 3 ? 0.85f : Beat), true);
	}

	// --- Special finds: a mark from a few steps away ---
	if (Raid->SpecialsLeft > 0 && !Ant->bDead)
	{
		if (Now - SpecialScanAt > 1.f)
		{
			SpecialScanAt = Now;
			SpecialFinds.Reset();
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				if (It->Special != 0) { SpecialFinds.Add(*It); }
			}
		}
		for (const TWeakObjectPtr<AKitchenLoot>& F : SpecialFinds)
		{
			const AKitchenLoot* L = F.Get();
			if (L && !L->IsCarried() && FVector::Dist(L->GetActorLocation(), Ant->GetActorLocation()) < 2600.f)
			{
				Marker(L->GetActorLocation() + FVector(0.f, 0.f, 40.f), FString::Printf(TEXT("НАХОДКА: %s"), *L->LootName), QJelly, false);
			}
		}
	}

	// --- My pockets ---
	if (const AAntPlayerState* PS = PlayerOwner ? PlayerOwner->GetPlayerState<AAntPlayerState>() : nullptr)
	{
		if ((PS->Tool0 != 0 || PS->Tool1 != 0) && !Ant->bDead)
		{
			const float TX = 30.f * S, TY = H - 270.f * S;
			FString Line;
			for (int32 p = 0; p < 2; ++p)
			{
				const uint8 Tool = p == 0 ? PS->Tool0 : PS->Tool1;
				const int32 Ch = p == 0 ? PS->Charges0 : PS->Charges1;
				if (Tool != 0) { Line += FString::Printf(TEXT("%s[%d] %s x%d"), Line.IsEmpty() ? TEXT("") : TEXT("    "), 5 + p, AntTool::Name(Tool), Ch); }
			}
			float LW = 0.f, LH = 0.f;
			GetTextSize(Line, LW, LH, Small, 1.1f * S);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), TX - 8.f * S, TY - 4.f * S, LW + 16.f * S, 28.f * S);
			Txt(Line, TX, TY, FLinearColor(0.85f, 0.8f, 1.f), Small, 1.1f * S);
		}
	}

	// --- Leaving: hold [L] at a nest entrance (the prompt waits until the player has been out at least once) ---
	const bool bHome = AAntRaid::IsHome(Ant);
	if (!bHome && !Ant->bDead) { BeenOutRaid = Raid->RaidNo; }
	const bool bKey = PC && PC->IsInputKeyDown(EKeys::L) && !Ant->bDead;
	if (!bKey)
	{
		LeaveHold = 0.f;
		bLeaveSent = false;
	}
	else if (!bLeaveSent)
	{
		LeaveHold += Dt;
		if (LeaveHold >= 0.8f)
		{
			bLeaveSent = true;
			PC->ServerRaid(AntRaidCmd::Leave, 0, 0);
		}
	}
	if (Raid->ExitEnds > 0.f)
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(Raid->ExitEnds - ServerNow));
		const float BY = H * 0.30f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), W * 0.5f - 380.f * S, BY, 760.f * S, 86.f * S);
		DrawRect(QGold, W * 0.5f - 380.f * S, BY, 760.f * S, 3.f * S);
		Txt(FString::Printf(TEXT("УХОДИМ ДОМОЙ ЧЕРЕЗ %d"), Left), W * 0.5f, BY + 8.f * S, QGold, Med, 1.25f * S, true);
		const FString Who = Raid->ExitNeed > 1 ? FString::Printf(TEXT("зовёт %s  ·  дома %d из %d  ·  [L] у входа — согласен, [L] снаружи — рано"), *Raid->ExitBy, (int32)Raid->ExitHome, (int32)Raid->ExitNeed)
			: FString(TEXT("выйди из гнезда, чтобы остаться"));
		Txt(Who, W * 0.5f, BY + 52.f * S, QWhite, Small, 1.1f * S, true);
	}
	else if (bHome && !Ant->bDead && !Ant->Carrying && BeenOutRaid == Raid->RaidNo)
	{
		const float PY = H * 0.56f;
		const FString Prompt = Raid->Cake == AntCake::Home ? TEXT("[L] удерживай — уйти в гнездо: рейд окончен, добыча ваша") : TEXT("[L] удерживай — уйти в гнездо и закончить рейд (торт ещё не дома)");
		float PW2 = 0.f, PH2 = 0.f;
		GetTextSize(Prompt, PW2, PH2, Small, 1.2f * S);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), W * 0.5f - PW2 * 0.5f - 14.f * S, PY - 6.f * S, PW2 + 28.f * S, 34.f * S);
		Txt(Prompt, W * 0.5f, PY, FLinearColor(1.f, 0.9f, 0.6f), Small, 1.2f * S, true);
		if (LeaveHold > 0.f && !bLeaveSent)
		{
			Bar(W * 0.5f - 110.f * S, PY + 26.f * S, 220.f * S, 8.f * S, FMath::Clamp(LeaveHold / 0.8f, 0.f, 1.f), QGold);
		}
	}

	// --- The first raid of a colony: what to do, step by step ---
	if (Raid->Nest.Raids == 0 && Ant->Creature == 0 && GS->Phase == 0)
	{
		if (HintShift != Raid->RaidNo)
		{
			HintShift = Raid->RaidNo;
			bHintOut = bHintGrab = bHintHome = bHintCake = false;
			HintsDoneAt = -1.f;
		}
		const AAntPlayerState* PS = PlayerOwner ? PlayerOwner->GetPlayerState<AAntPlayerState>() : nullptr;
		if (!Ant->bDead)
		{
			if (!bHintOut && !Ant->IsInNest()) { bHintOut = true; }
			if (bHintOut && !bHintGrab && Ant->Carrying && !Ant->Carrying->bIsGear) { bHintGrab = true; }
			if (bHintGrab && !bHintHome && PS && PS->Delivered > 0) { bHintHome = true; }
			if (bHintHome && !bHintCake && Raid->Cake == AntCake::Home) { bHintCake = true; HintsDoneAt = Now; }
		}
		const float Fade = HintsDoneAt < 0.f ? 1.f : FMath::Clamp((HintsDoneAt + 8.f - Now) / 2.f, 0.f, 1.f);
		if (Fade > 0.f)
		{
			const float HX = 30.f * S;
			float HY = H * 0.40f;
			Txt(bHintCake ? TEXT("ТОРТ ДОМА. Теперь [L] у входа — и рейд ваш") : TEXT("ПЕРВЫЙ РЕЙД"), HX, HY, FLinearColor(1.f, 0.8f, 0.2f, Fade), Med, 0.9f * S);
			HY += 34.f * S;
			struct FStep { const TCHAR* Text; bool bDone; };
			const FStep Steps[] =
			{
				{ TEXT("1. Выйди из гнезда: щель в стене ведёт на кухню"), bHintOut },
				{ TEXT("2. Схвати крошку на полу — [E]"), bHintGrab },
				{ TEXT("3. Дотащи её до входа: считается только то, что дома"), bHintHome },
				{ TEXT("4. Найди торт и принеси его всем отрядом"), bHintCake },
				{ TEXT("5. Удерживай [L] у входа, чтобы уйти живым"), false },
			};
			bool bCurrent = true;
			for (const FStep& St : Steps)
			{
				const FLinearColor C = St.bDone ? FLinearColor(0.4f, 1.f, 0.4f, 0.75f * Fade) : bCurrent ? FLinearColor(1.f, 1.f, 1.f, Fade) : FLinearColor(0.7f, 0.7f, 0.7f, 0.6f * Fade);
				const float Box = 13.f * S;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * Fade), HX, HY + 4.f * S, Box, Box);
				if (St.bDone)
				{
					DrawRect(FLinearColor(0.4f, 1.f, 0.4f, Fade), HX + 2.f * S, HY + 6.f * S, Box - 4.f * S, Box - 4.f * S);
				}
				Txt(St.Text, HX + Box + 8.f * S, HY, C, Small, 1.15f * S);
				if (!St.bDone) { bCurrent = false; }
				HY += 24.f * S;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// The results: what the raid brought, what it cost, and its story
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawRaidResults(AAntRaid* Raid, AAntGameState* GS)
{
	if (!AntUi::Classic())
	{
		// The debrief as a newsreel + a typed field report (AntUiCanvas.cpp). -AntUiClassic: the old panel below.
		const int32 SecondsLeft = FMath::Max(0, FMath::CeilToInt(Raid->PhaseEnds - GS->GetServerWorldTimeSeconds()));
		AntUiCanvas::DrawDebrief(Canvas, GetWorld(), Raid->Result, GS, SecondsLeft);
		if (AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner))
		{
			if (PC->WasInputKeyJustPressed(EKeys::Enter)) { PC->ServerRaid(AntRaidCmd::Skip, 0, 0); }
		}
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	const FAntRaidResult& R = Raid->Result;
	const bool bGood = R.End == AntRaidEnd::Left;
	const bool bWiped = R.End == AntRaidEnd::Wiped;
	const FLinearColor Accent = bGood ? QGold : QRed;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0.f, 0.f, W, H);
	const float PW = FMath::Min(W - 60.f * S, 1400.f * S), PH = 700.f * S;
	const float PX = (W - PW) * 0.5f, PY = FMath::Max(20.f * S, (H - PH) * 0.5f);
	DrawRect(QPanel, PX, PY, PW, PH);
	DrawRect(Accent, PX, PY, PW, 4.f * S);

	const FString Title = bGood ? FString::Printf(TEXT("РЕЙД %d: УШЛИ С ДОБЫЧЕЙ"), R.RaidNo) : bWiped ? FString::Printf(TEXT("РЕЙД %d: ОТРЯД ПОГИБ"), R.RaidNo) : FString::Printf(TEXT("РЕЙД %d: ВРЕМЯ ВЫШЛО"), R.RaidNo);
	Txt(Title, W * 0.5f, PY + 18.f * S, Accent, Big, 1.9f * S, true);

	// --- Left: the numbers, big ones first ---
	const float LX = PX + 40.f * S;
	float Y = PY + 112.f * S;
	auto Row = [&](const FString& Label, const FString& Value, const FLinearColor& C, float Scale)
	{
		Txt(Label, LX, Y + (Scale > 1.3f ? 10.f * S : 0.f), QGrey, Small, 1.25f * S);
		Txt(Value, LX + 280.f * S, Y, C, Scale > 1.3f ? Med : Small, Scale * S);
		Y += (Scale > 1.3f ? 54.f : 32.f) * S;
	};
	Row(TEXT("ЕДА В КЛАДОВУЮ"), bWiped ? FString::Printf(TEXT("+%d  (из %d: половину унесли красные)"), R.Kept, R.Banked) : FString::Printf(TEXT("+%d"), R.Kept), bWiped ? QRed : QGold, 1.8f);
	Row(TEXT("МАТОЧНОЕ МОЛОЧКО"), FString::Printf(TEXT("+%d"), R.Jelly), R.Jelly > 0 ? QJelly : QDim, 1.8f);
	{
		static const TCHAR* CakeText[] = { TEXT("остался в квартире"), TEXT("бросили по дороге"), TEXT("остался у красных"), TEXT("ДОМА"), TEXT("съеден в красном холме"), TEXT("потерян"), TEXT("разбит") };
		const bool bHomeCake = R.Cake == AntCake::Home && !bWiped;
		Row(TEXT("ТОРТ"), bHomeCake ? TEXT("ДОМА") : (R.Cake == AntCake::Home ? TEXT("был дома — растащили") : CakeText[FMath::Min<int32>(R.Cake, 6)]), bHomeCake ? QGreen : QRed, 1.8f);
	}
	Y += 6.f * S;
	Row(TEXT("Запрос Королевы"), FString::Printf(TEXT("%d из %d"), R.Kept, R.Ask), R.Kept >= R.Ask ? QGreen : QGrey, 1.25f);
	if (R.ContractsTaken > 0) { Row(TEXT("Контракты"), FString::Printf(TEXT("%d из %d"), R.ContractsDone, R.ContractsTaken), R.ContractsDone > 0 ? QGreen : QGrey, 1.1f); }
	Row(TEXT("Погибло"), R.Lost > 0 ? FString::Printf(TEXT("%d, не вернулось %d"), R.Deaths, R.Lost) : FString::Printf(TEXT("%d"), R.Deaths), (R.Deaths + R.Lost) > 0 ? QRed : QGreen, 1.25f);
	if (R.SquadOut > 0) { Row(TEXT("Рабочие отрядов"), FString::Printf(TEXT("вернулось %d из %d"), R.SquadBack, R.SquadOut), R.SquadBack >= R.SquadOut ? QGreen : R.SquadBack > 0 ? QGold : QRed, 1.1f); }
	Row(TEXT("Личинки"), R.LarvaeLaid > 0 ? FString::Printf(TEXT("осталось %d, Королева отложила +%d"), R.LarvaeLeft, R.LarvaeLaid) : FString::Printf(TEXT("осталось %d"), R.LarvaeLeft), R.LarvaeLeft > 0 ? QWhite : QRed, 1.25f);
	{
		const int32 D = R.MoodAfter - R.MoodBefore;
		Row(TEXT("Королева"), FString::Printf(TEXT("%s  (%s%d)"), AntNest::MoodName(R.MoodAfter), D >= 0 ? TEXT("+") : TEXT(""), D), D > 0 ? QGreen : D < 0 ? QRed : QGrey, 1.25f);
		Bar(LX + 280.f * S, Y - 2.f * S, 240.f * S, 9.f * S, R.MoodAfter / 100.f, R.MoodAfter >= 60 ? QGreen : R.MoodAfter > 25 ? QGold : QRed);
		Y += 16.f * S;
	}
	if (R.QueenAte > 0) { Row(TEXT("Королева в ярости"), FString::Printf(TEXT("съела %d из кладовой"), R.QueenAte), QRed, 1.1f); }
	if (R.NewTier > 0) { Row(TEXT("НОВАЯ УГРОЗА ОТКРЫТА"), FString::Printf(TEXT("%s «%s»"), Roman(R.NewTier), AntTier::Def(R.NewTier).Name), QGold, 1.1f); }
	if (R.Seed >= 0) { Row(TEXT("Сид квартиры"), R.SeedText.IsEmpty() ? FString::Printf(TEXT("%d"), R.Seed) : R.SeedText, QGrey, 1.1f); }

	// --- Right: the story of the raid, then the awards ---
	const float RX = PX + PW * 0.52f;
	float RY = PY + 112.f * S;
	Txt(TEXT("ИСТОРИЯ РЕЙДА"), RX, RY, QGold, Small, 1.3f * S);
	RY += 30.f * S;
	const float MaxW = PX + PW - RX - 30.f * S;
	for (const FString& Line : R.Story)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Line, TW, TH, Small, 1.22f * S);
		const float Fit = FMath::Min(1.f, MaxW / FMath::Max(1.f, TW));
		Txt(Line, RX, RY, QWhite, Small, 1.22f * S * Fit);
		RY += 29.f * S;
	}
	if (GS->Awards.Num() > 0)
	{
		RY += 12.f * S;
		Txt(TEXT("НАГРАДЫ"), RX, RY, QGold, Small, 1.3f * S);
		RY += 30.f * S;
		for (int32 i = 0; i < FMath::Min(GS->Awards.Num(), 4); ++i)
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(GS->Awards[i], TW, TH, Small, 1.05f * S);
			Txt(GS->Awards[i], RX, RY, QGrey, Small, 1.05f * S * FMath::Min(1.f, MaxW / FMath::Max(1.f, TW)));
			RY += 24.f * S;
		}
	}
	const int32 Left = FMath::Max(0, FMath::CeilToInt(Raid->PhaseEnds - GS->GetServerWorldTimeSeconds()));
	Txt(FString::Printf(TEXT("В гнездо через %d с  ·  [Enter] — сразу (хост)"), Left), W * 0.5f, PY + PH - 40.f * S, QGrey, Small, 1.1f * S, true);
	if (AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner))
	{
		if (PC->WasInputKeyJustPressed(EKeys::Enter)) { PC->ServerRaid(AntRaidCmd::Skip, 0, 0); }
	}
}
