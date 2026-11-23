#include "AntColonyView.h"
#include "AntColony.h"
#include "AntMetaBus.h"
#include "AntNest.h"
#include "AntRaid.h"
#include "AntHUD.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// The colony screen (AntColonyView.h). One cutaway of 1920x1080 "colony units", zoomed and panned into the screen; everything is
// drawn on the Canvas from the textures under /Game/UI/Colony (docs/COLONY.md lists them), and falls back to flat shapes when a
// texture is missing, so the screen always works.

using namespace AntUi;

namespace
{
	constexpr float WW = 1920.f, WH = 1080.f;
	constexpr int32 Entrance = AntColony::Count;   // graph node of the way in (the gap under the baseboard)

	struct FEdge { int32 A, B; int32 Dig; };
	const FEdge Edges[] =
	{
		{ Entrance, AntColony::Throne, -1 },
		{ AntColony::Throne, AntColony::Nursery, -1 },
		{ AntColony::Throne, AntColony::Barracks, -1 },
		{ AntColony::Throne, AntColony::Stores, -1 },
		{ AntColony::Stores, AntColony::Arsenal, -1 },
		{ AntColony::Nursery, AntColony::Fungus, AntColony::Pipe },
		{ AntColony::Fungus, AntColony::Trophies, AntColony::Pipe },
		{ AntColony::Barracks, AntColony::Workshop, AntColony::Wiring },
		{ AntColony::Workshop, AntColony::Radio, AntColony::Wiring },
		{ AntColony::Stores, AntColony::Memorial, AntColony::Gap },
		{ AntColony::Memorial, AntColony::Parade, AntColony::Gap },
	};
	constexpr int32 NumEdges = UE_ARRAY_COUNT(Edges);
	/** The first edge of each dig (where its "dig here" tag sits). */
	const int32 DigEdge[AntColony::DigCount] = { 5, 7, 9 };

	struct FWalker
	{
		int32 Edge = 0;
		bool bBack = false;
		float T = 0.f;       // 0..1 along the path of the edge
		float Speed = 60.f;  // colony units a second
		float Phase = 0.f;
		int8 Carry = -1;     // food kind it carries, -1 none
		float Size = 1.f;
		bool bShow = false;  // one of the post-raid show's columns (goes to the stores, then away)
		float Delay = 0.f;
	};
	struct FMote { FVector2D P; FVector2D V; float Size; float Seed; };

	/** A stage of the post-raid show. */
	namespace Stage { enum : uint8 { Title, Haul, Queen, Brood, Trophy, Build, Chronicle, Heroes, Count }; }
	struct FStage { uint8 Kind; float Start; float Len; };

	struct FState
	{
		bool bOn = true;
		float Zoom = 1.f;
		FVector2D Center = FVector2D(WW * 0.5f, WH * 0.5f);
		float ZoomGoal = 1.f;
		FVector2D CenterGoal = FVector2D(WW * 0.5f, WH * 0.5f);
		int32 Open = -1;         // chamber sheet open, or 100 + dig
		int32 Hover = -1;
		uint8 SeenShow = 0;
		bool bSeenInit = false;
		float ShowT = -1.f;
		TArray<FStage> Stages;
		FAntRaidResult ShowR;
		TArray<FAntPlayerHighlight> Highlights;
		int32 FlashChamber = -1;
		int32 FlashDig = -1;
		float LastInput = 0.f;
		float SleepAt = -1.f;
		int32 LastFace = -1;
		float FaceChangedAt = -100.f;
		TArray<FWalker> Ants;
		TArray<FMote> Motes;
		double LastTime = 0.0;
		float Clock = 0.f;
		float FrameMs = 0.f;
		int32 Items = 0;
		bool bDragging = false;
		FVector2D DragFrom = FVector2D::ZeroVector;
		FVector2D DragCenter = FVector2D::ZeroVector;
		bool bMouseWasDown = false;
		FString Tip;
		FVector2D Mouse = FVector2D(-1.f, -1.f);
		bool bClick = false;
		FRandomStream Rand = FRandomStream(1234);
	};
	FState& St() { static FState S; return S; }

	UTexture2D* T(const TCHAR* Name) { return AntUi::Tex(*FString::Printf(TEXT("Colony/T_Colony_%s"), Name)); }

	/** The view: colony units -> screen pixels. */
	struct FView
	{
		float U = 1.f;          // pixels per colony unit
		float Base = 1.f;       // U at zoom 1
		FVector2D Off = FVector2D::ZeroVector;
		float W = 1920.f, H = 1080.f;
		FVector2D S(const FVector2D& P) const { return Off + P * U; }
		FVector2D S(float X, float Y) const { return Off + FVector2D(X, Y) * U; }
		FVector2D World(const FVector2D& Px) const { return (Px - Off) / U; }
	};

	float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
	float Pulse(float Speed, float Phase = 0.f) { return 0.5f + 0.5f * FMath::Sin(St().Clock * Speed + Phase); }

	FLinearColor KindColor(int32 K)
	{
		static const FLinearColor C[AntFoodKind::Count] = { FLinearColor(0.95f, 0.93f, 0.88f), FLinearColor(0.55f, 0.22f, 0.16f), FLinearColor(0.93f, 0.72f, 0.18f), FLinearColor(0.62f, 0.42f, 0.2f) };
		return C[FMath::Clamp(K, 0, AntFoodKind::Count - 1)];
	}
	const TCHAR* KindIcon(int32 K)
	{
		static const TCHAR* I[AntFoodKind::Count] = { TEXT("Sugar"), TEXT("Nut"), TEXT("Cheese"), TEXT("Bread") };
		return I[FMath::Clamp(K, 0, AntFoodKind::Count - 1)];
	}
	const TCHAR* FaceTex(int32 Face)
	{
		static const TCHAR* N[5] = { TEXT("Queen_Furious"), TEXT("Queen_Grumpy"), TEXT("Queen_Happy"), TEXT("Queen_Delighted"), TEXT("Queen_Sleeping") };
		return N[FMath::Clamp(Face, 0, 4)];
	}
	const TCHAR* RomanN(int32 N)
	{
		static const TCHAR* R[] = { TEXT("—"), TEXT("I"), TEXT("II"), TEXT("III") };
		return R[FMath::Clamp(N, 0, 3)];
	}

	// ------------------------------------------------------------------------------------------ geometry

	FVector2D NodePos(int32 N)
	{
		if (N == Entrance) { return FVector2D(936.f, 150.f); }
		const AntColony::FChamberDef& D = AntColony::Def((uint8)N);
		return FVector2D(D.X, D.Y + D.H * 0.24f);   // the middle of the chamber's floor
	}

	/** Where a tunnel leaves chamber N towards point To. */
	FVector2D Port(int32 N, const FVector2D& To)
	{
		if (N == Entrance) { return FVector2D(936.f, 150.f); }
		const AntColony::FChamberDef& D = AntColony::Def((uint8)N);
		const FVector2D C(D.X, D.Y);
		const FVector2D Dlt = To - C;
		if (FMath::Abs(Dlt.X) * D.H > FMath::Abs(Dlt.Y) * D.W * 0.8f)
		{
			return FVector2D(D.X + FMath::Sign(Dlt.X) * D.W * 0.43f, D.Y + D.H * 0.22f);
		}
		return FVector2D(D.X + FMath::Clamp(Dlt.X * 0.2f, -D.W * 0.2f, D.W * 0.2f), D.Y + FMath::Sign(Dlt.Y) * D.H * 0.41f);
	}

	/** The walking path of an edge: floor of A, its port, the port of B, floor of B. */
	void EdgePath(int32 E, FVector2D Out[4])
	{
		const FEdge& Ed = Edges[E];
		const FVector2D PA = NodePos(Ed.A), PB = NodePos(Ed.B);
		Out[0] = PA;
		Out[1] = Port(Ed.A, PB);
		Out[2] = Port(Ed.B, PA);
		Out[3] = PB;
	}

	float PathLen(const FVector2D P[4]) { return FVector2D::Distance(P[0], P[1]) + FVector2D::Distance(P[1], P[2]) + FVector2D::Distance(P[2], P[3]); }

	FVector2D PathAt(const FVector2D P[4], float T, FVector2D* OutDir)
	{
		const float L0 = FVector2D::Distance(P[0], P[1]), L1 = FVector2D::Distance(P[1], P[2]), L2 = FVector2D::Distance(P[2], P[3]);
		float D = FMath::Clamp(T, 0.f, 1.f) * (L0 + L1 + L2);
		int32 Seg = 0;
		if (D > L0) { D -= L0; Seg = 1; if (D > L1) { D -= L1; Seg = 2; } }
		const float SegL[3] = { L0, L1, L2 };
		const FVector2D A = P[Seg], B = P[Seg + 1];
		if (OutDir) { *OutDir = (B - A).GetSafeNormal(); }
		return SegL[Seg] > 0.f ? FMath::Lerp(A, B, FMath::Clamp(D / SegL[Seg], 0.f, 1.f)) : A;
	}

	bool EdgeOpen(const FAntNestState& N, int32 E) { return Edges[E].Dig < 0 || AntColony::IsDug(N, (uint8)Edges[E].Dig); }
}

// ---------------------------------------------------------------------------------------------
// Drawing helpers (colony units in, pixels out)
// ---------------------------------------------------------------------------------------------

namespace
{
	void Line(UCanvas* Cv, const FVector2D& A, const FVector2D& B, float Th, const FLinearColor& C)
	{
		FCanvasLineItem L(A, B);
		L.SetColor(C);
		L.LineThickness = Th;
		L.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(L);
		++St().Items;
	}

	/** A soft round blob (the dot texture, or a square without it). P in pixels, R radius in pixels. */
	void Dot(UCanvas* Cv, const FVector2D& P, float R, const FLinearColor& C, bool bAdd = false)
	{
		if (R <= 0.2f) { return; }
		if (bAdd)
		{
			// Light: the glow texture is black outside its disc, so adding it leaves no square. Without it: no light at all.
			if (UTexture2D* G = T(TEXT("Glow")))
			{
				FCanvasTileItem It(P - FVector2D(R, R), G->GetResource(), FVector2D(2.f * R, 2.f * R), FLinearColor(C.R * C.A, C.G * C.A, C.B * C.A, 1.f));
				It.BlendMode = SE_BLEND_Additive;
				Cv->DrawItem(It);
				++St().Items;
			}
			return;
		}
		if (UTexture2D* D = T(TEXT("Dot")))
		{
			FCanvasTileItem It(P - FVector2D(R, R), D->GetResource(), FVector2D(2.f * R, 2.f * R), C);
			It.BlendMode = bAdd ? SE_BLEND_Additive : SE_BLEND_Translucent;
			Cv->DrawItem(It);
		}
		else
		{
			AntUiCanvas::Rect(Cv, P.X - R * 0.7f, P.Y - R * 0.7f, R * 1.4f, R * 1.4f, C);
		}
		++St().Items;
	}

	/** A sprite centred on a point of the cutaway, Size in colony units. False when the texture is missing (the caller may draw a stand-in). */
	bool Spr(UCanvas* Cv, const FView& V, UTexture2D* Tex, const FVector2D& At, const FVector2D& Size, const FLinearColor& Tint = FLinearColor::White, float Rot = 0.f, bool bFlip = false)
	{
		if (!Tex) { return false; }
		const FVector2D P = V.S(At - Size * 0.5f);
		AntUiCanvas::Tile(Cv, Tex, P.X, P.Y, Size.X * V.U, Size.Y * V.U, Tint, FVector2D(bFlip ? 1.f : 0.f, 0.f), FVector2D(bFlip ? 0.f : 1.f, 1.f), Rot);
		++St().Items;
		return true;
	}

	bool IconW(UCanvas* Cv, const FView& V, const TCHAR* Name, const FVector2D& At, float Size, const FLinearColor& Tint = FLinearColor::White, float Rot = 0.f)
	{
		return Spr(Cv, V, AntUi::Icon(Name), At, FVector2D(Size, Size), Tint, Rot);
	}

	/** A point inside a chamber, from fractions of its picture (0,0 top left .. 1,1 bottom right). */
	FVector2D In(const AntColony::FChamberDef& D, float FX, float FY) { return FVector2D(D.X + (FX - 0.5f) * D.W, D.Y + (FY - 0.5f) * D.H); }

	/** A worker ant seen from the side, walking along Dir. Everything procedural: three body blobs, three legs that step, antennae. */
	void DrawAnt(UCanvas* Cv, const FView& V, const FVector2D& At, FVector2D Dir, float Size, float Phase, int8 Carry, const FLinearColor& Body = FLinearColor(0.045f, 0.022f, 0.012f, 1.f), float LoadK = 1.f)
	{
		const float L = 15.f * Size * V.U;   // body length in pixels
		if (L < 2.5f) { return; }
		Dir = Dir.GetSafeNormal();
		if (Dir.IsNearlyZero()) { Dir = FVector2D(1.f, 0.f); }
		FVector2D Side(-Dir.Y, Dir.X);
		if (Side.Y < 0.f || (FMath::IsNearlyZero(Side.Y) && Side.X < 0.f)) { Side = -Side; }   // legs point to the ground
		const FVector2D P = V.S(At) - Side * L * 0.22f;
		const float Bob = FMath::Sin(Phase * 2.f) * 0.025f * L;
		const FVector2D Abd = P - Dir * 0.42f * L - Side * (0.04f * L + Bob);
		const FVector2D Tho = P - Side * Bob;
		const FVector2D Head = P + Dir * 0.36f * L - Side * (0.06f * L + Bob);
		const float Th = FMath::Max(1.f, L * 0.06f);
		const FLinearColor Leg(Body.R * 1.4f, Body.G * 1.4f, Body.B * 1.4f, 0.95f);
		for (int32 i = 0; i < 3; ++i)
		{
			const float S = FMath::Sin(Phase + i * 2.1f);
			const FVector2D Base = Tho + Dir * (i - 1) * 0.08f * L;
			const FVector2D Foot = Tho + Dir * ((i - 1) * 0.3f + S * 0.12f) * L + Side * 0.36f * L;
			const FVector2D Knee = (Base + Foot) * 0.5f - Side * 0.1f * L + Dir * (i - 1) * 0.06f * L;
			Line(Cv, Base, Knee, Th, Leg);
			Line(Cv, Knee, Foot, Th, Leg);
		}
		Dot(Cv, Abd, 0.25f * L, Body);
		Dot(Cv, Tho, 0.15f * L, Body);
		Dot(Cv, Head, 0.18f * L, Body);
		if (L > 14.f) { Dot(Cv, Abd - Side * 0.08f * L - Dir * 0.04f * L, 0.09f * L, FLinearColor(0.4f, 0.22f, 0.1f, 0.45f)); }   // the shine
		const float W1 = FMath::Sin(Phase * 1.3f) * 0.08f * L;
		Line(Cv, Head + Dir * 0.08f * L, Head + Dir * 0.3f * L - Side * (0.28f * L + W1), Th * 0.8f, Leg);
		if (Carry >= 0)
		{
			{ FLinearColor LC = KindColor(Carry) * LoadK; LC.A = 1.f; Dot(Cv, Head + Dir * 0.22f * L - Side * 0.32f * L, 0.2f * L, LC); }
		}
	}

