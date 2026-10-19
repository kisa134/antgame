#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "AcidProjectile.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "AntKitchenGameMode.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PointLightComponent.h"
#include "HumanCharacter.h"
#include "AntGameState.h"
#include "AntArmy.h"
#include "AntBotController.h"
#include "AntChain.h"

AAntCharacter::AAntCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAntMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	SetNetCullDistanceSquared(1.0e9f);

	// A sphere: the collision never needs to rotate when the ant is on a wall or ceiling.
	GetCapsuleComponent()->InitCapsuleSize(BodyRadius, BodyRadius);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->MaxWalkSpeed = 480.f;
	CM->MaxAcceleration = 4000.f;
	CM->JumpZVelocity = 560.f;
	CM->AirControl = 0.7f;
	CM->GravityScale = 1.4f;
	CM->MaxStepHeight = 12.f;
	CM->SetWalkableFloorAngle(55.f);
	CM->bOrientRotationToMovement = false;
	CM->bEnablePhysicsInteraction = true;
	CM->PushForceFactor = 30000.f;      // shove crumbs around, not launch them
	CM->InitialPushForceFactor = 60.f;
	// Casual co-op: trust the client's position so crawling never rubber-bands.
	CM->bIgnoreClientMovementErrorChecksAndCorrection = true;
	CM->bServerAcceptClientAuthoritativePosition = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 125.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 45.f);
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->ProbeSize = 6.f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 18.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 95.f;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	Weapon = AntKit::StarterWeapon();
}

void AAntCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntCharacter, Health);
	DOREPLIFETIME(AAntCharacter, Carrying);
	DOREPLIFETIME(AAntCharacter, Team);
	DOREPLIFETIME(AAntCharacter, AntClass);
	DOREPLIFETIME(AAntCharacter, bDead);
	DOREPLIFETIME(AAntCharacter, Weapon);
	DOREPLIFETIME(AAntCharacter, BodyColor);
	DOREPLIFETIME(AAntCharacter, bSprinting);
	DOREPLIFETIME(AAntCharacter, Temperature);
	DOREPLIFETIME(AAntCharacter, StatusFlags);
	DOREPLIFETIME(AAntCharacter, TrailPoints);
	DOREPLIFETIME(AAntCharacter, bLayingTrail);
	DOREPLIFETIME(AAntCharacter, Chain);
	DOREPLIFETIME(AAntCharacter, HeldBy);
	DOREPLIFETIME(AAntCharacter, HoldingAnt);
}

void AAntCharacter::BeginPlay()
{
	Super::BeginPlay();
	BuildBody();
	OnRep_Carry();
	OnRep_Weapon();
	ApplyClassStats();
	ApplyCameraLook();
}

void AAntCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ApplyCameraLook();
}

void AAntCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AAntPlayerState* PS = GetPlayerState<AAntPlayerState>())
	{
		BodyColor = PS->AntColor;
		OnRep_BodyColor();
		if (Creature == 0)
		{
			Team = PS->Team;
			SetClass(PS->AntClass, true);
		}
		else
		{
			if (Creature != 2) { Team = PS->Team; }   // a player-fly works for its colony; spiders hunt everyone
			ApplyClassStats();
		}
	}
}

int32 AAntCharacter::CarriedWeight() const
{
	return Carrying ? FMath::Max(1, Carrying->Weight) : 0;
}

void AAntCharacter::OnRep_Class()
{
	ApplyClassStats();
}

void AAntCharacter::ApplyClassStats()
{
	const FAntClassDef& D = GetClassDef();
	const float Frac = MaxHealth > 0.f ? Health / MaxHealth : 1.f;
	MaxHealth = HealthOverride > 0.f ? HealthOverride : D.Health;
	if (HasAuthority())
	{
		Health = FMath::Clamp(Frac, 0.05f, 1.f) * MaxHealth;
	}
	GetCharacterMovement()->JumpZVelocity = D.JumpZ * 0.9f;
	if (VisualRoot)
	{
		VisualRoot->SetRelativeScale3D(FVector(D.BodyScale * AntScale * ExtraScale));
	}
}

void AAntCharacter::SetClass(EAntClass NewClass, bool bForce)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!bForce && !IsInNest())
	{
		Toast(TEXT("Сменить касту можно только в своём муравейнике"));
		return;
	}
	if (!bForce)
	{
		Health = MaxHealth;   // fresh body
	}
	AntClass = NewClass;
	ApplyClassStats();
	if (AAntPlayerState* PS = GetPlayerState<AAntPlayerState>())
	{
		PS->AntClass = NewClass;
	}
	if (!bForce)
	{
		const FAntClassDef& D = GetClassDef();
		Toast(FString::Printf(TEXT("Ты теперь %s: сила %.0f, здоровье %.0f"), D.Name, D.Strength, D.Health));
	}
}

void AAntCharacter::PickClass1() { ServerSetClass(0); }
void AAntCharacter::PickClass2() { ServerSetClass(1); }
void AAntCharacter::PickClass3() { ServerSetClass(2); }
void AAntCharacter::PickClass4() { ServerSetClass(3); }

void AAntCharacter::ServerSetClass_Implementation(uint8 NewClass)
{
	if (!bDead)
	{
		SetClass((EAntClass)FMath::Min<uint8>(NewClass, 3), false);
	}
}

void AAntCharacter::TickNurse(float Dt)
{
	const FAntClassDef& D = GetClassDef();
	if (D.HealRadius <= 0.f)
	{
		return;
	}
	NurseTimer += Dt;
	if (NurseTimer < 0.5f)
	{
		return;
	}
	const float Step = NurseTimer;
	NurseTimer = 0.f;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (A && !A->bDead && A->Team == Team && A->Health < A->MaxHealth
			&& FVector::Dist(A->GetActorLocation(), GetActorLocation()) < D.HealRadius)
		{
			A->Health = FMath::Min(A->MaxHealth, A->Health + D.HealPerSec * Step * (A == this ? 0.5f : 1.f));
		}
	}
}

AHumanCharacter* AAntCharacter::GetRiddenHuman() const
{
	const UAntMovementComponent* M = GetAntMovement();
	const UPrimitiveComponent* C = M ? M->GetRiddenComponent() : nullptr;
	return C ? Cast<AHumanCharacter>(C->GetOwner()) : nullptr;
}

UAntMovementComponent* AAntCharacter::GetAntMovement() const
{
	return Cast<UAntMovementComponent>(GetCharacterMovement());
}

bool AAntCharacter::IsCrawling() const
{
	const UAntMovementComponent* M = GetAntMovement();
	return M && M->IsCrawling();
}

bool AAntCharacter::IsClimbing() const
{
	return IsCrawling() && VisNormal.Z < 0.6f;
}

// ---------------------------------------------------------------------------------------------
// Body made of spheres and cylinders; legs are procedural.
// ---------------------------------------------------------------------------------------------

void AAntCharacter::BuildBody()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FLinearColor Body = BodyColor;
	const FLinearColor Leg = Body * 0.6f;
	const FLinearColor Eye(0.01f, 0.01f, 0.01f);

	auto AddBody = [this, &Body](USceneComponent* Parent, EAntShape S, const FVector& L, const FVector& Sc, const FRotator& R)
	{
		UStaticMeshComponent* C = AntKit::AddPart(this, Parent, S, L, Sc, Body, R);
		C->SetCastShadow(true);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)))
		{
			BodyMIDs.Add(M);
		}
		return C;
	};
	auto AddLeg = [this, &Leg](EAntShape S, const FVector& Sc)
	{
		UStaticMeshComponent* C = AntKit::AddPart(this, VisualRoot, S, FVector::ZeroVector, Sc, Leg);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)))
		{
			LegMIDs.Add(M);
		}
		return C;
	};

	// Generated art (Meshy) when imported, engine shapes otherwise.
	const AntArt::FPart& ArtBody = AntArt::Get(TEXT("SM_Ant_Body"));
	const AntArt::FPart& ArtScape = AntArt::Get(TEXT("SM_Ant_Antenna_Scape"));
	const AntArt::FPart& ArtFlag = AntArt::Get(TEXT("SM_Ant_Antenna_Flagellum"));
	const AntArt::FPart& ArtFemur = AntArt::Get(TEXT("SM_Ant_Femur"));
	const AntArt::FPart& ArtTibia = AntArt::Get(TEXT("SM_Ant_Tibia"));
	const AntArt::FPart& ArtCoxa = AntArt::Get(TEXT("SM_Ant_Coxa"));
	const AntArt::FPart& ArtGland = AntArt::Get(TEXT("SM_Ant_AcidGland"));
	auto AddArtPart = [this](USceneComponent* Parent, const AntArt::FPart& P)
	{
		UStaticMeshComponent* C = AntArt::AddArt(this, Parent, P);
		if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0))
		{
			ArtMIDs.Add(M);
		}
		return C;
	};
	bArtLegs = ArtFemur.IsValid() && ArtTibia.IsValid();

	if (ArtBody.IsValid())
	{
		UStaticMeshComponent* B = AddArtPart(VisualRoot, ArtBody);
		B->SetRelativeTransform(AntArt::Fit(ArtBody, FVector(-6.f, 0.f, 6.f), 98.f, ArtFlipBody));
		B->SetCastShadow(true);
	}
	else
	{
		AddBody(VisualRoot, EAntShape::Sphere, FVector(-30.f, 0.f, 2.f), FVector(0.48f, 0.34f, 0.30f), FRotator(8.f, 0.f, 0.f)); // abdomen
		AddBody(VisualRoot, EAntShape::Sphere, FVector(-8.f, 0.f, 0.f), FVector(0.12f), FRotator::ZeroRotator);                  // petiole
		AddBody(VisualRoot, EAntShape::Sphere, FVector(4.f, 0.f, 2.f), FVector(0.32f, 0.20f, 0.20f), FRotator::ZeroRotator);    // thorax
		AddBody(VisualRoot, EAntShape::Sphere, FVector(26.f, 0.f, 6.f), FVector(0.30f, 0.26f, 0.24f), FRotator::ZeroRotator);   // head
	}

	for (int32 s = -1; s <= 1; s += 2)
	{
		if (!ArtBody.IsValid())
		{
			AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(34.f, 9.f * s, 10.f), FVector(0.08f), Eye);
			AddBody(VisualRoot, EAntShape::Cube, FVector(42.f, 5.f * s, 1.f), FVector(0.13f, 0.03f, 0.04f), FRotator(0.f, -25.f * s, 0.f)); // mandible
		}

		// Elbowed antenna: scape + flagellum on a swaying pivot.
		USceneComponent* P = AntKit::AddPivot(this, VisualRoot, FVector(34.f, 6.f * s, 14.f), FRotator(40.f, 25.f * s, 0.f));
		USceneComponent* Elbow = AntKit::AddPivot(this, P, FVector(16.f, 0.f, 0.f), FRotator(-70.f, 0.f, 0.f));
		if (ArtScape.IsValid() && ArtFlag.IsValid())
		{
			AntArt::PlaceAlong(AddArtPart(P, ArtScape), ArtScape, FVector::ZeroVector, FVector(17.f, 0.f, 0.f), FVector::UpVector, 3.4f, false, true);
			AntArt::PlaceAlong(AddArtPart(Elbow, ArtFlag), ArtFlag, FVector::ZeroVector, FVector(24.f, 0.f, 0.f), FVector::UpVector, 3.2f, ArtFlipFlagellum, true);
		}
		else
		{
			AddBody(P, EAntShape::Cylinder, FVector(8.f, 0.f, 0.f), FVector(0.022f, 0.022f, 0.16f), FRotator(90.f, 0.f, 0.f));
			AddBody(Elbow, EAntShape::Cylinder, FVector(11.f, 0.f, 0.f), FVector(0.026f, 0.026f, 0.22f), FRotator(90.f, 0.f, 0.f));
		}
		AntennaPivots.Add(P);
	}

	// Six legs: femur + tibia + knee joint, placed every frame by UpdateLegs.
	const float HipX[3] = { 10.f, 2.f, -6.f };
	const float RestX[3] = { 36.f, 4.f, -30.f };
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			FAntLeg L;
			L.HipLocal = FVector(HipX[i], 6.f * s, -2.f);
			L.RestLocal = FVector(RestX[i], 40.f * s, -24.f);
			L.Side = s;
			L.Group = (i + (s > 0 ? 1 : 0)) % 2;
			Legs.Add(L);
			if (bArtLegs)
			{
				LegFemurs.Add(AddArtPart(VisualRoot, ArtFemur));
				LegTibias.Add(AddArtPart(VisualRoot, ArtTibia));
				LegKnees.Add(nullptr);
				LegCoxas.Add(ArtCoxa.IsValid() ? AddArtPart(VisualRoot, ArtCoxa) : nullptr);
			}
			else
			{
				LegFemurs.Add(AddLeg(EAntShape::Cylinder, FVector(0.07f)));
				LegTibias.Add(AddLeg(EAntShape::Cylinder, FVector(0.05f)));
				LegKnees.Add(AddLeg(EAntShape::Sphere, FVector(0.075f)));
				LegCoxas.Add(nullptr);
			}
		}
	}

	// Acid gland at the tip of the abdomen, glows in the weapon's rarity color.
	if (ArtGland.IsValid())
	{
		UStaticMeshComponent* G = AntArt::AddArt(this, VisualRoot, ArtGland);
		G->SetRelativeTransform(AntArt::Fit(ArtGland, FVector(-57.f, 0.f, 6.f), 18.f, false));
		GlandArtMID = G->CreateDynamicMaterialInstance(0);
	}
	else
	{
		UStaticMeshComponent* Gland = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-55.f, 0.f, 4.f), FVector(0.13f), AntKit::RarityColor(Weapon.Rarity));
		GlandMID = Cast<UMaterialInstanceDynamic>(Gland->GetMaterial(0));
	}

	BuildStatusFx();
}

