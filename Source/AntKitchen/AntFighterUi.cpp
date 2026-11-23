#include "AntFighterUi.h"
#include "AntFighter.h"
#include "AntCosmetics.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "AntHUD.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "Engine/Canvas.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"

using namespace AntUi;

namespace
{
	uint8 GScreen = 0;
	int32 GTab = 0;
	int32 GCaste = -1;
	double GOpenAt = 0.0;
	FString GTip;

	/** One frame of a screen: the canvas, the 1920x1080 sheet fitted into it, the mouse and the click. */
	struct FCtx
	{
		UCanvas* Cv = nullptr;
		float U = 1.f, OX = 0.f, OY = 0.f;
		FVector2D M = FVector2D(-1.f, -1.f);
		bool bClick = false;
		float X(float V) const { return OX + V * U; }
		float Y(float V) const { return OY + V * U; }
		bool Hover(float X0, float Y0, float W, float H) const { return M.X >= X(X0) && M.X < X(X0 + W) && M.Y >= Y(Y0) && M.Y < Y(Y0 + H); }
		FVector2D Txt(const FString& S, float X0, float Y0, EFont F, float Pt, const FLinearColor& C, int32 Align = 0, float Shadow = 0.f) const
		{
			return AntUiCanvas::Text(Cv, S, X(X0), Y(Y0), F, Pt * U, C, Align, Shadow) / U;
		}
		float Wrap(const FString& S, float X0, float Y0, float W, EFont F, float Pt, const FLinearColor& C, int32 Lines = 3) const
		{
			return AntUiCanvas::Wrap(Cv, S, X(X0), Y(Y0), W * U, F, Pt * U, C, Lines) / U;
		}
		void Rect(float X0, float Y0, float W, float H, const FLinearColor& C) const { AntUiCanvas::Rect(Cv, X(X0), Y(Y0), W * U, H * U, C); }
		void Frame(float X0, float Y0, float W, float H, float Th, const FLinearColor& C) const { AntUiCanvas::Frame(Cv, X(X0), Y(Y0), W * U, H * U, Th * U, C); }
	};

	FLinearColor HexC(uint32 H) { return AntUi::Hex(H); }

	bool Btn(FCtx& C, const FString& Label, float X0, float Y0, float W, float H, bool bEnabled, bool bOn, const FString& Tip = FString(), float Pt = 18.f)
	{
		const bool bHot = bEnabled && C.Hover(X0, Y0, W, H);
		const FLinearColor Edge = !bEnabled ? WithA(C::Cocoa, 0.5f) : bOn ? C::LeafDeep : C::AmberDeep;
		const FLinearColor Fill = !bEnabled ? WithA(C::InkSoft, 0.7f) : bOn ? (bHot ? C::Leaf : WithA(C::LeafDeep, 0.95f)) : bHot ? C::Amber : WithA(C::InkSoft, 0.95f);
		C.Rect(X0, Y0, W, H, Edge);
		C.Rect(X0 + 2.f, Y0 + 2.f, W - 4.f, H - 4.f, Fill);
		if (bEnabled) { C.Rect(X0 + 2.f, Y0 + 2.f, W - 4.f, FMath::Max(1.f, (H - 4.f) * 0.16f), FLinearColor(1.f, 0.9f, 0.7f, bHot ? 0.25f : 0.1f)); }
		const FLinearColor Ink = !bEnabled ? C::Muted : (bHot && !bOn) ? C::Ink : C::Sugar;
		const FVector2D Sz = AntUiCanvas::TextSize(C.Cv, Label, EFont::Display, Pt * C.U) / C.U;
		const float K = Sz.X > W - 14.f ? (W - 14.f) / Sz.X : 1.f;
		C.Txt(Label, X0 + W * 0.5f, Y0 + (H - Sz.Y * K) * 0.5f, EFont::Display, Pt * K, Ink, 1);
		if (C.Hover(X0, Y0, W, H) && !Tip.IsEmpty()) { GTip = Tip; }
		return bHot && C.bClick;
	}

