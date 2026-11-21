#include "AntUiCanvas.h"
#include "AntUiStyle.h"
#include "AntSnowball.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "CanvasItem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

using namespace AntUi;

namespace AntUiCanvas
{
	namespace
	{
		float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Pow(1.f - X, 3.f); }
		UFont* FontAt(EFont F, float Pt) { return CanvasFont(F, FMath::Clamp(FMath::RoundToInt(Pt), 6, 200)); }
	}

	void Rect(UCanvas* Cv, float X, float Y, float W, float H, const FLinearColor& Col)
	{
		if (!Cv || W <= 0.f || H <= 0.f) { return; }
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), Col);
		T.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(T);
	}

	void Tile(UCanvas* Cv, UTexture2D* Tex, float X, float Y, float W, float H, const FLinearColor& Tint, const FVector2D& UV0, const FVector2D& UV1, float RotDeg)
	{
		if (!Cv || W <= 0.f || H <= 0.f) { return; }
		if (!Tex || !Tex->GetResource())
		{
			return;
		}
		FCanvasTileItem T(FVector2D(X, Y), Tex->GetResource(), FVector2D(W, H), UV0, UV1, Tint);
		T.BlendMode = SE_BLEND_Translucent;
		if (RotDeg != 0.f)
		{
			T.Rotation = FRotator(0.f, RotDeg, 0.f);
			T.PivotPoint = FVector2D(0.5f, 0.5f);
		}
		Cv->DrawItem(T);
	}

	FVector2D TextSize(UCanvas* Cv, const FString& S, EFont F, float Pt)
	{
		float XL = 0.f, YL = 0.f;
		if (UFont* Fnt = FontAt(F, Pt)) { Cv->TextSize(Fnt, S, XL, YL); }
		return FVector2D(XL, YL);
	}

	FVector2D Text(UCanvas* Cv, const FString& S, float X, float Y, EFont F, float Pt, const FLinearColor& Col, int32 Align, float Shadow)
	{
		UFont* Fnt = FontAt(F, Pt);
		if (!Cv || !Fnt || S.IsEmpty()) { return FVector2D::ZeroVector; }
		const FVector2D M = TextSize(Cv, S, F, Pt);
		const float DX = Align == 1 ? X - M.X * 0.5f : Align == 2 ? X - M.X : X;
		FCanvasTextItem T(FVector2D(DX, Y), FText::FromString(S), Fnt, Col);
		if (Shadow > 0.f) { T.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Shadow), FVector2D(1.5f, 2.f)); }
		Cv->DrawItem(T);
		return M;
	}

	FVector2D TextFit(UCanvas* Cv, const FString& S, float X, float Y, float MaxW, EFont F, float Pt, const FLinearColor& Col, int32 Align, float Shadow)
	{
		const FVector2D M = TextSize(Cv, S, F, Pt);
		const float K = M.X > MaxW && M.X > 0.f ? FMath::Max(0.66f, MaxW / M.X) : 1.f;
		return Text(Cv, S, X, Y, F, Pt * K, Col, Align, Shadow);
	}

	float Wrap(UCanvas* Cv, const FString& S, float X, float Y, float MaxW, EFont F, float Pt, const FLinearColor& Col, int32 MaxLines, float LineH, int32 VisibleChars)
	{
		TArray<FString> Words;
		S.ParseIntoArray(Words, TEXT(" "));
		const float H = TextSize(Cv, TEXT("Ау"), F, Pt).Y * LineH;
		int32 Lines = 0;
		int32 Budget = VisibleChars < 0 ? MAX_int32 : VisibleChars;
		FString Line;
		auto Flush = [&]()
		{
			if (Lines < MaxLines && Budget > 0)
			{
				const bool bLast = Lines + 1 == MaxLines;
				FString Out = Line;
				if (Out.Len() > Budget) { Out = Out.Left(Budget); }
				Budget -= Line.Len() + 1;
				Text(Cv, Out, X, Y + Lines * H, F, Pt, Col);
				(void)bLast;
			}
			++Lines;
			Line.Reset();
		};
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (!Line.IsEmpty() && TextSize(Cv, Try, F, Pt).X > MaxW) { Flush(); Line = W; }
			else { Line = Try; }
		}
		if (!Line.IsEmpty()) { Flush(); }
		return FMath::Min(Lines, MaxLines) * H;
	}

	void Frame(UCanvas* Cv, float X, float Y, float W, float H, float Th, const FLinearColor& Col)
	{
		Rect(Cv, X, Y, W, Th, Col); Rect(Cv, X, Y + H - Th, W, Th, Col); Rect(Cv, X, Y, Th, H, Col); Rect(Cv, X + W - Th, Y, Th, H, Col);
	}

	void Paper(UCanvas* Cv, float X, float Y, float W, float H, bool bTape)
	{
		Rect(Cv, X + 6.f, Y + 9.f, W, H, FLinearColor(0.f, 0.f, 0.f, 0.35f));
		if (UTexture2D* P = Tex(TEXT("Materials/T_UI_Paper")))
		{
			Tile(Cv, P, X, Y, W, H, FLinearColor::White, FVector2D(0.f, 0.f), FVector2D(W / 512.f, H / 512.f));
		}
		else
		{
			Rect(Cv, X, Y, W, H, C::Paper);
		}
		if (UTexture2D* Torn = Tex(TEXT("Materials/T_UI_TornEdge")))
		{
			Tile(Cv, Torn, X, Y + H - 2.f, W, 16.f, C::Paper, FVector2D(0.f, 0.f), FVector2D(W / 512.f, 1.f));
		}
		if (bTape)
		{
			UTexture2D* T = Tex(TEXT("Materials/T_UI_Tape"));
			const float TW = FMath::Min(140.f, W * 0.25f), TH = TW * 0.27f;
			Tile(Cv, T, X - TW * 0.25f, Y - TH * 0.35f, TW, TH, FLinearColor(1.f, 1.f, 1.f, 0.9f), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), -30.f);
			Tile(Cv, T, X + W - TW * 0.75f, Y - TH * 0.35f, TW, TH, FLinearColor(1.f, 1.f, 1.f, 0.9f), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), 30.f);
		}
	}

	void Glass(UCanvas* Cv, float X, float Y, float W, float H, float Alpha)
	{
		Rect(Cv, X, Y, W, H, WithA(C::Cocoa, 0.85f * Alpha));
		Rect(Cv, X + 1.5f, Y + 1.5f, W - 3.f, H - 3.f, WithA(C::Ink, Alpha));
	}

	void Cover(UCanvas* Cv, UTexture2D* T, float X, float Y, float W, float H, const FLinearColor& Tint)
	{
		if (!T || W <= 0.f || H <= 0.f) { return; }
		const float Img = (float)T->GetSizeX() / FMath::Max(1, T->GetSizeY());
		const float Box = W / H;
		FVector2D UV0(0.f, 0.f), UV1(1.f, 1.f);
		if (Box < Img) { const float U = Box / Img; UV0.X = 0.5f - U * 0.5f; UV1.X = 0.5f + U * 0.5f; }
		else { const float V = Img / Box; UV0.Y = 0.5f - V * 0.5f; UV1.Y = 0.5f + V * 0.5f; }
		Tile(Cv, T, X, Y, W, H, Tint, UV0, UV1);
	}

	void Icon(UCanvas* Cv, const TCHAR* Name, float X, float Y, float Size, const FLinearColor& Tint)
	{
		Tile(Cv, AntUi::Icon(Name), X, Y, Size, Size, Tint);
	}

	void Stamp(UCanvas* Cv, const FString& S, float CX, float CY, float Deg, const FLinearColor& Col, float Pt)
	{
		const FVector2D M = TextSize(Cv, S, EFont::Display, Pt);
		const float W = M.X + Pt * 1.6f, H = M.Y + Pt * 0.9f;
		if (UTexture2D* B = Tex(TEXT("Materials/T_UI_StampBox")))
		{
			Tile(Cv, B, CX - W * 0.5f, CY - H * 0.5f, W, H, WithA(Col, 0.92f), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), Deg);
		}
		else
		{
			Frame(Cv, CX - W * 0.5f, CY - H * 0.5f, W, H, 4.f, Col);
		}
		Text(Cv, S, CX, CY - M.Y * 0.5f, EFont::Display, Pt, WithA(Col, 0.95f), 1);
	}

	void Bar(UCanvas* Cv, float X, float Y, float W, float H, float Frac, const FLinearColor& Fill)
	{
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		Rect(Cv, X - 1.f, Y - 1.f, W + 2.f, H + 2.f, WithA(C::Cocoa, 0.9f));
		Rect(Cv, X, Y, W, H, WithA(C::Ink, 0.82f));
		const float FW = (W - 2.f) * Frac;
		Rect(Cv, X + 1.f, Y + 1.f, FW, H - 2.f, Fill);
		Rect(Cv, X + 1.f, Y + 1.f, FW, FMath::Max(1.f, (H - 2.f) * 0.3f), FLinearColor(1.f, 1.f, 1.f, 0.22f));
		if (W > 120.f && H >= 8.f)
		{
			for (int32 i = 1; i < 10; ++i) { Rect(Cv, X + W * i / 10.f, Y + H * 0.55f, 1.f, H * 0.45f, FLinearColor(0.f, 0.f, 0.f, 0.35f)); }
		}
	}

	// ------------------------------------------------------------------------------------------------- the card pick

	namespace
	{
		const TCHAR* PickArt(uint8 Id)
		{
			using namespace AntPick;
			switch (Id)
			{
			case Recruit2: case Recruit3: return TEXT("Cards/T_UI_CardRecruit");
			case Veteran: return TEXT("Cards/T_UI_CardVeteran");
			case Workers12: case Workers25: return TEXT("Cards/T_UI_CardWorkers");
			case TrailLong: case TrailSecond: case TrailFast: case TrailHeavy: case TrailForage: return TEXT("Cards/T_UI_CardTrail");
			case Bite: case CardJaws: return TEXT("Cards/T_UI_CardBite");
			case Speed: case CardLegs: case CardSpring: case SquadFast: return TEXT("Cards/T_UI_CardSpeed");
			case Carry: return TEXT("Cards/T_UI_CardCarry");
			case Acid: return TEXT("Cards/T_UI_CardAcid");
			case Grip: return TEXT("Cards/T_UI_CardGrip");
			case SquadStrong: return TEXT("Cards/T_UI_CardSquad");
			case SquadTough: case CardChitin: case CardPadding: return TEXT("Cards/T_UI_CardArmor");
			case ToolDecoy: return TEXT("Cards/T_UI_CardDecoy");
			case ToolAlarm: return TEXT("Cards/T_UI_CardAlarm");
			case ToolBait: return TEXT("Cards/T_UI_CardBait");
			case CardQuiet: return TEXT("Cards/T_UI_CardQuiet");
			case CardLever: return TEXT("Cards/T_UI_CardLever");
			case CardGreedy: case CardOvertime: return TEXT("Cards/T_UI_CardGreed");
			case CardNurse: case CardBrood: return TEXT("Cards/T_UI_CardBrood");
			default: return TEXT("Cards/T_UI_CardRecruit");
			}
		}

		const TCHAR* PoolIcon(uint8 Pool)
		{
			static const TCHAR* Icons[AntPick::PoolCount] = { TEXT("Squad"), TEXT("Worker"), TEXT("Trail"), TEXT("Strength"), TEXT("Shield"), TEXT("Wrench"), TEXT("Decree") };
			return Icons[FMath::Clamp<int32>(Pool, 0, AntPick::PoolCount - 1)];
		}
	}

	FLinearColor PickRarity(uint8 PickId, FString* OutName)
	{
		const uint8 Pool = AntPick::Pool(PickId);
		int32 R = 0;
		switch (Pool)
		{
		case AntPick::PSquad: case AntPick::PWorkers: R = 0; break;
		case AntPick::PTrail: case AntPick::PPersonal: R = 1; break;
		case AntPick::PSquadUp: case AntPick::PTools: R = 2; break;
		default: R = 3; break;
		}
		static const TCHAR* Keys[4] = { TEXT("rarity.common"), TEXT("rarity.rare"), TEXT("rarity.epic"), TEXT("rarity.legend") };
		if (OutName) { *OutName = L(Keys[R]); }
		const FLinearColor Cols[4] = { C::Common, C::Rare, C::Epic, C::Legendary };
		return Cols[R];
	}

	void DrawCardPick(UCanvas* Cv, const TArray<uint8>& Offer, float OfferEnds, float Now, int32 PickNo, const FVector2D& Mouse, bool bMouseOn, FBox2D OutRects[3])
	{
		const float K = FMath::Max(0.7f, Cv->ClipY / 1080.f) * HudScale();
		const float CW = 290.f * K, CH = 420.f * K, Gap = 28.f * K;
		const float RowW = CW * 3.f + Gap * 2.f;
		const float X0 = Cv->ClipX * 0.5f - RowW * 0.5f;
		const float Y0 = Cv->ClipY - CH - 110.f * K;
		const float Left = FMath::Max(0.f, OfferEnds - Now);
		const float Age = FMath::Clamp(10.f - Left, 0.f, 10.f);

		// Header + the timer (masking tape that tears shorter).
		const float HA = Ease(Age / 0.3f);
		Text(Cv, Lf(TEXT("cards.title"), { PickNo }), Cv->ClipX * 0.5f, Y0 - 92.f * K, EFont::Display, 26.f * K, WithA(C::Honey, HA), 1, 0.7f * HA);
		Text(Cv, Lf(TEXT("cards.sub"), { FMath::CeilToInt(Left) }), Cv->ClipX * 0.5f, Y0 - 56.f * K, EFont::Body, 18.f * K, WithA(C::Sugar, 0.9f * HA), 1, 0.7f * HA);
		const FLinearColor TimerC = Left < 3.f ? C::Jam : C::Amber;
		Rect(Cv, X0, Y0 - 22.f * K, RowW, 6.f * K, WithA(C::Ink, 0.6f * HA));
		Rect(Cv, X0, Y0 - 22.f * K, RowW * FMath::Clamp(Left / 10.f, 0.f, 1.f), 6.f * K, WithA(TimerC, HA));

		for (int32 i = 0; i < 3; ++i)
		{
			OutRects[i] = FBox2D(ForceInit);
			if (!Offer.IsValidIndex(i)) { continue; }
			const uint8 Id = Offer[i];
			const float In = Ease((Age - 0.07f * i) / 0.35f);
			float X = X0 + i * (CW + Gap);
			const bool bHover = bMouseOn && FBox2D(FVector2D(X, Y0), FVector2D(X + CW, Y0 + CH)).IsInside(Mouse);
			const float Y = Y0 + (1.f - In) * 220.f * K - (bHover ? 16.f * K : 0.f);
			const float A = In;
			FString RarityName;
			const FLinearColor Rar = PickRarity(Id, &RarityName);

			Rect(Cv, X + 8.f * K, Y + 14.f * K, CW, CH, FLinearColor(0.f, 0.f, 0.f, 0.45f * A));
			if (bHover) { Rect(Cv, X - 6.f * K, Y - 6.f * K, CW + 12.f * K, CH + 12.f * K, WithA(Rar, 0.35f)); }
			Rect(Cv, X, Y, CW, CH, WithA(Rar, A));
			Rect(Cv, X + 5.f * K, Y + 5.f * K, CW - 10.f * K, CH - 10.f * K, WithA(C::Ink, A));
			if (UTexture2D* Board = Tex(TEXT("Materials/T_UI_Cardboard")))
			{
				Tile(Cv, Board, X + 5.f * K, Y + 5.f * K, CW - 10.f * K, CH - 10.f * K, FLinearColor(0.32f, 0.22f, 0.15f, 0.55f * A), FVector2D(0.f, 0.f), FVector2D(0.6f, 0.85f));
			}
			// The picture: a 3:4 painting cropped to the window.
			const float IX = X + 12.f * K, IY = Y + 12.f * K, IW = CW - 24.f * K, IH = CH * 0.55f;
			const float VisH = FMath::Clamp((IH / IW) * 0.75f, 0.2f, 1.f);
			if (UTexture2D* Art = Tex(PickArt(Id)))
			{
				Tile(Cv, Art, IX, IY, IW, IH, FLinearColor(1.f, 1.f, 1.f, A), FVector2D(0.f, 0.12f), FVector2D(1.f, 0.12f + VisH));
			}
			else
			{
				Rect(Cv, IX, IY, IW, IH, WithA(C::Cocoa, A));
			}
			if (UTexture2D* Fade = Tex(TEXT("Materials/T_UI_FadeV")))
			{
				Tile(Cv, Fade, IX, IY + IH * 0.55f, IW, IH * 0.45f + 1.f, WithA(C::Ink, A));
			}
			Frame(Cv, IX, IY, IW, IH, 2.f, WithA(Rar, 0.8f * A));
			// The key: a sugar cube with the number.
			Rect(Cv, X - 10.f * K, Y - 10.f * K, 46.f * K, 46.f * K, WithA(C::Sugar, A));
			Frame(Cv, X - 10.f * K, Y - 10.f * K, 46.f * K, 46.f * K, 2.f, WithA(C::Cocoa, A));
			Text(Cv, FString::FromInt(i + 1), X + 13.f * K, Y - 7.f * K, EFont::Display, 24.f * K, WithA(C::Ink, A), 1);
			// Pool ribbon, title, text, rarity.
			float TY = IY + IH - 26.f * K;
			Icon(Cv, PoolIcon(AntPick::Pool(Id)), IX + 4.f * K, TY - 10.f * K, 40.f * K, FLinearColor(1.f, 1.f, 1.f, A));
			TextFit(Cv, AntPick::PoolName(AntPick::Pool(Id)), IX + 50.f * K, TY, IW - 54.f * K, EFont::Bold, 15.f * K, WithA(Rar, A), 0, 0.8f * A);
			TY = IY + IH + 14.f * K;
			TextFit(Cv, AntPick::Name(Id), X + CW * 0.5f, TY, CW - 30.f * K, EFont::Display, 20.f * K, WithA(C::Sugar, A), 1, 0.5f * A);
			TY += 40.f * K;
			Wrap(Cv, AntPick::Text(Id), X + 18.f * K, TY, CW - 36.f * K, EFont::Body, 15.5f * K, WithA(FMath::Lerp(C::Sugar, C::Muted, 0.35f), A), 4, 1.15f);
			Text(Cv, RarityName, X + CW * 0.5f, Y + CH - 30.f * K, EFont::Bold, 13.f * K, WithA(Rar, 0.9f * A), 1);
			OutRects[i] = FBox2D(FVector2D(X, Y0), FVector2D(X + CW, Y0 + CH));
		}
		Text(Cv, L(TEXT("cards.hint")), Cv->ClipX * 0.5f, Y0 + CH + 24.f * K, EFont::Body, 16.f * K, WithA(C::Muted, 0.85f), 1, 0.7f);
	}

	// ------------------------------------------------------------------------------------------------- the debrief

	void DrawDebrief(UCanvas* Cv, UWorld* World, const FAntRaidResult& R, const AAntGameState* GS, int32 SecondsLeft)
	{
		const float W = Cv->ClipX, H = Cv->ClipY;
		const float K = FMath::Min(W / 1920.f, H / 1080.f);
		const float OX = (W - 1920.f * K) * 0.5f, OY = (H - 1080.f * K) * 0.5f;
		auto X_ = [&](float X) { return OX + X * K; };
		auto Y_ = [&](float Y) { return OY + Y * K; };
		const float Now = World ? World->GetRealTimeSeconds() : 0.f;
		static int32 SeenRaid = -1;
		static float SeenAt = 0.f;
		if (SeenRaid != R.RaidNo || Now < SeenAt) { SeenRaid = R.RaidNo; SeenAt = Now; }
		const float T = Now - SeenAt;
		const bool bGood = R.End == AntRaidEnd::Left;
		const bool bWiped = R.End == AntRaidEnd::Wiped;
		const bool bCake = R.Cake == AntCake::Home && !bWiped;

		// ---- the newsreel plate: sepia film, grain, scratches, a little flicker
		Rect(Cv, 0.f, 0.f, W, H, C::Ink);
		if (UTexture2D* Plate = Tex(bGood ? TEXT("Backgrounds/T_UI_NewsWin") : TEXT("Backgrounds/T_UI_NewsLose")))
		{
			const float Img = 1920.f / 1080.f, Scr = W / H;
			const float Z = 1.03f + T * 0.004f;
			FVector2D UV0(0.f, 0.f), UV1(1.f, 1.f);
			if (Scr > Img) { const float V = Img / Scr / Z; UV0 = FVector2D((1.f - 1.f / Z) * 0.5f, 0.5f - V * 0.5f); UV1 = FVector2D(1.f - UV0.X, 0.5f + V * 0.5f); }
			else { const float U = Scr / Img / Z; UV0 = FVector2D(0.5f - U * 0.5f, (1.f - 1.f / Z) * 0.5f); UV1 = FVector2D(0.5f + U * 0.5f, 1.f - UV0.Y); }
			const float Flick = 0.86f + 0.06f * FMath::Sin(T * 23.f) * FMath::Sin(T * 7.3f);
			Tile(Cv, Plate, 0.f, 0.f, W, H, FLinearColor(Flick, Flick * 0.97f, Flick * 0.9f, 1.f), UV0, UV1);
		}
		Rect(Cv, 0.f, 0.f, W, H, FLinearColor(0.05f, 0.03f, 0.015f, 0.42f));
		if (UTexture2D* Grain = Tex(TEXT("Materials/T_UI_Grain")))
		{
			const float J = FMath::Frac(FMath::FloorToFloat(T * 12.f) * 0.618f);
			Tile(Cv, Grain, 0.f, 0.f, W, H, FLinearColor(1.f, 0.95f, 0.85f, 0.18f), FVector2D(J, J * 1.7f), FVector2D(J + W / 256.f, J * 1.7f + H / 256.f));
		}
		if (UTexture2D* Scr = Tex(TEXT("Materials/T_UI_Scratches")))
		{
			const float J = FMath::Frac(FMath::FloorToFloat(T * 8.f) * 0.371f);
			Tile(Cv, Scr, 0.f, 0.f, W, H, FLinearColor(1.f, 1.f, 1.f, 0.35f), FVector2D(J, 0.f), FVector2D(J + 1.f, 1.f));
		}
		Tile(Cv, Tex(TEXT("Materials/T_UI_Vignette")), 0.f, 0.f, W, H, FLinearColor(1.f, 1.f, 1.f, 0.95f));
		Rect(Cv, 0.f, 0.f, W, Y_(44.f), FLinearColor(0.f, 0.f, 0.f, 0.9f));
		Rect(Cv, 0.f, H - (H - Y_(1036.f)), W, H - Y_(1036.f), FLinearColor(0.f, 0.f, 0.f, 0.9f));

		// ---- left: the reel's title card, the headline, the hero, the Queen
		const float LA = Ease(T / 0.6f);
		Text(Cv, L(TEXT("radio.name")), X_(90.f), Y_(80.f), EFont::Bold, 20.f * K, WithA(C::Honey, LA), 0, 0.8f);
		Text(Cv, L(TEXT("news.title")), X_(90.f), Y_(110.f), EFont::Display, 50.f * K, WithA(C::Sugar, LA), 0, 0.8f);
		Text(Cv, Lf(TEXT("news.issue"), { R.RaidNo, FString(AntTier::Def(FMath::Clamp<int32>(R.Tier, 1, AntTier::Max)).Name) }), X_(92.f), Y_(184.f), EFont::Mono, 18.f * K, WithA(C::Sugar, 0.85f * LA), 0, 0.8f);
		const FString Headline = bCake ? L(TEXT("news.cake")) : bGood ? L(TEXT("news.left")) : bWiped ? L(TEXT("news.wiped")) : L(TEXT("news.timeout"));
		const float HA = Ease((T - 0.4f) / 0.5f);
		Wrap(Cv, Headline, X_(90.f), Y_(240.f), 760.f * K, EFont::Display, 40.f * K, WithA(bGood ? C::Honey : C::Sugar, HA), 3, 1.1f);

		// The hero of the raid: whoever brought the most.
		{
			const AAntPlayerState* Best = nullptr;
			if (GS)
			{
				for (APlayerState* P : GS->PlayerArray)
				{
					const AAntPlayerState* A = Cast<AAntPlayerState>(P);
					if (A && (!Best || A->Delivered > Best->Delivered)) { Best = A; }
				}
			}
			const float MA = Ease((T - 1.0f) / 0.5f);
			const float MX = X_(90.f), MY = Y_(470.f);
			Glass(Cv, MX, MY, 760.f * K, 190.f * K, 0.7f * MA);
			if (Best && Best->Delivered > 0)
			{
				Tile(Cv, Tex(CastePortrait((int32)Best->AntClass)), MX + 16.f * K, MY + 16.f * K, 158.f * K, 158.f * K, FLinearColor(1.f, 1.f, 1.f, MA));
				Frame(Cv, MX + 16.f * K, MY + 16.f * K, 158.f * K, 158.f * K, 2.f, WithA(C::Amber, MA));
				Text(Cv, L(TEXT("news.mvp")), MX + 196.f * K, MY + 20.f * K, EFont::Bold, 18.f * K, WithA(C::Amber, MA));
				TextFit(Cv, Best->GetPlayerName(), MX + 196.f * K, MY + 50.f * K, 540.f * K, EFont::Display, 32.f * K, WithA(C::Sugar, MA));
				Wrap(Cv, Lf(TEXT("news.mvp.line"), { Best->Delivered, CasteName((int32)Best->AntClass) }), MX + 196.f * K, MY + 104.f * K, 540.f * K, EFont::Body, 20.f * K, WithA(C::Sugar, 0.85f * MA), 2);
				Icon(Cv, TEXT("Medal"), MX + 690.f * K, MY + 14.f * K, 56.f * K, FLinearColor(1.f, 1.f, 1.f, MA));
			}
			else
			{
				Wrap(Cv, L(TEXT("news.mvp.none")), MX + 24.f * K, MY + 60.f * K, 700.f * K, EFont::Hand, 26.f * K, WithA(C::Sugar, MA), 3);
			}
		}
		// The Queen's resolution, by her mood.
		{
			const float QA = Ease((T - 1.6f) / 0.6f);
			const int32 Mood = R.MoodAfter;
			const TCHAR* Key = Mood <= 25 ? TEXT("queen.rage") : Mood < 60 ? TEXT("queen.calm") : Mood < 85 ? TEXT("queen.glad") : TEXT("queen.joy");
			const float QX = X_(90.f), QY = Y_(690.f);
			Tile(Cv, Tex(TEXT("Portraits/T_UI_PorQueen")), QX, QY, 150.f * K, 150.f * K, FLinearColor(1.f, 1.f, 1.f, QA));
			Text(Cv, L(TEXT("news.queen")), QX + 170.f * K, QY + 4.f * K, EFont::Bold, 18.f * K, WithA(C::Amber, QA));
			Text(Cv, FString::Printf(TEXT("%s  (%s%d)"), AntNest::MoodName(Mood), Mood - R.MoodBefore >= 0 ? TEXT("+") : TEXT(""), Mood - R.MoodBefore), QX + 170.f * K, QY + 32.f * K, EFont::Body, 19.f * K, WithA(C::Sugar, 0.8f * QA));
			Wrap(Cv, L(Key), QX + 170.f * K, QY + 64.f * K, 600.f * K, EFont::Hand, 27.f * K, WithA(C::Honey, QA), 3, 1.15f);
		}

		// ---- right: the field report, typed out
		const float PX = X_(930.f), PY = Y_(76.f), PW = 900.f * K, PH = 920.f * K;
		const float PA = Ease((T - 0.2f) / 0.5f);
		const float Slide = (1.f - PA) * 60.f * K;
		Paper(Cv, PX + Slide, PY, PW, PH);
		const float X = PX + Slide + 46.f * K;
		float Y = PY + 40.f * K;
		const FLinearColor Ink = C::Graphite;
		const FLinearColor Dim = WithA(C::Graphite, 0.65f);
		Text(Cv, Lf(TEXT("report.title"), { R.RaidNo }), X, Y, EFont::Display, 28.f * K, Ink);
		Y += 44.f * K;
		Text(Cv, Lf(TEXT("report.theatre"), { FString(AntTier::Def(FMath::Clamp<int32>(R.Tier, 1, AntTier::Max)).Name), R.SeedText.IsEmpty() ? FString(TEXT("—")) : R.SeedText }), X, Y, EFont::Mono, 16.f * K, Dim);
		Y += 34.f * K;
		Rect(Cv, X, Y, PW - 92.f * K, 2.f * K, WithA(C::Graphite, 0.5f));
		Y += 16.f * K;
		// The course of the operation: the story lines, one by one, at typewriter speed.
		Text(Cv, L(TEXT("report.course")), X, Y, EFont::Bold, 18.f * K, Ink);
		Y += 32.f * K;
		int32 Budget = FMath::Max(0, FMath::FloorToInt((T - 0.7f) * 70.f));
		for (int32 i = 0; i < FMath::Min(R.Story.Num(), 9); ++i)
		{
			if (Budget <= 0) { break; }
			const FString& Line = R.Story[i];
			const FString Shown = Line.Len() > Budget ? Line.Left(Budget) : Line;
			Budget -= Line.Len();
			TextFit(Cv, Shown, X, Y, PW - 92.f * K, EFont::Mono, 16.f * K, Ink);
			Y += 26.f * K;
		}
		if (R.Story.Num() == 0) { Text(Cv, L(TEXT("report.quiet")), X, Y, EFont::Mono, 16.f * K, Dim); Y += 26.f * K; }
		Y = FMath::Max(Y, PY + 440.f * K);
		Rect(Cv, X, Y, PW - 92.f * K, 2.f * K, WithA(C::Graphite, 0.5f));
		Y += 16.f * K;
		const float NA = Ease((T - 2.0f) / 0.5f);
		const float ColW = (PW - 92.f * K) * 0.5f;
		auto Row = [&](float RX, float& RY, const FString& Label, const FString& Value, const FLinearColor& VC)
		{
			Text(Cv, Label, RX, RY, EFont::Mono, 16.f * K, WithA(Dim, NA));
			TextFit(Cv, Value, RX + ColW - 24.f * K, RY - 4.f * K, ColW * 0.5f, EFont::Display, 22.f * K, WithA(VC, NA), 2);
			RY += 36.f * K;
		};
		float Y1 = Y, Y2 = Y;
		Text(Cv, L(TEXT("report.trophies")), X, Y1, EFont::Bold, 18.f * K, WithA(Ink, NA)); Y1 += 34.f * K;
		Row(X, Y1, L(TEXT("report.food")), FString::Printf(TEXT("+%d"), R.Kept), bWiped ? C::JamDeep : C::LeafDeep);
		Row(X, Y1, L(TEXT("report.jelly")), FString::Printf(TEXT("+%d"), R.Jelly), C::JellyDeep);
		Row(X, Y1, L(TEXT("report.ask")), FString::Printf(TEXT("%d / %d"), R.Kept, R.Ask), R.Kept >= R.Ask ? C::LeafDeep : Ink);
		Row(X, Y1, L(TEXT("report.cake")), bCake ? L(TEXT("report.cake.home")) : L(TEXT("report.cake.lost")), bCake ? C::LeafDeep : C::JamDeep);
		const float X2 = X + ColW + 24.f * K;
		Text(Cv, L(TEXT("report.losses")), X2, Y2, EFont::Bold, 18.f * K, WithA(Ink, NA)); Y2 += 34.f * K;
		Row(X2, Y2, L(TEXT("report.deaths")), FString::FromInt(R.Deaths), R.Deaths > 0 ? C::JamDeep : C::LeafDeep);
		Row(X2, Y2, L(TEXT("report.lost")), FString::FromInt(R.Lost), R.Lost > 0 ? C::JamDeep : C::LeafDeep);
		Row(X2, Y2, L(TEXT("report.squad")), FString::Printf(TEXT("%d / %d"), R.SquadBack, R.SquadOut), R.SquadBack >= R.SquadOut ? C::LeafDeep : C::JamDeep);
		Row(X2, Y2, L(TEXT("report.larvae")), R.LarvaeLaid > 0 ? FString::Printf(TEXT("%d (+%d)"), R.LarvaeLeft, R.LarvaeLaid) : FString::FromInt(R.LarvaeLeft), R.LarvaeLeft > 0 ? Ink : C::JamDeep);
		Y = FMath::Max(Y1, Y2) + 6.f * K;
		// A couple of funny lines: the awards of the shift, or the report's own remarks.
		const float FA = Ease((T - 2.6f) / 0.5f);
		TArray<FString> Fun;
		if (GS) { for (int32 i = 0; i < FMath::Min(GS->Awards.Num(), 2); ++i) { Fun.Add(GS->Awards[i]); } }
		if (R.QueenAte > 0) { Fun.Add(Lf(TEXT("report.fun.ate"), { R.QueenAte })); }
		if (R.NewTier > 0) { Fun.Add(Lf(TEXT("report.fun.tier"), { FString(AntTier::Def(FMath::Clamp<int32>(R.NewTier, 1, AntTier::Max)).Name) })); }
		if (Fun.Num() < 2 && R.Deaths == 0) { Fun.Add(L(TEXT("report.fun.nodeaths"))); }
		if (Fun.Num() < 2 && R.Deaths >= 3) { Fun.Add(L(TEXT("report.fun.deaths"))); }
		for (int32 i = 0; i < FMath::Min(Fun.Num(), 3); ++i)
		{
			TextFit(Cv, TEXT("— ") + Fun[i], X, Y, PW - 92.f * K, EFont::Hand, 21.f * K, WithA(C::JamDeep, FA));
			Y += 30.f * K;
		}
		// The verdict stamp.
		const float SA = Ease((T - 2.2f) / 0.25f);
		if (SA > 0.f)
		{
			const float Pop = 1.f + 0.5f * (1.f - SA);
			Stamp(Cv, bCake ? L(TEXT("stamp.cake")) : bGood ? L(TEXT("stamp.left")) : bWiped ? L(TEXT("stamp.wiped")) : L(TEXT("stamp.timeout")),
				PX + Slide + PW - 210.f * K, PY + 380.f * K, -8.f, WithA(bGood ? C::LeafDeep : C::JamDeep, SA), 26.f * K * Pop);
		}
		Text(Cv, Lf(TEXT("report.next"), { SecondsLeft }), W * 0.5f, Y_(1046.f), EFont::Body, 18.f * K, WithA(C::Sugar, 0.85f), 1);
	}

	// ------------------------------------------------------------------------------------------------- dev tests

	void DrawTests(AHUD* Hud, UCanvas* Cv)
	{
		static FString Test;
		static bool bRead = false;
		if (!bRead) { bRead = true; FParse::Value(FCommandLine::Get(), TEXT("AntUiTest="), Test); }
		if (Test.IsEmpty() || !Hud || !Cv) { return; }
		UWorld* World = Hud->GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		if (Test == TEXT("cards"))
		{
			static float Start = -1.f;
			if (Start < 0.f || Now - Start > 12.f) { Start = Now; }
			TArray<uint8> Offer = { (uint8)AntPick::Recruit3, (uint8)AntPick::TrailFast, (uint8)AntPick::CardQuiet };
			FBox2D Rects[3];
			DrawCardPick(Cv, Offer, Start + 10.f, Now, 3, FVector2D(Cv->ClipX * 0.5f, Cv->ClipY - 300.f), true, Rects);
		}
		else if (Test.StartsWith(TEXT("debrief")))
		{
			const bool bLose = Test.Contains(TEXT("lose"));
			FAntRaidResult R;
			R.End = bLose ? AntRaidEnd::Wiped : AntRaidEnd::Left;
			R.RaidNo = 14; R.Tier = 2; R.Banked = 812; R.Kept = bLose ? 406 : 812; R.Ask = 600; R.Jelly = 3;
			R.Cake = bLose ? AntCake::Clan : AntCake::Home; R.Deaths = bLose ? 4 : 2; R.Lost = bLose ? 1 : 0; R.SquadOut = 9; R.SquadBack = bLose ? 3 : 8;
			R.MoodBefore = 52; R.MoodAfter = bLose ? 21 : 71; R.LarvaeLeft = bLose ? 0 : 3; R.LarvaeLaid = bLose ? 0 : 2; R.QueenAte = bLose ? 60 : 0;
			R.SeedText = TEXT("4471 · Будний день");
			R.Story = { TEXT("0:14 — Объект «Именины» найден на Верхнем Леднике"), TEXT("2:31 — Гигант вышел на Передовую"), TEXT("4:05 — Засада Рыжих у порога отбита"),
				TEXT("6:12 — Гигант пал на Катке (2,5 с)"), bLose ? TEXT("7:02 — Отряд накрыла Белая Погода") : TEXT("7:35 — Объект «Именины» доставлен в Щель") };
			DrawDebrief(Cv, World, R, World ? World->GetGameState<AAntGameState>() : nullptr, 12);
		}
	}
}
