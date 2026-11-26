// The "release" layer of the kitchen: the house shell with real holes (an ant crack to the nest outside,
// a crack under the window, crawlable air ducts carved into the walls and the ceiling), the street
// outside the west wall with the colony's nest, extra clutter that ants can use, secret places.
#include "KitchenBuilder.h"
#include "AntCharacter.h"
#include "AntTypes.h"
#include "KitchenArt.h"
#include "Map4Look.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace HousePalette
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	const FLinearColor WallC = Hex(240, 225, 185);
	const FLinearColor Ceiling = Hex(250, 250, 245);
	const FLinearColor Baseboard = Hex(120, 80, 50);
	const FLinearColor Wood = Hex(160, 105, 60);
	const FLinearColor DarkWood = Hex(110, 70, 40);
	const FLinearColor Steel = Hex(175, 180, 185);
	const FLinearColor DarkSteel = Hex(70, 72, 78);
	const FLinearColor Black = Hex(20, 20, 22);
	const FLinearColor White = Hex(245, 245, 245);
	const FLinearColor Dirt = Hex(95, 65, 40);
	const FLinearColor DarkDirt = Hex(60, 40, 25);
	const FLinearColor Grass = Hex(86, 142, 60);
	const FLinearColor GrassDark = Hex(52, 104, 38);
	const FLinearColor Soil = Hex(110, 80, 50);
	const FLinearColor FenceC = Hex(160, 120, 78);
	const FLinearColor Concrete = Hex(168, 165, 158);
	const FLinearColor Asphalt = Hex(58, 58, 62);
	const FLinearColor Fabric = Hex(170, 40, 50);
	const FLinearColor Duct = Hex(150, 152, 150);
}
using namespace HousePalette;

namespace
{
	/** Subtracts an axis-aligned cavity from a set of solid boxes (the complement is at most 6 slabs per box). */
	void Carve(TArray<FBox>& Pieces, const FBox& Cavity)
	{
		TArray<FBox> Out;
		for (const FBox& B : Pieces)
		{
			if (!B.Intersect(Cavity))
			{
				Out.Add(B);
				continue;
			}
			const FBox C(FVector::Max(B.Min, Cavity.Min), FVector::Min(B.Max, Cavity.Max));
			auto Push = [&Out](const FVector& Mn, const FVector& Mx)
			{
				if (Mx.X - Mn.X > 0.5f && Mx.Y - Mn.Y > 0.5f && Mx.Z - Mn.Z > 0.5f)
				{
					Out.Add(FBox(Mn, Mx));
				}
			};
			Push(B.Min, FVector(C.Min.X, B.Max.Y, B.Max.Z));                              // x below
			Push(FVector(C.Max.X, B.Min.Y, B.Min.Z), B.Max);                              // x above
			Push(FVector(C.Min.X, B.Min.Y, B.Min.Z), FVector(C.Max.X, C.Min.Y, B.Max.Z)); // y below
			Push(FVector(C.Min.X, C.Max.Y, B.Min.Z), FVector(C.Max.X, B.Max.Y, B.Max.Z)); // y above
			Push(FVector(C.Min.X, C.Min.Y, B.Min.Z), FVector(C.Max.X, C.Max.Y, C.Min.Z)); // z below
			Push(FVector(C.Min.X, C.Min.Y, C.Max.Z), FVector(C.Max.X, C.Max.Y, B.Max.Z)); // z above
		}
		Pieces = Out;
	}
}

