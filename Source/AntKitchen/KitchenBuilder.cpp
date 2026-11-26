#include "KitchenBuilder.h"
#include "AntGameState.h"
#include "AntFluidWorld.h"
#include "AntCharacter.h"
#include "KitchenInteractables.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "AntLab.h"
#include "KitchenFixture.h"
#include "Map4Look.h"
#include "AntLife.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/BoxComponent.h"
#include "AntWreck.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AntFx.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "AntFood.h"

namespace KitchenPalette
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }

	const FLinearColor TileA = Hex(225, 215, 195);
	const FLinearColor TileB = Hex(150, 60, 55);
	const FLinearColor WallC = Hex(240, 225, 185);
	const FLinearColor Ceiling = Hex(250, 250, 245);
	const FLinearColor Baseboard = Hex(120, 80, 50);
	const FLinearColor Backsplash = Hex(120, 170, 200);
	const FLinearColor Cabinet = Hex(130, 190, 170);
	const FLinearColor CabinetDoor = Hex(145, 205, 185);
	const FLinearColor CounterTop = Hex(60, 60, 68);
	const FLinearColor Steel = Hex(175, 180, 185);
	const FLinearColor DarkSteel = Hex(70, 72, 78);
	const FLinearColor Black = Hex(20, 20, 22);
	const FLinearColor Wood = Hex(160, 105, 60);
	const FLinearColor DarkWood = Hex(110, 70, 40);
	const FLinearColor White = Hex(245, 245, 245);
	const FLinearColor Dirt = Hex(95, 65, 40);
	const FLinearColor DarkDirt = Hex(60, 40, 25);
}
using namespace KitchenPalette;

AKitchenBuilder::AKitchenBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void AKitchenBuilder::BeginPlay()
{
	Super::BeginPlay();
	if (AntLab::Get())
	{
		bLab = true;
		BuildLab();   // KAN-37: the grey-box flat of the lab variant instead of the kitchen (KitchenBuilderLab.cpp)
		return;
	}
	Build();
	AntLife::Spawn(GetWorld());   // the living flat: the TV, the world outside, the small life of the rooms (AntLife.h)
}

// ---------------------------------------------------------------------------------------------
// Layout data shared with the GameMode (server) so loot sits on real surfaces.
// ---------------------------------------------------------------------------------------------

// Level v2: the fire ants' nest is the red anthill in yard B (SM_Y_AntHill), not a mound in the kitchen. The box-built kitchen keeps the old place.
FVector AKitchenBuilder::NestLocation(uint8 Team)
{
	if (const FAntLabVariant* Lab = AntLab::Get())
	{
		return Team == 0 ? Lab->Nest : Lab->Origin + FVector(0.f, -80000.f, 0.f);   // the lab has no red hill: far away and out of reach
	}
	if (Team == 0)
	{
		return FVector(-7900.f, -2500.f, 0.f);
	}
	return KitchenArt::Enabled() ? FVector(12800.f, -10700.f, 0.f) : FVector(6300.f, 1250.f, 0.f);
}

void AKitchenBuilder::HeavySpots(TArray<FVector>& Out)
{
	Out.Reset();
	if (KitchenArt::Enabled())
	{
		for (const FVector3f& P : LevelV2::HeavySpots())   // level v2: none closer than 25 s from the nest
		{
			Out.Add(FVector(P));
		}
		return;
	}
	Out.Add(FVector(0.f, -2600.f, 0.f));        // middle of the floor, between the nests
	Out.Add(FVector(1800.f, -3900.f, 0.f));     // under the window
	Out.Add(FVector(-1600.f, 1900.f, 0.f));     // by the stove
	Out.Add(FVector(3600.f, -400.f, 0.f));      // near the fire ants
	Out.Add(FVector(1000.f, -900.f, 1430.f));   // on the table: drop it down!
	Out.Add(FVector(-4000.f, -1600.f, 0.f));    // near our nest
}
float AKitchenBuilder::NestRadius(uint8 Team)
{
	if (const FAntLabVariant* Lab = AntLab::Get())
	{
		return Team == 0 ? Lab->NestRadius : 560.f;
	}
	return Team == 0 ? 950.f : 560.f;
}

FVector AKitchenBuilder::HomeFor(uint8 Team, const FVector& From)
{
	if (AntLab::Get() && Team == 0)
	{
		return AntLab::HomeFor(From);   // the lab: the nearer of the nest's two entrances
	}
	if (Team == 0 && KitchenArt::Enabled() && Region(From) == 2)
	{
		return LevelV2::SouthEntrance();
	}
	return NestLocation(Team);
}

// Level v2: the poop lies in yard A by a stepping stone, the bin stands at the east end of yard B.
FVector AKitchenBuilder::YardPoop() { return KitchenArt::Enabled() ? FVector(1300.f, -8300.f, 0.f) : FVector(-3000.f, -9500.f, 0.f); }
FVector AKitchenBuilder::YardBin() { return KitchenArt::Enabled() ? FVector(18600.f, -11000.f, 0.f) : FVector(4200.f, -13000.f, 0.f); }
FVector AKitchenBuilder::HumanSpawn() { if (const FAntLabVariant* Lab = AntLab::Get()) { return FVector(Lab->HumanSpawn, Lab->Origin.Z + 1720.f); } return FVector(3800.f, -3200.f, 1720.f); }
FVector AKitchenBuilder::HumanFleePoint() { if (AntLab::Get()) { return HumanSpawn(); } return FVector(5600.f, -4200.f, 1720.f); }
FBox AKitchenBuilder::HotBurner() { return FBox(FVector(-610.f, 3240.f, 1735.f), FVector(-90.f, 3760.f, 1900.f)); }
FBox AKitchenBuilder::PlayArea()
{
	if (AntLab::Get())
	{
		return AntLab::PlayArea();
	}
	const FBox2D B = LevelV2::WanderBox(0);   // level v2: the east end belongs to the rubbish corner now (x up to 5000, it was 6200)
	return FBox(FVector(B.Min.X, B.Min.Y, 0.f), FVector(KitchenArt::Enabled() ? B.Max.X : 6200.f, B.Max.Y, 0.f));
}

FVector AKitchenBuilder::RandomFloorPoint(FRandomStream* Rng)
{
	if (AntLab::Get())
	{
		return AntLab::RandomFloorPoint(Rng);
	}
	auto R = [Rng](float A, float B) { return Rng ? Rng->FRandRange(A, B) : FMath::FRandRange(A, B); };
	// ~60% kitchen, ~40% living room.
	if (R(0.f, 1.f) < 0.6f)
	{
		const FBox A = PlayArea();
		return FVector(R(A.Min.X, A.Max.X), R(A.Min.Y, A.Max.Y), 0.f);
	}
	if (!KitchenArt::Enabled())
	{
		return FVector(R(8000.f, 19000.f), R(-8300.f, -500.f), 0.f);
	}
	const FBox2D L = LevelV2::WanderBox(1);   // level v2: clear of the TV stand and the things along the south wall
	return FVector(R(L.Min.X, L.Max.X), R(L.Min.Y, L.Max.Y), 0.f);
}

int32 AKitchenBuilder::Region(const FVector& P)
{
	if (AntLab::Get())
	{
		return AntLab::InHouse(P) ? 0 : 2;   // the lab: the whole flat (loggia too) is one region; the graph does the routing
	}
	if (P.Y < -5150.f && (P.X < 7100.f || P.Y < -9150.f))
	{
		return 2;   // the yard: A under the kitchen window, B behind the living room (level v2)
	}
	if (P.X < -7050.f)
	{
		return 3;   // west lawn, sidewalk, street
	}
	return P.X < 7100.f ? 0 : 1;
}

FVector AKitchenBuilder::RandomYardPoint(FRandomStream* Rng)
{
	if (AntLab::Get())
	{
		return AntLab::RandomFloorPoint(Rng);
	}
	auto R = [Rng](float A, float B) { return Rng ? Rng->FRandRange(A, B) : FMath::FRandRange(A, B); };
	if (!KitchenArt::Enabled())
	{
		return FVector(R(-10500.f, 5500.f), R(-14500.f, -6000.f), 0.f);
	}
	// Level v2: the yard is two strips by the house. A (under the kitchen window) is a little bigger than B (behind the living room).
	if (R(0.f, 1.f) < 0.56f)
	{
		return FVector(R(-9200.f, 6800.f), R(-8600.f, -5500.f), 0.f);
	}
	return FVector(R(5400.f, 20200.f), R(-12000.f, -9400.f), 0.f);
}

namespace
{
	/** Walk a fixed chain of points (Fwd order, or reversed): the next point from where you are. */
	FVector Along(const FVector& From, const FVector* Fwd, int32 N, bool bReverse)
	{
		FVector Pts[8];
		N = FMath::Min(N, 8);
		for (int32 i = 0; i < N; ++i)
		{
			Pts[i] = bReverse ? Fwd[N - 1 - i] : Fwd[i];
		}
		// Which leg of the path are we on?
		int32 Best = -1;
		float BestD = 450.f;
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const FVector C = FMath::ClosestPointOnSegment(From, Pts[i], Pts[i + 1]);
			const float D = (float)FVector::Dist(C, From);
			if (D < BestD)
			{
				BestD = D;
				Best = i;
			}
		}
		if (Best < 0)
		{
			return Pts[0];
		}
		const int32 Next = Best + 1;
		if (FVector::Dist(From, Pts[Next]) < 110.f && Next + 1 < N)
		{
			return Pts[Next + 1];
		}
		return Pts[Next];
	}

	/** Kitchen -> back yard through the crack under the window (walkers). */
	FVector WindowPath(const FVector& From, bool bToYard)
	{
		static const FVector Out[] = { FVector(-4500.f, -4800.f, 0.f), FVector(-4500.f, -5400.f, 0.f), FVector(-4500.f, -5800.f, 0.f) };
		return Along(From, Out, UE_ARRAY_COUNT(Out), !bToYard);
	}
	/** Kitchen -> the nest outside through the crack in the west wall. */
	FVector CrackPath(const FVector& From, bool bToWest)
	{
		static const FVector Out[] = { FVector(-6700.f, -2500.f, 0.f), FVector(-7500.f, -2500.f, 0.f) };
		return Along(From, Out, UE_ARRAY_COUNT(Out), !bToWest);
	}
	/** West lawn -> back yard around the corner of the house. */
	FVector CornerPath(const FVector& From, bool bToYard)
	{
		static const FVector Out[] = { FVector(-8300.f, -4700.f, 0.f), FVector(-8300.f, -5700.f, 0.f) };
		return Along(From, Out, UE_ARRAY_COUNT(Out), !bToYard);
	}
	// ---- level v2
	/** Which strip of the yard: 0 = A (under the kitchen window), 1 = B (behind the living room, the red anthill). */
	int32 YardPart(const FVector& P) { return (P.Y < -9000.f && P.X > 5200.f) ? 1 : 0; }
	/** Yard A -> yard B through the gap at the corner of the living room. */
	FVector YardPassPath(const FVector& From, bool bToB)
	{
		static const FVector Out[] = { FVector(6100.f, -8400.f, 0.f), FVector(6100.f, -9000.f, 0.f), FVector(6100.f, -9600.f, 0.f) };
		return Along(From, Out, UE_ARRAY_COUNT(Out), !bToB);
	}
	/** Living room -> yard B through the red crack under the living-room window (the fire ants' way in). */
	FVector RedCrackPath(const FVector& From, bool bToYard)
	{
		static const FVector Out[] = { FVector(19500.f, -8700.f, 0.f), FVector(19500.f, -9100.f, 0.f), FVector(19500.f, -9600.f, 0.f) };
		return Along(From, Out, UE_ARRAY_COUNT(Out), !bToYard);
	}
}

FVector AKitchenBuilder::Route(const FVector& From, const FVector& To, bool bFlying)
{
	if (AntLab::Get())
	{
		return AntLab::Route(From, To);   // the lab: room to room over the variant's route graph
	}
	const int32 RF = Region(From), RT = Region(To);
	const bool bV2 = KitchenArt::Enabled();
	if (RF == RT)
	{
		// Level v2: the two strips of the yard meet only at the corner of the living room.
		if (bV2 && RF == 2 && YardPart(From) != YardPart(To))
		{
			if (bFlying)
			{
				const FVector Over(6100.f, -9000.f, 3200.f);
				return FVector::Dist2D(From, Over) < 500.f ? To : Over;
			}
			return YardPassPath(From, YardPart(To) == 1);
		}
		return To;
	}
	if (bV2 && !bFlying && ((RF == 1 && RT == 2 && YardPart(To) == 1) || (RF == 2 && RT == 1 && YardPart(From) == 1)))
	{
		return RedCrackPath(From, RT == 2);   // living room <-> yard B: the red crack
	}
	if (bV2 && bFlying && ((RF == 1 && RT == 2 && YardPart(To) == 1) || (RF == 2 && RT == 1 && YardPart(From) == 1)))
	{
		// Flyers: the living-room window (an opening without glass, like the kitchen one).
		const FVector W(18285.f, -9100.f, 3200.f);
		if (FVector::Dist(From, W) < 350.f)
		{
			return FVector(18285.f, RT == 2 ? -9800.f : -8300.f, 3200.f);
		}
		return W;
	}
	if (bV2 && RF == 2 && YardPart(From) == 1 && RT != 2)
	{
		return Route(From, FVector(6100.f, -8300.f, bFlying ? 3200.f : 0.f), bFlying);   // out of yard B first: everything else is reached from yard A
	}
	if (bFlying && (RF == 2 || RT == 2 || RF == 3 || RT == 3))
	{
		// Flyers use the window; from the west lawn they go round the corner first.
		if (RF == 3 || RT == 3)
		{
			const FVector C(-8300.f, -5600.f, 1500.f);
			if (FVector::Dist2D(From, C) < 400.f) { return RT == 3 ? FVector(-8300.f, -2500.f, 800.f) : FVector(-4000.f, -6500.f, 1500.f); }
			if (RF == 3 || RT == 3) { return C; }
		}
		if (RF == 1)
		{
			return Route(From, FVector(0.f, -4300.f, 1000.f), bFlying);
		}
		const FVector W(0.f, -5100.f, 3200.f);
		if (FVector::Dist(From, W) < 350.f)
		{
			return FVector(0.f, RT == 2 ? -5900.f : -4300.f, 3200.f);
		}
		return W;
	}
	if (RF == 3 || RT == 3)
	{
		// The west lawn: through the crack from the kitchen, round the corner from the yard.
		const int32 Other = RF == 3 ? RT : RF;
		if (Other == 1)
		{
			return Route(From, FVector(-6000.f, -2500.f, 0.f), bFlying);   // living room -> kitchen first
		}
		if (Other == 0)
		{
			return CrackPath(From, RT == 3);
		}
		return CornerPath(From, RT == 2);
	}
	if (RF == 2 || RT == 2)
	{
		// The back yard is reached through the crack under the kitchen window.
		if (RF == 1)
		{
			return Route(From, FVector(0.f, -4300.f, 0.f), bFlying);   // living room -> kitchen first
		}
		return WindowPath(From, RT == 2);
	}
	const bool bToKitchen = RT == 0;
	const bool bFromKitchen = RF == 0;
	const FVector D = Doorway();
	const float Z = FMath::Clamp((float)FMath::Min(From.Z, To.Z), 0.f, 2800.f);
	// At the door: step through to the other side. Otherwise head for our side of the door.
	// (650, not 600: "our side of the door" below is exactly 600 from it, and an army walks to that point exactly: it stood there for ever)
	if (FMath::Abs(From.Y - D.Y) < 700.f && FMath::Abs(From.X - D.X) < 650.f)
	{
		return FVector(bToKitchen ? 6200.f : 8000.f, D.Y, Z);
	}
	return FVector(bFromKitchen ? 6500.f : 7700.f, D.Y, Z);
}

