#include "AntCharacter.h"
#include "AcidProjectile.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntKitchenGameMode.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AAntCharacter::AAntCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SetNetCullDistanceSquared(1.0e9f);

	GetCapsuleComponent()->InitCapsuleSize(26.f, 26.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->MaxWalkSpeed = 600.f;
	CM->JumpZVelocity = 620.f;
	CM->AirControl = 0.7f;
	CM->GravityScale = 1.4f;
	CM->MaxFlySpeed = 420.f;
	CM->BrakingDecelerationFlying = 6000.f;
	CM->MaxStepHeight = 20.f;
	CM->SetWalkableFloorAngle(55.f);
	CM->bOrientRotationToMovement = false;
	CM->NavAgentProps.bCanFly = true;
	// Casual co-op: trust the client's position so wall-climbing never rubber-bands.
	CM->bIgnoreClientMovementErrorChecksAndCorrection = true;
	CM->bServerAcceptClientAuthoritativePosition = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 230.f;
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
	DOREPLIFETIME(AAntCharacter, CarriedValue);
	DOREPLIFETIME(AAntCharacter, CarriedWeight);
	DOREPLIFETIME(AAntCharacter, CarriedItems);
	DOREPLIFETIME(AAntCharacter, bDead);
	DOREPLIFETIME(AAntCharacter, Weapon);
	DOREPLIFETIME(AAntCharacter, BodyColor);
	DOREPLIFETIME(AAntCharacter, bSprinting);
}

void AAntCharacter::BeginPlay()
{
	Super::BeginPlay();
	BuildBody();
	OnRep_Carry();
	OnRep_Weapon();
}

void AAntCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AAntPlayerState* PS = GetPlayerState<AAntPlayerState>())
	{
		BodyColor = PS->AntColor;
		OnRep_BodyColor();
	}
}

// ---------------------------------------------------------------------------------------------
// Body made of spheres and cylinders
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

	auto AddBody = [this, &Body](EAntShape S, const FVector& L, const FVector& Sc, const FRotator& R)
	{
		UStaticMeshComponent* C = AntKit::AddPart(this, VisualRoot, S, L, Sc, Body, R);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)))
		{
			BodyMIDs.Add(M);
		}
		return C;
	};

	AddBody(EAntShape::Sphere, FVector(-30.f, 0.f, -4.f), FVector(0.48f, 0.34f, 0.30f), FRotator(8.f, 0.f, 0.f)); // abdomen
	AddBody(EAntShape::Sphere, FVector(-8.f, 0.f, -6.f), FVector(0.12f), FRotator::ZeroRotator);                  // petiole
	AddBody(EAntShape::Sphere, FVector(4.f, 0.f, -4.f), FVector(0.32f, 0.20f, 0.20f), FRotator::ZeroRotator);    // thorax
	AddBody(EAntShape::Sphere, FVector(26.f, 0.f, 0.f), FVector(0.30f, 0.26f, 0.24f), FRotator::ZeroRotator);    // head

	for (int32 s = -1; s <= 1; s += 2)
	{
		AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(34.f, 9.f * s, 4.f), FVector(0.08f), Eye);
		AddBody(EAntShape::Cube, FVector(42.f, 5.f * s, -5.f), FVector(0.13f, 0.03f, 0.04f), FRotator(0.f, -25.f * s, 0.f));       // mandible
		AddBody(EAntShape::Cylinder, FVector(36.f, 8.f * s, 18.f), FVector(0.025f, 0.025f, 0.32f), FRotator(-35.f, 0.f, -25.f * s)); // antenna
	}

	const float LegX[3] = { 12.f, 0.f, -12.f };
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			USceneComponent* P = AntKit::AddPivot(this, VisualRoot, FVector(LegX[i], 8.f * s, -8.f));
			UStaticMeshComponent* L = AntKit::AddPart(this, P, EAntShape::Cylinder, FVector(0.f, 14.f * s, -9.f), FVector(0.05f, 0.05f, 0.36f), Leg, FRotator(0.f, 0.f, 60.f * s));
			if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(L->GetMaterial(0)))
			{
				LegMIDs.Add(M);
			}
			LegPivots.Add(P);
		}
	}

	// Acid gland at the tip of the abdomen, glows in the weapon's rarity color.
	UStaticMeshComponent* Gland = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-55.f, 0.f, -2.f), FVector(0.13f), AntKit::RarityColor(Weapon.Rarity));
	GlandMID = Cast<UMaterialInstanceDynamic>(Gland->GetMaterial(0));

	// Food chunk carried on the back.
	CarryChunk = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-26.f, 0.f, 24.f), FVector(0.2f), FLinearColor(1.f, 0.75f, 0.2f));
	CarryChunk->SetVisibility(false);
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
}

