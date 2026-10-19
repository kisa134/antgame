#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "AntSwarm.h"
#include "HumanAIController.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntTypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"

namespace HumanLook
{
	const float HalfHeight = 1700.f;
	const FLinearColor Skin(0.85f, 0.55f, 0.40f);
	const FLinearColor Shirt(0.80f, 0.20f, 0.15f);
	const FLinearColor Jeans(0.12f, 0.18f, 0.40f);
	const FLinearColor Slipper(1.00f, 0.45f, 0.65f);
	const FLinearColor Hair(0.25f, 0.13f, 0.05f);
}
using namespace HumanLook;

AHumanCharacter::AHumanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SetNetCullDistanceSquared(1.0e9f);

	AIControllerClass = AHumanAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(340.f, HalfHeight);
	// Ants walk between the Human's feet instead of getting stuck on him.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->MaxWalkSpeed = 720.f;
	CM->MaxStepHeight = 250.f;
	CM->JumpZVelocity = 0.f;
	CM->NavAgentProps.bCanJump = false;
	CM->bOrientRotationToMovement = false;
	CM->bIgnoreClientMovementErrorChecksAndCorrection = true;
	CM->bServerAcceptClientAuthoritativePosition = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 1400.f));
	SpringArm->TargetArmLength = 2400.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 500.f);
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 90.f;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);
}

void AHumanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHumanCharacter, Health);
	DOREPLIFETIME(AHumanCharacter, SprayFuel);
	DOREPLIFETIME(AHumanCharacter, bSpraying);
	DOREPLIFETIME(AHumanCharacter, bKnockedOut);
	DOREPLIFETIME(AHumanCharacter, Rage);
	DOREPLIFETIME(AHumanCharacter, Held);
	DOREPLIFETIME(AHumanCharacter, SwatReadyTime);
	DOREPLIFETIME(AHumanCharacter, StompReadyTime);
}

void AHumanCharacter::BeginPlay()
{
	Super::BeginPlay();
	BuildBody();
}