void AKitchenBuilder::LootSpots(TArray<FVector>& L, TArray<int32>& T)
{
	L.Reset();
	T.Reset();
	const bool bModelled = KitchenArt::Enabled();
	if (bModelled)
	{
		// Level v2: the whole table is data (docs/level/level_v2.json -> LevelV2Data.inl): 327 spots, tiers by the gradient rule
		// (nothing dear close to the nest). The code below is the v1 table and only serves the box-built kitchen.
		for (const FLevelV2Spot& S : LevelV2::LootSpots())
		{
			L.Add(FVector(S.X, S.Y, S.Z + 35.f));
			T.Add(S.Tier);
		}
		return;
	}
	auto Add = [&L, &T, bModelled](float X, float Y, float SurfaceZ, int32 Tier)
	{
		if (bModelled)
		{
			// The modelled kitchen: a few spots moved off pieces that now stand there, or up onto their real surface.
			static const float Moved[][6] =
			{
				{ 4100.f, 4300.f, 930.f,    3750.f, 4420.f, 930.f },     // fridge shelf 1: the butter tub stands there now
				{ 3500.f, 3600.f, 1730.f,   3700.f, 3650.f, 1730.f },    // fridge shelf 2: clear of the plate of leftovers
				{ 3300.f, 4000.f, 2560.f,   3050.f, 4050.f, 2560.f },    // freezer: out of the ice cream tub
				{ 3800.f, 4300.f, 200.f,    3800.f, 4300.f, 240.f },     // fridge floor: inside the crisper tray
				{ 800.f, 400.f, 1400.f,     600.f, 200.f, 1400.f },      // table: in the jam puddle, not inside the jar
				{ 650.f, 3900.f, 1070.f,    900.f, 4100.f, 1085.f },     // dishwasher upper rack: in the bowl (it rolls out with the rack)
				{ 1600.f, -500.f, 1900.f,   1740.f, -360.f, 1906.f },    // top of the cake: beside the cherry in the middle
				{ -750.f, 4880.f, 2770.f,   -750.f, 4900.f, 2775.f },    // on the utensil rail
				{ -3800.f, -3800.f, 170.f,  -3800.f, -3800.f, 262.f },   // on top of the modelled kibble heap
				{ -3600.f, -3650.f, 170.f,  -3600.f, -3650.f, 232.f },
				{ 19700.f, -6500.f, 4600.f, 19700.f, -6500.f, 4660.f },  // bookshelf: on its top board, not inside it
				{ 19700.f, -3500.f, 4600.f, 19700.f, -3500.f, 4660.f },
				{ 19700.f, -5000.f, 2800.f, 19380.f, -5000.f, 2760.f },  // bookshelf shelves: the free strip in front of the books
				{ 19700.f, -4000.f, 1900.f, 19380.f, -4000.f, 1860.f },
				{ 19700.f, -6000.f, 1000.f, 19380.f, -6000.f, 960.f },
				{ 13700.f, -3400.f, 340.f,  13700.f, -3400.f, 402.f },   // coffee table shelf: on the magazines
				{ 3300.f, -14100.f, 0.f,    3500.f, -14100.f, 0.f },     // beside the apple core
				{ 4200.f, -13000.f, 2700.f, 4200.f, -13000.f, 2790.f },  // on the bags in the yard bin
				{ -7100.f, -12500.f, 5200.f, -7100.f, -12500.f, 5460.f }, // on the tree's branch
				{ -10500.f, 800.f, 0.f,     -10500.f, 600.f, 0.f },      // off the joint between two pavement slabs
			};
			for (const auto& M : Moved)
			{
				if (X == M[0] && Y == M[1] && SurfaceZ == M[2])
				{
					X = M[3]; Y = M[4]; SurfaceZ = M[5];
					break;
				}
			}
		}
		L.Add(FVector(X, Y, SurfaceZ + 35.f));
		T.Add(Tier);
	};

	// Floor crumbs, scattered with a fixed seed so every run looks the same.
	FRandomStream R(1337);
	int32 Placed = 0;
	for (int32 Guard = 0; Placed < 36 && Guard < 500; ++Guard)
	{
		const FVector P(R.FRandRange(-6000.f, 6500.f), R.FRandRange(-4600.f, 2700.f), 0.f);
		if (FVector::Dist2D(P, NestLocation(0)) < 1100.f) continue;
		if (FVector::Dist2D(P, NestLocation(1)) < 1000.f) continue;
		if (FVector::Dist2D(P, FVector(6000.f, -1500.f, 0.f)) < 800.f) continue;   // trash can
		if (FVector::Dist2D(P, FVector(6000.f, -2700.f, 0.f)) < 750.f) continue;   // trash lid
		if (FVector::Dist2D(P, FVector(6200.f, 3900.f, 0.f)) < 550.f) continue;    // bucket
		if (FVector::Dist2D(P, FVector(-3800.f, -3800.f, 0.f)) < 560.f) continue;  // cat bowl
		if (FVector::Dist2D(P, FVector(-2500.f, -3900.f, 0.f)) < 500.f) continue;  // water bowl
		if (P.X > -5850.f && P.X < -4550.f && P.Y > 450.f && P.Y < 1350.f) continue; // cereal box
		Add(P.X, P.Y, 0.f, 0);
		++Placed;
	}

	// Cereal spill
	Add(-4400.f, 700.f, 0.f, 0); Add(-4300.f, 1200.f, 0.f, 1); Add(-4450.f, 300.f, 0.f, 0);
	Add(-5200.f, 900.f, 420.f, 1); // on top of the box

	// Counters
	Add(-6000.f, 3500.f, 1710.f, 1);
	Add(-5000.f, 3600.f, 1760.f, 1);   // cutting board
	Add(-6250.f, 4200.f, 2150.f, 2);   // toaster top
	Add(-2200.f, 3400.f, 1710.f, 1);
	Add(-2250.f, 4400.f, 2040.f, 2);   // on the cereal box on the counter
	Add(600.f, 3300.f, 1710.f, 1);
	Add(1800.f, 3400.f, 1710.f, 1);
	Add(1850.f, 4450.f, 2350.f, 2);    // microwave top
	Add(1760.f, 4430.f, 1770.f, 3);    // inside the microwave: the plate of leftovers
	Add(300.f, 3600.f, 420.f, 1);  Add(900.f, 4200.f, 420.f, 2);  Add(650.f, 3900.f, 1070.f, 2);   // dishwasher racks
	Add(-2150.f, 4880.f, 1920.f, 1); Add(-2050.f, 4880.f, 2520.f, 2);                               // spice rack shelves
	Add(-750.f, 4880.f, 2770.f, 2);                                                                   // on the utensil rail
	Add(800.f, 4600.f, 2210.f, 2);     // sugar jar lid
	Add(-750.f, 4000.f, 1740.f, 2);    // stove, between the burners
	Add(-3500.f, 3900.f, 1150.f, 2);   // sink bottom
	Add(-3100.f, 4300.f, 1150.f, 1);
	Add(-4500.f, 4600.f, 4400.f, 3);   // on top of the upper cabinets
	Add(-6200.f, 4650.f, 4400.f, 3);

	// Fridge top
	Add(3700.f, 4000.f, 3700.f, 3);
	Add(3200.f, 3600.f, 3700.f, 3);

	// Table
	Add(0.f, -800.f, 1430.f, 2);       // plate
	Add(1600.f, -500.f, 1900.f, 3);    // top of the cake
	Add(-1000.f, -300.f, 1400.f, 2);
	Add(-1300.f, -1500.f, 1400.f, 1);
	Add(2400.f, -1500.f, 1400.f, 1);
	Add(800.f, 400.f, 1400.f, 2);
	Add(-400.f, 300.f, 1400.f, 1);

	// Chairs
	Add(0.f, -2750.f, 880.f, 1);
	Add(1200.f, 1650.f, 880.f, 1);

	// Trash corner
	Add(5900.f, -1400.f, 900.f, 3);    // inside the pedal bin, on the bag
	Add(6200.f, -1650.f, 900.f, 2);
	Add(5800.f, -1700.f, 60.f, 2);     // on the bin floor, next to the bag
	Add(5200.f, -300.f, 0.f, 1);
	Add(5000.f, -900.f, 0.f, 1);
	Add(6000.f, -2700.f, 0.f, 1);
	Add(6600.f, -500.f, 0.f, 1);
	Add(4800.f, -1800.f, 0.f, 1);

	// Cat bowls
	Add(-3800.f, -3800.f, 170.f, 1);
	Add(-3600.f, -3650.f, 170.f, 1);
	Add(-2500.f, -3900.f, 140.f, 1);

	// Wall shelf (climb the west wall!)
	Add(-6780.f, -650.f, 2680.f, 3);
	Add(-6780.f, 800.f, 2680.f, 2);

	// Window sill
	Add(-800.f, -4910.f, 2200.f, 2);
	Add(900.f, -4910.f, 2200.f, 2);

	// Mop bucket
	Add(6200.f, 3900.f, 650.f, 2);

	// ---- Secret places: better loot for those who explore ----
	Add(-5500.f, 900.f, 20.f, 1);  Add(-5150.f, 1120.f, 20.f, 2);          // inside the cereal box
	Add(-3700.f, 4300.f, 140.f, 2); Add(-3150.f, 3450.f, 160.f, 2);       // under the sink (+ mouse trap bait)
	Add(-750.f, 3900.f, 915.f, 3);  Add(-400.f, 3500.f, 312.f, 2);        // oven: tray and floor
	Add(3200.f, 3900.f, 930.f, 2);  Add(4100.f, 4300.f, 930.f, 2);        // fridge shelves
	Add(3500.f, 3600.f, 1730.f, 3); Add(3800.f, 4300.f, 200.f, 2);
	Add(3300.f, 4000.f, 2560.f, 3); Add(4200.f, 3500.f, 2560.f, 3);       // freezer
	Add(3700.f, 3600.f, 0.f, 2);    Add(3200.f, 4300.f, 0.f, 1);          // under the fridge
	Add(-6800.f, 4870.f, 0.f, 3);   Add(-1000.f, 4870.f, 0.f, 1);         // corridor behind the cabinets
	Add(1500.f, 4870.f, 0.f, 2);    Add(2600.f, 4300.f, 0.f, 1);
	Add(3700.f, 5300.f, 0.f, 3);    Add(4100.f, 5400.f, 0.f, 3);          // the mouse's room

	// ---- Living room ----
	FRandomStream R2(4242);
	for (int32 i = 0; i < 16; ++i)   // crumbs on the wooden floor and the rug
	{
		Add(R2.FRandRange(8000.f, 19000.f), R2.FRandRange(-8300.f, -700.f), 0.f, i % 5 == 0 ? 1 : 0);
	}
	Add(11500.f, 100.f, 950.f, 2);  Add(13500.f, 200.f, 950.f, 1);  Add(15500.f, 100.f, 950.f, 2);   // sofa cushions
	Add(12000.f, 900.f, 0.f, 3);    Add(15000.f, 800.f, 0.f, 3);    Add(13600.f, 700.f, 0.f, 2);     // under the sofa: lost treasure
	Add(13000.f, -3800.f, 980.f, 2); Add(14500.f, -3000.f, 980.f, 2);                                // coffee table
	Add(13700.f, -3400.f, 340.f, 1);                                                                  // coffee table shelf
	Add(12200.f, -8300.f, 1100.f, 2); Add(15200.f, -8300.f, 1100.f, 2);                              // TV stand top
	Add(13700.f, -8400.f, 120.f, 3);                                                                  // inside the TV stand
	Add(19700.f, -6500.f, 4600.f, 3); Add(19700.f, -3500.f, 4600.f, 3);                              // top of the bookshelf
	Add(19700.f, -5000.f, 2800.f, 2); Add(19700.f, -4000.f, 1900.f, 2); Add(19700.f, -6000.f, 1000.f, 1);
	Add(9250.f, -5750.f, 800.f, 2);                                                                   // armchair seat
	Add(18800.f, 900.f, 800.f, 1);                                                                    // plant pot soil

	// ---- Back yard ----
	Add(-6600.f, -7800.f, 0.f, 1);  Add(-5200.f, -10800.f, 0.f, 1);  Add(-1200.f, -8600.f, 0.f, 0);  Add(2400.f, -11800.f, 0.f, 1);
	Add(2900.f, -12700.f, 30.f, 2);  Add(2500.f, -12500.f, 0.f, 2);   Add(3300.f, -14100.f, 0.f, 1);   // spilled by the bin
	Add(4200.f, -13000.f, 2700.f, 3);                                                                  // inside the bin, on the bag
	Add(-8400.f, -12500.f, 160.f, 1); Add(-7100.f, -12500.f, 5200.f, 3);                              // under the tree / on the branch
	Add(-1000.f, -5300.f, 2200.f, 2);                                                                  // outer window sill
	// ---- West lawn, sidewalk, street ----
	Add(-8900.f, 1500.f, 0.f, 1);  Add(-8200.f, 3900.f, 0.f, 1);  Add(-8800.f, -4200.f, 0.f, 0);
	Add(-10400.f, -1500.f, 260.f, 3); Add(-10100.f, -1300.f, 120.f, 2);                               // the ice cream
	Add(-9900.f, 4000.f, 0.f, 1);  Add(-10500.f, 800.f, 0.f, 1);                                      // sidewalk
	Add(-13500.f, 0.f, -230.f, 2); Add(-12300.f, 2600.f, -220.f, 2);                                  // under the car, on the manhole
	Add(-7350.f, 0.f, 8960.f, 3);  Add(-7350.f, -4200.f, 8960.f, 3);                                  // the roof gutter: climb the downspout
	// ---- Inside the house: new hiding places ----
	Add(5000.f, 2000.f, -400.f, 3); Add(4850.f, 2150.f, -400.f, 2);                                   // the pit under the loose tile
	Add(-3100.f, -5100.f, 100.f, 1); Add(-3100.f, 1500.f, 5420.f, 3); Add(5200.f, 400.f, 5420.f, 3); // in the air ducts
	Add(-1850.f, 2600.f, 10.f, 2);                                                                    // the matchbox
	Add(1200.f, -4300.f, 0.f, 1);  Add(6550.f, -4150.f, 320.f, 2);                                    // under the newspaper, in the shoe
	Add(9000.f, -2000.f, 0.f, 2);  Add(13800.f, -8400.f, 80.f, 2);                                    // the sock, behind the TV
	Add(9250.f, -4800.f, 1245.f, 2); Add(13500.f, -700.f, 0.f, 1); Add(13900.f, -500.f, 0.f, 1);     // grandma's cookie, under the kid's feet
}

// ---------------------------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------------------------

namespace
{
	// Real surfaces for the procedural kitchen: textured, world-projected (M_Surface).
	struct FSurf { int32 Mode = -1; const TCHAR* Tex = nullptr; float Size = 400.f; float Tint = 1.f; float Rough = 0.6f; float Metal = 0.f; bool bUseColor = false; };

	FSurf SurfaceFor(const FLinearColor& C)
	{
		auto Is = [&C](const FLinearColor& K) { return C.Equals(K, 0.002f); };
		FSurf S;
		if (Is(TileA) || Is(TileB)) { S.Mode = 0; S.Tex = TEXT("T_Floor_Tile"); S.Size = 1400.f; S.Tint = Is(TileA) ? 1.f : 0.93f; S.Rough = 0.24f; }   // crisp satin, not a mirror (the mess must read against it): from an ant's eye every tile is seen at a grazing angle
		else if (Is(WallC)) { S.Mode = 4; S.bUseColor = true; S.Rough = 0.9f; }
		else if (Is(Ceiling)) { S.Mode = 0; S.Tex = TEXT("T_Ceiling"); S.Size = 900.f; S.Rough = 0.95f; }
		else if (Is(Backsplash)) { S.Mode = 0; S.Tex = TEXT("T_Subway_Tile"); S.Size = 700.f; S.Rough = 0.12f; }
		else if (Is(CounterTop)) { S.Mode = 0; S.Tex = TEXT("T_Granite"); S.Size = 800.f; S.Rough = 0.2f; }
		else if (Is(Cabinet)) { S.Mode = 0; S.Tex = TEXT("T_Cabinet_Sage"); S.Size = 900.f; S.Tint = 0.92f; S.Rough = 0.55f; }
		else if (Is(CabinetDoor)) { S.Mode = 0; S.Tex = TEXT("T_Cabinet_Sage"); S.Size = 900.f; S.Rough = 0.5f; }
		else if (Is(Wood)) { S.Mode = 0; S.Tex = TEXT("T_Wood_Oak"); S.Size = 1100.f; S.Rough = 0.5f; }
		else if (Is(DarkWood)) { S.Mode = 0; S.Tex = TEXT("T_Wood_Walnut"); S.Size = 900.f; S.Rough = 0.45f; }
		else if (Is(Baseboard)) { S.Mode = 0; S.Tex = TEXT("T_Wood_Oak"); S.Size = 700.f; S.Tint = 0.65f; S.Rough = 0.5f; }
		else if (Is(Steel)) { S.Mode = 0; S.Tex = TEXT("T_Steel_Brushed"); S.Size = 500.f; S.Rough = 0.32f; S.Metal = 0.9f; }
		else if (Is(DarkSteel)) { S.Mode = 0; S.Tex = TEXT("T_Steel_Brushed"); S.Size = 500.f; S.Tint = 0.4f; S.Rough = 0.38f; S.Metal = 0.85f; }
		else if (Is(Dirt)) { S.Mode = 0; S.Tex = TEXT("T_Soil"); S.Size = 700.f; S.Rough = 0.95f; }
		else if (Is(DarkDirt)) { S.Mode = 0; S.Tex = TEXT("T_Soil"); S.Size = 700.f; S.Tint = 0.6f; S.Rough = 0.95f; }
		else if (Is(Hex(170, 40, 50))) { S.Mode = 0; S.Tex = TEXT("T_Rug_Red"); S.Size = 900.f; S.Rough = 0.95f; }
		else if (Is(White)) { S.Mode = 0; S.Tex = TEXT("T_Enamel_White"); S.Size = 600.f; S.Rough = 0.2f; }
		else if (Is(Hex(70, 92, 125)) || Is(Hex(52, 70, 98))) { S.Mode = 0; S.Tex = TEXT("T_Fabric_Grey"); S.Size = 300.f; S.bUseColor = true; S.Rough = 0.95f; }   // sofa fabric
		else if (Is(Hex(196, 152, 104))) { S.Mode = 0; S.Tex = TEXT("T_Parquet_Oak"); S.Size = 1600.f; S.Rough = 0.45f; }
		else if (Is(Hex(205, 160, 105))) { S.Mode = 0; S.Tex = TEXT("T_Wood_Oak"); S.Size = 600.f; S.Tint = 1.15f; S.Rough = 0.6f; }
		return S;
	}

	/** What an ant's feet hear on this color (see AntSfx::StepFor). None = hard: tile, stone, enamel, glass, plaster. */
	FName SurfaceTag(const FLinearColor& C)
	{
		auto Is = [&C](const FLinearColor& K) { return C.Equals(K, 0.002f); };
		if (Is(Wood) || Is(DarkWood) || Is(Baseboard) || Is(Cabinet) || Is(CabinetDoor) || Is(Hex(196, 152, 104)) || Is(Hex(205, 160, 105)) || Is(Hex(160, 120, 78)) || Is(Hex(95, 68, 45)))
		{
			return TEXT("Surf_Wood");
		}
		if (Is(Steel) || Is(DarkSteel) || Is(Hex(40, 92, 55)) || Is(Hex(30, 70, 42)))
		{
			return TEXT("Surf_Metal");
		}
		if (Is(Hex(170, 40, 50)) || Is(Hex(70, 92, 125)) || Is(Hex(52, 70, 98)) || Is(Hex(230, 180, 70)))
		{
			return TEXT("Surf_Fabric");
		}
		if (Is(Dirt) || Is(DarkDirt) || Is(Hex(110, 80, 50)))
		{
			return TEXT("Surf_Soil");
		}
		if (Is(Hex(86, 142, 60)) || Is(Hex(52, 104, 38)) || Is(Hex(70, 120, 50)) || Is(Hex(60, 120, 50)))
		{
			return TEXT("Surf_Grass");
		}
		return NAME_None;
	}

	void ApplySurface(UStaticMeshComponent* C, const FLinearColor& Color)
	{
		if (C)
		{
			const FName Tag = SurfaceTag(Color);
			if (!Tag.IsNone())
			{
				C->ComponentTags.AddUnique(Tag);
			}
		}
		if (C && Map4Look::Enabled())
		{
			// Map v4: the palette colour picks a real surface (wood, tile, wallpaper of this room, fabric...), shared material
			FLinearColor Tint;
			if (const Map4Look::FLook* L = Map4Look::ForColor(Color, C->Bounds.Origin, Tint))
			{
				if (UMaterialInterface* M = Map4Look::Get(*L, Tint))
				{
					C->SetMaterial(0, M);
					if (L->Step) { C->ComponentTags.AddUnique(FName(L->Step)); }
					return;
				}
			}
		}
		static UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Textures/M_Surface.M_Surface"));
		if (Base && !Base->IsRooted()) { Base->AddToRoot(); }   // static: must survive map travel
		if (!Base || !C)
		{
			return;
		}
		const FSurf S = SurfaceFor(Color);
		if (S.Mode < 0)
		{
			return;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, C);
		M->SetScalarParameterValue(TEXT("Mode"), (float)S.Mode);
		M->SetScalarParameterValue(TEXT("TileSize"), S.Size);
		M->SetScalarParameterValue(TEXT("Roughness"), S.Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), S.Metal);
		M->SetVectorParameterValue(TEXT("Tint"), S.bUseColor ? (S.Tex ? Color * 2.2f : Color) : FLinearColor(S.Tint, S.Tint, S.Tint, 1.f));
		if (S.Tex)
		{
			static TMap<FString, UTexture*> Cache;
			UTexture*& T = Cache.FindOrAdd(S.Tex);
			if (!T)
			{
				T = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Textures/%s.%s"), S.Tex, S.Tex));
				if (T) { T->AddToRoot(); }   // cached across map travel
			}
			if (T)
			{
				M->SetTextureParameterValue(TEXT("Tex"), T);
			}
			else
			{
				M->SetScalarParameterValue(TEXT("Mode"), 4.f);
				M->SetVectorParameterValue(TEXT("Tint"), Color);
			}
		}
		C->SetMaterial(0, M);
	}
}

void AKitchenBuilder::Surface(UStaticMeshComponent* C, const FLinearColor& Color)
{
	ApplySurface(C, Color);
}

UStaticMeshComponent* AKitchenBuilder::Part(EAntShape Shape, const FVector& Center, const FVector& Size, const FRotator& Rot, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* C = AntKit::AddPart(this, Root, Shape, Center, Size / 100.f, Color, Rot, bCollide, true);
	ApplySurface(C, Color);
	if (C && Map4Look::Enabled())
	{
		// map v4: what the legacy builder pieces are by their proportions
		FVector S = Size.GetAbs();
		float Lo = S.GetMin(), Hi = S.GetMax(), Mid = S.X + S.Y + S.Z - Lo - Hi;
		if (Shape == EAntShape::Cube && (Lo <= 45.f || Lo <= 0.2f * Mid))
		{
			C->ComponentTags.AddUnique(TEXT("M4Arch"));   // boards, sheets, slats, thresholds: flat by nature
		}
		else if (Shape == EAntShape::Cylinder && Size.Z <= 0.1f * FMath::Max(Size.X, Size.Y))
		{
			Map4Look::Swap(C, TEXT("SM_M4_LiquidDisc"), nullptr, TEXT("M4Fx"));   // a flat disc: a spill, a mark, a plate's shadow
			ApplySurface(C, Color);
		}
		else if (Shape == EAntShape::Cylinder && Size.Z > 2.f * FMath::Max(Size.X, Size.Y) && Color.Equals(Hex(250, 245, 230), 0.01f))
		{
			Map4Look::Swap(C, TEXT("SM_M4_Candle"));   // (two slots: Bone wax, Plinth wick)
		}
		else if (Shape == EAntShape::Cylinder && Size.Z < Size.X && Color.Equals(Hex(190, 190, 180), 0.01f))
		{
			Map4Look::Swap(C, TEXT("SM_M4_Drain"));   // the mouse's bottle-cap table: a metal crown cap
		}
	}
	return C;
}

UStaticMeshComponent* AKitchenBuilder::Box(const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* C = Part(EAntShape::Cube, (Min + Max) * 0.5f, Max - Min, FRotator::ZeroRotator, Color, bCollide);
	if (Map4Look::Enabled() && Color.Equals(WallC, 0.002f))
	{
		WallSkins(Min, Max);
	}
	if (C && (Color.Equals(WallC, 0.002f) || Color.Equals(Ceiling, 0.002f) || Color.Equals(Hex(225, 215, 195), 0.002f) || Color.Equals(Hex(196, 152, 104), 0.002f)
		|| Color.Equals(Hex(120, 95, 70), 0.002f) || Color.Equals(DarkDirt, 0.002f) || Color.Equals(Hex(150, 152, 150), 0.002f) || Color.Equals(TileB, 0.002f)))
	{
		C->ComponentTags.AddUnique(TEXT("M4Arch"));   // the shell: walls, ceilings, floors, the pit, ducts (slabs by nature)
		if (UStaticMesh* Sharp = Map4Look::Prop(TEXT("SM_M4_BoxSharp"))) { C->SetStaticMesh(Sharp); }   // (a bevel scaled to a wall drew a groove at every joint)
	}
	return C;
}