void AAntCharacter::BuildStatusFx()
{
	// Food chunk carried on the back.
	CarryChunk = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-20.f, 0.f, 30.f), FVector(0.2f), FLinearColor(1.f, 0.75f, 0.2f));
	CarryChunk->SetVisibility(false);   // the real food is carried now; kept for old saves

	// Ice crust shown when frozen.
	FrostShell = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-6.f, 0.f, 4.f), FVector(0.95f, 0.55f, 0.5f), FLinearColor(0.75f, 0.9f, 1.f));
	FrostShell->SetVisibility(false);
	FlameShell = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-6.f, 0.f, 22.f), FVector(0.7f, 0.55f, 0.75f), FLinearColor(4.f, 1.2f, 0.1f));
	FlameShell->SetCastShadow(false);
	FlameShell->SetVisibility(false);
	WebShell = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-8.f, 0.f, 6.f), FVector(1.05f, 0.7f, 0.6f), FLinearColor(0.92f, 0.92f, 0.95f));
	WebShell->SetCastShadow(false);
	WebShell->SetVisibility(false);
	FlameLight = NewObject<UPointLightComponent>(this);
	FlameLight->SetupAttachment(VisualRoot);
	FlameLight->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	FlameLight->IntensityUnits = ELightUnits::Unitless;
	FlameLight->bUseInverseSquaredFalloff = false;
	FlameLight->LightFalloffExponent = 2.f;
	FlameLight->AttenuationRadius = 260.f;
	FlameLight->SetLightColor(FLinearColor(1.f, 0.45f, 0.1f));
	FlameLight->CastShadows = false;
	FlameLight->SetVisibility(false);
	FlameLight->RegisterComponent();
}

void AAntCharacter::OnRep_BodyColor()
{
	for (UMaterialInstanceDynamic* M : BodyMIDs)
	{
		if (M) { M->SetVectorParameterValue(TEXT("Color"), BodyColor); }
	}
	for (UMaterialInstanceDynamic* M : LegMIDs)
	{
		if (M) { M->SetVectorParameterValue(TEXT("Color"), BodyColor * 0.6f); }
	}
	// Generated art: tint the chitin towards the player's color (the default color keeps the texture as is).
	const float MaxC = FMath::Max3(BodyColor.R, BodyColor.G, BodyColor.B);
	FLinearColor Tint = FLinearColor::White;
	if (MaxC > 0.f && !BodyColor.Equals(FLinearColor(0.12f, 0.04f, 0.03f), 0.01f))
	{
		Tint = FMath::Lerp(FLinearColor::White, BodyColor / MaxC, 0.7f) * 1.3f;
	}
	for (UMaterialInstanceDynamic* M : ArtMIDs)
	{
		if (M) { M->SetVectorParameterValue(TEXT("Tint"), Tint); }
	}
}

void AAntCharacter::OnRep_Weapon()
{
	if (GlandMID)
	{
		GlandMID->SetVectorParameterValue(TEXT("Color"), AntKit::RarityColor(Weapon.Rarity));
	}
	if (GlandArtMID)
	{
		GlandArtMID->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, AntKit::RarityColor(Weapon.Rarity), Weapon.Rarity > 0 ? 0.8f : 0.f));
		GlandArtMID->SetScalarParameterValue(TEXT("Glow"), 1.5f + Weapon.Rarity * 1.5f);
	}
}

void AAntCharacter::OnRep_Carry()
{
	if (CarryChunk)
	{
		CarryChunk->SetVisibility(false);
	}
	UpdateSpeed();
}

void AAntCharacter::OnRep_Dead()
{
	if (bDead)
	{
		bWantsFire = false;
		VisualRoot->SetWorldLocationAndRotation(GetActorLocation() - VisNormal * 20.f, FRotationMatrix::MakeFromZX(VisNormal, VisHeading).ToQuat());
		VisualRoot->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.15f) * AntScale);   // squashed flat
		for (USceneComponent* P : AntennaPivots) { if (P) P->SetVisibility(false, true); }
		for (UStaticMeshComponent* C : LegFemurs) { if (C) C->SetVisibility(false); }
		for (UStaticMeshComponent* C : LegTibias) { if (C) C->SetVisibility(false); }
		for (UStaticMeshComponent* C : LegKnees) { if (C) C->SetVisibility(false); }
		for (UStaticMeshComponent* C : LegCoxas) { if (C) C->SetVisibility(false); }
		if (FrostShell) { FrostShell->SetVisibility(false); }
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void AAntCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);

	Input->BindAxis("MoveForward", this, &AAntCharacter::MoveForward);
	Input->BindAxis("MoveRight", this, &AAntCharacter::MoveRight);
	Input->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	Input->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	Input->BindAxis("TurnRate", this, &AAntCharacter::TurnRate);
	Input->BindAxis("LookUpRate", this, &AAntCharacter::LookUpRate);

	Input->BindAction("Jump", IE_Pressed, this, &AAntCharacter::DoJump);
	Input->BindAction("Jump", IE_Released, this, &AAntCharacter::DoStopJump);
	Input->BindAction("Fire", IE_Pressed, this, &AAntCharacter::StartFire);
	Input->BindAction("Fire", IE_Released, this, &AAntCharacter::StopFire);
	Input->BindAction("Interact", IE_Pressed, this, &AAntCharacter::Interact);
	Input->BindAction("Sprint", IE_Pressed, this, &AAntCharacter::StartSprint);
	Input->BindAction("Sprint", IE_Released, this, &AAntCharacter::StopSprint);
	Input->BindAction("LayTrail", IE_Pressed, this, &AAntCharacter::ToggleTrail);
	Input->BindAction("RecallSwarm", IE_Pressed, this, &AAntCharacter::RecallSwarm);
	// Caste selection, inside the nest.
	Input->BindAction("AltFire", IE_Pressed, this, &AAntCharacter::AltFirePressed);
	Input->BindAction("AltFire", IE_Released, this, &AAntCharacter::AltFireReleased);
	Input->BindKey(EKeys::G, IE_Pressed, this, &AAntCharacter::LinkPressed);
	Input->BindKey(EKeys::V, IE_Pressed, this, &AAntCharacter::ToggleBodycam);
	Input->BindKey(EKeys::F, IE_Pressed, this, &AAntCharacter::CmdArmy);
	Input->BindKey(EKeys::R, IE_Pressed, this, &AAntCharacter::CmdArmyTarget);
	Input->BindKey(EKeys::Z, IE_Pressed, this, &AAntCharacter::CmdFollow);
	Input->BindKey(EKeys::X, IE_Pressed, this, &AAntCharacter::CmdAttack);
	Input->BindKey(EKeys::C, IE_Pressed, this, &AAntCharacter::CmdFree);
	Input->BindKey(EKeys::One, IE_Pressed, this, &AAntCharacter::PickClass1);
	Input->BindKey(EKeys::Two, IE_Pressed, this, &AAntCharacter::PickClass2);
	Input->BindKey(EKeys::Three, IE_Pressed, this, &AAntCharacter::PickClass3);
	Input->BindKey(EKeys::Four, IE_Pressed, this, &AAntCharacter::PickClass4);
}

