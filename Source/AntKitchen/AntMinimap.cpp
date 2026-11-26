#include "AntMinimap.h"
#include "AntSquad.h"
#include "AntTrail.h"
#include "AntRaid.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntKitchenGameMode.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "KitchenBuilder.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

using namespace AntUi;

namespace
{
	int32 GMode = AntMinimap::Near;

	constexpr float NearRadius = 4200.f;     // world units from the middle to the edge in the near mode (a third of the kitchen)

	/** World -> screen of the map. Near: centred on the player, the camera's forward is up. Whole: the flat, north (-Y) up. */
	struct FMapXf
	{
		FVector2D Org;         // screen point of the centre
		FVector2D Centre;      // world XY at the centre
		float Scale = 1.f;     // pixels a world unit
		float Cos = 1.f, Sin = 0.f;
		bool bTurn = false;
		FBox2D Clip;           // the map's rectangle on the screen

		FVector2D operator()(const FVector2D& W) const
		{
			const FVector2D D = W - Centre;
			if (!bTurn)
			{
				return Org + FVector2D(D.X, D.Y) * Scale;   // (world X right, world Y down: seen from above, not mirrored)
			}
			// forward (Cos, Sin) up, right (-Sin, Cos) right
			const float Right = -D.X * Sin + D.Y * Cos;
			const float Fwd = D.X * Cos + D.Y * Sin;
			return Org + FVector2D(Right, -Fwd) * Scale;
		}
		FVector2D operator()(const FVector& W) const { return (*this)(FVector2D(W.X, W.Y)); }
		bool Inside(const FVector2D& P, float Pad = 0.f) const
		{
			return P.X >= Clip.Min.X + Pad && P.X <= Clip.Max.X - Pad && P.Y >= Clip.Min.Y + Pad && P.Y <= Clip.Max.Y - Pad;
		}
	};

	/** Sutherland-Hodgman: a convex polygon clipped to the map's rectangle. */
	void ClipPoly(TArray<FVector2D>& Poly, const FBox2D& R)
	{
		auto Pass = [&Poly](int32 Axis, float V, bool bKeepGreater)
		{
			TArray<FVector2D> In = MoveTemp(Poly);
			Poly.Reset();
			const int32 N = In.Num();
			for (int32 i = 0; i < N; ++i)
			{
				const FVector2D& A = In[i];
				const FVector2D& B = In[(i + 1) % N];
				const bool bA = bKeepGreater ? A[Axis] >= V : A[Axis] <= V;
				const bool bB = bKeepGreater ? B[Axis] >= V : B[Axis] <= V;
				if (bA) { Poly.Add(A); }
				if (bA != bB)
				{
					const float T = (V - A[Axis]) / (B[Axis] - A[Axis]);
					Poly.Add(A + (B - A) * T);
				}
			}
		};
		Pass(0, R.Min.X, true);
		if (Poly.Num()) { Pass(0, R.Max.X, false); }
		if (Poly.Num()) { Pass(1, R.Min.Y, true); }
		if (Poly.Num()) { Pass(1, R.Max.Y, false); }
	}

	/** Liang-Barsky: a segment clipped to the rectangle. False: nothing of it is inside. */
	bool ClipSeg(FVector2D& A, FVector2D& B, const FBox2D& R)
	{
		const FVector2D D = B - A;
		float T0 = 0.f, T1 = 1.f;
		const float P[4] = { -D.X, D.X, -D.Y, D.Y };
		const float Q[4] = { A.X - R.Min.X, R.Max.X - A.X, A.Y - R.Min.Y, R.Max.Y - A.Y };
		for (int32 i = 0; i < 4; ++i)
		{
			if (FMath::IsNearlyZero(P[i]))
			{
				if (Q[i] < 0.f) { return false; }
				continue;
			}
			const float T = Q[i] / P[i];
			if (P[i] < 0.f) { T0 = FMath::Max(T0, T); }
			else { T1 = FMath::Min(T1, T); }
			if (T0 > T1) { return false; }
		}
		const FVector2D A0 = A;
		A = A0 + D * T0;
		B = A0 + D * T1;
		return true;
	}