void AAntCharacter::OnRep_Weapon()
{
	if (GlandMID)
	{
		GlandMID->SetVectorParameterValue(TEXT("Color"), AntKit::RarityColor(Weapon.Rarity));
	}
}

void AAntCharacter::OnRep_Carry()
{
	if (CarryChunk)
	{
		CarryChunk->SetVisibility(CarriedWeight > 0);
		CarryChunk->SetRelativeScale3D(FVector(0.15f + 0.035f * CarriedWeight));
	}
	UpdateSpeed();
}

void AAntCharacter::OnRep_Dead()
{
	if (bDead)
	{
		bWantsFire = false;
		bClimbing = false;
		VisualRoot->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.15f));   // squashed flat
		VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -22.f));
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
}

void AAntCharacter::MoveForward(float Value)
{
	ForwardInput = Value;
	if (!Controller || bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}
	if (bClimbing)
	{
		// W while facing a wall = crawl up it.
		AddMovementInput(FVector::UpVector, Value);
		AddMovementInput(GetActorForwardVector(), 0.2f);
		return;
	}
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
}

void AAntCharacter::MoveRight(float Value)
{
	if (!Controller || bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
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
	if (bDead)
	{
		return;
	}
	if (bClimbing)
	{
		// Kick off the wall.
		bClimbing = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		LaunchCharacter(-GetActorForwardVector() * 420.f + FVector::UpVector * 380.f, true, true);
		return;
	}
	Jump();
}

void AAntCharacter::DoStopJump()
{
	StopJumping();
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
			UpdateClimb();
			TickFire();
		}

		if (HasAuthority())
		{
			if (CarriedWeight > 0 && IsInNest())
			{
				if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
				{
					GM->OnLootDelivered(this, CarriedValue, CarriedItems);
				}
				CarriedValue = 0;
				CarriedWeight = 0;
				CarriedItems = 0;
				OnRep_Carry();
			}

			if (GetWorld()->GetTimeSeconds() - LastDamageTime > 5.f && Health < MaxHealth)
			{
				Health = FMath::Min(MaxHealth, Health + 6.f * DeltaSeconds);
			}

			if (GetActorLocation().Z < -800.f)
			{
				Die(nullptr, nullptr);
			}
		}
	}

	UpdateSpeed();
	UpdateVisuals(DeltaSeconds);
}