void AAntCharacter::SurfaceAxes(FVector& OutForward, FVector& OutRight) const
{
	const UAntMovementComponent* M = GetAntMovement();
	const FVector N = M ? M->GetSurfaceNormal() : FVector::UpVector;
	const FRotator CR = Controller ? Controller->GetControlRotation() : GetActorRotation();
	const FRotationMatrix CM(CR);
	const FVector CamF = CM.GetUnitAxis(EAxis::X);
	const FVector CamR = CM.GetUnitAxis(EAxis::Y);
	const FVector CamU = CM.GetUnitAxis(EAxis::Z);

	if (N.Z > 0.95f)
	{
		// Plain floor: classic third-person controls.
		const FRotator Yaw(0.f, CR.Yaw, 0.f);
		OutForward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
		OutRight = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
		return;
	}

	// On walls/ceilings: "forward" is where the camera looks, projected on the surface.
	// Looking straight at a wall, forward blends towards camera-up (= up the wall).
	const FVector PF = FVector::VectorPlaneProject(CamF, N);
	const float W = FMath::Clamp(1.f - (float)PF.Size(), 0.f, 1.f);
	OutForward = (PF + FVector::VectorPlaneProject(CamU, N) * W).GetSafeNormal();
	OutRight = FVector::VectorPlaneProject(CamR, N).GetSafeNormal();
	if (OutForward.IsNearlyZero())
	{
		OutForward = FVector::CrossProduct(OutRight, N).GetSafeNormal();
	}
}

void AAntCharacter::MoveForward(float Value)
{
	if (!Controller || bDead || IsAttached() || FMath::IsNearlyZero(Value))
	{
		return;
	}
	FVector F, R;
	SurfaceAxes(F, R);
	AddMovementInput(F, Value);
}

void AAntCharacter::MoveRight(float Value)
{
	if (!Controller || bDead || IsAttached() || FMath::IsNearlyZero(Value))
	{
		return;
	}
	FVector F, R;
	SurfaceAxes(F, R);
	AddMovementInput(R, Value);
}

void AAntCharacter::TurnRate(float Value)
{
	AddControllerYawInput(Value * 60.f * GetWorld()->GetDeltaSeconds());
}

void AAntCharacter::LookUpRate(float Value)
{
	AddControllerPitchInput(Value * 45.f * GetWorld()->GetDeltaSeconds());
}

void AAntCharacter::StartFire() { if (!bDead) { bWantsFire = true; } }
void AAntCharacter::StopFire() { bWantsFire = false; }

void AAntCharacter::Interact()
{
	if (!bDead)
	{
		ServerInteract();
	}
}

void AAntCharacter::StartSprint()
{
	bSprinting = true;
	UpdateSpeed();
	ServerSetSprint(true);
}

void AAntCharacter::StopSprint()
{
	bSprinting = false;
	UpdateSpeed();
	ServerSetSprint(false);
}

void AAntCharacter::ServerSetSprint_Implementation(bool bNewSprint)
{
	bSprinting = bNewSprint;
	UpdateSpeed();
}

void AAntCharacter::DoJump()
{
	if (!bDead && IsAttached())
	{
		LinkPressed();   // wriggle free / let go of the chain
		return;
	}
	if (!bDead)
	{
		Jump();
		AntSfx::Play(this, AntSfx::Jump, GetActorLocation(), 0.5f);
	}
}

void AAntCharacter::DoStopJump()
{
	StopJumping();
}

void AAntCharacter::ToggleTrail()
{
	if (!bDead)
	{
		ServerToggleTrail();
	}
}

void AAntCharacter::RecallSwarm()
{
	ServerRecallSwarm();
}

// ---------------------------------------------------------------------------------------------
// Pheromone trails: start at food, crawl home; the swarm follows the trail.
// ---------------------------------------------------------------------------------------------

void AAntCharacter::ServerToggleTrail_Implementation()
{
	if (bDead)
	{
		return;
	}
	if (bLayingTrail)
	{
		CancelTrail(TEXT("Тропа отменена"));
		return;
	}
	if (IsInNest())
	{
		Toast(TEXT("Тропу начинают у еды и ведут домой, в муравейник"));
		return;
	}
	AKitchenLoot* Food = nullptr;
	float Best = 480.f;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (!It->bIsGear && D < Best)
		{
			Best = D;
			Food = *It;
		}
	}
	if (!Food)
	{
		Toast(TEXT("Рядом нет еды. Встань у еды и нажми T, потом ползи в муравейник"));
		return;
	}
	TrailFood = Food;
	TrailPoints.Reset();
	TrailNormals.Reset();
	TrailPoints.Add(Food->GetActorLocation() - FVector(0.f, 0.f, 30.f));
	TrailNormals.Add(FVector::UpVector);
	bLayingTrail = true;
	Toast(FString::Printf(TEXT("Метка феромона у «%s». Ползи в муравейник — рой пойдёт по твоему следу"), *Food->LootName));
}

void AAntCharacter::ServerRecallSwarm_Implementation()
{
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		const int32 N = Swarm->RecallStreams(GetDisplayName());
		Toast(N > 0 ? FString::Printf(TEXT("Твои тропы стёрты, %d рабочих вернулись в гнездо"), N) : FString(TEXT("У тебя нет троп")));
	}
}

void AAntCharacter::CancelTrail(const TCHAR* Why)
{
	bLayingTrail = false;
	TrailPoints.Reset();
	TrailNormals.Reset();
	TrailFood = nullptr;
	if (Why)
	{
		Toast(Why);
	}
}

void AAntCharacter::TickTrail()
{
	if (!bLayingTrail)
	{
		return;
	}
	if (!TrailFood.IsValid())
	{
		CancelTrail(TEXT("Еду унесли — тропа пропала"));
		return;
	}
	const FVector P = GetActorLocation();
	const UAntMovementComponent* M = GetAntMovement();
	const FVector N = (M && M->IsCrawling()) ? M->GetSurfaceNormal() : FVector::UpVector;
	if (FVector::Dist(P, TrailPoints.Last()) >= TrailStep && (M && M->IsCrawling()))
	{
		TrailPoints.Add(P - N * (BodyRadius - 2.f));
		TrailNormals.Add(N);
		if (TrailPoints.Num() > TrailMaxPoints)
		{
			CancelTrail(TEXT("Тропа слишком длинная, феромон выветрился"));
			return;
		}
	}

	if (IsInNest())
	{
		TArray<FVector> Pts;
		TArray<FVector> Nrm;
		Pts.Add(AKitchenBuilder::NestLocation() + FVector(0.f, 0.f, 2.f));
		Nrm.Add(FVector::UpVector);
		for (int32 i = TrailPoints.Num() - 1; i >= 0; --i)
		{
			Pts.Add(TrailPoints[i]);
			Nrm.Add(TrailNormals.IsValidIndex(i) ? TrailNormals[i] : FVector::UpVector);
		}
		AKitchenLoot* Food = TrailFood.Get();
		const FString FoodName = Food ? Food->LootName : FString();
		CancelTrail(nullptr);

		FString Why;
		AAntSwarm* Swarm = AAntSwarm::Get(GetWorld());
		const int32 Sent = Swarm ? Swarm->CreateStream(Pts, Nrm, Food, this, Why) : 0;
		if (Sent > 0)
		{
			Toast(FString::Printf(TEXT("Тропа готова! %d рабочих пошли за «%s»"), Sent, *FoodName));
			if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
			{
				GM->Broadcast(FString::Printf(TEXT("%s проложил тропу: %d муравьёв идут за «%s»"), *GetDisplayName(), Sent, *FoodName), FLinearColor(0.5f, 1.f, 0.6f));
			}
		}
		else
		{
			Toast(Why);
		}
	}
}

void AAntCharacter::UpdateTrailDots()
{
	const int32 Want = bDead ? 0 : TrailPoints.Num();
	while (TrailDots.Num() < Want)
	{
		UStaticMeshComponent* D = AntKit::AddPart(this, RootComponent, EAntShape::Sphere, FVector::ZeroVector, FVector(0.12f), FLinearColor(0.3f, 1.f, 0.35f));
		D->SetUsingAbsoluteLocation(true);
		D->SetUsingAbsoluteRotation(true);
		D->SetCastShadow(false);
		TrailDots.Add(D);
	}
	for (int32 i = 0; i < TrailDots.Num(); ++i)
	{
		const bool bShow = i < Want;
		TrailDots[i]->SetVisibility(bShow);
		if (bShow)
		{
			const float Pulse = 0.10f + 0.04f * FMath::Sin(AnimTime * 6.f - i * 0.5f);
			TrailDots[i]->SetWorldLocation(FVector(TrailPoints[i]) + FVector(0.f, 0.f, 4.f));
			TrailDots[i]->SetWorldScale3D(FVector(Pulse));
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Launch / teleport (movement is client-authoritative, so the server asks the owner to do it)
// ---------------------------------------------------------------------------------------------

void AAntCharacter::ApplyLaunch(const FVector& V)
{
	if (bDead)
	{
		return;
	}
	if (UAntMovementComponent* M = GetAntMovement())
	{
		M->BlockSticking(0.35f);
	}
	LaunchCharacter(V, true, true);
	if (!IsLocallyControlled())
	{
		ClientLaunch(V);
	}
}

void AAntCharacter::ClientLaunch_Implementation(FVector_NetQuantize10 V)
{
	if (UAntMovementComponent* M = GetAntMovement())
	{
		M->BlockSticking(0.35f);
	}
	LaunchCharacter(V, true, true);
}

void AAntCharacter::ApplyTeleport(const FVector& Location)
{
	if (bDead)
	{
		return;
	}
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (!IsLocallyControlled())
	{
		ClientTeleport(Location);
	}
	for (FAntLeg& L : Legs)
	{
		L.bInit = false;
	}
}

void AAntCharacter::ClientTeleport_Implementation(FVector_NetQuantize Location)
{
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	for (FAntLeg& L : Legs)
	{
		L.bInit = false;
	}
}

// ---------------------------------------------------------------------------------------------
// Environment: temperature, honey, water, electricity
// ---------------------------------------------------------------------------------------------

void AAntCharacter::ApplyEnvironment(const FAntEnv& Env, float Dt)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}

	FAntEnv E = Env;
	if (IsInNest())
	{
		E.Temp(24.f, 30.f);    // the nest is cozy
		E.Dps = 0.f;
	}

	Temperature = FMath::FInterpConstantTo(Temperature, E.TargetTemp, Dt, E.Rate);
	// Fire: catch it on very hot spots, water puts it out, it spreads to ants you touch.
	if (Temperature > 85.f && BurnTime <= 0.f)
	{
		Ignite(5.f);
	}
	if ((E.Flags & AntStatus::Wet) && BurnTime > 0.f)
	{
		BurnTime = 0.f;
		AntSfx::Everyone(this, AntSfx::Splash, GetActorLocation(), 1.4f);
		Toast(TEXT("Пшшш! Вода потушила огонь"));
	}
	if (BurnTime > 0.f)
	{
		BurnTime -= Dt;
		E.Flags |= AntStatus::Burning;
		E.Hurt(14.f, TEXT("Ты ГОРИШЬ! Беги к воде"));
		E.Temp(90.f, 25.f);
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* O = *It;
			if (O && O != this && !O->bDead && O->BurnTime <= 0.f && !O->IsInNest()
				&& FVector::Dist(O->GetActorLocation(), GetActorLocation()) < BodyRadius * 5.f)
			{
				O->Ignite(4.f);
				if (IsEnemy(O)) { Toast(FString::Printf(TEXT("Поджёг: %s!"), *O->GetDisplayName())); }
			}
		}
	}
	if (WebTime > 0.f)
	{
		WebTime -= Dt;
		E.Flags |= AntStatus::Webbed;
	}
	StatusFlags = E.Flags;

	float Dps = E.Dps;
	const TCHAR* Cause = *E.Cause;
	if (BurnTime > 0.f)
	{
		LastHazard = TEXT("сгорел заживо");
	}
	if (Temperature > 55.f)
	{
		Dps += (Temperature - 55.f) * 0.6f;
		Cause = TEXT("Горячо! Убирайся с огня");
	}
	else if (Temperature < -4.f)
	{
		Dps += 3.f + (-4.f - Temperature) * 0.3f;
		Cause = TEXT("Ты замерзаешь! Беги к теплу");
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Dps > 0.f)
	{
		LastHazard = BurnTime > 0.f ? TEXT("сгорел заживо") : Temperature > 55.f ? TEXT("поджарился") : (Temperature < -4.f ? TEXT("замёрз насмерть") :
			(HasStatus(AntStatus::Drowning) ? TEXT("утонул в раковине") : TEXT("не пережил кухню")));
		LastHazardTime = Now;
		DamageAccum += Dps * Dt;
		if (DamageAccum >= 1.f)
		{
			const float Whole = FMath::FloorToFloat(DamageAccum);
			DamageAccum -= Whole;
			UGameplayStatics::ApplyDamage(this, Whole, nullptr, nullptr, UDamageType::StaticClass());
		}
		if (Now - LastEnvToast > 3.f && Cause && *Cause)
		{
			LastEnvToast = Now;
			Toast(Cause);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AAntCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDead)
	{
		if (IsLocallyControlled())
		{
			TickFire();
		}

		if (HasAuthority())
		{
			TickNurse(DeltaSeconds);

			if (GetWorld()->GetTimeSeconds() - LastDamageTime > 5.f && Health < MaxHealth)
			{
				Health = FMath::Min(MaxHealth, Health + 6.f * DeltaSeconds);
			}

			if (GetActorLocation().Z < -800.f)
			{
				Die(nullptr, nullptr);
			}
			TickTrail();
			if (HoldingAnt && (!IsValid(HoldingAnt) || HoldingAnt->bDead || HoldingAnt->HeldBy != this))
			{
				HoldingAnt = nullptr;
			}
		}
		ApplyAttachedPose();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateTrailDots();
	}

	UpdateSpeed();
	UpdateVisuals(DeltaSeconds);
}

void AAntCharacter::TickFire()
{
	if (!bWantsFire || !Controller)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextFireTime)
	{
		return;
	}
	const float ColdMult = Temperature < 8.f ? 0.6f : 1.f;
	NextFireTime = Now + 1.f / FMath::Max(0.2f, Weapon.FireRate * ColdMult);

	const FVector Origin = GetMuzzleLocation();
	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Aim = CamRot.Vector();
	const FVector End = CamLoc + Aim * 30000.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntAim), false, this);
	const FVector Target = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params) ? FVector(Hit.ImpactPoint) : End;

	FVector Dir = (Target - Origin).GetSafeNormal();
	if (FVector::DotProduct(Target - Origin, Aim) < 50.f)
	{
		Dir = Aim;
	}
	ServerFire(Origin, Dir);
}