void AHumanCharacter::BuildBody()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USceneComponent* R = VisualRoot;

	// Legs + fluffy slippers. Pivots sit at the hips so the whole leg swings.
	for (int32 s = -1; s <= 1; s += 2)
	{
		USceneComponent* Hip = AntKit::AddPivot(this, R, FVector(0.f, 170.f * s, -150.f));
		AntKit::AddPart(this, Hip, EAntShape::Cylinder, FVector(0.f, 0.f, -700.f), FVector(2.8f, 2.8f, 14.f), Jeans);
		AntKit::AddPart(this, Hip, EAntShape::Cube, FVector(110.f, 0.f, -1490.f), FVector(4.8f, 2.4f, 1.2f), Slipper);
		AntKit::AddPart(this, Hip, EAntShape::Sphere, FVector(320.f, 0.f, -1430.f), FVector(1.1f), FLinearColor::White);
		LegPivots.Add(Hip);
	}

	// Torso, belly, neck, head.
	AntKit::AddPart(this, R, EAntShape::Cube, FVector(0.f, 0.f, 450.f), FVector(4.2f, 7.6f, 12.f), Shirt);
	AntKit::AddPart(this, R, EAntShape::Sphere, FVector(130.f, 0.f, 330.f), FVector(4.f, 6.f, 7.f), Shirt);
	AntKit::AddPart(this, R, EAntShape::Cube, FVector(0.f, 0.f, -120.f), FVector(4.4f, 7.8f, 1.2f), FLinearColor(0.1f, 0.06f, 0.03f)); // belt
	AntKit::AddPart(this, R, EAntShape::Cylinder, FVector(0.f, 0.f, 1110.f), FVector(1.6f, 1.6f, 1.4f), Skin);
	AntKit::AddPart(this, R, EAntShape::Sphere, FVector(0.f, 0.f, 1400.f), FVector(4.8f), Skin);
	AntKit::AddPart(this, R, EAntShape::Sphere, FVector(-40.f, 0.f, 1490.f), FVector(4.9f, 4.9f, 3.5f), Hair);
	for (int32 s = -1; s <= 1; s += 2)
	{
		AntKit::AddPart(this, R, EAntShape::Sphere, FVector(200.f, 80.f * s, 1440.f), FVector(0.6f), FLinearColor::White);
		AntKit::AddPart(this, R, EAntShape::Sphere, FVector(228.f, 80.f * s, 1440.f), FVector(0.3f), FLinearColor(0.02f, 0.02f, 0.02f));
		AntKit::AddPart(this, R, EAntShape::Cube, FVector(215.f, 80.f * s, 1510.f), FVector(0.2f, 1.f, 0.15f), Hair, FRotator(0.f, 0.f, 10.f * s));
	}
	AntKit::AddPart(this, R, EAntShape::Sphere, FVector(240.f, 0.f, 1390.f), FVector(0.7f), Skin);
	AntKit::AddPart(this, R, EAntShape::Cube, FVector(220.f, 0.f, 1300.f), FVector(0.2f, 1.2f, 0.15f), FLinearColor(0.4f, 0.05f, 0.05f));

	// Arms. The right hand holds the bug spray can.
	for (int32 s = -1; s <= 1; s += 2)
	{
		USceneComponent* Shoulder = AntKit::AddPivot(this, R, FVector(0.f, 470.f * s, 1000.f));
		AntKit::AddPart(this, Shoulder, EAntShape::Cylinder, FVector(0.f, 0.f, -550.f), FVector(2.2f, 2.2f, 11.f), Shirt);
		AntKit::AddPart(this, Shoulder, EAntShape::Sphere, FVector(0.f, 0.f, -1120.f), FVector(2.2f), Skin);
		if (s > 0)
		{
			AntKit::AddPart(this, Shoulder, EAntShape::Cylinder, FVector(0.f, 0.f, -1230.f), FVector(1.6f, 1.6f, 3.2f), FLinearColor(0.9f, 0.85f, 0.1f));
			AntKit::AddPart(this, Shoulder, EAntShape::Cylinder, FVector(0.f, 0.f, -1400.f), FVector(0.5f, 0.5f, 0.6f), FLinearColor(0.1f, 0.5f, 0.1f));
		}
		ArmPivots.Add(Shoulder);
	}

	// Generated model: replaces the shapes (they stay as a fallback when the asset is missing).
	const AntArt::FPart& Dad = AntArt::Get(TEXT("SM_Human_Dad"));
	if (Dad.IsValid())
	{
		TArray<USceneComponent*> Old;
		R->GetChildrenComponents(true, Old);
		for (USceneComponent* C : Old)
		{
			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(C))
			{
				Prim->SetVisibility(false);
			}
		}
		ModelPivot = AntKit::AddPivot(this, R, FVector(0.f, 0.f, -HalfHeight));   // feet
		UStaticMeshComponent* Body = AntArt::AddArt(this, ModelPivot, Dad);
		const FVector E = Dad.Bounds.GetSize();
		const float S = 3350.f / FMath::Max(1.f, (float)E.Z);
		// Shoulders are the wide side: they go across (Y).
		const float Yaw = (E.X > E.Y ? 90.f : 0.f) + ModelYaw;
		const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
		const FVector C = Dad.Bounds.GetCenter();
		const FVector FeetCenter(C.X, C.Y, Dad.Bounds.Min.Z);
		Body->SetRelativeTransform(FTransform(Q, -Q.RotateVector(FeetCenter * S), FVector(S)));

		const AntArt::FPart& Slip = AntArt::Get(TEXT("SM_Slipper"));
		if (Slip.IsValid())
		{
			SlipperArt = AntArt::AddArt(this, R, Slip);
			SlipperFit = AntArt::Fit(Slip, FVector::ZeroVector, 950.f, false);
			SlipperArt->SetVisibility(false);
			SlipperArt->SetCastShadow(true);
		}
		const AntArt::FPart& Can = AntArt::Get(TEXT("SM_BugSpray"));
		if (Can.IsValid())
		{
			SprayArt = AntArt::AddArt(this, R, Can);
			// Upright can in front of the chest, nozzle where the spray comes out.
			FTransform T = AntArt::Fit(Can, FVector(1000.f, 470.f, 640.f), 380.f, false, FRotator(90.f, 0.f, 0.f));
			SprayArt->SetRelativeTransform(T);
			SprayArt->SetVisibility(false);
		}
		const AntArt::FPart& Paper = AntArt::Get(TEXT("SM_RolledNewspaper"));
		if (Paper.IsValid())
		{
			PaperArt = AntArt::AddArt(this, R, Paper);
			PaperArt->SetUsingAbsoluteLocation(true);
			PaperArt->SetUsingAbsoluteRotation(true);
			PaperArt->SetUsingAbsoluteScale(true);
			PaperArt->SetVisibility(false);
		}
	}

	// Spray cloud puffs (only visible while spraying).
	for (int32 i = 0; i < 16; ++i)
	{
		UStaticMeshComponent* Puff = AntKit::AddPart(this, R, EAntShape::Sphere, FVector::ZeroVector, FVector(0.5f), FLinearColor(0.55f, 0.9f, 0.35f));
		Puff->SetCastShadow(false);
		Puff->SetUsingAbsoluteLocation(true);
		Puff->SetVisibility(false);
		Puffs.Add(Puff);
	}
}

