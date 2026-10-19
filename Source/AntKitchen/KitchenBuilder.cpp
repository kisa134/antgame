#include "KitchenBuilder.h"
#include "AntGameState.h"
#include "AntCharacter.h"
#include "KitchenInteractables.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	Build();
}

// ---------------------------------------------------------------------------------------------
// Layout data shared with the GameMode (server) so loot sits on real surfaces.
// ---------------------------------------------------------------------------------------------

FVector AKitchenBuilder::NestLocation(uint8 Team) { return Team == 0 ? FVector(-6650.f, -2500.f, 0.f) : FVector(6300.f, 1250.f, 0.f); }

void AKitchenBuilder::HeavySpots(TArray<FVector>& Out)
{
	Out.Reset();
	Out.Add(FVector(0.f, -2600.f, 0.f));        // middle of the floor, between the nests
	Out.Add(FVector(1800.f, -3900.f, 0.f));     // under the window
	Out.Add(FVector(-1600.f, 1900.f, 0.f));     // by the stove
	Out.Add(FVector(3600.f, -400.f, 0.f));      // near the fire ants
	Out.Add(FVector(1000.f, -900.f, 1430.f));   // on the table: drop it down!
	Out.Add(FVector(-4000.f, -1600.f, 0.f));    // near our nest
}
FVector AKitchenBuilder::HumanSpawn() { return FVector(3800.f, -3200.f, 1720.f); }
FVector AKitchenBuilder::HumanFleePoint() { return FVector(5600.f, -4200.f, 1720.f); }
FBox AKitchenBuilder::HotBurner() { return FBox(FVector(-610.f, 3240.f, 1735.f), FVector(-90.f, 3760.f, 1900.f)); }
FBox AKitchenBuilder::PlayArea() { return FBox(FVector(-6000.f, -4300.f, 0.f), FVector(6200.f, 2550.f, 0.f)); }

