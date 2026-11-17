#include "AntHUD.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const FLinearColor RWhite(1.f, 1.f, 1.f);
	const FLinearColor RGrey(0.7f, 0.7f, 0.7f);
	const FLinearColor RGold(1.f, 0.8f, 0.2f);
	const FLinearColor RRed(1.f, 0.3f, 0.25f);
	const FLinearColor RGreen(0.4f, 1.f, 0.4f);
	const FLinearColor RPanel(0.f, 0.f, 0.f, 0.55f);

	/** Dev: -AntCardTest shows the card vote with made-up cards (drawn only), to look at the screen without winning a shift. */
	bool CardTest()
	{
		static const bool b = FParse::Param(FCommandLine::Get(), TEXT("AntCardTest"));
		return b;
	}
}

// ---------------------------------------------------------------------------------------------
// During the shift: orders, cards, greed, the event, the Human's destination, pings, first-shift hints
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawRun(AAntGameState* GS, APawn* Me)
{
	if (!GS || !Canvas)
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Now = GetWorld()->GetTimeSeconds();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	if (FirstDrawTime < 0.f)
	{
		FirstDrawTime = Now;
	}
	const AAntCharacter* Ant = Cast<AAntCharacter>(Me);
	const AAntPlayerState* PS = PlayerOwner ? PlayerOwner->GetPlayerState<AAntPlayerState>() : nullptr;
	const uint8 MyTeam = Ant ? Ant->Team : (PS ? PS->Team : 0);

	// --- Pings from the team: a marker with a word, a few seconds ---
	for (const FAntPingMark& P : GS->Pings)
	{
		const float Age = Now - P.Time;
		if (Age < 0.f || Age > 6.f || P.Team != MyTeam)
		{
			continue;
		}
		const FLinearColor C = P.Kind == AntPing::Human ? RRed : P.Kind == AntPing::Trophy ? RGold : P.Kind == AntPing::Door ? FLinearColor(0.6f, 1.f, 0.9f) : RWhite;
		Marker(P.Loc + FVector(0.f, 0.f, 30.f), FString::Printf(TEXT("%s  ·  %s"), AntPing::Name(P.Kind), *P.Who), FLinearColor(C.R, C.G, C.B, FMath::Clamp((6.f - Age) / 1.5f, 0.f, 1.f)), true);
	}

	if (!GS->bHeistMode)
	{
		return;
	}
	if (GS->Phase != 0 || CardTest())
	{
		DrawInterlude(GS);
		if (!CardTest())
		{
			return;
		}
	}
	const float ServerNow = GS->GetServerWorldTimeSeconds();

	// --- Under the shift panel: greed, the running event ---
	float RowY = 128.f * S;
	if (GS->bQuotaMet)
	{
		const float Beat = 0.8f + 0.2f * FMath::Abs(FMath::Sin(Now * 3.f));
		const FString G = GS->Greed > 1.001f
			? FString::Printf(TEXT("ЖАДНОСТЬ ×%.1f  ·  всё, что принесёте сейчас, дороже"), GS->Greed)
			: FString(TEXT("ЖАДНОСТЬ ×1.0  ·  пока кто-то снаружи, множитель растёт"));
		Txt(G, W * 0.5f, RowY, FLinearColor(1.f, 0.8f, 0.2f, Beat), Small, 1.15f * S, true);
		RowY += 24.f * S;
	}
	if (GS->EventId != SeenEvent)
	{
		SeenEvent = GS->EventId;
		if (GS->EventId != 0)
		{
			Announce(AntEvent::Name(GS->EventId), AntEvent::Text(GS->EventId), FLinearColor(0.55f, 0.85f, 1.f));
		}
	}
	if (GS->EventId != 0)
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(GS->EventEnds - ServerNow));
		Txt(FString::Printf(TEXT("%s  ·  ещё %d с"), AntEvent::Name(GS->EventId), Left), W * 0.5f, RowY, FLinearColor(0.55f, 0.85f, 1.f), Small, 1.15f * S, true);
		RowY += 24.f * S;
		if (GS->EventId == AntEvent::Pizza || GS->EventId == AntEvent::Flood)
		{
			Marker(FVector(GS->EventLoc) + FVector(0.f, 0.f, 120.f), GS->EventId == AntEvent::Pizza ? TEXT("Пицца") : TEXT("Лужа"), FLinearColor(0.55f, 0.85f, 1.f), false);
		}
	}
	// Where the Human is heading (a noise, a door): a small mark there.
	if (GS->ShiftStage >= 1 && (GS->HumanGoal == AntHumanGoal::Noise || GS->HumanGoal == AntHumanGoal::Shut))
	{
		Marker(FVector(GS->HumanGoalLoc) + FVector(0.f, 0.f, 80.f), TEXT("сюда идёт Человек"), FLinearColor(1.f, 0.6f, 0.4f), false);
	}

	// --- Left: the Queen's orders and the colony's cards, small ---
	float X = 30.f * S, Y = 150.f * S;
	if (GS->Orders.Num() > 0)
	{
		Txt(TEXT("ПРИКАЗЫ КОРОЛЕВЫ"), X, Y, FLinearColor(1.f, 0.8f, 0.2f, 0.9f), Small, 1.05f * S);
		Y += 22.f * S;
		for (int32 i = 0; i < GS->Orders.Num(); ++i)
		{
			const bool bDone = (GS->OrdersDone & (1 << i)) != 0;
			const bool bFail = (GS->OrdersFailed & (1 << i)) != 0;
			const float Box = 12.f * S;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y + 3.f * S, Box, Box);
			if (bDone)
			{
				DrawRect(RGreen, X + 2.f * S, Y + 5.f * S, Box - 4.f * S, Box - 4.f * S);
			}
			else if (bFail)
			{
				DrawRect(RRed, X + 2.f * S, Y + 8.f * S, Box - 4.f * S, 2.f * S);
			}
			else
			{
				DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), X + 1.f, Y + 3.f * S + 1.f, Box - 2.f, 1.f);
				DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), X + 1.f, Y + 3.f * S + Box - 2.f, Box - 2.f, 1.f);
			}
			Txt(AntOrder::Name(GS->Orders[i]), X + Box + 8.f * S, Y, bDone ? RGreen : bFail ? FLinearColor(0.6f, 0.45f, 0.45f, 0.8f) : FLinearColor(1.f, 1.f, 1.f, 0.85f), Small, 1.f * S);
			Y += 20.f * S;
		}
		Y += 8.f * S;
	}
	if (GS->Cards.Num() > 0)
	{
		FString Line = TEXT("Карты: ");
		for (int32 i = 0; i < GS->Cards.Num(); ++i)
		{
			Line += (i > 0 ? TEXT(" · ") : TEXT("")) + FString(AntCard::Name(GS->Cards[i]));
			if (i % 3 == 2 && i + 1 < GS->Cards.Num())
			{
				Txt(Line, X, Y, FLinearColor(0.75f, 0.9f, 1.f, 0.85f), Small, 0.95f * S);
				Y += 18.f * S;
				Line = TEXT("             ");
			}
		}
		Txt(Line, X, Y, FLinearColor(0.75f, 0.9f, 1.f, 0.85f), Small, 0.95f * S);
		Y += 24.f * S;
	}

	// --- First shift only: three steps for somebody who has never played ---
	if (Ant && Ant->Creature == 0 && Ant->Team == 0 && GS->Shift == 1 && GS->RunShifts == 0 && GS->Phase == 0)
	{
		if (HintShift != GS->Shift)
		{
			HintShift = GS->Shift;
			bHintOut = bHintGrab = bHintHome = false;
			HintsDoneAt = -1.f;
		}
		if (!Ant->bDead)
		{
			if (!bHintOut && !Ant->IsInNest()) { bHintOut = true; }
			if (bHintOut && !bHintGrab && Ant->Carrying && !Ant->Carrying->bIsGear) { bHintGrab = true; }
			if (bHintGrab && !bHintHome && PS && PS->Delivered > 0) { bHintHome = true; HintsDoneAt = Now; }
		}
		const float Fade = HintsDoneAt < 0.f ? 1.f : FMath::Clamp((HintsDoneAt + 6.f - Now) / 2.f, 0.f, 1.f);
		if (Fade > 0.f)
		{
			const float HX = 30.f * S;
			float HY = H * 0.42f;
			Txt(bHintHome ? TEXT("ТАК И РАБОТАЕМ. Дальше сам") : TEXT("ПЕРВАЯ СМЕНА"), HX, HY, FLinearColor(1.f, 0.8f, 0.2f, Fade), Med, 0.9f * S);
			HY += 34.f * S;
			struct FStep { const TCHAR* Text; bool bDone; };
			const FStep Steps[] =
			{
				{ TEXT("1. Выйди из гнезда: щель в стене ведёт на кухню"), bHintOut },
				{ TEXT("2. Подойди к крошке на полу и схвати её — [E]"), bHintGrab },
				{ TEXT("3. Дотащи её в гнездо: метка «МУРАВЕЙНИК»"), bHintHome },
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
// Between shifts: the awards under the result banner, then the card vote
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawInterlude(AAntGameState* GS)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Now = GetWorld()->GetTimeSeconds();
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	const bool bTest = CardTest();
	const int32 Stage = bTest ? 2 : GS->Interlude;

	if (Stage <= 1 && !bTest)
	{
		// The result banner (DrawTop) sits at 0.3 H, 200 tall: orders and awards go right under it.
		float Y = H * 0.3f + 212.f * S;
		const int32 Lines = GS->Orders.Num() + GS->Awards.Num() + (GS->Awards.Num() > 0 ? 1 : 0) + (GS->Orders.Num() > 0 ? 1 : 0);
		if (Lines > 0)
		{
			DrawRect(RPanel, W * 0.5f - 430.f * S, Y - 8.f * S, 860.f * S, Lines * 26.f * S + 20.f * S);
		}
		if (GS->Orders.Num() > 0)
		{
			Txt(TEXT("ПРИКАЗЫ КОРОЛЕВЫ"), W * 0.5f, Y, RGold, Small, 1.15f * S, true);
			Y += 26.f * S;
			for (int32 i = 0; i < GS->Orders.Num(); ++i)
			{
				const bool bDone = (GS->OrdersDone & (1 << i)) != 0;
				Txt(FString::Printf(TEXT("%s  —  %s"), AntOrder::Name(GS->Orders[i]), bDone ? TEXT("выполнен") : TEXT("нет")), W * 0.5f, Y, bDone ? RGreen : RGrey, Small, 1.1f * S, true);
				Y += 26.f * S;
			}
		}
		if (GS->Awards.Num() > 0)
		{
			Txt(TEXT("НАГРАДЫ СМЕНЫ"), W * 0.5f, Y, RGold, Small, 1.15f * S, true);
			Y += 26.f * S;
			for (const FString& A : GS->Awards)
			{
				Txt(A, W * 0.5f, Y, RWhite, Small, 1.15f * S, true);
				Y += 26.f * S;
			}
		}
		return;
	}

	// The card vote: three cards side by side, 1 / 2 / 3 to vote, the clock under them.
	TArray<uint8> Offer = GS->CardOffer;
	TArray<uint8> Votes = GS->CardVotes;
	uint8 Picked = GS->CardPicked;
	if (bTest)
	{
		Offer = { AntCard::Lever, AntCard::Brood, AntCard::Spring };
		Votes = { 2, 1, 0 };
		Picked = 255;
	}
	if (Offer.Num() == 0)
	{
		return;
	}
	const AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, 0.f, W, H);
	Txt(Stage == 3 ? TEXT("КАРТА КОЛОНИИ") : TEXT("ВЫБЕРИТЕ КАРТУ ДЛЯ КОЛОНИИ"), W * 0.5f, H * 0.2f, RGold, Big, 1.9f * S, true);
	Txt(Stage == 3 ? TEXT("Действует до конца забега") : TEXT("Голосуйте клавишами 1, 2, 3. Побеждает большинство, при равенстве решает Королева"), W * 0.5f, H * 0.2f + 64.f * S, RGrey, Small, 1.15f * S, true);
	const float CW = 330.f * S, CH = 210.f * S, Gap = 28.f * S;
	const float Total = Offer.Num() * CW + (Offer.Num() - 1) * Gap;
	const float X0 = W * 0.5f - Total * 0.5f, Y0 = H * 0.36f;
	for (int32 i = 0; i < Offer.Num(); ++i)
	{
		const float X = X0 + i * (CW + Gap);
		const bool bWinner = Stage == 3 && Offer[i] == Picked;
		const bool bMine = PC && PC->MyVote == i && Stage == 2 && !bTest;
		const float Dim = (Stage == 3 && !bWinner) ? 0.35f : 1.f;
		DrawRect(FLinearColor(0.07f, 0.06f, 0.04f, 0.92f * Dim), X, Y0, CW, CH);
		const FLinearColor Edge = bWinner ? RGold : bMine ? RGreen : FLinearColor(0.75f, 0.9f, 1.f, 0.7f);
		DrawRect(FLinearColor(Edge.R, Edge.G, Edge.B, Edge.A * Dim), X, Y0, CW, (bWinner || bMine ? 5.f : 3.f) * S);
		DrawRect(FLinearColor(Edge.R, Edge.G, Edge.B, Edge.A * Dim), X, Y0 + CH - 3.f * S, CW, 3.f * S);
		Txt(FString::Printf(TEXT("[%d]"), i + 1), X + 16.f * S, Y0 + 14.f * S, FLinearColor(1.f, 0.8f, 0.2f, Dim), Med, 1.1f * S);
		float NW = 0.f, NH = 0.f;
		GetTextSize(AntCard::Name(Offer[i]), NW, NH, Med, 1.3f * S);
		const float NameFit = FMath::Min(1.f, (CW - 90.f * S) / FMath::Max(1.f, NW));
		Txt(AntCard::Name(Offer[i]), X + 74.f * S, Y0 + 12.f * S, FLinearColor(1.f, 1.f, 1.f, Dim), Med, 1.3f * S * NameFit);
		// The description, wrapped by hand into the card.
		{
			TArray<FString> Words;
			FString(AntCard::Text(Offer[i])).ParseIntoArray(Words, TEXT(" "));
			FString Line;
			float LY = Y0 + 78.f * S;
			for (const FString& Wd : Words)
			{
				const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
				float TW = 0.f, TH = 0.f;
				GetTextSize(Try, TW, TH, Small, 1.15f * S);
				if (TW > CW - 32.f * S && !Line.IsEmpty())
				{
					Txt(Line, X + 16.f * S, LY, FLinearColor(0.85f, 0.85f, 0.85f, Dim), Small, 1.15f * S);
					LY += 24.f * S;
					Line = Wd;
				}
				else
				{
					Line = Try;
				}
			}
			Txt(Line, X + 16.f * S, LY, FLinearColor(0.85f, 0.85f, 0.85f, Dim), Small, 1.15f * S);
		}
		const int32 V = Votes.IsValidIndex(i) ? Votes[i] : 0;
		for (int32 v = 0; v < FMath::Min(V, 8); ++v)
		{
			DrawRect(FLinearColor(1.f, 0.8f, 0.2f, Dim), X + 16.f * S + v * 20.f * S, Y0 + CH - 34.f * S, 14.f * S, 14.f * S);
		}
		if (bMine)
		{
			TxtRight(TEXT("твой голос"), X + CW - 14.f * S, Y0 + CH - 38.f * S, RGreen, Small, 1.05f * S);
		}
		else if (bWinner)
		{
			TxtRight(TEXT("выбрана"), X + CW - 14.f * S, Y0 + CH - 38.f * S, RGold, Small, 1.1f * S);
		}
	}
	if (Stage == 2)
	{
		const int32 Left = bTest ? 14 : FMath::Max(0, FMath::CeilToInt(GS->InterludeEnds - GS->GetServerWorldTimeSeconds()));
		Txt(FString::Printf(TEXT("Осталось %d с"), Left), W * 0.5f, Y0 + CH + 26.f * S, Left <= 5 ? RRed : RWhite, Med, 1.1f * S, true);
	}
	if (GS->Cards.Num() > 0 && Stage == 2)
	{
		FString Owned = TEXT("Уже есть: ");
		for (int32 i = 0; i < GS->Cards.Num(); ++i)
		{
			Owned += (i > 0 ? TEXT(" · ") : TEXT("")) + FString(AntCard::Name(GS->Cards[i]));
		}
		Txt(Owned, W * 0.5f, Y0 + CH + 70.f * S, FLinearColor(0.75f, 0.9f, 1.f, 0.85f), Small, 1.05f * S, true);
	}
}