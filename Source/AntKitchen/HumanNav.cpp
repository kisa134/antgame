#include "HumanNav.h"
#include "KitchenBuilder.h"
#include "AntLab.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionQueryParams.h"
#include "Algo/Reverse.h"

void FHumanNav::Build(UWorld* World, const AActor* Ignore, float CapsuleRadius, float CapsuleHalfHeight, float StepHeight)
{
	// Both rooms: the kitchen x -7000..7000, y -5000..5000; the living room x 7200..20200, y -9000..1500.
	X0 = -7000.f;
	Y0 = -9000.f;
	W = FMath::CeilToInt((20200.f - X0) / Cell);
	H = FMath::CeilToInt((5000.f - Y0) / Cell);
	if (const FAntLabVariant* Lab = AntLab::Get())
	{
		// KAN-37: the lab flat's own bounds
		X0 = (float)Lab->Bounds.Min.X - 400.f;
		Y0 = (float)Lab->Bounds.Min.Y - 400.f;
		W = FMath::CeilToInt(((float)Lab->Bounds.Max.X + 400.f - X0) / Cell);
		H = FMath::CeilToInt(((float)Lab->Bounds.Max.Y + 400.f - Y0) / Cell);
	}
	Blocked.Init(1, W * H);
	if (!World)
	{
		return;
	}
	// His body from just above what he can step onto, up to his shoulders.
	const float Bottom = StepHeight + 12.f;
	const float Top = FMath::Max(Bottom + 2.f * CapsuleRadius + 10.f, 2.f * CapsuleHalfHeight - 150.f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleRadius - 15.f, (Top - Bottom) * 0.5f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanNav), false, Ignore);
	// The pieces only ants collide with (the yarn bridge, the garland...) are not in his way.
	for (TActorIterator<AKitchenBuilder> It(World); It; ++It)
	{
		TInlineComponentArray<UPrimitiveComponent*> Parts(*It);
		for (UPrimitiveComponent* C : Parts)
		{
			if (C && C->ComponentHasTag(TEXT("AntsOnly")))
			{
				Params.AddIgnoredComponent(C);
			}
		}
	}
	const FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			FVector C = Centre(X, Y);
			if (AKitchenBuilder::Region(C) > 1)
			{
				continue;   // the yard: not his
			}
			C.Z = (Bottom + Top) * 0.5f;
			Blocked[Y * W + X] = World->OverlapAnyTestByObjectType(C, FQuat::Identity, Obj, Shape, Params) ? 1 : 0;
		}
	}
}

int32 FHumanNav::NumFree() const
{
	int32 N = 0;
	for (uint8 B : Blocked) { N += B ? 0 : 1; }
	return N;
}

bool FHumanNav::CellOf(const FVector& P, int32& X, int32& Y) const
{
	X = FMath::FloorToInt((P.X - X0) / Cell);
	Y = FMath::FloorToInt((P.Y - Y0) / Cell);
	return X >= 0 && Y >= 0 && X < W && Y < H;
}

bool FHumanNav::IsFree(const FVector& P) const
{
	int32 X, Y;
	return CellOf(P, X, Y) && Free(X, Y);
}

bool FHumanNav::NearestFreeCell(int32& X, int32& Y, int32 MaxCells) const
{
	if (Free(X, Y))
	{
		return true;
	}
	int32 BX = 0, BY = 0, Best = MAX_int32;
	for (int32 R = 1; R <= MaxCells && Best == MAX_int32; ++R)
	{
		for (int32 DY = -R; DY <= R; ++DY)
		{
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Free(X + DX, Y + DY))
				{
					continue;
				}
				const int32 D = DX * DX + DY * DY;
				if (D < Best) { Best = D; BX = X + DX; BY = Y + DY; }
			}
		}
	}
	if (Best == MAX_int32)
	{
		return false;
	}
	X = BX;
	Y = BY;
	return true;
}

bool FHumanNav::NearestFree(const FVector& P, FVector& Out, int32 MaxCells) const
{
	int32 X, Y;
	CellOf(P, X, Y);
	X = FMath::Clamp(X, 0, W - 1);
	Y = FMath::Clamp(Y, 0, H - 1);
	const int32 OX = X, OY = Y;
	if (W <= 0 || !NearestFreeCell(X, Y, MaxCells))
	{
		return false;
	}
	Out = (X == OX && Y == OY && IsFree(P)) ? FVector(P.X, P.Y, 0.f) : Centre(X, Y);
	return true;
}

