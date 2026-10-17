#include "KitchenBuilder.h"
#include "AntCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Engine/World.h"

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

FVector AKitchenBuilder::NestLocation() { return FVector(-6650.f, -2500.f, 0.f); }
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
		if (FVector::Dist2D(P, NestLocation()) < 1100.f) continue;
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
	Add(-2200.f, 4600.f, 1710.f, 1);
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
		Box(FVector(A + 15.f, 2988.f, 170.f), FVector(A + W - 15.f, 3000.f, 1600.f), DoorColor, false);
		const float Mid = A + W * 0.5f;
		Box(FVector(Mid - 80.f, 2970.f, 1420.f), FVector(Mid + 80.f, 2988.f, 1460.f), HandleColor, false);
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

void AKitchenBuilder::Lamp(const FVector& Loc, float Intensity, const FLinearColor& Color, float Radius, bool bShadows)
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
	Box(FVector(-7200.f, 5000.f, 0.f), FVector(7200.f, 5200.f, 5400.f), WallC);
	Box(FVector(-7200.f, -5200.f, 0.f), FVector(7200.f, -5000.f, 5400.f), WallC);
	Box(FVector(-7200.f, -5000.f, 0.f), FVector(-7000.f, 5000.f, 5400.f), WallC);
	Box(FVector(7000.f, -5000.f, 0.f), FVector(7200.f, 5000.f, 5400.f), WallC);
	Box(FVector(-7200.f, -5200.f, 5400.f), FVector(7200.f, 5200.f, 5600.f), Ceiling);
	Box(FVector(-7000.f, -5000.f, 0.f), FVector(7000.f, -4970.f, 220.f), Baseboard, false);
	Box(FVector(6970.f, -5000.f, 0.f), FVector(7000.f, 5000.f, 220.f), Baseboard, false);
	Box(FVector(-7000.f, -5000.f, 0.f), FVector(-6970.f, -3200.f, 220.f), Baseboard, false);
	Box(FVector(-7000.f, -1800.f, 0.f), FVector(-6970.f, 5000.f, 220.f), Baseboard, false);

	// ---- North counter run: x [-7000, 2500], y [3000, 5000] ----
	Box(FVector(-7000.f, 3100.f, 0.f), FVector(2500.f, 5000.f, 120.f), Black);                  // kick plinth
	Box(FVector(-7000.f, 3000.f, 120.f), FVector(-4200.f, 5000.f, 1650.f), Cabinet);
	Box(FVector(-4200.f, 3000.f, 120.f), FVector(-2800.f, 5000.f, 1100.f), Cabinet);            // under sink
	Box(FVector(-2800.f, 3000.f, 120.f), FVector(2500.f, 5000.f, 1650.f), Cabinet);

	// Countertop with a hole for the sink basin
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
	Box(FVector(-3900.f, 3500.f, 1150.f), FVector(-3300.f, 4400.f, 1158.f), Hex(110, 170, 230), false); // puddle
	// Faucet
	Cyl(FVector(-3500.f, 4850.f, 1710.f), 60.f, 520.f, Steel);
	Box(FVector(-3550.f, 4350.f, 2160.f), FVector(-3450.f, 4850.f, 2240.f), Steel);
	Cyl(FVector(-3500.f, 4400.f, 2060.f), 40.f, 100.f, Steel, false);
	Cyl(FVector(-3750.f, 4800.f, 1710.f), 50.f, 120.f, Hex(200, 40, 40));
	Cyl(FVector(-3250.f, 4800.f, 1710.f), 50.f, 120.f, Hex(40, 80, 200));

	// Stove
	Box(FVector(-1500.f, 3000.f, 1710.f), FVector(0.f, 5000.f, 1740.f), Black);
	const FVector Burners[4] = { FVector(-1150.f, 3500.f, 0.f), FVector(-350.f, 3500.f, 0.f), FVector(-1150.f, 4400.f, 0.f), FVector(-350.f, 4400.f, 0.f) };
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bHot = (i == 1);
		Cyl(FVector(Burners[i].X, Burners[i].Y, 1740.f), 260.f, 25.f, bHot ? Hex(255, 60, 20) : DarkSteel);
		Cyl(FVector(Burners[i].X, Burners[i].Y, 1765.f), 120.f, 6.f, bHot ? Hex(255, 160, 40) : Black, false);
	}
	Box(FVector(-1450.f, 2988.f, 300.f), FVector(-50.f, 3000.f, 1500.f), Black, false);   // oven door
	Box(FVector(-1350.f, 2950.f, 1410.f), FVector(-150.f, 2985.f, 1460.f), Steel, false);
	Box(FVector(-1300.f, 2990.f, 500.f), FVector(-200.f, 2992.f, 1100.f), Hex(40, 30, 25), false);

	// Cabinet doors
	Doors(-7000.f, -4200.f, 4, CabinetDoor, Steel);
	Doors(-4200.f, -2800.f, 2, CabinetDoor, Steel);
	Doors(-2800.f, -1500.f, 2, CabinetDoor, Steel);
	Doors(0.f, 2500.f, 3, CabinetDoor, Steel);

	// Backsplash
	Box(FVector(-7000.f, 4970.f, 1710.f), FVector(2500.f, 5000.f, 2900.f), Backsplash, false);

	// Upper cabinets and range hood
	Box(FVector(-7000.f, 4300.f, 3000.f), FVector(-2000.f, 5000.f, 4400.f), Cabinet);
	Box(FVector(1000.f, 4300.f, 3000.f), FVector(2500.f, 5000.f, 4400.f), Cabinet);
	Box(FVector(-1600.f, 4200.f, 3300.f), FVector(100.f, 5000.f, 3600.f), Steel);
	Box(FVector(-1000.f, 4600.f, 3600.f), FVector(-500.f, 5000.f, 5400.f), Steel);

	// Things on the counters
	Box(FVector(-6600.f, 3900.f, 1710.f), FVector(-5900.f, 4500.f, 2150.f), Hex(200, 40, 40));   // toaster
	Box(FVector(-6450.f, 4050.f, 2150.f), FVector(-6050.f, 4120.f, 2153.f), Black, false);
	Box(FVector(-6450.f, 4280.f, 2150.f), FVector(-6050.f, 4350.f, 2153.f), Black, false);
	Box(FVector(-5600.f, 3250.f, 1710.f), FVector(-4500.f, 3950.f, 1760.f), Wood);               // cutting board
	Box(FVector(-5400.f, 3450.f, 1760.f), FVector(-4800.f, 3510.f, 1772.f), Steel, false);       // knife
	Box(FVector(-4800.f, 3440.f, 1760.f), FVector(-4550.f, 3520.f, 1790.f), Black, false);
	Box(FVector(1300.f, 4000.f, 1710.f), FVector(2400.f, 4900.f, 2350.f), DarkSteel);            // microwave
	Box(FVector(1400.f, 3988.f, 1800.f), FVector(2050.f, 4000.f, 2250.f), Black, false);
	Cyl(FVector(800.f, 4600.f, 1710.f), 250.f, 500.f, Hex(230, 235, 240));                         // sugar jar
	Cyl(FVector(-2200.f, 4200.f, 1710.f), 180.f, 300.f, Hex(230, 120, 40));                        // mug

	// ---- Fridge ----
	Box(FVector(2700.f, 3100.f, 0.f), FVector(4700.f, 5000.f, 3700.f), White);
	Box(FVector(2700.f, 3085.f, 2500.f), FVector(4700.f, 3100.f, 2520.f), Hex(190, 190, 190), false);
	Box(FVector(2800.f, 3050.f, 1500.f), FVector(2860.f, 3085.f, 2400.f), Steel, false);
	Box(FVector(2800.f, 3050.f, 2650.f), FVector(2860.f, 3085.f, 3400.f), Steel, false);
	Box(FVector(3500.f, 3090.f, 2000.f), FVector(3650.f, 3100.f, 2150.f), Hex(240, 60, 60), false);  // magnets
	Box(FVector(3900.f, 3090.f, 1800.f), FVector(4000.f, 3100.f, 1900.f), Hex(60, 160, 240), false);
	Box(FVector(3300.f, 3090.f, 1650.f), FVector(3700.f, 3100.f, 1950.f), White * 0.9f, false);      // a note

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

	Box(FVector(-5800.f, 500.f, 0.f), FVector(-4600.f, 1300.f, 420.f), Hex(230, 70, 40));            // cereal box (lying)
	Box(FVector(-5700.f, 494.f, 120.f), FVector(-4700.f, 500.f, 300.f), Hex(250, 210, 50), false);
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * 0.7f;
		Ball(FVector(-4500.f + FMath::Cos(A) * (120.f + i * 25.f), 900.f + FMath::Sin(A) * (150.f + i * 30.f), 15.f), FVector(28.f, 28.f, 14.f), Hex(250, 160, 40));
	}

	// Window on the south wall
	Box(FVector(-1500.f, -5000.f, 2200.f), FVector(1500.f, -4988.f, 4200.f), Hex(150, 200, 255), false);
	Box(FVector(-30.f, -4990.f, 2200.f), FVector(30.f, -4980.f, 4200.f), White, false);
	Box(FVector(-1500.f, -4990.f, 3170.f), FVector(1500.f, -4980.f, 3230.f), White, false);
	Box(FVector(-1650.f, -5000.f, 2120.f), FVector(1650.f, -4820.f, 2200.f), White);                  // sill

	// Wall shelf with jars (west wall)
	Box(FVector(-7000.f, -1100.f, 2600.f), FVector(-6550.f, 1100.f, 2680.f), Wood);
	Box(FVector(-7000.f, -900.f, 2400.f), FVector(-6900.f, -850.f, 2600.f), DarkWood, false);
	Box(FVector(-7000.f, 850.f, 2400.f), FVector(-6900.f, 900.f, 2600.f), DarkWood, false);
	Cyl(FVector(-6780.f, 250.f, 2680.f), 170.f, 380.f, Hex(230, 150, 30));                             // honey jar
	Cyl(FVector(-6780.f, -150.f, 2680.f), 110.f, 280.f, Hex(170, 40, 30));
	Cyl(FVector(-6780.f, -1000.f + 50.f, 2680.f), 60.f, 220.f, Hex(60, 140, 60));

	// ---- The ant nest (hole in the west baseboard + dirt mound) ----
	const FVector Nest = NestLocation();
	Ball(FVector(-6900.f, Nest.Y, -40.f), FVector(700.f, 800.f, 380.f), Dirt);
	Ball(FVector(-6990.f, Nest.Y, 150.f), FVector(120.f, 320.f, 240.f), Black);
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

	// ---- Lights ----
	const FVector LampSpots[3] = { FVector(-3500.f, 0.f, 0.f), FVector(2200.f, -1500.f, 0.f), FVector(1000.f, 2800.f, 0.f) };
	for (const FVector& S : LampSpots)
	{
		Cyl(FVector(S.X, S.Y, 5150.f), 350.f, 250.f, White, false);
		Lamp(FVector(S.X, S.Y, 4950.f), 8.f, FLinearColor(1.f, 0.93f, 0.8f), 16000.f, true);
	}
	Lamp(FVector(-350.f, 3500.f, 1950.f), 4.f, FLinearColor(1.f, 0.25f, 0.05f), 1100.f, false);   // hot burner glow
	Lamp(FVector(-6200.f, Nest.Y, 500.f), 3.f, FLinearColor(1.f, 0.7f, 0.4f), 1600.f, false);      // nest glow
	Lamp(FVector(0.f, -4500.f, 3200.f), 3.f, FLinearColor(0.7f, 0.85f, 1.f), 6000.f, false);       // window daylight

	UDirectionalLightComponent* Fill = NewObject<UDirectionalLightComponent>(this);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetupAttachment(Root);
	Fill->SetRelativeRotation(FRotator(-55.f, 35.f, 0.f));
	Fill->Intensity = 1.5f;
	Fill->CastShadows = false;
	Fill->RegisterComponent();
	AddInstanceComponent(Fill);
}

// ---------------------------------------------------------------------------------------------
// Hazards (server only): the hot burner fries ants standing on it.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	HazardTimer += DeltaSeconds;
	if (HazardTimer < 0.25f)
	{
		return;
	}
	HazardTimer = 0.f;

	const FBox Hot = HotBurner();
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && Hot.IsInsideOrOn(Ant->GetActorLocation()))
		{
			UGameplayStatics::ApplyDamage(Ant, 9.f, nullptr, this, UDamageType::StaticClass());
		}
	}
}
