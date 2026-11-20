#pragma once

#include "CoreMinimal.h"

/**
 * Level v2 (20.11.2026): the gameplay tables of the redesigned flat, as data. Generated from docs/level/level_v2.json by
 * Tools/level/gen_level_v2_code.py into LevelV2Data.inl; the layout itself is Tools/level/level_v2_layout.py.
 * Static tables only: the same on every machine.
 */
struct FLevelV2Spot { float X, Y, Z; int32 Tier; };
struct FLevelV2Box { float X0, Y0, Z0, X1, Y1, Z1; const TCHAR* Name; };
struct FLevelV2Main { float X, Y, Z; int32 Chance; const TCHAR* Where; };
struct FLevelV2Zone { int32 Kind; float X0, Y0, Z0, X1, Y1, Z1, Radius; };
struct FLevelV2District { float X0, Y0, X1, Y1; const TCHAR* Name; };
struct FLevelV2Callout { float X, Y, Z; const TCHAR* Name; };

namespace LevelV2
{
	ANTKITCHEN_API TConstArrayView<FLevelV2Spot> LootSpots();
	ANTKITCHEN_API TConstArrayView<FVector3f> HeavySpots();
	ANTKITCHEN_API TConstArrayView<FLevelV2Main> MainSpots();
	ANTKITCHEN_API TConstArrayView<FVector2f> PatrolStops();
	ANTKITCHEN_API TConstArrayView<FLevelV2Box> CoverBoxes();
	/** The box the Human strolls in: 0 kitchen, 1 living room (x0, y0, x1, y1). */
	ANTKITCHEN_API FBox2D WanderBox(int32 Room);

	/** The cover table: P lies inside one of the designed hiding places (a slipper, a bottle, under a toy arch...). */
	ANTKITCHEN_API const FLevelV2Box* CoverAt(const FVector& P);
	/** Inside the house: the only place the Human takes targets from. */
	ANTKITCHEN_API bool InHouse(const FVector& P);
	/** New sticky / wet spots: 0 sticky, 1 wet, -1 neither. */
	ANTKITCHEN_API int32 ZoneAt(const FVector& P);
	/** The district the HUD names for this place ("Плато", "Свалка"...), or null. */
	ANTKITCHEN_API const TCHAR* DistrictAt(const FVector& P);
	/** The nearest callout within Radius (2D; within 900 in Z), or null. */
	ANTKITCHEN_API const TCHAR* CalloutNear(const FVector& P, float Radius = 900.f);

	// ---- the second way home (the south nest entrance at the house corner, reached through the cat flap and the yard)
	inline FVector SouthEntrance() { return FVector(-8150.f, -5750.f, 0.f); }
	inline float SouthEntranceRadius() { return 500.f; }

	// ---- the cat flap in the living room's west wall: only opened from the living-room side
	inline bool OnLatchSideOfCatFlap(const FVector& P) { return P.X > 7150.f; }

	// ---- the yard sprinkler: 9 s of water every 26 s over a circle of the lawn (world time: the same on every machine)
	inline FVector Sprinkler() { return FVector(-1500.f, -7300.f, 0.f); }
	inline float SprinklerRadius() { return 1400.f; }
	inline float SprinklerPeriod() { return 26.f; }
	inline float SprinklerWet() { return 9.f; }
	/** 0..1 while it sprays (eased at both ends), 0 when it is off. ServerTime: AGameStateBase::GetServerWorldTimeSeconds. */
	ANTKITCHEN_API float SprinklerLevel(double ServerTime);

	// ---- the main object
	ANTKITCHEN_API int32 MainValue();
	ANTKITCHEN_API float MainWeightDesign();
}