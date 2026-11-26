#include "LevelV2.h"
#include "LevelV3.h"

#include "LevelV2Data.inl"
#include "AntLab.h"

// Level v3 (-AntLevelV3) hands out its own tables through the same functions: every caller of LevelV2 gets the layout in use.
// KAN-37: so does a lab variant (AntLab.h): its food, heavy pieces, cake, the Human's round, cover, rooms and graph nodes.

namespace
{
	struct FLabTables
	{
		const FAntLabVariant* For = nullptr;
		TArray<FLevelV2Spot> Loot;
		TArray<FVector3f> Heavy;
		TArray<FLevelV2Main> Main;
		TArray<FVector2f> Patrol;
		TArray<FLevelV2Box> Cover;
		TArray<FLevelV2Box> Bounds;
		TArray<FLevelV2Zone> Zones;
		TArray<FLevelV2District> Districts;
		TArray<FLevelV2Callout> Callouts;
		TArray<FString> Names;   // the TCHAR* above point into these
	};
	FLabTables GLabTables;

	const FLabTables* Lab()
	{
		const FAntLabVariant* V = AntLab::Get();
		if (!V)
		{
			return nullptr;
		}
		FLabTables& T = GLabTables;
		if (T.For == V)
		{
			return &T;
		}
		T = FLabTables();
		T.For = V;
		T.Names.Reserve(V->Cover.Num() + V->Rooms.Num() + V->Nodes.Num() + 4);
		auto Name = [&T](const FString& S) { T.Names.Add(S); return *T.Names.Last(); };
		for (const FAntLabFood& F : V->Food)
		{
			if (F.bHeavy) { T.Heavy.Add(FVector3f(F.At)); }
			else { T.Loot.Add({ (float)F.At.X, (float)F.At.Y, (float)F.At.Z, F.Tier }); }
		}
		T.Main.Add({ (float)V->Cake.X, (float)V->Cake.Y, (float)V->Cake.Z, 100, Name(V->CakeWhere) });
		for (const FVector2D& P : V->Patrol) { T.Patrol.Add(FVector2f(P)); }
		for (int32 i = 0; i < V->Cover.Num(); ++i)
		{
			const FBox& B = V->Cover[i];
			T.Cover.Add({ (float)B.Min.X, (float)B.Min.Y, (float)B.Min.Z, (float)B.Max.X, (float)B.Max.Y, (float)B.Max.Z, Name(V->CoverNames[i]) });
		}
		T.Bounds.Add({ (float)V->Bounds.Min.X - 300.f, (float)V->Bounds.Min.Y - 300.f, (float)V->Origin.Z - 500.f, (float)V->Bounds.Max.X + 300.f, (float)V->Bounds.Max.Y + 300.f,
			(float)(V->Origin.Z + V->Ceiling + 200.f), Name(V->Name) });
		for (const FAntLabZone& Z : V->Zones)
		{
			const int32 Kind = Z.Kind == TEXT("sticky") ? 0 : Z.Kind == TEXT("wet") ? 1 : -1;
			if (Kind >= 0) { T.Zones.Add({ Kind, (float)Z.Box.Min.X, (float)Z.Box.Min.Y, (float)Z.Box.Min.Z, (float)Z.Box.Max.X, (float)Z.Box.Max.Y, (float)Z.Box.Max.Z, 0.f }); }
		}
		for (const FAntLabRoom& R : V->Rooms) { T.Districts.Add({ (float)R.Rect.Min.X, (float)R.Rect.Min.Y, (float)R.Rect.Max.X, (float)R.Rect.Max.Y, Name(R.Name) }); }
		for (const FAntLabNode& N : V->Nodes) { if (!N.bHole) { T.Callouts.Add({ (float)N.At.X, (float)N.At.Y, (float)N.At.Z, Name(N.Name) }); } }
		return &T;
	}
}

namespace LevelV2
{

TConstArrayView<FLevelV2Spot> LootSpots() { if (const FLabTables* L = Lab()) { return L->Loot; } return LevelV3::Enabled() ? LevelV3::LootSpots() : TConstArrayView<FLevelV2Spot>(GV2Loot, UE_ARRAY_COUNT(GV2Loot)); }
TConstArrayView<FVector3f> HeavySpots() { if (const FLabTables* L = Lab()) { return L->Heavy; } return LevelV3::Enabled() ? LevelV3::HeavySpots() : TConstArrayView<FVector3f>(GV2Heavy, UE_ARRAY_COUNT(GV2Heavy)); }
TConstArrayView<FLevelV2Main> MainSpots() { if (const FLabTables* L = Lab()) { return L->Main; } return LevelV3::Enabled() ? LevelV3::MainSpots() : TConstArrayView<FLevelV2Main>(GV2Main, UE_ARRAY_COUNT(GV2Main)); }
TConstArrayView<FVector2f> PatrolStops() { if (const FLabTables* L = Lab()) { return L->Patrol; } return LevelV3::Enabled() ? LevelV3::PatrolStops() : TConstArrayView<FVector2f>(GV2Patrol, UE_ARRAY_COUNT(GV2Patrol)); }
TConstArrayView<FLevelV2Box> CoverBoxes() { if (const FLabTables* L = Lab()) { return L->Cover; } return LevelV3::Enabled() ? LevelV3::CoverBoxes() : TConstArrayView<FLevelV2Box>(GV2Cover, UE_ARRAY_COUNT(GV2Cover)); }

FVector SouthEntrance()
{
	if (const FAntLabVariant* V = AntLab::Get()) { return V->Nest2; }   // the lab: the nest's second entrance
	return FVector(-8150.f, -5750.f, 0.f);
}
float SouthEntranceRadius()
{
	if (const FAntLabVariant* V = AntLab::Get()) { return V->NestRadius; }
	return 500.f;
}

FBox2D WanderBox(int32 Room)
{
	if (const FAntLabVariant* V = AntLab::Get())
	{
		return V->Bounds;
	}
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
	TConstArrayView<FLevelV2Box> Bounds() { if (const FLabTables* L = Lab()) { return L->Bounds; } return LevelV3::Enabled() ? LevelV3::TargetBounds() : TConstArrayView<FLevelV2Box>(GV2TargetBounds, UE_ARRAY_COUNT(GV2TargetBounds)); }
	TConstArrayView<FLevelV2Zone> ZoneTable() { if (const FLabTables* L = Lab()) { return L->Zones; } return LevelV3::Enabled() ? LevelV3::Zones() : TConstArrayView<FLevelV2Zone>(GV2Zones, UE_ARRAY_COUNT(GV2Zones)); }
	TConstArrayView<FLevelV2District> DistrictTable() { if (const FLabTables* L = Lab()) { return L->Districts; } return LevelV3::Enabled() ? LevelV3::Districts() : TConstArrayView<FLevelV2District>(GV2Districts, UE_ARRAY_COUNT(GV2Districts)); }
	TConstArrayView<FLevelV2Callout> CalloutTable() { if (const FLabTables* L = Lab()) { return L->Callouts; } return LevelV3::Enabled() ? LevelV3::Callouts() : TConstArrayView<FLevelV2Callout>(GV2Callouts, UE_ARRAY_COUNT(GV2Callouts)); }
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