	void Panel(FCtx& C, float X0, float Y0, float W, float H, const FString& Title)
	{
		AntUiCanvas::Glass(C.Cv, C.X(X0), C.Y(Y0), W * C.U, H * C.U, 0.88f);
		C.Rect(X0, Y0, W, 3.f, C::AmberDeep);
		AntUiCanvas::Tile(C.Cv, AntUi::Tex(TEXT("Materials/T_UI_Tape")), C.X(X0 + W - 90.f), C.Y(Y0 - 10.f), 100.f * C.U, 26.f * C.U, FLinearColor(1.f, 1.f, 1.f, 0.85f), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), 8.f);
		if (!Title.IsEmpty()) { C.Txt(Title, X0 + 18.f, Y0 + 12.f, EFont::Display, 20.f, C::Honey, 0, 0.6f); }
	}

	AAntFighterState* FSOf(const AAntPlayerController* PC) { return PC ? AAntFighterState::Get(PC->GetWorld()) : nullptr; }

	void SetCaste(AAntPlayerController* PC, int32 Caste)
	{
		AAntCharacter* Ant = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
		UFunction* Fn = Ant ? Ant->FindFunction(TEXT("ServerSetClass")) : nullptr;
		if (Fn)
		{
			struct { uint8 NewClass; } Params{ (uint8)FMath::Clamp(Caste, 0, 4) };
			Ant->ProcessEvent(Fn, &Params);
		}
	}

	const TCHAR* CasteIcon(int32 Caste)
	{
		switch (Caste) { case 1: return TEXT("Soldier"); case 2: return TEXT("Scout"); case 3: return TEXT("Barrel"); case 4: return TEXT("Wrench"); default: return TEXT("Forager"); }
	}

	/** Saves the profile and sends the loadout. */
	void Commit(AAntPlayerController* PC)
	{
		AntFighter::SaveProfile();
		AntFighter::PushLoadout(PC);
	}

	// ---------------------------------------------------------------------------------------------- the profile: tabs

	void TabFighter(FCtx& C, AAntPlayerController* PC)
	{
		FAntProfile& P = AntFighter::Profile();
		const int32 R = P.Rank();
		const AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>();
		const int32 Caste = PS ? (int32)PS->AntClass : 0;
		// left: the soldier's book
		AntUiCanvas::Paper(C.Cv, C.X(60.f), C.Y(170.f), 560.f * C.U, 820.f * C.U, true);
		if (UTexture2D* Por = AntUi::Tex(AntUi::CastePortrait(Caste)))
		{
			AntUiCanvas::Tile(C.Cv, Por, C.X(100.f), C.Y(210.f), 220.f * C.U, 220.f * C.U, FLinearColor::White);
		}
		C.Frame(100.f, 210.f, 220.f, 220.f, 3.f, C::Cocoa);
		C.Txt(P.Name.IsEmpty() ? FString(TEXT("Боец")) : P.Name, 340.f, 214.f, EFont::Bold, 30.f, C::Graphite);
		C.Txt(AntUi::CasteName(Caste), 340.f, 256.f, EFont::Body, 22.f, C::Graphite);
		C.Txt(FString::Printf(TEXT("рейдов: %d"), P.Raids), 340.f, 292.f, EFont::Mono, 18.f, C::Graphite);
		C.Txt(FString::Printf(TEXT("донесено еды: %d"), P.TotalFood), 340.f, 318.f, EFont::Mono, 18.f, C::Graphite);
		C.Txt(FString::Printf(TEXT("тел донесено: %d"), P.TotalRescues), 340.f, 344.f, EFont::Mono, 18.f, C::Graphite);
		C.Txt(FString::Printf(TEXT("медалей: %d из %d"), FMath::CountBits(P.MedalBits()), (int32)AntFeat::Count), 340.f, 370.f, EFont::Mono, 18.f, C::Graphite);
		// the rank and the bar to the next one
		AntUiCanvas::Stamp(C.Cv, AntRank::Name(R), C.X(340.f), C.Y(480.f), -6.f, C::JamDeep, 30.f * C.U);
		C.Txt(FString::Printf(TEXT("Знаки: %s"), AntRank::Insignia(R)), 100.f, 540.f, EFont::Hand, 22.f, C::Graphite);
		const int32 Cur = AntRank::XpFor(R), Next = R + 1 < AntRank::Count ? AntRank::XpFor(R + 1) : Cur;
		const float Frac = Next > Cur ? FMath::Clamp((float)(P.Xp - Cur) / (Next - Cur), 0.f, 1.f) : 1.f;
		AntUiCanvas::Bar(C.Cv, C.X(100.f), C.Y(590.f), 480.f * C.U, 26.f * C.U, Frac, C::Amber);
		C.Txt(R + 1 < AntRank::Count ? FString::Printf(TEXT("опыт %d · до «%s» ещё %d"), P.Xp, AntRank::Name(R + 1), Next - P.Xp) : FString::Printf(TEXT("опыт %d · выше только Королева"), P.Xp),
			100.f, 624.f, EFont::Bold, 19.f, C::Graphite);
		// the ladder
		for (int32 i = 0; i < AntRank::Count; ++i)
		{
			const float Y0 = 668.f + i * 38.f;
			const bool bMe = i == R;
			if (bMe) { C.Rect(92.f, Y0 - 2.f, 496.f, 34.f, WithA(C::Amber, 0.35f)); }
			C.Txt(AntRank::Name(i), 104.f, Y0, bMe ? EFont::Bold : EFont::Body, 20.f, i <= R ? C::Graphite : WithA(C::Graphite, 0.5f));
			C.Txt(FString::Printf(TEXT("%d"), AntRank::XpFor(i)), 576.f, Y0, EFont::Mono, 18.f, WithA(C::Graphite, 0.7f), 2);
		}
		// right: the last raid
		Panel(C, 660.f, 170.f, 1200.f, 470.f, TEXT("ПОСЛЕДНИЙ РЕЙД"));
		if (P.LastRaid.Num() == 0)
		{
			C.Wrap(TEXT("Опыт дают за дело: донесённая еда, секунды под тортом, товарищ, вынесенный до гнезда, отвлечённый Гигант, открученный крепёж, дела внутри людей, красные и пауки, возвращение живым. Медали — за смешные подвиги; они открывают облик."), 684.f, 220.f, 1150.f, EFont::Body, 22.f, C::Sugar, 6);
		}
		else
		{
			C.Txt(FString::Printf(TEXT("+%d опыта"), P.LastRaidXp), 684.f, 214.f, EFont::Display, 34.f, C::Honey);
			for (int32 i = 0; i < P.LastRaid.Num() && i < 9; ++i)
			{
				C.Txt(P.LastRaid[i], 690.f, 270.f + i * 38.f, EFont::Mono, 19.f, C::Sugar);
			}
		}
		// right below: what the perks of this caste do now
		Panel(C, 660.f, 660.f, 1200.f, 330.f, TEXT("ВЫБРАННЫЕ ПЕРКИ КАСТЫ"));
		for (int32 t = 0; t < AntPerk::Tiers; ++t)
		{
			const int32 Ch = AntPerk::Get(P.Perks, Caste, t);
			const bool bOpen = R >= AntPerk::NeedRank(t);
			const FString Line = !bOpen ? FString::Printf(TEXT("%d ярус — откроется в звании «%s»"), t + 1, AntRank::Name(AntPerk::NeedRank(t)))
				: Ch == 0 ? FString::Printf(TEXT("%d ярус — не выбран (вкладка «Касты»)"), t + 1)
				: FString::Printf(TEXT("%d ярус — «%s»: %s"), t + 1, AntPerk::Def(Caste, t, Ch - 1).Name, AntPerk::Def(Caste, t, Ch - 1).Text);
			C.Wrap(Line, 684.f, 712.f + t * 86.f, 1150.f, EFont::Body, 21.f, Ch > 0 && bOpen ? C::Sugar : C::Muted, 2);
		}
	}

	void TabCastes(FCtx& C, AAntPlayerController* PC)
	{
		FAntProfile& P = AntFighter::Profile();
		const int32 R = P.Rank();
		const AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>();
		const int32 Mine = PS ? (int32)PS->AntClass : 0;
		if (GCaste < 0) { GCaste = Mine; }
		static const TCHAR* Names[AntPerk::Castes] = { TEXT("ФУРАЖИР"), TEXT("СОЛДАТ"), TEXT("РАЗВЕДЧИК"), TEXT("БОЧКА"), TEXT("САПЁР") };
		for (int32 c = 0; c < AntPerk::Castes; ++c)
		{
			const float X0 = 60.f + c * 362.f;
			if (Btn(C, Names[c], X0, 160.f, 346.f, 64.f, true, c == GCaste, FString::Printf(TEXT("Дерево перков: %s"), Names[c]), 22.f)) { GCaste = c; }
			AntUiCanvas::Icon(C.Cv, CasteIcon(c), C.X(X0 + 6.f), C.Y(166.f), 52.f * C.U);
		}
		const int32 Ca = GCaste;
		C.Txt(Ca == Mine ? FString(TEXT("Это твоя каста сейчас")) : FString(TEXT("Сейчас ты другой касты")), 60.f, 246.f, EFont::Hand, 24.f, Ca == Mine ? C::Leaf : C::Muted);
		if (Ca != Mine && Btn(C, TEXT("СТАТЬ ЭТОЙ КАСТОЙ"), 1440.f, 238.f, 420.f, 50.f, PC->GetPawn() != nullptr, false, TEXT("Сменить касту можно в гнезде"), 18.f)) { SetCaste(PC, Ca); }
		if (Ca == 4)
		{
			C.Wrap(TEXT("Сапёр: крепёж и ткань в полтора раза быстрее, свой у Гиганта внутри. Слабее в бою (урон ×0.9), сила 11, здоровье 95."), 60.f, 290.f, 1360.f, EFont::Body, 20.f, C::Sugar, 2);
		}
		for (int32 t = 0; t < AntPerk::Tiers; ++t)
		{
			const float Y0 = 350.f + t * 220.f;
			const bool bOpen = R >= AntPerk::NeedRank(t);
			const int32 Ch = AntPerk::Get(P.Perks, Ca, t);
			C.Txt(FString::Printf(TEXT("%d ЯРУС"), t + 1), 60.f, Y0 + 70.f, EFont::Display, 28.f, bOpen ? C::Honey : C::Muted);
			C.Txt(FString::Printf(TEXT("с «%s»"), AntRank::Name(AntPerk::NeedRank(t))), 60.f, Y0 + 110.f, EFont::Body, 19.f, bOpen ? C::Sugar : C::Muted);
			for (int32 o = 0; o < 2; ++o)
			{
				const float X0 = 260.f + o * 820.f;
				const bool bOn = Ch == o + 1;
				const bool bHot = bOpen && C.Hover(X0, Y0, 760.f, 190.f);
				AntUiCanvas::Paper(C.Cv, C.X(X0), C.Y(Y0), 760.f * C.U, 190.f * C.U, bOn);
				if (!bOpen) { C.Rect(X0, Y0, 760.f, 190.f, FLinearColor(0.02f, 0.015f, 0.01f, 0.55f)); }
				if (bOn) { C.Frame(X0, Y0, 760.f, 190.f, 5.f, C::LeafDeep); }
				else if (bHot) { C.Frame(X0, Y0, 760.f, 190.f, 4.f, C::Amber); }
				const AntPerk::FDef& D = AntPerk::Def(Ca, t, o);
				C.Txt(D.Name, X0 + 28.f, Y0 + 22.f, EFont::Display, 30.f, C::Graphite);
				C.Wrap(D.Text, X0 + 28.f, Y0 + 74.f, 700.f, EFont::Body, 22.f, C::Graphite, 3);
				if (bOn) { AntUiCanvas::Stamp(C.Cv, TEXT("ВЫБРАН"), C.X(X0 + 640.f), C.Y(Y0 + 40.f), -8.f, C::LeafDeep, 20.f * C.U); }
				if (bHot) { GTip = FString::Printf(TEXT("%s. Клик — выбрать (ещё клик — снять). Менять можно в гнезде сколько угодно"), D.Name); }
				if (bHot && C.bClick)
				{
					P.Perks = AntPerk::Set(P.Perks, Ca, t, bOn ? 0 : o + 1);
					UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER ui: perk caste %d tier %d -> %d"), Ca, t, bOn ? 0 : o + 1);
					Commit(PC);
				}
			}
			if (!bOpen) { C.Txt(TEXT("или"), 1040.f, Y0 + 80.f, EFont::Hand, 26.f, C::Muted, 1); }
			else { C.Txt(TEXT("или"), 1040.f, Y0 + 80.f, EFont::Hand, 26.f, C::Honey, 1); }
		}
	}

	void TabMedals(FCtx& C, AAntPlayerController* PC)
	{
		const FAntProfile& P = AntFighter::Profile();
		const int32 Cols = 6;
		Panel(C, 50.f, 140.f, 1820.f, 888.f, FString());
		for (int32 i = 0; i < AntFeat::Count; ++i)
		{
			const AntFeat::FDef& D = AntFeat::Def(i);
			const int32 N = P.FeatCount.IsValidIndex(i) ? P.FeatCount[i] : 0;
			const float X0 = 66.f + (i % Cols) * 302.f, Y0 = 152.f + (i / Cols) * 146.f;
			const bool bHot = C.Hover(X0, Y0, 294.f, 140.f);
			C.Rect(X0, Y0, 294.f, 140.f, bHot ? WithA(C::Amber, 0.22f) : WithA(C::InkSoft, 0.7f));
			AntFighterUi::Badge(C.Cv, C.X(X0 + 10.f), C.Y(Y0 + 16.f), 108.f * C.U, D.Icon, D.Color, N == 0, -12.f + (i % 5) * 6.f);
			C.Wrap(N > 0 ? FString(D.Name) : FString(TEXT("Не получена")), X0 + 128.f, Y0 + 18.f, 160.f, EFont::Bold, 19.f, N > 0 ? C::Sugar : C::Muted, 3);
			if (N > 0) { C.Txt(FString::Printf(TEXT("×%d · +%d"), N, D.Xp), X0 + 128.f, Y0 + 108.f, EFont::Mono, 15.f, C::Honey); }
			if (bHot) { GTip = N > 0 ? FString::Printf(TEXT("«%s» — %s. Получена раз: %d. +%d опыта"), D.Name, D.Text, N, D.Xp) : FString::Printf(TEXT("Не получена. Подсказка: %s"), D.Text); }
		}
	}

	void TabLook(FCtx& C, AAntPlayerController* PC)
	{
		FAntProfile& P = AntFighter::Profile();
		const int32 R = P.Rank();
		const uint64 Med = P.MedalBits();
		P.Cosm.SetNumZeroed(AntCosm::SlotCount);
		for (int32 s = 0; s < AntCosm::SlotCount; ++s)
		{
			const float Y0 = 160.f + s * 76.f;
			C.Rect(60.f, Y0, 1100.f, 66.f, WithA(C::Ink, 0.85f));
			C.Rect(60.f, Y0, 6.f, 66.f, C::Amber);
			C.Txt(AntCosm::SlotName(s), 86.f, Y0 + 16.f, EFont::Display, 22.f, C::Honey);
			const int32 Cur = P.Cosm[s];
			const AntCosm::FItem& I = AntCosm::Item(s, Cur);
			if (I.Color != 0) { C.Rect(340.f, Y0 + 18.f, 30.f, 30.f, AntCosmetics::Col(I.Color)); C.Frame(340.f, Y0 + 18.f, 30.f, 30.f, 2.f, C::Cocoa); }
			C.Txt(I.Name, 390.f, Y0 + 16.f, EFont::Bold, 24.f, C::Sugar);
			int32 Open = 0;
			for (int32 k = 0; k < AntCosm::NumItems(s); ++k) { Open += AntCosm::Unlocked(s, k, R, Med) ? 1 : 0; }
			C.Txt(FString::Printf(TEXT("%d / %d"), Open, AntCosm::NumItems(s)), 920.f, Y0 + 20.f, EFont::Mono, 18.f, C::Muted, 2);
			auto Step = [&](int32 Dir)
			{
				int32 K = Cur;
				for (int32 n = 0; n < AntCosm::NumItems(s); ++n)
				{
					K = (K + Dir + AntCosm::NumItems(s)) % AntCosm::NumItems(s);
					if (AntCosm::Unlocked(s, K, R, Med)) { break; }
				}
				P.Cosm[s] = (uint8)K;
				UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER ui: %s -> %s"), AntCosm::SlotName(s), AntCosm::Item(s, K).Name);
				Commit(PC);
			};
			if (Btn(C, TEXT("‹"), 950.f, Y0 + 8.f, 90.f, 50.f, Open > 1, false, TEXT("Предыдущий открытый"), 26.f)) { Step(-1); }
			if (Btn(C, TEXT("›"), 1050.f, Y0 + 8.f, 90.f, 50.f, Open > 1, false, TEXT("Следующий открытый"), 26.f)) { Step(1); }
			if (C.Hover(60.f, Y0, 880.f, 66.f))
			{
				FString Locked;
				for (int32 k = 0; k < AntCosm::NumItems(s); ++k)
				{
					if (!AntCosm::Unlocked(s, k, R, Med)) { Locked += FString::Printf(TEXT("%s«%s» — %s"), Locked.IsEmpty() ? TEXT("") : TEXT("; "), AntCosm::Item(s, k).Name, *AntCosm::UnlockText(s, k)); }
				}
				GTip = Locked.IsEmpty() ? FString(TEXT("Всё в этом ряду уже открыто")) : FString::Printf(TEXT("Закрыто: %s"), *Locked);
			}
		}
		// right: the insignia, the emote, what everybody sees
		Panel(C, 1200.f, 160.f, 660.f, 360.f, TEXT("ЗНАКИ ЗВАНИЯ"));
		C.Wrap(FString::Printf(TEXT("«%s»: %s — на спине у муравья, все в рейде видят."), AntRank::Name(R), AntRank::Insignia(R)), 1224.f, 210.f, 610.f, EFont::Body, 21.f, C::Sugar, 3);
		if (Btn(C, P.bInsignia ? TEXT("НОСИТЬ: ДА") : TEXT("НОСИТЬ: НЕТ"), 1224.f, 320.f, 300.f, 56.f, true, P.bInsignia, TEXT("Показывать знаки звания на спине"), 20.f)) { P.bInsignia = !P.bInsignia; Commit(PC); }
		C.Wrap(TEXT("Никаких флагов и армейских знаков: только сахар, спички и крышечки."), 1224.f, 400.f, 610.f, EFont::Hand, 21.f, C::Muted, 2);
		Panel(C, 1200.f, 540.f, 660.f, 300.f, TEXT("ЖЕСТ  [N]"));
		C.Wrap(TEXT("Без отдания чести: топот, усы-пропеллер, поклон Королеве, танец с крошкой. Видят все."), 1224.f, 590.f, 610.f, EFont::Body, 21.f, C::Sugar, 3);
		if (Btn(C, TEXT("ПОКАЗАТЬ"), 1224.f, 700.f, 300.f, 56.f, P.Cosm[AntCosm::Emote] > 0, false, TEXT("Сыграть выбранный жест (в рейде — клавиша N)"), 20.f)) { PC->DoEmote(); }
		C.Wrap(TEXT("Облик — только для вида: на силу и скорость не влияет."), 1200.f, 870.f, 660.f, EFont::Hand, 22.f, C::Honey, 2);
	}

	// ---------------------------------------------------------------------------------------------- the roster

	void DrawRoster(FCtx& C, AAntPlayerController* PC)
	{
		AAntFighterState* FS = FSOf(PC);
		const FString Me = AntFighter::IdOf(PC->PlayerState);
		const AAntRaid* Raid = AAntRaid::Get(PC->GetWorld());
		const bool bNest = !Raid || !Raid->InRaid();
		const int32 Cap = FS ? FS->DraftCap : 2;
		const int32 Mine = AntVets::NumDrafted(FS, Me);
		Panel(C, 60.f, 150.f, 1120.f, 880.f, FString::Printf(TEXT("КАЗАРМА · ВЕТЕРАНОВ %d"), FS ? FS->Roster.Num() : 0));
		if (!FS || FS->Roster.Num() == 0)
		{
			C.Wrap(TEXT("Пока пусто. Муравей отряда, переживший рейд, получает имя, звание и причуду — и ждёт здесь следующего похода. Погибший не вернётся."), 90.f, 210.f, 1060.f, EFont::Body, 24.f, C::Sugar, 4);
		}
		else
		{
			for (int32 i = 0; i < FS->Roster.Num() && i < 15; ++i)
			{
				const FAntVeteran& V = FS->Roster[i];
				const float X0 = 84.f + (i % 3) * 362.f, Y0 = 200.f + (i / 3) * 164.f;
				const bool bMine = V.DraftedBy == Me, bOther = !V.DraftedBy.IsEmpty() && !bMine;
				AntUiCanvas::Paper(C.Cv, C.X(X0), C.Y(Y0), 346.f * C.U, 150.f * C.U, bMine);
				if (bMine) { C.Frame(X0, Y0, 346.f, 150.f, 4.f, C::LeafDeep); }
				AntUiCanvas::Icon(C.Cv, V.Caste ? TEXT("Soldier") : TEXT("Forager"), C.X(X0 + 8.f), C.Y(Y0 + 10.f), 44.f * C.U);
				C.Txt(V.Name, X0 + 60.f, Y0 + 8.f, EFont::Bold, 22.f, C::Graphite);
				C.Txt(AntRank::Name(V.Rank), X0 + 60.f, Y0 + 38.f, EFont::Display, 16.f, C::JamDeep);
				C.Txt(FString::Printf(TEXT("рейдов %d · груза %d · врагов %d"), V.Raids, V.Carried, V.Kills), X0 + 14.f, Y0 + 66.f, EFont::Mono, 14.f, C::Graphite);
				FString Tr;
				for (int32 t = 0; t < AntTrait::Count; ++t) { if (V.Has(t)) { Tr += (Tr.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(AntTrait::Name(t)); } }
				C.Txt(Tr, X0 + 14.f, Y0 + 88.f, EFont::Hand, 19.f, C::LeafDeep);
				if (C.Hover(X0, Y0, 346.f, 112.f))
				{
					FString Tx;
					for (int32 t = 0; t < AntTrait::Count; ++t) { if (V.Has(t)) { Tx += FString::Printf(TEXT(" «%s»: %s."), AntTrait::Name(t), AntTrait::Text(t)); } }
					GTip = FString::Printf(TEXT("%s %s, в колонии с рейда №%d.%s"), AntRank::Name(V.Rank), *V.Name, V.Joined, *Tx);
				}
				if (bOther)
				{
					C.Txt(TEXT("в чужом отряде"), X0 + 330.f, Y0 + 118.f, EFont::Hand, 18.f, C::Muted, 2);
				}
				else if (Btn(C, bMine ? TEXT("ОТПУСТИТЬ") : TEXT("В ОТРЯД"), X0 + 186.f, Y0 + 112.f, 150.f, 32.f, bNest && (bMine || Mine < Cap), bMine,
					bMine ? FString(TEXT("Оставить в казарме")) : FString::Printf(TEXT("Взять в свой отряд (%d из %d)"), Mine, Cap), 15.f))
				{
					PC->ServerVeteran(bMine ? AntVets::Cmd::Undraft : AntVets::Cmd::Draft, V.Id, 0);
				}
			}
		}
		// right: my squad and the doctrine
		Panel(C, 1210.f, 150.f, 650.f, 330.f, FString::Printf(TEXT("МОЙ ОТРЯД · %d / %d"), Mine, Cap));
		int32 Row = 0;
		if (FS)
		{
			for (const FAntVeteran& V : FS->Roster)
			{
				if (V.DraftedBy != Me || Row >= 5) { continue; }
				C.Txt(FString::Printf(TEXT("• %s %s"), AntRank::Name(V.Rank), *V.Name), 1236.f, 200.f + Row * 34.f, EFont::Bold, 21.f, C::Sugar);
				++Row;
			}
		}
		if (Row == 0) { C.Txt(TEXT("Ветеранов в отряде нет"), 1236.f, 200.f, EFont::Body, 21.f, C::Muted); }
		if (Btn(C, TEXT("НАБРАТЬ ЛУЧШИХ"), 1236.f, 400.f, 300.f, 54.f, bNest && FS && Mine < Cap, false, TEXT("Взять в отряд самых опытных из свободных"), 18.f)) { PC->ServerVeteran(AntVets::Cmd::AutoDraft, 0, 0); }
		C.Wrap(TEXT("Место в отряде даёт Казарма (ветка «Солдаты») и Выводок."), 1552.f, 402.f, 290.f, EFont::Hand, 17.f, C::Muted, 3);
		Panel(C, 1210.f, 500.f, 650.f, 230.f, TEXT("ДОКТРИНА ОТРЯДА"));
		const int32 Best = AntVets::BestDraftedRank(FS, Me);
		const uint8 Doc = FS ? FS->DoctrineOf(Me) : 0;
		for (int32 d = 0; d < AntDoctrine::Count; ++d)
		{
			const bool bOk = d == 0 || Best >= AntDoctrine::NeedRank(d);
			if (Btn(C, AntDoctrine::Name(d), 1236.f + d * 152.f, 550.f, 144.f, 50.f, bOk, Doc == d,
				bOk ? FString(AntDoctrine::Text(d)) : FString::Printf(TEXT("%s. Нужен ветеран в звании «%s»"), AntDoctrine::Text(d), AntRank::Name(AntDoctrine::NeedRank(d))), 15.f))
			{
				PC->ServerVeteran(AntVets::Cmd::Doctrine, d, 0);
			}
		}
		C.Wrap(AntDoctrine::Text(Doc), 1236.f, 616.f, 600.f, EFont::Body, 19.f, C::Sugar, 3);
		// the memorial wall
		AntUiCanvas::Paper(C.Cv, C.X(1210.f), C.Y(750.f), 650.f * C.U, 280.f * C.U, true);
		C.Txt(TEXT("СТЕНА ПАМЯТИ"), 1236.f, 766.f, EFont::Display, 22.f, C::Graphite);
		if (!FS || FS->Fallen.Num() == 0)
		{
			C.Txt(TEXT("Пока никто не пал. Берегите ветеранов."), 1236.f, 810.f, EFont::Hand, 22.f, C::Graphite);
		}
		else
		{
			int32 n = 0;
			for (int32 i = FS->Fallen.Num() - 1; i >= 0 && n < 7; --i, ++n)
			{
				const FAntFallen& F = FS->Fallen[i];
				AntUiCanvas::TextFit(C.Cv, FString::Printf(TEXT("%s %s — %s, рейд №%d"), AntRank::Lower(F.Rank), *F.Name, *F.Cause, F.Raid), C.X(1236.f), C.Y(806.f + n * 30.f), 600.f * C.U, EFont::Hand, 19.f * C.U, C::Graphite);
			}
			if (FS->Fallen.Num() > 7) { C.Txt(FString::Printf(TEXT("…и ещё %d"), FS->Fallen.Num() - 7), 1836.f, 1000.f, EFont::Hand, 18.f, C::Graphite, 2); }
		}
	}

	FCtx MakeCtx(UCanvas* Cv, AAntPlayerController* PC, bool bAfterNest)
	{
		FCtx C;
		C.Cv = Cv;
		const float W = Cv->ClipX, H = Cv->ClipY;
		C.U = FMath::Min(W / 1920.f, H / 1080.f);
		C.OX = (W - 1920.f * C.U) * 0.5f;
		C.OY = (H - 1080.f * C.U) * 0.5f;
		float MX = -1.f, MY = -1.f;
		if (PC) { PC->GetMousePosition(MX, MY); C.bClick = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton); }
		if (AAntHUD::DevClickAt.X > 0.f || AAntHUD::DevClickAt.Y > 0.f)
		{
			// tests without a window: "nest click X Y" (the nest screen counts the frames down; on the results screen we do)
			MX = AAntHUD::DevClickAt.X * W;
			MY = AAntHUD::DevClickAt.Y * H;
			C.bClick = AAntHUD::DevClickFrames == (bAfterNest ? 1 : 2);
			if (!bAfterNest && AAntHUD::DevClickFrames > 0) { --AAntHUD::DevClickFrames; }
		}
		C.M = FVector2D(MX, MY);
		return C;
	}
}