// ---------------------------------------------------------------------------------------------
// Input (only when a player controls the Human)
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxis("MoveForward", this, &AHumanCharacter::MoveForward);
	Input->BindAxis("MoveRight", this, &AHumanCharacter::MoveRight);
	Input->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	Input->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	Input->BindAxis("TurnRate", this, &AHumanCharacter::TurnRate);
	Input->BindAxis("LookUpRate", this, &AHumanCharacter::LookUpRate);
	Input->BindAction("Fire", IE_Pressed, this, &AHumanCharacter::OnStompPressed);
	Input->BindAction("AltFire", IE_Pressed, this, &AHumanCharacter::OnSprayPressed);
	Input->BindAction("AltFire", IE_Released, this, &AHumanCharacter::OnSprayReleased);
	Input->BindAction("Interact", IE_Pressed, this, &AHumanCharacter::OnGrabPressed);
	Input->BindKey(EKeys::Q, IE_Pressed, this, &AHumanCharacter::OnSwatPressed);
}

bool AHumanCharacter::AimPoint(FVector& OutPoint, FVector& OutDir) const
{
	if (!Controller)
	{
		return false;
	}
	FVector CamLoc;
	FRotator CamRot;
	Controller->GetPlayerViewPoint(CamLoc, CamRot);
	OutDir = CamRot.Vector();
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(HumanAim), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + OutDir * 9000.f, ECC_Visibility, P))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}
	OutPoint = CamLoc + OutDir * 2000.f;
	return true;
}

void AHumanCharacter::OnSwatPressed()
{
	FVector P, D;
	if (AimPoint(P, D)) { ServerSwat(P); }
}

void AHumanCharacter::OnGrabPressed()
{
	FVector P, D;
	if (AimPoint(P, D)) { ServerGrabOrThrow(P, D); }
}

void AHumanCharacter::ServerSwat_Implementation(FVector_NetQuantize Point) { TrySwat(Point); }
void AHumanCharacter::ServerGrabOrThrow_Implementation(FVector_NetQuantize Point, FVector_NetQuantizeNormal Dir) { GrabOrThrow(Point, Dir); }