void AKitchenBuilder::LootSpots(TArray<FVector>& L, TArray<int32>& T)
{
	L.Reset();
	T.Reset();
	auto Add = [&L, &T](float X, float Y, float SurfaceZ, int32 Tier)
	{
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
	Add(5750.f, -1250.f, 1540.f, 3);
	Add(6250.f, -1250.f, 1540.f, 2);
	Add(5200.f, -300.f, 0.f, 1);
	Add(5000.f, -900.f, 0.f, 1);
	Add(6000.f, -2700.f, 50.f, 1);     // on the fallen lid
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
}

// ---------------------------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------------------------

UStaticMeshComponent* AKitchenBuilder::Part(EAntShape Shape, const FVector& Center, const FVector& Size, const FRotator& Rot, const FLinearColor& Color, bool bCollide)
{
	return AntKit::AddPart(this, Root, Shape, Center, Size / 100.f, Color, Rot, bCollide, true);
}

UStaticMeshComponent* AKitchenBuilder::Box(const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bCollide)
{
	return Part(EAntShape::Cube, (Min + Max) * 0.5f, Max - Min, FRotator::ZeroRotator, Color, bCollide);
}

UStaticMeshComponent* AKitchenBuilder::Cyl(const FVector& Base, float Radius, float Height, const FLinearColor& Color, bool bCollide)
{
	return Part(EAntShape::Cylinder, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(Radius * 2.f, Radius * 2.f, Height), FRotator::ZeroRotator, Color, bCollide);
}

UStaticMeshComponent* AKitchenBuilder::Ball(const FVector& Center, const FVector& Radii, const FLinearColor& Color)
{
	// Non-uniform spheres get no collision (scaled sphere collision is unreliable).
	return Part(EAntShape::Sphere, Center, Radii * 2.f, FRotator::ZeroRotator, Color, false);
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
	// ---- Floor (one collision slab + checker tiles on top) ----
	Box(FVector(-7000.f, -5000.f, -100.f), FVector(7000.f, 5000.f, 0.f), TileA);
	for (int32 i = 0; i < 14; ++i)
	{
		for (int32 j = 0; j < 10; ++j)
		{
			if ((i + j) % 2 == 0) continue;
			const float X = -7000.f + i * 1000.f, Y = -5000.f + j * 1000.f;
			Box(FVector(X + 5.f, Y + 5.f, 0.f), FVector(X + 995.f, Y + 995.f, 2.f), TileB, false);
		}
	}

	// ---- Walls, ceiling, baseboards ----
	// North wall is thick: a mouse room hides inside it behind the fridge (x 3500..4300).
	Box(FVector(-7200.f, 5000.f, 0.f), FVector(3500.f, 5600.f, 5400.f), WallC);
	Box(FVector(4300.f, 5000.f, 0.f), FVector(7200.f, 5600.f, 5400.f), WallC);
	Box(FVector(3500.f, 5000.f, 450.f), FVector(4300.f, 5600.f, 5400.f), WallC);
	Box(FVector(3500.f, 5550.f, 0.f), FVector(4300.f, 5600.f, 450.f), WallC);
	Box(FVector(3500.f, 5000.f, 0.f), FVector(3800.f, 5050.f, 450.f), WallC);       // front slab with the mouse hole
	Box(FVector(3960.f, 5000.f, 0.f), FVector(4300.f, 5050.f, 450.f), WallC);
	Box(FVector(3800.f, 5000.f, 140.f), FVector(3960.f, 5050.f, 450.f), WallC);
	Box(FVector(3500.f, 5000.f, -100.f), FVector(4300.f, 5600.f, 0.f), Hex(120, 95, 70));   // mouse room floor
	// South wall with a real window opening: the sun shines in.
	Box(FVector(-7200.f, -5200.f, 0.f), FVector(-1500.f, -5000.f, 5400.f), WallC);
	Box(FVector(1500.f, -5200.f, 0.f), FVector(7200.f, -5000.f, 5400.f), WallC);
	Box(FVector(-1500.f, -5200.f, 0.f), FVector(1500.f, -5000.f, 2200.f), WallC);
	Box(FVector(-1500.f, -5200.f, 4200.f), FVector(1500.f, -5000.f, 5400.f), WallC);
	// Outside: a lawn far below the window, so the view isn't a void.
	Box(FVector(-40000.f, -60000.f, -900.f), FVector(40000.f, -5200.f, -800.f), Hex(70, 120, 50), false);
	Box(FVector(-7200.f, -5000.f, 0.f), FVector(-7000.f, 5000.f, 5400.f), WallC);
	Box(FVector(7000.f, -5000.f, 0.f), FVector(7200.f, 5000.f, 5400.f), WallC);
	Box(FVector(-7200.f, -5200.f, 5400.f), FVector(7200.f, 5600.f, 5600.f), Ceiling);
	Box(FVector(-7000.f, -5000.f, 0.f), FVector(7000.f, -4970.f, 220.f), Baseboard, false);
	Box(FVector(6970.f, -5000.f, 0.f), FVector(7000.f, 5000.f, 220.f), Baseboard, false);
	Box(FVector(-7000.f, -5000.f, 0.f), FVector(-6970.f, -3200.f, 220.f), Baseboard, false);
	Box(FVector(-7000.f, -1800.f, 0.f), FVector(-6970.f, 5000.f, 220.f), Baseboard, false);

	BuildNorthRun();

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
	Box(FVector(1300.f, 4000.f, 1710.f), FVector(2400.f, 4900.f, 2350.f), DarkSteel);            // microwave
	Box(FVector(1400.f, 3988.f, 1800.f), FVector(2050.f, 4000.f, 2250.f), Black, false);
	Cyl(FVector(800.f, 4600.f, 1710.f), 250.f, 500.f, Hex(230, 235, 240));                         // sugar jar
	Cyl(FVector(-2200.f, 4200.f, 1710.f), 180.f, 300.f, Hex(230, 120, 40));                        // mug

	BuildFridge();

	// ---- Trash corner (east) ----
	Cyl(FVector(6000.f, -1500.f, 0.f), 650.f, 1500.f, Steel);
	Cyl(FVector(6000.f, -1500.f, 1500.f), 670.f, 40.f, DarkSteel);
	Ball(FVector(6000.f, -1500.f, 1560.f), FVector(480.f, 480.f, 120.f), Hex(30, 70, 40));            // bag sticking out
	Box(FVector(5700.f, -1850.f, 1600.f), FVector(6100.f, -1700.f, 1640.f), Hex(240, 210, 60), false); // banana peel
	Ball(FVector(6300.f, -1750.f, 1650.f), FVector(70.f, 70.f, 110.f), Hex(170, 120, 60));            // apple core
	Cyl(FVector(6000.f, -2700.f, 0.f), 680.f, 50.f, Steel);                                            // fallen lid
	Ball(FVector(5200.f, -400.f, 250.f), FVector(500.f, 400.f, 300.f), Black);                        // tipped bag
	Box(FVector(4900.f, -700.f, 0.f), FVector(5300.f, -500.f, 30.f), Hex(230, 170, 90), false);       // pizza crust

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
	Box(FVector(-2500.f, -2600.f, 0.f), FVector(3500.f, 1600.f, 6.f), Hex(170, 40, 50), false);      // rug

	Chair(FVector(0.f, -2750.f, 0.f), true, DarkWood);
	Chair(FVector(1200.f, 1650.f, 0.f), false, DarkWood);

	// ---- South-west: pet corner and spilled cereal ----
	Cyl(FVector(-3800.f, -3800.f, 0.f), 480.f, 170.f, Hex(240, 140, 40));
	Ball(FVector(-3800.f, -3800.f, 170.f), FVector(380.f, 380.f, 60.f), Hex(110, 60, 25));
	Cyl(FVector(-2500.f, -3900.f, 0.f), 420.f, 140.f, Hex(120, 140, 170));
	Cyl(FVector(-2500.f, -3900.f, 140.f), 380.f, 4.f, Hex(150, 200, 255), false);

	// Cereal box lying on its side, open end facing east: a cozy tunnel.
	Box(FVector(-5800.f, 500.f, 0.f), FVector(-4600.f, 1300.f, 20.f), Hex(230, 70, 40));
	Box(FVector(-5800.f, 500.f, 400.f), FVector(-4600.f, 1300.f, 420.f), Hex(230, 70, 40));
	Box(FVector(-5800.f, 500.f, 20.f), FVector(-4600.f, 520.f, 400.f), Hex(230, 70, 40));
	Box(FVector(-5800.f, 1280.f, 20.f), FVector(-4600.f, 1300.f, 400.f), Hex(230, 70, 40));
	Box(FVector(-5800.f, 520.f, 20.f), FVector(-5780.f, 1280.f, 400.f), Hex(210, 60, 35));
	Box(FVector(-5700.f, 494.f, 120.f), FVector(-4700.f, 500.f, 300.f), Hex(250, 210, 50), false);
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * 0.7f;
		Ball(FVector(-4500.f + FMath::Cos(A) * (120.f + i * 25.f), 900.f + FMath::Sin(A) * (150.f + i * 30.f), 15.f), FVector(28.f, 28.f, 14.f), Hex(250, 160, 40));
	}

	// Window on the south wall
	// (no glass: the opening lets sunlight and sky in)
	Box(FVector(-30.f, -4990.f, 2200.f), FVector(30.f, -4980.f, 4200.f), White, false);
	Box(FVector(-1500.f, -4990.f, 3170.f), FVector(1500.f, -4980.f, 3230.f), White, false);
	Box(FVector(-1650.f, -5000.f, 2120.f), FVector(1650.f, -4820.f, 2200.f), White);                  // sill

	// Wall shelf with jars (west wall)
	Box(FVector(-7000.f, -1100.f, 2600.f), FVector(-6550.f, 1100.f, 2680.f), Wood);
	Box(FVector(-7000.f, -900.f, 2400.f), FVector(-6900.f, -850.f, 2600.f), DarkWood, false);
	Box(FVector(-7000.f, 850.f, 2400.f), FVector(-6900.f, 900.f, 2600.f), DarkWood, false);
	Cyl(FVector(-6780.f, -150.f, 2680.f), 110.f, 280.f, Hex(170, 40, 30));
	Cyl(FVector(-6780.f, -1000.f + 50.f, 2680.f), 60.f, 220.f, Hex(60, 140, 60));

	// ---- The ant nest (hole in the west baseboard + dirt mound) ----
	const FVector Nest = NestLocation();
	AddMound(0, Ball(FVector(-6900.f, Nest.Y, -40.f), FVector(700.f, 800.f, 380.f), Dirt));
	AddMound(0, Ball(FVector(-6990.f, Nest.Y, 150.f), FVector(120.f, 320.f, 240.f), Black));
	AddMound(0, Ball(FVector(-6650.f, Nest.Y - 650.f, -30.f), FVector(380.f, 300.f, 200.f), Dirt));
	AddMound(0, Ball(FVector(-6700.f, Nest.Y + 680.f, -30.f), FVector(340.f, 320.f, 180.f), Dirt));
	Cyl(FVector(Nest.X, Nest.Y, 0.f), NestRadius(), 4.f, DarkDirt, false);
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = i * 1.05f;
		Part(EAntShape::Cube, FVector(Nest.X + FMath::Cos(A) * 420.f, Nest.Y + FMath::Sin(A) * 420.f, 12.f), FVector(260.f, 18.f, 18.f), FRotator(0.f, i * 37.f, 0.f), DarkWood, false);
	}
	for (int32 i = 0; i < 14; ++i)   // little ant trail painted on the tiles
	{
		const float T = i / 13.f;
		const FVector P = FMath::Lerp(FVector(-5900.f, -2500.f, 3.f), FVector(-1800.f, -900.f, 3.f), T);
		Ball(P + FVector(0.f, FMath::Sin(T * 9.f) * 120.f, 0.f), FVector(22.f, 14.f, 3.f), Hex(40, 25, 20));
	}

	// ---- Rival fire-ant nest: red mound by the trash ----
	{
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
		LampShades.Add(Cyl(FVector(S.X, S.Y, 5150.f), 350.f, 250.f, White, false));
		MainLamps.Add(Lamp(FVector(S.X, S.Y, 4950.f), 8.f, FLinearColor(1.f, 0.93f, 0.8f), 16000.f, true));
	}
	Lamp(FVector(-350.f, 3500.f, 1950.f), 4.f, FLinearColor(1.f, 0.25f, 0.05f), 1100.f, false);   // hot burner glow
	Lamp(FVector(-6200.f, Nest.Y, 500.f), 3.f, FLinearColor(1.f, 0.7f, 0.4f), 1600.f, false);      // nest glow
	WindowLight = Lamp(FVector(0.f, -4500.f, 3200.f), 3.f, FLinearColor(0.7f, 0.85f, 1.f), 6000.f, false);       // window daylight
	Lamp(FVector(3900.f, 5300.f, 330.f), 2.5f, FLinearColor(1.f, 0.6f, 0.25f), 800.f, false);      // candle in the mouse room

	BuildSecrets();

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
	SkyL->RegisterComponent();
	AddInstanceComponent(SkyL);

	// Dust in the air: visible sun beams.
	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this);
	Fog->SetupAttachment(Root);
	Fog->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	Fog->FogDensity = 0.0025f;
	Fog->FogHeightFalloff = 0.02f;
	Fog->bEnableVolumetricFog = true;
	Fog->VolumetricFogScatteringDistribution = 0.75f;
	Fog->VolumetricFogExtinctionScale = 0.6f;
	Fog->VolumetricFogDistance = 12000.f;
	Fog->RegisterComponent();
	AddInstanceComponent(Fog);

	SetMainLights(AKitchenSwitch::AreLightsOn(GetWorld()));
}