	FLinearColor ShellTint(int32 Tier, bool bHot)
	{
		const float K = Tier >= 3 ? 1.04f : Tier == 2 ? 0.96f : 0.86f;
		const float H = bHot ? 1.18f : 1.f;
		return FLinearColor(K * H, K * H * 0.98f, K * H * 0.95f, 1.f);
	}
}

// ---------------------------------------------------------------------------------------------
// The cutaway
// ---------------------------------------------------------------------------------------------

namespace
{
	void DrawBackground(UCanvas* Cv, const FView& V, const FAntNestState& N)
	{
		AntUiCanvas::Rect(Cv, 0.f, 0.f, V.W, V.H, FLinearColor(0.012f, 0.006f, 0.003f, 1.f));
		const FVector2D TL = V.S(0.f, 0.f);
		if (UTexture2D* Bg = T(TEXT("Bg")))
		{
			AntUiCanvas::Tile(Cv, Bg, TL.X, TL.Y, WW * V.U, WH * V.U, FLinearColor(0.92f, 0.9f, 0.88f, 1.f));
		}
		else
		{
			// Soil strata without the plate.
			const FLinearColor Band[4] = { FLinearColor(0.32f, 0.22f, 0.16f), FLinearColor(0.36f, 0.2f, 0.07f), FLinearColor(0.2f, 0.09f, 0.035f), FLinearColor(0.12f, 0.05f, 0.02f) };
			const float Y0[5] = { 0.f, 110.f, 270.f, 600.f, 1080.f };
			for (int32 i = 0; i < 4; ++i) { const FVector2D P = V.S(0.f, Y0[i]); AntUiCanvas::Rect(Cv, P.X, P.Y, WW * V.U, (Y0[i + 1] - Y0[i]) * V.U, Band[i]); }
		}
		++St().Items;
		const float C = St().Clock;
		// The drip of the pipe (left): a drop swells, falls, is gone. With the fungus farm dug it falls into the farm.
		{
			const float Ph = FMath::Fmod(C, 2.6f);
			const FVector2D From(186.f, 336.f);
			if (Ph < 0.9f) { Dot(Cv, V.S(From + FVector2D(0.f, Ph * 6.f)), (2.f + Ph * 4.f) * V.U, FLinearColor(0.7f, 0.85f, 1.f, 0.75f)); }
			else if (Ph < 1.5f)
			{
				const float F = (Ph - 0.9f) / 0.6f;
				const float Fall = AntColony::IsDug(N, AntColony::Pipe) ? 140.f : 90.f;
				Dot(Cv, V.S(From + FVector2D(0.f, 6.f + F * F * Fall)), 4.5f * V.U, FLinearColor(0.7f, 0.85f, 1.f, 0.8f * (1.f - F * 0.6f)));
			}
		}
		// The wiring (right) hums: a warm flicker.
		{
			const float Fl = 0.16f + 0.08f * FMath::Sin(C * 23.f) * FMath::Sin(C * 3.7f) + (FMath::Fmod(C, 7.3f) < 0.12f ? 0.25f : 0.f);
			Dot(Cv, V.S(1838.f, 430.f), 120.f * V.U, FLinearColor(1.f, 0.55f, 0.2f, Fl), true);
		}
		// The gap under the floor (bottom right): the light breathes.
		Dot(Cv, V.S(1790.f, 935.f), 150.f * V.U, FLinearColor(1.f, 0.78f, 0.45f, 0.12f + 0.06f * Pulse(0.9f)), true);
	}

	void DrawTunnels(UCanvas* Cv, const FView& V, const FAntNestState& N, int32 HoverDig)
	{
		for (int32 e = 0; e < NumEdges; ++e)
		{
			FVector2D P[4];
			EdgePath(e, P);
			const FVector2D A = V.S(P[1]), B = V.S(P[2]);
			if (EdgeOpen(N, e))
			{
				Line(Cv, A, B, 34.f * V.U, FLinearColor(0.11f, 0.055f, 0.025f, 0.95f));
				Line(Cv, A, B, 24.f * V.U, FLinearColor(0.16f, 0.085f, 0.045f, 1.f));
				Dot(Cv, A, 15.f * V.U, FLinearColor(0.16f, 0.085f, 0.045f, 1.f));
				Dot(Cv, B, 15.f * V.U, FLinearColor(0.16f, 0.085f, 0.045f, 1.f));
				Line(Cv, A + FVector2D(0.f, 9.f * V.U), B + FVector2D(0.f, 9.f * V.U), 3.f * V.U, FLinearColor(0.3f, 0.17f, 0.09f, 0.7f));   // the trodden floor
			}
			else
			{
				// Not dug yet: chalk dashes where the tunnel will go.
				const bool bHot = HoverDig == Edges[e].Dig;
				const float Len = FVector2D::Distance(A, B);
				const FVector2D Dir = (B - A) / FMath::Max(1.f, Len);
				for (float D = 0.f; D < Len; D += 22.f * V.U)
				{
					Line(Cv, A + Dir * D, A + Dir * FMath::Min(Len, D + 12.f * V.U), 3.f * V.U, FLinearColor(1.f, 0.85f, 0.55f, bHot ? 0.8f : 0.35f));
				}
			}
		}
	}

	void DrawChamberShell(UCanvas* Cv, const FView& V, const FAntNestState& N, uint8 c, bool bHot, float Flash)
	{
		const AntColony::FChamberDef& D = AntColony::Def(c);
		const int32 Tier = AntColony::TierOf(N, c);
		const bool bOpen = AntColony::IsOpen(N, c);
		const FVector2D Size(D.W, D.H);
		const FVector2D At(D.X, D.Y);
		UTexture2D* Shell = T(D.Shell == 0 ? TEXT("Shell_A") : TEXT("Shell_B"));
		if (!bOpen)
		{
			// Behind soil not dug yet: a faint outline of what will be here.
			const float A = bHot ? 0.5f : 0.22f;
			for (int32 i = 0; i < 36; i += 2)
			{
				const float a0 = i * PI / 18.f, a1 = (i + 1) * PI / 18.f;
				Line(Cv, V.S(At + FVector2D(FMath::Cos(a0) * D.W * 0.44f, FMath::Sin(a0) * D.H * 0.4f)), V.S(At + FVector2D(FMath::Cos(a1) * D.W * 0.44f, FMath::Sin(a1) * D.H * 0.4f)), 2.5f * V.U, FLinearColor(1.f, 0.85f, 0.55f, A));
			}
			return;
		}
		if (Tier <= 0)
		{
			// Dug, nothing built: a bare hollow.
			if (!Spr(Cv, V, Shell, At, Size, FLinearColor(0.42f, 0.38f, 0.36f, bHot ? 1.f : 0.85f), 0.f, D.bFlip))
			{
				const FVector2D P = V.S(At - Size * 0.45f);
				AntUiCanvas::Rect(Cv, P.X, P.Y, D.W * 0.9f * V.U, D.H * 0.9f * V.U, FLinearColor(0.06f, 0.03f, 0.015f, 0.9f));
			}
			return;
		}
		FLinearColor Tint = ShellTint(Tier, bHot);
		if (Flash > 0.f) { Tint = Tint * (1.f + 0.45f * Flash); Tint.A = 1.f; }
		if (!Spr(Cv, V, Shell, At, Size, Tint, 0.f, D.bFlip))
		{
			const FVector2D P = V.S(At - Size * 0.45f);
			AntUiCanvas::Rect(Cv, P.X, P.Y, D.W * 0.9f * V.U, D.H * 0.9f * V.U, FLinearColor(0.2f * Tint.R, 0.11f * Tint.G, 0.05f * Tint.B, 1.f));
		}
		// Tier II: matchsticks and toothpicks hold the walls. Tier III: bottle caps, foil and buttons dress it up.
		if (Tier >= 2) { Spr(Cv, V, T(TEXT("Frame")), At, Size, FLinearColor(1.f, 1.f, 1.f, 1.f), 0.f, D.bFlip); }
		if (Tier >= 3) { Spr(Cv, V, T(TEXT("Deco")), At, Size, FLinearColor(1.f, 1.f, 1.f, 1.f), 0.f, D.bFlip); }
		// The lamp: a glowing crumb, warmer and bigger with the tier.
		Dot(Cv, V.S(In(D, D.bFlip ? 0.27f : 0.73f, 0.3f)), (40.f + 25.f * Tier) * V.U, FLinearColor(1.f, 0.62f, 0.25f, 0.08f + 0.04f * Tier + 0.02f * Pulse(2.f, c)), true);
	}

