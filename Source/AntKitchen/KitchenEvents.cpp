#include "KitchenEvents.h"
#include "AntRun.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "HumanCharacter.h"
#include "AntFx.h"
#include "AntTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"

// =============================================================================================
// The microwave's start button and the TV: noise on demand
// =============================================================================================

AKitchenNoiseBox::AKitchenNoiseBox()
{
	InteractRadius = 300.f;
}

void AKitchenNoiseBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenNoiseBox, Kind);
	DOREPLIFETIME(AKitchenNoiseBox, bOn);
}

void AKitchenNoiseBox::BeginPlay()
{
	Super::BeginPlay();
	UsePointsLocal.Reset();
	UsePointsLocal.Add(FVector::ZeroVector);
	if (Kind == 0)
	{
		// A green start button on the microwave's front panel; reachable from the worktop under it and from the door.
		Button = AntKit::AddPart(this, Root, EAntShape::Cylinder, FVector(0.f, -6.f, 0.f), FVector(0.5f, 0.5f, 0.12f), FLinearColor(0.2f, 0.85f, 0.3f), FRotator(0.f, 0.f, 90.f));
		UsePointsLocal.Add(FVector(0.f, -120.f, -230.f));
		UsePointsLocal.Add(FVector(-200.f, -60.f, -120.f));
	}
	else
	{
		UsePointsLocal.Add(FVector(250.f, 0.f, 0.f));
		UsePointsLocal.Add(FVector(-250.f, 0.f, 0.f));
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(Root);
		// The microwave glows from inside; the TV throws its flicker into the room.
		Light->SetRelativeLocation(Kind == 0 ? FVector(-520.f, 420.f, 80.f) : FVector(1200.f, 500.f, 900.f));
		Light->IntensityUnits = ELightUnits::Unitless;
		Light->bUseInverseSquaredFalloff = false;
		Light->LightFalloffExponent = 2.f;
		Light->Intensity = 0.f;
		Light->AttenuationRadius = Kind == 0 ? 900.f : 5000.f;
		Light->CastShadows = false;
		Light->SetLightColor(Kind == 0 ? FLinearColor(1.f, 0.75f, 0.35f) : FLinearColor(0.5f, 0.7f, 1.f));
		Light->RegisterComponent();
		AddInstanceComponent(Light);
	}
}

FString AKitchenNoiseBox::GetPrompt(const AAntCharacter* Ant) const
{
	if (Kind == 0)
	{
		return bOn ? TEXT("Микроволновка гудит...") : TEXT("Запустить микроволновку (дзынькнет — Человек придёт)");
	}
	return bOn ? TEXT("Выключить телевизор") : TEXT("Включить телевизор (Человек пойдёт выключать)");
}

void AKitchenNoiseBox::Interact(AAntCharacter* Ant)
{
	if (Kind == 0)
	{
		if (!bOn)
		{
			bOn = true;
			OffAt = Now() + 5.f;
			AntSfx::Everyone(this, AntSfx::Toaster, GetActorLocation(), 0.6f);
			if (Ant) { Ant->Toast(TEXT("Микроволновка запущена: через 5 секунд дзынькнет")); }
			UE_LOG(LogTemp, Display, TEXT("ANTNOISEBOX microwave started by %s"), Ant ? *Ant->GetDisplayName() : TEXT("-"));
		}
		return;
	}
	bOn = !bOn;
	NextNoise = 0.f;
	AntSfx::Everyone(this, AntSfx::PickupGear, GetActorLocation(), bOn ? 1.2f : 0.8f);
	if (Ant) { Ant->Toast(bOn ? TEXT("Телевизор орёт на всю квартиру") : TEXT("Телевизор выключен")); }
	UE_LOG(LogTemp, Display, TEXT("ANTNOISEBOX tv %s by %s"), bOn ? TEXT("on") : TEXT("off"), Ant ? *Ant->GetDisplayName() : TEXT("-"));
}

void AKitchenNoiseBox::SwitchOff()
{
	if (Kind == 1 && bOn)
	{
		bOn = false;
		AntSfx::Everyone(this, AntSfx::PickupGear, GetActorLocation(), 0.8f);
	}
}

void AKitchenNoiseBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Anim += DeltaSeconds;
	if (Light)
	{
		const float Want = !bOn ? 0.f : Kind == 0 ? 5.f : 3.f + 2.5f * FMath::Abs(FMath::Sin(Anim * 7.f) * FMath::Sin(Anim * 2.3f));
		Light->SetIntensity(FMath::FInterpTo(Light->Intensity, Want, DeltaSeconds, 10.f));
	}
	if (!HasAuthority() || !bOn)
	{
		return;
	}
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	if (Kind == 0)
	{
		if (Now() >= OffAt)
		{
			bOn = false;
			AntSfx::Everyone(this, AntSfx::Toaster, GetActorLocation(), 1.9f);   // ding
			if (GM) { GM->MakeNoise(GetActorLocation(), 1.f, AntNoise::Microwave, this); }
		}
	}
	else if (Now() >= NextNoise)
	{
		NextNoise = Now() + 2.5f;
		if (GM) { GM->MakeNoise(GetActorLocation(), 0.8f, AntNoise::TV, this); }
	}
}