void AHumanCharacter::TrySwat(const FVector& InPoint)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || bKnockedOut || Now < SwatReadyTime)
	{
		return;
	}
	// Out of reach: slap as far as the arm goes.
	FVector Point = InPoint;
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 600.f);
	if (FVector::Dist(Point, Chest) > SwatRange)
	{
		Point = Chest + (Point - Chest).GetSafeNormal() * SwatRange;
	}
	SwatReadyTime = Now + 2.6f * (1.f - 0.15f * Rage);
	MulticastSwat(Point);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Point]()
	{
		AntSfx::Everyone(this, AntSfx::Stomp, Point, 1.5f);
		if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
		{
			Swarm->KillInSphere(Point, SwatRadius);
		}
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* Ant = *It;
			if (Ant && !Ant->bDead && !Ant->IsInNest() && FVector::Dist(Ant->GetActorLocation(), Point) < SwatRadius + AAntCharacter::BodyRadius)
			{
				UGameplayStatics::ApplyDamage(Ant, 150.f, GetController(), this, UDamageType::StaticClass());
			}
		}
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Point;
			if (To.Size() < SwatRadius * 1.6f)
			{
				It->Kick(To.GetSafeNormal() * 500.f + FVector(0.f, 0.f, 300.f), It->GetActorLocation());
			}
		}
	}), 0.28f, false);
}

void AHumanCharacter::GrabOrThrow(const FVector& Point, const FVector& Dir)
{
	if (!HasAuthority() || bKnockedOut)
	{
		return;
	}
	if (Held)
	{
		// Throw it: food smashes when it lands hard (and bowls ants over).
		AKitchenLoot* L = Held;
		Held = nullptr;
		L->Throw(Dir.GetSafeNormal() * 2600.f + FVector(0.f, 0.f, 300.f));
		return;
	}
	AKitchenLoot* Best = nullptr;
	float BestD = 700.f;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L || L->bIsGear || L->bHeld || FVector::Dist2D(L->GetActorLocation(), GetActorLocation()) > 2600.f)
		{
			continue;
		}
		const float D = (float)FVector::Dist(L->GetActorLocation(), Point);
		if (D < BestD)
		{
			BestD = D;
			Best = L;
		}
	}
	if (Best)
	{
		Held = Best;
		Best->PickUpBy(VisualRoot, FVector(700.f, 420.f, 500.f));
	}
}

void AHumanCharacter::MulticastSwat_Implementation(FVector_NetQuantize Point)
{
	SwatPoint = Point;
	SwatLocalStart = GetWorld()->GetTimeSeconds();
}

void AHumanCharacter::MoveForward(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
	}
}

void AHumanCharacter::MoveRight(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
	}
}

void AHumanCharacter::TurnRate(float Value) { AddControllerYawInput(Value * 60.f * GetWorld()->GetDeltaSeconds()); }
void AHumanCharacter::LookUpRate(float Value) { AddControllerPitchInput(Value * 45.f * GetWorld()->GetDeltaSeconds()); }

void AHumanCharacter::OnStompPressed() { ServerStomp(); }
void AHumanCharacter::OnSprayPressed() { ServerSetSpray(true); }
void AHumanCharacter::OnSprayReleased() { ServerSetSpray(false); }

void AHumanCharacter::ServerStomp_Implementation() { TryStomp(-1); }
void AHumanCharacter::ServerSetSpray_Implementation(bool bOn) { SetSpraying(bOn); }

// ---------------------------------------------------------------------------------------------
// Attacks (server)
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::TryStomp(int32 Leg)
{
	if (!HasAuthority() || bKnockedOut || bStompPending)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < StompReadyTime)
	{
		return;
	}
	PendingLeg = (uint8)(Leg < 0 ? NextLeg : FMath::Clamp(Leg, 0, 1));
	NextLeg = 1 - PendingLeg;
	bStompPending = true;
	StompImpactTime = Now + 0.55f;
	StompReadyTime = Now + 1.05f * (1.f - 0.18f * Rage);
	MulticastStomp(PendingLeg);
}

void AHumanCharacter::MulticastStomp_Implementation(uint8 Leg)
{
	LocalStompStart = GetWorld()->GetTimeSeconds();
	LocalStompLeg = Leg;
}