// ---------------------------------------------------------------------------------------------
// North counter run: cabinets stop 250 short of the wall, leaving a secret corridor behind them
// (y 4750..5000) under the countertop. Inside: the drawer, the hollow under-sink cabinet, the oven.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildNorthRun()
{
	const float Back = 4750.f;   // back face of the cabinets

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
	Box(FVector(-1450.f, 4700.f, 300.f), FVector(-50.f, Back, 1500.f), Hex(30, 25, 25));
	Box(FVector(-1450.f, 3100.f, 880.f), FVector(-50.f, 4650.f, 900.f), Steel);                      // rack
	Box(FVector(-1100.f, 3500.f, 900.f), FVector(-400.f, 4300.f, 915.f), Hex(60, 60, 64));          // baking tray
	Box(FVector(-1400.f, 3200.f, 1478.f), FVector(-100.f, 4600.f, 1490.f), Hex(255, 90, 30), false); // glowing element
	Box(FVector(-1400.f, 3200.f, 302.f), FVector(-100.f, 4600.f, 312.f), Hex(255, 90, 30), false);
	Box(FVector(0.f, 3000.f, 120.f), FVector(2500.f, Back, 1650.f), Cabinet);

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
		const bool bHot = (i == 1);
		Cyl(FVector(Burners[i].X, Burners[i].Y, 1740.f), 260.f, 25.f, bHot ? Hex(255, 60, 20) : DarkSteel);
		Cyl(FVector(Burners[i].X, Burners[i].Y, 1765.f), 120.f, 6.f, bHot ? Hex(255, 160, 40) : Black, false);
	}
	Box(FVector(-1450.f, 2950.f, 1560.f), FVector(-50.f, 2990.f, 1640.f), Steel, false);   // oven control strip
	// Generated oven shell around the hollow oven (the inside stays our box so ants can crawl in).
	AntArt::DressAuto(this, Root, TEXT("SM_Oven_Body"), FVector(-1500.f, 2990.f, 120.f), FVector(0.f, 4750.f, 1740.f), 0.f, 6.f, false);   // shell only: the oven stays hollow inside
	for (int32 i = 0; i < 4; ++i)
	{
		Part(EAntShape::Cylinder, FVector(-1250.f + i * 330.f, 2945.f, 1600.f), FVector(70.f, 70.f, 30.f), FRotator(90.f, 0.f, 0.f), Black, false);
	}

	Doors(-2800.f, -1500.f, 2, CabinetDoor, Steel);
	Doors(0.f, 2500.f, 3, CabinetDoor, Steel);

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
	Ball(FVector(3600.f, 4000.f, 935.f), FVector(330.f, 300.f, 10.f), Hex(170, 20, 40));          // spilled jam
	Box(FVector(4000.f, 3600.f, 1730.f), FVector(4500.f, 4000.f, 1950.f), Hex(250, 210, 60));     // cheese block
	Cyl(FVector(3200.f, 3700.f, 1730.f), 260.f, 120.f, Hex(240, 240, 235));                       // plate of leftovers
	Cyl(FVector(3600.f, 4000.f, 2560.f), 330.f, 500.f, Hex(240, 220, 200));                       // ice cream tub (freezer)
	Box(FVector(2900.f, 3300.f, 2560.f), FVector(3200.f, 3600.f, 2700.f), Hex(200, 230, 255));    // ice tray
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
	UStaticMeshComponent* HoneyJar = Part(EAntShape::Cylinder, FVector(-6760.f, 260.f, 2780.f), FVector(340.f, 340.f, 300.f), FRotator(90.f, 0.f, 0.f), Hex(230, 150, 30), true);
	if (AntArt::Dress(this, Root, TEXT("SM_HoneyJar"), FVector(-6950.f, 70.f, 2680.f), FVector(-6560.f, 460.f, 3000.f), 0.f))
	{
		AntArt::HideAll({ HoneyJar });
	}
	Box(FVector(-6950.f, 90.f, 2680.f), FVector(-6560.f, 430.f, 2686.f), Honey, false);
	Box(FVector(-7000.f, 190.f, 0.f), FVector(-6994.f, 360.f, 2680.f), Honey, false);
	Cyl(FVector(-6780.f, 280.f, 0.f), 280.f, 4.f, Honey, false);

	// Jam jar knocked over on the table.
	UStaticMeshComponent* JamJar = Part(EAntShape::Cylinder, FVector(980.f, 520.f, 1530.f), FVector(240.f, 240.f, 260.f), FRotator(0.f, 20.f, 90.f), Hex(200, 30, 50), true);
	if (AntArt::Dress(this, Root, TEXT("SM_JamJar"), FVector(860.f, 360.f, 1400.f), FVector(1100.f, 680.f, 1610.f), 90.f))
	{
		AntArt::HideAll({ JamJar });
	}

	// Cereal box on the counter (climbable, a prize on top).
	{
		UStaticMeshComponent* Shell = Box(FVector(-2700.f, 4150.f, 1710.f), FVector(-1800.f, 4650.f, 2030.f), Hex(230, 70, 40));
		if (AntArt::Dress(this, Root, TEXT("SM_CerealBox"), FVector(-2700.f, 4150.f, 1710.f), FVector(-1800.f, 4650.f, 2030.f), 90.f))
		{
			AntArt::HideAll({ Shell });
		}
	}
	Cyl(FVector(610.f, 250.f, 1400.f), 290.f, 5.f, Hex(170, 20, 40), false);

	// Mouse hole (behind the fridge) and the room in the wall.
	Ball(FVector(3880.f, 4999.f, 140.f), FVector(80.f, 4.f, 60.f), Black);
	Box(FVector(3820.f, 4996.f, 0.f), FVector(3940.f, 5000.f, 140.f), Black, false);
	Box(FVector(3550.f, 5250.f, 0.f), FVector(3800.f, 5500.f, 90.f), Hex(200, 50, 40));        // matchbox bed
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

