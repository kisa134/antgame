#include "LevelV2.h"
#include "LevelV3.h"

#include "LevelV2Data.inl"

// Level v3 (-AntLevelV3) hands out its own tables through the same functions: every caller of LevelV2 gets the layout in use.

namespace LevelV2
{

TConstArrayView<FLevelV2Spot> LootSpots() { return LevelV3::Enabled() ? LevelV3::LootSpots() : TConstArrayView<FLevelV2Spot>(GV2Loot, UE_ARRAY_COUNT(GV2Loot)); }
TConstArrayView<FVector3f> HeavySpots() { return LevelV3::Enabled() ? LevelV3::HeavySpots() : TConstArrayView<FVector3f>(GV2Heavy, UE_ARRAY_COUNT(GV2Heavy)); }
TConstArrayView<FLevelV2Main> MainSpots() { return LevelV3::Enabled() ? LevelV3::MainSpots() : TConstArrayView<FLevelV2Main>(GV2Main, UE_ARRAY_COUNT(GV2Main)); }
TConstArrayView<FVector2f> PatrolStops() { return LevelV3::Enabled() ? LevelV3::PatrolStops() : TConstArrayView<FVector2f>(GV2Patrol, UE_ARRAY_COUNT(GV2Patrol)); }
TConstArrayView<FLevelV2Box> CoverBoxes() { return LevelV3::Enabled() ? LevelV3::CoverBoxes() : TConstArrayView<FLevelV2Box>(GV2Cover, UE_ARRAY_COUNT(GV2Cover)); }

FBox2D WanderBox(int32 Room)
{
	if (LevelV3::Enabled())
	{
		return LevelV3::WanderBox(Room);
	}
	const float* B = Room == 0 ? GV2WanderKitchen : GV2WanderLiving;
	return FBox2D(FVector2D(B[0], B[1]), FVector2D(B[2], B[3]));
}

namespace
{
	bool Inside(const FLevelV2Box& B, const FVector& P, float Slack)
	{
		return P.X >= B.X0 - Slack && P.X <= B.X1 + Slack && P.Y >= B.Y0 - Slack && P.Y <= B.Y1 + Slack && P.Z >= B.Z0 - Slack && P.Z <= B.Z1 + Slack;
	}
	TConstArrayView<FLevelV2Box> Bounds() { return LevelV3::Enabled() ? LevelV3::TargetBounds() : TConstArrayView<FLevelV2Box>(GV2TargetBounds, UE_ARRAY_COUNT(GV2TargetBounds)); }
	TConstArrayView<FLevelV2Zone> ZoneTable() { return LevelV3::Enabled() ? LevelV3::Zones() : TConstArrayView<FLevelV2Zone>(GV2Zones, UE_ARRAY_COUNT(GV2Zones)); }
	TConstArrayView<FLevelV2District> DistrictTable() { return LevelV3::Enabled() ? LevelV3::Districts() : TConstArrayView<FLevelV2District>(GV2Districts, UE_ARRAY_COUNT(GV2Districts)); }
	TConstArrayView<FLevelV2Callout> CalloutTable() { return LevelV3::Enabled() ? LevelV3::Callouts() : TConstArrayView<FLevelV2Callout>(GV2Callouts, UE_ARRAY_COUNT(GV2Callouts)); }
}

const FLevelV2Box* CoverAt(const FVector& P)
{
	// (the ant's centre rides ~5 uu above what it stands on: the boxes start at the floor, so a little slack below is enough)
	for (const FLevelV2Box& B : CoverBoxes())
	{
		if (P.X >= B.X0 && P.X <= B.X1 && P.Y >= B.Y0 && P.Y <= B.Y1 && P.Z >= B.Z0 - 6.f && P.Z <= B.Z1)
		{
			return &B;
		}
	}
	return nullptr;
}

bool InHouse(const FVector& P)
{
	for (const FLevelV2Box& B : Bounds())
	{
		if (Inside(B, P, 0.f))
		{
			return true;
		}
	}
	return false;
}

int32 ZoneAt(const FVector& P)
{
	for (const FLevelV2Zone& Z : ZoneTable())
	{
		if (Z.Kind > 1 || P.X < Z.X0 || P.X > Z.X1 || P.Y < Z.Y0 || P.Y > Z.Y1 || P.Z < Z.Z0 || P.Z > Z.Z1)
		{
			continue;   // (kind 2: the shards of v3, LevelV3::ShardsAt)
		}
		if (Z.Radius > 0.f && FVector2D::DistSquared(FVector2D(P), FVector2D((Z.X0 + Z.X1) * 0.5f, (Z.Y0 + Z.Y1) * 0.5f)) > Z.Radius * Z.Radius)
		{
			continue;
		}
		return Z.Kind;
	}
	return -1;
}

const TCHAR* DistrictAt(const FVector& P)
{
	for (const FLevelV2District& D : DistrictTable())
	{
		if (P.X >= D.X0 && P.X <= D.X1 && P.Y >= D.Y0 && P.Y <= D.Y1)
		{
			return D.Name;
		}
	}
	return nullptr;
}

const TCHAR* CalloutNear(const FVector& P, float Radius)
{
	const TCHAR* Best = nullptr;
	float BestD = Radius * Radius;
	for (const FLevelV2Callout& C : CalloutTable())
	{
		const float D = FVector2D::DistSquared(FVector2D(P), FVector2D(C.X, C.Y));
		if (D < BestD && FMath::Abs(P.Z - C.Z) < 900.f)
		{
			BestD = D;
			Best = C.Name;
		}
	}
	return Best;
}

float SprinklerLevel(double ServerTime)
{
	if (LevelV3::Enabled() && LevelV3::IsHazardOff(TEXT("sprinkler")))
	{
		return 0.f;   // v3, Red yard B: the sprinkler is off, the washing hangs
	}
	const float T = (float)FMath::Fmod(ServerTime, (double)SprinklerPeriod());
	const float Start = SprinklerPeriod() - SprinklerWet();
	if (T < Start)
	{
		return 0.f;
	}
	const float U = (T - Start) / SprinklerWet();
	return FMath::Clamp(FMath::Min(U, 1.f - U) * 8.f, 0.f, 1.f);
}

int32 MainValue() { return GV2MainValue; }
float MainWeightDesign() { return GV2MainWeightDesign; }

} // namespace LevelV2
