#include "KitchenGadgets.h"
#include "Map4Look.h"
#include "AntFx.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "KitchenLoot.h"
#include "AntCharacter.h"
#include "AntTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "AntReact.h"

namespace
{
	FLinearColor Hex(uint8 R, uint8 G, uint8 B) { return FLinearColor(FColor(R, G, B)); }
	const FLinearColor Steel = Hex(175, 180, 185);
	const FLinearColor DarkSteel = Hex(70, 72, 78);
	const FLinearColor Black = Hex(20, 20, 22);
	const FLinearColor Warn(1.f, 0.7f, 0.2f);
	const FLinearColor Info(0.9f, 0.9f, 0.9f);

	UPointLightComponent* MakeLight(AActor* Owner, USceneComponent* Parent, const FVector& Loc, float Intensity, const FLinearColor& Color, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(Parent);
		L->SetRelativeLocation(Loc);
		L->IntensityUnits = ELightUnits::Unitless;
		L->bUseInverseSquaredFalloff = false;
		L->LightFalloffExponent = 2.f;
		L->Intensity = Intensity;
		L->AttenuationRadius = Radius;
		L->CastShadows = false;
		L->SetLightColor(Color);
		L->RegisterComponent();
		Owner->AddInstanceComponent(L);
		return L;
	}
}

// =============================================================================================
// The stove knob + the pot of soup on the front burner
// =============================================================================================

AKitchenKnob::AKitchenKnob()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractRadius = 200.f;
}

void AKitchenKnob::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenKnob, bOn);
	DOREPLIFETIME(AKitchenKnob, Boil);
}

bool AKitchenKnob::IsBurnerOn(UWorld* World)
{
	for (TActorIterator<AKitchenKnob> It(World); It; ++It)
	{
		if (It->bOn && AntReact::HasPower(World)) { return true; }   // an electric stove: no power, no heat (AntReact.h)
	}
	return false;
}