FVector AHumanCharacter::GetStompPoint(int32 Leg) const
{
	return GetActorLocation() + GetActorForwardVector() * 480.f + GetActorRightVector() * (Leg == 0 ? -180.f : 180.f) - FVector(0.f, 0.f, HalfHeight);
}

void AHumanCharacter::DoStompImpact()
{
	const FVector P = GetStompPoint(PendingLeg);
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Swarm->KillInSphere(P, StompRadius * 1.15f);
	}
	// The floor shakes: food nearby jumps (and carriers often lose it).
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		const FVector To = It->GetActorLocation() - P;
		if (To.Size2D() < StompRadius * 2.f && FMath::Abs(To.Z) < 300.f)
		{
			const float K = 1.f - (float)To.Size2D() / (StompRadius * 2.f);
			It->Kick(To.GetSafeNormal2D() * 350.f * K + FVector(0.f, 0.f, 650.f * K), It->GetActorLocation() + FMath::VRand() * 10.f);
		}
	}
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest())
		{
			continue;
		}
		const FVector A = Ant->GetActorLocation();
		if (FVector::Dist2D(A, P) < StompRadius && A.Z - P.Z < 220.f)
		{
			UGameplayStatics::ApplyDamage(Ant, 200.f, GetController(), this, UDamageType::StaticClass());
		}
	}
}

void AHumanCharacter::SetSpraying(bool bOn)
{
	if (HasAuthority())
	{
		bSpraying = bOn && !bKnockedOut && SprayFuel > 5.f;
	}
}

FVector AHumanCharacter::GetSprayOrigin() const
{
	return GetActorTransform().TransformPosition(FVector(1080.f, 470.f, 810.f));
}

FVector AHumanCharacter::GetSprayDirection() const
{
	FRotator R = GetController() ? GetController()->GetControlRotation() : GetBaseAimRotation();
	R.Pitch = FMath::ClampAngle((float)R.Pitch, -70.f, 15.f);
	R.Yaw = GetActorRotation().Yaw;
	R.Roll = 0.f;
	return R.Vector();
}

void AHumanCharacter::DoSprayDamage(float Dt)
{
	const FVector Origin = GetSprayOrigin();
	const FVector Dir = GetSprayDirection();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(20.f));
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Swarm->KillInCone(Origin, Dir, GetSprayRange(), CosCone, Dt);
	}

	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest())
		{
			continue;
		}
		const FVector To = Ant->GetActorLocation() - Origin;
		const float Dist = To.Size();
		if (Dist > GetSprayRange() || Dist < 1.f || FVector::DotProduct(To / Dist, Dir) < CosCone)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanSpray), false, this);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Origin, Ant->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			continue;  // hiding behind something
		}
		UGameplayStatics::ApplyDamage(Ant, 60.f * (1.f + 0.3f * Rage) * Dt, GetController(), this, UDamageType::StaticClass());
	}
}

float AHumanCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bKnockedOut)
	{
		return 0.f;
	}
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Actual <= 0.f)
	{
		return 0.f;
	}
	Health = FMath::Max(0.f, Health - Actual);
	LastHurtTime = GetWorld()->GetTimeSeconds();
	if (Health <= 0.f)
	{
		KnockOut(EventInstigator);
	}
	return Actual;
}

void AHumanCharacter::KnockOut(AController* By)
{
	bKnockedOut = true;
	bSpraying = false;
	bStompPending = false;
	KnockoutEnd = GetWorld()->GetTimeSeconds() + 12.f;
	OnRep_KnockedOut();
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->OnHumanKnockedOut(By);
	}
}

void AHumanCharacter::OnRep_KnockedOut()
{
	GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
}

void AHumanCharacter::OnRep_Rage()
{
	GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed();
}

void AHumanCharacter::SetRage(uint8 NewRage)
{
	Rage = FMath::Min<uint8>(NewRage, 3);
	OnRep_Rage();
}

void AHumanCharacter::ResetHuman(const FVector& Location)
{
	Health = MaxHealth;
	SprayFuel = 100.f;
	bSpraying = false;
	bStompPending = false;
	bKnockedOut = false;
	OnRep_KnockedOut();
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
}