void AKitchenNoiseBox::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	// Shut inside a running microwave.
	if (Kind == 0 && bOn && KitchenContainers::At(Ant->GetActorLocation()) == KitchenContainers::Microwave
		&& !KitchenContainers::IsOpen(GetWorld(), KitchenContainers::Microwave))
	{
		Env.Temp(230.f, 60.f);
		Env.Hurt(25.f, TEXT("Ты в работающей микроволновке!"));
	}
}

// =============================================================================================
// The robot vacuum
// =============================================================================================

ARobotVacuum::ARobotVacuum()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(30.f);

	// The body is made here, not in BeginPlay: ants ride it, and a base must have the same name on every machine.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetMobility(EComponentMobility::Movable);
}

void ARobotVacuum::BeginPlay()
{
	Super::BeginPlay();
	Body->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Cylinder));
	Body->SetWorldScale3D(FVector(Radius * 2.f / 100.f, Radius * 2.f / 100.f, Height / 100.f));
	AntKit::Paint(Body, FLinearColor(0.08f, 0.08f, 0.09f));
	UAntMovementComponent::MakeClimbable(Body);
	// A raised sensor turret and a lamp, so its heading reads at a glance (unscaled sizes: the body is scaled).
	AntKit::AddPart(this, Body, EAntShape::Cylinder, FVector(0.f, 0.f, 62.f), FVector(0.28f, 0.28f, 0.28f), FLinearColor(0.2f, 0.2f, 0.22f));
	Lamp = AntKit::AddPart(this, Body, EAntShape::Cube, FVector(44.f, 0.f, 20.f), FVector(0.08f, 0.3f, 0.3f), FLinearColor(0.2f, 1.f, 0.4f));
	if (Lamp && GetNetMode() != NM_DedicatedServer)
	{
		AntFx::MakeGlow(Lamp, FLinearColor(0.3f, 3.f, 0.8f));
	}
	Heading = GetActorForwardVector();
}

void ARobotVacuum::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	UWorld* W = GetWorld();
	Life -= DeltaSeconds;
	if (Life <= 0.f)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTVACUUM done: swallowed %d pieces"), Eaten);
		Destroy();
		return;
	}
	const FVector Me = GetActorLocation();
	const FBox Area = AKitchenBuilder::PlayArea();
	if (TurnLeft > 0.f)
	{
		// Bumped: turns on the spot, then tries the new way.
		TurnLeft -= DeltaSeconds;
		Heading = Heading.RotateAngleAxis(TurnSign * 150.f * DeltaSeconds, FVector::UpVector);
		SetActorRotation(Heading.Rotation());
	}
	else
	{
		const FVector Next = Me + Heading * 300.f * DeltaSeconds;
		bool bBlocked = Next.X < Area.Min.X + Radius || Next.X > Area.Max.X - Radius || Next.Y < Area.Min.Y + Radius || Next.Y > Area.Max.Y - Radius;
		// It turns away from the Human's feet (it would shove him across the kitchen otherwise).
		for (TActorIterator<AHumanCharacter> It(W); It && !bBlocked; ++It)
		{
			const FVector ToH = It->GetActorLocation() - Me;
			bBlocked = ToH.Size2D() < Radius + 460.f && FVector::DotProduct(ToH.GetSafeNormal2D(), Heading) > 0.f;
		}
		if (!bBlocked)
		{
			// Furniture in the way? (the static kitchen only: it shoves everything else aside)
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AntVacuum), false, this);
			const FVector Probe = Me + FVector(0.f, 0.f, Height * 0.5f);
			bBlocked = W->SweepSingleByObjectType(Hit, Probe, Probe + Heading * 60.f, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
				FCollisionShape::MakeSphere(Radius * 0.85f), Params) && Hit.ImpactNormal.Z < 0.5f;
		}
		if (bBlocked)
		{
			TurnLeft = FMath::FRandRange(0.5f, 1.4f);
			TurnSign = FMath::RandBool() ? 1.f : -1.f;
		}
		else
		{
			SetActorLocation(Next);
		}
	}

	EatClock += DeltaSeconds;
	if (EatClock < 0.1f)
	{
		return;
	}
	EatClock = 0.f;
	// Ants on the floor in its way go under it. Those standing on top of it ride.
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->bDead || A->Creature != 0)
		{
			continue;
		}
		const FVector P = A->GetActorLocation();
		if (P.Z > Me.Z + Height * 0.5f - 14.f)
		{
			continue;   // on its top: a passenger
		}
		// On the floor at its leading edge: under it goes (an ant clinging to its side or its back is carried along).
		const FVector To = P - Me;
		if (To.Size2D() < Radius + 14.f && FVector::DotProduct(To.GetSafeNormal2D(), Heading) > 0.15f && TurnLeft <= 0.f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTVACUUM ran over %s"), *A->GetDisplayName());
			A->HurtBy(300.f, TEXT("засосан роботом-пылесосом"));
		}
	}
	// Small loose food disappears into it.
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (L && !L->bIsGear && !L->bTrophy && !L->bVault && !L->bBody && !L->bHeld && !L->IsCarried() && L->Weight <= 4
			&& FVector::Dist2D(L->GetActorLocation(), Me) < Radius + 20.f && L->GetActorLocation().Z < Me.Z + 80.f)
		{
			++Eaten;
			AntSfx::Everyone(this, AntSfx::Crunch, L->GetActorLocation(), 1.7f);
			L->Destroy();
		}
	}
}