// Map v4: a wall box shows one material on all its faces. Each vertical face that looks into another room, out of the
// house or into a window / door reveal gets a paper-thin skin (no collision) with that side's surface: the living room's
// wallpaper on the kitchen wall's far side, the facade outside, plaster in the reveals.
void AKitchenBuilder::WallSkins(const FVector& Min, const FVector& Max)
{
	auto Room = [](const FVector& P, float Grow) { return Map4Look::RoomAt(P, Grow); };
	const FVector Ctr = (Min + Max) * 0.5f;
	const int32 Own = Map4Look::WallRoom(Ctr);
	const FVector Size = Max - Min;
	static const FVector Dirs[4] = { FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
	for (const FVector& D : Dirs)
	{
		const bool bX = D.X != 0.f;
		const float Wide = bX ? Size.Y : Size.X;
		if (Wide < 20.f || Size.Z < 20.f) { continue; }
		const float Face = bX ? (D.X > 0 ? Max.X : Min.X) : (D.Y > 0 ? Max.Y : Min.Y);
		FVector Probe = Ctr;
		if (bX) { Probe.X = Face + D.X * 12.f; } else { Probe.Y = Face + D.Y * 12.f; }
		const int32 Side = Room(Probe, 0.f);
		const bool bReveal = Side == 0 && Room(Probe, 230.f) != 0;
		const TCHAR* Look = Side == 2 ? TEXT("WallL") : Side == 1 ? TEXT("WallK") : bReveal ? TEXT("Plaster") : TEXT("WallOut");
		const int32 Want = Side;
		if (Want == Own && Side != 0) { continue; }   // the box itself already shows this room's paper
		const Map4Look::FLook* L = Map4Look::Named(Look);
		if (!L) { continue; }
		FVector SMin = Min, SMax = Max;
		if (bX) { SMin.X = SMax.X = Face + D.X * 0.6f; SMin.X -= 0.4f; SMax.X += 0.4f; }
		else { SMin.Y = SMax.Y = Face + D.Y * 0.6f; SMin.Y -= 0.4f; SMax.Y += 0.4f; }
		UStaticMeshComponent* S = Part(EAntShape::Cube, (SMin + SMax) * 0.5f, SMax - SMin, FRotator::ZeroRotator, Hex(1, 2, 3), false);
		if (S)
		{
			S->SetMaterial(0, Map4Look::Get(*L));
			S->SetCastShadow(false);
			S->ComponentTags.Add(TEXT("M4Skin"));
			if (UStaticMesh* Sharp = Map4Look::Prop(TEXT("SM_M4_BoxSharp"))) { S->SetStaticMesh(Sharp); }
		}
	}
}

UStaticMeshComponent* AKitchenBuilder::Cyl(const FVector& Base, float Radius, float Height, const FLinearColor& Color, bool bCollide)
{
	return Part(EAntShape::Cylinder, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(Radius * 2.f, Radius * 2.f, Height), FRotator::ZeroRotator, Color, bCollide);
}

UStaticMeshComponent* AKitchenBuilder::Ball(const FVector& Center, const FVector& Radii, const FLinearColor& Color)
{
	// Big blobs (mounds, pillows, bags) collide as a sphere of the smallest radius; crumbs and dust don't.
	UStaticMeshComponent* C = Part(EAntShape::Sphere, Center, Radii * 2.f, FRotator::ZeroRotator, Color, Radii.GetMin() >= 100.f);
	if (C && Map4Look::Enabled())
	{
		// map v4: what the blob is: a bag, a heap of earth, a dust bunny, a stain or a crumb
		const FLinearColor H = Color.LinearRGBToHSV();
		const float Lum = Color.GetLuminance();
		const bool bFlat = Radii.Z < 0.35f * FMath::Min(Radii.X, Radii.Y);
		const bool bEarth = Color.Equals(Dirt, 0.01f) || Color.Equals(DarkDirt, 0.01f) || (H.R < 40.f && H.G > 0.35f && H.B < 0.55f && Radii.GetMax() >= 120.f);
		const TCHAR* MeshName = nullptr;
		const TCHAR* LookName = nullptr;
		if (bFlat) { MeshName = TEXT("SM_M4_Pheromone"); C->ComponentTags.AddUnique(TEXT("M4Fx")); }
		else if (Lum > 0.5f && H.R > 30.f && H.R < 60.f && H.G > 0.55f) { MeshName = TEXT("SM_M4_Droplet"); LookName = TEXT("Spark"); C->ComponentTags.AddUnique(TEXT("M4Fx")); }   // a flame
		else if (Lum < 0.03f && Radii.GetMax() >= 150.f) { MeshName = TEXT("SM_M4_TrashBag"); LookName = TEXT("TrashBag"); }
		else if (bEarth) { MeshName = TEXT("SM_M4_Crumb"); LookName = TEXT("Soil"); }
		else if (H.G < 0.18f && Lum > 0.15f && Lum < 0.6f) { MeshName = TEXT("SM_M4_DustBunny"); LookName = TEXT("Dust"); }
		else { MeshName = TEXT("SM_M4_Crumb"); }
		if (UStaticMesh* M = Map4Look::Prop(MeshName))
		{
			C->SetStaticMesh(M);
			if (LookName)
			{
				if (const Map4Look::FLook* L = Map4Look::Named(LookName)) { C->SetMaterial(0, Map4Look::Get(*L)); }
			}
		}
	}
	return C;
}

void AKitchenBuilder::Doors(float X0, float X1, int32 Count, const FLinearColor& DoorColor, const FLinearColor& HandleColor)
{
	const float W = (X1 - X0) / Count;
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = X0 + W * i;
		UStaticMeshComponent* D = Box(FVector(A + 15.f, 2988.f, 170.f), FVector(A + W - 15.f, 3000.f, 1600.f), DoorColor, false);
		const float Mid = A + W * 0.5f;
		UStaticMeshComponent* H = Box(FVector(Mid - 80.f, 2970.f, 1420.f), FVector(Mid + 80.f, 2988.f, 1460.f), HandleColor, false);
		if (AntArt::DressAuto(this, Root, TEXT("SM_Cabinet_Door"), FVector(A + 15.f, 2960.f, 170.f), FVector(A + W - 15.f, 3000.f, 1600.f)))
		{
			AntArt::HideAll({ D, H });
		}
	}
}

void AKitchenBuilder::Chair(const FVector& C, bool bBackSouth, const FLinearColor& Color)
{
	Box(FVector(C.X - 450.f, C.Y - 450.f, 800.f), FVector(C.X + 450.f, C.Y + 450.f, 880.f), Color);
	const float Off = 380.f;
	for (int32 sx = -1; sx <= 1; sx += 2)
	{
		for (int32 sy = -1; sy <= 1; sy += 2)
		{
			const float X = C.X + sx * Off, Y = C.Y + sy * Off;
			Box(FVector(X - 35.f, Y - 35.f, 0.f), FVector(X + 35.f, Y + 35.f, 800.f), Color);
		}
	}
	if (bBackSouth)
	{
		Box(FVector(C.X - 450.f, C.Y - 530.f, 880.f), FVector(C.X + 450.f, C.Y - 450.f, 2100.f), Color);
	}
	else
	{
		Box(FVector(C.X - 450.f, C.Y + 450.f, 880.f), FVector(C.X + 450.f, C.Y + 530.f, 2100.f), Color);
	}
}

UPointLightComponent* AKitchenBuilder::Lamp(const FVector& Loc, float Intensity, const FLinearColor& Color, float Radius, bool bShadows)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(Loc);
	L->IntensityUnits = ELightUnits::Unitless;
	L->bUseInverseSquaredFalloff = false;
	L->LightFalloffExponent = 2.f;
	L->Intensity = Intensity;
	L->AttenuationRadius = Radius;
	L->CastShadows = bShadows;
	L->VolumetricScatteringIntensity = 0.f;   // lamps must not light up the haze: only the sun draws shafts in it
	L->SetLightColor(Color);
	L->RegisterComponent();
	AddInstanceComponent(L);
	return L;
}

// ---------------------------------------------------------------------------------------------
// The kitchen
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::Build()
{
	bArt = KitchenArt::Enabled();   // the modelled kitchen replaces most of the boxes below (see BuildArt)
	// ---- Floor, walls, ceiling, baseboards: the shell with all its real holes (KitchenHouse.cpp) ----
	BuildShell();
	for (int32 i = 0; i < (Map4Look::Enabled() ? 0 : 14); ++i)   // checker tiles on top of the slab (map v4: the floor material has the tiles and the grout)
	{
		for (int32 j = 0; j < 10; ++j)
		{
			if ((i + j) % 2 == 0) continue;
			const float X = -7000.f + i * 1000.f, Y = -5000.f + j * 1000.f;
			if (X < 5300.f && X + 1000.f > 4700.f && Y < 2300.f && Y + 1000.f > 1700.f) continue;   // the loose tile's pit
			Box(FVector(X + 5.f, Y + 5.f, 0.f), FVector(X + 995.f, Y + 995.f, 2.f), TileB, false);
		}
	}

	// Fields beyond the fences, far below everything: the view isn't a void.
	Box(FVector(-40000.f, -60000.f, -400.f), FVector(40000.f, 30000.f, -300.f), Hex(70, 120, 50), false);

	BuildNorthRun();
	BuildYard();
	BuildWestStreet();
	BuildClutter();
	BuildAppliances();
	if (bArt)
	{
		BuildArt();
	}

	if (!bArt)   // modelled: splashback, wall cabinets, hood, toaster, cutting board and knife, sugar jar, mug
	{
		// Backsplash
		Box(FVector(-7000.f, 4970.f, 1710.f), FVector(2500.f, 5000.f, 2900.f), Backsplash, false);

		// Upper cabinets and range hood
		Box(FVector(-7000.f, 4300.f, 3000.f), FVector(-2000.f, 5000.f, 4400.f), Cabinet);
		Box(FVector(1000.f, 4300.f, 3000.f), FVector(2500.f, 5000.f, 4400.f), Cabinet);
		Box(FVector(-1600.f, 4200.f, 3300.f), FVector(100.f, 5000.f, 3600.f), Steel);
		Box(FVector(-1000.f, 4600.f, 3600.f), FVector(-500.f, 5000.f, 5400.f), Steel);

		// Things on the counters
		{
			UStaticMeshComponent* T = Box(FVector(-6600.f, 3900.f, 1710.f), FVector(-5900.f, 4500.f, 2150.f), Hex(200, 40, 40));   // toaster
			if (AntArt::DressAuto(this, Root, TEXT("SM_Toaster_Body"), FVector(-6600.f, 3900.f, 1710.f), FVector(-5900.f, 4500.f, 2160.f), 0.f, 4.f))
			{
				AntArt::HideAll({ T });
			}
		}
		Box(FVector(-6450.f, 4050.f, 2150.f), FVector(-6050.f, 4120.f, 2153.f), Black, false);
		Box(FVector(-6450.f, 4280.f, 2150.f), FVector(-6050.f, 4350.f, 2153.f), Black, false);
		Box(FVector(-5600.f, 3250.f, 1710.f), FVector(-4500.f, 3950.f, 1760.f), Wood);               // cutting board
		Box(FVector(-5400.f, 3450.f, 1760.f), FVector(-4800.f, 3510.f, 1772.f), Steel, false);       // knife
		Box(FVector(-4800.f, 3440.f, 1760.f), FVector(-4550.f, 3520.f, 1790.f), Black, false);
		Cyl(FVector(800.f, 4600.f, 1710.f), 250.f, 500.f, Hex(230, 235, 240));                         // sugar jar
		Cyl(FVector(-2200.f, 4200.f, 1710.f), 180.f, 300.f, Hex(230, 120, 40));                        // mug
	}
	BuildFridge();

	// ---- Trash corner (east): the pedal bin itself is in BuildAppliances (hollow), the lid is a gadget ----
	Ball(FVector(5200.f, -400.f, 250.f), FVector(500.f, 400.f, 300.f), Black);                        // tipped bag
	Box(FVector(4900.f, -700.f, 0.f), FVector(5300.f, -500.f, 30.f), Hex(230, 170, 90), false);       // pizza crust

	if (!bArt)   // modelled: bucket and mop, clock, table and everything on it, rug, chairs, the dog's mat, bowls and kibble
	{
		// Mop bucket + leaning mop stick (climbable)
		Cyl(FVector(6200.f, 3900.f, 0.f), 420.f, 650.f, Hex(40, 90, 200));
		Part(EAntShape::Cube, FVector(6450.f, 4400.f, 2150.f), FVector(60.f, 60.f, 4300.f), FRotator(0.f, 0.f, -12.f), Wood, true);

		// Clock on the east wall
		Part(EAntShape::Cylinder, FVector(6985.f, 1500.f, 3600.f), FVector(600.f, 600.f, 30.f), FRotator(90.f, 0.f, 0.f), White, false);
		Box(FVector(6960.f, 1490.f, 3600.f), FVector(6970.f, 1510.f, 3830.f), Black, false);
		Box(FVector(6960.f, 1500.f, 3590.f), FVector(6970.f, 1680.f, 3610.f), Black, false);

		// ---- Dining table ----
		Box(FVector(-1700.f, -1800.f, 1320.f), FVector(2700.f, 800.f, 1400.f), Wood);
		const FVector Legs[4] = { FVector(-1500.f, -1600.f, 0.f), FVector(2500.f, -1600.f, 0.f), FVector(-1500.f, 600.f, 0.f), FVector(2500.f, 600.f, 0.f) };
		for (const FVector& L : Legs)
		{
			Cyl(L, 110.f, 1320.f, DarkWood);
		}
		Box(FVector(-1700.f, -900.f, 1400.f), FVector(2700.f, -100.f, 1406.f), Hex(200, 50, 60), false); // runner
		Cyl(FVector(0.f, -800.f, 1400.f), 450.f, 30.f, White);                                           // plate
		Cyl(FVector(1600.f, -500.f, 1400.f), 500.f, 450.f, Hex(250, 150, 180));                          // cake
		Cyl(FVector(1600.f, -500.f, 1850.f), 510.f, 50.f, White);
		Ball(FVector(1450.f, -350.f, 1960.f), FVector(60.f, 60.f, 60.f), Hex(200, 10, 20));
		Cyl(FVector(2300.f, 300.f, 1400.f), 180.f, 380.f, Hex(60, 110, 200));                            // cup
		Cyl(FVector(-1000.f, 300.f, 1400.f), 300.f, 300.f, White);                                       // sugar bowl
		Cyl(FVector(-1450.f, -1200.f, 1400.f), 80.f, 260.f, Hex(230, 230, 230));                         // salt
		Cyl(FVector(-1250.f, -1200.f, 1400.f), 80.f, 260.f, Hex(60, 50, 45));                            // pepper
		Box(FVector(-2500.f, -2600.f, 0.f), FVector(3500.f, 1600.f, 4.f), Hex(170, 40, 50));             // rug (real collision: ants walk ON it)

		Chair(FVector(0.f, -2750.f, 0.f), true, DarkWood);
		Chair(FVector(1200.f, 1650.f, 0.f), false, DarkWood);

		// ---- South-west: the dog's corner (bowls on a mat) and spilled cereal ----
		Box(FVector(-4500.f, -4500.f, 0.f), FVector(-1800.f, -3200.f, 4.f), Hex(60, 80, 70));            // rubber mat
		Cyl(FVector(-3800.f, -3800.f, 0.f), 480.f, 170.f, Hex(240, 140, 40));
		Cyl(FVector(-3800.f, -3800.f, 170.f), 340.f, 55.f, Hex(110, 60, 25));                           // kibble heap (walkable)
		Ball(FVector(-3800.f, -3800.f, 225.f), FVector(300.f, 300.f, 40.f), Hex(110, 60, 25));
		Cyl(FVector(-2500.f, -3900.f, 0.f), 420.f, 140.f, Hex(120, 140, 170));
	}
	Cyl(FVector(-2500.f, -3900.f, 136.f), bArt ? 330.f : 380.f, 4.f, Hex(150, 200, 255));                          // water surface (wet, walkable)

	// Cereal box lying on its side, open end facing east: a cozy tunnel.
	if (!bArt)   // modelled: SM_K_CerealBox, hollow, flaps open at the east end
	{
		Box(FVector(-5800.f, 500.f, 0.f), FVector(-4600.f, 1300.f, 20.f), Hex(230, 70, 40));
		Box(FVector(-5800.f, 500.f, 400.f), FVector(-4600.f, 1300.f, 420.f), Hex(230, 70, 40));
		Box(FVector(-5800.f, 500.f, 20.f), FVector(-4600.f, 520.f, 400.f), Hex(230, 70, 40));
		Box(FVector(-5800.f, 1280.f, 20.f), FVector(-4600.f, 1300.f, 400.f), Hex(230, 70, 40));
		Box(FVector(-5800.f, 520.f, 20.f), FVector(-5780.f, 1280.f, 400.f), Hex(210, 60, 35));
		Box(FVector(-5700.f, 494.f, 120.f), FVector(-4700.f, 500.f, 300.f), Hex(250, 210, 50), false);
	}
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * 0.7f;
		Ball(FVector(-4500.f + FMath::Cos(A) * (120.f + i * 25.f), 900.f + FMath::Sin(A) * (150.f + i * 30.f), 15.f), FVector(28.f, 28.f, 14.f), Hex(250, 160, 40));
	}

	// Window on the south wall
	// (no glass: the opening lets sunlight and sky in)
	if (!bArt)   // modelled: window frame and sill, the wall shelf with its jars
	{
		Box(FVector(-30.f, -4990.f, 2200.f), FVector(30.f, -4980.f, 4200.f), White, false);
		Box(FVector(-1500.f, -4990.f, 3170.f), FVector(1500.f, -4980.f, 3230.f), White, false);
		Box(FVector(-1650.f, -5000.f, 2120.f), FVector(1650.f, -4820.f, 2200.f), White);                  // sill

		// Wall shelf with jars (west wall)
		Box(FVector(-7000.f, -1100.f, 2600.f), FVector(-6550.f, 1100.f, 2680.f), Wood);
		Box(FVector(-7000.f, -900.f, 2400.f), FVector(-6900.f, -850.f, 2600.f), DarkWood, false);
		Box(FVector(-7000.f, 850.f, 2400.f), FVector(-6900.f, 900.f, 2600.f), DarkWood, false);
		Cyl(FVector(-6780.f, -150.f, 2680.f), 110.f, 280.f, Hex(170, 40, 30));
		Cyl(FVector(-6780.f, -1000.f + 50.f, 2680.f), 60.f, 220.f, Hex(60, 140, 60));
	}
	// ---- The ant nest is OUTSIDE now (BuildWestStreet): here only the worn trail from the crack ----
	const FVector Nest = NestLocation();
	for (int32 i = 0; i < 14; ++i)   // little ant trail painted on the tiles
	{
		const float T = i / 13.f;
		const FVector P = FMath::Lerp(FVector(-6400.f, -2500.f, 3.f), FVector(-1800.f, -900.f, 3.f), T);
		Ball(P + FVector(0.f, FMath::Sin(T * 9.f) * 120.f, 0.f), FVector(22.f, 14.f, 3.f), Hex(40, 25, 20));
	}

	// ---- Rival fire-ant nest ----
	if (bArt)
	{
		// Level v2: the red anthill in yard B (SM_Y_AntHill from the placement table), their trail to the red crack under the
		// living-room window, a few red crumbs of earth round the mound that grow with their stores.
		const FVector R = NestLocation(1);
		const FLinearColor Red = Hex(150, 45, 25), DarkRed = Hex(90, 25, 15);
		AddMound(1, Ball(FVector(R.X + 560.f, R.Y + 330.f, -30.f), FVector(200.f, 180.f, 120.f), DarkRed));
		AddMound(1, Ball(FVector(R.X - 520.f, R.Y - 380.f, -30.f), FVector(180.f, 210.f, 110.f), Red));
		Cyl(FVector(R.X, R.Y, 0.f), NestRadius(1), 3.f, Hex(70, 25, 15), false);
		for (int32 i = 0; i < 22; ++i)
		{
			const float T = i / 21.f;
			const FVector P = FMath::Lerp(FVector(R.X + 600.f, R.Y + 200.f, 3.f), FVector(19500.f, -9300.f, 3.f), T);
			Ball(P + FVector(0.f, FMath::Sin(T * 11.f) * 140.f, 0.f), FVector(22.f, 14.f, 3.f), Hex(140, 30, 15));
		}
		Lamp(FVector(R.X - 300.f, R.Y, 700.f), 3.f, FLinearColor(1.f, 0.3f, 0.15f), 1500.f, false);
	}
	else
	{
		// The box-built kitchen: a red mound by the trash.
		const FVector R = NestLocation(1);
		const FLinearColor Red = Hex(150, 45, 25), DarkRed = Hex(90, 25, 15);
		AddMound(1, Ball(FVector(R.X + 420.f, R.Y, -60.f), FVector(520.f, 600.f, 420.f), Red));
		AddMound(1, Ball(FVector(R.X + 420.f, R.Y, 260.f), FVector(110.f, 110.f, 90.f), Black));       // entrance on top
		AddMound(1, Ball(FVector(R.X + 150.f, R.Y + 420.f, -20.f), FVector(220.f, 200.f, 160.f), DarkRed));
		AddMound(1, Ball(FVector(R.X + 180.f, R.Y - 430.f, -20.f), FVector(200.f, 230.f, 150.f), DarkRed));
		Cyl(FVector(R.X, R.Y, 0.f), NestRadius(1), 4.f, Hex(70, 25, 15), false);
		for (int32 i = 0; i < 18; ++i)   // their trail towards the middle
		{
			const float T = i / 17.f;
			const FVector P = FMath::Lerp(FVector(R.X - 600.f, R.Y, 3.f), FVector(1500.f, -1400.f, 3.f), T);
			Ball(P + FVector(0.f, FMath::Sin(T * 11.f) * 140.f, 0.f), FVector(22.f, 14.f, 3.f), Hex(140, 30, 15));
		}
		Lamp(FVector(R.X - 300.f, R.Y, 500.f), 3.f, FLinearColor(1.f, 0.3f, 0.15f), 1500.f, false);
	}

	// ---- Spider web on the east wall (the spider's lair) ----
	{
		const FVector C = SpiderLair() + FVector(10.f, 0.f, 0.f);
		const FLinearColor SilkC(0.85f, 0.85f, 0.9f);
		const int32 Spokes = 14;
		TArray<FVector> Ends;
		for (int32 i = 0; i < Spokes; ++i)
		{
			const float A = 2.f * PI * i / Spokes;
			const float R = 650.f + 120.f * FMath::Sin(i * 2.3f);
			Ends.Add(C + FVector(0.f, FMath::Cos(A) * R, FMath::Sin(A) * R));
		}
		auto Thread = [this, &SilkC](const FVector& A, const FVector& B)
		{
			const FVector D = B - A;
			UStaticMeshComponent* T = Part(EAntShape::Cylinder, (A + B) * 0.5f, FVector(3.f, 3.f, (float)D.Size()), FRotationMatrix::MakeFromZ(D).Rotator(), SilkC, false);
			T->SetCastShadow(false);
		};
		for (const FVector& E : Ends) { Thread(C, E); }
		for (int32 Ring = 1; Ring <= 6; ++Ring)
		{
			const float K = Ring / 6.5f;
			for (int32 i = 0; i < Spokes; ++i)
			{
				Thread(FMath::Lerp(C, Ends[i], K), FMath::Lerp(C, Ends[(i + 1) % Spokes], K));
			}
		}
		// Wrapped-up prey: little silk cocoons.
		Ball(C + FVector(-20.f, 260.f, -330.f), FVector(30.f, 40.f, 70.f), FLinearColor(0.9f, 0.9f, 0.85f));
		Ball(C + FVector(-20.f, -310.f, 120.f), FVector(30.f, 35.f, 60.f), FLinearColor(0.9f, 0.9f, 0.85f));
	}

	// ---- Lights ----
	const FVector LampSpots[3] = { FVector(-3500.f, 0.f, 0.f), FVector(2200.f, -1500.f, 0.f), FVector(1000.f, 2800.f, 0.f) };
	for (const FVector& S : LampSpots)
	{
		if (!bArt)   // the modelled lamps join LampShades in BuildArt
		{
			LampShades.Add(Cyl(FVector(S.X, S.Y, 5150.f), 350.f, 250.f, White, false));
		}
		MainLamps.Add(Lamp(FVector(S.X, S.Y, 4950.f), 8.f, FLinearColor(1.f, 0.93f, 0.8f), 16000.f, true));
	}
	Lamp(FVector(-6800.f, Nest.Y, 300.f), 2.f, FLinearColor(1.f, 0.7f, 0.4f), 1400.f, false);      // daylight through the crack
	WindowLight = Lamp(FVector(0.f, -4500.f, 3200.f), 3.f, FLinearColor(0.7f, 0.85f, 1.f), 6000.f, false);       // window daylight
	Lamp(FVector(3900.f, 5300.f, 330.f), 2.5f, FLinearColor(1.f, 0.6f, 0.25f), 800.f, false);      // candle in the mouse room

	// The lamp over the dinner table: a warm pool of light on the table, real shadows under it. Follows the switch.
	{
		const FVector P(500.f, -500.f, 3500.f);
		if (!LevelV3::Enabled())   // (level v3: the modelled pendant lamp, SM_V3_PendantLamp, hangs there instead of the box-built one)
		{
			Cyl(FVector(P.X, P.Y, P.Z + 280.f), 9.f, 5400.f - P.Z - 280.f, Black, false);                                             // cord
			Part(EAntShape::Cone, FVector(P.X, P.Y, P.Z + 150.f), FVector(950.f, 950.f, 300.f), FRotator::ZeroRotator, Hex(200, 110, 55), false);   // shade
			PendantBulb = AntFx::MakeGlow(Part(EAntShape::Sphere, FVector(P.X, P.Y, P.Z - 30.f), FVector(170.f, 170.f, 200.f), FRotator::ZeroRotator, White, false), FLinearColor(6.f, 4.4f, 2.2f));
		}
		Pendant = NewObject<USpotLightComponent>(this);
		Pendant->SetMobility(EComponentMobility::Movable);
		Pendant->SetupAttachment(Root);
		Pendant->SetRelativeLocationAndRotation(FVector(P.X, P.Y, P.Z - 40.f), FRotator(-90.f, 0.f, 0.f));
		Pendant->IntensityUnits = ELightUnits::Unitless;
		Pendant->bUseInverseSquaredFalloff = false;
		Pendant->LightFalloffExponent = 2.f;
		Pendant->Intensity = 0.f;
		Pendant->AttenuationRadius = 6500.f;
		Pendant->InnerConeAngle = 22.f;   // a pool on the table and the floor around it, not a floodlight for the walls
		Pendant->OuterConeAngle = 46.f;
		Pendant->CastShadows = true;
		Pendant->VolumetricScatteringIntensity = 0.5f;   // a soft cone in the haze at night
		Pendant->SetLightColor(FLinearColor(1.f, 0.76f, 0.48f));
		Pendant->RegisterComponent();
		AddInstanceComponent(Pendant);
	}
	// The light under the range hood: always on, a small warm island over the stove.
	{
		const FVector P(-750.f, 4550.f, 3290.f);
		HoodBulb = AntFx::MakeGlow(Box(FVector(P.X - 260.f, P.Y - 60.f, P.Z - 6.f), FVector(P.X + 260.f, P.Y + 60.f, P.Z + 10.f), White, false), FLinearColor(5.f, 4.2f, 2.8f));
		HoodLight = Lamp(P - FVector(0.f, 150.f, 60.f), 0.f, FLinearColor(1.f, 0.84f, 0.62f), 2700.f, true);
	}
	// The light a real room bounces around, faked with weak shadowless fills (cheap, and identical on every machine):
	// one from above the ceiling - the floor under furniture is dark but never black - and one from under the floor -
	// the undersides of tables and the lower walls catch what the floor throws back. Outside the room on purpose:
	// no bright blob on the ceiling or the floor next to them.
	for (const FVector& C : { FVector(0.f, 0.f, 0.f), FVector(13700.f, -4000.f, 0.f) })
	{
		AmbFills.Add(Lamp(C + FVector(0.f, 0.f, 9500.f), 0.f, FLinearColor::White, 30000.f, false));
		BounceFills.Add(Lamp(C - FVector(0.f, 0.f, 4500.f), 0.f, FLinearColor::White, 30000.f, false));
	}

	BuildSecrets();
	BuildLivingRoom();

	Fill = NewObject<UDirectionalLightComponent>(this);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetupAttachment(Root);
	// The sun: low, from the south, through the window. Casts real shadows and light shafts.
	Fill->SetRelativeRotation(FRotator(-32.f, 82.f, 0.f));
	Fill->Intensity = 7.f;
	Fill->CastShadows = true;
	Fill->SetLightColor(FLinearColor(1.f, 0.9f, 0.75f));
	Fill->bAtmosphereSunLight = true;
	Fill->VolumetricScatteringIntensity = 3.f;
	Fill->LightSourceAngle = 1.2f;   // slightly soft shadow edges: cosier, and furniture shadows read as shade, not as holes
	if (Map4Look::Enabled())
	{
		// map v4: contact shadows ground the small things (crumbs, screws, an ant's feet) at ant scale
		Fill->ContactShadowLength = 0.04f;
		Fill->ContactShadowLengthInWS = false;
	}
	Fill->RegisterComponent();
	AddInstanceComponent(Fill);

	// Sky (seen through the window and bounced around by Lumen).
	USkyAtmosphereComponent* Sky = NewObject<USkyAtmosphereComponent>(this);
	Sky->SetupAttachment(Root);
	Sky->RegisterComponent();
	AddInstanceComponent(Sky);

	USkyLightComponent* SkyL = NewObject<USkyLightComponent>(this);
	SkyL->SetMobility(EComponentMobility::Movable);
	SkyL->SetupAttachment(Root);
	SkyL->bRealTimeCapture = true;
	SkyL->Intensity = 1.2f;
	SkyL->VolumetricScatteringIntensity = 0.25f;   // the sky barely lights the haze: the room stays clear, the beams stand out
	SkyL->RegisterComponent();
	AddInstanceComponent(SkyL);
	SkyFill = SkyL;

	// Dust in the air: visible sun beams. The haze itself is thin (the far wall must stay crisp); the beams come from
	// the sun lighting it far more strongly than anything else does (FTimeOfDay::SunShafts), the lamps not at all.
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(Root);
	Fog->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	Fog->FogDensity = 0.004f;
	Fog->FogHeightFalloff = 0.02f;
	Fog->FogMaxOpacity = 0.6f;
	Fog->bEnableVolumetricFog = true;
	Fog->VolumetricFogScatteringDistribution = 0.45f;
	Fog->VolumetricFogExtinctionScale = 1.f;
	Fog->VolumetricFogDistance = 16000.f;
	Fog->RegisterComponent();
	AddInstanceComponent(Fog);
	Haze = Fog;

	// Exposure and white balance follow the time of day (off with -AntNoPP, like the rest of the grade).
	if (!FParse::Param(FCommandLine::Get(), TEXT("AntNoPP")) && GetNetMode() != NM_DedicatedServer)
	{
		TodGrade = NewObject<UPostProcessComponent>(this);
		TodGrade->SetupAttachment(Root);
		TodGrade->bUnbound = true;
		TodGrade->Priority = 20.f;
		TodGrade->RegisterComponent();
		AddInstanceComponent(TodGrade);
		if (Map4Look::Enabled())
		{
			// map v4: each room keeps its own colour under the shared time-of-day grade (soft 900 uu blend at the doors):
			// the kitchen warm and a little dense, the living room cooler and calmer, the yard green and crisp
			struct FRoomGrade { FVector Center, Extent; FVector4 Gain, Gamma, Sat; float Contrast; };
			const FRoomGrade Rooms[] = {
				{ FVector(0.f, 0.f, 2700.f), FVector(7000.f, 5000.f, 2700.f), FVector4(1.04f, 1.0f, 0.93f, 1.f), FVector4(1.f, 1.f, 1.f, 0.97f), FVector4(1.f, 1.f, 1.f, 1.02f), 1.06f },
				{ FVector(13700.f, -3750.f, 2700.f), FVector(6500.f, 5250.f, 2700.f), FVector4(0.97f, 1.0f, 1.04f, 1.f), FVector4(1.f, 1.f, 1.f, 1.01f), FVector4(1.f, 1.f, 1.f, 0.96f), 1.04f },
				{ FVector(-5500.f, -10000.f, 1500.f), FVector(12500.f, 5000.f, 3000.f), FVector4(1.0f, 1.03f, 0.97f, 1.f), FVector4(1.f, 1.f, 1.f, 1.f), FVector4(1.f, 1.f, 1.f, 1.08f), 1.08f },
			};
			for (const FRoomGrade& Rg : Rooms)
			{
				UBoxComponent* Bx = NewObject<UBoxComponent>(this);
				Bx->SetupAttachment(Root);
				Bx->SetRelativeLocation(Rg.Center);
				Bx->SetBoxExtent(Rg.Extent);
				Bx->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Bx->SetHiddenInGame(true);
				Bx->ComponentTags.Add(TEXT("AntDebug"));
				Bx->RegisterComponent();
				AddInstanceComponent(Bx);
				UPostProcessComponent* Pp = NewObject<UPostProcessComponent>(this);
				Pp->SetupAttachment(Bx);
				Pp->bUnbound = false;
				Pp->BlendRadius = 900.f;
				Pp->Priority = 25.f;
				FPostProcessSettings& G = Pp->Settings;
				G.bOverride_ColorGainMidtones = true; G.ColorGainMidtones = Rg.Gain;
				G.bOverride_ColorGammaShadows = true; G.ColorGammaShadows = Rg.Gamma;
				G.bOverride_ColorSaturation = true; G.ColorSaturation = Rg.Sat;
				G.bOverride_ColorContrast = true; G.ColorContrast = FVector4(Rg.Contrast, Rg.Contrast, Rg.Contrast, 1.f);
				Pp->RegisterComponent();
				AddInstanceComponent(Pp);
			}
		}
	}
	// -AntTime=0|1|2 pins the time of day (morning / golden evening / night) whatever the shift is.
	int32 Pinned = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntTime="), Pinned))
	{
		ForcedTod = FMath::Clamp(Pinned, 0, 2);
	}

	bMainOn = AKitchenSwitch::AreLightsOn(GetWorld());
	UpdateTimeOfDay(0.f);
	bBuilt = true;
	ApplyV3Layout();   // level v3: the districts of the seed known so far (a client may have it before it is built)
}