	void Tris(UCanvas* Cv, const TArray<FCanvasUVTri>& T)
	{
		if (T.Num() == 0) { return; }
		FCanvasTriangleItem Item(T, GWhiteTexture);
		Item.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(Item);
	}

	void AddPoly(TArray<FCanvasUVTri>& Out, const TArray<FVector2D>& P, const FLinearColor& C)
	{
		for (int32 i = 1; i + 1 < P.Num(); ++i)
		{
			FCanvasUVTri& T = Out.AddDefaulted_GetRef();
			T.V0_Pos = P[0]; T.V1_Pos = P[i]; T.V2_Pos = P[i + 1];
			T.V0_Color = T.V1_Color = T.V2_Color = C;
		}
	}

	void Line(UCanvas* Cv, FVector2D A, FVector2D B, float Th, const FLinearColor& C, const FBox2D& Clip)
	{
		if (!ClipSeg(A, B, Clip)) { return; }
		FCanvasLineItem L(A, B);
		L.SetColor(C);
		L.LineThickness = Th;
		L.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(L);
	}

	/** An arrow head: Tip in Dir (screen), Size long. */
	void Arrow(TArray<FCanvasUVTri>& Out, const FVector2D& At, const FVector2D& Dir, float Size, const FLinearColor& C)
	{
		const FVector2D F = Dir.GetSafeNormal().IsNearlyZero() ? FVector2D(0.f, -1.f) : Dir.GetSafeNormal();
		const FVector2D R(-F.Y, F.X);
		const FVector2D Tip = At + F * Size, L = At - F * Size * 0.6f - R * Size * 0.7f, Rt = At - F * Size * 0.6f + R * Size * 0.7f, Back = At - F * Size * 0.25f;
		AddPoly(Out, { Tip, Rt, Back }, C);
		AddPoly(Out, { Tip, Back, L }, C);
	}

	/** The district's floor tone: kitchen, living room, yard, inside the walls. */
	FLinearColor Floor(const TCHAR* Name)
	{
		static const TCHAR* Living[] = { TEXT("Порог"), TEXT("Бабушкин угол"), TEXT("Игровая"), TEXT("Стенка") };
		static const TCHAR* Yard[] = { TEXT("Палисадник"), TEXT("Красный двор") };
		for (const TCHAR* N : Living) { if (FCString::Strcmp(N, Name) == 0) { return Hex(0x5A4636, 0.92f); } }
		for (const TCHAR* N : Yard) { if (FCString::Strcmp(N, Name) == 0) { return Hex(0x3C4A2E, 0.85f); } }
		if (FCString::Strcmp(Name, TEXT("Застенье")) == 0) { return Hex(0x2A1E16, 0.95f); }
		return Hex(0x6B5440, 0.92f);   // the kitchen
	}

	/** Players of the colony in the lobby's order (by id), so the colour of a slot is the same everywhere. */
	int32 SlotOf(const UWorld* W, const APlayerState* PS)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		if (!GS || !PS) { return 0; }
		TArray<const APlayerState*> Ps;
		for (const APlayerState* P : GS->PlayerArray) { if (P && !P->IsABot()) { Ps.Add(P); } }
		Ps.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
		return FMath::Max(0, Ps.IndexOfByKey(PS));
	}

	struct FMapPlayer
	{
		const APlayerState* PS = nullptr;
		const AAntCharacter* Ant = nullptr;
		FLinearColor Col;
		bool bMe = false;
	};

	void Players(UWorld* W, const APlayerState* MyPS, TArray<FMapPlayer>& Out)
	{
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			const APlayerState* PS = It->GetPlayerState();
			if (!PS || PS->IsABot() || It->Team != 0 || It->Creature != 0 || It->bCorpseGone)
			{
				continue;
			}
			if (Out.ContainsByPredicate([PS](const FMapPlayer& P) { return P.PS == PS; })) { continue; }
			const AAntPlayerState* APS = Cast<AAntPlayerState>(PS);
			FMapPlayer& P = Out.AddDefaulted_GetRef();
			P.PS = PS;
			P.Ant = *It;
			P.Col = PlayerColor(SlotOf(W, PS), APS ? APS->AntColor : FLinearColor::White);
			P.bMe = PS == MyPS;
		}
	}
}

namespace AntMinimap
{
	int32& Mode() { return GMode; }