void AKitchenKnob::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("AntGadgetsOn")))   // dev/test, see AKitchenFaucet::BeginPlay
	{
		bOn = true;
		Boil = 1.f;
	}
	// The actor sits on the oven's control strip (the second knob from the left, in front of the hot burner).
	// Everything else is placed in world space relative to it: the burner is at (-350, 3500, 1740).
	const FVector B = FVector(-350.f, 3500.f, 1740.f) - GetActorLocation();   // burner, local
	const bool bArt = KitchenArt::Enabled();
	if (bArt)
	{
		// The modelled kitchen: our knob is the middle one of five, our coil the front-right burner; the pot itself is
		// part of the kitchen (SM_K_Pot, without its lid). The soup, the bubbles and the glow below are still ours.
		Knob = KitchenArt::AddAt(this, Root, TEXT("SM_K_Range_Knob"), 2, KitchenArt::ESolid::Moving);
		Coil = KitchenArt::AddAt(this, Root, TEXT("SM_K_Range_Burner"), 1, KitchenArt::ESolid::Moving);
		bKnobArt = Knob != nullptr;
		if (Knob)
		{
			KnobBase = Knob->GetRelativeRotation().Quaternion();
		}
		const int32 Ring = Coil ? Coil->GetMaterialIndex(TEXT("BurnerRing")) : INDEX_NONE;
		const Map4Look::FLook* CoilLook = Ring != INDEX_NONE && Map4Look::Enabled() ? Map4Look::Named(TEXT("Coil")) : nullptr;
		if (CoilLook)
		{
			CoilMID = Map4Look::Own(*CoilLook, Coil);   // map v4: cast iron that glows red as it heats up ("Color" drives it as before)
			if (CoilMID) { Coil->SetMaterial(Ring, CoilMID); }
		}
		else if (UMaterialInterface* Base = Ring != INDEX_NONE ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")) : nullptr)
		{
			CoilMID = UMaterialInstanceDynamic::Create(Base, Coil);   // the ring reddens as it heats up
			Coil->SetMaterial(Ring, CoilMID);
		}
	}
	if (!Knob)
	{
		Knob = Part(Root, EAntShape::Cylinder, FVector(0.f, -15.f, 0.f), FVector(70.f, 70.f, 30.f), Black, true, FRotator(90.f, 0.f, 0.f));
		Part(Knob, EAntShape::Cube, FVector(0.f, 0.f, 0.f), FVector(10.f, 60.f, 34.f), Hex(230, 230, 230), false);   // the pointer line
	}
	if (!Coil)
	{
		Coil = Part(Root, EAntShape::Cylinder, B + FVector(0.f, 0.f, 12.f), FVector(520.f, 520.f, 25.f), DarkSteel, true);
		Part(Root, EAntShape::Cylinder, B + FVector(0.f, 0.f, 28.f), FVector(240.f, 240.f, 6.f), Black, false);
	}
	if (!bArt)
	{
		// The pot: a steel cylinder with a rolled rim; the soup fills it almost to the top.
		Part(Root, EAntShape::Cylinder, B + FVector(0.f, 0.f, 285.f), FVector(840.f, 840.f, 520.f), Steel, true);
		Part(Root, EAntShape::Cylinder, B + FVector(0.f, 0.f, 548.f), FVector(880.f, 880.f, 26.f), Steel, true);                       // rim
		for (float s : { -1.f, 1.f })
		{
			Part(Root, EAntShape::Cube, B + FVector(470.f * s, 0.f, 420.f), FVector(120.f, 260.f, 50.f), DarkSteel, true);          // handles
		}
	}
	Soup = Part(Root, EAntShape::Cylinder, B + FVector(0.f, 0.f, 540.f), FVector(760.f, 760.f, 10.f), Hex(230, 150, 60), true);  // soup surface (walkable... and scalding)
	Map4Look::Swap(Soup, TEXT("SM_M4_LiquidDisc"), TEXT("Soup"), TEXT("M4Fx"));
	Map4Look::Swap(Part(Root, EAntShape::Cylinder, B + FVector(180.f, -120.f, 545.f), FVector(160.f, 160.f, 8.f), Hex(240, 200, 90), false), TEXT("SM_M4_CarrotSlice"), TEXT("Carrot"));    // a floating carrot
	Map4Look::Swap(Part(Root, EAntShape::Cube, B + FVector(-220.f, 150.f, 545.f), FVector(140.f, 60.f, 8.f), Hex(120, 170, 60), false), TEXT("SM_M4_BayLeaf"), TEXT("LeafLight"));         // a leaf
	Map4Look::Swap(Part(Root, EAntShape::Cylinder, B + FVector(-380.f, 300.f, 900.f), FVector(60.f, 60.f, 700.f), Hex(190, 150, 100), true, FRotator(0.f, 30.f, 60.f)), TEXT("SM_M4_WoodSpoon"));   // wooden spoon leaning in
	for (int32 i = 0; i < 7; ++i)
	{
		const float A = i * 0.9f;
		Bubbles.Add(Part(Root, EAntShape::Sphere, B + FVector(FMath::Cos(A) * 220.f, FMath::Sin(A) * 220.f, 548.f), FVector(50.f, 50.f, 40.f), Hex(245, 190, 110), false));
		Bubbles.Last()->SetVisibility(false);
		Map4Look::Swap(Bubbles.Last(), TEXT("SM_M4_Droplet"), TEXT("Soup"), TEXT("M4Fx"));
	}
	Glow = MakeLight(this, Root, B + FVector(0.f, 0.f, 120.f), 0.f, FLinearColor(1.f, 0.35f, 0.08f), 1100.f);
	UsePointsLocal = { FVector(0.f, -120.f, 0.f), FVector(0.f, -60.f, -150.f) };
}

FString AKitchenKnob::GetPrompt(const AAntCharacter* Ant) const
{
	return bOn ? TEXT("Выключить конфорку") : TEXT("Включить конфорку (суп закипит)");
}

void AKitchenKnob::Interact(AAntCharacter* Ant)
{
	bOn = !bOn;
	if (bOn)
	{
		Broadcast(TEXT("Кто-то включил плиту! Конфорка раскаляется, суп скоро закипит"), Warn);
		AntSfx::Everyone(this, AntSfx::Sizzle, GetActorLocation(), 0.8f);
	}
	else
	{
		Broadcast(TEXT("Плиту выключили"), Info);
	}
}