FString AHumanCharacter::GetDisplayName() const
{
	if (const APlayerState* PS = GetPlayerState())
	{
		return FString::Printf(TEXT("Человек (%s)"), *PS->GetPlayerName());
	}
	return TEXT("Человек (ИИ)");
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AHumanCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();

	if (HasAuthority())
	{
		if (bStompPending && Now >= StompImpactTime)
		{
			bStompPending = false;
			DoStompImpact();
			AntSfx::Everyone(this, AntSfx::Stomp, GetStompPoint(PendingLeg), FMath::FRandRange(0.9f, 1.1f));
		}

		if (bSpraying && (SprayFuel <= 0.f || bKnockedOut))
		{
			bSpraying = false;
		}
		if (bSpraying)
		{
			SprayFuel = FMath::Max(0.f, SprayFuel - 22.f * DeltaSeconds);
			SprayAccum += DeltaSeconds;
			if (SprayAccum >= 0.2f)
			{
				DoSprayDamage(SprayAccum);
				SprayAccum = 0.f;
			}
		}
		else
		{
			SprayFuel = FMath::Min(100.f, SprayFuel + 9.f * DeltaSeconds);
		}

		if (bKnockedOut && Now >= KnockoutEnd)
		{
			bKnockedOut = false;
			Health = MaxHealth;
			OnRep_KnockedOut();
		}
		else if (!bKnockedOut && Now - LastHurtTime > 6.f)
		{
			Health = FMath::Min(MaxHealth, Health + 25.f * DeltaSeconds);
		}
	}

	UpdateVisuals(DeltaSeconds);
}