// ---------------------------------------------------------------------------------------------
// The modelled kitchen: every fixed piece of the Blender set at the placement from its manifest (KitchenArtData.inl).
// Each piece collides with its own triangles, so the cavities the game relies on are real: the corridor behind the
// cabinets, the hollow oven, fridge, dishwasher, microwave, bin and under-sink unit, the gap under the fridge.
// Doors, drawers, lids and racks are not placed here: they are gadget actors (AKitchenDoor, AKitchenMover, ...).
// ---------------------------------------------------------------------------------------------

bool AKitchenBuilder::ScatterTufts(const TArray<FTransform>& Blades, int32 Seed)
{
	// One instanced component per tuft model, no collision (ants walk through grass as before). A tuft has 5-9 blades,
	// so one tuft stands in for three of the old single strips.
	static const TCHAR* Names[3] = { TEXT("SM_Y_GrassTuft_A"), TEXT("SM_Y_GrassTuft_B"), TEXT("SM_Y_GrassTuft_C") };
	UInstancedStaticMeshComponent* Tufts[3] = { nullptr, nullptr, nullptr };
	for (int32 k = 0; k < 3; ++k)
	{
		UStaticMesh* M = KitchenArt::Mesh(Names[k]);
		if (!M)
		{
			return false;
		}
		Tufts[k] = NewObject<UInstancedStaticMeshComponent>(this);
		Tufts[k]->SetupAttachment(Root);
		Tufts[k]->SetStaticMesh(M);
		Tufts[k]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tufts[k]->SetCastShadow(false);
		Tufts[k]->SetCanEverAffectNavigation(false);
		KitchenArt::ApplyLook(Tufts[k], Names[k]);
		for (int32 m = 0; m < Tufts[k]->GetNumMaterials(); ++m)
		{
			FLinearColor T(0, 0, 0);
			UMaterialInterface* Mi = Tufts[k]->GetMaterial(m);
			if (Mi) { Mi->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Tint")), T); }
			UE_LOG(LogTemp, Display, TEXT("M4TUFT %s slot %d %s tint %s"), Names[k], m, *GetNameSafe(Mi ? Mi->GetMaterial() : nullptr), *T.ToString());
		}
		Tufts[k]->RegisterComponent();
		AddInstanceComponent(Tufts[k]);
	}
	FRandomStream R(Seed);
	TArray<FTransform> Xf[3];
	for (int32 i = 0; i < Blades.Num(); i += 3)
	{
		const FVector P = Blades[i].GetLocation();
		const float S = KitchenArt::ImportScale * R.FRandRange(0.8f, 1.35f);
		Xf[R.RandRange(0, 2)].Add(FTransform(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.f), FVector(S)));
	}
	for (int32 k = 0; k < 3; ++k)
	{
		Tufts[k]->AddInstances(Xf[k], false, true);
	}
	return true;
}

bool AKitchenBuilder::LetThrough(ACharacter* Big)
{
	UCapsuleComponent* Cap = Big ? Big->GetCapsuleComponent() : nullptr;
	if (!Cap || !Big->GetWorld())
	{
		return false;
	}
	for (TActorIterator<AKitchenBuilder> It(Big->GetWorld()); It; ++It)
	{
		if (!It->bBuilt)
		{
			return false;   // not built yet (a client joining): ask again
		}
		for (UPrimitiveComponent* C : It->AntsOnlyParts)
		{
			if (C)
			{
				Cap->IgnoreComponentWhenMoving(C, true);
			}
		}
		return true;
	}
	return false;
}

void AKitchenBuilder::LetStepOver(ACharacter* Big)
{
	// map v4: low soft or toy things a grown man steps over or onto without a thought (the kid's car track, a floor
	// cushion, knocked loose or not): his capsule passes them (the collision test had him stalled at both)
	UCapsuleComponent* Cap = Big ? Big->GetCapsuleComponent() : nullptr;
	if (!Cap || !Big->GetWorld())
	{
		return;
	}
	static const TCHAR* Names[] = { TEXT("SM_K_CarTrack"), TEXT("SM_V3_FloorCushion"), TEXT("SM_L_TrainTrack"), TEXT("SM_M4R_L_TrainTrack") };
	auto Consider = [&](AActor* A)
	{
		TArray<UStaticMeshComponent*> Ms;
		A->GetComponents<UStaticMeshComponent>(Ms);
		for (UStaticMeshComponent* C : Ms)
		{
			if (!C || !C->GetStaticMesh()) { continue; }
			const FString N = C->GetStaticMesh()->GetName();
			for (const TCHAR* K : Names) { if (N == K) { Cap->IgnoreComponentWhenMoving(C, true); break; } }
		}
	};
	for (TActorIterator<AKitchenBuilder> It(Big->GetWorld()); It; ++It) { Consider(*It); }
	for (TActorIterator<AAntWreckBody> It(Big->GetWorld()); It; ++It) { Consider(*It); }
}

UPrimitiveComponent* AKitchenBuilder::FindArt(const UWorld* World, const TCHAR* MeshName)
{
	for (TActorIterator<AKitchenBuilder> It(World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
		for (UStaticMeshComponent* C : Parts)
		{
			if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName() == MeshName)
			{
				return C;
			}
		}
	}
	return nullptr;
}