bool FHumanNav::Clear(int32 XA, int32 YA, int32 XB, int32 YB) const
{
	// Every cell the segment between the two centres passes through, and its side neighbours where it cuts a corner.
	const int32 N = FMath::Max(FMath::Abs(XB - XA), FMath::Abs(YB - YA)) * 3;
	for (int32 i = 0; i <= N; ++i)
	{
		const float T = N > 0 ? (float)i / N : 0.f;
		const float FX = XA + (XB - XA) * T, FY = YA + (YB - YA) * T;
		if (!Free(FMath::FloorToInt(FX + 0.5f - 0.3f), FMath::FloorToInt(FY + 0.5f - 0.3f)) || !Free(FMath::FloorToInt(FX + 0.5f + 0.3f), FMath::FloorToInt(FY + 0.5f + 0.3f))
			|| !Free(FMath::FloorToInt(FX + 0.5f - 0.3f), FMath::FloorToInt(FY + 0.5f + 0.3f)) || !Free(FMath::FloorToInt(FX + 0.5f + 0.3f), FMath::FloorToInt(FY + 0.5f - 0.3f)))
		{
			return false;
		}
	}
	return true;
}

bool FHumanNav::FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (W <= 0)
	{
		return false;
	}
	int32 SX, SY, GX, GY;
	CellOf(From, SX, SY);
	CellOf(To, GX, GY);
	SX = FMath::Clamp(SX, 0, W - 1); SY = FMath::Clamp(SY, 0, H - 1);
	GX = FMath::Clamp(GX, 0, W - 1); GY = FMath::Clamp(GY, 0, H - 1);
	const int32 WantGX = GX, WantGY = GY;
	if (!NearestFreeCell(SX, SY, 6) || !NearestFreeCell(GX, GY, 12))
	{
		return false;
	}
	const bool bGoalMoved = GX != WantGX || GY != WantGY;

	// A*, 8 neighbours, no cutting of corners.
	const int32 Num = W * H;
	TArray<float> Cost;
	Cost.Init(TNumericLimits<float>::Max(), Num);
	TArray<int32> Came;
	Came.Init(-1, Num);
	TArray<uint8> Closed;
	Closed.Init(0, Num);
	struct FNode { float F; int32 I; bool operator<(const FNode& O) const { return F < O.F; } };
	TArray<FNode> Open;
	auto Heur = [GX, GY](int32 X, int32 Y)
	{
		const float DX = FMath::Abs(X - GX), DY = FMath::Abs(Y - GY);
		return (DX + DY) + (1.41421f - 2.f) * FMath::Min(DX, DY);
	};
	const int32 Start = SY * W + SX, Goal = GY * W + GX;
	Cost[Start] = 0.f;
	Open.HeapPush({ Heur(SX, SY), Start });
	bool bFound = false;
	while (Open.Num() > 0)
	{
		FNode Cur;
		Open.HeapPop(Cur, EAllowShrinking::No);
		if (Closed[Cur.I]) { continue; }
		Closed[Cur.I] = 1;
		if (Cur.I == Goal) { bFound = true; break; }
		const int32 CX = Cur.I % W, CY = Cur.I / W;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX == 0 && DY == 0) || !Free(CX + DX, CY + DY)) { continue; }
				if (DX != 0 && DY != 0 && (!Free(CX + DX, CY) || !Free(CX, CY + DY))) { continue; }
				const int32 NI = (CY + DY) * W + (CX + DX);
				if (Closed[NI]) { continue; }
				const float NC = Cost[Cur.I] + ((DX != 0 && DY != 0) ? 1.41421f : 1.f);
				if (NC < Cost[NI])
				{
					Cost[NI] = NC;
					Came[NI] = Cur.I;
					Open.HeapPush({ NC + Heur(CX + DX, CY + DY), NI });
				}
			}
		}
	}
	if (!bFound)
	{
		return false;
	}
	TArray<int32> Cells;
	for (int32 I = Goal; I >= 0; I = Came[I])
	{
		Cells.Add(I);
	}
	Algo::Reverse(Cells);
	// Straighten: from each kept corner, skip to the furthest cell in plain sight.
	int32 At = 0;
	while (At < Cells.Num() - 1)
	{
		int32 Next = At + 1;
		for (int32 J = Cells.Num() - 1; J > At + 1; --J)
		{
			if (Clear(Cells[At] % W, Cells[At] / W, Cells[J] % W, Cells[J] / W)) { Next = J; break; }
		}
		OutPoints.Add(Centre(Cells[Next] % W, Cells[Next] / W));
		At = Next;
	}
	if (OutPoints.Num() == 0)
	{
		OutPoints.Add(Centre(GX, GY));
	}
	if (!bGoalMoved)
	{
		OutPoints.Last() = FVector(To.X, To.Y, 0.f);   // the goal itself, not the middle of its cell
	}
	return true;
}

FVector FHumanNavFollower::Next(const FHumanNav* Nav, const FVector& From, const FVector& To, float Now)
{
	if (!Nav || !Nav->IsBuilt())
	{
		return To;
	}
	if (Path.Num() == 0 || FVector::Dist2D(Goal, To) > 300.f || Now - At > 1.5f)
	{
		Goal = To;
		At = Now;
		if (!Nav->FindPath(From, To, Path))
		{
			Path.Reset();
		}
	}
	while (Path.Num() > 1 && FVector::Dist2D(From, Path[0]) < 260.f)
	{
		Path.RemoveAt(0);
	}
	return Path.Num() > 0 ? FVector(Path[0].X, Path[0].Y, From.Z) : To;
}