	void Draw(AHUD* Hud, UCanvas* Cv, APlayerController* PC, float S)
	{
		UWorld* W = Hud ? Hud->GetWorld() : nullptr;
		if (!W || !Cv || !PC)
		{
			return;
		}
		if (PC->WasInputKeyJustPressed(EKeys::I) && !PC->IsInputKeyDown(EKeys::LeftControl))
		{
			GMode = (GMode + 1) % Count;
		}
		// Top right, under the feed's lines (the top left is the snowball's mass panel, the bottom left health and the squad).
		const float Top = 122.f * S;
		if (GMode == Off)
		{
			AntUiCanvas::Text(Cv, TEXT("[I] карта"), Cv->ClipX - 22.f * S, Top, EFont::Bold, FMath::Max(11.f, 13.f * S), WithA(C::Sugar, 0.55f), 2, 0.5f);
			return;
		}
		const APawn* MePawn = PC->GetPawn();
		const APlayerState* MyPS = PC->PlayerState;
		const AAntRaid* Raid = AAntRaid::Get(W);
		const float Px = FMath::Max(11.f, 13.f * S);   // the smallest text: still readable at 1280x720

		// ---- the frame and the transform
		FMapXf X;
		float MW, MH;
		FVector2D Me2(0.f, 0.f);
		float CamYaw = 0.f;
		if (PC->PlayerCameraManager) { CamYaw = (float)PC->PlayerCameraManager->GetCameraRotation().Yaw; }
		if (MePawn) { Me2 = FVector2D(MePawn->GetActorLocation().X, MePawn->GetActorLocation().Y); }
		if (GMode == Near)
		{
			MW = MH = 300.f * S;
			X.Centre = Me2;
			X.Scale = MW * 0.5f / NearRadius;
			X.bTurn = true;
			X.Cos = FMath::Cos(FMath::DegreesToRadians(CamYaw));
			X.Sin = FMath::Sin(FMath::DegreesToRadians(CamYaw));
		}
		else
		{
			// The whole flat: the yard on the west to the living room's far wall (level v3's districts).
			const FBox2D Flat(FVector2D(-15300.f, -12200.f), FVector2D(20400.f, 5600.f));
			MW = 560.f * S;
			MH = MW * (Flat.Max.Y - Flat.Min.Y) / (Flat.Max.X - Flat.Min.X);
			X.Centre = Flat.GetCenter();
			X.Scale = MW / (Flat.Max.X - Flat.Min.X);
		}
		const float X0 = Cv->ClipX - MW - 22.f * S, Y0 = Top;
		X.Org = FVector2D(X0 + MW * 0.5f, Y0 + MH * 0.5f);
		X.Clip = FBox2D(FVector2D(X0, Y0), FVector2D(X0 + MW, Y0 + MH));

		AntUiCanvas::Rect(Cv, X0 + 4.f * S, Y0 + 5.f * S, MW, MH, FLinearColor(0.f, 0.f, 0.f, 0.35f));
		AntUiCanvas::Rect(Cv, X0, Y0, MW, MH, WithA(C::Ink, 0.86f));

		// ---- the floor plan: districts, the walls of the house
		TArray<FCanvasUVTri> T;
		struct FLabel { FString Name; FVector2D At; float Area; };
		TArray<FLabel> Labels;
		if (LevelV3::Enabled())
		{
			for (const FLevelV2District& D : LevelV3::Districts())
			{
				TArray<FVector2D> P = { X(FVector2D(D.X0, D.Y0)), X(FVector2D(D.X1, D.Y0)), X(FVector2D(D.X1, D.Y1)), X(FVector2D(D.X0, D.Y1)) };
				ClipPoly(P, X.Clip);
				if (P.Num() >= 3) { AddPoly(T, P, Floor(D.Name)); }
				const float Area = (D.X1 - D.X0) * (D.Y1 - D.Y0);
				FLabel* L = Labels.FindByPredicate([&D](const FLabel& Lb) { return Lb.Name == D.Name; });
				if (!L) { Labels.Add({ FString(D.Name), X(FVector2D((D.X0 + D.X1) * 0.5f, (D.Y0 + D.Y1) * 0.5f)), Area }); }
				else if (Area > L->Area) { L->At = X(FVector2D((D.X0 + D.X1) * 0.5f, (D.Y0 + D.Y1) * 0.5f)); L->Area = Area; }
			}
			Tris(Cv, T);
			for (const FLevelV2Box& B : LevelV3::TargetBounds())
			{
				const FVector2D P[4] = { X(FVector2D(B.X0, B.Y0)), X(FVector2D(B.X1, B.Y0)), X(FVector2D(B.X1, B.Y1)), X(FVector2D(B.X0, B.Y1)) };
				for (int32 i = 0; i < 4; ++i) { Line(Cv, P[i], P[(i + 1) % 4], FMath::Max(1.f, 2.f * S), WithA(C::Sugar, 0.45f), X.Clip); }
			}
		}
		// Words on the map: never outside its frame, never over each other (the players' names and the landmarks first).
		TArray<FBox2D> Used;
		auto Label = [&](const FString& Str, float Ax, float Ay, const FLinearColor& Col, int32 Align) -> bool
		{
			const FVector2D Sz = AntUiCanvas::TextSize(Cv, Str, EFont::Bold, Px);
			const float L = Align == 1 ? Ax - Sz.X * 0.5f : Align == 2 ? Ax - Sz.X : Ax;
			const FBox2D B(FVector2D(L - 2.f, Ay), FVector2D(L + Sz.X + 2.f, Ay + Sz.Y));
			if (B.Min.X < X.Clip.Min.X + 2.f || B.Max.X > X.Clip.Max.X - 2.f || B.Min.Y < X.Clip.Min.Y + 1.f || B.Max.Y > X.Clip.Max.Y - 1.f)
			{
				return false;
			}
			for (const FBox2D& U : Used) { if (U.Intersect(B)) { return false; } }
			Used.Add(B);
			AntUiCanvas::Text(Cv, Str, Ax, Ay, EFont::Bold, Px, Col, Align, 0.6f);
			return true;
		};

		// ---- the trails, in the colour of whoever laid them
		TArray<FMapPlayer> Ps;
		Players(W, MyPS, Ps);
		auto ColourOf = [&Ps, W](const APlayerState* PS) -> FLinearColor
		{
			if (const FMapPlayer* P = Ps.FindByPredicate([PS](const FMapPlayer& Q) { return Q.PS == PS; })) { return P->Col; }
			const AAntPlayerState* A = Cast<AAntPlayerState>(PS);
			return PlayerColor(SlotOf(W, PS), A ? A->AntColor : FLinearColor::White);
		};
		TArray<FAntTrailView> Trails;
		AntTrail::GetTrails(W, Trails);
		for (const FAntTrailView& V : Trails)
		{
			const FLinearColor Col = WithA(ColourOf(V.Owner), V.bLaying ? 0.55f : 0.9f);
			for (int32 i = 1; i < V.Points.Num(); ++i)
			{
				Line(Cv, X(V.Points[i - 1]), X(V.Points[i]), FMath::Max(1.5f, 3.f * S), Col, X.Clip);
			}
		}

		// ---- the nest, the south entrance, the cake
		auto Spot = [&](const FVector& At, const FString& Text, const FLinearColor& Col, float Size)
		{
			const FVector2D P = X(At);
			if (!X.Inside(P, Size)) { return; }
			TArray<FCanvasUVTri> D;
			AddPoly(D, { P + FVector2D(0.f, -Size), P + FVector2D(Size, 0.f), P + FVector2D(0.f, Size), P + FVector2D(-Size, 0.f) }, WithA(C::Ink, 0.9f));
			const float K = Size * 0.68f;
			AddPoly(D, { P + FVector2D(0.f, -K), P + FVector2D(K, 0.f), P + FVector2D(0.f, K), P + FVector2D(-K, 0.f) }, Col);
			Tris(Cv, D);
			if (!Text.IsEmpty()) { Label(Text, P.X, P.Y + Size + 1.f, Col, 1); }
		};
		Spot(AKitchenBuilder::NestLocation(0), TEXT("ГНЕЗДО"), C::Amber, 7.f * S);
		if (LevelV3::Enabled()) { Spot(LevelV2::SouthEntrance(), GMode == Whole ? FString() : FString(TEXT("ЮЖН. ВХОД")), C::AmberDeep, 5.f * S); }
		if (Raid && Raid->bCakeKnown && (Raid->Cake == AntCake::Lying || Raid->Cake == AntCake::Clan || Raid->Cake == AntCake::InHill))
		{
			const float Beat = 0.75f + 0.25f * FMath::Sin(W->GetTimeSeconds() * 5.f);
			Spot(FVector(Raid->CakeLoc), TEXT("ТОРТ"), WithA(Raid->Cake == AntCake::Lying ? C::Honey : C::Jam, Beat), 7.f * S);
		}

		// ---- the squads: a dot a worker (a sample), own squad brighter
		TArray<FVector> Sq;
		for (const FMapPlayer& P : Ps)
		{
			AntSquad::GetSquadPositions(P.PS, Sq);
			const float D = FMath::Max(3.f, (P.bMe ? 5.f : 4.f) * S);
			const FLinearColor Col = WithA(P.Col, P.bMe ? 1.f : 0.8f);
			for (const FVector& At : Sq)
			{
				const FVector2D Q = X(At);
				if (!X.Inside(Q, D)) { continue; }
				AntUiCanvas::Rect(Cv, Q.X - D * 0.5f - 1.f, Q.Y - D * 0.5f - 1.f, D + 2.f, D + 2.f, WithA(C::Ink, 0.85f));
				AntUiCanvas::Rect(Cv, Q.X - D * 0.5f, Q.Y - D * 0.5f, D, D, Col);
			}
		}

		// ---- the players: an arrow each in his colour, the name beside it; one off the map sits on its edge
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (const FMapPlayer& P : Ps)
			{
				if (P.bMe != (Pass == 1)) { continue; }   // (me last: on top)
				FVector2D Q = X(P.Ant->GetActorLocation());
				const float Yaw = P.bMe ? CamYaw : (float)P.Ant->GetActorRotation().Yaw;
				const FVector Fw(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.f);
				const FVector2D Dir = X(P.Ant->GetActorLocation() + Fw * 100.f) - Q;
				const float Size = FMath::Max(P.bMe ? 8.f : 6.5f, (P.bMe ? 11.f : 9.f) * S);
				const bool bOff = !X.Inside(Q, Size);
				if (bOff)
				{
					Q.X = FMath::Clamp(Q.X, X.Clip.Min.X + Size, X.Clip.Max.X - Size);
					Q.Y = FMath::Clamp(Q.Y, X.Clip.Min.Y + Size, X.Clip.Max.Y - Size);
				}
				TArray<FCanvasUVTri> A;
				if (P.Ant->bDead)
				{
					// dead: a cross (the body waits to be carried home)
					const float K = Size * 0.7f;
					AntUiCanvas::Rect(Cv, Q.X - K, Q.Y - 1.5f * S, K * 2.f, 3.f * S, WithA(P.Col, 0.8f));
					AntUiCanvas::Rect(Cv, Q.X - 1.5f * S, Q.Y - K, 3.f * S, K * 2.f, WithA(P.Col, 0.8f));
				}
				else
				{
					// (mine: a light rim, so it is found at a glance; the others: an ink rim)
					Arrow(A, Q, Dir, Size + FMath::Max(2.f, 3.f * S), P.bMe ? C::Sugar : WithA(C::Ink, bOff ? 0.6f : 0.95f));
					Arrow(A, Q, Dir, Size, WithA(P.Col, bOff ? 0.65f : 1.f));
					Tris(Cv, A);
				}
				Used.Add(FBox2D(Q - FVector2D(Size, Size), Q + FVector2D(Size, Size)));   // (no district's name over a player)
				if (!P.bMe)
				{
					const FString Name = P.PS->GetPlayerName().Left(10);
					const FLinearColor NC = WithA(P.Col, bOff ? 0.75f : 1.f);
					if (!Label(Name, Q.X + Size + 3.f * S, Q.Y - Px * 0.62f, NC, 0)) { Label(Name, Q.X - Size - 3.f * S, Q.Y - Px * 0.62f, NC, 2); }
				}
			}
		}