void AKitchenBuilder::BuildArt()
{
	const TConstArrayView<FKitchenArtInst> All = KitchenArt::Instances();
	const bool bV3 = LevelV3::Enabled();
	ArtByInst.Init(nullptr, All.Num());
	TSet<int32> Fixtured;
	if (bV3)
	{
		LevelV3::FixtureRows(Fixtured);   // the shelf, the jars, the curtains...: their actors build them (KitchenFixture)
	}
	for (int32 n = 0; n < All.Num(); ++n)
	{
		const FKitchenArtInst& I = All[n];
		if (bV3 && I.Role == KitchenArt::Static && FCString::Strcmp(I.Name, TEXT("SM_V3_CurtainPanel")) == 0)
		{
			UStaticMeshComponent* Px = AKitchenFixture::AddClimbProxy(this, Root, KitchenArt::WorldTransform(I));   // the ants climb a curtain (the sheet has no grip)
			MakeAntsOnly(Px);
			CurtainProxies.Add(n, Px);
		}
		if (I.Role != KitchenArt::Static || Fixtured.Contains(n))
		{
			continue;
		}
		// (level v3: what a district variant may move or hide is movable, the rest is static)
		UStaticMeshComponent* C = KitchenArt::Add(this, Root, I.Name, I.Index, KitchenArt::WorldTransform(I), KitchenArt::ESolid::Static, !(bV3 && LevelV3::IsVariantRow(n)));
		ArtByInst[n] = C;
		if (C && bV3 && (LevelV3::BaseFlags(n) & 1))
		{
			MakeAntsOnly(C);
		}
		if (C && bV3)
		{
			KitchenArt::ApplyTint(C, LevelV3::BaseTint(n));
			if (FCString::Strcmp(I.Name, TEXT("SM_V3_CurtainPanel")) == 0)
			{
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // (the climb proxy above is what collides)
			}
		}
		if (C && FString(I.Name).EndsWith(TEXT("_CeilingLamp")))
		{
			LampShades.Add(C);   // its shade (material 0) glows with the light switch like the old cylinders did
		}
		// Level v2, "ants only": threads and strings an ant walks along but the Human, grandma and the kid walk straight through.
		static const TCHAR* const AntsOnly[] = { TEXT("SM_L_YarnBridge"), TEXT("SM_L_YarnThread"), TEXT("SM_L_Garland"), TEXT("SM_K_ChiliString"), TEXT("SM_K_FlyPaper") };   // (map v4: + the yarn thread: the Human stalled at grandma's thread)
		for (const TCHAR* N : AntsOnly)
		{
			if (C && FCString::Strcmp(I.Name, N) == 0)
			{
				C->ComponentTags.AddUnique(TEXT("AntsOnly"));
				C->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);   // food does not hang on a thread
				C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
				AntsOnlyParts.Add(C);
			}
		}
	}
	if (bV3)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTSEED level v3 base built: %d rows, %d of them held by fixtures"), All.Num(), Fixtured.Num());
	}
	// The pavement slabs have open joints: a bed under them, so nobody drops through a joint into the void.
	Box(FVector(-11000.f, -5200.f, -260.f), FVector(-9400.f, 5800.f, -200.f), Hex(110, 108, 102));
	// The mop bucket is hollow now: the water in it is ours (wet zone in EnvironmentFor, a loot spot floats on it).
	if (UStaticMeshComponent* Water = Cyl(FVector(6200.f, 3900.f, 600.f), 305.f, 40.f, Hex(150, 200, 255)))   // (KAN-22: the hero bucket is 310 inside at this height)
	{
		Water->ComponentTags.AddUnique(TEXT("AntBucketWater"));   // spilled when the bucket tips or breaks (AntWreck)
	}
	if (Map4Look::Enabled())
	{
		// map v4 set dressing: things of the family that tell the evening (each stands on whatever is under it; ants climb them)
		struct FDress { const TCHAR* Name; float X, Y, Yaw; };
		static const FDress Dress[] =
		{
			{ TEXT("SM_M4_BirthdayCake"), 3350.f, 4450.f, 200.f },   // the birthday cake, hidden by grandma on top of the fridge
			{ TEXT("SM_M4_Teddy"), 16250.f, 380.f, 10.f },           // the kid's teddy on the sofa
		};
		for (const FDress& D : Dress)
		{
			FHitResult H;
			const FVector Top(D.X, D.Y, 5300.f);
			FCollisionQueryParams Q(SCENE_QUERY_STAT(M4Dress), true, this);
			Q.bIgnoreTouches = true;
			if (!GetWorld()->LineTraceSingleByChannel(H, Top, Top - FVector(0.f, 0.f, 5600.f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(M4Dress), true)))
			{
				continue;
			}
			if (UStaticMeshComponent* C = Map4Look::AddProp(this, Root, D.Name, FTransform(FRotator(0.f, D.Yaw, 0.f), H.ImpactPoint - GetActorLocation(), FVector(KitchenArt::ImportScale)), true))
			{
				C->ComponentTags.AddUnique(TEXT("M4Dress"));
				UE_LOG(LogTemp, Display, TEXT("M4DRESS %s on %s at (%.0f,%.0f,%.0f)"), D.Name, H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("-"), H.ImpactPoint.X, H.ImpactPoint.Y, H.ImpactPoint.Z);
			}
		}
		// The cornice: its carved cove (many small facets in the corner) stopped the ants between the wall and the ceiling
		// (map v4 collision test: 25 stalls, the same in v3). It keeps its look; ants and everything else collide with a
		// plain 45-degree wedge filling the same corner, so wall -> cove -> ceiling is two clean turns.
		{
			TArray<UStaticMeshComponent*> Ms;
			GetComponents<UStaticMeshComponent>(Ms);
			int32 Wedges = 0;
			for (UStaticMeshComponent* C : Ms)
			{
				if (!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetName() != TEXT("SM_V3_Cornice")) { continue; }
				const FVector Sc = C->GetComponentScale().GetAbs();
				const FBox Mb = C->GetStaticMesh()->GetBoundingBox();
				const float Len = (float)(Mb.GetSize().X * Sc.X);
				const float H = (float)FMath::Max(Mb.GetSize().Z * Sc.Z, Mb.Max.Y * Sc.Y);   // the cove's drop on the wall / reach on the ceiling
				if (Len < 50.f || H < 10.f) { continue; }
				UBoxComponent* Wd = NewObject<UBoxComponent>(this);
				Wd->SetupAttachment(Root);
				// (centre 10 uu into the corner, so both tips sit inside the wall and the ceiling: no knife-edge sliver at either
				// joint for a crawling ant to slip off; map v4 test: falls where the ceiling met the tip)
				Wd->SetWorldLocationAndRotation(C->GetComponentLocation() + C->GetComponentQuat().RotateVector(FVector(Mb.GetCenter().X * Sc.X, -10.f / FMath::Max(0.01f, (float)Sc.Y), 10.f / FMath::Max(0.01f, (float)Sc.Z)) * FVector(1.f, Sc.Y, Sc.Z)),
					C->GetComponentQuat() * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(45.f)));
				Wd->SetBoxExtent(FVector(Len * 0.5f, H * 0.7071f, H * 0.7071f));
				Wd->SetCollisionObjectType(ECC_WorldStatic);
				Wd->SetCollisionResponseToAllChannels(ECR_Block);
				Wd->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
				Wd->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
				Wd->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Wd->SetHiddenInGame(true);
				Wd->SetCanEverAffectNavigation(false);
				Wd->ComponentTags.Add(TEXT("AntDebug"));
				Wd->RegisterComponent();
				AddInstanceComponent(Wd);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				++Wedges;
			}
			UE_LOG(LogTemp, Display, TEXT("M4CORNICE %d cornices collide as a 45-degree wedge"), Wedges);
		}
		// the fly paper over the table reads as fly paper: a few flies stuck to the strip (dressing, no collision)
		for (UPrimitiveComponent* Ap : AntsOnlyParts)
		{
			UStaticMeshComponent* C = Cast<UStaticMeshComponent>(Ap);
			if (!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetName() != TEXT("SM_K_FlyPaper")) { continue; }
			const FBox B = C->Bounds.GetBox();
			const FVector E = B.GetExtent(), Mid = B.GetCenter();
			static const float Hs[3] = { 0.28f, 0.52f, 0.71f };
			for (int32 k = 0; k < 3; ++k)
			{
				const float Side = (k % 2) ? 1.f : -1.f;
				const FVector At(Mid.X + Side * E.X * 0.85f, Mid.Y + (k - 1) * E.Y * 0.3f, B.Min.Z + Hs[k] * 2.f * E.Z);
				UStaticMesh* Fly = Map4Look::Prop(TEXT("SM_M4_FlyBody"));
				const float S = Fly ? 44.f / FMath::Max(1.f, (float)Fly->GetBounds().BoxExtent.GetMax() * 2.f) : 1.f;
				if (UStaticMeshComponent* F = Map4Look::AddProp(this, Root, TEXT("SM_M4_FlyBody"), FTransform(FRotator(70.f * Side, 90.f * Side + k * 25.f, 15.f * k), At - GetActorLocation(), FVector(S)), false))
				{
					F->ComponentTags.AddUnique(TEXT("M4Dress"));
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// North counter run: cabinets stop 250 short of the wall, leaving a secret corridor behind them
// (y 4750..5000) under the countertop. Inside: the drawer, the hollow under-sink cabinet, the oven.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildNorthRun()
{
	const float Back = 4750.f;   // back face of the cabinets

	if (!bArt)   // modelled: the whole run - cabinets, drawers, under-sink unit and its contents, worktops, sink, faucet, range
	{
		Box(FVector(-7000.f, 3100.f, 0.f), FVector(-4200.f, Back, 120.f), Black);                   // kick plinths
		Box(FVector(-2800.f, 3100.f, 0.f), FVector(2500.f, Back, 120.f), Black);
		Box(FVector(-4200.f, 3100.f, 0.f), FVector(-3500.f, Back, 120.f), Black);

		// West block with a drawer cavity (x -5600..-4300, z 1330..1600).
		Box(FVector(-7000.f, 3000.f, 120.f), FVector(-4200.f, Back, 1330.f), Cabinet);
		Box(FVector(-7000.f, 3000.f, 1330.f), FVector(-5600.f, Back, 1650.f), Cabinet);
		Box(FVector(-4300.f, 3000.f, 1330.f), FVector(-4200.f, Back, 1650.f), Cabinet);
		Box(FVector(-5600.f, 3000.f, 1600.f), FVector(-4300.f, Back, 1650.f), Cabinet);
		Box(FVector(-5600.f, 3060.f, 1330.f), FVector(-4300.f, Back, 1332.f), Hex(90, 120, 110), false);
		for (int32 i = 0; i < 4; ++i)   // decorative doors under the drawer
		{
			const float A = -7000.f + 700.f * i;
			UStaticMeshComponent* D = Box(FVector(A + 15.f, 2988.f, 170.f), FVector(A + 685.f, 3000.f, 1290.f), CabinetDoor, false);
			UStaticMeshComponent* H = Box(FVector(A + 270.f, 2970.f, 1180.f), FVector(A + 430.f, 2988.f, 1220.f), Steel, false);
			if (AntArt::DressAuto(this, Root, TEXT("SM_Cabinet_Door"), FVector(A + 15.f, 2960.f, 170.f), FVector(A + 685.f, 3000.f, 1290.f)))
			{
				AntArt::HideAll({ D, H });
			}
		}
		Box(FVector(-6985.f, 2988.f, 1350.f), FVector(-5615.f, 3000.f, 1590.f), CabinetDoor, false);   // fake drawer fronts
		Box(FVector(-6380.f, 2970.f, 1450.f), FVector(-6220.f, 2988.f, 1490.f), Steel, false);

		// Hollow under-sink cabinet: left door shut, right door hanging open.
		Box(FVector(-4200.f, 3000.f, 120.f), FVector(-2800.f, Back, 140.f), Hex(110, 150, 135));
		Box(FVector(-4200.f, 3000.f, 140.f), FVector(-4180.f, Back, 1100.f), Cabinet);
		Box(FVector(-2820.f, 3000.f, 140.f), FVector(-2800.f, Back, 1100.f), Cabinet);
		Box(FVector(-4180.f, Back - 20.f, 140.f), FVector(-3600.f, Back, 1100.f), Hex(110, 150, 135));     // back panel with a hole
		Box(FVector(-3400.f, Back - 20.f, 140.f), FVector(-2820.f, Back, 1100.f), Hex(110, 150, 135));
		Box(FVector(-3600.f, Back - 20.f, 330.f), FVector(-3400.f, Back, 1100.f), Hex(110, 150, 135));
		{
			UStaticMeshComponent* D = Box(FVector(-4200.f, 2985.f, 140.f), FVector(-3500.f, 3000.f, 1100.f), CabinetDoor);
			UStaticMeshComponent* H = Box(FVector(-3700.f, 2970.f, 600.f), FVector(-3660.f, 2985.f, 760.f), Steel, false);
			if (AntArt::DressAuto(this, Root, TEXT("SM_Cabinet_Door"), FVector(-4200.f, 2960.f, 140.f), FVector(-3500.f, 3000.f, 1100.f)))
			{
				AntArt::HideAll({ D, H });
			}
		}
		Part(EAntShape::Cube, FVector(-2920.f, 2671.f, 620.f), FVector(700.f, 15.f, 940.f), FRotator(0.f, -110.f, 0.f), CabinetDoor, true);
		Box(FVector(-4200.f, 3000.f, 0.f), FVector(-3500.f, 3100.f, 120.f), Black);                       // plinth under the shut door
		// Pipes: drain going down from the sink, U-trap into the wall.
		Cyl(FVector(-3500.f, 4000.f, 640.f), 45.f, 460.f, Hex(235, 235, 230));
		Part(EAntShape::Cylinder, FVector(-3500.f, 4360.f, 640.f), FVector(90.f, 90.f, 720.f), FRotator(0.f, 0.f, 90.f), Hex(235, 235, 230), true);
		Cyl(FVector(-3900.f, 3500.f, 140.f), 160.f, 650.f, Hex(60, 120, 220));      // detergent bottle
		Cyl(FVector(-3900.f, 3500.f, 790.f), 60.f, 120.f, Hex(240, 240, 240));
		Box(FVector(-3300.f, 4100.f, 140.f), FVector(-3000.f, 4400.f, 260.f), Hex(230, 210, 60));     // sponge
		Box(FVector(-3300.f, 4100.f, 260.f), FVector(-3000.f, 4400.f, 290.f), Hex(60, 160, 70));

		// Left of the oven, the oven itself (hollow, door is a gadget), right block.
		Box(FVector(-2800.f, 3000.f, 120.f), FVector(-1500.f, Back, 1650.f), Cabinet);
		Box(FVector(-1500.f, 3000.f, 120.f), FVector(0.f, Back, 300.f), DarkSteel);
		Box(FVector(-1500.f, 3000.f, 1500.f), FVector(0.f, Back, 1650.f), DarkSteel);
		Box(FVector(-1500.f, 3000.f, 300.f), FVector(-1450.f, Back, 1500.f), DarkSteel);
		Box(FVector(-50.f, 3000.f, 300.f), FVector(0.f, Back, 1500.f), DarkSteel);
		Box(FVector(-1450.f, 4700.f, 300.f), FVector(-800.f, Back, 1500.f), Hex(30, 25, 25));     // oven back wall, with a hole gnawed through into the corridor
		Box(FVector(-600.f, 4700.f, 300.f), FVector(-50.f, Back, 1500.f), Hex(30, 25, 25));
		Box(FVector(-800.f, 4700.f, 430.f), FVector(-600.f, Back, 1500.f), Hex(30, 25, 25));
		Box(FVector(-1450.f, 3100.f, 880.f), FVector(-50.f, 4650.f, 900.f), Steel);                      // rack
		Box(FVector(-1100.f, 3500.f, 900.f), FVector(-400.f, 4300.f, 915.f), Hex(60, 60, 64));          // baking tray
		Box(FVector(-1400.f, 3200.f, 1478.f), FVector(-100.f, 4600.f, 1490.f), Hex(255, 90, 30), false); // glowing element
		Box(FVector(-1400.f, 3200.f, 302.f), FVector(-100.f, 4600.f, 312.f), Hex(255, 90, 30), false);
		Box(FVector(1300.f, 3000.f, 120.f), FVector(2500.f, Back, 1650.f), Cabinet);   // the dishwasher is x 0..1300 (BuildAppliances)

		// Countertop with a hole for the sink basin (covers the secret corridor too).
		Box(FVector(-7000.f, 2950.f, 1650.f), FVector(-4200.f, 5000.f, 1710.f), CounterTop);
		Box(FVector(-2800.f, 2950.f, 1650.f), FVector(2500.f, 5000.f, 1710.f), CounterTop);
		Box(FVector(-4200.f, 2950.f, 1650.f), FVector(-2800.f, 3300.f, 1710.f), CounterTop);
		Box(FVector(-4200.f, 4700.f, 1650.f), FVector(-2800.f, 5000.f, 1710.f), CounterTop);
		Box(FVector(-4200.f, 3300.f, 1650.f), FVector(-4100.f, 4700.f, 1710.f), CounterTop);
		Box(FVector(-2900.f, 3300.f, 1650.f), FVector(-2800.f, 4700.f, 1710.f), CounterTop);

		// Sink basin
		Box(FVector(-4100.f, 3300.f, 1100.f), FVector(-2900.f, 4700.f, 1150.f), Steel);
		Box(FVector(-4200.f, 3000.f, 1100.f), FVector(-4100.f, 5000.f, 1650.f), Steel);
		Box(FVector(-2900.f, 3000.f, 1100.f), FVector(-2800.f, 5000.f, 1650.f), Steel);
		Box(FVector(-4100.f, 3000.f, 1100.f), FVector(-2900.f, 3300.f, 1650.f), Cabinet);
		Box(FVector(-4100.f, 4700.f, 1100.f), FVector(-2900.f, 5000.f, 1650.f), Steel);
		// Faucet
		Cyl(FVector(-3500.f, 4850.f, 1710.f), 60.f, 520.f, Steel);
		Box(FVector(-3550.f, 4350.f, 2160.f), FVector(-3450.f, 4850.f, 2240.f), Steel);
		Cyl(FVector(-3500.f, 4400.f, 2060.f), 40.f, 100.f, Steel, false);
		Cyl(FVector(-3750.f, 4800.f, 1710.f), 50.f, 120.f, Hex(200, 40, 40));
		Cyl(FVector(-3250.f, 4800.f, 1710.f), 50.f, 120.f, Hex(40, 80, 200));

		// Stove top
		Box(FVector(-1500.f, 3000.f, 1710.f), FVector(0.f, 5000.f, 1740.f), Black);
		const FVector Burners[4] = { FVector(-1150.f, 3500.f, 0.f), FVector(-350.f, 3500.f, 0.f), FVector(-1150.f, 4400.f, 0.f), FVector(-350.f, 4400.f, 0.f) };
		for (int32 i = 0; i < 4; ++i)
		{
			if (i == 1) { continue; }   // the front-right burner belongs to the stove knob gadget (coil, pot of soup, glow)
			Cyl(FVector(Burners[i].X, Burners[i].Y, 1740.f), 260.f, 25.f, DarkSteel);
			Cyl(FVector(Burners[i].X, Burners[i].Y, 1765.f), 120.f, 6.f, Black, false);
		}
		Box(FVector(-1450.f, 2950.f, 1560.f), FVector(-50.f, 2990.f, 1640.f), Steel, false);   // oven control strip
		// Generated oven shell around the hollow oven (the inside stays our box so ants can crawl in).
		AntArt::DressAuto(this, Root, TEXT("SM_Oven_Body"), FVector(-1500.f, 2990.f, 120.f), FVector(0.f, 4750.f, 1740.f), 0.f, 6.f, false);   // shell only: the oven stays hollow inside
		for (int32 i = 0; i < 4; ++i)
		{
			if (i == 1) { continue; }   // the gadget's knob
			Part(EAntShape::Cylinder, FVector(-1250.f + i * 330.f, 2945.f, 1600.f), FVector(70.f, 70.f, 30.f), FRotator(90.f, 0.f, 0.f), Black, false);
		}

		Doors(-2800.f, -1500.f, 2, CabinetDoor, Steel);
		Doors(1300.f, 2500.f, 2, CabinetDoor, Steel);
	}
	// The corridor behind the cabinets: dark, dusty, forgotten.
	for (int32 i = 0; i < 9; ++i)
	{
		const float X = -6600.f + i * 1050.f;
		Ball(FVector(X, 4880.f, 20.f), FVector(70.f + (i % 3) * 25.f, 60.f, 35.f), Hex(150, 145, 140));   // dust bunnies
	}
	Cyl(FVector(-2000.f, 4880.f, 0.f), 40.f, 1650.f, Hex(200, 200, 195));   // pipe in the corridor
	Box(FVector(-5000.f, 4790.f, 0.f), FVector(-4700.f, 4960.f, 25.f), Hex(240, 230, 200));   // a lost receipt
}

// ---------------------------------------------------------------------------------------------
// Fridge: hollow, on little feet (ants walk under it), pulled 250 away from the wall.
// The main door is a gadget (AKitchenDoor); the freezer up top is reached through a gap at the back.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildFridge()
{
	const FLinearColor Inside = Hex(225, 232, 238);
	const float Back = 4750.f;

	if (!bArt)   // modelled: the fridge (same cavities), its shelves and the food on them
	{
		for (const FVector F : { FVector(2780.f, 3180.f, 0.f), FVector(4620.f, 3180.f, 0.f), FVector(2780.f, 4670.f, 0.f), FVector(4620.f, 4670.f, 0.f) })
		{
			Cyl(F, 50.f, 120.f, DarkSteel);
		}
		Box(FVector(2700.f, 3100.f, 120.f), FVector(4700.f, Back, 200.f), Inside);
		Box(FVector(2700.f, 3100.f, 3620.f), FVector(4700.f, Back, 3700.f), White);
		Box(FVector(2700.f, 3100.f, 200.f), FVector(2780.f, Back, 3620.f), White);
		Box(FVector(4620.f, 3100.f, 200.f), FVector(4700.f, Back, 3620.f), White);
		Box(FVector(2780.f, 4670.f, 200.f), FVector(4620.f, Back, 3620.f), Inside);
		// Divider between fridge and freezer, with a gap at the back to sneak up.
		Box(FVector(2780.f, 3100.f, 2500.f), FVector(4620.f, 4450.f, 2560.f), Inside);
		Box(FVector(2780.f, 4450.f, 2500.f), FVector(3500.f, 4670.f, 2560.f), Inside);
		Box(FVector(3800.f, 4450.f, 2500.f), FVector(4620.f, 4670.f, 2560.f), Inside);
		// Freezer front (fixed) and handle.
		UStaticMeshComponent* FreezerFront = Box(FVector(2700.f, 3000.f, 2500.f), FVector(4700.f, 3100.f, 3700.f), White);
		UStaticMeshComponent* FreezerHandle = Box(FVector(2800.f, 2950.f, 2650.f), FVector(2860.f, 3000.f, 3400.f), Steel, false);
		// Generated fridge shell (outside only; the inside stays our boxes so ants can explore it).
		if (AntArt::DressAuto(this, Root, TEXT("SM_Fridge_Body"), FVector(2700.f, 2990.f, 0.f), FVector(4700.f, 4750.f, 3700.f), 0.f, 8.f, false))   // shell only: hollow inside
		{
			AntArt::HideAll({ FreezerHandle });
		}
		(void)FreezerFront;
		// Shelves (gap at the front so ants can climb past them).
		Box(FVector(2780.f, 3300.f, 900.f), FVector(4620.f, 4670.f, 930.f), Hex(200, 220, 230));
		Box(FVector(2780.f, 3300.f, 1700.f), FVector(4620.f, 4670.f, 1730.f), Hex(200, 220, 230));
		// Food inside.
		Box(FVector(2900.f, 4100.f, 200.f), FVector(3300.f, 4500.f, 900.f), Hex(250, 250, 250));      // milk carton
		Box(FVector(2900.f, 4100.f, 700.f), FVector(3300.f, 4500.f, 760.f), Hex(60, 120, 220), false);
		Cyl(FVector(4200.f, 4200.f, 200.f), 220.f, 380.f, Hex(240, 200, 120));                       // butter tub
		{   // jam jar, tipped over
			UStaticMeshComponent* J = Cyl(FVector(3600.f, 4000.f, 930.f), 170.f, 260.f, Hex(200, 30, 50));
			UStaticMeshComponent* Lid = Cyl(FVector(3600.f, 4000.f, 1190.f), 180.f, 40.f, Hex(230, 230, 230));
			if (AntArt::Dress(this, Root, TEXT("SM_JamJar"), FVector(3380.f, 3840.f, 930.f), FVector(3820.f, 4160.f, 1150.f), 0.f))
			{
				AntArt::HideAll({ J, Lid });
			}
		}
	}
	Ball(FVector(3600.f, 4000.f, 935.f), FVector(330.f, 300.f, 10.f), Hex(170, 20, 40));          // spilled jam
	if (!bArt)
	{
		Box(FVector(4000.f, 3600.f, 1730.f), FVector(4500.f, 4000.f, 1950.f), Hex(250, 210, 60));     // cheese block
		Cyl(FVector(3200.f, 3700.f, 1730.f), 260.f, 120.f, Hex(240, 240, 235));                       // plate of leftovers
		Cyl(FVector(3600.f, 4000.f, 2560.f), 330.f, 500.f, Hex(240, 220, 200));                       // ice cream tub (freezer)
		Box(FVector(2900.f, 3300.f, 2560.f), FVector(3200.f, 3600.f, 2700.f), Hex(200, 230, 255));    // ice tray
	}
	// Drip puddle under and behind the fridge (the wire behind sparks into it).
	Box(FVector(2900.f, 3700.f, 0.f), FVector(4500.f, 4950.f, 3.f), Hex(120, 170, 220), false);
	// Dust under the fridge.
	Ball(FVector(3000.f, 3500.f, 25.f), FVector(90.f, 70.f, 30.f), Hex(150, 145, 140));
	Ball(FVector(4300.f, 3900.f, 25.f), FVector(110.f, 80.f, 35.f), Hex(150, 145, 140));
}

// ---------------------------------------------------------------------------------------------
// Little stories around the kitchen: honey drip, jam, the mouse's secret room.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildSecrets()
{
	const FLinearColor Honey = Hex(235, 150, 20);

	// Honey jar tipped over on the shelf, dripping down the wall into a sticky puddle.
	if (!bArt)   // modelled: SM_K_JarHoney
	{
		UStaticMeshComponent* HoneyJar = Part(EAntShape::Cylinder, FVector(-6760.f, 260.f, 2780.f), FVector(340.f, 340.f, 300.f), FRotator(90.f, 0.f, 0.f), Hex(230, 150, 30), true);
		if (AntArt::Dress(this, Root, TEXT("SM_HoneyJar"), FVector(-6950.f, 70.f, 2680.f), FVector(-6560.f, 460.f, 3000.f), 0.f))
		{
			AntArt::HideAll({ HoneyJar });
		}
	}
	Box(FVector(-6950.f, 90.f, 2680.f), FVector(-6560.f, 430.f, 2686.f), Honey, false);
	Box(FVector(-7000.f, 190.f, 0.f), FVector(-6994.f, 360.f, 2680.f), Honey, false);
	Cyl(FVector(-6780.f, 280.f, 0.f), 280.f, 4.f, Honey, false);

	// Jam jar knocked over on the table.
	if (!bArt)   // modelled: SM_K_JarJam
	{
		UStaticMeshComponent* JamJar = Part(EAntShape::Cylinder, FVector(980.f, 520.f, 1530.f), FVector(240.f, 240.f, 260.f), FRotator(0.f, 20.f, 90.f), Hex(200, 30, 50), true);
		if (AntArt::Dress(this, Root, TEXT("SM_JamJar"), FVector(860.f, 360.f, 1400.f), FVector(1100.f, 680.f, 1610.f), 90.f))
		{
			AntArt::HideAll({ JamJar });
		}
	}

	// Cereal box on the counter (climbable, a prize on top).
	{
		UStaticMeshComponent* Shell = Box(FVector(-2700.f, 4150.f, 1710.f), FVector(-1800.f, 4650.f, 2030.f), Hex(230, 70, 40));
		if (Map4Look::Swap(Shell, TEXT("SM_M4_CerealBox")) && Map4Look::Prop(TEXT("SM_M4_CerealBox")))
		{
			Shell->SetMaterial(0, Map4Look::Prop(TEXT("SM_M4_CerealBox"))->GetMaterial(0));   // its printed box (map v4)
		}
		if (AntArt::Dress(this, Root, TEXT("SM_CerealBox"), FVector(-2700.f, 4150.f, 1710.f), FVector(-1800.f, 4650.f, 2030.f), 90.f))
		{
			AntArt::HideAll({ Shell });
		}
	}
	Cyl(FVector(610.f, 250.f, 1400.f), 290.f, 5.f, Hex(170, 20, 40), false);

	// Mouse hole (behind the fridge) and the room in the wall.
	Ball(FVector(3880.f, 4999.f, 140.f), FVector(80.f, 4.f, 60.f), Black);
	Box(FVector(3820.f, 4996.f, 0.f), FVector(3940.f, 5000.f, 140.f), Black, false);
	if (UStaticMeshComponent* Mb = Box(FVector(3550.f, 5250.f, 0.f), FVector(3800.f, 5500.f, 90.f), Hex(200, 50, 40)))        // matchbox bed
	{
		Mb->ComponentTags.AddUnique(TEXT("M4Arch"));   // a matchbox is a box
		if (const Map4Look::FLook* L = Map4Look::Enabled() ? Map4Look::Named(TEXT("Cardboard")) : nullptr) { Mb->SetMaterial(0, Map4Look::Get(*L, FLinearColor(1.1f, 0.35f, 0.3f))); }
	}
	Box(FVector(3560.f, 5260.f, 90.f), FVector(3790.f, 5490.f, 100.f), Hex(240, 230, 210), false);
	Cyl(FVector(4100.f, 5150.f, 0.f), 70.f, 20.f, Hex(190, 190, 180));                          // bottle cap table
	Cyl(FVector(3950.f, 5450.f, 0.f), 25.f, 120.f, Hex(250, 245, 230));                          // candle
	Ball(FVector(3950.f, 5450.f, 140.f), FVector(12.f, 12.f, 22.f), Hex(255, 200, 80));
	for (int32 i = 0; i < 5; ++i)   // stolen crumbs pile
	{
		Ball(FVector(4150.f + (i % 2) * 40.f, 5420.f - i * 25.f, 15.f + i * 6.f), FVector(30.f), Hex(200, 160, 90));
	}

	// Air vent ducts are gadgets (AKitchenPassage), but the light switch plate gets a dark cable track.
	Box(FVector(-6998.f, 1890.f, 2420.f), FVector(-6994.f, 1910.f, 5400.f), Hex(225, 220, 205), false);
}

// ---------------------------------------------------------------------------------------------
// Environment (server only): every zone and gadget gets a say in what happens to each ant.
// ---------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------
// Living room east of the kitchen (x 7200..20200, y -9000..1500): sofa with a dark space under it,
// coffee table, TV stand, a tall bookshelf to climb, armchair, plant, hot radiator.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildLivingRoom()
{
	const FLinearColor Fabric = Hex(70, 92, 125), FabricDark = Hex(52, 70, 98), Book[5] = { Hex(150, 40, 40), Hex(40, 90, 150), Hex(220, 190, 80), Hex(60, 120, 70), Hex(230, 230, 220) };
	// Shell: floor, ceiling, walls, baseboards.
	Box(FVector(7000.f, -9200.f, -100.f), FVector(20400.f, 1700.f, 0.f), Hex(196, 152, 104));   // parquet
	Box(FVector(7000.f, -9200.f, 5400.f), FVector(20400.f, 1700.f, 5600.f), Ceiling);
	Box(FVector(7200.f, 1500.f, 0.f), FVector(20400.f, 1700.f, 5400.f), WallC);       // north
	Box(FVector(20200.f, -9000.f, 0.f), FVector(20400.f, 1500.f, 5400.f), WallC);     // east
	if (bArt)
	{
		// Level v2: the south wall has a window (an opening, like the kitchen one) and the red crack under it: the fire ants' way in
		// from yard B. The west wall has the cat flap into yard A (SM_L_CatFlap lines the hole, its leaf is an AKitchenMover).
		SolidBox(FBox(FVector(7000.f, -9200.f, 0.f), FVector(20400.f, -9000.f, 5400.f)),
			{ FBox(FVector(16785.f, -9200.f, 2200.f), FVector(19785.f, -9000.f, 4200.f)), FBox(FVector(19400.f, -9250.f, 0.f), FVector(19600.f, -8950.f, 120.f)) }, WallC);
		SolidBox(FBox(FVector(7000.f, -9000.f, 0.f), FVector(7200.f, -5000.f, 5400.f)), { FBox(FVector(6950.f, -7600.f, 30.f), FVector(7250.f, -7100.f, 590.f)) }, WallC);
		// The flap's sill is 30 above the floor and its liner overhangs the wall by 10: an ant on the floor wedged under that lip
		// (seen in the game). A bevelled threshold on each side: 13 degrees up to the sill.
		Part(EAntShape::Cube, FVector(7277.f, -7350.f, 14.f), FVector(150.f, 470.f, 8.f), FRotator(-13.f, 0.f, 0.f), Hex(235, 232, 222), true);
		Part(EAntShape::Cube, FVector(6923.f, -7350.f, 14.f), FVector(150.f, 470.f, 8.f), FRotator(13.f, 0.f, 0.f), Hex(235, 232, 222), true);
		// The TV stand's left bay (one of the three places of the birthday cake): the guide rail of its sliding doors stands
		// 12 uu proud of the bay's floor (z 100) and stopped a big load dead (seen in the game: four ants could not get the
		// cake over it). A threshold board over the rail: a ramp up to it from inside, flat across the two door tracks to the
		// front edge. The doors' bottom edges (z 103) sink a few uu into it.
		Part(EAntShape::Cube, FVector(11790.f, -7834.f, 106.f), FVector(2650.f, 142.f, 6.f), FRotator(0.f, 0.f, -5.5f), Hex(96, 66, 44), true);
		Box(FVector(10465.f, -7766.f, 107.f), FVector(13115.f, -7702.f, 113.f), Hex(96, 66, 44));
	}
	else
	{
		Box(FVector(7000.f, -9200.f, 0.f), FVector(20400.f, -9000.f, 5400.f), WallC);     // south
		Box(FVector(7000.f, -9000.f, 0.f), FVector(7200.f, -5000.f, 5400.f), WallC);      // west (below the kitchen)
	}
	if (!bArt)   // modelled: SM_L_Skirting
	{
		Box(FVector(7200.f, 1470.f, 0.f), FVector(20200.f, 1500.f, 220.f), Baseboard);
		Box(FVector(7200.f, -9000.f, 0.f), FVector(20200.f, -8970.f, 220.f), Baseboard);
		Box(FVector(20170.f, -9000.f, 0.f), FVector(20200.f, 1500.f, 220.f), Baseboard);
	}

	if (!bArt)   // modelled: carpet, sofa with cushions and pillows, coffee table and what lies on it, TV stand, TV, console
	{
		// Rug.
		Box(FVector(10500.f, -6800.f, 0.f), FVector(17500.f, -1300.f, 4.f), Hex(170, 40, 50));

		// Sofa on legs along the north wall: ants run underneath, where lost treasure lies in the dark.
		for (const FVector L : { FVector(10600.f, -200.f, 0.f), FVector(16400.f, -200.f, 0.f), FVector(10600.f, 1300.f, 0.f), FVector(16400.f, 1300.f, 0.f) })
		{
			Cyl(L, 70.f, 260.f, DarkWood);
		}
		Box(FVector(10200.f, -300.f, 260.f), FVector(16800.f, 1400.f, 700.f), FabricDark);              // base
		for (int32 i = 0; i < 3; ++i)                                                                       // seat cushions
		{
			const float X0 = 10500.f + i * 2000.f;
			Box(FVector(X0 + 20.f, -260.f, 700.f), FVector(X0 + 1980.f, 1000.f, 950.f), Fabric);
		}
		Box(FVector(10200.f, 1000.f, 700.f), FVector(16800.f, 1400.f, 2100.f), FabricDark);              // backrest
		Box(FVector(9800.f, -300.f, 260.f), FVector(10200.f, 1400.f, 1350.f), FabricDark);               // armrests
		Box(FVector(16800.f, -300.f, 260.f), FVector(17200.f, 1400.f, 1350.f), FabricDark);
		Ball(FVector(12200.f, 900.f, 1300.f), FVector(380.f, 160.f, 330.f), Hex(230, 180, 70));            // throw pillows
		Ball(FVector(15600.f, 880.f, 1300.f), FVector(360.f, 150.f, 320.f), Hex(200, 80, 60));
		for (int32 i = 0; i < 5; ++i)                                                                       // dust under the sofa
		{
			Ball(FVector(11000.f + i * 1200.f, 600.f + (i % 2) * 300.f, 25.f), FVector(80.f, 60.f, 30.f), Hex(150, 145, 140));
		}

		// Coffee table with a lower shelf (magazines) - snacks on top.
		for (const FVector L : { FVector(12600.f, -4100.f, 0.f), FVector(14900.f, -4100.f, 0.f), FVector(12600.f, -2700.f, 0.f), FVector(14900.f, -2700.f, 0.f) })
		{
			Cyl(L, 60.f, 900.f, Wood);
		}
		Box(FVector(12500.f, -4200.f, 900.f), FVector(15000.f, -2600.f, 980.f), Wood);
		Box(FVector(12550.f, -4150.f, 300.f), FVector(14950.f, -2650.f, 340.f), Wood);
		Box(FVector(12900.f, -3900.f, 340.f), FVector(13900.f, -3100.f, 380.f), Hex(200, 60, 60));          // magazines
		Box(FVector(13000.f, -3800.f, 380.f), FVector(13950.f, -3150.f, 400.f), Hex(240, 230, 210));
		Cyl(FVector(14300.f, -3500.f, 980.f), 330.f, 160.f, White);                                         // snack bowl
		Cyl(FVector(13100.f, -3200.f, 980.f), 120.f, 380.f, Hex(200, 60, 40));                              // soda can
		Box(FVector(12700.f, -4050.f, 980.f), FVector(13300.f, -3750.f, 1010.f), Black);                    // remote

		// TV stand (hollow: ants go inside) and the TV.
		Box(FVector(11000.f, -9000.f, 0.f), FVector(16500.f, -7700.f, 80.f), DarkWood);
		Box(FVector(11000.f, -9000.f, 1050.f), FVector(16500.f, -7700.f, 1100.f), DarkWood);
		Box(FVector(11000.f, -9000.f, 80.f), FVector(11080.f, -7700.f, 1050.f), DarkWood);
		Box(FVector(16420.f, -9000.f, 80.f), FVector(16500.f, -7700.f, 1050.f), DarkWood);
		Box(FVector(13700.f, -9000.f, 80.f), FVector(13780.f, -7700.f, 1050.f), DarkWood);
		Box(FVector(11800.f, -8900.f, 1100.f), FVector(15700.f, -8750.f, 3300.f), Black);                   // TV
		Box(FVector(11850.f, -8752.f, 1150.f), FVector(15650.f, -8748.f, 3250.f), Hex(40, 70, 130), false);  // glowing screen
		Box(FVector(13500.f, -8900.f, 1100.f), FVector(14000.f, -8500.f, 1180.f), Black);                   // TV foot
		Box(FVector(14300.f, -8600.f, 1100.f), FVector(14900.f, -8200.f, 1250.f), Hex(30, 30, 35));         // game console
	}
	Lamp(FVector(13750.f, -8000.f, 2200.f), 3.f, FLinearColor(0.4f, 0.6f, 1.f), 3000.f, false);         // TV light

	if (!bArt)   // modelled: bookshelf with books and vase, armchair, floor lamp
	{
		// Tall bookshelf on the east wall: shelves to climb, books, a vase on top.
		Box(FVector(20100.f, -7100.f, 0.f), FVector(20200.f, -1900.f, 4600.f), DarkWood);                   // back
		Box(FVector(19200.f, -7100.f, 0.f), FVector(20200.f, -7000.f, 4600.f), DarkWood);                   // sides
		Box(FVector(19200.f, -2000.f, 0.f), FVector(20200.f, -1900.f, 4600.f), DarkWood);
		Box(FVector(19200.f, -4540.f, 0.f), FVector(20200.f, -4460.f, 4600.f), DarkWood);                   // middle divider
		for (int32 k = 0; k <= 5; ++k)
		{
			const float Z = k * 900.f + (k == 5 ? 100.f : 0.f);
			Box(FVector(19200.f, -7000.f, Z), FVector(20100.f, -2000.f, Z + 60.f), DarkWood);
			if (k < 5)
			{
				FRandomStream BR(77 + k);
				float Y = -6950.f;
				while (Y < -2200.f)
				{
					const float W = BR.FRandRange(90.f, 220.f);
					const float H = BR.FRandRange(450.f, 780.f);
					if (BR.FRand() < 0.15f) { Y += 300.f; continue; }   // gaps to squeeze through
					if (Y + W > -4540.f && Y < -4460.f) { Y = -4440.f; continue; }
					Box(FVector(19350.f, Y, Z + 60.f), FVector(20050.f, Y + W, Z + 60.f + H), Book[BR.RandRange(0, 4)]);
					Y += W + 8.f;
				}
			}
		}
		Cyl(FVector(19700.f, -5600.f, 4660.f), 220.f, 700.f, Hex(70, 130, 160));                          // vase

		// Armchair.
		Box(FVector(8500.f, -6700.f, 0.f), FVector(10000.f, -4900.f, 650.f), FabricDark);
		Box(FVector(8550.f, -6650.f, 650.f), FVector(9950.f, -4950.f, 800.f), Fabric);
		Box(FVector(8300.f, -6700.f, 0.f), FVector(8500.f, -4900.f, 1900.f), FabricDark);
		Box(FVector(8500.f, -6900.f, 0.f), FVector(10000.f, -6700.f, 1200.f), FabricDark);
		Box(FVector(8500.f, -4900.f, 0.f), FVector(10000.f, -4700.f, 1200.f), FabricDark);

		// Floor lamp and a potted plant in the north-east corner.
		Cyl(FVector(17800.f, 700.f, 0.f), 260.f, 60.f, DarkSteel);
		Cyl(FVector(17800.f, 700.f, 60.f), 35.f, 3800.f, DarkSteel);
		Cyl(FVector(17800.f, 700.f, 3700.f), 420.f, 500.f, Hex(240, 225, 190), false);
	}
	Lamp(FVector(17800.f, 700.f, 3600.f), 5.f, FLinearColor(1.f, 0.8f, 0.55f), 4500.f, true);
	if (!bArt)   // modelled: potted plant, radiator
	{
		Cyl(FVector(18900.f, 900.f, 0.f), 450.f, 780.f, Hex(170, 90, 60));
		Cyl(FVector(18900.f, 900.f, 700.f), 420.f, 80.f, Dirt);
		for (int32 i = 0; i < 7; ++i)
		{
			const float A = i * 0.9f;
			Ball(FVector(18900.f + FMath::Cos(A) * 260.f, 900.f + FMath::Sin(A) * 260.f, 1300.f + (i % 3) * 300.f), FVector(240.f, 120.f, 60.f), Hex(60, 130, 60));
		}

		// Radiator under where a window would be (north wall): hot!
		for (int32 i = 0; i < 9; ++i)
		{
			Box(FVector(7800.f + i * 200.f, 1300.f, 300.f), FVector(7920.f + i * 200.f, 1470.f, 1400.f), White);
		}
		Box(FVector(7800.f, 1300.f, 300.f), FVector(9520.f, 1470.f, 360.f), White);
	}
	// Main lights of the living room (they follow the switch).
	MainLamps.Add(Lamp(FVector(11000.f, -4000.f, 4950.f), 7.f, FLinearColor(1.f, 0.92f, 0.8f), 15000.f, true));
	MainLamps.Add(Lamp(FVector(16500.f, -4000.f, 4950.f), 7.f, FLinearColor(1.f, 0.92f, 0.8f), 15000.f, true));
	if (!bArt)   // the modelled lamps join LampShades in BuildArt
	{
		LampShades.Add(Cyl(FVector(11000.f, -4000.f, 5150.f), 350.f, 250.f, White, false));
		LampShades.Add(Cyl(FVector(16500.f, -4000.f, 5150.f), 350.f, 250.f, White, false));
	}
}

void AKitchenBuilder::EnvironmentFor(const AAntCharacter* Ant, FAntEnv& Env)
{
	if (AntLab::Get())
	{
		AntLab::EnvironmentFor(Ant, Env);   // the lab: the variant's zones (heat, cold, water, steam)
		return;
	}
	if (const UAntFluidWorld* Fluid = UAntFluidWorld::Get(Ant)) { Fluid->AffectAnt(Ant, Env); }   // the liquid prototype (-AntFluid)
	const FVector P = Ant->GetActorLocation();
	const bool bV2 = KitchenArt::Enabled();
	// The radiator. Level v2: it hangs under the new living-room window in the south wall.
	if ((bV2 ? FBox(FVector(17345.f, -9000.f, 250.f), FVector(19245.f, -8650.f, 1500.f)) : FBox(FVector(7700.f, 1150.f, 250.f), FVector(9600.f, 1500.f, 1500.f))).IsInside(P))
	{
		Env.Temp(70.f, 12.f);
	}
	if (bV2)
	{
		// The new spills (jam by the high chair, the fly ribbon, the coffee on the pavement; the puddles under the trap and by the sink).
		const int32 Zone = LevelV2::ZoneAt(P);
		if (Zone == 0) { Env.Flags |= AntStatus::Sticky; }
		else if (Zone == 1) { Env.Flags |= AntStatus::Wet; }
		// Level v3: the shards of a broken jar, cup or picture frame cut (10 a second, "порезан").
		if (LevelV3::Enabled() && LevelV3::ShardsAt(P)) { Env.Hurt(LevelV3::ShardDps(), TEXT("порезан осколками")); }
		// The yard sprinkler: every 26 s it waters a circle of the lawn for 9 s. Wet and cold: slow going, like the tap's stream.
		if (P.Z < 400.f && P.Y < -5200.f && FVector::DistSquared2D(P, LevelV2::Sprinkler()) < FMath::Square(LevelV2::SprinklerRadius()))
		{
			const AGameStateBase* GS = Ant->GetWorld()->GetGameState();
			if (LevelV2::SprinklerLevel(GS ? GS->GetServerWorldTimeSeconds() : Ant->GetWorld()->GetTimeSeconds()) > 0.2f)
			{
				Env.Flags |= AntStatus::Wet;
				Env.Temp(14.f, 6.f);
			}
		}
	}

	// (the front burner's heat comes from the stove knob gadget: only when it is switched on)

	static const FBox Sticky[] =
	{
		FBox(FVector(-7000.f, 150.f, 0.f), FVector(-6930.f, 400.f, 2600.f)),       // honey dripping down the west wall
		FBox(FVector(-7000.f, 0.f, -50.f), FVector(-6450.f, 560.f, 70.f)),         // honey puddle on the floor
		FBox(FVector(-6950.f, 80.f, 2660.f), FVector(-6560.f, 440.f, 2760.f)),     // honey on the shelf
		FBox(FVector(280.f, -60.f, 1380.f), FVector(940.f, 560.f, 1470.f)),        // jam on the table
		FBox(FVector(3300.f, 3700.f, 900.f), FVector(3900.f, 4300.f, 990.f)),      // jam spilled in the fridge
		FBox(FVector(-10180.f, 2320.f, 0.f), FVector(-9820.f, 2680.f, 60.f)),       // chewing gum on the sidewalk
		FBox(FVector(-10780.f, -1880.f, 0.f), FVector(-10020.f, -1120.f, 50.f)),    // melted ice cream puddle
	};
	for (const FBox& B : Sticky)
	{
		if (B.IsInside(P))
		{
			Env.Flags |= AntStatus::Sticky;
		}
	}

	// Cat water bowl and the mop bucket.
	if (FVector::Dist2D(P, FVector(-2500.f, -3900.f, 0.f)) < 380.f && P.Z > 110.f && P.Z < 230.f)
	{
		Env.Flags |= AntStatus::Wet;
		Env.Temp(16.f, 6.f);
	}
	if (FVector::Dist2D(P, FVector(6200.f, 3900.f, 0.f)) < 305.f && P.Z > 600.f && P.Z < 720.f)
	{
		Env.Flags |= AntStatus::Wet;
		Env.Temp(15.f, 6.f);
	}
}

void AKitchenBuilder::SetMainLights(bool bOn)
{
	bMainOn = bOn;
	if (bLab)
	{
		return;   // the lab's grey box has its own plain lights
	}
	ApplyLights();
}

// ---------------------------------------------------------------------------------------------
// Time of day. Every shift has its own light: 1 morning, 2 golden evening, 3 night with the lamps on, then round
// again. All of it is local (this actor is built on every machine); the only input is the replicated shift number.
// ---------------------------------------------------------------------------------------------

AKitchenBuilder::FTimeOfDay AKitchenBuilder::TimeOfDay(int32 Index)
{
	FTimeOfDay T;
	switch (Index)
	{
	default:   // morning: a clean sun from the south-east, a long strip of light across the west floor up to the counters
		T.SunRot = FRotator(-26.f, 105.f, 0.f);
		T.SunColor = FLinearColor(1.f, 0.93f, 0.80f);
		T.SunLux = 9.f;
		T.SunShafts = 26.f;
		T.Sky = 1.25f;
		T.Lamps = 0.22f;
		T.Practicals = 0.35f;
		T.Amb = 0.40f;
		T.AmbColor = FLinearColor(1.f, 0.95f, 0.86f);
		T.Bounce = 0.32f;
		T.BounceColor = FLinearColor(1.f, 0.9f, 0.76f);
		T.WindowColor = FLinearColor(0.78f, 0.88f, 1.f);
		T.Window = 2.5f;
		T.FogDensity = 0.004f;
		T.ExposureBias = -0.35f;
		T.WhiteTemp = 6800.f;
		T.FxAmbient = 1.f;
		T.FxTint = FLinearColor(1.f, 0.97f, 0.92f);
		break;
	case 1:    // golden evening: a low sun from the south-west, under the table and across the east floor to the fridge
		T.SunRot = FRotator(-18.f, 50.f, 0.f);
		T.SunColor = FLinearColor(1.f, 0.78f, 0.5f);
		T.SunLux = 13.f;
		T.SunShafts = 30.f;
		T.Sky = 1.1f;
		T.Lamps = 0.2f;
		T.Practicals = 0.8f;
		T.Amb = 0.34f;
		T.AmbColor = FLinearColor(1.f, 0.7f, 0.42f);
		T.Bounce = 0.38f;
		T.BounceColor = FLinearColor(1.f, 0.62f, 0.35f);
		T.WindowColor = FLinearColor(1.f, 0.66f, 0.38f);
		T.Window = 3.2f;
		T.FogDensity = 0.005f;
		T.ExposureBias = -0.5f;
		T.WhiteTemp = 7700.f;
		T.FxAmbient = 0.85f;
		T.FxTint = FLinearColor(1.f, 0.86f, 0.7f);
		break;
	case 2:    // night: moonlight through the window, the lamps carry the room
		T.SunRot = FRotator(-38.f, 108.f, 0.f);
		T.SunColor = FLinearColor(0.55f, 0.68f, 1.f);
		T.SunLux = 0.7f;
		T.SunShafts = 45.f;
		T.Sky = 1.f;
		T.Lamps = 0.12f;          // the ceiling lamps only glimmer: the pendant, the hood, the fridge and the TV carry the night
		T.Practicals = 1.f;
		T.Amb = 0.55f;
		T.AmbColor = FLinearColor(0.42f, 0.55f, 1.f);
		T.Bounce = 0.26f;
		T.BounceColor = FLinearColor(0.62f, 0.7f, 1.f);
		T.WindowColor = FLinearColor(0.4f, 0.52f, 1.f);
		T.Window = 1.f;
		T.FogDensity = 0.003f;
		T.ExposureBias = -1.4f;
		T.WhiteTemp = 6100.f;
		T.FxAmbient = 0.35f;
		T.FxTint = FLinearColor(0.8f, 0.88f, 1.f);
		break;
	}
	if (LevelV3::Enabled())
	{
		// Level v3 (docs/level/LEVEL_V3.md, v3.time_of_day): three clearly different evenings of the same flat.
		switch (Index)
		{
		default:   // day, 14:00: a high, white sun from the south, a bright sky in the windows; the ceiling lamps off
			T.SunRot = FRotator(-50.f, 96.f, 0.f);
			T.SunColor = FLinearColor(1.f, 0.97f, 0.9f);
			T.SunLux = 10.f;
			T.SunShafts = 18.f;
			T.Sky = 1.5f;
			T.Lamps = 0.f;
			T.Practicals = 0.12f;
			T.Amb = 0.42f;
			T.AmbColor = FLinearColor(0.96f, 0.97f, 1.f);
			T.Bounce = 0.34f;
			T.BounceColor = FLinearColor(1.f, 0.95f, 0.88f);
			T.WindowColor = FLinearColor(0.86f, 0.93f, 1.f);
			T.Window = 3.f;
			T.ExposureBias = -0.25f;
			T.WhiteTemp = 6400.f;
			T.FxAmbient = 1.f;
			T.FxTint = FLinearColor(1.f, 0.99f, 0.96f);
			break;
		case 1:    // evening, 19:30: the sun nearly down, deep orange shafts low across the floor; the lamps are on and warm
			T.SunRot = FRotator(-9.f, 40.f, 0.f);
			T.SunColor = FLinearColor(1.f, 0.52f, 0.22f);
			T.SunLux = 8.f;
			T.SunShafts = 42.f;
			T.Sky = 0.55f;
			T.Lamps = 0.9f;
			T.Practicals = 1.f;
			T.Amb = 0.3f;
			T.AmbColor = FLinearColor(1.f, 0.66f, 0.4f);
			T.Bounce = 0.3f;
			T.BounceColor = FLinearColor(1.f, 0.58f, 0.32f);
			T.WindowColor = FLinearColor(1.f, 0.48f, 0.26f);
			T.Window = 2.2f;
			T.FogDensity = 0.006f;
			T.ExposureBias = -0.3f;
			T.WhiteTemp = 8300.f;
			T.FxAmbient = 0.8f;
			T.FxTint = FLinearColor(1.f, 0.8f, 0.6f);
			break;
		case 2:    // night, 23:00: blue moonlight through the window, the ceiling lamps off; the hood light, the TV and an open fridge carry the room
			T.SunRot = FRotator(-34.f, 112.f, 0.f);
			T.SunColor = FLinearColor(0.48f, 0.6f, 1.f);
			T.SunLux = 0.8f;
			T.SunShafts = 60.f;
			T.Sky = 0.16f;
			T.Lamps = 0.f;
			T.Practicals = 0.45f;
			T.Amb = 0.2f;
			T.AmbColor = FLinearColor(0.34f, 0.44f, 1.f);
			T.Bounce = 0.1f;
			T.BounceColor = FLinearColor(0.5f, 0.58f, 1.f);
			T.WindowColor = FLinearColor(0.34f, 0.46f, 1.f);
			T.Window = 1.2f;
			T.FogDensity = 0.003f;
			T.ExposureBias = -1.f;
			T.WhiteTemp = 5600.f;
			T.FxAmbient = 0.3f;
			T.FxTint = FLinearColor(0.7f, 0.8f, 1.f);
			break;
		}
	}
	if (Map4Look::Enabled() && Index == 0)
	{
		// map v4 day: a lower, stronger sun that lays long shafts across the floor, less flat fill from the sky
		T.SunRot = FRotator(-38.f, 100.f, 0.f);
		T.SunLux = 13.f;
		T.SunShafts = 34.f;
		T.Sky = 1.05f;
		T.Amb = 0.28f;
		T.Bounce = 0.26f;
		T.ExposureBias = -0.35f;
	}
	if (Map4Look::Enabled() && Index == 1)
	{
		// map v4 evening: the same low sun, but whites stay warm white rather than orange (the wallpapers and wood carry the warmth)
		T.SunColor = FLinearColor(1.f, 0.66f, 0.38f);
		T.AmbColor = FLinearColor(1.f, 0.84f, 0.68f);
		T.BounceColor = FLinearColor(1.f, 0.76f, 0.56f);
		T.WindowColor = FLinearColor(1.f, 0.62f, 0.42f);
		T.WhiteTemp = 7000.f;
		T.ExposureBias = -0.1f;
	}
	return T;
}

AKitchenBuilder::FTimeOfDay AKitchenBuilder::Blend(const FTimeOfDay& A, const FTimeOfDay& B, float K)
{
	FTimeOfDay T;
	T.SunRot = FMath::Lerp(A.SunRot, B.SunRot, K);
	T.SunColor = FMath::Lerp(A.SunColor, B.SunColor, K);
	T.SunLux = FMath::Lerp(A.SunLux, B.SunLux, K);
	T.SunShafts = FMath::Lerp(A.SunShafts, B.SunShafts, K);
	T.Sky = FMath::Lerp(A.Sky, B.Sky, K);
	T.Lamps = FMath::Lerp(A.Lamps, B.Lamps, K);
	T.Practicals = FMath::Lerp(A.Practicals, B.Practicals, K);
	T.Amb = FMath::Lerp(A.Amb, B.Amb, K);
	T.AmbColor = FMath::Lerp(A.AmbColor, B.AmbColor, K);
	T.Bounce = FMath::Lerp(A.Bounce, B.Bounce, K);
	T.BounceColor = FMath::Lerp(A.BounceColor, B.BounceColor, K);
	T.WindowColor = FMath::Lerp(A.WindowColor, B.WindowColor, K);
	T.Window = FMath::Lerp(A.Window, B.Window, K);
	T.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, K);
	T.ExposureBias = FMath::Lerp(A.ExposureBias, B.ExposureBias, K);
	T.WhiteTemp = FMath::Lerp(A.WhiteTemp, B.WhiteTemp, K);
	T.FxAmbient = FMath::Lerp(A.FxAmbient, B.FxAmbient, K);
	T.FxTint = FMath::Lerp(A.FxTint, B.FxTint, K);
	return T;
}

void AKitchenBuilder::UpdateTimeOfDay(float Dt)
{
	int32 Want = 0;
	if (ForcedTod >= 0)
	{
		Want = ForcedTod;
	}
	else if (LevelV3::Enabled() && LevelV3::Current())
	{
		Want = FMath::Clamp(LevelV3::TimeOfDay(), 0, 2);   // level v3: the raid seed's time of day (day 14:00, evening 19:30, night 23:00)
	}
	else if (const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>())
	{
		Want = GS->bHeistMode ? TimeOfDayFor(GS->Shift) : 0;
	}
	if (TodIndex < 0)
	{
		// First time: no fade.
		TodIndex = Want;
		Tod = TodFrom = TimeOfDay(Want);
		TodBlend = 1.f;
		ApplyLights();
		return;
	}
	if (Want != TodIndex)
	{
		TodIndex = Want;
		TodFrom = Tod;
		TodBlend = 0.f;
	}
	if (TodBlend < 1.f)
	{
		TodBlend = FMath::Min(1.f, TodBlend + Dt / 6.f);
		Tod = Blend(TodFrom, TimeOfDay(TodIndex), FMath::SmoothStep(0.f, 1.f, TodBlend));
		ApplyLights();
	}
}

void AKitchenBuilder::ApplyLights()
{
	if (MainLampBase.Num() != MainLamps.Num())
	{
		MainLampBase.Reset();
		for (const UPointLightComponent* L : MainLamps)
		{
			MainLampBase.Add(L ? L->Intensity : 0.f);
		}
	}
	const float Lamps = bMainOn ? Tod.Lamps : 0.f;
	for (int32 i = 0; i < MainLamps.Num(); ++i)
	{
		if (UPointLightComponent* L = MainLamps[i])
		{
			L->SetVisibility(Lamps > 0.01f);
			L->SetIntensity(MainLampBase[i] * Lamps);
			L->SetLightColor(FLinearColor(1.f, 0.86f, 0.68f));   // warm bulbs
		}
	}
	for (UStaticMeshComponent* S : LampShades)
	{
		if (UMaterialInstanceDynamic* M = S ? Cast<UMaterialInstanceDynamic>(S->GetMaterial(0)) : nullptr)
		{
			M->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(0.2f, 0.2f, 0.2f), FLinearColor(3.f, 2.8f, 2.4f), FMath::Clamp(Lamps * 1.4f, 0.f, 1.f)));
		}
	}
	// The pendant follows the switch; the hood light is its own little switch and stays on.
	const float PendantK = bMainOn ? Tod.Practicals : 0.f;
	if (Pendant)
	{
		Pendant->SetVisibility(PendantK > 0.01f);
		Pendant->SetIntensity(18.f * PendantK);
	}
	if (PendantBulb)
	{
		PendantBulb->SetVectorParameterValue(TEXT("Color"), FLinearColor(6.f, 4.4f, 2.2f) * FMath::Max(0.03f, PendantK));
	}
	if (HoodLight)
	{
		HoodLight->SetIntensity(5.f * Tod.Practicals);
	}
	if (HoodBulb)
	{
		HoodBulb->SetVectorParameterValue(TEXT("Color"), FLinearColor(5.f, 4.2f, 2.8f) * Tod.Practicals);
	}
	const float RoomDim = bMainOn ? 1.f : 0.6f;   // lamps off: less light bouncing around
	for (UPointLightComponent* L : AmbFills)
	{
		if (L)
		{
			L->SetLightColor(Tod.AmbColor);
			L->SetIntensity(Tod.Amb * RoomDim);
		}
	}
	for (UPointLightComponent* L : BounceFills)
	{
		if (L)
		{
			L->SetLightColor(Tod.BounceColor);
			L->SetIntensity(Tod.Bounce * RoomDim);
		}
	}
	if (Fill)
	{
		Fill->SetRelativeRotation(Tod.SunRot);
		Fill->SetLightColor(Tod.SunColor);
		Fill->SetIntensity(Tod.SunLux);
		Fill->SetVolumetricScatteringIntensity(Tod.SunShafts);
	}
	if (SkyFill)
	{
		SkyFill->SetIntensity(Tod.Sky);
	}
	// The window glow is daylight (or moonlight): with the lamps off it is toned down so the room reads as dark.
	if (WindowLight)
	{
		WindowLight->SetLightColor(Tod.WindowColor);
		WindowLight->SetIntensity(Tod.Window * (bMainOn ? 1.f : 0.45f));
	}
	if (Haze)
	{
		Haze->SetFogDensity(Tod.FogDensity);
	}
	if (TodGrade)
	{
		FPostProcessSettings& G = TodGrade->Settings;
		G.bOverride_AutoExposureBias = true;
		G.AutoExposureBias = Tod.ExposureBias - (bMainOn ? 0.f : 0.7f);   // switch off: the room visibly drops, even by day
		G.bOverride_WhiteTemp = true;
		G.WhiteTemp = Tod.WhiteTemp;
		if (Map4Look::Enabled())
		{
			// map v4 grade: soft ambient occlusion in the corners and under things, a touch of vignette, gentler saturation
			G.bOverride_AmbientOcclusionIntensity = true; G.AmbientOcclusionIntensity = 0.9f;
			G.bOverride_AmbientOcclusionRadius = true; G.AmbientOcclusionRadius = 120.f;
			G.bOverride_VignetteIntensity = true; G.VignetteIntensity = 0.35f;
			G.bOverride_ColorSaturation = true; G.ColorSaturation = FVector4(0.95f, 0.95f, 0.95f, 1.f);
			G.bOverride_ColorContrast = true; G.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.f);
			G.bOverride_BloomIntensity = true; G.BloomIntensity = 0.45f;
		}
	}
	AntFx::SetAmbient(this, Tod.FxAmbient * (bMainOn ? 1.f : 0.7f), Tod.FxTint);
}