// ---------------------------------------------------------------------------------------------
// Walls, ceiling, baseboards: one solid shell with the real holes carved out of it.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildShell()
{
	// Holes an ant can actually crawl through.
	const FBox NestCrack(FVector(-7250.f, -2750.f, 0.f), FVector(-6950.f, -2250.f, 250.f));      // west wall, to the nest outside
	const FBox WindowCrack(FVector(-4600.f, -5250.f, 0.f), FVector(-4400.f, -4950.f, 120.f));    // south wall, under the window, to the yard
	// Air duct 1: floor grille on the south wall -> up inside the wall -> across the ceiling -> down the north wall -> grille above the upper cabinets.
	const FBox S1(FVector(-3300.f, -5180.f, 80.f), FVector(-2900.f, -5020.f, 5580.f));
	const FBox G1(FVector(-3300.f, -5020.f, 80.f), FVector(-2900.f, -4990.f, 380.f));
	const FBox Socket(FVector(-3200.f, -5020.f, 560.f), FVector(-3000.f, -4990.f, 660.f));   // a cracked wall socket: the back way into the same shaft
	const FBox D1(FVector(-3300.f, -5180.f, 5420.f), FVector(-2900.f, 5180.f, 5580.f));
	const FBox N1(FVector(-3300.f, 5020.f, 4480.f), FVector(-2900.f, 5180.f, 5580.f));
	const FBox R1(FVector(-4700.f, 5020.f, 4480.f), FVector(-2900.f, 5180.f, 4640.f));
	const FBox G2(FVector(-4700.f, 4990.f, 4480.f), FVector(-4300.f, 5020.f, 4640.f));
	// Air duct 2: floor grille on the east wall -> up -> across the ceiling (west, then north) -> down behind the fridge -> grille above the fridge.
	const FBox S2(FVector(7020.f, 200.f, 80.f), FVector(7180.f, 600.f, 5580.f));
	const FBox G3(FVector(6990.f, 200.f, 80.f), FVector(7020.f, 600.f, 380.f));
	const FBox D2a(FVector(3500.f, 200.f, 5420.f), FVector(7180.f, 600.f, 5580.f));
	const FBox D2b(FVector(3500.f, 200.f, 5420.f), FVector(3900.f, 5180.f, 5580.f));
	const FBox N2(FVector(3500.f, 5020.f, 3700.f), FVector(3900.f, 5180.f, 5580.f));
	const FBox G4(FVector(3500.f, 4990.f, 3700.f), FVector(3900.f, 5020.f, 3860.f));
	// A loose tile by the trash: a pit under the floor. Level v2: 300 deep (it was 100: the cargo dropped into it sat inside the floor slab).
	const FBox Pit(FVector(4700.f, 1700.f, -300.f), FVector(5300.f, 2300.f, 1.f));
	// Level v2: two more ways through the kitchen walls.
	const FBox NorthHole(FVector(-7250.f, 4770.f, 0.f), FVector(-6950.f, 4980.f, 150.f));   // west wall: the front lawn <-> the corridor behind the cabinets
	const FBox EastHole(FVector(6950.f, 880.f, 0.f), FVector(7250.f, 1080.f, 160.f));       // east wall, behind the cardboard box: kitchen <-> living room

	DuctWind.Reset();
	// (box, direction the draft blows). The fan pulls air from the floor grilles up and out above the cabinets.
	DuctWind.Add({ S1, FVector(0.f, 0.f, 1.f) });
	DuctWind.Add({ FBox(FVector(-3300.f, -5020.f, 5420.f), FVector(-2900.f, 5020.f, 5580.f)), FVector(0.f, 1.f, 0.f) });
	DuctWind.Add({ FBox(FVector(-3300.f, 5020.f, 4640.f), FVector(-2900.f, 5180.f, 5580.f)), FVector(0.f, 0.f, -1.f) });
	DuctWind.Add({ R1, FVector(-1.f, 0.f, 0.f) });
	DuctWind.Add({ S2, FVector(0.f, 0.f, 1.f) });
	DuctWind.Add({ FBox(FVector(3900.f, 200.f, 5420.f), FVector(7020.f, 600.f, 5580.f)), FVector(-1.f, 0.f, 0.f) });
	DuctWind.Add({ FBox(FVector(3500.f, 600.f, 5420.f), FVector(3900.f, 5020.f, 5580.f)), FVector(0.f, 1.f, 0.f) });
	DuctWind.Add({ FBox(FVector(3500.f, 5020.f, 3860.f), FVector(3900.f, 5180.f, 5580.f)), FVector(0.f, 0.f, -1.f) });

	auto Solid = [this](const FBox& B, std::initializer_list<FBox> Cavities, const FLinearColor& C)
	{
		TArray<FBox> P;
		P.Add(B);
		for (const FBox& Cav : Cavities)
		{
			Carve(P, Cav);
		}
		for (const FBox& Q : P)
		{
			Box(Q.Min, Q.Max, C);
		}
	};

	// North wall is thick: a mouse room hides inside it behind the fridge (x 3500..4300).
	Solid(FBox(FVector(-7200.f, 5000.f, 0.f), FVector(3500.f, 5600.f, 5400.f)), { N1, R1, G2 }, WallC);
	Box(FVector(4300.f, 5000.f, 0.f), FVector(7200.f, 5600.f, 5400.f), WallC);
	Solid(FBox(FVector(3500.f, 5000.f, 450.f), FVector(4300.f, 5600.f, 5400.f)), { N2, G4 }, WallC);
	Box(FVector(3500.f, 5550.f, 0.f), FVector(4300.f, 5600.f, 450.f), WallC);
	Box(FVector(3500.f, 5000.f, 0.f), FVector(3800.f, 5050.f, 450.f), WallC);       // front slab with the mouse hole
	Box(FVector(3960.f, 5000.f, 0.f), FVector(4300.f, 5050.f, 450.f), WallC);
	Box(FVector(3800.f, 5000.f, 140.f), FVector(3960.f, 5050.f, 450.f), WallC);
	Box(FVector(3500.f, 5000.f, -100.f), FVector(4300.f, 5600.f, 0.f), Hex(120, 95, 70));   // mouse room floor
	// South wall with the window opening; the vent shaft and the crack under the window are carved out.
	Solid(FBox(FVector(-7200.f, -5200.f, 0.f), FVector(-1500.f, -5000.f, 5400.f)), { S1, G1, Socket, WindowCrack }, WallC);
	Box(FVector(1500.f, -5200.f, 0.f), FVector(7200.f, -5000.f, 5400.f), WallC);
	Box(FVector(-1500.f, -5200.f, 0.f), FVector(1500.f, -5000.f, 2200.f), WallC);
	Box(FVector(-1500.f, -5200.f, 4200.f), FVector(1500.f, -5000.f, 5400.f), WallC);
	// West wall with the ants' crack at floor level.
	Solid(FBox(FVector(-7200.f, -5000.f, 0.f), FVector(-7000.f, 5000.f, 5400.f)), { NestCrack, NorthHole }, WallC);
	Box(FVector(-7200.f, 4770.f, -100.f), FVector(-7000.f, 4980.f, 0.f), Hex(120, 95, 70));   // the floor of the north hole (the floor slab stops at the wall's inner face)
	// ... and of the nest crack: between the lawn (up to x -7200) and the floor slab (from x -7000) there was a slot 200 wide and
	// bottomless. Ants crossed it by luck; the bots that walk out of the nest fell into it and died below the world.
	Box(FVector(-7200.f, -2750.f, -100.f), FVector(-7000.f, -2250.f, 0.f), Hex(120, 95, 70));
	Box(FVector(-4600.f, -5200.f, -100.f), FVector(-4400.f, -5000.f, 0.f), Hex(120, 95, 70));   // the same slot under the window crack (lawn up to y -5200, slab from y -5000)
	// East wall with a doorway into the living room (y -4800..-3200, 3800 high) and the second vent shaft.
	Box(FVector(7000.f, -5000.f, 0.f), FVector(7200.f, -4800.f, 5400.f), WallC);
	Solid(FBox(FVector(7000.f, -3200.f, 0.f), FVector(7200.f, 5000.f, 5400.f)), { S2, G3, EastHole }, WallC);
	Box(FVector(7000.f, -4800.f, 3800.f), FVector(7200.f, -3200.f, 5400.f), WallC);
	if (!bArt)   // modelled: SM_L_DoorFrame with a real door leaf (AKitchenMover; it starts open)
	{
		Box(FVector(6960.f, -4840.f, 0.f), FVector(7240.f, -4800.f, 3840.f), Wood);   // door frame
		Box(FVector(6960.f, -3200.f, 0.f), FVector(7240.f, -3160.f, 3840.f), Wood);
		Box(FVector(6960.f, -4840.f, 3800.f), FVector(7240.f, -3160.f, 3840.f), Wood);
	}
	// Ceiling with both ducts inside it.
	Solid(FBox(FVector(-7200.f, -5200.f, 5400.f), FVector(7200.f, 5600.f, 5600.f)), { S1, D1, N1, S2, D2a, D2b, N2 }, Ceiling);
	// Floor: one collision slab with the pit under the loose tile.
	Solid(FBox(FVector(-7000.f, -5000.f, -100.f), FVector(7000.f, 5000.f, 0.f)), { Pit }, Hex(225, 215, 195));
	Box(FVector(4700.f, 1700.f, -400.f), FVector(5300.f, 2300.f, -300.f), DarkDirt);          // pit floor (v2: at -300)
	Box(FVector(4600.f, 1600.f, -400.f), FVector(4700.f, 2400.f, 0.f), DarkDirt);             // pit walls
	Box(FVector(5300.f, 1600.f, -400.f), FVector(5400.f, 2400.f, 0.f), DarkDirt);
	Box(FVector(4700.f, 1600.f, -400.f), FVector(5300.f, 1700.f, 0.f), DarkDirt);
	Box(FVector(4700.f, 2300.f, -400.f), FVector(5300.f, 2400.f, 0.f), DarkDirt);
	// The loose tile itself: lifted on one side, you squeeze under the raised edge.
	Part(EAntShape::Cube, FVector(5000.f, 2000.f, 110.f), FVector(1000.f, 1000.f, 20.f), FRotator(0.f, 0.f, 12.f), Hex(150, 60, 55), true);

	// Baseboards: real collision now, with gaps where the holes and grilles are.
	auto Base = [this](float X0, float Y0, float X1, float Y1) { Box(FVector(X0, Y0, 0.f), FVector(X1, Y1, 220.f), Baseboard, true); };
	if (!bArt)   // modelled: SM_K_Skirting runs with the same gaps
	{
		Base(-7000.f, -5000.f, -4600.f, -4970.f);     // south, west of the window crack
		Base(-4400.f, -5000.f, -3300.f, -4970.f);     // ... up to the vent grille
		Base(-2900.f, -5000.f, 7000.f, -4970.f);      // ... east of it
		Base(6970.f, -3160.f, 7000.f, 200.f);         // east, below the grille
		Base(6970.f, 600.f, 7000.f, 5000.f);          // east, above it
		Base(6970.f, -5000.f, 7000.f, -4840.f);
		Base(-7000.f, -5000.f, -6970.f, -2750.f);     // west, south of the crack
		Base(-7000.f, -2250.f, -6970.f, 5000.f);      // west, north of it
	}

	BuildDucts();
}

