#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

/**
 * Level v2: the Human finds his way round the furniture. v1 let him walk in a straight line and bounce off whatever stood
 * there; the redesigned flat has a toy fortress, a train set, a coffee table with ramps and a cardboard box in his way.
 * A coarse grid over both rooms (200 uu cells): a cell is free when his capsule fits there, tested from just above his
 * step height up - so the things he steps over (slippers, single toy blocks, bottles, the toy truck: up to 245 uu) do not
 * block him and the things he cannot (a table, a box, a stack of blocks) do. A* over it, straightened by line of sight.
 * Server only, plain data: built once per world by the AI controller that owns it, from the static kitchen (WorldStatic).
 */
struct ANTKITCHEN_API FHumanNav
{
	static constexpr float Cell = 200.f;

	bool IsBuilt() const { return W > 0; }
	void Build(UWorld* World, const AActor* Ignore, float CapsuleRadius, float CapsuleHalfHeight, float StepHeight);

	/** The way From -> To as corner points (the last one is To, or the free cell nearest to it). False: no way at all. */
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const;
	/** Can he stand there? */
	bool IsFree(const FVector& P) const;
	/** The free cell centre nearest to P (within MaxCells rings), or P itself when it is free. False: none. */
	bool NearestFree(const FVector& P, FVector& Out, int32 MaxCells = 8) const;
	int32 NumFree() const;
	int32 NumCells() const { return W * H; }


private:
	int32 W = 0, H = 0;
	float X0 = 0.f, Y0 = 0.f;
	TArray<uint8> Blocked;

	bool CellOf(const FVector& P, int32& X, int32& Y) const;
	FVector Centre(int32 X, int32 Y) const { return FVector(X0 + (X + 0.5f) * Cell, Y0 + (Y + 0.5f) * Cell, 0.f); }
	bool Free(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < W && Y < H && !Blocked[Y * W + X]; }
	bool Clear(int32 XA, int32 YA, int32 XB, int32 YB) const;
	bool NearestFreeCell(int32& X, int32& Y, int32 MaxCells) const;
};

/** Walks a path of the grid: asks for a new one when the goal moves or the old one is stale. One per walker. */
struct ANTKITCHEN_API FHumanNavFollower
{
	TArray<FVector> Path;
	FVector Goal = FVector::ZeroVector;
	float At = -100.f;

	/** The point to walk towards right now on the way From -> To; To itself when there is no grid or no way. */
	FVector Next(const FHumanNav* Nav, const FVector& From, const FVector& To, float Now);
	void Reset() { Path.Reset(); }
};