void AKitchenBuilder::AddMound(int32 Team, UStaticMeshComponent* C)
{
	if (C)
	{
		C->SetMobility(EComponentMobility::Movable);   // grows during the match
		Mounds(Team).Add(C);
		MoundBaseScale[Team].Add(C->GetRelativeScale3D());
		MoundBaseLoc[Team].Add(C->GetRelativeLocation());
	}
}

void AKitchenBuilder::UpdateMounds(float Dt)
{
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	const float Target = FMath::Max(1.f, (float)GS->TargetScore);
	const float Want[2] = { FMath::Clamp(GS->TeamScore / Target, 0.f, 1.f), FMath::Clamp(GS->RivalScore / Target, 0.f, 1.f) };
	for (int32 T = 0; T < 2; ++T)
	{
		MoundGrowth[T] = FMath::FInterpTo(MoundGrowth[T], Want[T], Dt, 0.8f);
		// Up to ~2x wider and ~2.6x taller at the end of the match.
		const FVector K(1.f + MoundGrowth[T], 1.f + MoundGrowth[T], 1.f + 1.6f * MoundGrowth[T]);
		const FVector Home = NestLocation(T);
		for (int32 i = 0; i < Mounds(T).Num(); ++i)
		{
			const FVector Base = MoundBaseLoc[T][i];
			const FVector Off = Base - FVector(Home.X, Home.Y, 0.f);
			Mounds(T)[i]->SetRelativeLocation(FVector(Home.X, Home.Y, 0.f) + FVector(Off.X * (0.6f + 0.4f * K.X), Off.Y * (0.6f + 0.4f * K.Y), Off.Z + MoundGrowth[T] * 120.f));
			Mounds(T)[i]->SetRelativeScale3D(MoundBaseScale[T][i] * K);
		}
	}
}

void AKitchenBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bLab)
	{
		return;   // the lab: nothing of the kitchen's life (time of day, drips, mounds) runs
	}
	FlatBoxTimer -= DeltaSeconds;
	if (FlatBoxTimer <= 0.f)
	{
		FlatBoxTimer = 5.f;   // map v4: frames and outlets of new or reset fixtures get their grip box too
		Map4Look::BoxWallFlats(GetWorld());
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateMounds(DeltaSeconds);
		UpdateTimeOfDay(DeltaSeconds);
		// The fridge drips into the puddle behind it: a drop, then a little splash and rings.
		const FVector DripAt(3700.f, 4300.f, 3.f);
		DripTimer -= DeltaSeconds;
		if (DripTimer <= 0.f)
		{
			DripTimer = FMath::FRandRange(1.6f, 3.2f);
			DripRipple = 0.39f;
			if (AAntFxManager* Fx = AAntFxManager::Get(this))
			{
				FAntFxBit D;
				D.Pos = DripAt + FVector(0.f, 0.f, 112.f);
				D.Gravity = 1500.f;
				D.Scale0 = FVector(0.07f);
				D.Scale1 = FVector(0.07f, 0.07f, 0.12f);
				D.Color = FLinearColor(0.78f, 0.9f, 1.f, 0.75f);
				D.Life = 0.39f;
				D.FadeIn = 0.f;
				D.FadePow = 0.1f;
				D.FloorZ = DripAt.Z;
				Fx->Emit(D);
			}
		}
		if (DripRipple > 0.f)
		{
			DripRipple -= DeltaSeconds;
			if (DripRipple <= 0.f)
			{
				AntFx::Splash(this, DripAt + FVector(0.f, 0.f, 1.f), 3.f);
			}
		}
		// Level v2: the yard sprinkler. A jet sweeps round the head; where it lands the lawn is wet (EnvironmentFor).
		if (bArt)
		{
			const AGameStateBase* GS = GetWorld()->GetGameState();
			const double ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
			const float Level = LevelV2::SprinklerLevel(ServerNow);
			SprinkleTimer -= DeltaSeconds;
			if (Level > 0.f && SprinkleTimer <= 0.f)
			{
				SprinkleTimer = 0.07f;
				const FVector Head = LevelV2::Sprinkler() + FVector(0.f, 0.f, 190.f);
				const float Arm = (float)FMath::Fmod(ServerNow * 2.2, 2.0 * PI);
				AAntFxManager* Fx = AAntFxManager::Get(this);
				for (int32 i = 0; i < 4; ++i)
				{
					const float A = Arm + FMath::FRandRange(-0.25f, 0.25f) + (i % 2) * PI;
					const float R = FMath::Sqrt(FMath::FRand()) * LevelV2::SprinklerRadius() * Level;
					const FVector Land = LevelV2::Sprinkler() + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 2.f);
					AntFx::Splash(this, Land, 3.f + 2.f * FMath::FRand());
					if (Fx)
					{
						// a drop on its way from the head to where it lands
						FAntFxBit D;
						D.Pos = Head;
						D.Vel = (Land - Head) / 0.55f + FVector(0.f, 0.f, 0.5f * 1500.f * 0.55f);
						D.Gravity = 1500.f;
						D.Scale0 = FVector(0.09f);
						D.Scale1 = FVector(0.07f, 0.07f, 0.14f);
						D.Color = FLinearColor(0.78f, 0.9f, 1.f, 0.8f);
						D.Life = 0.55f;
						D.FadeIn = 0.f;
						D.FadePow = 0.1f;
						D.FloorZ = 0.f;
						Fx->Emit(D);
					}
				}
			}
		}
	}
	// Drafts in the air ducts carry whoever crawls inside (movement is owner-authoritative: push locally).
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->Creature != 0 || !(A->IsLocallyControlled() || (HasAuthority() && !A->IsPlayerControlled())))
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		for (const FDuctWind& W : DuctWind)
		{
			if (W.Box.IsInside(P))
			{
				A->AddMovementInput(W.Dir, 1.f);
				break;
			}
		}
	}
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	HazardTimer += DeltaSeconds;
	if (HazardTimer < 0.25f)
	{
		return;
	}
	const float Dt = HazardTimer;
	HazardTimer = 0.f;

	TArray<AKitchenInteractable*> Gadgets;
	for (TActorIterator<AKitchenInteractable> It(GetWorld()); It; ++It)
	{
		Gadgets.Add(*It);
	}

	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead)
		{
			continue;
		}
		FAntEnv Env;
		EnvironmentFor(Ant, Env);
		for (AKitchenInteractable* G : Gadgets)
		{
			G->AffectAnt(Ant, Env);
		}
		AntFood::AffectAnt(Ant, Env);   // honey, jam, juice on the floor and sticky food hold the other side's ants
		Ant->ApplyEnvironment(Env, Dt);
	}
}