namespace AntFighterUi
{
	TArray<FAntFighterToast>& Toasts() { static TArray<FAntFighterToast> T; return T; }
	bool IsOpen() { return GScreen != Screen::None; }

	void Open(AAntPlayerController* PC, uint8 Which, int32 Tab)
	{
		GScreen = Which;
		GTab = FMath::Clamp(Tab, 0, 3);
		GOpenAt = FPlatformTime::Seconds();
		GCaste = -1;
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER ui: open screen %d tab %d"), (int32)Which, GTab);
	}

	void Close() { GScreen = Screen::None; }
	void SetCaste(int32 Caste) { GCaste = Caste >= 0 && Caste < AntPerk::Castes ? Caste : -1; }

	void Badge(UCanvas* Cv, float X, float Y, float Size, const TCHAR* Icon, uint32 Color, bool bLocked, float TapeDeg)
	{
		UTexture2D* Dot = AntUi::Tex(TEXT("Materials/T_UI_Dot"));
		const FLinearColor Cap = bLocked ? FLinearColor(0.16f, 0.15f, 0.14f, 1.f) : AntUi::Hex(Color);   // locked: a plain grey cap
		const float CX = X + Size * 0.5f, CY = Y + Size * 0.5f;
		// the crimped rim of a bottle cap: 21 teeth around a disc
		for (int32 i = 0; i < 21; ++i)
		{
			const float A = TWO_PI * i / 21.f;
			const float TS = Size * 0.16f;
			AntUiCanvas::Tile(Cv, Dot, CX + FMath::Cos(A) * Size * 0.43f - TS * 0.5f, CY + FMath::Sin(A) * Size * 0.43f - TS * 0.5f, TS, TS, Cap * 0.7f);
		}
		AntUiCanvas::Tile(Cv, Dot, X + Size * 0.06f, Y + Size * 0.06f, Size * 0.88f, Size * 0.88f, Cap * 0.75f);
		AntUiCanvas::Tile(Cv, Dot, X + Size * 0.13f, Y + Size * 0.13f, Size * 0.74f, Size * 0.74f, Cap);
		AntUiCanvas::Tile(Cv, Dot, X + Size * 0.2f, Y + Size * 0.16f, Size * 0.3f, Size * 0.2f, FLinearColor(1.f, 1.f, 1.f, bLocked ? 0.05f : 0.28f));   // a shine
		if (bLocked)
		{
			AntUiCanvas::Text(Cv, TEXT("?"), CX, CY - Size * 0.24f, EFont::Display, Size * 0.38f, C::Muted, 1);
		}
		else
		{
			// a sticker of the picture on the cap, and a strip of tape over its top edge
			AntUiCanvas::Tile(Cv, Dot, X + Size * 0.22f, Y + Size * 0.22f, Size * 0.56f, Size * 0.56f, FLinearColor(0.98f, 0.95f, 0.88f, 0.95f));
			AntUiCanvas::Icon(Cv, Icon, X + Size * 0.25f, Y + Size * 0.25f, Size * 0.5f);
			AntUiCanvas::Tile(Cv, AntUi::Tex(TEXT("Materials/T_UI_Tape")), X + Size * 0.18f, Y - Size * 0.04f, Size * 0.64f, Size * 0.17f, FLinearColor(1.f, 1.f, 1.f, 0.85f), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), TapeDeg);
		}
	}

	void DrawNest(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC, bool bResults)
	{
		if (!Cv || !PC) { return; }
		AntFighter::LocalTick(PC);
		FCtx C = MakeCtx(Cv, PC, !bResults);
		GTip.Reset();
		if (bResults)
		{
			// the highlights of the raid that just ended, over the debrief's right side
			const AAntFighterState* FS = FSOf(PC);
			const AAntRaid* Raid = AAntRaid::Get(PC->GetWorld());
			if (!FS || !Raid || FS->HighlightsRaid != Raid->RaidNo || FS->Highlights.Num() == 0) { return; }
			// The first 11 s of the results: then it steps aside so the field report can be read in full.
			const AGameStateBase* GSB = PC->GetWorld()->GetGameState();
			const float Left = GSB ? Raid->PhaseEnds - (float)GSB->GetServerWorldTimeSeconds() : 99.f;
			if (Raid->PhaseEnds > 0.f && Left < 6.f && Left > -1.f) { return; }
			const int32 N = FMath::Min(FS->Highlights.Num(), 9);
			const float H0 = 70.f + N * 88.f;
			AntUiCanvas::Glass(Cv, C.X(1350.f), C.Y(110.f), 540.f * C.U, H0 * C.U, 0.9f);
			C.Rect(1350.f, 110.f, 540.f, 3.f, C::AmberDeep);
			C.Txt(TEXT("ИТОГИ БОЙЦОВ"), 1372.f, 124.f, EFont::Display, 22.f, C::Honey, 0, 0.6f);
			const float T = (float)(FPlatformTime::Seconds() - GOpenAt);
			for (int32 i = 0; i < N; ++i)
			{
				const FAntHighlight& H = FS->Highlights[i];
				const float Y0 = 170.f + i * 88.f;
				const uint32 Col = H.Kind == 0 ? 0xF5B735 : H.Kind == 1 ? 0xE58FB0 : H.Kind == 4 ? 0x9A2620 : H.Kind == 3 || H.Kind == 5 ? 0x86C95A : 0xC8731E;
				FString Icon = H.Icon;
				Icon.RemoveFromStart(TEXT("Icons/T_UI_Icon_"));
				Badge(Cv, C.X(1366.f), C.Y(Y0), 62.f * C.U, *Icon, Col, false, -10.f + 7.f * (i % 3));
				C.Txt(FString::Printf(TEXT("%s · %s"), *H.Title, *H.Player), 1442.f, Y0 + 2.f, EFont::Display, 15.f, H.Kind == 4 ? C::Jam : C::Honey);
				C.Wrap(H.Line, 1442.f, Y0 + 24.f, 432.f, EFont::Body, 16.f, C::Sugar, 2);
			}
			return;
		}
		// the two buttons on the nest's header
		if (!IsOpen())
		{
			const FAntProfile& P = AntFighter::Profile();
			if (Btn(C, TEXT("БОЕЦ"), 300.f, 16.f, 140.f, 48.f, true, false, FString::Printf(TEXT("Профиль бойца: %s, опыт %d. Касты и перки, медали, облик"), AntRank::Name(P.Rank()), P.Xp), 20.f)) { Open(PC, Screen::Profile, 0); }
			if (Btn(C, TEXT("КАЗАРМА"), 450.f, 16.f, 140.f, 48.f, true, false, TEXT("Ветераны отряда: кого взять в рейд, доктрина, стена памяти"), 20.f)) { Open(PC, Screen::Roster, 0); }
			AntUiCanvas::Icon(Cv, TEXT("Emblem"), C.X(240.f), C.Y(14.f), 52.f * C.U);
			return;
		}
		// an open screen: the whole sheet
		AntUiCanvas::Rect(Cv, 0.f, 0.f, Cv->ClipX, Cv->ClipY, FLinearColor(0.006f, 0.004f, 0.003f, 0.94f));
		AntUiCanvas::Cover(Cv, AntUi::Tex(GScreen == Screen::Roster ? TEXT("Backgrounds/T_UI_BgNest") : TEXT("Backgrounds/T_UI_BgWarRoom")), 0.f, 0.f, Cv->ClipX, Cv->ClipY, FLinearColor(0.2f, 0.18f, 0.16f, 1.f));
		AntUiCanvas::Tile(Cv, AntUi::Tex(TEXT("Materials/T_UI_Vignette")), 0.f, 0.f, Cv->ClipX, Cv->ClipY, FLinearColor(1.f, 1.f, 1.f, 0.9f));
		C.Txt(GScreen == Screen::Roster ? TEXT("КАЗАРМА") : TEXT("БОЕЦ"), 60.f, 40.f, EFont::Display, 52.f, C::Honey, 0, 1.f);
		if (GScreen == Screen::Profile)
		{
			static const TCHAR* Tabs[4] = { TEXT("Боец"), TEXT("Касты"), TEXT("Медали"), TEXT("Облик") };
			for (int32 t = 0; t < 4; ++t)
			{
				if (Btn(C, Tabs[t], 420.f + t * 230.f, 48.f, 214.f, 60.f, true, t == GTab, FString(), 22.f)) { GTab = t; }
			}
			switch (GTab)
			{
			case 0: TabFighter(C, PC); break;
			case 1: TabCastes(C, PC); break;
			case 2: TabMedals(C, PC); break;
			default: TabLook(C, PC); break;
			}
		}
		else
		{
			DrawRoster(C, PC);
		}
		if (Btn(C, TEXT("НАЗАД"), 1640.f, 40.f, 220.f, 64.f, true, false, TEXT("Вернуться в гнездо"), 22.f) || PC->WasInputKeyJustPressed(EKeys::BackSpace)) { Close(); }
		// the tip line
		if (!GTip.IsEmpty())
		{
			C.Rect(60.f, 1036.f, 1800.f, 38.f, WithA(C::Ink, 0.92f));
			C.Txt(GTip, 76.f, 1042.f, EFont::Body, 19.f, C::Sugar);
		}
		if (AAntHUD::DevClickAt.X > 0.f || AAntHUD::DevClickAt.Y > 0.f)
		{
			AntUiCanvas::Rect(Cv, C.M.X - 9.f, C.M.Y - 1.f, 18.f, 3.f, FLinearColor::White);
			AntUiCanvas::Rect(Cv, C.M.X - 1.f, C.M.Y - 9.f, 3.f, 18.f, FLinearColor::White);
		}
	}

	void DrawRaid(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC)
	{
		if (!Cv || !PC) { return; }
		AntFighter::LocalTick(PC);
		// Veterans' names over them when they are close to the view (the plain squad tags show only on aim / selection).
		if (Hud && PC->PlayerCameraManager)
		{
			const FVector View = PC->PlayerCameraManager->GetCameraLocation();
			const float UU = FMath::Min(Cv->ClipX / 1920.f, Cv->ClipY / 1080.f);
			for (TActorIterator<AAntCharacter> It(PC->GetWorld()); It; ++It)
			{
				if (It->VetName.IsEmpty() || It->bDead || It->IsHidden()) { continue; }
				const FVector P = It->GetActorLocation() + It->GetVisualUp() * 22.f;
				if (FVector::DistSquared(P, View) > FMath::Square(900.f) || FVector::DotProduct(P - View, PC->PlayerCameraManager->GetCameraRotation().Vector()) <= 0.f) { continue; }
				const FVector S = Hud->Project(P, false);
				AntUiCanvas::Text(Cv, It->VetName, S.X, S.Y - 16.f * UU, EFont::Bold, 17.f * UU, C::Honey, 1, 0.8f);
			}
		}
		TArray<FAntFighterToast>& T = Toasts();
		const double Now = FPlatformTime::Seconds();
		T.RemoveAll([Now](const FAntFighterToast& X) { return Now - X.Time > 5.5; });
		const float U = FMath::Min(Cv->ClipX / 1920.f, Cv->ClipY / 1080.f) * AntUi::HudScale();
		for (int32 i = 0; i < T.Num(); ++i)
		{
			const FAntFighterToast& X = T[i];
			const float Age = (float)(Now - X.Time);
			const float A = FMath::Clamp(FMath::Min(Age / 0.25f, (5.5f - Age) / 0.6f), 0.f, 1.f);
			const float Slide = (1.f - FMath::Clamp(Age / 0.3f, 0.f, 1.f)) * 40.f * U;
			const float W = 420.f * U, H = 84.f * U;
			const float X0 = Cv->ClipX - W - 24.f * U + Slide, Y0 = 220.f * U + i * (H + 10.f * U);
			AntUiCanvas::Rect(Cv, X0, Y0, W, H, FLinearColor(0.012f, 0.007f, 0.004f, 0.82f * A));
			AntUiCanvas::Rect(Cv, X0, Y0, 5.f * U, H, WithA(AntUi::Hex(X.Color), A));
			Badge(Cv, X0 + 10.f * U, Y0 + 8.f * U, 68.f * U, *X.Icon, X.Color, false);
			AntUiCanvas::Text(Cv, X.Title, X0 + 90.f * U, Y0 + 10.f * U, EFont::Display, 16.f * U, WithA(X.Kind == 0 ? C::Honey : C::Amber, A));
			AntUiCanvas::TextFit(Cv, X.Line, X0 + 90.f * U, Y0 + 38.f * U, W - 100.f * U, EFont::Bold, 22.f * U, WithA(C::Sugar, A));
		}
	}
}