void AKitchenKnob::Tick(float Dt)
{
	Super::Tick(Dt);
	Anim += Dt;
	if (HasAuthority())
	{
		Boil = FMath::Clamp(Boil + ((bOn && AntReact::HasPower(this)) ? Dt / 25.f : -Dt / 12.f), 0.f, 1.f);
	}
	if (Knob && bKnobArt)
	{
		if (KnobShown != (bOn ? 1 : 0))
		{
			KnobShown = bOn ? 1 : 0;
			Knob->SetRelativeRotation(KnobBase * FQuat(FVector::RightVector, FMath::DegreesToRadians(bOn ? 120.f : 0.f)));   // about its own axis
		}
	}
	else if (Knob)
	{
		Knob->SetRelativeRotation(FRotator(90.f, 0.f, bOn ? 120.f : 0.f));
	}
	if (CoilMID)
	{
		CoilMID->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(0.65f, 0.12f, 0.05f), FLinearColor(3.f, 0.35f, 0.08f), FMath::Clamp(Boil * 2.f, 0.f, 1.f)));
	}
	else if (Coil && !bKnobArt)
	{
		AntKit::Paint(Coil, FMath::Lerp(DarkSteel, Hex(255, 60, 20), FMath::Clamp(Boil * 2.f, 0.f, 1.f)));
	}
	if (Glow)
	{
		Glow->SetIntensity(FMath::Clamp(Boil * 2.f, 0.f, 1.f) * 5.f);
	}
	// Steam: a wisp off the hot coil, a proper column off the soup as it comes to the boil.
	if (Boil > 0.15f && GetNetMode() != NM_DedicatedServer)
	{
		SteamAccum += Dt * (1.5f + Boil * 7.f);
		while (SteamAccum >= 1.f)
		{
			SteamAccum -= 1.f;
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const float R = FMath::FRandRange(0.f, 300.f);
			AntFx::Steam(this, FVector(-350.f + FMath::Cos(A) * R, 3500.f + FMath::Sin(A) * R, 2300.f), 150.f + Boil * 130.f, 150.f + Boil * 170.f);
		}
	}
	const bool bBoiling = Boil > 0.6f;
	for (int32 i = 0; i < Bubbles.Num(); ++i)
	{
		Bubbles[i]->SetVisibility(bBoiling && FMath::Sin(Anim * 7.f + i * 1.9f) > 0.2f);
		Bubbles[i]->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.4f) * (0.6f + 0.6f * FMath::Abs(FMath::Sin(Anim * 7.f + i * 1.9f))));
	}
	if (Soup)
	{
		const FVector B = FVector(-350.f, 3500.f, 1740.f) - GetActorLocation();
		Soup->SetRelativeLocation(B + FVector(0.f, 0.f, 540.f + (bBoiling ? FMath::Sin(Anim * 9.f) * 6.f : 0.f)));
	}
}

void AKitchenKnob::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = Ant->GetActorLocation();
	const FVector B(-350.f, 3500.f, 1740.f);
	if (!bOn && Boil < 0.1f)
	{
		return;
	}
	const float Heat = FMath::Clamp(Boil * 2.f, 0.f, 1.f);
	// The coil and the pot's walls.
	if (FVector::Dist2D(P, B) < 460.f && P.Z > 1735.f && P.Z < 2320.f)
	{
		Env.Temp(FMath::Lerp(40.f, 150.f, Heat), 10.f + 60.f * Heat);
	}
	// On the soup: scalding, and wet.
	if (FVector::Dist2D(P, B) < 390.f && P.Z > 2270.f && P.Z < 2360.f)
	{
		Env.Flags |= AntStatus::Wet;
		Env.Temp(FMath::Lerp(30.f, 100.f, Heat), 8.f + 40.f * Heat);
	}
	// Steam above the pot once it boils.
	if (Boil > 0.6f && FVector::Dist2D(P, B) < 600.f && P.Z > 2320.f && P.Z < 3400.f)
	{
		Env.Flags |= AntStatus::Wet;
		Env.Temp(62.f, 12.f);
	}
}

// =============================================================================================
// The pedal bin
// =============================================================================================

AKitchenPedalBin::AKitchenPedalBin()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractRadius = 260.f;
}

void AKitchenPedalBin::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenPedalBin, bOpen);
}

void AKitchenPedalBin::BeginPlay()
{
	Super::BeginPlay();
	// Actor at the bin's base centre. The lid hinges at the back (+Y) rim, the pedal sticks out at the front (-Y).
	Hinge = AntKit::AddPivot(this, Root, FVector(0.f, 660.f, 1540.f));
	const bool bArt = KitchenArt::Enabled();   // the modelled lid and pedal have the same pivots
	if (!(bArt && KitchenArt::AddAt(this, Hinge, TEXT("SM_K_Bin_Lid"), 0, KitchenArt::ESolid::Moving, false, TEXT("ArtLid"))))
	{
		Part(Hinge, EAntShape::Cylinder, FVector(0.f, -660.f, 0.f), FVector(1380.f, 1380.f, 40.f), Steel, true);        // the lid
		Part(Hinge, EAntShape::Cylinder, FVector(0.f, -660.f, 40.f), FVector(400.f, 400.f, 30.f), DarkSteel, true);      // the knob on top
		Part(Hinge, EAntShape::Cube, FVector(0.f, -40.f, -10.f), FVector(300.f, 80.f, 60.f), DarkSteel, false);         // hinge barrel
	}
	Pedal = bArt ? KitchenArt::AddAt(this, Root, TEXT("SM_K_Bin_Pedal"), 0, KitchenArt::ESolid::Moving) : nullptr;
	bPedalArt = Pedal != nullptr;
	if (Pedal)
	{
		PedalBase = Pedal->GetRelativeRotation().Quaternion();
	}
	else
	{
		Pedal = Part(Root, EAntShape::Cube, FVector(0.f, -760.f, 35.f), FVector(320.f, 240.f, 50.f), DarkSteel, true);
		Part(Root, EAntShape::Cube, FVector(0.f, -640.f, 20.f), FVector(60.f, 100.f, 40.f), Black, true);                // the lever
	}
	UsePointsLocal = { FVector(0.f, -900.f, 0.f), FVector(-200.f, -800.f, 0.f), FVector(200.f, -800.f, 0.f) };
}