void AKitchenBuilder::SolidBox(const FBox& B, std::initializer_list<FBox> Cavities, const FLinearColor& Color)
{
	TArray<FBox> P;
	P.Add(B);
	for (const FBox& Cav : Cavities)
	{
		Carve(P, Cav);
	}
	for (const FBox& Q : P)
	{
		Box(Q.Min, Q.Max, Color);
	}
}

// ---------------------------------------------------------------------------------------------
// Level v2: the edges of the world. v1 had none on four sides: an ant walked off the lawn or the road and fell for ever.
// The yards are fenced by SM_Y_FencePanel runs (placement table); just outside every fence, and round the street, stands an
// invisible wall. It blocks pawns and food but is not a "surface" (not WorldStatic): an ant that climbed a fence cannot cling
// to it and cannot get past it. (An ant that falls below z -800 all the same dies, as before: AAntCharacter::Tick.)
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildEdges()
{
	static const float Edges[][4] =
	{
		{ -15420.f, -5320.f, -15300.f, 5720.f },   // the street: behind the car
		{ -15420.f, 5600.f, -9460.f, 5720.f },     // the street: north end of the pavement and the road
		{ -9460.f, 5670.f, -7140.f, 5790.f },      // behind the front lawn's north fence, round the house corner
		{ -15420.f, -5320.f, -9400.f, -5200.f },   // the street: south end
		{ -9590.f, -8990.f, -9470.f, -5200.f },    // behind yard A's west fence
		{ -9590.f, -8990.f, 5140.f, -8870.f },     // behind yard A's south fence
		{ 5010.f, -12390.f, 5130.f, -8860.f },     // behind yard B's west fence
		{ 5010.f, -12390.f, 20590.f, -12270.f },   // behind yard B's south fence
		{ 20470.f, -12390.f, 20590.f, -8990.f },   // behind yard B's east fence
		{ 20190.f, -9200.f, 20590.f, -9080.f },    // the notch between the house corner and that wall: the lawn ends at y -9200 and
		                                           // an ant that went north along the wall walked off it (found by the "edges" sweep)
	};
	// Ground under the front lawn's north fence and a little beyond it: an ant that squeezes between the pickets stands on
	// grass until it meets the invisible wall (the lawn box ended exactly on the fence line).
	Box(FVector(-9460.f, 5600.f, -200.f), FVector(-7140.f, 5800.f, 0.f), Grass);
	for (const auto& E : Edges)
	{
		if (UStaticMeshComponent* C = Box(FVector(E[0], E[1], -400.f), FVector(E[2], E[3], 14000.f), White, true))
		{
			C->SetVisibility(false);
			C->SetHiddenInGame(true);
			C->SetCastShadow(false);
			C->SetCollisionObjectType(ECC_WorldDynamic);   // solid, but nothing to crawl on (UAntMovementComponent::IsStickable)
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Vent grilles (slats you squeeze between), the fan hum light inside, the drafts are in Tick.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildDucts()
{
	// A grille: frame + horizontal slats with 48-unit gaps (an ant is ~15 long). Axis: 0 = faces -Y/+Y (on an X-wall), 1 = faces ±X.
	auto Grille = [this](const FVector& Min, const FVector& Max, bool bOnYWall)
	{
		const float T = 10.f;
		if (bOnYWall)
		{
			// Opening in a wall that runs along X: slats span X, stacked in Z, thin in Y.
			Box(FVector(Min.X - 30.f, Min.Y, Min.Z - 30.f), FVector(Min.X, Max.Y, Max.Z + 30.f), Duct);
			Box(FVector(Max.X, Min.Y, Min.Z - 30.f), FVector(Max.X + 30.f, Max.Y, Max.Z + 30.f), Duct);
			Box(FVector(Min.X - 30.f, Min.Y, Min.Z - 30.f), FVector(Max.X + 30.f, Max.Y, Min.Z), Duct);
			Box(FVector(Min.X - 30.f, Min.Y, Max.Z), FVector(Max.X + 30.f, Max.Y, Max.Z + 30.f), Duct);
			for (float Z = Min.Z + 48.f; Z < Max.Z - 20.f; Z += 48.f + T)
			{
				Part(EAntShape::Cube, FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, Z + T * 0.5f), FVector(Max.X - Min.X, Max.Y - Min.Y, T), FRotator(25.f, 0.f, 0.f), Duct, true);
			}
		}
		else
		{
			Box(FVector(Min.X, Min.Y - 30.f, Min.Z - 30.f), FVector(Max.X, Min.Y, Max.Z + 30.f), Duct);
			Box(FVector(Min.X, Max.Y, Min.Z - 30.f), FVector(Max.X, Max.Y + 30.f, Max.Z + 30.f), Duct);
			Box(FVector(Min.X, Min.Y - 30.f, Min.Z - 30.f), FVector(Max.X, Max.Y + 30.f, Min.Z), Duct);
			Box(FVector(Min.X, Min.Y - 30.f, Max.Z), FVector(Max.X, Max.Y + 30.f, Max.Z + 30.f), Duct);
			for (float Z = Min.Z + 48.f; Z < Max.Z - 20.f; Z += 48.f + T)
			{
				Part(EAntShape::Cube, FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, Z + T * 0.5f), FVector(Max.X - Min.X, Max.Y - Min.Y, T), FRotator(0.f, 0.f, 25.f), Duct, true);
			}
		}
	};
	if (!bArt)   // modelled: SM_K_VentGrille / SM_K_VentGrilleSmall, slats to squeeze between
	{
		Grille(FVector(-3300.f, -5020.f, 80.f), FVector(-2900.f, -4990.f, 380.f), true);     // south floor
		Grille(FVector(-4700.f, 4990.f, 4480.f), FVector(-4300.f, 5020.f, 4640.f), true);    // above the upper cabinets
		Grille(FVector(6990.f, 200.f, 80.f), FVector(7020.f, 600.f, 380.f), false);          // east floor
		Grille(FVector(3500.f, 4990.f, 3700.f), FVector(3900.f, 5020.f, 3860.f), true);      // above the fridge
	}

	// The wall socket: a cracked plate, the gap at the bottom leads into the shaft behind it.
	Box(FVector(-3250.f, -5000.f, 660.f), FVector(-2950.f, -4984.f, 720.f), White);
	Box(FVector(-3250.f, -5000.f, 480.f), FVector(-3200.f, -4984.f, 660.f), White);
	Box(FVector(-3000.f, -5000.f, 480.f), FVector(-2950.f, -4984.f, 660.f), White);
	Box(FVector(-3250.f, -5000.f, 480.f), FVector(-2950.f, -4984.f, 560.f), White);
	Box(FVector(-3140.f, -4986.f, 600.f), FVector(-3120.f, -4983.f, 640.f), Black, false);   // the two holes
	Box(FVector(-3080.f, -4986.f, 600.f), FVector(-3060.f, -4983.f, 640.f), Black, false);

	// Dust and a dead fly inside the ducts, dim light leaking in from the grilles, the fan.
	Ball(FVector(-3100.f, -5100.f, 110.f), FVector(60.f, 50.f, 25.f), Hex(150, 145, 140));
	Ball(FVector(-3100.f, 2000.f, 5430.f), FVector(70.f, 55.f, 25.f), Hex(150, 145, 140));
	Ball(FVector(-3050.f, 3500.f, 5440.f), FVector(28.f, 22.f, 18.f), Hex(40, 40, 45));   // the fly
	Ball(FVector(5500.f, 400.f, 5430.f), FVector(70.f, 55.f, 25.f), Hex(150, 145, 140));
	Lamp(FVector(-3100.f, -4900.f, 200.f), 1.2f, FLinearColor(0.8f, 0.9f, 1.f), 900.f, false);
	Lamp(FVector(-3100.f, 0.f, 5500.f), 0.8f, FLinearColor(0.7f, 0.8f, 1.f), 2500.f, false);
	Lamp(FVector(5300.f, 400.f, 5500.f), 0.8f, FLinearColor(0.7f, 0.8f, 1.f), 2500.f, false);
	// Fan blades in the ceiling runs (decor; the "fan" is the draft).
	Part(EAntShape::Cube, FVector(-3100.f, 0.f, 5500.f), FVector(380.f, 20.f, 150.f), FRotator(0.f, 0.f, 30.f), DarkSteel, false);
	Part(EAntShape::Cube, FVector(-3100.f, 0.f, 5500.f), FVector(380.f, 20.f, 150.f), FRotator(0.f, 0.f, -30.f), DarkSteel, false);
}

// ---------------------------------------------------------------------------------------------
// Outside the west wall: a strip of lawn with the colony's nest, a fence with a gate, the sidewalk,
// the street with a parked car, a gutter downspout to climb to the roof gutter.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildWestStreet()
{
	// Lawn strip between the house and the fence (joins the back yard at the SW corner).
	Box(FVector(-9400.f, -5200.f, -200.f), FVector(-7200.f, 5600.f, 0.f), Grass);
	// House facade above the kitchen wall, a roof edge with a gutter, a downspout at the corner (climb it!).
	if (!bArt)   // modelled: facade boards, gutter (a real channel), roof edge, hollow drainpipe
	{
		Box(FVector(-7260.f, -5300.f, 5400.f), FVector(-7200.f, 5700.f, 9000.f), WallC);
		Box(FVector(-7260.f, 5600.f, 0.f), FVector(-7200.f, 5700.f, 5400.f), WallC);
		Box(FVector(-7260.f, -5300.f, 0.f), FVector(-7200.f, -5200.f, 5400.f), WallC);
		Box(FVector(-7500.f, -5400.f, 8900.f), FVector(-7200.f, 5800.f, 8960.f), Steel);              // gutter bottom
		Box(FVector(-7500.f, -5400.f, 8960.f), FVector(-7440.f, 5800.f, 9150.f), Steel);              // gutter lip
		Box(FVector(-7500.f, -5400.f, 9150.f), FVector(-7200.f, 5800.f, 9200.f), Hex(120, 60, 50));   // roof tiles edge
		Cyl(FVector(-7380.f, -5250.f, 0.f), 90.f, 8900.f, Steel);                                     // downspout
		Box(FVector(-7460.f, -5330.f, 7200.f), FVector(-7300.f, -5170.f, 7260.f), Steel);              // bracket
		Box(FVector(-7460.f, -5330.f, 3600.f), FVector(-7300.f, -5170.f, 3660.f), Steel);
		Cyl(FVector(-7380.f, -5250.f, 8900.f), 120.f, 60.f, Steel, false);
	}
	Ball(FVector(-7350.f, 1200.f, 8990.f), FVector(90.f, 60.f, 40.f), Hex(80, 70, 50));            // leaves and a dead beetle in the gutter
	Ball(FVector(-7350.f, -3000.f, 8990.f), FVector(110.f, 70.f, 40.f), Hex(80, 70, 50));
	Ball(FVector(-7340.f, 3600.f, 8990.f), FVector(70.f, 45.f, 35.f), Hex(30, 30, 35));

	// The colony's nest: a crater of dirt right outside the crack (grows with the colony's stores).
	const FVector Nest = NestLocation(0);
	for (int32 i = 0; i < 10; ++i)
	{
		if (i == 0 || i == 9) { continue; }   // the ring opens towards the house: the worn path to the crack
		const float A = i * (2.f * PI / 10.f) + 0.314f;
		const float R = 620.f + 50.f * FMath::Sin(i * 2.1f);
		AddMound(0, Ball(FVector(Nest.X + FMath::Cos(A) * R, Nest.Y + FMath::Sin(A) * R, -60.f), FVector(300.f, 260.f, 170.f + 30.f * FMath::Cos(i * 1.7f)), i % 3 == 0 ? DarkDirt : Dirt));
	}
	UStaticMeshComponent* Hole = Cyl(FVector(Nest.X, Nest.Y, 0.f), 320.f, 3.f, Black, false);      // the entrance hole
	if (Hole && Map4Look::Enabled())
	{
		// KAN-39: map v4 turns every flat builder disc into a spill (SM_M4_LiquidDisc) painted in its colour, and this one's
		// near-black paint with the spill's gloss was a black mirror right where the ants (and a joining guest) appear: the
		// floor round an ant standing at the nest went black and reflective. The hole is dark, matte, trodden earth.
		if (const Map4Look::FLook* Earth = Map4Look::Named(TEXT("Soil")))
		{
			Hole->SetMaterial(0, Map4Look::Get(*Earth, FLinearColor(0.32f, 0.3f, 0.28f)));
		}
		Hole->SetCastShadow(false);
	}
	Cyl(FVector(Nest.X, Nest.Y, 0.f), NestRadius(0), 2.f, DarkDirt, false);                       // trampled ring
	for (int32 i = 0; i < 12; ++i)                                                                   // dirt clods and grains
	{
		const float A = i * 0.9f;
		Ball(FVector(Nest.X + FMath::Cos(A) * (700.f + (i % 3) * 90.f), Nest.Y + FMath::Sin(A) * (650.f + (i % 2) * 120.f), 10.f), FVector(40.f + (i % 4) * 12.f, 35.f, 25.f), Dirt);
	}
	// Dirt dragged into the kitchen through the crack.
	for (int32 i = 0; i < 8; ++i)
	{
		const float T = i / 7.f;
		Ball(FMath::Lerp(FVector(-7150.f, -2500.f, 3.f), FVector(-6400.f, -2450.f, 3.f), T) + FVector(0.f, FMath::Sin(T * 7.f) * 90.f, 0.f), FVector(60.f - i * 5.f, 40.f, 3.f), Dirt);
	}
	Lamp(FVector(Nest.X, Nest.Y, 350.f), 2.f, FLinearColor(1.f, 0.75f, 0.45f), 1800.f, false);

	if (!bArt)   // modelled: SM_Y_FencePanel run and the gate (AKitchenMover; it starts open)
	{
		// Fence along the sidewalk, with a gate left ajar (another way out to the street).
		for (float Y = -5200.f; Y <= 5600.f; Y += 1100.f)
		{
			if (Y > 3200.f && Y < 4500.f) { continue; }   // the gate
			Box(FVector(-9460.f, Y - 60.f, 0.f), FVector(-9340.f, Y + 60.f, 2600.f), FenceC);
		}
		Box(FVector(-9440.f, -5200.f, 600.f), FVector(-9360.f, 3200.f, 760.f), FenceC);
		Box(FVector(-9440.f, -5200.f, 1900.f), FVector(-9360.f, 3200.f, 2060.f), FenceC);
		Box(FVector(-9440.f, 4500.f, 600.f), FVector(-9360.f, 5600.f, 760.f), FenceC);
		Box(FVector(-9440.f, 4500.f, 1900.f), FVector(-9360.f, 5600.f, 2060.f), FenceC);
		Part(EAntShape::Cube, FVector(-9800.f, 3600.f, 1300.f), FVector(80.f, 1150.f, 2500.f), FRotator(0.f, -50.f, 0.f), FenceC, true);   // the gate, swung open
	}
	if (!bArt)
	{
		// The old big yard's north fence (y -5200) closed it off from the sidewalk. Level v2: that yard is cut to a strip by the
		// house and fenced by SM_Y_FencePanel runs from the placement table; the street ends at invisible walls (BuildEdges).
		for (float X = -12000.f; X <= -9400.f; X += 1100.f)
		{
			if (X > -10800.f && X < -10000.f) { continue; }
			Box(FVector(X - 60.f, -5260.f, 0.f), FVector(X + 60.f, -5140.f, 2600.f), FenceC);
		}
		Box(FVector(-12000.f, -5240.f, 600.f), FVector(-10800.f, -5160.f, 760.f), FenceC);
		Box(FVector(-10000.f, -5240.f, 600.f), FVector(-9400.f, -5160.f, 760.f), FenceC);
		Box(FVector(-12000.f, -5240.f, 1900.f), FVector(-10800.f, -5160.f, 2060.f), FenceC);
		Box(FVector(-10000.f, -5240.f, 1900.f), FVector(-9400.f, -5160.f, 2060.f), FenceC);
	}

	if (!bArt)   // modelled: pavement, kerb, road, manhole, storm drain, the car on its wheels, hydrant, street lamp
	{
		// Sidewalk slabs, the curb, the road.
		for (float Y = -5200.f; Y < 5600.f; Y += 1200.f)
		{
			Box(FVector(-11000.f, Y + 8.f, -200.f), FVector(-9400.f, FMath::Min(Y + 1192.f, 5600.f), 0.f), Concrete);
		}
		Box(FVector(-11100.f, -5200.f, -230.f), FVector(-11000.f, 5600.f, 0.f), Hex(140, 138, 130));   // curb
		Box(FVector(-16500.f, -5200.f, -260.f), FVector(-11100.f, 5600.f, -230.f), Asphalt);           // road
		for (float Y = -4800.f; Y < 5600.f; Y += 1600.f)
		{
			Box(FVector(-13900.f, Y, -229.f), FVector(-13700.f, Y + 800.f, -226.f), White, false);     // lane dashes
		}
		Cyl(FVector(-12300.f, 2600.f, -232.f), 560.f, 10.f, DarkSteel);                                 // manhole cover
		Box(FVector(-11100.f, -3400.f, -260.f), FVector(-10900.f, -2600.f, -229.f), Black);              // storm drain at the curb
		for (int32 i = 0; i < 6; ++i)
		{
			Box(FVector(-11100.f, -3380.f + i * 130.f, -230.f), FVector(-10900.f, -3340.f + i * 130.f, -224.f), DarkSteel);
		}
		// A parked car: shade underneath, a world to explore for a later day.
		Box(FVector(-15200.f, -4200.f, 600.f), FVector(-11900.f, 4300.f, 1900.f), Hex(60, 90, 160));
		Box(FVector(-14700.f, -2300.f, 1900.f), FVector(-12400.f, 2300.f, 2850.f), Hex(60, 90, 160));
		Box(FVector(-14650.f, -2250.f, 2000.f), FVector(-12450.f, 2250.f, 2750.f), Hex(25, 30, 40), false);   // glass
		for (const FVector W : { FVector(-14700.f, -2900.f, 0.f), FVector(-12400.f, -2900.f, 0.f), FVector(-14700.f, 3000.f, 0.f), FVector(-12400.f, 3000.f, 0.f) })
		{
			Part(EAntShape::Cylinder, W + FVector(0.f, 0.f, 650.f), FVector(1300.f, 1300.f, 450.f), FRotator(90.f, 0.f, 0.f), Black, true);
		}
		Box(FVector(-15000.f, 4300.f, 900.f), FVector(-14600.f, 4320.f, 1200.f), Hex(255, 240, 200), false);   // headlights
		Box(FVector(-12500.f, 4300.f, 900.f), FVector(-12100.f, 4320.f, 1200.f), Hex(255, 240, 200), false);
		Box(FVector(-13900.f, 4320.f, 500.f), FVector(-13200.f, 4330.f, 800.f), White, false);               // plate
		// Street furniture: a hydrant, a lamp post, a dropped ice cream and a trodden gum on the sidewalk.
		Cyl(FVector(-10300.f, 4400.f, 0.f), 180.f, 900.f, Hex(200, 40, 40));
		Cyl(FVector(-10300.f, 4400.f, 900.f), 230.f, 120.f, Hex(200, 40, 40));
		Part(EAntShape::Cylinder, FVector(-10300.f, 4400.f, 650.f), FVector(160.f, 700.f, 160.f), FRotator(0.f, 0.f, 90.f), Hex(200, 40, 40), true);
		Cyl(FVector(-10700.f, -4600.f, 0.f), 90.f, 7200.f, DarkSteel);
		Cyl(FVector(-10700.f, -4600.f, 0.f), 220.f, 100.f, DarkSteel);
		Box(FVector(-10900.f, -4800.f, 7200.f), FVector(-10500.f, -4400.f, 7450.f), Hex(240, 240, 230), false);
	}
	Lamp(FVector(-10700.f, -4600.f, 7100.f), 6.f, FLinearColor(1.f, 0.9f, 0.7f), 7000.f, false);
	if (!bArt)   // modelled: SM_S_IceCream, SM_S_Gum
	{
		// The ice cream: cone on its side, scoop melting into a sweet puddle (loot spots on top).
		Part(EAntShape::Cone, FVector(-10100.f, -1300.f, 110.f), FVector(240.f, 240.f, 520.f), FRotator(0.f, 20.f, -80.f), Hex(230, 190, 120), true);
		Ball(FVector(-10400.f, -1500.f, 120.f), FVector(220.f, 220.f, 140.f), Hex(250, 220, 240));
		Cyl(FVector(-10400.f, -1500.f, 0.f), 360.f, 4.f, Hex(245, 200, 225), false);
		Cyl(FVector(-10000.f, 2500.f, 0.f), 170.f, 12.f, Hex(240, 160, 200));                            // chewing gum (sticky!)
	}
	// Grass blades along the strip (no collision).
	UInstancedStaticMeshComponent* Blades = NewObject<UInstancedStaticMeshComponent>(this, TEXT("WestGrass"));
	Blades->SetupAttachment(Root);
	Blades->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cube));
	Blades->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Blades->SetCastShadow(false);
	Blades->SetCanEverAffectNavigation(false);
	Blades->RegisterComponent();
	AntKit::Paint(Blades, GrassDark);
	FRandomStream R(777);
	TArray<FTransform> Xf;
	for (int32 i = 0; i < 900; ++i)
	{
		const FVector P(R.FRandRange(-9300.f, -7300.f), R.FRandRange(-5100.f, 5500.f), 0.f);
		if (FVector::Dist2D(P, Nest) < 950.f) { continue; }
		const float H = R.FRandRange(180.f, 520.f);
		Xf.Add(FTransform(FRotator(R.FRandRange(-18.f, 18.f), R.FRandRange(0.f, 360.f), R.FRandRange(-18.f, 18.f)), P + FVector(0.f, 0.f, H * 0.5f), FVector(0.14f, 0.03f, H / 100.f)));
	}
	if (!bArt || !ScatterTufts(Xf, 777))
	{
		Blades->AddInstances(Xf, false, true);
	}
}