	void DrawLabel(UCanvas* Cv, const FView& V, const FAntNestState& N, uint8 c, bool bHot, bool bCanBuild)
	{
		const AntColony::FChamberDef& D = AntColony::Def(c);
		const int32 Tier = AntColony::TierOf(N, c);
		const float Pt = FMath::Clamp(12.f * V.U, 9.f, 22.f);
		const FVector2D At = V.S(D.X, D.Y + D.H * 0.5f - 4.f);
		const FString S = Tier > 0 ? FString::Printf(TEXT("%s  %s"), D.Name, RomanN(Tier)) : FString(D.Name);
		const FVector2D M = AntUiCanvas::TextSize(Cv, S, EFont::Bold, Pt);
		const float PadX = 8.f, PadY = 3.f;
		AntUiCanvas::Rect(Cv, At.X - M.X * 0.5f - PadX, At.Y - PadY, M.X + PadX * 2.f, M.Y + PadY * 2.f, bHot ? WithA(C::AmberDeep, 0.95f) : FLinearColor(0.03f, 0.015f, 0.008f, 0.78f));
		AntUiCanvas::Text(Cv, S, At.X, At.Y, EFont::Bold, Pt, Tier > 0 ? C::Sugar : C::Muted, 1);
		if (bCanBuild)
		{
			// Something can be built here now: a small amber pin that breathes.
			Dot(Cv, FVector2D(At.X + M.X * 0.5f + PadX + 8.f, At.Y + M.Y * 0.5f), (5.f + 2.f * Pulse(4.f)) , C::Honey);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// What lives in each chamber
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Food the stores show by kind: during the show it grows from what was there before the raid. */
	float ShownHeld(const FAntNestState& N, int32 K)
	{
		const float Now = (float)AntColony::Held(N, (uint8)K);
		const FState& S = St();
		if (S.ShowT < 0.f || !S.ShowR.StockBefore.IsValidIndex(K)) { return Now; }
		float Haul = 0.f;
		for (const FStage& G : S.Stages) { if (G.Kind == Stage::Haul) { Haul = Ease((S.ShowT - G.Start - 0.6f) / FMath::Max(0.1f, G.Len - 1.2f)); } }
		return FMath::Lerp((float)S.ShowR.StockBefore[K], Now, Haul);
	}

	float StageProgress(uint8 Kind)
	{
		const FState& S = St();
		if (S.ShowT < 0.f) { return 1.f; }
		for (const FStage& G : S.Stages) { if (G.Kind == Kind) { return FMath::Clamp((S.ShowT - G.Start) / FMath::Max(0.1f, G.Len), 0.f, 1.f); } }
		return 1.f;
	}

	void DrawPile(UCanvas* Cv, const FView& V, const FVector2D& Floor, int32 K, float Amount, float Cap, float MaxW)
	{
		const float F = FMath::Clamp(FMath::Sqrt(FMath::Max(0.f, Amount) / FMath::Max(1.f, Cap)), 0.f, 1.15f);
		if (F < 0.03f) { return; }
		static const TCHAR* Tex[AntFoodKind::Count] = { TEXT("Pile_Sugar"), TEXT("Pile_Protein"), TEXT("Pile_Fat"), TEXT("Pile_Crumbs") };
		const float W = MaxW * (0.35f + 0.65f * F), H = W * 0.62f * (0.55f + 0.45f * F);
		if (!Spr(Cv, V, T(Tex[K]), Floor - FVector2D(0.f, H * 0.45f), FVector2D(W, H)))
		{
			// A heap of grains.
			FRandomStream R(K * 31 + 7);
			const int32 Grains = FMath::Clamp(FMath::RoundToInt(F * 26.f), 3, 30);
			for (int32 i = 0; i < Grains; ++i)
			{
				const float X = R.FRandRange(-0.5f, 0.5f), Y = R.FRand() * (1.f - FMath::Abs(X) * 1.6f);
				Dot(Cv, V.S(Floor + FVector2D(X * W, -Y * H)), (3.f + 3.f * F) * V.U, KindColor(K) * R.FRandRange(0.8f, 1.1f));
			}
		}
	}

	void DrawContents(UCanvas* Cv, const FView& V, const FAntNestState& N, const AAntRaid* Raid, uint8 c)
	{
		const AntColony::FChamberDef& D = AntColony::Def(c);
		const int32 Tier = AntColony::TierOf(N, c);
		if (!AntColony::IsOpen(N, c) || (Tier <= 0 && c != AntColony::Throne && c != AntColony::Stores && c != AntColony::Nursery))
		{
			return;
		}
		const float C = St().Clock;
		switch (c)
		{
		case AntColony::Stores:
		{
			const float Cap = 260.f * (1.f + Tier);
			const FVector2D Spots[AntFoodKind::Count] = { In(D, 0.36f, 0.6f), In(D, 0.64f, 0.6f), In(D, 0.33f, 0.71f), In(D, 0.67f, 0.71f) };
			for (int32 K = 0; K < AntFoodKind::Count; ++K) { DrawPile(Cv, V, Spots[K], K, ShownHeld(N, K), Cap, D.W * 0.33f); }
			// The jelly: a thimble on a shelf, the cake slice kept for the Queen.
			const float JellyShown = FMath::Lerp((float)(St().ShowT >= 0.f ? St().ShowR.JellyBefore : N.Jelly), (float)N.Jelly, StageProgress(Stage::Haul));
			if (JellyShown > 0.f)
			{
				const float Sz = FMath::Clamp(28.f + JellyShown * 1.5f, 28.f, 58.f);
				if (!Spr(Cv, V, T(TEXT("Jelly")), In(D, 0.86f, 0.62f), FVector2D(Sz, Sz))) { IconW(Cv, V, TEXT("Jelly"), In(D, 0.86f, 0.62f), Sz); }
			}
			if (N.CakesHome > 0)
			{
				if (!Spr(Cv, V, T(TEXT("CakeSlice")), In(D, 0.5f, 0.46f), FVector2D(64.f, 52.f))) { IconW(Cv, V, TEXT("Cake"), In(D, 0.5f, 0.45f), 52.f); }
			}
			break;
		}
		case AntColony::Nursery:
		{
			// Larvae on the floor, each one glows; cocoons (workers to be) along the back wall.
			const int32 Shown = FMath::RoundToInt(FMath::Lerp((float)(St().ShowT >= 0.f ? St().ShowR.LarvaeLeft : N.Larvae), (float)N.Larvae, StageProgress(Stage::Brood)));
			for (int32 i = 0; i < FMath::Min(Shown, AntNest::MaxLarvae); ++i)
			{
				const int32 Row = i / 6, Col = i % 6;
				const FVector2D P = In(D, 0.22f + Col * 0.112f + Row * 0.05f, 0.76f - Row * 0.085f);
				Dot(Cv, V.S(P), 22.f * V.U, FLinearColor(1.f, 0.85f, 0.55f, 0.14f + 0.1f * Pulse(2.2f, i * 0.9f)), true);
				const float Wig = FMath::Sin(C * 2.f + i) * 4.f;
				if (!Spr(Cv, V, T(TEXT("Larva")), P, FVector2D(34.f, 22.f), FLinearColor::White, Wig, (i & 1) != 0)) { Dot(Cv, V.S(P), 9.f * V.U, FLinearColor(0.97f, 0.94f, 0.86f, 1.f)); }
			}
			const int32 Cocoons = FMath::Clamp(N.Workers / 12, 0, 6) + Tier;
			for (int32 i = 0; i < FMath::Min(Cocoons, 8); ++i)
			{
				const FVector2D P = In(D, 0.2f + i * 0.085f, 0.47f + (i % 2) * 0.04f);
				if (!Spr(Cv, V, T(TEXT("Cocoon")), P, FVector2D(22.f, 34.f), FLinearColor::White, FMath::Sin(C * 0.7f + i) * 3.f)) { Dot(Cv, V.S(P), 9.f * V.U, FLinearColor(0.85f, 0.75f, 0.55f, 1.f)); }
			}
			break;
		}
		case AntColony::Barracks:
		{
			const int32 Bunks = FMath::Min(1 + Tier * 2, 7);
			for (int32 i = 0; i < Bunks; ++i)
			{
				const FVector2D P = In(D, 0.22f + (i % 4) * 0.18f, 0.72f - (i / 4) * 0.2f);
				if (!Spr(Cv, V, T(TEXT("Bunk")), P, FVector2D(58.f, 30.f))) { AntUiCanvas::Rect(Cv, V.S(P).X - 26.f * V.U, V.S(P).Y - 12.f * V.U, 52.f * V.U, 24.f * V.U, FLinearColor(0.55f, 0.3f, 0.12f, 1.f)); }
			}
			IconW(Cv, V, TEXT("Soldier"), In(D, 0.5f, 0.36f), 46.f + Tier * 6.f, FLinearColor(1.f, 1.f, 1.f, 0.95f));
			break;
		}
		case AntColony::Arsenal:
		{
			static const TCHAR* Tools[4] = { TEXT("Cookie"), TEXT("Bolt"), TEXT("Spray"), TEXT("Bomb") };
			static const uint8 Need[4] = { 1, 1, 2, 3 };
			for (int32 i = 0; i < 4; ++i)
			{
				if (Tier < Need[i]) { continue; }
				IconW(Cv, V, Tools[i], In(D, 0.25f + i * 0.17f, 0.64f), 44.f, FLinearColor::White, FMath::Sin(C * 0.8f + i) * 2.f);
			}
			Spr(Cv, V, T(TEXT("Crate")), In(D, 0.82f, 0.7f), FVector2D(60.f, 48.f));
			break;
		}
		case AntColony::Fungus:
		{
			const int32 Count = 2 + Tier * 3;
			FRandomStream R(77);
			for (int32 i = 0; i < Count; ++i)
			{
				const float X = 0.18f + (i % 6) * 0.13f + R.FRandRange(-0.03f, 0.03f);
				const float Y = 0.74f - (i / 6) * 0.13f;
				const float Sz = R.FRandRange(26.f, 46.f) * (0.8f + 0.1f * Tier);
				const float Sway = FMath::Sin(C * 0.9f + i * 1.7f) * 3.f;
				const FVector2D P = In(D, X, Y) - FVector2D(0.f, Sz * 0.4f);
				if (!Spr(Cv, V, T(TEXT("Mushroom")), P, FVector2D(Sz, Sz), FLinearColor::White, Sway, (i & 1) != 0))
				{
					Dot(Cv, V.S(P), Sz * 0.3f * V.U, FLinearColor(0.85f, 0.7f, 0.55f, 1.f));
				}
			}
			break;
		}
		case AntColony::Trophies:
		{
			const int32 Slots = AntColony::TrophySlots(N);
			float NewAlpha = 1.f;
			for (const FStage& G : St().Stages) { if (G.Kind == Stage::Trophy && St().ShowT >= 0.f) { NewAlpha = Ease((St().ShowT - G.Start - 0.8f) / 1.2f); } }
			for (int32 i = 0; i < N.Trophies.Num() && i < Slots; ++i)
			{
				const int32 Id = N.Trophies[i];
				const bool bNew = St().ShowT >= 0.f && St().ShowR.NewTrophies.Contains((uint8)Id);
				const FVector2D P = In(D, 0.2f + (i % 5) * 0.15f, 0.36f + (i / 5) * 0.2f);
				const float Sz = 40.f;
				const FLinearColor Tn(1.f, 1.f, 1.f, bNew ? NewAlpha : 1.f);
				Line(Cv, V.S(P - FVector2D(0.f, Sz * 0.5f)), V.S(P - FVector2D(0.f, Sz * 0.8f)), 1.5f * V.U, FLinearColor(0.8f, 0.7f, 0.5f, 0.8f * Tn.A));   // the pin
				if (!Spr(Cv, V, T(*FString::Printf(TEXT("Trophy_%s"), AntColony::Trophy(Id).Tex)), P, FVector2D(Sz, Sz), Tn, FMath::Sin(C * 0.6f + i) * 3.f))
				{
					Dot(Cv, V.S(P), 12.f * V.U, FLinearColor(1.f, 0.8f, 0.3f, Tn.A));
				}
			}
			if (N.Trophies.Num() > Slots) { Spr(Cv, V, T(TEXT("Crate")), In(D, 0.84f, 0.72f), FVector2D(56.f, 44.f)); }
			break;
		}
		case AntColony::Workshop:
		{
			IconW(Cv, V, TEXT("Gear"), In(D, 0.3f, 0.42f), 50.f, FLinearColor::White, C * 25.f);
			if (Tier >= 2) { IconW(Cv, V, TEXT("Gear"), In(D, 0.4f, 0.36f), 34.f, FLinearColor::White, -C * 37.f); }
			IconW(Cv, V, TEXT("Wrench"), In(D, 0.62f, 0.62f), 46.f, FLinearColor::White, -20.f);
			if (Tier >= 3) { IconW(Cv, V, TEXT("Engineers"), In(D, 0.78f, 0.5f), 50.f); }
			// The bulb from the wiring.
			Dot(Cv, V.S(In(D, 0.5f, 0.24f)), 26.f * V.U, FLinearColor(1.f, 0.85f, 0.5f, 0.35f + 0.15f * FMath::Sin(C * 17.f) * FMath::Sin(C * 2.3f)), true);
			break;
		}
		case AntColony::Radio:
		{
			const FVector2D P = In(D, 0.42f, 0.55f);
			if (!Spr(Cv, V, T(TEXT("Radio")), P, FVector2D(70.f, 70.f))) { IconW(Cv, V, TEXT("Radio"), P, 62.f); }
			// Sound waves, stronger while the show plays (the newsreel is on the air).
			const bool bOnAir = St().ShowT >= 0.f;
			for (int32 w = 0; w < (bOnAir ? 3 : 1 + Tier / 2); ++w)
			{
				const float Ph = FMath::Fmod(C * 0.8f + w * 0.33f, 1.f);
				const float R = (30.f + Ph * 50.f);
				const FLinearColor Col(1.f, 0.85f, 0.5f, (1.f - Ph) * (bOnAir ? 0.7f : 0.35f));
				for (int32 s = -2; s < 2; ++s)
				{
					const float a0 = s * 0.25f, a1 = (s + 1) * 0.25f;
					Line(Cv, V.S(P + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * R + FVector2D(10.f, -10.f)), V.S(P + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * R + FVector2D(10.f, -10.f)), 2.f * V.U, Col);
				}
			}
			break;
		}
		case AntColony::Parade:
		{
			Spr(Cv, V, T(TEXT("Target")), In(D, 0.8f, 0.6f), FVector2D(52.f, 52.f));
			// A squad drills: four scouts trot back and forth.
			for (int32 i = 0; i < 2 + Tier; ++i)
			{
				const float X = 0.2f + 0.45f * Pulse(0.7f, i * 0.4f) + i * 0.04f;
				const float Dir = FMath::Cos(C * 0.7f + i * 0.4f) >= 0.f ? 1.f : -1.f;
				DrawAnt(Cv, V, In(D, X, 0.74f), FVector2D(Dir, 0.f), 0.9f, C * 14.f + i, -1, FLinearColor(0.06f, 0.035f, 0.02f, 1.f));
			}
			IconW(Cv, V, TEXT("Scouts"), In(D, 0.3f, 0.4f), 40.f);
			break;
		}
		case AntColony::Memorial:
		{
			const FVector2D P = In(D, 0.5f, 0.5f);
			const FVector2D PlSize(D.W * 0.52f, D.H * 0.48f);
			if (!Spr(Cv, V, T(TEXT("Plaque")), P, PlSize))
			{
				const FVector2D TL = V.S(P - PlSize * 0.5f);
				AntUiCanvas::Rect(Cv, TL.X, TL.Y, PlSize.X * V.U, PlSize.Y * V.U, FLinearColor(0.09f, 0.05f, 0.03f, 1.f));
			}
			const TArray<FAntFallenVeteran> Fallen = AntColony::Fallen(N);
			const float Pt = FMath::Clamp(8.f * V.U, 6.f, 16.f);
			const FVector2D TL = V.S(P - PlSize * 0.5f + FVector2D(12.f, 12.f));
			if (Fallen.Num() == 0)
			{
				AntUiCanvas::Text(Cv, TEXT("пока никого"), TL.X + PlSize.X * V.U * 0.5f - 12.f * V.U, TL.Y + 10.f * V.U, EFont::Hand, Pt * 1.2f, FLinearColor(0.85f, 0.75f, 0.55f, 0.6f), 1);
			}
			for (int32 i = 0; i < Fallen.Num() && i < 6; ++i)
			{
				const FAntFallenVeteran& F = Fallen[i];
				const FString L = F.Rank.IsEmpty() ? FString::Printf(TEXT("%s · рейд %d"), *F.Name, F.Raid) : FString::Printf(TEXT("%s %s · рейд %d"), *F.Rank, *F.Name, F.Raid);
				AntUiCanvas::TextFit(Cv, L, TL.X, TL.Y + i * Pt * 1.25f, (PlSize.X - 24.f) * V.U, EFont::Hand, Pt, FLinearColor(0.92f, 0.82f, 0.6f, 0.9f));
			}
			if (Tier >= 2)
			{
				const FVector2D Fl = In(D, 0.85f, 0.66f);
				Dot(Cv, V.S(Fl), 30.f * V.U, FLinearColor(1.f, 0.6f, 0.2f, 0.25f + 0.1f * FMath::Sin(C * 11.f)), true);
				Dot(Cv, V.S(Fl - FVector2D(0.f, 4.f + FMath::Sin(C * 9.f))), 6.f * V.U, FLinearColor(1.f, 0.85f, 0.4f, 0.95f));
				Line(Cv, V.S(Fl + FVector2D(0.f, 2.f)), V.S(Fl + FVector2D(0.f, 30.f)), 4.f * V.U, FLinearColor(0.85f, 0.65f, 0.4f, 1.f));   // the match
			}
			break;
		}
		default:
			break;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Her Sweetness the Queen Mother
// ---------------------------------------------------------------------------------------------

namespace
{
	int32 FaceNow(const FAntNestState& N)
	{
		const FState& S = St();
		if (S.ShowT >= 0.f)
		{
			// Through the show she wears the mood she had before the raid until her stage, then reacts.
			for (const FStage& G : S.Stages)
			{
				if (G.Kind == Stage::Queen) { return AntColony::QueenFace(S.ShowT < G.Start + G.Len * 0.3f ? S.ShowR.MoodBefore : N.Mood); }
			}
		}
		if (S.SleepAt >= 0.f) { return 4; }
		return AntColony::QueenFace(N.Mood);
	}

	FString QueenLine(const FAntRaidResult& R, const FAntNestState& N)
	{
		const bool bWiped = R.End == AntRaidEnd::Wiped;
		const bool bCake = R.Cake == AntCake::Home && !bWiped;
		if (bWiped) { return TEXT("Где мои солдаты?! Где мой торт?!"); }
		if (bCake && R.Deaths > 0) { return TEXT("Торт... Но какой ценой. Помянем."); }
		if (bCake && R.MoodAfter >= 85) { return TEXT("Торт! Мой именинный торт! Обожаю вас!"); }
		if (bCake) { return TEXT("Торт дома. Колония служит исправно."); }
		if (R.Kept >= R.Ask && R.Ask > 0) { return TEXT("Еды много. А торт? Где торт, я спрашиваю?"); }
		return TEXT("Это всё? Я в ярости. Несите сладкое!");
	}

	void DrawQueen(UCanvas* Cv, const FView& V, const FAntNestState& N, const FAntRaidResult& R)
	{
		FState& S = St();
		const AntColony::FChamberDef& D = AntColony::Def(AntColony::Throne);
		const int32 Tier = AntColony::ThroneTier(N);
		const float Tm = S.Clock;
		// The seat grows with the colony: earth (in the picture), a matchbox, a bottle-cap canopy with foil.
		if (Tier >= 2) { Spr(Cv, V, T(Tier >= 3 ? TEXT("Seat_3") : TEXT("Seat_2")), In(D, 0.5f, 0.52f), FVector2D(D.W * 0.5f, D.H * 0.62f)); }
		const int32 Face = FaceNow(N);
		if (Face != S.LastFace) { S.LastFace = Face; S.FaceChangedAt = Tm; }
		const float Since = Tm - S.FaceChangedAt;
		const float Size = D.H * (0.66f + 0.07f * (Tier - 1));
		const float Breath = FMath::Sin(Tm * (Face == 4 ? 0.9f : 1.7f));
		const float SY = 1.f + 0.028f * Breath, SX = 1.f - 0.012f * Breath;
		FVector2D Off(0.f, 0.f);
		float Rot = 0.f;
		if (Since < 1.4f)
		{
			const float K = 1.f - Since / 1.4f;
			if (Face >= 2 && Face <= 3) { Off.Y = -FMath::Abs(FMath::Sin(Since * 9.f)) * 16.f * K; Rot = FMath::Sin(Since * 12.f) * 4.f * K; }
			else if (Face <= 1) { Off.X = FMath::Sin(Since * 45.f) * 6.f * K; }
		}
		if (Face == 0) { Off.X += FMath::Sin(Tm * 31.f) * 1.2f; }   // furious: she trembles
		const FVector2D Base = In(D, 0.5f, 0.76f);
		const FVector2D At = Base - FVector2D(0.f, Size * SY * 0.5f) + Off;
		const FVector2D Sz(Size * SX, Size * SY);
		if (!Spr(Cv, V, T(FaceTex(Face)), At, Sz, FLinearColor::White, Rot))
		{
			// Without her portrait: a plump amber abdomen, a small head, eyes that say how she feels.
			const FVector2D P = V.S(At);
			const float U = V.U;
			Dot(Cv, P + FVector2D(-Size * 0.12f, Size * 0.12f) * U, Size * 0.3f * U, FLinearColor(0.85f, 0.5f, 0.12f, 1.f));
			Dot(Cv, P + FVector2D(Size * 0.16f, -Size * 0.02f) * U, Size * 0.14f * U, FLinearColor(0.55f, 0.3f, 0.08f, 1.f));
			Dot(Cv, P + FVector2D(Size * 0.27f, -Size * 0.15f) * U, Size * 0.13f * U, FLinearColor(0.6f, 0.34f, 0.1f, 1.f));
			const FLinearColor Eye = Face == 4 ? FLinearColor(0.1f, 0.05f, 0.02f, 1.f) : FLinearColor::White;
			Dot(Cv, P + FVector2D(Size * 0.31f, -Size * 0.19f) * U, Size * (Face == 4 ? 0.015f : 0.04f) * U, Eye);
			if (Face != 4) { Dot(Cv, P + FVector2D(Size * 0.32f, -Size * 0.19f) * U, Size * 0.02f * U, FLinearColor(0.02f, 0.01f, 0.f, 1.f)); }
			if (Face <= 1) { Line(Cv, P + FVector2D(Size * 0.26f, -Size * 0.27f) * U, P + FVector2D(Size * 0.36f, -Size * (Face == 0 ? 0.22f : 0.25f)) * U, 3.f * U, FLinearColor(0.1f, 0.05f, 0.02f, 1.f)); }
		}
		if (Tier >= 2) { Spr(Cv, V, T(Tier >= 3 ? TEXT("Crown_3") : TEXT("Crown_2")), At, Sz, FLinearColor::White, Rot); }
		if (Face == 4)
		{
			for (int32 z = 0; z < 3; ++z)
			{
				const float Ph = FMath::Fmod(Tm * 0.5f + z * 0.33f, 1.f);
				const FVector2D P = V.S(At + FVector2D(Size * (0.3f + Ph * 0.15f), -Size * (0.35f + Ph * 0.4f)));
				AntUiCanvas::Text(Cv, TEXT("z"), P.X, P.Y, EFont::Hand, (14.f + Ph * 12.f) * V.U, FLinearColor(1.f, 0.95f, 0.8f, 1.f - Ph), 1);
			}
		}
		// What she says in the show.
		float Say = 0.f;
		for (const FStage& G : S.Stages) { if (G.Kind == Stage::Queen && S.ShowT >= G.Start + G.Len * 0.3f && S.ShowT < G.Start + G.Len + 1.5f) { Say = 1.f; } }
		if (Say > 0.f)
		{
			const FString L = QueenLine(R, N);
			const float Pt = FMath::Clamp(13.f * V.U, 11.f, 30.f);
			const FVector2D M = AntUiCanvas::TextSize(Cv, L, EFont::Hand, Pt);
			const FVector2D P = V.S(In(D, 0.5f, 0.02f));
			AntUiCanvas::Paper(Cv, P.X - M.X * 0.5f - 12.f, P.Y - M.Y - 18.f, M.X + 24.f, M.Y + 14.f, false);
			AntUiCanvas::Text(Cv, L, P.X, P.Y - M.Y - 11.f, EFont::Hand, Pt, C::Graphite, 1);
		}
	}

	// ------------------------------------------------------------------------------------------ the ants

	void PickNext(FWalker& A, const FAntNestState& N, int32 Node)
	{
		TArray<int32, TInlineAllocator<8>> Opts;
		for (int32 e = 0; e < NumEdges; ++e)
		{
			if (!EdgeOpen(N, e) || e == A.Edge) { continue; }
			if (Edges[e].A == Node || Edges[e].B == Node) { Opts.Add(e); }
		}
		if (Opts.Num() == 0) { A.bBack = !A.bBack; A.T = 0.f; return; }   // a dead end: turn round
		A.Edge = Opts[St().Rand.RandRange(0, Opts.Num() - 1)];
		A.bBack = Edges[A.Edge].B == Node;
		A.T = 0.f;
	}

	void TickAnts(const FAntNestState& N, float Dt)
	{
		FState& S = St();
		TArray<int32, TInlineAllocator<16>> Open;
		for (int32 e = 0; e < NumEdges; ++e) { if (EdgeOpen(N, e)) { Open.Add(e); } }
		const int32 Want = FMath::Clamp(N.Workers / 2, 6, 46);
		int32 Plain = 0;
		for (const FWalker& A : S.Ants) { Plain += A.bShow ? 0 : 1; }
		while (Plain < Want && Open.Num() > 0)
		{
			FWalker A;
			A.Edge = Open[S.Rand.RandRange(0, Open.Num() - 1)];
			A.bBack = S.Rand.FRand() < 0.5f;
			A.T = S.Rand.FRand();
			A.Speed = S.Rand.FRandRange(38.f, 70.f);
			A.Phase = S.Rand.FRand() * 6.f;
			A.Size = S.Rand.FRandRange(0.85f, 1.1f);
			A.Carry = S.Rand.FRand() < 0.35f ? (int8)S.Rand.RandRange(0, AntFoodKind::Count - 1) : -1;
			S.Ants.Add(A);
			++Plain;
		}
		for (int32 i = S.Ants.Num() - 1; i >= 0 && Plain > Want; --i) { if (!S.Ants[i].bShow) { S.Ants.RemoveAtSwap(i); --Plain; } }
		for (int32 i = S.Ants.Num() - 1; i >= 0; --i)
		{
			FWalker& A = S.Ants[i];
			if (A.Delay > 0.f) { A.Delay -= Dt; continue; }
			if (!EdgeOpen(N, A.Edge)) { S.Ants.RemoveAtSwap(i); continue; }
			FVector2D P[4];
			EdgePath(A.Edge, P);
			A.T += A.Speed * Dt / FMath::Max(10.f, PathLen(P));
			A.Phase += Dt * A.Speed * 0.42f;
			if (A.T < 1.f) { continue; }
			const int32 Node = A.bBack ? Edges[A.Edge].A : Edges[A.Edge].B;
			if (A.bShow)
			{
				// The show's columns: in at the entrance, down to the stores, the load dropped, and away into the colony.
				if (Node == AntColony::Throne) { A.Edge = 3; A.bBack = false; A.T = 0.f; continue; }
				A.bShow = false;
				A.Carry = -1;
				PickNext(A, N, Node);
				continue;
			}
			if (Node == AntColony::Stores) { A.Carry = -1; }
			if (Node == Entrance && S.Rand.FRand() < 0.6f) { A.Carry = (int8)S.Rand.RandRange(0, AntFoodKind::Count - 1); }
			PickNext(A, N, Node);
		}
	}

	/** A tunnel that climbs (more up than across): its ants walk behind the chamber pictures, so they never cross the Queen or the stores. */
	bool IsShaft(int32 E)
	{
		const FVector2D A = NodePos(Edges[E].A), B = NodePos(Edges[E].B);
		return FMath::Abs(B.Y - A.Y) > FMath::Abs(B.X - A.X);
	}

	/** bShafts: the ants of the climbing tunnels (drawn before the chambers, small, dark, no load); otherwise the rest. */
	void DrawAnts(UCanvas* Cv, const FView& V, bool bShafts)
	{
		for (const FWalker& A : St().Ants)
		{
			if (A.Delay > 0.f || IsShaft(A.Edge) != bShafts) { continue; }
			if (bShafts)
			{
				FVector2D P[4];
				EdgePath(A.Edge, P);
				FVector2D Dir;
				const FVector2D At = PathAt(P, A.bBack ? 1.f - A.T : A.T, &Dir);
				DrawAnt(Cv, V, At, A.bBack ? -Dir : Dir, A.Size * 0.78f, A.Phase, A.bShow ? A.Carry : -1, FLinearColor(0.03f, 0.014f, 0.008f, 1.f), 0.6f);   // the show's carriers keep a muted load
				continue;
			}
			FVector2D P[4];
			EdgePath(A.Edge, P);
			FVector2D Dir;
			const FVector2D At = PathAt(P, A.bBack ? 1.f - A.T : A.T, &Dir);
			DrawAnt(Cv, V, At, A.bBack ? -Dir : Dir, A.Size * (A.bShow ? 1.35f : 1.f), A.Phase, A.Carry);
		}
	}

	void DrawMotes(UCanvas* Cv, const FView& V, float Dt)
	{
		FState& S = St();
		if (S.Motes.Num() == 0)
		{
			for (int32 i = 0; i < 44; ++i) { S.Motes.Add({ FVector2D(S.Rand.FRandRange(0.f, WW), S.Rand.FRandRange(150.f, WH)), FVector2D(S.Rand.FRandRange(-4.f, 4.f), S.Rand.FRandRange(-6.f, -1.f)), (float)S.Rand.FRandRange(1.2f, 3.2f), (float)(S.Rand.FRand() * 10.f) }); }
		}
		// The near layer: dust in the light, moving faster than the soil when the view pans (parallax 1.25).
		const FVector2D Mid(WW * 0.5f, WH * 0.5f);
		for (FMote& M : S.Motes)
		{
			M.P += M.V * Dt + FVector2D(FMath::Sin(S.Clock * 0.6f + M.Seed) * 3.f * Dt, 0.f);
			if (M.P.Y < 120.f) { M.P.Y = WH + 10.f; M.P.X = S.Rand.FRandRange(0.f, WW); }
			const FVector2D Par = S.Center + (M.P - S.Center) * 1.25f;
			const float A = 0.18f + 0.16f * FMath::Sin(S.Clock * 1.3f + M.Seed);
			Dot(Cv, V.S(Par), M.Size * V.U * 1.25f, FLinearColor(1.f, 0.85f, 0.6f, A), true);
		}
		(void)Mid;
	}
}

// ---------------------------------------------------------------------------------------------
// The post-raid show
// ---------------------------------------------------------------------------------------------

namespace
{
	float DevShowWant = -100.f;

	void StartShow(const AAntRaid* Raid)
	{
		FState& S = St();
		const FAntNestState& N = Raid->Nest;
		const FAntRaidResult& R = Raid->Result;
		S.ShowR = R;
		S.ShowT = 0.f;
		S.Stages.Reset();
		S.SleepAt = -1.f;
		S.Open = -1;
		float At = 0.f;
		auto Add = [&](uint8 K, float Len) { S.Stages.Add({ K, At, Len }); At += Len; };
		Add(Stage::Title, 2.2f);
		int32 Haul = R.Grown;
		for (int32 V : R.Kinds) { Haul += V; }
		if (Haul > 0 || R.Jelly > 0) { Add(Stage::Haul, 4.6f); }
		Add(Stage::Queen, 3.2f);
		if (R.LarvaeLaid > 0 || R.WorkersHatched > 0) { Add(Stage::Brood, 2.2f); }
		if (R.NewTrophies.Num() > 0) { Add(Stage::Trophy, 2.8f); }
		S.FlashChamber = -1;
		S.FlashDig = -1;
		for (uint8 c = 1; c < AntColony::Count && S.FlashChamber < 0; ++c) { if (AntColony::CanUpgrade(N, c)) { S.FlashChamber = c; } }
		if (S.FlashChamber < 0) { for (uint8 d = 0; d < AntColony::DigCount && S.FlashDig < 0; ++d) { if (AntColony::CanDig(N, d)) { S.FlashDig = d; } } }
		if (S.FlashChamber >= 0 || S.FlashDig >= 0) { Add(Stage::Build, 2.4f); }
		if (!R.Chronicle.IsEmpty()) { Add(Stage::Chronicle, 3.2f); }
		// Highlights: the personal layer's (ranks, medals) first, then what the colony saw itself.
		S.Highlights = AntMetaBus::GetPlayerHighlights();
		for (const FString& H : R.Heroes)
		{
			FString Title, Line;
			if (H.Split(TEXT("|"), &Title, &Line)) { FAntPlayerHighlight X; X.Title = Title; X.Line = Line; S.Highlights.Add(X); }
		}
		while (S.Highlights.Num() > 5) { S.Highlights.RemoveAt(S.Highlights.Num() - 1); }
		if (S.Highlights.Num() > 0) { Add(Stage::Heroes, 1.4f + 1.1f * S.Highlights.Num()); }
		// The haul walks in: columns of carriers from the entrance to the stores, loaded by kind.
		int32 Total = 0;
		for (int32 V : R.Kinds) { Total += V; }
		const int32 Carriers = FMath::Clamp(Total / 40, Total > 0 ? 6 : 0, 30);
		float HaulStart = 0.f;
		for (const FStage& G : S.Stages) { if (G.Kind == Stage::Haul) { HaulStart = G.Start; } }
		int32 k = 0, Left = R.Kinds.IsValidIndex(0) ? R.Kinds[0] : 0;
		for (int32 i = 0; i < Carriers; ++i)
		{
			while (k < AntFoodKind::Count - 1 && Left <= (Total / FMath::Max(1, Carriers)) / 2) { ++k; Left += R.Kinds.IsValidIndex(k) ? R.Kinds[k] : 0; }
			Left -= Total / FMath::Max(1, Carriers);
			FWalker A;
			A.bShow = true;
			A.Edge = 0;
			A.T = 0.f;
			A.Speed = 95.f;
			A.Size = 1.05f;
			A.Carry = (int8)k;
			A.Delay = HaulStart + 0.1f + i * 0.14f;
			A.Phase = i * 0.7f;
			S.Ants.Add(A);
		}
		AntColonyView::SetOn(true);
		UE_LOG(LogTemp, Display, TEXT("ANTCOLONY show for raid %d: %d stages, %.1f s, %d carriers, %d highlights"), R.RaidNo, S.Stages.Num(), At, Carriers, S.Highlights.Num());
	}

	float ShowLength() { const FState& S = St(); return S.Stages.Num() > 0 ? S.Stages.Last().Start + S.Stages.Last().Len : 0.f; }

	const FStage* CurrentStage()
	{
		const FState& S = St();
		for (const FStage& G : S.Stages) { if (S.ShowT >= G.Start && S.ShowT < G.Start + G.Len) { return &G; } }
		return nullptr;
	}

	/** Where the camera looks in a stage. */
	void StageCamera(const FStage* G, const FAntNestState& N, float& Zoom, FVector2D& Center)
	{
		Zoom = 1.f;
		Center = FVector2D(WW * 0.5f, WH * 0.5f);
		if (!G) { return; }
		auto Look = [&](uint8 c, float Z) { const AntColony::FChamberDef& D = AntColony::Def(c); Zoom = Z; Center = FVector2D(D.X, D.Y); };
		switch (G->Kind)
		{
		case Stage::Haul: Zoom = 1.3f; Center = FVector2D(960.f, 520.f); break;
		case Stage::Queen: Look(AntColony::Throne, 1.85f); break;
		case Stage::Brood: Look(AntColony::Nursery, 1.9f); break;
		case Stage::Trophy: Look(AntColony::Def(AntColony::Trophies).Dig >= 0 && AntColony::TierOf(N, AntColony::Trophies) > 0 ? AntColony::Trophies : AntColony::Stores, 1.8f); break;
		case Stage::Build:
			if (St().FlashChamber >= 0) { Look((uint8)St().FlashChamber, 1.5f); }
			else if (St().FlashDig >= 0) { FVector2D P[4]; EdgePath(DigEdge[St().FlashDig], P); Zoom = 1.5f; Center = (P[1] + P[2]) * 0.5f; }
			break;
		default: break;
		}
	}

	void Caption(UCanvas* Cv, float W, float Fs, const FString& Big, const FString& Small, float Alpha, const FLinearColor& Col)
	{
		if (Alpha <= 0.01f) { return; }
		const float Y = 96.f * Fs;
		const FVector2D M = AntUiCanvas::TextSize(Cv, Big, EFont::Display, 34.f * Fs);
		const float BW = FMath::Max(M.X, AntUiCanvas::TextSize(Cv, Small, EFont::Bold, 17.f * Fs).X) + 80.f * Fs;
		AntUiCanvas::Rect(Cv, W * 0.5f - BW * 0.5f, Y, BW, 92.f * Fs, FLinearColor(0.012f, 0.006f, 0.003f, 0.82f * Alpha));
		AntUiCanvas::Rect(Cv, W * 0.5f - BW * 0.5f, Y, BW, 3.f * Fs, WithA(Col, Alpha));
		AntUiCanvas::Rect(Cv, W * 0.5f - BW * 0.5f, Y + 89.f * Fs, BW, 3.f * Fs, WithA(Col, Alpha));
		AntUiCanvas::Text(Cv, Big, W * 0.5f, Y + 8.f * Fs, EFont::Display, 34.f * Fs, WithA(Col, Alpha), 1, 0.7f * Alpha);
		AntUiCanvas::Text(Cv, Small, W * 0.5f, Y + 58.f * Fs, EFont::Bold, 17.f * Fs, WithA(C::Sugar, Alpha), 1);
	}

	void DrawShowOverlay(UCanvas* Cv, const FView& V, const FAntNestState& N, float W, float H, float Fs)
	{
		FState& S = St();
		const FStage* G = CurrentStage();
		if (!G) { return; }
		const FAntRaidResult& R = S.ShowR;
		const float In_ = FMath::Clamp((S.ShowT - G->Start) / 0.35f, 0.f, 1.f), Out = FMath::Clamp((G->Start + G->Len - S.ShowT) / 0.35f, 0.f, 1.f);
		const float A = FMath::Min(In_, Out);
		const bool bWiped = R.End == AntRaidEnd::Wiped;
		const bool bCake = R.Cake == AntCake::Home && !bWiped;
		// The newsreel's frame: dark bars top and bottom.
		const float Bar = 46.f * Fs;
		AntUiCanvas::Rect(Cv, 0.f, H - Bar, W, Bar, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		AntUiCanvas::Text(Cv, TEXT("«ГНЕЗДОВОЕ РАДИО ЩЕЛИ» ПЕРЕДАЁТ"), 24.f * Fs, H - Bar + 12.f * Fs, EFont::Mono, 14.f * Fs, WithA(C::Honey, 0.85f));
		AntUiCanvas::Text(Cv, TEXT("[ЛКМ / Пробел] — пропустить"), W - 24.f * Fs, H - Bar + 12.f * Fs, EFont::Mono, 14.f * Fs, WithA(C::Muted, 0.9f), 2);
		switch (G->Kind)
		{
		case Stage::Title:
			Caption(Cv, W, Fs, FString::Printf(TEXT("РЕЙД %d: %s"), R.RaidNo, bWiped ? TEXT("ОТРЯД ПОГИБ") : bCake ? TEXT("ТОРТ ДОМА!") : TEXT("ВЕРНУЛИСЬ БЕЗ ТОРТА")),
				FString::Printf(TEXT("%s · угроза «%s»%s"), R.bSimulated ? TEXT("рейд на бумаге") : TEXT("сводка с фронта"), AntTier::Def(R.Tier).Name, R.Deaths > 0 ? *FString::Printf(TEXT(" · потерь %d"), R.Deaths) : TEXT("")),
				A, bWiped ? C::Jam : bCake ? C::Honey : C::Amber);
			break;
		case Stage::Haul:
		{
			int32 Total = 0;
			FString Parts;
			for (int32 k = 0; k < R.Kinds.Num() && k < AntFoodKind::Count; ++k)
			{
				Total += R.Kinds[k];
				if (R.Kinds[k] > 0) { Parts += FString::Printf(TEXT("%s%s +%d"), Parts.IsEmpty() ? TEXT("") : TEXT("  ·  "), AntColony::KindName((uint8)k), R.Kinds[k]); }
			}
			if (R.Grown > 0) { Parts += FString::Printf(TEXT("  ·  грибница +%d"), R.Grown); }
			if (R.Jelly + R.GrownJelly > 0) { Parts += FString::Printf(TEXT("  ·  молочко +%d"), R.Jelly + R.GrownJelly); }
			Caption(Cv, W, Fs, FString::Printf(TEXT("В КЛАДОВЫЕ: +%d"), Total + R.Grown), Parts, A, C::Honey);
			break;
		}
		case Stage::Queen:
		{
			const int32 D = R.MoodAfter - R.MoodBefore;
			Caption(Cv, W, Fs, FString::Printf(TEXT("КОРОЛЕВА %s%d"), D >= 0 ? TEXT("+") : TEXT(""), D), FString::Printf(TEXT("Её Сладчайшество %s (%d из 100)%s"), AntNest::MoodName(R.MoodAfter), R.MoodAfter,
				R.QueenAte > 0 ? *FString::Printf(TEXT(" · в гневе съела %d еды"), R.QueenAte) : TEXT("")), A, D >= 0 ? C::Leaf : C::Jam);
			break;
		}
		case Stage::Brood:
			Caption(Cv, W, Fs, FString::Printf(TEXT("ЯСЛИ: +%d %s"), R.LarvaeLaid, R.LarvaeLaid == 1 ? TEXT("ЛИЧИНКА") : R.LarvaeLaid < 5 && R.LarvaeLaid > 0 ? TEXT("ЛИЧИНКИ") : TEXT("ЛИЧИНОК")),
				FString::Printf(TEXT("+%d рабочих · личинок всего %d из %d"), R.WorkersHatched, N.Larvae, AntNest::MaxLarvae), A, C::Leaf);
			break;
		case Stage::Trophy:
		{
			const int32 Id = R.NewTrophies[0];
			Caption(Cv, W, Fs, FString::Printf(TEXT("НОВЫЙ ТРОФЕЙ: %s"), *FString(AntColony::Trophy(Id).Name).ToUpper()),
				FString::Printf(TEXT("за то, что: %s%s"), AntColony::Trophy(Id).How, R.NewTrophies.Num() > 1 ? *FString::Printf(TEXT(" · и ещё %d"), R.NewTrophies.Num() - 1) : TEXT("")), A, C::Honey);
			// It comes down from above and is hung on the wall.
			const float F = Ease((S.ShowT - G->Start) / (G->Len * 0.7f));
			UTexture2D* Tx = T(*FString::Printf(TEXT("Trophy_%s"), AntColony::Trophy(Id).Tex));
			const float Sz = FMath::Lerp(220.f, 80.f, F) * Fs;
			const FVector2D P = FMath::Lerp(FVector2D(W * 0.5f, H * 0.42f), FVector2D(W * 0.5f, H * 0.55f), F);
			if (Tx) { AntUiCanvas::Tile(Cv, Tx, P.X - Sz * 0.5f, P.Y - Sz * 0.5f, Sz, Sz, WithA(FLinearColor::White, 1.f - F * 0.6f), FVector2D(0.f, 0.f), FVector2D(1.f, 1.f), FMath::Sin(S.Clock * 3.f) * 6.f); }
			break;
		}
		case Stage::Build:
		{
			FString What, Sub;
			if (S.FlashChamber >= 0)
			{
				int32 Fd, J, M;
				AntColony::Cost(N, (uint8)S.FlashChamber, Fd, J, M);
				const int32 Next = AntColony::TierOf(N, (uint8)S.FlashChamber) + 1;
				What = FString::Printf(TEXT("МОЖНО СТРОИТЬ: %s %s"), *FString(AntColony::Def((uint8)S.FlashChamber).Name).ToUpper(), RomanN(Next));
				Sub = FString::Printf(TEXT("«%s»: %s — %d еды%s"), *AntColony::TierName((uint8)S.FlashChamber, Next), *AntColony::TierText((uint8)S.FlashChamber, Next), Fd, J > 0 ? *FString::Printf(TEXT(" + %d мол."), J) : TEXT(""));
			}
			else
			{
				int32 Fd, J;
				AntColony::DigCost(N, Fd, J);
				What = FString::Printf(TEXT("МОЖНО ПРОРЫТЬ: %s"), *FString(AntColony::Dig((uint8)S.FlashDig).Name).ToUpper());
				Sub = FString::Printf(TEXT("%s: %s — %d еды"), AntColony::Dig((uint8)S.FlashDig).Toward, AntColony::Dig((uint8)S.FlashDig).Text, Fd);
			}
			Caption(Cv, W, Fs, What, Sub, A, C::Amber);
			break;
		}
		case Stage::Chronicle:
		{
			const int32 Chars = FMath::FloorToInt((S.ShowT - G->Start) * 38.f);
			Caption(Cv, W, Fs, TEXT("ЛЕТОПИСЬ КОЛОНИИ"), R.Chronicle.Left(Chars), A, C::Sugar);
			break;
		}
		case Stage::Heroes:
		{
			Caption(Cv, W, Fs, TEXT("ОТЛИЧИЛИСЬ В РЕЙДЕ"), TEXT("приказом Её Сладчайшества"), A, C::Honey);
			for (int32 i = 0; i < S.Highlights.Num(); ++i)
			{
				const float Appear = Ease((S.ShowT - G->Start - 0.6f - i * 1.1f) / 0.5f);
				if (Appear <= 0.f) { continue; }
				const FAntPlayerHighlight& X = S.Highlights[i];
				const float CW = 330.f * Fs, CH = 118.f * Fs;
				const float CX = W * 0.5f - (S.Highlights.Num() * (CW + 16.f * Fs)) * 0.5f + i * (CW + 16.f * Fs);
				const float CY = H * 0.58f + (1.f - Appear) * 60.f * Fs;
				AntUiCanvas::Paper(Cv, CX, CY, CW, CH, true);
				const TCHAR* Icon = !X.Icon.IsEmpty() ? nullptr : X.Title.Contains(TEXT("ЗВАН")) ? TEXT("Medal") : X.Title.Contains(TEXT("МЕДАЛ")) ? TEXT("Medal") : X.Title.Contains(TEXT("СПАС")) ? TEXT("Medic") : X.Title.Contains(TEXT("НОСИЛ")) ? TEXT("Carry") : TEXT("Cake");
				UTexture2D* ITex = Icon ? AntUi::Icon(Icon) : AntUi::Tex(*X.Icon);
				AntUiCanvas::Tile(Cv, ITex, CX + 12.f * Fs, CY + 22.f * Fs, 72.f * Fs, 72.f * Fs, FLinearColor::White);
				AntUiCanvas::TextFit(Cv, X.Title, CX + 94.f * Fs, CY + 16.f * Fs, CW - 104.f * Fs, EFont::Display, 16.f * Fs, C::JamDeep);
				AntUiCanvas::Wrap(Cv, X.Player.IsEmpty() ? X.Line : FString::Printf(TEXT("%s: %s"), *X.Player, *X.Line), CX + 94.f * Fs, CY + 46.f * Fs, CW - 104.f * Fs, EFont::Body, 14.f * Fs, C::Graphite, 3);
				if (X.RankAfter > X.RankBefore) { AntUiCanvas::Stamp(Cv, FString::Printf(TEXT("ЗВАНИЕ %d"), X.RankAfter), CX + CW - 60.f * Fs, CY + CH - 22.f * Fs, -12.f, C::Jam, 13.f * Fs); }
			}
			break;
		}
		default:
			break;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// The sheets on top: stores, buttons, the chamber's card, the chronicle
// ---------------------------------------------------------------------------------------------

namespace
{
	bool Hot(float X, float Y, float W, float H) { const FVector2D& M = St().Mouse; return M.X >= X && M.X < X + W && M.Y >= Y && M.Y < Y + H; }

	bool Btn(UCanvas* Cv, const FString& Label, float X, float Y, float W, float H, bool bEnabled, const FString& Tip, float Fs, bool bOn = false)
	{
		FState& S = St();
		const bool bHot = Hot(X, Y, W, H);
		const FLinearColor Fill = !bEnabled ? FLinearColor(0.05f, 0.035f, 0.025f, 0.92f) : bOn ? C::LeafDeep : bHot ? C::Amber : WithA(C::AmberDeep, 0.95f);
		AntUiCanvas::Rect(Cv, X, Y, W, H, bEnabled ? C::Cocoa : FLinearColor(0.08f, 0.07f, 0.06f, 1.f));
		AntUiCanvas::Rect(Cv, X + 2.f, Y + 2.f, W - 4.f, H - 4.f, Fill);
		if (bEnabled) { AntUiCanvas::Rect(Cv, X + 2.f, Y + 2.f, W - 4.f, (H - 4.f) * 0.2f, FLinearColor(1.f, 0.9f, 0.7f, bHot ? 0.25f : 0.12f)); }
		const float Pt = 15.f * Fs;
		const FVector2D M = AntUiCanvas::TextSize(Cv, Label, EFont::Bold, Pt);
		AntUiCanvas::TextFit(Cv, Label, X + W * 0.5f, Y + (H - M.Y) * 0.5f, W - 12.f, EFont::Bold, Pt, bEnabled ? C::Sugar : C::Muted, 1);
		if (bHot && !Tip.IsEmpty()) { S.Tip = Tip; }
		return bHot && bEnabled && S.bClick;
	}

	void Send(AAntPlayerController* PC, uint8 Cmd, int32 A, int32 B) { if (PC) { PC->ServerRaid(Cmd, A, B); } }

	void DrawTopBar(UCanvas* Cv, const AAntRaid* Raid, const AAntGameState* GS, AAntPlayerController* PC, float W, float Fs, bool bHost)
	{
		const FAntNestState& N = Raid->Nest;
		const float BH = 84.f * Fs;
		AntUiCanvas::Rect(Cv, 0.f, 0.f, W, BH, FLinearColor(0.012f, 0.006f, 0.003f, 0.86f));
		AntUiCanvas::Rect(Cv, 0.f, BH - 3.f * Fs, W, 3.f * Fs, WithA(C::AmberDeep, 0.9f));
		AntUiCanvas::Text(Cv, TEXT("МУРАВЕЙНИК"), 22.f * Fs, 8.f * Fs, EFont::Display, 28.f * Fs, C::Amber, 0, 0.6f);
		AntUiCanvas::Text(Cv, FString::Printf(TEXT("Великая Колония Щели · уровень %d из %d · рейдов %d"), AntColony::Level(N), AntColony::MaxLevel(), N.Raids), 24.f * Fs, 50.f * Fs, EFont::Body, 13.f * Fs, C::Muted);
		struct FChip { const TCHAR* Icon; FString Value; FString Label; FLinearColor Col; FString Tip; };
		TArray<FChip> Chips;
		for (int32 k = 0; k < AntFoodKind::Count; ++k)
		{
			Chips.Add({ KindIcon(k), FString::Printf(TEXT("%d"), FMath::RoundToInt(ShownHeld(N, k))), AntColony::KindName((uint8)k), C::Sugar,
				FString::Printf(TEXT("%s в кладовых. Вся еда — одна казна: строят, растят личинок и рабочих из общей суммы (%d)"), AntColony::KindName((uint8)k), N.Food) });
		}
		Chips.Add({ TEXT("Jelly"), FString::Printf(TEXT("%d"), N.Jelly), TEXT("Молочко"), C::Jelly, TEXT("Маточное молочко: из торта, трофеев, находок, приказов. На него строят II и III ярусы") });
		Chips.Add({ TEXT("Larva"), FString::Printf(TEXT("%d/%d"), N.Larvae, AntNest::MaxLarvae), TEXT("Личинки"), N.Larvae > 1 ? C::Sugar : C::Jam, TEXT("Личинки — общие жизни в рейде: каждая смерть игрока тратит одну") });
		Chips.Add({ TEXT("Worker"), FString::Printf(TEXT("%d"), N.Workers), TEXT("Рабочие"), C::Sugar, TEXT("Рабочие идут в отряды игроков. Кто не вернулся из рейда — того больше нет") });
		const float CW = 118.f * Fs;
		float X = 430.f * Fs;
		for (const FChip& Ch : Chips)
		{
			AntUiCanvas::Tile(Cv, AntUi::Icon(Ch.Icon), X, 12.f * Fs, 40.f * Fs, 40.f * Fs, FLinearColor::White);
			AntUiCanvas::Text(Cv, Ch.Value, X + 44.f * Fs, 10.f * Fs, EFont::Bold, 21.f * Fs, Ch.Col);
			AntUiCanvas::Text(Cv, Ch.Label, X + 44.f * Fs, 40.f * Fs, EFont::Body, 12.f * Fs, C::Muted);
			if (Hot(X, 0.f, CW, BH)) { St().Tip = Ch.Tip; }
			X += CW;
		}
		// The Queen's mood.
		const float MX = X + 6.f * Fs, MW = 190.f * Fs;
		const int32 Mood = N.Mood;
		const FLinearColor MC = Mood >= 60 ? C::Leaf : Mood > 25 ? C::Amber : C::Jam;
		AntUiCanvas::Text(Cv, FString::Printf(TEXT("Королева %s"), AntNest::MoodName(Mood)), MX, 12.f * Fs, EFont::Bold, 14.f * Fs, MC);
		AntUiCanvas::Bar(Cv, MX, 40.f * Fs, MW, 14.f * Fs, Mood / 100.f, MC);
		if (Hot(MX, 0.f, MW, BH)) { St().Tip = TEXT("Настроение Королевы: торт, перевыполненный запрос, разная еда, трофеи и приказы радуют; смерти и потерянный торт огорчают. Довольная (60) разрешает III ярусы"); }
		// Buttons: the planning sheet, and go.
		if (Btn(Cv, TEXT("ШТАБ  [Tab]"), W - 446.f * Fs, 16.f * Fs, 196.f * Fs, 52.f * Fs, true, TEXT("Планшет штаба: угроза, приказы Королевы, отряд и карманы, усиления на рейд"), Fs))
		{
			AntColonyView::SetOn(false);
		}
		const AAntPlayerState* MyPS = PC ? PC->GetPlayerState<AAntPlayerState>() : nullptr;
		const float Left = Raid->StartEnds > 0.f && GS ? Raid->StartEnds - GS->GetServerWorldTimeSeconds() : -1.f;
		const bool bReady = MyPS && MyPS->bNestReady;
		const FString Go = Left >= 0.f ? FString::Printf(TEXT("ВЫХОДИМ ЧЕРЕЗ %d"), FMath::Max(0, FMath::CeilToInt(Left))) : bHost ? FString(TEXT("В РЕЙД  [Enter]")) : bReady ? FString(TEXT("ГОТОВ")) : FString(TEXT("ГОТОВ  [Enter]"));
		const bool bEnter = PC && PC->WasInputKeyJustPressed(EKeys::Enter);
		if (Btn(Cv, Go, W - 240.f * Fs, 16.f * Fs, 222.f * Fs, 52.f * Fs, true, bHost ? TEXT("Начать рейд (отряд, карманы и угроза — в штабе)") : TEXT("Отметить, что готов. Рейд начинает хост"), Fs, bReady && !bHost) || bEnter)
		{
			if (bHost) { Send(PC, AntRaidCmd::Ready, 1, 0); Send(PC, AntRaidCmd::Start, 0, 0); }
			else { Send(PC, AntRaidCmd::Ready, bReady ? 0 : 1, 0); }
		}
	}

	void DrawBottom(UCanvas* Cv, const FAntNestState& N, const AAntRaid* Raid, float W, float H, float Fs, bool bHost)
	{
		FState& S = St();
		// A strip of receipt paper: the chronicle's last line (the rest is in the radio's card and the save).
		const float BH = 46.f * Fs, Y = H - BH - 6.f * Fs, PW = W * 0.56f;
		AntUiCanvas::Paper(Cv, 14.f * Fs, Y, PW, BH, false);
		AntUiCanvas::Text(Cv, TEXT("ЛЕТОПИСЬ"), 28.f * Fs, Y + 13.f * Fs, EFont::Display, 13.f * Fs, C::JamDeep);
		const FString Last = N.Chronicle.Num() > 0 ? N.Chronicle.Last() : FString(TEXT("Пусто. Первая запись появится после первого рейда."));
		AntUiCanvas::TextFit(Cv, Last, 128.f * Fs, Y + 13.f * Fs, PW - 128.f * Fs, EFont::Mono, 13.f * Fs, C::Graphite);
		const FString Hint = !Raid->NestNote.IsEmpty() ? Raid->NestNote
			: bHost ? FString(TEXT("Клик по залу — карточка и стройка · колесо — зум · тяни — двигать · пунктир — ещё не прорыто"))
			: FString(TEXT("Колония хоста: строит он · клик по залу — карточка · колесо — зум · тяни — двигать"));
		AntUiCanvas::Rect(Cv, PW + 26.f * Fs, Y, W - PW - 40.f * Fs, BH, FLinearColor(0.012f, 0.006f, 0.003f, 0.72f));
		AntUiCanvas::Wrap(Cv, Hint, PW + 38.f * Fs, Y + 5.f * Fs, W - PW - 64.f * Fs, EFont::Body, 13.f * Fs, Raid->NestNote.IsEmpty() ? C::Muted : C::Jam, 2, 1.15f);
		// What the mouse is on: a slip of paper by the cursor.
		if (!S.Tip.IsEmpty() && S.Mouse.X >= 0.f)
		{
			const float TW = 430.f * Fs, Pt = 14.f * Fs;
			float X = S.Mouse.X + 22.f * Fs, TY = S.Mouse.Y + 20.f * Fs;
			if (X + TW > W - 10.f) { X = S.Mouse.X - TW - 16.f * Fs; }
			const float TH = AntUiCanvas::Wrap(Cv, S.Tip, -10000.f, -10000.f, TW - 24.f * Fs, EFont::Body, Pt, FLinearColor(0.f, 0.f, 0.f, 0.f), 8) + 18.f * Fs;
			if (TY + TH > H - 8.f) { TY = S.Mouse.Y - TH - 14.f * Fs; }
			AntUiCanvas::Rect(Cv, X + 4.f, TY + 5.f, TW, TH, FLinearColor(0.f, 0.f, 0.f, 0.35f));
			AntUiCanvas::Rect(Cv, X, TY, TW, TH, C::Paper);
			AntUiCanvas::Rect(Cv, X, TY, 4.f * Fs, TH, C::Amber);
			AntUiCanvas::Wrap(Cv, S.Tip, X + 14.f * Fs, TY + 9.f * Fs, TW - 24.f * Fs, EFont::Body, Pt, C::Graphite, 8);
		}
	}
}

namespace
{
	/** The card of a chamber (or of a tunnel to dig, Open = 100 + dig). Returns its rectangle for the mouse. */
	FBox2D DrawSheet(UCanvas* Cv, const FAntNestState& N, AAntPlayerController* PC, float W, float H, float Fs, bool bHost)
	{
		FState& S = St();
		const float PW = 520.f * Fs, PX = W - PW - 22.f * Fs, PY = 104.f * Fs, PH = H - PY - 70.f * Fs;
		const FBox2D Box(FVector2D(PX, PY), FVector2D(PX + PW, PY + PH));
		AntUiCanvas::Paper(Cv, PX, PY, PW, PH, true);
		if (Btn(Cv, TEXT("×"), PX + PW - 52.f * Fs, PY + 12.f * Fs, 40.f * Fs, 36.f * Fs, true, TEXT("Закрыть"), Fs)) { S.Open = -1; S.bClick = false; }
		const float X = PX + 26.f * Fs, TW = PW - 52.f * Fs;
		float Y = PY + 18.f * Fs;
		const FString HostOnly = TEXT("Колония хоста: строит и решает он");
		if (S.Open >= 100)
		{
			const uint8 D = (uint8)(S.Open - 100);
			const AntColony::FDigDef& Dg = AntColony::Dig(D);
			AntUiCanvas::Text(Cv, Dg.Name, X, Y, EFont::Display, 26.f * Fs, C::JamDeep);
			Y += 44.f * Fs;
			AntUiCanvas::Text(Cv, FString::Printf(TEXT("Ход %s"), Dg.Toward), X, Y, EFont::Bold, 17.f * Fs, C::Graphite);
			Y += 32.f * Fs;
			Y += AntUiCanvas::Wrap(Cv, Dg.Text, X, Y, TW, EFont::Body, 16.f * Fs, C::Graphite, 4) + 14.f * Fs;
			Y += AntUiCanvas::Wrap(Cv, TEXT("Каждый следующий ход дороже: копать дальше от гнезда. Куда рыть первым — решает хост, и колонии растут по-разному."), X, Y, TW, EFont::Body, 14.f * Fs, WithA(C::Graphite, 0.75f), 4) + 18.f * Fs;
			int32 F, J;
			AntColony::DigCost(N, F, J);
			FString Why;
			const bool bCan = AntColony::CanDig(N, D, &Why);
			if (AntColony::IsDug(N, D)) { AntUiCanvas::Text(Cv, TEXT("Прорыт"), X, Y, EFont::Display, 20.f * Fs, C::LeafDeep); }
			else if (Btn(Cv, FString::Printf(TEXT("ПРОРЫТЬ — %d еды%s"), F, J > 0 ? *FString::Printf(TEXT(" + %d мол."), J) : TEXT("")), X, Y, TW, 58.f * Fs, bCan && bHost, bHost ? (bCan ? FString(Dg.Text) : Why) : HostOnly, Fs))
			{
				Send(PC, AntRaidCmd::Dig, D, 0);
			}
			if (!bCan && !AntColony::IsDug(N, D)) { AntUiCanvas::Text(Cv, Why, X, Y + 66.f * Fs, EFont::Bold, 14.f * Fs, C::Jam); }
			return Box;
		}
		const uint8 c = (uint8)FMath::Clamp(S.Open, 0, AntColony::Count - 1);
		const AntColony::FChamberDef& D = AntColony::Def(c);
		const int32 Tier = AntColony::TierOf(N, c);
		AntUiCanvas::Tile(Cv, AntUi::Icon(D.Icon), PX + PW - 120.f * Fs, PY + 52.f * Fs, 70.f * Fs, 70.f * Fs, FLinearColor::White);
		AntUiCanvas::TextFit(Cv, D.Name, X, Y, TW - 100.f * Fs, EFont::Display, 26.f * Fs, C::JamDeep);
		Y += 44.f * Fs;
		AntUiCanvas::Text(Cv, Tier > 0 ? FString::Printf(TEXT("Ярус %s из III"), RomanN(Tier)) : AntColony::IsOpen(N, c) ? FString(TEXT("Не построено")) : FString::Printf(TEXT("За %s — ещё не прорыт"), AntColony::Dig((uint8)D.Dig).Name), X, Y, EFont::Bold, 16.f * Fs, Tier > 0 ? C::LeafDeep : C::Graphite);
		Y += 30.f * Fs;
		Y += AntUiCanvas::Wrap(Cv, D.About, X, Y, TW - 90.f * Fs, EFont::Body, 15.f * Fs, C::Graphite, 3) + 12.f * Fs;
		for (int32 t = 1; t <= AntColony::MaxTier; ++t)
		{
			const bool bHave = t <= Tier;
			const bool bNext = t == Tier + 1;
			AntUiCanvas::Rect(Cv, X, Y, TW, 1.f, WithA(C::Graphite, 0.25f));
			AntUiCanvas::Text(Cv, FString::Printf(TEXT("%s%s. %s"), bHave ? TEXT("+ ") : TEXT(""), RomanN(t), *AntColony::TierName(c, t)), X, Y + 6.f * Fs, EFont::Bold, 15.f * Fs, bHave ? C::LeafDeep : bNext ? C::AmberDeep : WithA(C::Graphite, 0.6f));
			Y += 28.f * Fs;
			Y += AntUiCanvas::Wrap(Cv, AntColony::TierText(c, t), X + 14.f * Fs, Y, TW - 14.f * Fs, EFont::Body, 13.5f * Fs, bHave || bNext ? C::Graphite : WithA(C::Graphite, 0.55f), 2) + 8.f * Fs;
		}
		Y += 4.f * Fs;
		// What the room holds.
		const float Pt = 13.f * Fs;
		auto Row = [&](const FString& L, const FLinearColor& Col) { AntUiCanvas::TextFit(Cv, L, X, Y, TW, EFont::Body, Pt, Col); Y += Pt * 1.4f; };
		switch (c)
		{
		case AntColony::Throne:
			Row(FString::Printf(TEXT("Королева %s: %d из 100"), AntNest::MoodName(N.Mood), N.Mood), C::Graphite);
			Row(FString::Printf(TEXT("Уровень колонии %d из %d (трон: 8 и 20)"), AntColony::Level(N), AntColony::MaxLevel()), C::Graphite);
			Row(FString::Printf(TEXT("Тортов дома %d · лучшая добыча %d"), N.CakesHome, N.BestHaul), C::Graphite);
			break;
		case AntColony::Stores:
			for (int32 k = 0; k < AntFoodKind::Count; ++k) { Row(FString::Printf(TEXT("%-7s %6d"), AntColony::KindName((uint8)k), AntColony::Held(N, (uint8)k)), C::Graphite); }
			Row(FString::Printf(TEXT("Всего   %6d · молочко %d"), N.Food, N.Jelly), C::JamDeep);
			break;
		case AntColony::Nursery:
			Row(FString::Printf(TEXT("Личинок %d из %d · рабочих %d"), N.Larvae, AntNest::MaxLarvae, N.Workers), C::Graphite);
			if (Btn(Cv, FString::Printf(TEXT("Вырастить личинку — %d еды"), AntNest::LarvaCost), X, Y, TW * 0.49f, 40.f * Fs, bHost && N.Food >= AntNest::LarvaCost && N.Larvae < AntNest::MaxLarvae, bHost ? TEXT("Ещё одна жизнь на всех в рейде") : HostOnly, Fs)) { Send(PC, AntRaidCmd::Larva, 0, 0); }
			if (Btn(Cv, FString::Printf(TEXT("+%d рабочих — %d еды"), AntNest::WorkerPack, AntNest::WorkerPackCost), X + TW * 0.51f, Y, TW * 0.49f, 40.f * Fs, bHost && N.Food >= AntNest::WorkerPackCost && N.Workers < AntNest::MaxWorkers, bHost ? TEXT("Рабочие для отрядов") : HostOnly, Fs)) { Send(PC, AntRaidCmd::Workers, 0, 0); }
			Y += 48.f * Fs;
			break;
		case AntColony::Fungus:
			Row(FString::Printf(TEXT("Вырастила всего: %d еды"), N.Grown), C::Graphite);
			break;
		case AntColony::Trophies:
			Row(FString::Printf(TEXT("Трофеев %d из %d · висят %d · к еде +%d%%"), N.Trophies.Num(), AntColony::TrophyCount(), FMath::Min(N.Trophies.Num(), AntColony::TrophySlots(N)), FMath::RoundToInt((AntColony::LootBonus(N) - 1.f) * 100.f)), C::JamDeep);
			for (int32 i = 0; i < AntColony::TrophyCount() && Y < PY + PH - 100.f * Fs; ++i)
			{
				const bool bHas = AntColony::HasTrophy(N, i);
				Row(FString::Printf(TEXT("%s %s — %s"), bHas ? TEXT("+") : TEXT("·"), AntColony::Trophy(i).Name, AntColony::Trophy(i).How), bHas ? C::LeafDeep : WithA(C::Graphite, 0.6f));
			}
			break;
		case AntColony::Memorial:
		{
			const TArray<FAntFallenVeteran> F = AntColony::Fallen(N);
			if (F.Num() == 0) { Row(TEXT("Имён на стене пока нет"), C::Graphite); }
			for (int32 i = 0; i < F.Num() && Y < PY + PH - 100.f * Fs; ++i) { Row(FString::Printf(TEXT("%s%s, рейд %d — %s"), F[i].Rank.IsEmpty() ? TEXT("") : *(F[i].Rank + TEXT(" ")), *F[i].Name, F[i].Raid, *F[i].Cause), C::Graphite); }
			break;
		}
		default:
			break;
		}
		// Build.
		const float BY = PY + PH - 86.f * Fs;
		if (c == AntColony::Throne)
		{
			AntUiCanvas::Wrap(Cv, TEXT("Тронный зал не строят: он растёт сам, когда растёт колония."), X, BY, TW, EFont::Body, 14.f * Fs, C::Graphite, 2);
		}
		else if (!AntColony::IsOpen(N, c))
		{
			const uint8 Dg = (uint8)D.Dig;
			int32 F, J;
			AntColony::DigCost(N, F, J);
			FString Why;
			const bool bCan = AntColony::CanDig(N, Dg, &Why);
			if (Btn(Cv, FString::Printf(TEXT("ПРОРЫТЬ %s — %d еды%s"), *FString(AntColony::Dig(Dg).Name).ToUpper(), F, J > 0 ? *FString::Printf(TEXT(" + %d мол."), J) : TEXT("")), X, BY, TW, 58.f * Fs, bCan && bHost, bHost ? (bCan ? FString(AntColony::Dig(Dg).Text) : Why) : HostOnly, Fs)) { Send(PC, AntRaidCmd::Dig, Dg, 0); }
		}
		else if (Tier < AntColony::MaxTier)
		{
			int32 F, J, M;
			AntColony::Cost(N, c, F, J, M);
			FString Why;
			const bool bCan = AntColony::CanUpgrade(N, c, &Why);
			if (Btn(Cv, FString::Printf(TEXT("ПОСТРОИТЬ %s — %d еды%s"), RomanN(Tier + 1), F, J > 0 ? *FString::Printf(TEXT(" + %d мол."), J) : TEXT("")), X, BY, TW, 58.f * Fs, bCan && bHost,
				bHost ? (bCan ? AntColony::TierText(c, Tier + 1) : Why) : HostOnly, Fs))
			{
				Send(PC, AntRaidCmd::Chamber, c, 0);
			}
			if (!bCan) { AntUiCanvas::Text(Cv, Why, X, BY - 24.f * Fs, EFont::Bold, 14.f * Fs, C::Jam); }
			if (M > 0) { AntUiCanvas::Text(Cv, TEXT("нужна довольная Королева (60)"), X + TW, BY - 24.f * Fs, EFont::Body, 12.f * Fs, WithA(C::Graphite, 0.7f), 2); }
		}
		else
		{
			AntUiCanvas::Text(Cv, TEXT("Построено всё"), X, BY + 10.f * Fs, EFont::Display, 20.f * Fs, C::LeafDeep);
		}
		return Box;
	}

	/** "Dig here" tags on the tunnels not dug yet. Returns the dig under the mouse, or -1. */
	int32 DrawDigTags(UCanvas* Cv, const FView& V, const FAntNestState& N, float Fs, bool bMouseFree)
	{
		int32 HotDig = -1;
		for (uint8 d = 0; d < AntColony::DigCount; ++d)
		{
			if (AntColony::IsDug(N, d)) { continue; }
			FVector2D P[4];
			EdgePath(DigEdge[d], P);
			FVector2D Mid = V.S((P[1] + P[2]) * 0.5f);
			int32 F, J;
			AntColony::DigCost(N, F, J);
			const FString L1 = FString::Printf(TEXT("КОПАТЬ: %s"), AntColony::Dig(d).Toward);
			const FString L2 = FString::Printf(TEXT("%d еды%s"), F, J > 0 ? *FString::Printf(TEXT(" + %d мол."), J) : TEXT(""));
			const float Pt = FMath::Clamp(11.f * V.U, 9.f, 18.f);
			const FVector2D M = AntUiCanvas::TextSize(Cv, L1, EFont::Bold, Pt);
			const float TW = M.X + 20.f, TH = M.Y * 2.3f + 10.f;
			Mid.X = FMath::Clamp(Mid.X, TW * 0.5f + 8.f, V.W - TW * 0.5f - 8.f);
			const bool bHot = bMouseFree && Hot(Mid.X - TW * 0.5f, Mid.Y - TH * 0.5f, TW, TH);
			const float Flash = St().FlashDig == d && St().ShowT >= 0.f ? Pulse(7.f) : 0.f;
			AntUiCanvas::Rect(Cv, Mid.X - TW * 0.5f + 3.f, Mid.Y - TH * 0.5f + 4.f, TW, TH, FLinearColor(0.f, 0.f, 0.f, 0.4f));
			AntUiCanvas::Rect(Cv, Mid.X - TW * 0.5f, Mid.Y - TH * 0.5f, TW, TH, bHot || Flash > 0.5f ? C::Honey : C::Paper);
			AntUiCanvas::Text(Cv, L1, Mid.X, Mid.Y - TH * 0.5f + 4.f, EFont::Bold, Pt, C::Graphite, 1);
			AntUiCanvas::Text(Cv, L2, Mid.X, Mid.Y - TH * 0.5f + 6.f + M.Y, EFont::Mono, Pt * 0.92f, AntColony::CanDig(N, d) ? C::LeafDeep : C::JamDeep, 1);
			if (bHot)
			{
				HotDig = d;
				St().Tip = FString::Printf(TEXT("%s (%s): %s. Клик — подробнее"), AntColony::Dig(d).Name, AntColony::Dig(d).Toward, AntColony::Dig(d).Text);
			}
		}
		return HotDig;
	}

	int32 ChamberAt(const FVector2D& World, const FAntNestState& N)
	{
		for (uint8 c = 0; c < AntColony::Count; ++c)
		{
			const AntColony::FChamberDef& D = AntColony::Def(c);
			const float DX = (World.X - D.X) / (D.W * 0.46f), DY = (World.Y - D.Y) / (D.H * 0.44f);
			if (DX * DX + DY * DY <= 1.f) { return c; }
		}
		return -1;
	}
}

// ---------------------------------------------------------------------------------------------
// The frame
// ---------------------------------------------------------------------------------------------

namespace AntColonyView
{

bool IsOn() { return St().bOn; }
void SetOn(bool bOn) { St().bOn = bOn; }
void DevOpen(int32 Chamber) { St().Open = Chamber; }
void Open(int32 Chamber)
{
	St().bOn = true;
	if (Chamber >= 0 && Chamber < AntColony::Count) { St().Open = Chamber; }
}
void DevZoom(float Zoom, float X, float Y)
{
	FState& S = St();
	S.ZoomGoal = S.Zoom = FMath::Clamp(Zoom, 1.f, 2.5f);
	S.CenterGoal = S.Center = FVector2D(X * WW, Y * WH);
}
void DevSleep(bool bAsleep) { St().SleepAt = bAsleep ? St().Clock : -1.f; St().LastInput = St().Clock; }
void DevShowAt(float Seconds) { DevShowWant = Seconds < 0.f ? -1.f : Seconds; }
FString DevStats()
{
	const FState& S = St();
	return FString::Printf(TEXT("%.2f ms, %d items, %d ants, zoom %.2f, show %.1f/%.1f"), S.FrameMs, S.Items, S.Ants.Num(), S.Zoom, S.ShowT, ShowLength());
}

void Draw(AHUD* Hud, UCanvas* Cv, AAntRaid* Raid, AAntGameState* GS, AAntPlayerController* PC)
{
	if (!Hud || !Cv || !Raid)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	FState& S = St();
	S.Items = 0;
	const float Dt = S.LastTime > 0.0 ? FMath::Clamp((float)(T0 - S.LastTime), 0.f, 0.1f) : 0.016f;
	S.LastTime = T0;
	S.Clock += Dt;
	const FAntNestState& N = Raid->Nest;
	const bool bHost = Hud->GetWorld()->GetNetMode() != NM_Client;
	const float W = Cv->ClipX, H = Cv->ClipY;
	const float Fs = FMath::Max(0.6f, FMath::Min(W / 1920.f, H / 1080.f));

	// ---- the show: once per raid (or a raid on paper, or the admin's replay)
	if (!S.bSeenInit) { S.bSeenInit = true; S.SeenShow = Raid->ColonyShow; }
	if (Raid->ColonyShow != S.SeenShow)
	{
		S.SeenShow = Raid->ColonyShow;
		if (Raid->Result.RaidNo > 0) { StartShow(Raid); }
	}
	if (DevShowWant > -50.f)
	{
		if (DevShowWant < 0.f) { S.ShowT = -1.f; S.Stages.Reset(); }
		else
		{
			if (S.ShowT < 0.f && Raid->Result.RaidNo > 0) { StartShow(Raid); }
			S.ShowT = DevShowWant;
			for (FWalker& A : S.Ants) { if (A.bShow) { A.Delay = FMath::Max(0.f, A.Delay - DevShowWant); } }
		}
		DevShowWant = -100.f;
	}

	// ---- the mouse (tests without a window click through "-AntDo nest click X Y")
	float MX = -1.f, MY = -1.f;
	bool bPress = false, bRelease = false, bDown = false;
	if (PC)
	{
		PC->GetMousePosition(MX, MY);
		bPress = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
		bRelease = PC->WasInputKeyJustReleased(EKeys::LeftMouseButton);
		bDown = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::RightMouseButton);
		bPress |= PC->WasInputKeyJustPressed(EKeys::RightMouseButton);
	}
	if (AAntHUD::DevClickAt.X > 0.f || AAntHUD::DevClickAt.Y > 0.f)
	{
		MX = AAntHUD::DevClickAt.X * W;
		MY = AAntHUD::DevClickAt.Y * H;
		bPress = AAntHUD::DevClickFrames == 2;
		bRelease = AAntHUD::DevClickFrames == 1;
		bDown = bPress;
		if (AAntHUD::DevClickFrames > 0) { --AAntHUD::DevClickFrames; }
	}
	const FVector2D Mouse(MX, MY);
	if (FVector2D::Distance(Mouse, S.Mouse) > 1.f || bPress) { S.LastInput = S.Clock; S.SleepAt = -1.f; }
	S.Mouse = Mouse;
	S.bClick = bPress;
	S.Tip.Reset();
	if (S.ShowT < 0.f && S.SleepAt < 0.f && S.Clock - S.LastInput > 25.f) { S.SleepAt = S.Clock; }

	// ---- the show runs, and can be skipped
	if (S.ShowT >= 0.f)
	{
		S.ShowT += Dt;
		const bool bSkip = bPress || (PC && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Escape)));
		if (bSkip || S.ShowT > ShowLength())
		{
			S.ShowT = -1.f;
			S.Stages.Reset();
			for (int32 i = S.Ants.Num() - 1; i >= 0; --i) { if (S.Ants[i].bShow) { S.Ants.RemoveAtSwap(i); } }
			S.ZoomGoal = 1.f;
			S.CenterGoal = FVector2D(WW * 0.5f, WH * 0.5f);
			bPress = bRelease = false;
			S.bClick = false;
		}
		else
		{
			StageCamera(CurrentStage(), N, S.ZoomGoal, S.CenterGoal);
		}
	}
	if (PC && PC->WasInputKeyJustPressed(EKeys::Tab)) { SetOn(false); }

	// ---- the view
	FView V;
	V.W = W;
	V.H = H;
	V.Base = FMath::Max(W / WW, H / WH);
	const bool bShowing = S.ShowT >= 0.f;
	if (!bShowing && PC)
	{
		const bool bUp = PC->WasInputKeyJustPressed(EKeys::MouseScrollUp), bDn = PC->WasInputKeyJustPressed(EKeys::MouseScrollDown);
		if (bUp || bDn)
		{
			const float NewZ = FMath::Clamp(S.ZoomGoal * (bUp ? 1.18f : 1.f / 1.18f), 1.f, 2.5f);
			// Keep the point under the mouse where it is.
			const float U0 = V.Base * S.Zoom;
			const FVector2D Under = S.Center + (Mouse - FVector2D(W, H) * 0.5f) / U0;
			S.CenterGoal = Under - (Mouse - FVector2D(W, H) * 0.5f) / (V.Base * NewZ);
			S.ZoomGoal = NewZ;
			S.LastInput = S.Clock;
		}
	}
	const float K = FMath::Min(1.f, Dt * (bShowing ? 2.2f : 9.f));
	S.Zoom += (S.ZoomGoal - S.Zoom) * K;
	// Dragging pans (no easing while the hand holds it).
	if (!bShowing)
	{
		if (bPress) { S.DragFrom = Mouse; S.DragCenter = S.CenterGoal; S.bDragging = false; S.bMouseWasDown = true; }
		if (bDown && S.bMouseWasDown && FVector2D::Distance(Mouse, S.DragFrom) > 7.f) { S.bDragging = true; }
		if (S.bDragging && bDown) { S.CenterGoal = S.DragCenter - (Mouse - S.DragFrom) / (V.Base * S.Zoom); S.Center = S.CenterGoal; }
		if (!bDown && !bRelease) { S.bMouseWasDown = false; }
	}
	S.Center += (S.CenterGoal - S.Center) * K;
	V.U = V.Base * S.Zoom;
	{
		const float HX = FMath::Min(WW * 0.5f, W * 0.5f / V.U), HY = FMath::Min(WH * 0.5f, H * 0.5f / V.U);
		S.Center.X = FMath::Clamp(S.Center.X, HX, WW - HX);
		S.Center.Y = FMath::Clamp(S.Center.Y, HY, WH - HY);
		S.CenterGoal.X = FMath::Clamp(S.CenterGoal.X, HX, WW - HX);
		S.CenterGoal.Y = FMath::Clamp(S.CenterGoal.Y, HY, WH - HY);
	}
	V.Off = FVector2D(W, H) * 0.5f - S.Center * V.U;

	// ---- where the mouse is: the sheets first, then the cutaway
	const bool bSheet = S.Open >= 0 && !bShowing;
	const float SheetX = W - 542.f * Fs;
	const bool bOverUi = Mouse.Y < 84.f * Fs || Mouse.Y > H - 56.f * Fs || (bSheet && Mouse.X > SheetX) || bShowing;
	const int32 HoverC = bOverUi ? -1 : ChamberAt(V.World(Mouse), N);

	// ---- the cutaway
	TickAnts(N, Dt);
	DrawBackground(Cv, V, N);
	DrawTunnels(Cv, V, N, -1);
	DrawAnts(Cv, V, true);   // the shafts: behind the chambers
	for (uint8 c = 0; c < AntColony::Count; ++c)
	{
		const float Flash = (bShowing && S.FlashChamber == c && CurrentStage() && CurrentStage()->Kind == Stage::Build) ? Pulse(7.f) : 0.f;
		DrawChamberShell(Cv, V, N, c, HoverC == c || S.Open == c, Flash);
	}
	for (uint8 c = 0; c < AntColony::Count; ++c) { DrawContents(Cv, V, N, Raid, c); }
	DrawQueen(Cv, V, N, bShowing ? S.ShowR : Raid->Result);
	DrawAnts(Cv, V, false);
	for (uint8 c = 0; c < AntColony::Count; ++c) { DrawLabel(Cv, V, N, c, HoverC == c || S.Open == c, bHost && AntColony::CanUpgrade(N, c)); }
	const int32 HotDig = DrawDigTags(Cv, V, N, Fs, !bOverUi);
	DrawMotes(Cv, V, Dt);
	AntUiCanvas::Tile(Cv, AntUi::Tex(TEXT("Materials/T_UI_Vignette")), -0.06f * W - (S.Center.X - WW * 0.5f) * 0.04f, -0.06f * H, W * 1.12f, H * 1.12f, FLinearColor(1.f, 1.f, 1.f, 0.85f));

	// ---- hover and click on the cutaway (a click is a press and release without dragging)
	if (HoverC >= 0 && S.Tip.IsEmpty())
	{
		const AntColony::FChamberDef& D = AntColony::Def((uint8)HoverC);
		const int32 Tier = AntColony::TierOf(N, (uint8)HoverC);
		S.Tip = FString::Printf(TEXT("%s%s. %s%s"), D.Name, Tier > 0 ? *FString::Printf(TEXT(" %s «%s»"), RomanN(Tier), *AntColony::TierName((uint8)HoverC, Tier)) : TEXT(""), D.About,
			Tier > 0 ? *FString::Printf(TEXT(". Сейчас: %s"), *AntColony::TierText((uint8)HoverC, Tier)) : TEXT(""));
	}
	if (bRelease && !S.bDragging && !bOverUi && !bShowing)
	{
		if (HotDig >= 0) { S.Open = 100 + HotDig; }
		else if (HoverC >= 0) { S.Open = HoverC; }
		else { S.Open = -1; }
	}
	if (bRelease) { S.bDragging = false; S.bMouseWasDown = false; }

	// ---- the sheets
	if (bShowing)
	{
		DrawShowOverlay(Cv, V, N, W, H, Fs);
	}
	else
	{
		DrawTopBar(Cv, Raid, GS, PC, W, Fs, bHost);
		if (S.Open >= 0) { DrawSheet(Cv, N, PC, W, H, Fs, bHost); }
		DrawBottom(Cv, N, Raid, W, H, Fs, bHost);
	}
	if (AAntHUD::DevClickAt.X > 0.f || AAntHUD::DevClickAt.Y > 0.f)
	{
		AntUiCanvas::Rect(Cv, MX - 9.f, MY - 1.f, 18.f, 3.f, FLinearColor::White);
		AntUiCanvas::Rect(Cv, MX - 1.f, MY - 9.f, 3.f, 18.f, FLinearColor::White);
	}
	S.FrameMs = (float)((FPlatformTime::Seconds() - T0) * 1000.0);
	// Tests on any machine (a client has no -AntDo): -AntColonyShots=12,20,30 -AntColonyShotName=client -> Saved/Screenshots/colony/client_N.png
	{
		static TArray<float> ShotsAt;
		static FString ShotName;
		static bool bParsed = false;
		static int32 NextShot = 0;
		if (!bParsed)
		{
			bParsed = true;
			FString List;
			if (FParse::Value(FCommandLine::Get(), TEXT("AntColonyShots="), List, false))
			{
				TArray<FString> Parts;
				List.ParseIntoArray(Parts, TEXT(","));
				for (const FString& P : Parts) { ShotsAt.Add(FCString::Atof(*P)); }
			}
			if (!FParse::Value(FCommandLine::Get(), TEXT("AntColonyShotName="), ShotName)) { ShotName = TEXT("view"); }
		}
		if (ShotsAt.IsValidIndex(NextShot) && S.Clock >= ShotsAt[NextShot])
		{
			const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("colony") / FString::Printf(TEXT("%s_%d.png"), *ShotName, NextShot);
			FScreenshotRequest::RequestScreenshot(File, true, false);
			UE_LOG(LogTemp, Display, TEXT("ANTCOLONY shot %s (%s) | level %d, food %d, raids %d | %s"), *File, bHost ? TEXT("host") : TEXT("client"), AntColony::Level(N), N.Food, N.Raids, *DevStats());
			++NextShot;
		}
	}
	static double LastLog = 0.0;
	if (T0 - LastLog > 10.0)
	{
		LastLog = T0;
		UE_LOG(LogTemp, Display, TEXT("ANTCOLONY view: %s"), *DevStats());
	}
}

}   // namespace AntColonyView