FString AKitchenPedalBin::GetPrompt(const AAntCharacter* Ant) const
{
	return bOpen ? TEXT("Крышка мусорки открыта") : TEXT("Нажать педаль мусорки (крышка откроется на 15 с)");
}

void AKitchenPedalBin::Interact(AAntCharacter* Ant)
{
	if (bOpen)
	{
		return;
	}
	bOpen = true;
	CloseAt = Now() + 15.f;
	AntSfx::Everyone(this, AntSfx::FridgeDoor, GetActorLocation() + FVector(0.f, 0.f, 1500.f), 1.3f);
	if (Ant) { Ant->Toast(TEXT("Крышка поднялась! 15 секунд, чтобы залезть (или вылезти)")); }
}

void AKitchenPedalBin::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority() && bOpen && CloseAt > 0.f && Now() > CloseAt)
	{
		bOpen = false;
		CloseAt = -1.f;
		Broadcast(TEXT("Крышка мусорки захлопнулась. Кто внутри — ждите, пока кто-то нажмёт педаль"), Warn);
	}
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Alpha, Target))
	{
		Alpha = FMath::FInterpConstantTo(Alpha, Target, Dt, bOpen ? 3.f : 1.5f);
		if (Hinge)
		{
			Hinge->SetRelativeRotation(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-75.f * Alpha)));
		}
		if (Pedal)
		{
			if (bPedalArt)
			{
				Pedal->SetRelativeRotation(FQuat(FVector(-1.f, 0.f, 0.f), FMath::DegreesToRadians(-12.f * Alpha)) * PedalBase);   // the front edge goes down
			}
			else
			{
				Pedal->SetRelativeRotation(FRotator(-12.f * Alpha, 0.f, 0.f));
			}
		}
	}
}

// =============================================================================================
// The desk fan
// =============================================================================================

AKitchenFan::AKitchenFan()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractRadius = 240.f;
}

void AKitchenFan::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenFan, bOn);
}

void AKitchenFan::BeginPlay()
{
	Super::BeginPlay();
	// Base on the counter, pole, a head that sweeps left-right; it blows along the head's -Y.
	if (Map4Look::Enabled() && Map4Look::Prop(TEXT("SM_M4_FanBase")))
	{
		// map v4: a modelled desk fan (base with its red button, chrome pole, motor, wire cage, three blades), same pivots
		Map4Look::AddProp(this, Root, TEXT("SM_M4_FanBase"), FTransform::Identity, true);
		Map4Look::AddProp(this, Root, TEXT("SM_M4_FanPole"), FTransform::Identity, true);
		Head = AntKit::AddPivot(this, Root, FVector(0.f, 0.f, 480.f));
		Map4Look::AddProp(this, Head, TEXT("SM_M4_FanMotor"), FTransform::Identity, true);
		Map4Look::AddProp(this, Head, TEXT("SM_M4_FanCage"), FTransform::Identity, true);
		Blades = AntKit::AddPivot(this, Head, FVector(0.f, -40.f, 0.f));
		Map4Look::AddProp(this, Blades, TEXT("SM_M4_FanBlades"), FTransform::Identity, false);
		UsePointsLocal = { FVector(0.f, -330.f, 0.f), FVector(-250.f, -200.f, 0.f), FVector(250.f, -200.f, 0.f) };
		return;
	}
	Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 20.f), FVector(380.f, 380.f, 40.f), DarkSteel, true);
	Part(Root, EAntShape::Cube, FVector(0.f, -140.f, 60.f), FVector(120.f, 80.f, 40.f), Hex(200, 40, 40), true);    // the button
	Part(Root, EAntShape::Cylinder, FVector(0.f, 0.f, 240.f), FVector(50.f, 50.f, 400.f), Steel, true);
	Head = AntKit::AddPivot(this, Root, FVector(0.f, 0.f, 480.f));
	Part(Head, EAntShape::Cylinder, FVector(0.f, 40.f, 0.f), FVector(160.f, 160.f, 180.f), DarkSteel, true, FRotator(0.f, 0.f, 90.f));   // motor (axis along Y)
	Part(Head, EAntShape::Cylinder, FVector(0.f, -80.f, 0.f), FVector(620.f, 620.f, 30.f), Steel, true, FRotator(0.f, 0.f, 90.f));        // cage front ring
	for (int32 i = 0; i < 8; ++i)
	{
		Part(Head, EAntShape::Cube, FVector(0.f, -84.f, 0.f), FVector(600.f, 6.f, 6.f), Steel, false, FRotator(i * 22.5f, 0.f, 0.f));    // the grille (spokes in the XZ plane)
	}
	Blades = AntKit::AddPivot(this, Head, FVector(0.f, -40.f, 0.f));
	for (int32 i = 0; i < 3; ++i)
	{
		Part(Blades, EAntShape::Cube, FVector(0.f, 0.f, 0.f), FVector(520.f, 8.f, 110.f), Hex(230, 230, 230), false, FRotator(i * 60.f, 0.f, 0.f));
	}
	UsePointsLocal = { FVector(0.f, -330.f, 0.f), FVector(-250.f, -200.f, 0.f), FVector(250.f, -200.f, 0.f) };
}