		// ---- the districts' names, where there is room (the biggest first)
		Labels.Sort([](const FLabel& A, const FLabel& B) { return A.Area > B.Area; });
		for (const FLabel& L : Labels)
		{
			Label(L.Name, L.At.X, L.At.Y - Px * 0.6f, WithA(C::Sugar, 0.55f), 1);
		}

		// Dev: -AntMapLog writes what this machine's map shows every 5 s (the co-op test reads the guest's log).
		static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("AntMapLog"));
		static double NextLog = 0.0;
		if (bLog && FPlatformTime::Seconds() > NextLog)
		{
			NextLog = FPlatformTime::Seconds() + 5.0;
			FString Who;
			int32 Dots = 0;
			for (const FMapPlayer& P : Ps)
			{
				AntSquad::GetSquadPositions(P.PS, Sq);
				Dots += Sq.Num();
				const FVector2D Q = X(P.Ant->GetActorLocation());
				Who += FString::Printf(TEXT(" [%s%s %s squad %d %s]"), *P.PS->GetPlayerName(), P.bMe ? TEXT("*") : TEXT(""), *P.Col.ToFColor(true).ToHex(), Sq.Num(), X.Inside(Q) ? TEXT("on map") : TEXT("edge"));
			}
			UE_LOG(LogTemp, Display, TEXT("ANTMAP %s mode %d: %d players, %d squad dots, %d trails, cake %s:%s"), W->GetNetMode() == NM_Client ? TEXT("guest") : TEXT("host"), GMode, Ps.Num(), Dots, Trails.Num(),
				Raid && Raid->bCakeKnown ? TEXT("known") : TEXT("unknown"), *Who);
		}

		// ---- the frame, north, the key
		AntUiCanvas::Frame(Cv, X0, Y0, MW, MH, FMath::Max(1.f, 2.f * S), WithA(C::Amber, 0.85f));
		if (GMode == Near)
		{
			// North (-Y) on the rim.
			const FVector2D N = (X(FVector2D(Me2.X, Me2.Y - 1000.f)) - X.Org).GetSafeNormal();
			const float Hw = MW * 0.5f - 9.f * S;
			const float K = Hw / FMath::Max(FMath::Abs(N.X), FMath::Abs(N.Y));
			const FVector2D At = X.Org + N * K;
			AntUiCanvas::Rect(Cv, At.X - 8.f * S, At.Y - 8.f * S, 16.f * S, 16.f * S, WithA(C::Ink, 0.9f));
			AntUiCanvas::Text(Cv, TEXT("С"), At.X, At.Y - Px * 0.62f, EFont::Bold, Px, C::Honey, 1);
		}
		AntUiCanvas::Text(Cv, GMode == Near ? TEXT("[I] вся квартира") : TEXT("[I] скрыть карту"), X0 + 2.f * S, Y0 + MH + 3.f * S, EFont::Bold, Px, WithA(C::Sugar, 0.6f), 0, 0.6f);
	}

	bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		if (A.Num() < 2 || A[0] != TEXT("minimap") || !GM)
		{
			return false;
		}
		if (FCString::IsNumeric(*A[1]))
		{
			GMode = FMath::Clamp(FCString::Atoi(*A[1]), 0, Count - 1);
		}
		UWorld* W = GM->GetWorld();
		APlayerController* PC = W->GetFirstPlayerController();
		TArray<FMapPlayer> Ps;
		Players(W, PC ? PC->PlayerState : nullptr, Ps);
		TArray<FAntTrailView> Trails;
		AntTrail::GetTrails(W, Trails);
		TArray<FVector> Sq;
		for (const FMapPlayer& P : Ps)
		{
			AntSquad::GetSquadPositions(P.PS, Sq);
			UE_LOG(LogTemp, Display, TEXT("ANTMAP info: mode %d | player %s%s at %s, colour %s, dead %d, squad dots %d, trails %d"), GMode, *P.PS->GetPlayerName(), P.bMe ? TEXT(" (me)") : TEXT(""),
				*P.Ant->GetActorLocation().ToCompactString(), *P.Col.ToFColor(true).ToHex(), P.Ant->bDead ? 1 : 0, Sq.Num(), Trails.FilterByPredicate([&P](const FAntTrailView& V) { return V.Owner == P.PS; }).Num());
		}
		UE_LOG(LogTemp, Display, TEXT("ANTMAP info: mode %d, %d players, %d trails"), GMode, Ps.Num(), Trails.Num());
		return true;
	}
}