// ---------------------------------------------------------------------------------------------
// Clutter an ant can use: climbing aids, hiding places, things to push around, little stories.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildClutter()
{
	if (!bArt)   // modelled: towel, curtains and rod, spoon, fork, shoe, newspaper, tea bag, matchbox
	{
		// Kitchen towel hanging from the oven handle to the floor: the easy way up onto the stove.
		Part(EAntShape::Cube, FVector(-750.f, 2880.f, 820.f), FVector(600.f, 24.f, 1640.f), FRotator(0.f, 0.f, 0.f), Fabric, true);
		Box(FVector(-1050.f, 2860.f, 1600.f), FVector(-450.f, 2900.f, 1640.f), Fabric);
		// Curtains at the window, pooling on the floor: climb to the sill from either side.
		Part(EAntShape::Cube, FVector(-1750.f, -4940.f, 2700.f), FVector(360.f, 60.f, 5400.f), FRotator(0.f, 0.f, 0.f), Hex(200, 170, 120), true);
		Part(EAntShape::Cube, FVector(1750.f, -4940.f, 2700.f), FVector(360.f, 60.f, 5400.f), FRotator(0.f, 0.f, 0.f), Hex(200, 170, 120), true);
		Ball(FVector(-1750.f, -4850.f, 60.f), FVector(300.f, 200.f, 110.f), Hex(200, 170, 120));
		Ball(FVector(1750.f, -4850.f, 60.f), FVector(300.f, 200.f, 110.f), Hex(200, 170, 120));
		Box(FVector(-2000.f, -4960.f, 5300.f), FVector(2000.f, -4920.f, 5340.f), DarkSteel);                       // curtain rod
		// A spoon leaning against a table leg: a ramp up to the dinner.
		Part(EAntShape::Cube, FVector(-1700.f, -1950.f, 640.f), FVector(90.f, 30.f, 1420.f), FRotator(0.f, 0.f, -24.f), Steel, true);
		Part(EAntShape::Cylinder, FVector(-1990.f, -1950.f, 30.f), FVector(240.f, 160.f, 40.f), FRotator(0.f, 0.f, -10.f), Steel, true);
		// A fork fallen off the table, tines up (hurts? no - a hiding slot).
		Part(EAntShape::Cube, FVector(1200.f, -2400.f, 15.f), FVector(900.f, 80.f, 30.f), FRotator(0.f, 35.f, 0.f), Steel, true);
		// A shoe by the door and a crumpled newspaper (a cave).
		Box(FVector(6200.f, -4600.f, 0.f), FVector(6900.f, -3700.f, 320.f), Hex(60, 45, 35));
		Ball(FVector(6550.f, -4150.f, 320.f), FVector(360.f, 450.f, 200.f), Hex(60, 45, 35));
		Box(FVector(6150.f, -4650.f, 0.f), FVector(6950.f, -3650.f, 40.f), Hex(90, 70, 55));
		Part(EAntShape::Cube, FVector(1200.f, -4300.f, 150.f), FVector(900.f, 1200.f, 12.f), FRotator(0.f, 15.f, 18.f), Hex(235, 230, 215), true);   // newspaper, tented
		Part(EAntShape::Cube, FVector(1250.f, -4350.f, 150.f), FVector(900.f, 1200.f, 12.f), FRotator(0.f, 15.f, -18.f), Hex(235, 230, 215), true);
		// A tea bag left on the counter with a string hanging down to the floor (climb the string).
		Box(FVector(2000.f, 3100.f, 1710.f), FVector(2200.f, 3300.f, 1760.f), Hex(120, 80, 50));
		Box(FVector(2090.f, 2940.f, 0.f), FVector(2110.f, 2960.f, 1710.f), Hex(240, 240, 240));
		// Matchbox on the floor near the stove, half open: a drawer to hide in.
		Box(FVector(-2200.f, 2400.f, 0.f), FVector(-1700.f, 2800.f, 140.f), Hex(60, 90, 160));
		Box(FVector(-1700.f, 2420.f, 10.f), FVector(-1500.f, 2780.f, 120.f), Hex(230, 210, 170));
		Box(FVector(-2200.f, 2400.f, 140.f), FVector(-1500.f, 2800.f, 160.f), Hex(60, 90, 160));
	}
	// Spilled flour by the west counter (soft, white, walk-through), with a rolling pin.
	Cyl(FVector(-6000.f, 2300.f, 0.f), 520.f, 14.f, White, false);
	if (!bArt)   // modelled: rolling pin, tennis ball, bone
	{
		Part(EAntShape::Cylinder, FVector(-5400.f, 1900.f, 140.f), FVector(280.f, 280.f, 1400.f), FRotator(0.f, 0.f, 90.f), Wood, true);
		// Dog's chewed tennis ball under the table and a bone by the bowls.
		Ball(FVector(500.f, -400.f, 190.f), FVector(190.f, 190.f, 190.f), Hex(200, 230, 60));
		Part(EAntShape::Cylinder, FVector(-3100.f, -4300.f, 60.f), FVector(120.f, 120.f, 700.f), FRotator(0.f, 0.f, 90.f), Hex(235, 225, 200), true);
		Ball(FVector(-3450.f, -4300.f, 60.f), FVector(100.f, 80.f, 70.f), Hex(235, 225, 200));
		Ball(FVector(-2750.f, -4300.f, 60.f), FVector(100.f, 80.f, 70.f), Hex(235, 225, 200));
	}
	if (!bArt)   // modelled: sock, coin, charger, yarn ball and thread, the plate of biscuits; the TV stand has a real hole
	{
		// Living room: a dropped sock (fabric tunnel), a coin, a phone charger cable up to the sofa.
		Part(EAntShape::Cylinder, FVector(9000.f, -2000.f, 90.f), FVector(260.f, 260.f, 1100.f), FRotator(0.f, 30.f, 90.f), Hex(90, 90, 110), true);
		Cyl(FVector(11500.f, -5500.f, 0.f), 140.f, 10.f, Hex(210, 170, 70));
		Part(EAntShape::Cube, FVector(17300.f, -600.f, 250.f), FVector(16.f, 1600.f, 16.f), FRotator(0.f, 0.f, -30.f), White, true);
		Box(FVector(17000.f, -1400.f, 0.f), FVector(17400.f, -1200.f, 140.f), White);
		// Grandma's ball of yarn on the floor, the thread running up to her lap: climb it to the armchair.
		Ball(FVector(10600.f, -5000.f, 220.f), FVector(220.f, 220.f, 220.f), Hex(200, 60, 90));
		{
			const FVector A(10600.f, -5000.f, 440.f), B(9670.f, -5500.f, 1220.f);
			Part(EAntShape::Cylinder, (A + B) * 0.5f, FVector(14.f, 14.f, (float)FVector::Dist(A, B)), FRotationMatrix::MakeFromZ(B - A).Rotator(), Hex(200, 60, 90), true);
		}
		Cyl(FVector(9250.f, -4800.f, 1200.f), 200.f, 20.f, White);                                      // cookie plate on the armrest
		Cyl(FVector(9250.f, -4800.f, 1220.f), 110.f, 25.f, Hex(190, 130, 70));
		// Behind the TV: a hole gnawed into the hollow TV stand's back (another way in).
		Box(FVector(13780.f, -7720.f, 80.f), FVector(13860.f, -7700.f, 260.f), Black, false);
	}
}