void AAntCharacter::BotFire(const FVector& Direction)
{
	if (HasAuthority())
	{
		ServerFire_Implementation(GetMuzzleLocation(), Direction.GetSafeNormal());
	}
}

void AAntCharacter::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
	if (bDead)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < ServerNextFireTime)
	{
		return;
	}
	ServerNextFireTime = Now + 0.8f / FMath::Max(0.2f, Weapon.FireRate);

	FVector Start = Origin;
	if (FVector::Dist(Start, GetActorLocation()) > 200.f)
	{
		Start = GetMuzzleLocation();
	}

	AntSfx::Everyone(this, Weapon.bWeb ? AntSfx::Web : AntSfx::Spit, Start, FMath::Clamp(1.3f - Weapon.Damage * 0.01f, 0.6f, 1.3f));
	const int32 Pellets = FMath::Max<int32>(1, Weapon.Pellets);
	for (int32 i = 0; i < Pellets; ++i)
	{
		const FVector D = FMath::VRandCone(FVector(Direction), FMath::DegreesToRadians(Weapon.Spread));
		const FTransform SpawnTM(D.Rotation(), Start);
		AAcidProjectile* P = GetWorld()->SpawnActorDeferred<AAcidProjectile>(AAcidProjectile::StaticClass(), SpawnTM, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (P)
		{
			FAntWeapon W = Weapon;
			W.Damage *= DamageMult();
			P->Setup(W, D);
			P->Team = Team;
			P->FinishSpawning(SpawnTM);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Looting and gadgets
// ---------------------------------------------------------------------------------------------

AKitchenLoot* AAntCharacter::FindNearestLoot(float Radius) const
{
	AKitchenLoot* Best = nullptr;
	float BestDist = Radius;
	const FVector Me = GetActorLocation();
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		// Big food: measured from its edge, not its middle.
		const float D = FVector::Dist(It->GetActorLocation(), Me) - (It->bIsGear ? 0.f : It->GetRadius() * 0.7f);
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

AKitchenInteractable* AAntCharacter::FindNearestInteractable(float* OutDistance) const
{
	AKitchenInteractable* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	const FVector Me = GetActorLocation();
	for (TActorIterator<AKitchenInteractable> It(GetWorld()); It; ++It)
	{
		AKitchenInteractable* I = *It;
		if (!I || !I->CanInteract(this))
		{
			continue;
		}
		const float D = I->DistanceTo(Me);
		if (D < I->InteractRadius && D < BestDist)
		{
			BestDist = D;
			Best = I;
		}
	}
	if (OutDistance)
	{
		*OutDistance = BestDist;
	}
	return Best;
}

void AAntCharacter::ServerInteract_Implementation()
{
	if (bDead)
	{
		return;
	}
	if (HoldingAnt)
	{
		ReleaseAnt(false);
		return;
	}
	if (IsAttached())
	{
		return;
	}
	if (Carrying)
	{
		Toast(FString::Printf(TEXT("Отпустил «%s»"), *Carrying->LootName));
		LetGo();
		return;
	}
	// A fly picks up ants of its colony (and drops them wherever).
	if (Creature == 1)
	{
		if (AAntCharacter* Ally = FindGrabbableAlly(InteractRadius + 80.f))
		{
			AKitchenLoot* L = FindNearestLoot(InteractRadius + 30.f);
			if (!L || FVector::Dist(Ally->GetActorLocation(), GetActorLocation()) < FVector::Dist(L->GetActorLocation(), GetActorLocation()))
			{
				GrabAnt(Ally);
				return;
			}
		}
	}

	float GadgetDist = 0.f;
	AKitchenInteractable* Gadget = FindNearestInteractable(&GadgetDist);
	AKitchenLoot* Loot = FindNearestLoot(InteractRadius + 30.f);
	UE_LOG(LogTemp, Display, TEXT("ANTCARRY interact loot=%s gadget=%s"), Loot ? *Loot->LootName : TEXT("-"), Gadget ? *Gadget->GetName() : TEXT("-"));
	const float LootDist = Loot ? FVector::Dist(Loot->GetActorLocation(), GetActorLocation()) : TNumericLimits<float>::Max();

	if (Gadget && (!Loot || GadgetDist < LootDist))
	{
		Gadget->Interact(this);
		return;
	}
	if (!Loot)
	{
		return;
	}

	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();

	if (Loot->bIsGear && Creature != 0)
	{
		Toast(TEXT("Это оружие для муравьёв"));
		return;
	}
	if (Loot->bIsGear)
	{
		const FAntWeapon Old = Weapon;
		Weapon = Loot->Gear;
		OnRep_Weapon();
		AntSfx::Everyone(this, Weapon.Rarity >= 3 ? AntSfx::Legendary : AntSfx::PickupGear, GetActorLocation(), 1.f);
		const FVector DropAt = Loot->GetActorLocation();
		Loot->Destroy();
		if (GM)
		{
			GM->SpawnGear(DropAt, Old);
		}
		Toast(FString::Printf(TEXT("Экипировано: %s"), *Weapon.Name));
		return;
	}

	GrabLoot(Loot);
}

FVector AAntCharacter::GetHoldPoint() const
{
	const FVector Up = VisNormal;
	FVector Fwd = GetActorForwardVector();
	Fwd = (Fwd - Up * FVector::DotProduct(Fwd, Up)).GetSafeNormal();
	const float S = VisScale() / AntScale;
	return GetActorLocation() + (Up * 30.f + Fwd * 18.f) * AntScale * S * 1.3f;
}

void AAntCharacter::Webbed(float Seconds)
{
	if (!HasAuthority() || bDead || Creature == 2)
	{
		return;
	}
	if (WebTime <= 0.f)
	{
		Toast(TEXT("Тебя опутала паутина! Не двинуться..."));
	}
	WebTime = FMath::Max(WebTime, Seconds);
	StatusFlags |= AntStatus::Webbed;
	if (Carrying) { LetGo(); }
}

void AAntCharacter::BotBite()
{
	if (HasAuthority())
	{
		ServerBite_Implementation();
	}
}

void AAntCharacter::Ignite(float Seconds)
{
	if (!HasAuthority() || bDead || IsInNest() || HasStatus(AntStatus::Wet))
	{
		return;
	}
	if (BurnTime <= 0.f)
	{
		AntSfx::Everyone(this, AntSfx::Sizzle, GetActorLocation(), 1.f);
		Toast(TEXT("ТЫ ГОРИШЬ! В воду, быстро! (и поджигай врагов, пока горишь)"));
	}
	BurnTime = FMath::Max(BurnTime, Seconds);
	StatusFlags |= AntStatus::Burning;
}

void AAntCharacter::Bite()
{
	if (bDead || GetWorld()->GetTimeSeconds() < NextBiteTime)
	{
		return;
	}
	NextBiteTime = GetWorld()->GetTimeSeconds() + 0.7f;
	// Lunge forward along the surface (client-side movement).
	if (UAntMovementComponent* M = GetAntMovement())
	{
		FVector F = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
		F = FVector::VectorPlaneProject(F, VisNormal).GetSafeNormal();
		M->Velocity += F * 650.f;
	}
	AddTrauma(0.15f);
	ServerBite();
}

void AAntCharacter::ServerBite_Implementation()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (bDead || Now < ServerNextBiteTime)
	{
		return;
	}
	ServerNextBiteTime = Now + 0.55f;
	AntSfx::Everyone(this, AntSfx::Bite, GetActorLocation(), FMath::FRandRange(0.9f, 1.2f));
	const FVector Me = GetActorLocation();
	FVector Fwd = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
	const float Reach = 95.f;
	const float Dmg = 30.f * DamageMult();
	bool bHit = false;
	AAntPlayerController* PC = Cast<AAntPlayerController>(GetController());

	// Enemy ants in front.
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* O = *It;
		if (!O || O == this || O->bDead || !IsEnemy(O))
		{
			continue;
		}
		const FVector To = O->GetActorLocation() - Me;
		if (To.Size() < Reach && FVector::DotProduct(To.GetSafeNormal(), Fwd) > 0.2f)
		{
			UGameplayStatics::ApplyDamage(O, Dmg, GetController(), this, UDamageType::StaticClass());
			O->ApplyLaunch(To.GetSafeNormal() * 520.f + O->GetVisualUp() * 260.f);
			if (O->Carrying) { O->LetGo(); O->Toast(TEXT("Тебя укусили — груз выпал!")); }
			if (PC) { PC->ClientDamageNumber(O->GetActorLocation(), FMath::RoundToInt(Dmg), false); }
			bHit = true;
		}
	}
	// Riding the Human: chew on whatever is under your feet (more painful than the ankles).
	AHumanCharacter* Ridden = GetRiddenHuman();
	if (Ridden && !Ridden->IsKnockedOut())
	{
		UGameplayStatics::ApplyDamage(Ridden, Dmg * 1.5f, GetController(), this, UDamageType::StaticClass());
		if (PC) { PC->ClientDamageNumber(Me + Fwd * 40.f, FMath::RoundToInt(Dmg * 1.5f), false); }
		bHit = true;
	}
	// The Human's ankles.
	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		AHumanCharacter* H = *It;
		if (H == Ridden)
		{
			continue;
		}
		const FVector Foot = H->GetActorLocation() - FVector(0.f, 0.f, H->GetSimpleCollisionHalfHeight());
		if (FVector::Dist2D(Foot, Me) < H->GetSimpleCollisionRadius() + 140.f && Me.Z - Foot.Z < 400.f)
		{
			UGameplayStatics::ApplyDamage(H, Dmg * 0.8f, GetController(), this, UDamageType::StaticClass());
			if (PC) { PC->ClientDamageNumber(Me + Fwd * 40.f, FMath::RoundToInt(Dmg * 0.8f), false); }
			bHit = true;
		}
	}
	// Big food: bite a piece off it that you can carry alone.
	if (!bHit && !Carrying)
	{
		AKitchenLoot* L = FindNearestLoot(InteractRadius);
		if (L && !L->bIsGear && !L->IsCarried())
		{
			if (AKitchenLoot* Piece = L->BiteOff(this))
			{
				AntSfx::Everyone(this, AntSfx::Crunch, L->GetActorLocation(), 1.5f);
				GrabLoot(Piece);
			}
		}
	}
}

void AAntCharacter::ToggleBodycam()
{
	bBodycam = !bBodycam;
	ApplyCameraLook();
	if (AAntPlayerController* PC = Cast<AAntPlayerController>(GetController()))
	{
		PC->ClientToast(bBodycam ? TEXT("BODYCAM: камера на голове муравья (V — обратно)") : TEXT("Камера от третьего лица (V — bodycam)"));
	}
}

void AAntCharacter::ApplyCameraLook()
{
	if (!Camera || !IsLocallyControlled())
	{
		return;
	}
	// A cheap action-camera look: lens fringe, grain, vignette, motion blur, punchy contrast.
	FPostProcessSettings& P = Camera->PostProcessSettings;
	P.bOverride_VignetteIntensity = true;      P.VignetteIntensity = bBodycam ? 0.95f : 0.5f;
	P.bOverride_SceneFringeIntensity = true;   P.SceneFringeIntensity = bBodycam ? 3.5f : 1.2f;
	P.bOverride_FilmGrainIntensity = true;     P.FilmGrainIntensity = bBodycam ? 0.45f : 0.12f;
	P.bOverride_MotionBlurAmount = true;       P.MotionBlurAmount = bBodycam ? 0.9f : 0.45f;
	P.bOverride_BloomIntensity = true;         P.BloomIntensity = 1.1f;
	P.bOverride_ColorSaturation = true;        P.ColorSaturation = FVector4(1.f, 1.f, 1.f, bBodycam ? 0.85f : 1.05f);
	P.bOverride_ColorContrast = true;          P.ColorContrast = FVector4(1.f, 1.f, 1.f, bBodycam ? 1.15f : 1.05f);
	P.bOverride_DepthOfFieldFocalDistance = true;  P.DepthOfFieldFocalDistance = bBodycam ? 60.f : 180.f;
	P.bOverride_DepthOfFieldFstop = true;      P.DepthOfFieldFstop = bBodycam ? 2.8f : 4.f;
	Camera->PostProcessBlendWeight = 1.f;
	Camera->SetFieldOfView(bBodycam ? 112.f : 95.f);
	// Fisheye-ish lens for the bodycam.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ConsoleCommand(bBodycam ? TEXT("r.Upscale.Panini.D 0.8") : TEXT("r.Upscale.Panini.D 0"), false);
	}
}