void AAntCharacter::UpdateClimb()
{
	UCharacterMovementComponent* CM = GetCharacterMovement();
	const FVector Fwd = GetActorForwardVector();
	const FVector Start = GetActorLocation();
	const FVector End = Start + Fwd * (GetCapsuleComponent()->GetScaledCapsuleRadius() + 30.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntClimb), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	const bool bWall = bHit && FMath::Abs(Hit.ImpactNormal.Z) < 0.6f;
	const bool bWants = ForwardInput > 0.1f;

	if (bWall && bWants)
	{
		if (!bClimbing)
		{
			bClimbing = true;
			CM->SetMovementMode(MOVE_Flying);
		}
	}
	else if (bClimbing)
	{
		bClimbing = false;
		if (CM->MovementMode == MOVE_Flying)
		{
			CM->SetMovementMode(MOVE_Falling);
		}
		if (!bWall && bWants)
		{
			// Reached the top edge: hop over it.
			LaunchCharacter(Fwd * 260.f + FVector::UpVector * 320.f, true, true);
		}
	}
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
	NextFireTime = Now + 1.f / FMath::Max(0.2f, Weapon.FireRate);

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

	const int32 Pellets = FMath::Max<int32>(1, Weapon.Pellets);
	for (int32 i = 0; i < Pellets; ++i)
	{
		const FVector D = FMath::VRandCone(FVector(Direction), FMath::DegreesToRadians(Weapon.Spread));
		const FTransform SpawnTM(D.Rotation(), Start);
		AAcidProjectile* P = GetWorld()->SpawnActorDeferred<AAcidProjectile>(AAcidProjectile::StaticClass(), SpawnTM, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (P)
		{
			P->Setup(Weapon, D);
			P->FinishSpawning(SpawnTM);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Looting
// ---------------------------------------------------------------------------------------------

AKitchenLoot* AAntCharacter::FindNearestLoot(float Radius) const
{
	AKitchenLoot* Best = nullptr;
	float BestDist = Radius;
	const FVector Me = GetActorLocation();
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Me);
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

void AAntCharacter::ServerInteract_Implementation()
{
	if (bDead)
	{
		return;
	}
	AKitchenLoot* Loot = FindNearestLoot(InteractRadius + 30.f);
	if (!Loot)
	{
		return;
	}

	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();

	if (Loot->bIsGear)
	{
		const FAntWeapon Old = Weapon;
		Weapon = Loot->Gear;
		OnRep_Weapon();
		const FVector DropAt = Loot->GetActorLocation();
		Loot->Destroy();
		if (GM)
		{
			GM->SpawnGear(DropAt, Old);
		}
		Toast(FString::Printf(TEXT("Экипировано: %s"), *Weapon.Name));
		return;
	}

	if (CarriedWeight + Loot->Weight > MaxCarry)
	{
		Toast(TEXT("Слишком тяжело! Сначала оттащи груз в муравейник"));
		return;
	}

	CarriedWeight += Loot->Weight;
	CarriedValue += Loot->Value;
	CarriedItems += 1;
	OnRep_Carry();
	Toast(FString::Printf(TEXT("+ %s (%d)"), *Loot->LootName, Loot->Value));
	Loot->Destroy();
}

void AAntCharacter::DropEverything()
{
	if (!HasAuthority() || CarriedWeight <= 0)
	{
		return;
	}
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->SpawnStash(GetGroundLocation(), CarriedValue, CarriedWeight, CarriedItems);
	}
	CarriedValue = 0;
	CarriedWeight = 0;
	CarriedItems = 0;
	OnRep_Carry();
}

// ---------------------------------------------------------------------------------------------
// Damage and death
// ---------------------------------------------------------------------------------------------

float AAntCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bDead || IsInNest())
	{
		return 0.f;   // the nest is a safe zone
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
	bDead = true;
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
	const FVector L = GetActorLocation();
	return FVector::Dist2D(L, AKitchenBuilder::NestLocation()) < AKitchenBuilder::NestRadius() && L.Z < 400.f;
}

FVector AAntCharacter::GetMuzzleLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * 45.f + FVector(0.f, 0.f, 8.f);
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
	return TEXT("Муравей");
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
	UCharacterMovementComponent* CM = GetCharacterMovement();
	if (!CM)
	{
		return;
	}
	const float Load = FMath::Max(0.55f, 1.f - 0.035f * CarriedWeight);
	CM->MaxWalkSpeed = (bSprinting ? 950.f : 600.f) * Load;
	CM->MaxFlySpeed = 420.f * Load;
}

void AAntCharacter::UpdateVisuals(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || bDead || LegPivots.Num() == 0)
	{
		return;
	}
	const float Speed = GetVelocity().Size();
	WalkPhase += DeltaSeconds * (Speed / 30.f + (bClimbing ? 8.f : 0.f));
	const float Amp = 22.f * FMath::Min(1.f, Speed / 200.f);
	for (int32 i = 0; i < LegPivots.Num(); ++i)
	{
		const int32 Pair = i / 2;
		const int32 Side = i % 2;
		const float Offset = ((Pair + Side) % 2) ? PI : 0.f;   // tripod gait
		LegPivots[i]->SetRelativeRotation(FRotator(0.f, FMath::Sin(WalkPhase + Offset) * Amp, 0.f));
	}
}