// ---------------------------------------------------------------------------------------------
// Appliances with an inside: the dishwasher, the microwave, the pedal bin (their doors/lid are gadgets).
// Plus the ways up that de_rats taught us: a utensil rail, a spice rack of little shelves.
// ---------------------------------------------------------------------------------------------

void AKitchenBuilder::BuildAppliances()
{
	auto Solid = [this](const FBox& B, std::initializer_list<FBox> Cavities, const FLinearColor& C)
	{
		TArray<FBox> P;
		P.Add(B);
		for (const FBox& Cav : Cavities)
		{
			Carve(P, Cav);
		}
		for (const FBox& Q : P)
		{
			Box(Q.Min, Q.Max, C);
		}
	};

	// Dishwasher: a hollow box under the counter right of the stove; the door (gadget) drops into a ramp.
	if (!bArt)   // modelled: SM_K_Dishwasher (same cavity)
	{
		Solid(FBox(FVector(0.f, 3000.f, 120.f), FVector(1300.f, 4750.f, 1650.f)), { FBox(FVector(60.f, 2990.f, 160.f), FVector(1240.f, 4700.f, 1600.f)) }, Hex(200, 205, 210));
	}
	Box(FVector(60.f, 3040.f, 160.f), FVector(1240.f, 4700.f, 190.f), Hex(120, 170, 220), false);        // rinse water on the floor
	if (!bArt)   // modelled: the two racks roll out (AKitchenMover) and carry the plates and the bowl
	{
		for (float Z : { 400.f, 1050.f })                                                                       // two wire racks
		{
			for (float X = 120.f; X <= 1180.f; X += 212.f)
			{
				Box(FVector(X - 7.f, 3080.f, Z), FVector(X + 7.f, 4660.f, Z + 14.f), Steel);
			}
			for (float Y = 3100.f; Y <= 4660.f; Y += 390.f)
			{
				Box(FVector(80.f, Y - 7.f, Z), FVector(1220.f, Y + 7.f, Z + 14.f), Steel);
			}
			for (float X = 100.f; X <= 1200.f; X += 550.f)                                                      // rack posts
			{
				Box(FVector(X - 10.f, 3080.f, 160.f), FVector(X + 10.f, 3100.f, Z + 14.f), Steel);
				Box(FVector(X - 10.f, 4640.f, 160.f), FVector(X + 10.f, 4660.f, Z + 14.f), Steel);
			}
		}
		for (int32 i = 0; i < 4; ++i)   // dirty plates standing on edge in the lower rack
		{
			const float Y = 3350.f + i * 330.f;
			Part(EAntShape::Cylinder, FVector(650.f, Y, 414.f + 300.f), FVector(600.f, 600.f, 24.f), FRotator(0.f, 0.f, 90.f), Hex(240, 240, 235), true);
			Ball(FVector(650.f, Y - 14.f, 650.f), FVector(120.f, 6.f, 90.f), Hex(200, 120, 60));              // sauce smear
		}
		Part(EAntShape::Cylinder, FVector(300.f, 4300.f, 1064.f + 150.f), FVector(240.f, 240.f, 300.f), FRotator(0.f, 0.f, 180.f), Hex(230, 120, 40), true);   // a mug upside down (upper rack)
		Cyl(FVector(900.f, 4100.f, 1064.f), 260.f, 120.f, White);                                             // a bowl
		Part(EAntShape::Cube, FVector(1000.f, 3500.f, 1064.f + 60.f), FVector(220.f, 30.f, 120.f), FRotator(0.f, 20.f, 0.f), Steel, true);   // a knife standing in the cutlery basket
	}
	// Microwave: hollow, door on the left front edge (gadget), control panel on the right.
	if (!bArt)   // modelled: SM_K_Microwave, its glass plate and the bowl
	{
		Solid(FBox(FVector(1300.f, 4000.f, 1710.f), FVector(2400.f, 4900.f, 2350.f)), { FBox(FVector(1360.f, 3990.f, 1750.f), FVector(2160.f, 4860.f, 2310.f)) }, DarkSteel);
		Cyl(FVector(1760.f, 4425.f, 1750.f), 370.f, 16.f, Hex(200, 220, 230));                                // the glass plate
		Cyl(FVector(1760.f, 4425.f, 1766.f), 230.f, 90.f, White);                                             // a bowl of leftovers
	}
	Ball(FVector(1760.f, 4425.f, 1870.f), FVector(190.f, 190.f, 40.f), Hex(200, 120, 50));
	if (!bArt)
	{
		Box(FVector(2190.f, 3999.f, 1800.f), FVector(2370.f, 4001.f, 2300.f), Hex(30, 30, 35), false);        // panel
		for (int32 i = 0; i < 4; ++i)
		{
			Box(FVector(2210.f, 3996.f, 1860.f + i * 100.f), FVector(2350.f, 4000.f, 1900.f + i * 100.f), Hex(90, 90, 95), false);
		}
	}
	// The pedal bin: a hollow steel cylinder (12 wall panels), the bag and the junk inside, the lid is a gadget.
	const FVector Bin(6000.f, -1500.f, 0.f);
	if (!bArt)   // modelled: SM_K_Bin (hollow, same floor)
	{
		Cyl(Bin, 650.f, 30.f, DarkSteel);
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = i * 30.f;
			const float R = FMath::DegreesToRadians(A);
			Part(EAntShape::Cube, Bin + FVector(FMath::Cos(R) * 630.f, FMath::Sin(R) * 630.f, 770.f), FVector(40.f, 350.f, 1540.f), FRotator(0.f, A, 0.f), Steel, true);
		}
	}
	Ball(Bin + FVector(0.f, 0.f, 460.f), FVector(440.f, 430.f, 430.f), Hex(30, 70, 40));                   // the bag
	Box(Bin + FVector(-250.f, -200.f, 860.f), Bin + FVector(150.f, -50.f, 900.f), Hex(240, 210, 60), false);   // banana peel on the bag
	Ball(Bin + FVector(200.f, 150.f, 940.f), FVector(70.f, 70.f, 110.f), Hex(170, 120, 60));              // apple core
	Box(Bin + FVector(-500.f, 150.f, 30.f), Bin + FVector(-150.f, 350.f, 60.f), Hex(230, 170, 90), false); // pizza crust on the floor
	for (int32 i = 0; i < 5; ++i)                                                                           // fish bones
	{
		Box(Bin + FVector(100.f + i * 60.f, 350.f, 30.f), Bin + FVector(110.f + i * 60.f, 520.f, 36.f), White, false);
	}
	Box(Bin + FVector(80.f, 340.f, 30.f), Bin + FVector(420.f, 352.f, 36.f), White, false);
	Ball(Bin + FVector(-100.f, 300.f, 930.f), FVector(160.f, 110.f, 60.f), Hex(230, 200, 150));           // half a sandwich (the prize)

	if (!bArt)   // modelled: utensil rail with ladle, spatula and whisk, the spice shelves and jars
	{
		// Utensil rail above the stove: ladle and spatula hang down to the stove top; climb them up to the rail and the hood.
		Part(EAntShape::Cylinder, FVector(-750.f, 4900.f, 2760.f), FVector(30.f, 30.f, 1500.f), FRotator(0.f, 0.f, 90.f), Steel, true);
		Box(FVector(-1510.f, 4900.f, 2700.f), FVector(-1490.f, 5000.f, 2800.f), Steel);                      // rail brackets
		Box(FVector(-10.f, 4900.f, 2700.f), FVector(10.f, 5000.f, 2800.f), Steel);
		Part(EAntShape::Cylinder, FVector(-1150.f, 4900.f, 2400.f), FVector(24.f, 24.f, 700.f), FRotator::ZeroRotator, Steel, true);          // ladle handle
		Ball(FVector(-1150.f, 4860.f, 2040.f), FVector(110.f, 110.f, 70.f), Steel);                           // ladle bowl (touches the stove top at 1740? no: hangs; the spatula does)
		Part(EAntShape::Cube, FVector(-500.f, 4900.f, 2250.f), FVector(26.f, 26.f, 1000.f), FRotator::ZeroRotator, Hex(60, 45, 35), true);    // spatula handle, down to 1750
		Box(FVector(-600.f, 4890.f, 1750.f), FVector(-400.f, 4910.f, 1900.f), DarkSteel);                     // spatula blade on the stove top
		Part(EAntShape::Cylinder, FVector(-900.f, 4900.f, 2560.f), FVector(14.f, 14.f, 380.f), FRotator::ZeroRotator, Steel, true);           // a hanging whisk
		Ball(FVector(-900.f, 4900.f, 2320.f), FVector(70.f, 70.f, 110.f), Steel);

		// Spice rack: four little shelves up the backsplash left of the stove, jars on them (steps to the upper cabinets).
		for (int32 i = 0; i < 4; ++i)
		{
			const float Z = 1900.f + i * 300.f;
			Box(FVector(-2400.f, 4840.f, Z), FVector(-1800.f, 4970.f, Z + 20.f), Wood);
			Cyl(FVector(-2300.f + (i % 2) * 150.f, 4905.f, Z + 20.f), 45.f, 150.f, i % 3 == 0 ? Hex(200, 60, 40) : (i % 3 == 1 ? Hex(220, 170, 50) : Hex(90, 140, 60)));
			Cyl(FVector(-1950.f - (i % 2) * 100.f, 4905.f, Z + 20.f), 40.f, 130.f, Hex(230, 230, 220));
		}
	}
}