FVector AAntCharacter::AimPoint() const
{
	FVector CamLoc;
	FRotator CamRot;
	if (!Controller)
	{
		return GetActorLocation();
	}
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(AntCmdAim), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamRot.Vector() * 20000.f, ECC_Visibility, P))
	{
		return Hit.ImpactPoint;
	}
	return CamLoc + CamRot.Vector() * 3000.f;
}

void AAntCharacter::CmdArmy() { ServerCommand(0, GetActorLocation()); }
void AAntCharacter::CmdArmyTarget() { ServerCommand(1, AimPoint()); }
void AAntCharacter::CmdFollow() { ServerCommand(2, GetActorLocation()); }
void AAntCharacter::CmdAttack() { ServerCommand(3, AimPoint()); }
void AAntCharacter::CmdFree() { ServerCommand(4, GetActorLocation()); }

void AAntCharacter::ServerCommand_Implementation(uint8 Cmd, FVector_NetQuantize Point)
{
	if (bDead || Creature != 0 || Team > 1)
	{
		return;
	}
	AAntArmy* Army = AAntArmy::FindFor(this);
	if (Cmd == 0)
	{
		if (Army)
		{
			Army->Disband();
			Toast(TEXT("Армия возвращается в муравейник"));
			return;
		}
		// Take workers out of the nest.
		int32 Avail = 0;
		AAntSwarm* Swarm = AAntSwarm::Get(GetWorld());
		AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
		const bool bWar = GS && GS->bWarMode;
		if (bWar) { Avail = GS->WarPool(Team); }
		else if (Team == 0 && Swarm) { Avail = Swarm->Idle; }
		else if (GS) { Avail = GS->RivalWorkers; }
		const int32 N = bWar ? FMath::Min(Avail, 48) : FMath::Min(Avail, FMath::Clamp(Avail / 2, 150, 4000));
		if (N < (bWar ? 8 : 30))
		{
			Toast(bWar ? TEXT("Резерв армии пуст — держите территории и кассы, придёт подкрепление")
				: TEXT("В муравейнике почти нет свободных рабочих — тащите еду, они вылупятся"));
			return;
		}
		if (bWar)
		{
			if (AAntKitchenGameMode* WGM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>()) { WGM->AddPool(Team, -N); }
		}
		else if (Team == 0 && Swarm) { Swarm->Idle -= N; } else if (GS) { GS->RivalWorkers -= N; }
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Start = AKitchenBuilder::NestLocation(Team) + FVector(0.f, 0.f, 10.f);
		AAntArmy* A = GetWorld()->SpawnActorDeferred<AAntArmy>(AAntArmy::StaticClass(), FTransform(Start), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			A->Team = Team;
			A->Count = N;
			A->Commander = this;
			A->Mode = EArmyMode::Follow;
			A->FinishSpawning(FTransform(Start));
		}
		Toast(FString::Printf(TEXT("АРМИЯ: %d муравьёв вышли из муравейника за тобой! R — в атаку на прицел"), N));
		AntSfx::Everyone(this, AntSfx::ArmyCharge, GetActorLocation(), 1.f);
		return;
	}
	if (Cmd == 1)
	{
		if (!Army)
		{
			Toast(TEXT("Нет армии — нажми F"));
			return;
		}
		if (FVector::Dist(Point, GetActorLocation()) < 350.f)
		{
			Army->Mode = EArmyMode::Follow;
			Toast(TEXT("Армия: за мной!"));
		}
		else
		{
			Army->Mode = EArmyMode::Attack;
			Army->Target = Point;
			Toast(TEXT("Армия: В АТАКУ!"));
			AntSfx::Everyone(this, AntSfx::ArmyCharge, GetActorLocation(), 1.1f);
		}
		return;
	}
	// Orders for the bots of my colony.
	int32 N = 0;
	for (TActorIterator<AAntBotController> It(GetWorld()); It; ++It)
	{
		AAntCharacter* B = Cast<AAntCharacter>(It->GetPawn());
		if (B && B->Team == Team && B->Creature == 0)
		{
			It->SetOrder(Cmd == 2 ? 1 : Cmd == 3 ? 2 : 0, this, Point);
			++N;
		}
	}
	Toast(Cmd == 2 ? FString::Printf(TEXT("Отряд (%d): за мной!"), N) : Cmd == 3 ? FString::Printf(TEXT("Отряд (%d): атакуем точку!"), N) : TEXT("Отряд: свободная охота за едой"));
}

void AAntCharacter::DevInteract()
{
	ServerInteract_Implementation();
}

bool AAntCharacter::GrabLoot(AKitchenLoot* Loot)
{
	if (!HasAuthority() || !Loot || bDead || Loot->bIsGear || GetStrength() <= 0.f)
	{
		return false;
	}
	if (Carrying && Carrying != Loot)
	{
		LetGo();
	}
	FString Why;
	if (!Loot->TryGrab(this, Why))
	{
		if (!Why.IsEmpty()) { Toast(Why); }
		return false;
	}
	const float Ratio = Loot->CarryRatio();
	if (Loot->CarryTeam() != Team)
	{
		Toast(FString::Printf(TEXT("Перетягивание! Чужие держат «%s»"), *Loot->LootName));
	}
	else if (Ratio >= 1.f)
	{
		Toast(FString::Printf(TEXT("Тащим «%s» (+%d) в муравейник!"), *Loot->LootName, Loot->Value));
	}
	else if (Ratio >= 0.5f)
	{
		Toast(FString::Printf(TEXT("«%s» тяжёлый: волочём медленно. Позови помощь!"), *Loot->LootName));
	}
	else
	{
		Toast(FString::Printf(TEXT("«%s» не сдвинуть! Нужно ещё %d муравья"), *Loot->LootName,
			FMath::Max(1, Loot->AntsNeeded(GetStrength()) - Loot->NumCarriers())));
	}
	return true;
}