// ---------------------------------------------------------------------------------------------
// The yard outside the kitchen window: lawn, dog poop, outdoor trash bin, a tree, a fence.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildYard()
{
	const FLinearColor Grass = Hex(86, 142, 60), GrassDark = Hex(52, 104, 38), Soil = Hex(110, 80, 50);
	const FLinearColor FenceC = Hex(160, 120, 78), Bark = Hex(95, 68, 45), Leaves = Hex(60, 120, 50);
	const FLinearColor BinC = Hex(40, 92, 55), Stone = Hex(150, 148, 140);

	// Lawn at floor level (walkable).
	if (bArt)
	{
		// Level v2: two strips by the house instead of the 83 m2 field. A: under the kitchen window. B: behind the living room
		// (the red anthill, the tree, the bin). They meet at the corner of the living room. A little grass runs on under the fences.
		Box(FVector(-9600.f, -9000.f, -200.f), FVector(7000.f, -5200.f, 0.f), Grass);
		Box(FVector(5000.f, -12400.f, -200.f), FVector(20600.f, -9200.f, 0.f), Grass);
		Box(FVector(5000.f, -9200.f, -200.f), FVector(7000.f, -9000.f, 0.f), Grass);
		BuildEdges();
		// The south entrance of our nest at the house corner: the second place to bring food to (KitchenLoot.cpp). A hole in
		// trodden earth, crumbs of soil round it, a warm light like the one over the main entrance.
		const FVector South = LevelV2::SouthEntrance();
		Cyl(South, LevelV2::SouthEntranceRadius(), 2.f, DarkDirt, false);
		Cyl(South, 210.f, 3.f, Black, false);
		for (int32 i = 0; i < 9; ++i)
		{
			const float A = i * (2.f * PI / 9.f) + 0.4f;
			if (i == 1 || i == 2) { continue; }   // open towards the yard
			Ball(South + FVector(FMath::Cos(A) * 330.f, FMath::Sin(A) * 330.f, -20.f), FVector(150.f, 130.f, 70.f + 12.f * (i % 3)), i % 2 ? Dirt : DarkDirt);
		}
		Lamp(South + FVector(0.f, 0.f, 320.f), 1.6f, FLinearColor(1.f, 0.75f, 0.45f), 1500.f, false);
	}
	else
	{
		Box(FVector(-12000.f, -16000.f, -200.f), FVector(7000.f, -5200.f, 0.f), Grass);
		Box(FVector(7000.f, -16000.f, -200.f), FVector(21000.f, -9200.f, 0.f), Grass);
	}
	// Outer window sill: step out of the window onto it, then down the wall.
	if (!bArt)   // modelled: the second SM_K_WindowSill
	{
		Box(FVector(-1650.f, -5420.f, 2120.f), FVector(1650.f, -5200.f, 2200.f), White);
	}

	if (!bArt)   // modelled: SM_Y_FencePanel runs, stepping stones
	{
		// Fence.
		for (float X = -12000.f; X <= 21000.f; X += 1100.f)
		{
			Box(FVector(X - 60.f, -16060.f, 0.f), FVector(X + 60.f, -15940.f, 2600.f), FenceC);
		}
		Box(FVector(-12000.f, -16040.f, 600.f), FVector(21000.f, -15960.f, 760.f), FenceC);
		Box(FVector(-12000.f, -16040.f, 1900.f), FVector(21000.f, -15960.f, 2060.f), FenceC);
		for (float Y = -16000.f; Y <= -5200.f; Y += 1100.f)
		{
			Box(FVector(-12060.f, Y - 60.f, 0.f), FVector(-11940.f, Y + 60.f, 2600.f), FenceC);
		}
		Box(FVector(-12040.f, -16000.f, 600.f), FVector(-11960.f, -5200.f, 760.f), FenceC);
		Box(FVector(-12040.f, -16000.f, 1900.f), FVector(-11960.f, -5200.f, 2060.f), FenceC);

		// Stepping stones from the wall to the bin.
		for (int32 i = 0; i < 7; ++i)
		{
			const FVector C(600.f + i * 520.f, -6400.f - i * 950.f, 0.f);
			Cyl(C, 300.f + (i % 2) * 40.f, 22.f, Stone);
		}
	}
	// The outdoor trash bin (lid open, a bag and junk spilled on the grass).
	const FVector Bin = YardBin();
	if (!bArt)   // modelled: SM_Y_Bin with its lid (AKitchenMover) and the pile of bags in it, a bag on the grass
	{
		Cyl(Bin, 900.f, 2600.f, BinC);
		Cyl(Bin + FVector(0.f, 0.f, 2600.f), 930.f, 40.f, Hex(30, 70, 42));
		Part(EAntShape::Cube, Bin + FVector(-200.f, 950.f, 2900.f), FVector(1900.f, 80.f, 1300.f), FRotator(0.f, 0.f, -20.f), BinC, true);   // open lid
		Ball(Bin + FVector(0.f, 0.f, 2700.f), FVector(700.f, 700.f, 250.f), Hex(25, 25, 28));
		Ball(Bin + FVector(-1300.f, -500.f, 150.f), FVector(500.f, 380.f, 180.f), Hex(25, 25, 28));
	}
	Box(Bin + FVector(-1700.f, 300.f, 0.f), Bin + FVector(-1300.f, 500.f, 30.f), Hex(230, 170, 90), false);    // crust
	if (!bArt)   // modelled: SM_Y_AppleCore
	{
		Ball(Bin + FVector(-900.f, -1100.f, 60.f), FVector(90.f, 90.f, 130.f), Hex(170, 120, 60));                  // apple core
	}

	// A tree (climbable trunk) with a canopy.
	const FVector Tree = bArt ? FVector(6400.f, -10700.f, 0.f) : FVector(-8000.f, -12500.f, 0.f);   // level v2: in yard B by the passage
	if (!bArt)   // modelled: SM_Y_Tree (same trunk and branch)
	{
		Cyl(Tree, 520.f, 9000.f, Bark);
		Part(EAntShape::Cube, Tree + FVector(900.f, 0.f, 5200.f), FVector(1900.f, 200.f, 200.f), FRotator(-25.f, 0.f, 0.f), Bark, true);   // branch
		Ball(Tree + FVector(0.f, 0.f, 10500.f), FVector(4200.f, 4200.f, 2600.f), Leaves);
		Ball(Tree + FVector(-500.f, -400.f, -60.f), FVector(1300.f, 1300.f, 160.f), Soil);
	}

	// The dog's spot: a chewed ball and a bowl (the poop itself is the fly feeder actor).
	if (!bArt)   // modelled: the dog's bowl, ball, bone and rope
	{
		Cyl(FVector(-6000.f, -7200.f, 0.f), 500.f, 160.f, Hex(200, 40, 40));
		Ball(FVector(-4300.f, -7800.f, 220.f), FVector(220.f, 220.f, 220.f), Hex(240, 220, 50));
	}

	// Grass blades: one instanced mesh, thousands of thin tilted strips (no collision).
	UInstancedStaticMeshComponent* Blades = NewObject<UInstancedStaticMeshComponent>(this, TEXT("GrassBlades"));
	Blades->SetupAttachment(Root);
	Blades->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cube));
	Blades->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blades->SetCastShadow(false);
	Blades->SetCanEverAffectNavigation(false);
	Blades->RegisterComponent();
	AntKit::Paint(Blades, GrassDark);
	FRandomStream R(4242);
	TArray<FTransform> Xf;
	for (int32 i = 0; i < 3200; ++i)
	{
		const FVector P = RandomYardPoint(&R);
		if (FVector::Dist2D(P, Bin) < 1100.f || FVector::Dist2D(P, Tree) < 700.f || FVector::Dist2D(P, YardPoop()) < 700.f || P.Y > -5500.f)
		{
			continue;
		}
		if (bArt && (FVector::Dist2D(P, NestLocation(1)) < 1000.f || FVector::Dist2D(P, LevelV2::Sprinkler()) < 260.f))
		{
			continue;   // the red anthill's bare earth, the sprinkler head
		}
		const float H = R.FRandRange(180.f, 520.f);
		const FRotator Rot(R.FRandRange(-18.f, 18.f), R.FRandRange(0.f, 360.f), R.FRandRange(-18.f, 18.f));
		Xf.Add(FTransform(Rot, P + FVector(0.f, 0.f, H * 0.5f), FVector(0.14f, 0.03f, H / 100.f)));
	}
	if (!bArt || !ScatterTufts(Xf, 4242))   // modelled: tufts of several blades instead of single strips
	{
		Blades->AddInstances(Xf, false, true);
	}
}

// ---------------------------------------------------------------------------------------------
// Level v3: the districts of the raid seed (LevelV3::Current), applied on every machine from the same table rows:
// what a variant adds is spawned here, what it removes is hidden (and stops colliding), what it moves is put at its new place.
// Applying again first undoes the last seed. Loot, doors and the cake are the server's (AAntKitchenGameMode, AAntRaid).
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::MakeAntsOnly(UStaticMeshComponent* C)
{
	if (!C)
	{
		return;
	}
	C->ComponentTags.AddUnique(TEXT("AntsOnly"));
	C->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	AntsOnlyParts.Add(C);
	// Whoever already took the "ants only" list in (LetThrough) learns about this one too.
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsA<AAntCharacter>() && It->GetCapsuleComponent())
		{
			It->GetCapsuleComponent()->IgnoreComponentWhenMoving(C, true);
		}
	}
}

UStaticMeshComponent* AKitchenBuilder::AddV3Piece(const FKitchenArtInst& I, uint8 Flags, const TCHAR* Tint)
{
	UStaticMeshComponent* C = KitchenArt::Add(this, Root, I.Name, I.Index, KitchenArt::WorldTransform(I), KitchenArt::ESolid::Static, false);
	if (C)
	{
		if (Flags & 1)
		{
			MakeAntsOnly(C);
		}
		KitchenArt::ApplyTint(C, Tint);
		V3Extra.Add(C);
	}
	return C;
}

void AKitchenBuilder::SetArtShown(int32 Inst, bool bShown)
{
	UStaticMeshComponent* C = ArtByInst.IsValidIndex(Inst) ? ArtByInst[Inst] : nullptr;
	if (!C)
	{
		return;
	}
	C->SetVisibility(bShown, true);
	C->SetCollisionEnabled(bShown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AKitchenBuilder::ApplyV3Layout()
{
	if (!bBuilt || bLab || !LevelV3::Enabled())
	{
		return;
	}
	// ---- undo the last seed
	for (UStaticMeshComponent* C : V3Extra)
	{
		if (C)
		{
			AntsOnlyParts.Remove(C);
			C->DestroyComponent();
		}
	}
	V3Extra.Reset();
	for (int32 Inst : V3Hidden)
	{
		SetArtShown(Inst, true);
	}
	V3Hidden.Reset();
	for (const TPair<int32, FTransform>& KV : V3Moved)
	{
		if (ArtByInst.IsValidIndex(KV.Key) && ArtByInst[KV.Key])
		{
			ArtByInst[KV.Key]->SetWorldTransform(KV.Value);
		}
	}
	V3Moved.Reset();

	// ---- the variants of the current seed
	const TConstArrayView<FLevelV3Op> Ops = LevelV3::Ops();
	const TConstArrayView<FKitchenArtInst> Var = LevelV3::VarArt();
	int32 Added = 0, Hidden = 0, Moved = 0;
	for (int32 D = 0; D < LevelV3::NumDistricts(); ++D)
	{
		const int32 V = LevelV3::CurrentVariant(D);
		if (V == INDEX_NONE)
		{
			continue;
		}
		const FLevelV3Variant& Vr = LevelV3::Variants()[V];
		for (int32 k = 0; k < Vr.NumOps; ++k)
		{
			const FLevelV3Op& O = Ops[Vr.FirstOp + k];
			if (O.Kind == LevelV3::Add && Var.IsValidIndex(O.A) && LevelV3::IsFixtureObject(O.Text))
			{
				continue;   // the tablecloth, the teapot, the cups, the trolley: AKitchenFixture builds them (they move and break)
			}
			if (O.Kind == LevelV3::Add && Var.IsValidIndex(O.A))
			{
				Added += AddV3Piece(Var[O.A], LevelV3::VarFlags(O.A), LevelV3::VarTint(O.A)) ? 1 : 0;
			}
			else if (O.Kind == LevelV3::Remove && ArtByInst.IsValidIndex(O.A))
			{
				SetArtShown(O.A, false);
				V3Hidden.Add(O.A);
				++Hidden;
			}
			else if (O.Kind == LevelV3::Move && ArtByInst.IsValidIndex(O.A) && ArtByInst[O.A] && Var.IsValidIndex(O.B))
			{
				V3Moved.Add(O.A, ArtByInst[O.A]->GetComponentTransform());
				ArtByInst[O.A]->SetWorldTransform(KitchenArt::WorldTransform(Var[O.B]));
				++Moved;
			}
		}
	}

	// ---- the scatter that is a thing and not food: the kid's toys, a ball of yarn, the v2 mess (sugar, jam) brought back by the seed
	int32 Things = 0;
	if (const FLevelV3Seed* S = LevelV3::Current())
	{
		const TConstArrayView<FLevelV3Scatter> Sc = LevelV3::Scatter();
		for (int32 i = 0; i < S->ScatterNum; ++i)
		{
			if (!Sc.IsValidIndex(S->ScatterFirst + i))
			{
				break;
			}
			const FLevelV3Scatter& R = Sc[S->ScatterFirst + i];
			const FString Item = LevelV3::ScatterName(R.Item);
			FString Mesh;
			FVector P(R.X, R.Y, R.Z);
			float Yaw = R.Yaw;
			if (Item.StartsWith(TEXT("SM_"))) { Mesh = Item; }
			else if (Item == TEXT("клубочек")) { Mesh = TEXT("SM_L_YarnBall"); }
			else if (Item == TEXT("new38")) { Mesh = TEXT("SM_K_SugarSpill"); P = FVector(1250.f, -2450.f, 0.f); Yaw = 20.f; }
			else if (Item == TEXT("new39")) { Mesh = TEXT("SM_K_JamPuddle"); P = FVector(3450.f, -1500.f, 0.f); Yaw = 0.f; }
			if (Mesh.IsEmpty() || !KitchenArt::Mesh(*Mesh))
			{
				continue;   // crumbs, kibble, crisps: food (the server spawns it with the loot)
			}
			const float Scale = Mesh == TEXT("SM_L_YarnBall") ? 0.5f : 1.f;   // (a little ball, not grandma's big one)
			const FTransform T(FRotator(0.f, Yaw, 0.f), P, FVector(Scale * KitchenArt::ImportScale));
			if (UStaticMeshComponent* C = KitchenArt::Add(this, Root, *Mesh, 0, T, KitchenArt::ESolid::Static, false))
			{
				if (!Mesh.StartsWith(TEXT("SM_L_Toy")))
				{
					MakeAntsOnly(C);   // a puddle, a smear, a ball of yarn: the Human walks through
				}
				V3Extra.Add(C);
				++Things;
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSEED applied on %s: added %d, hidden %d, moved %d, scatter things %d | %s"),
		GetNetMode() == NM_Client ? TEXT("client") : TEXT("server"), Added, Hidden, Moved, Things, *LevelV3::Describe());
}

void AKitchenBuilder::ApplyLevelV3(UWorld* World)
{
	for (TActorIterator<AKitchenBuilder> It(World); It; ++It)
	{
		It->ApplyV3Layout();
	}
}