FString AKitchenFan::GetPrompt(const AAntCharacter* Ant) const
{
	return bOn ? TEXT("Выключить вентилятор") : TEXT("Включить вентилятор (сдует всё со столешницы)");
}

void AKitchenFan::Interact(AAntCharacter* Ant)
{
	bOn = !bOn;
	OffAt = bOn ? Now() + 20.f : -1.f;
	if (bOn)
	{
		Broadcast(TEXT("Вентилятор включён! Держитесь за столешницу"), Warn);
	}
}

void AKitchenFan::Tick(float Dt)
{
	Super::Tick(Dt);
	if (HasAuthority() && bOn && OffAt > 0.f && Now() > OffAt)
	{
		bOn = false;
	}
	Spin = FMath::FInterpTo(Spin, bOn ? 1.f : 0.f, Dt, bOn ? 2.f : 0.7f);
	if (Blades)
	{
		Blades->AddLocalRotation(FRotator(Spin * 1400.f * Dt, 0.f, 0.f));
	}
	if (Head)
	{
		if (Spin > 0.3f) { Sweep += Dt; }
		Head->SetRelativeRotation(FRotator(0.f, FMath::Sin(Sweep * 0.9f) * 35.f, 0.f));
	}
	if (Spin < 0.25f || !Head)
	{
		return;
	}
	// The wind: a box in front of the head, in the head's space. Whoever is in it gets blown along.
	const FTransform H = Head->GetComponentTransform();
	const FVector Dir = -H.GetUnitAxis(EAxis::Y);
	const FBox Zone(FVector(-700.f, -3400.f, -560.f), FVector(700.f, -150.f, 420.f));
	const float Strength = Spin;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || !(A->IsLocallyControlled() || (HasAuthority() && !A->IsPlayerControlled())))
		{
			continue;
		}
		const FVector L = H.InverseTransformPosition(A->GetActorLocation());
		if (Zone.IsInside(L))
		{
			const float Fade = FMath::Clamp(1.f - (-L.Y - 150.f) / 3300.f, 0.2f, 1.f);
			A->AddMovementInput(Dir, Strength * Fade * (A->Creature == 1 ? 2.f : 1.2f));
			if (A->IsCrawling() && Fade > 0.75f && FMath::FRand() < Dt * 0.6f * Strength)
			{
				A->ApplyLaunch(Dir * 260.f * AntKit::SpeedK + FVector(0.f, 0.f, 90.f * AntKit::SpeedK));   // torn off the surface
			}
		}
	}
	if (HasAuthority())
	{
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			AKitchenLoot* Loot = *It;
			if (!Loot || Loot->bHeld)
			{
				continue;
			}
			const FVector L = H.InverseTransformPosition(Loot->GetActorLocation());
			if (Zone.IsInside(L) && Loot->Weight <= 12)
			{
				Loot->Kick(Dir * (320.f - Loot->Weight * 18.f) * Strength * Dt, Loot->GetActorLocation());   // velocity change per tick
			}
		}
	}
}