void AAntCharacter::LetGo()
{
	if (HasAuthority() && Carrying)
	{
		AKitchenLoot* L = Carrying;
		Carrying = nullptr;
		L->Release(this);
		OnRep_Carry();
	}
}

void AAntCharacter::SetCarrying(AKitchenLoot* Loot)
{
	Carrying = Loot;
	if (Loot && bSprinting)
	{
		bSprinting = false;
	}
	OnRep_Carry();
}

void AAntCharacter::DropEverything()
{
	LetGo();
}

// ---------------------------------------------------------------------------------------------
// Damage and death
// ---------------------------------------------------------------------------------------------

float AAntCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bDead || IsInNest())
	{
		return 0.f;   // the own nest is a safe zone
	}
	if (const AAntCharacter* By = EventInstigator ? Cast<AAntCharacter>(EventInstigator->GetPawn()) : nullptr)
	{
		if (By != this && !IsEnemy(By))
		{
			return 0.f;   // no friendly fire
		}
	}
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Actual <= 0.f)
	{
		return 0.f;
	}
	Health = FMath::Max(0.f, Health - Actual);
	LastDamageTime = GetWorld()->GetTimeSeconds();
	if (Health <= 0.f)
	{
		Die(EventInstigator, DamageCauser);
	}
	return Actual;
}

void AAntCharacter::Die(AController* Killer, AActor* Causer)
{
	if (bDead)
	{
		return;
	}
	DropEverything();
	if (HoldingAnt) { ReleaseAnt(false); }
	if (HeldBy && IsValid(HeldBy)) { HeldBy->ReleaseAnt(false); }
	if (Chain) { Chain->Leave(this); }
	if (bLayingTrail)
	{
		CancelTrail(TEXT("Ты погиб — тропа оборвалась"));
	}
	bDead = true;
	BurnTime = 0.f;
	AntSfx::Everyone(this, AntSfx::Squish, GetActorLocation(), FMath::FRandRange(0.85f, 1.15f));
	OnRep_Dead();

	if (AAntPlayerState* PS = GetPlayerState<AAntPlayerState>())
	{
		PS->Deaths++;
	}
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->OnAntKilled(this, Killer, Causer);
	}
}

// ---------------------------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------------------------

bool AAntCharacter::IsInNest() const
{
	if (Team >= 2)
	{
		return false;   // monsters have no safe place
	}
	const FVector L = GetActorLocation();
	return FVector::Dist2D(L, AKitchenBuilder::NestLocation(Team)) < AKitchenBuilder::NestRadius(Team) && L.Z < 400.f;
}

FVector AAntCharacter::GetMuzzleLocation() const
{
	FVector Aim = GetActorForwardVector();
	if (Controller)
	{
		Aim = Controller->GetControlRotation().Vector();
	}
	return GetActorLocation() + VisNormal * 7.f + Aim * 26.f;
}

FVector AAntCharacter::GetGroundLocation() const
{
	const FVector Start = GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntGround), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start - FVector(0.f, 0.f, 6000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint + FVector(0.f, 0.f, 35.f);
	}
	return Start;
}

FString AAntCharacter::GetDisplayName() const
{
	if (const APlayerState* PS = GetPlayerState())
	{
		return PS->GetPlayerName();
	}
	return Creature == 1 ? TEXT("Муха") : Creature == 2 ? TEXT("Паук") : TEXT("Муравей");
}

void AAntCharacter::HurtBy(float Damage, const FString& Hazard)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	LastHazard = Hazard;
	LastHazardTime = GetWorld()->GetTimeSeconds();
	UGameplayStatics::ApplyDamage(this, Damage, nullptr, nullptr, UDamageType::StaticClass());
}

void AAntCharacter::Toast(const FString& Msg) const
{
	if (AAntPlayerController* PC = Cast<AAntPlayerController>(GetController()))
	{
		PC->ClientToast(Msg);
	}
}

void AAntCharacter::UpdateSpeed()
{
	UAntMovementComponent* M = GetAntMovement();
	if (!M)
	{
		return;
	}
	// Carrying: the heavier the load for the ants holding it, the slower everyone goes.
	float Load = 1.f;
	bool bHeavy = false, bVeryHeavy = false;
	if (Carrying)
	{
		const float R = Carrying->CarryRatio();
		if (Carrying->CarryTeam() != Team) { Load = 0.08f; bVeryHeavy = true; }   // losing a tug of war
		else if (R >= 1.f) { Load = FMath::Clamp(0.5f + 0.25f * (R - 1.f), 0.5f, 0.9f); }
		else if (R >= 0.5f) { Load = FMath::Max(0.05f, 0.43f * (R - 0.5f) / 0.5f); bVeryHeavy = true; }
		else { Load = 0.02f; bVeryHeavy = true; }
		bHeavy = R < 1.4f;
	}
	if (HoldingAnt)
	{
		Load *= 0.6f;   // a friend in the mandibles
	}
	const float ClassSpeed = GetClassDef().Speed * SpeedScale;
	M->MaxWalkSpeed = (bSprinting && !Carrying ? 760.f : 480.f) * Load * ClassSpeed;

	float Mult = 1.f;
	if (HasStatus(AntStatus::Sticky)) { Mult *= 0.35f; }
	if (HasStatus(AntStatus::Wet)) { Mult *= 0.7f; }
	if (Temperature < 8.f)
	{
		Mult *= FMath::Lerp(1.f, 0.45f, FMath::Clamp((8.f - Temperature) / 13.f, 0.f, 1.f));
	}
	if (IsFrozen()) { Mult *= 0.5f; }
	if (HasStatus(AntStatus::Webbed)) { Mult *= 0.08f; }
	M->SpeedMultiplier = Mult;
	M->bNoJump = HasStatus(AntStatus::Sticky) || IsFrozen() || Carrying != nullptr || HoldingAnt != nullptr || HasStatus(AntStatus::Webbed);
	M->bNoClimb = IsFrozen() || bVeryHeavy;
	M->bHeavyLoad = bHeavy;
}

// ---------------------------------------------------------------------------------------------
// Visuals: body aligned to the surface, procedural legs, antennae
// ---------------------------------------------------------------------------------------------

FVector AAntCharacter::EstimateSurfaceNormal() const
{
	// Remote ants: find the closest surface around the body.
	static const FVector Dirs[6] = { -FVector::UpVector, FVector::UpVector, FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector };
	const FVector Me = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntVisNormal), false, this);
	float Best = BodyRadius + 22.f;
	FVector N = FVector::UpVector;
	bool bAny = false;
	for (const FVector& D : Dirs)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Me, Me + D * (BodyRadius + 22.f), UAntMovementComponent::SurfaceObjects(), Params))
		{
			const float Dist = Hit.Distance - (FVector::DotProduct(D, -VisNormal) > 0.7f ? 4.f : 0.f);   // prefer the current surface
			if (Dist < Best)
			{
				Best = Dist;
				N = Hit.ImpactNormal;
				bAny = true;
			}
		}
	}
	return bAny ? N : FVector::UpVector;
}

void AAntCharacter::UpdateVisuals(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || bDead || Legs.Num() == 0)
	{
		return;
	}
	AnimTime += DeltaSeconds;

	// Skip the expensive part for ants far away from the local camera.
	bool bNear = true;
	if (const APlayerController* LPC = GetWorld()->GetFirstPlayerController())
	{
		if (LPC->PlayerCameraManager)
		{
			bNear = FVector::DistSquared(LPC->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) < FMath::Square(9000.f);
		}
	}

	// --- Surface normal ---
	FVector TargetN = FVector::UpVector;
	const bool bCrawling = IsCrawling();
	if (bCrawling)
	{
		TargetN = IsLocallyControlled() ? GetAntMovement()->GetSurfaceNormal() : EstimateSurfaceNormal();
	}
	VisNormal = FMath::VInterpNormalRotationTo(VisNormal, TargetN, DeltaSeconds, 720.f).GetSafeNormal();
	if (VisNormal.IsNearlyZero())
	{
		VisNormal = FVector::UpVector;
	}

	// --- Heading: where we walk, or where we aim when standing/shooting ---
	const FVector VelP = FVector::VectorPlaneProject(GetVelocity(), VisNormal);
	FVector AimDir = GetActorForwardVector();
	if (Controller)
	{
		AimDir = Controller->GetControlRotation().Vector();
	}
	FVector Desired;
	if (VelP.SizeSquared() > FMath::Square(60.f) && !bWantsFire)
	{
		Desired = VelP.GetSafeNormal();
	}
	else
	{
		Desired = FVector::VectorPlaneProject(AimDir, VisNormal);
		if (Desired.SizeSquared() < 0.04f)
		{
			Desired = FVector::VectorPlaneProject(FVector::UpVector, VisNormal);
		}
		Desired = Desired.GetSafeNormal();
		if (Desired.IsNearlyZero())
		{
			Desired = VisHeading;
		}
	}
	VisHeading = FVector::VectorPlaneProject(FMath::VInterpNormalRotationTo(VisHeading, Desired, DeltaSeconds, 540.f), VisNormal).GetSafeNormal();
	if (VisHeading.IsNearlyZero())
	{
		VisHeading = FVector::VectorPlaneProject(FVector::ForwardVector, VisNormal).GetSafeNormal();
		if (VisHeading.IsNearlyZero())
		{
			VisHeading = FVector::RightVector;
		}
	}

	const float Speed = (float)GetVelocity().Size();
	const float Bob = bCrawling ? FMath::Sin(AnimTime * 22.f) * FMath::Min(1.f, Speed / 500.f) * 1.5f : 0.f;
	VisualRoot->SetWorldLocationAndRotation(GetActorLocation() - VisNormal * (2.f - Bob) * AntScale, FRotationMatrix::MakeFromZX(VisNormal, VisHeading).ToQuat());
	if (IsAttached())
	{
		// Held in someone's jaws / gripping the next ant of the chain.
		VisNormal = AttachedUp;
		VisHeading = AttachedAlong;
		VisualRoot->SetWorldLocationAndRotation(GetActorLocation(), FRotationMatrix::MakeFromXZ(AttachedAlong, AttachedUp).ToQuat());
	}

	if (WebShell)
	{
		WebShell->SetVisibility(HasStatus(AntStatus::Webbed));
	}
	// On fire: flickering flames.
	if (FlameShell)
	{
		const bool bFire = IsBurning();
		FlameShell->SetVisibility(bFire);
		FlameLight->SetVisibility(bFire);
		if (bFire)
		{
			const float F = 0.8f + 0.25f * FMath::Sin(AnimTime * 31.f) + 0.15f * FMath::Sin(AnimTime * 17.f);
			FlameShell->SetRelativeScale3D(FVector(0.7f * F, 0.55f * F, 0.75f * F + 0.1f * FMath::Sin(AnimTime * 40.f)));
			FlameLight->SetIntensity(4.f * F);
		}
	}

	if (FrostShell)
	{
		FrostShell->SetVisibility(IsFrozen());
	}

	// Camera pivot sits "above" the ant relative to the surface it crawls on, so it never digs into a ceiling.
	if (IsLocallyControlled())
	{
		SpringArm->SocketOffset = FVector::ZeroVector;
		// Bodycam: camera on the ant's head, shaky and wide. Normal: close third person.
		const FVector Head = VisNormal * 14.f + VisHeading * 16.f;
		SpringArm->TargetOffset = FMath::VInterpTo(SpringArm->TargetOffset, bBodycam ? Head * 0.8f : VisNormal * 30.f, DeltaSeconds, 8.f);
		const float WantLen = bBodycam ? 0.f : (VisNormal.Z < 0.6f ? 175.f : 125.f);
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, WantLen, DeltaSeconds, bBodycam ? 12.f : 4.f);
		// Handheld feel: footsteps, breathing, and trauma from hits / stomps.
		if (LastHealthSeen >= 0.f && Health < LastHealthSeen - 0.5f)
		{
			AddTrauma(FMath::Clamp((LastHealthSeen - Health) / 40.f, 0.15f, 0.8f));
		}
		LastHealthSeen = Health;
		Trauma = FMath::Max(0.f, Trauma - DeltaSeconds * 1.4f);
		const float CamSh = Trauma * Trauma;
		const float CamWalk = FMath::Min(1.f, Speed / 400.f) * (bBodycam ? 1.f : 0.35f);
		const float CamT = AnimTime;
		const FRotator Shake(
			CamSh * 9.f * FMath::PerlinNoise1D(CamT * 23.f) + CamWalk * 1.6f * FMath::Sin(CamT * 21.f) + (bBodycam ? 0.5f * FMath::Sin(CamT * 1.3f) : 0.f),
			CamSh * 9.f * FMath::PerlinNoise1D(CamT * 23.f + 40.f) + CamWalk * 0.8f * FMath::Sin(CamT * 10.5f),
			CamSh * 12.f * FMath::PerlinNoise1D(CamT * 23.f + 80.f) + CamWalk * 1.2f * FMath::Sin(CamT * 10.5f + 1.f));
		Camera->SetRelativeRotation(Shake);
	}

	// Antennae sway, faster when moving.
	for (int32 i = 0; i < AntennaPivots.Num(); ++i)
	{
		const float s = (i == 0) ? -1.f : 1.f;
		const float Ph = AnimTime * (3.f + Speed / 150.f) + i * 1.7f;
		AntennaPivots[i]->SetRelativeRotation(FRotator(40.f + FMath::Sin(Ph) * 10.f, 25.f * s + FMath::Sin(Ph * 0.7f) * 12.f, 0.f));
	}

	if (bNear)
	{
		// Footsteps only for ants close to the listener.
		FootstepVolume = 0.f;
		if (const APlayerController* LPC = GetWorld()->GetFirstPlayerController(); LPC && LPC->PlayerCameraManager)
		{
			const float D = (float)FVector::Dist(LPC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
			FootstepVolume = IsLocallyControlled() ? 0.35f : FMath::Clamp(1.f - D / 900.f, 0.f, 1.f) * 0.3f;
		}
		UpdateLegs(DeltaSeconds, VisNormal);
	}
}