// =============================================================================================
// The sink overflowed
// =============================================================================================

AKitchenFlood::AKitchenFlood()
{
	InteractRadius = 0.f;
}

void AKitchenFlood::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitchenFlood, PoolRadius);
}

void AKitchenFlood::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		Water = AntKit::AddPart(this, Root, EAntShape::Cylinder, FVector(0.f, 0.f, Depth * 0.5f), FVector(1.f, 1.f, Depth / 100.f), FLinearColor(0.35f, 0.6f, 0.95f));
		if (Water)
		{
			Water->SetCastShadow(false);
			// The same soft unlit translucent material the splashes use: always cooked.
			if (UMaterialInterface* Soft = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent")))
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Soft, this);
				M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.5f, 0.9f, 0.42f));
				Water->SetMaterial(0, M);
			}
		}
	}
}

void AKitchenFlood::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (HasAuthority())
	{
		// Spreads for a few seconds, stands, then drains away in the last five.
		const float Grow = FMath::Clamp(Age / 7.f, 0.f, 1.f);
		const float Drain = FMath::Clamp((Life - Age) / 5.f, 0.f, 1.f);
		PoolRadius = FMath::Max(60.f, MaxRadius * FMath::Sqrt(Grow) * Drain);
		if (Age >= Life)
		{
			Destroy();
			return;
		}
		// Food floats: it bobs up and drifts away from the sink.
		const FVector C = GetActorLocation();
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			AKitchenLoot* L = *It;
			const FVector P = L->GetActorLocation();
			if (!L->bIsGear && !L->bHeld && !L->IsCarried() && P.Z < C.Z + 60.f && FVector::Dist2D(P, C) < PoolRadius)
			{
				const FVector Out = (P - C).GetSafeNormal2D();
				L->Kick(Out * 140.f * DeltaSeconds + FVector(0.f, 0.f, 900.f * DeltaSeconds), P);   // nearly weightless: it bobs and slides off
			}
		}
	}
	if (Water)
	{
		const float D = PoolRadius * 2.f / 100.f;
		Water->SetRelativeScale3D(FVector(D, D, Depth / 100.f));
		FxClock += DeltaSeconds;
		if (FxClock > 0.5f)
		{
			FxClock = 0.f;
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			AntFx::Ripple(this, GetActorLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(0.f, PoolRadius * 0.9f) + FVector(0.f, 0.f, Depth), 60.f);
		}
	}
}

void AKitchenFlood::AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const
{
	const FVector P = Ant->GetActorLocation();
	const FVector C = GetActorLocation();
	if (P.Z < C.Z + Depth + 14.f && P.Z > C.Z - 30.f && FVector::Dist2D(P, C) < PoolRadius)
	{
		Env.Flags |= AntStatus::Wet;   // wading: wet and slow (the pool is too shallow to drown in)
		Env.Temp(10.f, 5.f);
	}
}

// =============================================================================================
// Pizza delivery
// =============================================================================================

APizzaBox::APizzaBox()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

void APizzaBox::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Card(0.72f, 0.56f, 0.36f), Print(0.8f, 0.15f, 0.1f);
	// The open box: the tray on the floor, a low rim, the lid standing up at the back.
	AntKit::AddPart(this, Root, EAntShape::Cube, FVector(0.f, 0.f, Thick * 0.5f), FVector(Half * 2.f, Half * 2.f, Thick) / 100.f, Card, FRotator::ZeroRotator, true);
	AntKit::AddPart(this, Root, EAntShape::Cube, FVector(0.f, Half + 15.f, Half * 0.9f), FVector(Half * 2.f, 30.f, Half * 1.8f) / 100.f, Card, FRotator::ZeroRotator, true);
	AntKit::AddPart(this, Root, EAntShape::Cube, FVector(0.f, Half - 2.f, Half * 0.9f), FVector(Half * 1.2f, 6.f, Half * 0.7f) / 100.f, Print);
	for (int32 s = -1; s <= 1; s += 2)
	{
		AntKit::AddPart(this, Root, EAntShape::Cube, FVector(s * (Half - 10.f), 0.f, Thick + 35.f), FVector(20.f, Half * 2.f, 70.f) / 100.f, Card, FRotator::ZeroRotator, true);
	}
	// The pizza itself: a thin disc the slices lie on.
	AntKit::AddPart(this, Root, EAntShape::Cylinder, FVector(0.f, 0.f, Thick + 6.f), FVector(Half * 1.7f, Half * 1.7f, 12.f) / 100.f, FLinearColor(0.86f, 0.62f, 0.3f), FRotator::ZeroRotator, true);
}