void AHumanCharacter::UpdateVisuals(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || LegPivots.Num() < 2 || ArmPivots.Num() < 2)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	WalkPhase += DeltaSeconds * Speed / 140.f;
	const float Swing = FMath::Sin(WalkPhase) * 18.f * FMath::Min(1.f, Speed / 300.f);

	float LegPitch[2] = { Swing, -Swing };
	const float T = Now - LocalStompStart;
	if (T >= 0.f && T < 1.1f)
	{
		float P;
		if (T < 0.55f) { P = 55.f * (T / 0.55f); }
		else if (T < 0.68f) { P = FMath::Lerp(55.f, 12.f, (T - 0.55f) / 0.13f); }
		else { P = FMath::Lerp(12.f, 0.f, (T - 0.68f) / 0.42f); }
		LegPitch[LocalStompLeg % 2] = P;
	}
	LegPivots[0]->SetRelativeRotation(FRotator(LegPitch[0], 0.f, 0.f));
	LegPivots[1]->SetRelativeRotation(FRotator(LegPitch[1], 0.f, 0.f));

	ArmPivots[0]->SetRelativeRotation(FRotator(-Swing * 0.8f, 0.f, 0.f));
	ArmPivots[1]->SetRelativeRotation(FRotator(bSpraying ? 80.f : Swing * 0.8f, 0.f, 0.f));

	VisualRoot->SetRelativeRotation(FRotator(0.f, 0.f, bKnockedOut ? FMath::Sin(Now * 2.5f) * 10.f : 0.f));

	// Newspaper slap: swings down onto the target point.
	if (PaperArt)
	{
		const float ST = Now - SwatLocalStart;
		if (ST >= 0.f && ST < 0.8f)
		{
			const AntArt::FPart& Paper = AntArt::Get(TEXT("SM_RolledNewspaper"));
			const FVector Shoulder = GetActorLocation() + GetActorRightVector() * 470.f + FVector(0.f, 0.f, 900.f);
			const float K = ST < 0.28f ? FMath::Square(ST / 0.28f) : 1.f;
			const FVector Up = Shoulder + FVector(0.f, 0.f, 900.f);
			const FVector Tip = FMath::Lerp(Up, SwatPoint, K);
			const FVector Side = GetActorRightVector() * 120.f;
			FTransform TM = AntArt::Fit(Paper, (Tip + Side), 1500.f, false);
			const FVector Axis = (Tip - Shoulder).GetSafeNormal();
			TM.SetRotation(FRotationMatrix::MakeFromX(Axis).ToQuat() * AntArt::AxisFix(Paper, false));
			TM.SetLocation(Tip - Axis * 750.f + Side);
			PaperArt->SetWorldTransform(TM);
			PaperArt->SetVisibility(true);
		}
		else
		{
			PaperArt->SetVisibility(false);
		}
	}

	if (ModelPivot)
	{
		// Static model: waddle while walking, lean into stomps.
		const float Walk = FMath::Min(1.f, Speed / 300.f);
		float Lean = 0.f, Lift = FMath::Abs(FMath::Sin(WalkPhase)) * 35.f * Walk;
		if (T >= 0.f && T < 1.1f)
		{
			Lean = T < 0.45f ? -6.f * (T / 0.45f) : T < 0.6f ? FMath::Lerp(-6.f, 9.f, (T - 0.45f) / 0.15f) : FMath::Lerp(9.f, 0.f, (T - 0.6f) / 0.5f);
		}
		ModelPivot->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight + Lift));
		ModelPivot->SetRelativeRotation(FRotator(-Lean, 0.f, FMath::Sin(WalkPhase) * 3.f * Walk));
		if (SprayArt)
		{
			SprayArt->SetVisibility(bSpraying);
		}
		if (SlipperArt)
		{
			// A giant slipper comes down where the stomp lands.
			if (T >= 0.f && T < 1.1f)
			{
				float H;
				if (T < 0.4f) { H = FMath::Lerp(400.f, 1500.f, T / 0.4f); }
				else if (T < 0.55f) { H = FMath::Lerp(1500.f, 0.f, FMath::Square((T - 0.4f) / 0.15f)); }
				else if (T < 0.85f) { H = 0.f; }
				else { H = FMath::Lerp(0.f, 900.f, (T - 0.85f) / 0.25f); }
				const FVector Foot(480.f, LocalStompLeg == 0 ? -180.f : 180.f, -HalfHeight + 70.f + H);
				FTransform TM = SlipperFit;
				TM.AddToTranslation(Foot);
				SlipperArt->SetRelativeTransform(TM);
				SlipperArt->SetVisibility(true);
			}
			else
			{
				SlipperArt->SetVisibility(false);
			}
		}
	}

	// Hiss while spraying.
	if (bSpraying && !SprayAudio)
	{
		if (USoundBase* Hiss = AntSfx::Get(AntSfx::Spray))
		{
			SprayAudio = UGameplayStatics::SpawnSoundAttached(Hiss, VisualRoot, NAME_None, FVector(1000.f, 470.f, 700.f), EAttachLocation::KeepRelativeOffset, false, 1.2f);
		}
	}
	if (SprayAudio)
	{
		if (bSpraying && !SprayAudio->IsPlaying()) { SprayAudio->Play(); }
		if (!bSpraying && SprayAudio->IsPlaying()) { SprayAudio->Stop(); }
	}

	const FVector Origin = GetSprayOrigin();
	const FVector Dir = GetSprayDirection();
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Side, Dir);
	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		UStaticMeshComponent* Puff = Puffs[i];
		if (!bSpraying)
		{
			Puff->SetVisibility(false);
			continue;
		}
		const float Frac = FMath::Fmod(Now * 1.6f + i / (float)Puffs.Num(), 1.f);
		const float Ang = i * 2.399f;
		const float Spread = Frac * GetSprayRange() * 0.33f;
		const FVector Pos = Origin + Dir * Frac * GetSprayRange() + (Side * FMath::Cos(Ang) + Up * FMath::Sin(Ang)) * Spread * 0.6f;
		Puff->SetVisibility(true);
		Puff->SetWorldLocation(Pos);
		Puff->SetWorldScale3D(FVector(0.4f + Frac * 4.f));
	}
}