namespace
{
	void PlaceSegment(UStaticMeshComponent* C, const FVector& A, const FVector& B, float Thickness)
	{
		if (!C)
		{
			return;
		}
		const FVector D = B - A;
		const float Len = FMath::Max(1.f, (float)D.Size());
		C->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(D).ToQuat());
		C->SetWorldScale3D(FVector(Thickness / 100.f, Thickness / 100.f, Len / 100.f));
	}
}

void AAntCharacter::UpdateLegs(float Dt, const FVector& N)
{
	const FTransform BodyTM = VisualRoot->GetComponentTransform();
	const bool bGrounded = IsCrawling();
	const FVector Vel = FVector::VectorPlaneProject(GetVelocity(), N);
	const float Speed = (float)Vel.Size();
	const float VS = VisScale();
	const float StepDur = FMath::Clamp(0.13f - Speed * 0.00009f, 0.05f, 0.13f);
	const float Threshold = (16.f + Speed * 0.03f) * VS;
	const float StepHeight = (9.f + Speed * 0.005f) * VS;
	const float L1 = 24.f * VS * LegLength;
	const float L2 = 32.f * VS * LegLength;

	bool GroupStepping[2] = { false, false };
	for (const FAntLeg& L : Legs)
	{
		if (L.T < 1.f) { GroupStepping[L.Group] = true; }
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntFoot), false, this);

	for (int32 i = 0; i < Legs.Num(); ++i)
	{
		FAntLeg& L = Legs[i];
		const FVector RestW = BodyTM.TransformPosition(L.RestLocal);

		FVector Desired = RestW;
		bool bHit = false;
		if (bGrounded)
		{
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, RestW + N * 30.f * VS, RestW - N * 40.f * VS, UAntMovementComponent::SurfaceObjects(), Params))
			{
				Desired = Hit.ImpactPoint;
				bHit = true;
			}
		}

		if (!L.bInit)
		{
			L.Foot = Desired;
			L.T = 1.f;
			L.bInit = true;
		}

		if (!bGrounded || !bHit)
		{
			// Dangling in the air: legs pull in a little and wiggle.
			const FVector Tuck = BodyTM.TransformPosition(L.RestLocal * FVector(0.8f, 0.75f, 0.45f));
			const FVector Wiggle = N * FMath::Sin(AnimTime * 18.f + i) * 3.f * VS;
			L.Foot = FMath::VInterpTo(L.Foot, Tuck + Wiggle, Dt, 18.f);
			L.T = 1.f;
		}
		else if (L.T < 1.f)
		{
			L.T = FMath::Min(1.f, L.T + Dt / StepDur);
			L.Foot = FMath::Lerp(L.From, L.To, L.T) + N * FMath::Sin(L.T * PI) * StepHeight;
		}
		else
		{
			const float Dist = FVector::Dist(L.Foot, Desired);
			if (Dist > 140.f * VS)
			{
				L.Foot = Desired;   // teleported or fell far: snap
			}
			else if (Dist > Threshold && !GroupStepping[1 - L.Group])
			{
				L.From = L.Foot;
				L.To = Desired + Vel * StepDur * 0.6f;
				L.T = 0.f;
				if (!GroupStepping[L.Group] && FootstepVolume > 0.f)
				{
					// One tiny chitin tick per tripod step (louder for your own ant, pitch by size).
					AntSfx::Play(this, Creature == 2 ? AntSfx::SpiderStep : AntSfx::Step, L.To, FootstepVolume, FMath::Clamp(1.4f - VisScale(), 0.45f, 1.5f));
				}
				GroupStepping[L.Group] = true;
			}
		}

		// Two-bone IK: knee bends up and outwards.
		const FVector Hip = BodyTM.TransformPosition(L.HipLocal);
		FVector ToFoot = L.Foot - Hip;
		float D = (float)ToFoot.Size();
		const FVector Dir = D > KINDA_SMALL_NUMBER ? ToFoot / D : -N;
		D = FMath::Clamp(D, 4.f * VS, L1 + L2 - 0.5f * VS);
		const FVector Side = BodyTM.TransformVectorNoScale(FVector(0.f, (float)L.Side, 0.f));
		FVector Pole = FVector::VectorPlaneProject(N + Side * 0.5f, Dir).GetSafeNormal();
		if (Pole.IsNearlyZero())
		{
			Pole = N;
		}
		const float A = (L1 * L1 - L2 * L2 + D * D) / (2.f * D);
		const float H = FMath::Sqrt(FMath::Max(0.f, L1 * L1 - A * A));
		const FVector Knee = Hip + Dir * A + Pole * H;
		const FVector FootDraw = Hip + Dir * FMath::Min((float)ToFoot.Size(), L1 + L2);

		if (bArtLegs)
		{
			const FVector KneeUp = FVector::VectorPlaneProject(N, Dir).GetSafeNormal();
			AntArt::PlaceAlong(LegFemurs[i], AntArt::Get(LegUpperArt), Hip, Knee, KneeUp.IsNearlyZero() ? N : KneeUp, 7.5f * VS * LegThickness, ArtFlipFemur, false);
			AntArt::PlaceAlong(LegTibias[i], AntArt::Get(LegLowerArt), Knee, FootDraw, KneeUp.IsNearlyZero() ? N : KneeUp, 5.5f * VS * LegThickness, ArtFlipTibia, false);
			if (LegCoxas.IsValidIndex(i) && LegCoxas[i])
			{
				const FVector CoxaDir = (Knee - Hip).GetSafeNormal();
				AntArt::PlaceAlong(LegCoxas[i], AntArt::Get(TEXT("SM_Ant_Coxa")), Hip - CoxaDir * 3.f * VS, Hip + CoxaDir * 6.f * VS, N, 8.f * VS, false, false);
			}
		}
		else
		{
			PlaceSegment(LegFemurs[i], Hip, Knee, 7.f * VS * LegThickness);
			PlaceSegment(LegTibias[i], Knee, FootDraw, 5.f * VS * LegThickness);
			if (LegKnees[i])
			{
				LegKnees[i]->SetWorldLocation(Knee);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Holding each other: carry a friend, a fly lifting an ant, living chains and bridges
// ---------------------------------------------------------------------------------------------

float AAntCharacter::AntWeight() const
{
	const float B = GetClassDef().BodyScale;
	return 6.f * B * B * B;
}

bool AAntCharacter::CanSwitchTeam() const
{
	return Creature == 0 && (bDead || IsInNest());
}

AAntCharacter* AAntCharacter::FindGrabbableAlly(float Radius) const
{
	AAntCharacter* Best = nullptr;
	float BestD = Radius;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* O = *It;
		if (!O || O == this || O->bDead || O->Creature != 0 || O->Team != Team || O->IsAttached() || O->HoldingAnt)
		{
			continue;
		}
		const float D = (float)FVector::Dist(O->GetActorLocation(), GetActorLocation());
		if (D < BestD)
		{
			BestD = D;
			Best = O;
		}
	}
	return Best;
}

bool AAntCharacter::GrabAnt(AAntCharacter* O)
{
	if (!HasAuthority() || bDead || !O || O == this || O->bDead || O->Creature != 0 || O->IsAttached() || O->HoldingAnt || HoldingAnt || IsAttached())
	{
		return false;
	}
	if (O->Team != Team)
	{
		Toast(TEXT("Чужих не таскают — их кусают (ПКМ)"));
		return false;
	}
	const float Cap = GetStrength() * 1.1f;
	if (O->AntWeight() > Cap)
	{
		Toast(FString::Printf(TEXT("%s слишком тяжёлый для тебя (вес %.0f, ты поднимаешь %.0f)"), *O->GetDisplayName(), O->AntWeight(), Cap));
		return false;
	}
	if (Carrying) { LetGo(); }
	if (O->Carrying) { O->LetGo(); }
	HoldingAnt = O;
	O->HeldBy = this;
	O->AttachedSince = GetWorld()->GetTimeSeconds();
	O->OnRep_Link();
	UpdateSpeed();
	AntSfx::Everyone(this, AntSfx::Grab, GetActorLocation(), 0.85f);
	Toast(Creature == 1 ? FString::Printf(TEXT("Подхватил %s в воздух! E — сбросить где надо"), *O->GetDisplayName())
		: FString::Printf(TEXT("Несёшь %s в жвалах. G — поставить"), *O->GetDisplayName()));
	O->Toast(FString::Printf(TEXT("%s несёт тебя! Пробел или G — вырваться"), *GetDisplayName()));
	return true;
}

void AAntCharacter::ReleaseAnt(bool bThrow)
{
	if (!HasAuthority() || !HoldingAnt)
	{
		return;
	}
	AAntCharacter* O = HoldingAnt;
	HoldingAnt = nullptr;
	UpdateSpeed();
	if (!IsValid(O) || O->HeldBy != this)
	{
		return;
	}
	O->HeldBy = nullptr;
	O->OnRep_Link();
	FVector V = GetVelocity();
	if (bThrow)
	{
		const FVector Aim = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
		V += Aim * 650.f + FVector(0.f, 0.f, 250.f);
	}
	if (V.SizeSquared() > FMath::Square(60.f))
	{
		O->ApplyLaunch(V);
	}
}

void AAntCharacter::SetChain(AAntChain* NewChain)
{
	if (!HasAuthority())
	{
		return;
	}
	if (NewChain)
	{
		if (Carrying) { LetGo(); }
		if (HoldingAnt) { ReleaseAnt(false); }
		AttachedSince = GetWorld()->GetTimeSeconds();
	}
	Chain = NewChain;
	OnRep_Link();
}

void AAntCharacter::OnRep_Link()
{
	const bool bAtt = IsAttached();
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		const ECollisionResponse R = bAtt ? ECR_Ignore : ECR_Block;
		Cap->SetCollisionResponseToChannel(ECC_Pawn, R);
		Cap->SetCollisionResponseToChannel(ECC_WorldStatic, R);
		Cap->SetCollisionResponseToChannel(ECC_WorldDynamic, R);
		Cap->SetCollisionResponseToChannel(ECC_PhysicsBody, R);
	}
	UCharacterMovementComponent* CM = GetCharacterMovement();
	if (bAtt)
	{
		CM->StopMovementImmediately();
		CM->SetMovementMode(MOVE_Flying);
	}
	else if (!bDead)
	{
		CM->SetMovementMode(Creature == 1 ? MOVE_Flying : MOVE_Falling);
		if (UAntMovementComponent* M = GetAntMovement())
		{
			M->BlockSticking(0.1f);
		}
	}
	for (FAntLeg& L : Legs)
	{
		L.bInit = false;
	}
	UpdateSpeed();
}