void AKitchenBuilder::EnvironmentFor(const AAntCharacter* Ant, FAntEnv& Env)
{
	const FVector P = Ant->GetActorLocation();

	if (HotBurner().IsInside(P))
	{
		Env.Temp(150.f, 70.f);
	}
	else if (FBox(FVector(-650.f, 3200.f, 1740.f), FVector(-50.f, 3800.f, 2400.f)).IsInside(P))
	{
		Env.Temp(48.f, 8.f);   // hot air above the burner
	}

	static const FBox Sticky[] =
	{
		FBox(FVector(-7000.f, 150.f, 0.f), FVector(-6930.f, 400.f, 2600.f)),       // honey dripping down the west wall
		FBox(FVector(-7000.f, 0.f, -50.f), FVector(-6450.f, 560.f, 70.f)),         // honey puddle on the floor
		FBox(FVector(-6950.f, 80.f, 2660.f), FVector(-6560.f, 440.f, 2760.f)),     // honey on the shelf
		FBox(FVector(280.f, -60.f, 1380.f), FVector(940.f, 560.f, 1470.f)),        // jam on the table
		FBox(FVector(3300.f, 3700.f, 900.f), FVector(3900.f, 4300.f, 990.f)),      // jam spilled in the fridge
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
	if (FVector::Dist2D(P, FVector(6200.f, 3900.f, 0.f)) < 400.f && P.Z > 600.f && P.Z < 720.f)
	{
		Env.Flags |= AntStatus::Wet;
		Env.Temp(15.f, 6.f);
	}
}

void AKitchenBuilder::SetMainLights(bool bOn)
{
	for (UPointLightComponent* L : MainLamps)
	{
		if (L) { L->SetVisibility(bOn); }
	}
	for (UStaticMeshComponent* S : LampShades)
	{
		if (UMaterialInstanceDynamic* M = S ? Cast<UMaterialInstanceDynamic>(S->GetMaterial(0)) : nullptr)
		{
			M->SetVectorParameterValue(TEXT("Color"), bOn ? FLinearColor(3.f, 2.8f, 2.4f) : FLinearColor(0.2f, 0.2f, 0.2f));
		}
	}
	// The sun doesn't care about the light switch.
	if (WindowLight) { WindowLight->SetIntensity(bOn ? 3.f : 1.2f); WindowLight->SetLightColor(bOn ? FLinearColor(0.7f, 0.85f, 1.f) : FLinearColor(0.35f, 0.45f, 0.9f)); }
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
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateMounds(DeltaSeconds);
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
		Ant->ApplyEnvironment(Env, Dt);
	}
}