void AAntCharacter::ApplyAttachedPose()
{
	FVector P;
	FVector Along = GetActorForwardVector();
	FVector Up = FVector::UpVector;
	if (HeldBy)
	{
		if (!IsValid(HeldBy) || HeldBy->bDead || HeldBy->HoldingAnt != this)
		{
			if (HasAuthority())
			{
				HeldBy = nullptr;
				OnRep_Link();
			}
			return;
		}
		const bool bFly = HeldBy->Creature == 1;
		Up = bFly ? FVector::UpVector : HeldBy->GetVisualUp();
		P = HeldBy->GetHoldPoint() + (bFly ? FVector(0.f, 0.f, -BodyRadius * 1.3f) : Up * BodyRadius * 0.7f);
		Along = HeldBy->GetActorForwardVector();
		Along = FVector::VectorPlaneProject(Along, Up).GetSafeNormal();
		if (!bFly)
		{
			// Carried across the jaws, like real ants carry a nestmate.
			Along = FVector::CrossProduct(Up, Along).GetSafeNormal();
		}
	}
	else if (Chain)
	{
		const int32 Idx = Chain->IndexOf(this);
		if (Idx == INDEX_NONE)
		{
			return;
		}
		P = Chain->LinkPoint(Idx);
		Along = (Chain->PointAt(Idx * AAntChain::LinkLen) - P).GetSafeNormal();   // head towards the one it grips
		Up = FVector::VectorPlaneProject(FVector::UpVector, Along);
		if (Up.SizeSquared() < 0.01f)
		{
			Up = FVector::VectorPlaneProject(FVector::RightVector, Along);
		}
		Up.Normalize();
		// Every second ant flips: real ant chains are a tangle of legs.
		if (Idx % 2 == 1)
		{
			Up = -Up;
		}
	}
	else
	{
		return;
	}
	if (Along.IsNearlyZero())
	{
		Along = FVector::ForwardVector;
	}
	AttachedUp = Up;
	AttachedAlong = Along;
	SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

void AAntCharacter::AltFirePressed()
{
	Bite();
}

void AAntCharacter::LinkPressed()
{
	if (!bDead)
	{
		ServerLink(AimPoint());
	}
}

void AAntCharacter::ServerLink_Implementation(FVector_NetQuantize Aim)
{
	DoLink(Aim);
}

void AAntCharacter::DoLink(const FVector& Aim)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	if (HeldBy)
	{
		if (IsValid(HeldBy)) { HeldBy->ReleaseAnt(false); }
		else { HeldBy = nullptr; OnRep_Link(); }
		Toast(TEXT("Вырвался!"));
		return;
	}
	if (Chain)
	{
		Chain->Leave(this);
		Toast(TEXT("Отцепился от цепочки"));
		return;
	}
	if (HoldingAnt)
	{
		ReleaseAnt(false);
		return;
	}
	if (Creature == 1)
	{
		if (AAntCharacter* A = FindGrabbableAlly(InteractRadius + 80.f)) { GrabAnt(A); }
		else { Toast(TEXT("Рядом нет муравьёв твоей колонии")); }
		return;
	}
	if (Creature != 0)
	{
		return;
	}
	if (Carrying)
	{
		Toast(TEXT("Сначала отпусти груз (E)"));
		return;
	}
	// 1. Someone is building a chain right here: become the next link.
	if (AAntChain* C = AAntChain::FindNearTail(GetWorld(), Team, GetActorLocation(), 190.f))
	{
		if (C->Join(this))
		{
			Toast(C->bBridge
				? FString::Printf(TEXT("Ты звено моста: %d/%d. G или Пробел — отцепиться"), C->NumLinks(), FMath::CeilToInt(C->Span() / AAntChain::LinkLen))
				: FString::Printf(TEXT("Ты звено лестницы (%d). По вам можно лезть. G — отцепиться"), C->NumLinks()));
			return;
		}
	}
	// 2. A friend next to you: pick him up.
	if (AAntCharacter* A = FindGrabbableAlly(InteractRadius))
	{
		GrabAnt(A);
		return;
	}
	// 3. Commander with an army: the workers pour a bridge to where you aim.
	if (AAntArmy* Army = AAntArmy::FindFor(this))
	{
		const float D = (float)FVector::Dist(Aim, GetActorLocation());
		if (D > 250.f && D < AAntChain::LinkLen * AAntChain::MaxLinks)
		{
			Army->BuildBridge(GetActorLocation() - EstimateSurfaceNormal() * BodyRadius, Aim);
			return;
		}
	}
	// 4. Start a chain yourself.
	StartChainHere(Aim);
}

bool AAntCharacter::StartChainHere(const FVector& Aim)
{
	if (!IsCrawling() && GetCharacterMovement()->IsFalling())
	{
		Toast(TEXT("Сцепиться можно, когда держишься за поверхность"));
		return false;
	}
	const FVector N = EstimateSurfaceNormal();
	const FVector Me = GetActorLocation();
	const FVector S = Me - N * BodyRadius;
	const FVector ToAim = Aim - S;
	const float D = (float)ToAim.Size();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AntChainProbe), false, this);

	// Bridge: aim at the far bank across a gap or up/down to another level.
	if (D > 220.f && D < AAntChain::LinkLen * AAntChain::MaxLinks && FVector::DotProduct(ToAim / D, N) < 0.85f)
	{
		bool bGap = FMath::Abs(Aim.Z - S.Z) > 120.f;
		if (!bGap)
		{
			FHitResult Hit;
			const FVector Mid = (S + Aim) * 0.5f;
			bGap = !GetWorld()->LineTraceSingleByObjectType(Hit, Mid + FVector(0.f, 0.f, 20.f), Mid - FVector(0.f, 0.f, 160.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
		}
		if (bGap)
		{
			AAntChain::Start(GetWorld(), Team, this, S, ToAim / D, true, Aim);
			Toast(FString::Printf(TEXT("Ты начал ЖИВОЙ МОСТ: нужно ~%d муравьёв. Свои — к тебе на G (с армией — G построит сразу)"),
				FMath::CeilToInt(D / AAntChain::LinkLen)));
			return true;
		}
	}
	// Ladder from a ceiling.
	if (N.Z < -0.6f)
	{
		AAntChain::Start(GetWorld(), Team, this, S, FVector(0.f, 0.f, -1.f), false, S);
		Toast(TEXT("Висишь с потолка — начало живой лестницы вниз. Свои цепляются снизу (G)"));
		return true;
	}
	// Ladder over the edge of the table / shelf you stand on.
	FVector F = FVector::VectorPlaneProject(Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector(), N).GetSafeNormal();
	if (N.Z > 0.6f && !F.IsNearlyZero())
	{
		for (float Ahead = 50.f; Ahead <= 130.f; Ahead += 40.f)
		{
			const FVector Probe = Me + F * Ahead;
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByObjectType(Hit, Probe + N * 10.f, Probe - N * 350.f, FCollisionObjectQueryParams(ECC_WorldStatic), Q))
			{
				const FVector Anchor = Me + F * (Ahead - 20.f) - N * BodyRadius;
				AAntChain::Start(GetWorld(), Team, this, Anchor, FVector(0.f, 0.f, -1.f), false, Anchor);
				Toast(TEXT("Свесился с края — начало живой лестницы вниз. Свои цепляются снизу (G)"));
				return true;
			}
		}
	}
	Toast(TEXT("Цепочку начинают: с края (лестница вниз), с потолка, или прицелься в другой берег (мост)"));
	return false;
}
