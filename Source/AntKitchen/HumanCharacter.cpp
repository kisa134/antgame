#include "HumanCharacter.h"
#include "AntWeapon.h"
#include "AntVersus.h"
#include "AntFx.h"
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
#include "AntMovementComponent.h"
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
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "AntRun.h"
#include "EngineUtils.h"
#include "KitchenSlick.h"
#include "KitchenInteractables.h"
#include "Critters.h"
#include "AntFilm.h"
#include "GameFramework/GameStateBase.h"
#include "LevelV3.h"
#include "NpcRoutine.h"

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

	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(VisualRoot);
	BodyPivot->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));   // feet
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(BodyPivot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);
	BodyMesh->SetCastShadow(true);

	// The jointed body: one component per segment, made here so that each has the same name on every machine
	// (an ant standing on a shin is replicated relative to that shin). Meshes and collision come in SetupJointedBody.
	for (int32 i = 0; i < HumanSeg::Count; ++i)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(FName(*FString::Printf(TEXT("Seg_%s"), HumanBody::SegName(i))));
		C->SetupAttachment(BodyPivot);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetGenerateOverlapEvents(false);
		Segs.Add(C);
	}
}

void AHumanCharacter::InitFigure(uint8 InFig)
{
	Fig = InFig;
	const FHumanStyle& St = HumanBody::Style(Fig);
	FigScale = St.Scale;
	GetCapsuleComponent()->InitCapsuleSize(St.CapsuleR, St.CapsuleH);
	BodyPivot->SetRelativeLocation(FVector(0.f, 0.f, -St.CapsuleH));   // feet
	BodyPivot->SetRelativeScale3D(FVector(FigScale));
	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->MaxWalkSpeed = St.WalkSpeed;
	CM->MaxStepHeight = 250.f * FigScale;
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 1400.f * FigScale));
	SpringArm->TargetArmLength = 2400.f * FigScale;
}

float AHumanCharacter::FeetZ() const
{
	return (float)GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void AHumanCharacter::Panic(float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	bKnockedOut = true;
	bSpraying = false;
	bStompPending = false;
	KnockoutEnd = GetWorld()->GetTimeSeconds() + Seconds;
	OnRep_KnockedOut();
}

void AHumanCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHumanCharacter, Health);
	DOREPLIFETIME(AHumanCharacter, SprayFuel);
	DOREPLIFETIME(AHumanCharacter, ShakeReadyTime);
	DOREPLIFETIME(AHumanCharacter, bSpraying);
	DOREPLIFETIME(AHumanCharacter, bKnockedOut);
	DOREPLIFETIME(AHumanCharacter, Rage);
	DOREPLIFETIME(AHumanCharacter, Held);
	DOREPLIFETIME(AHumanCharacter, SwatReadyTime);
	DOREPLIFETIME(AHumanCharacter, StompReadyTime);
	DOREPLIFETIME(AHumanCharacter, bStunned);
	DOREPLIFETIME(AHumanCharacter, Act);
	DOREPLIFETIME(AHumanCharacter, StunKind);
	DOREPLIFETIME(AHumanCharacter, FolkLook);
	DOREPLIFETIME(AHumanCharacter, bCarryPose);
	DOREPLIFETIME(AHumanCharacter, BackAcheUntil);
	DOREPLIFETIME(AHumanCharacter, bTowel);
	DOREPLIFETIME(AHumanCharacter, Task);
}

void AHumanCharacter::PlayBusy(uint8 Busy, const FVector& Target)
{
	if (!HasAuthority() || !bNewBody)
	{
		return;
	}
	if (Act.Kind == HumanAct::Busy && Act.Arg == Busy && ActTime() >= 0.f)
	{
		return;
	}
	StartAct(HumanAct::Busy, Busy, Target);
}

void AHumanCharacter::EndBusy()
{
	if (!HasAuthority())
	{
		return;
	}
	if (Act.Kind == HumanAct::Sit)
	{
		StartAct(HumanAct::Rise, Act.Arg, FVector(Act.Target));
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	else if (Act.Kind == HumanAct::Busy)
	{
		StartAct(HumanAct::None, 0, FVector::ZeroVector);
	}
}

void AHumanCharacter::SitAt(const FVector& Seat, float Yaw, uint8 How)
{
	if (!HasAuthority() || !bNewBody)
	{
		return;
	}
	// The feet go in front of the seat (as grandma's in front of her armchair), facing away from it; the pelvis goes back onto it.
	const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector();
	const float Off = (How == 2 || How == 5 || How == 6) && Seat.Z < FeetZ() + 120.f ? 190.f * FigScale / 0.88f : 660.f * FigScale / 0.88f;
	SitFrom = GetActorLocation();
	const FVector Stand(Seat.X + Fwd.X * Off, Seat.Y + Fwd.Y * Off, GetActorLocation().Z);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorLocationAndRotation(Stand, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller) { Controller->SetControlRotation(FRotator(0.f, Yaw, 0.f)); }
	StartAct(HumanAct::Sit, How, Seat);
}

void AHumanCharacter::SetAway(bool bInAway, const FVector& At)
{
	if (!HasAuthority() || bInAway == bAway)
	{
		return;
	}
	bAway = bInAway;
	if (bAway)
	{
		TryShakeOff();   // whoever rides him falls off at the door
		AwayFrom = At;
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorLocation(FVector(23000.f, At.Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);   // out in the corridor behind the door
	}
	else
	{
		SetActorLocation(FVector(At.X, At.Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		SetActorHiddenInGame(false);
		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRT dad %s at %s"), bAway ? TEXT("leaves the flat") : TEXT("is back"), *At.ToCompactString());
}

FVector AHumanCharacter::GetSoleLocation(int32 Leg) const
{
	if (bNewBody && Leg == 2)
	{
		return GetPalmLocation(0) - FVector(0.f, 0.f, AAntCharacter::BodyRadius);   // slapped flat: it sticks to the palm
	}
	if (bNewBody && Leg == 3)
	{
		// Swatted: on the business end of the rolled newspaper.
		return Segs[HumanSeg::HandR]->GetComponentTransform().TransformPosition(FVector(1250.f, -35.f, -150.f) / HumanBody::Scale) - FVector(0.f, 0.f, AAntCharacter::BodyRadius);
	}
	if (bNewBody)
	{
		// Under the ball of the real foot, wherever it is: on the floor, in the air mid-stride, cocked for a stomp.
		// (A squashed ant is put BodyRadius above this point: that leaves it flat against the sole, showing when the foot lifts.)
		const UStaticMeshComponent* Foot = Segs[Leg == 0 ? HumanSeg::FootL : HumanSeg::FootR];
		return Foot->GetComponentTransform().TransformPosition(FVector(230.f, 0.f, -163.f) / HumanBody::Scale) - FVector(0.f, 0.f, AAntCharacter::BodyRadius * 1.4f);
	}
	return GetActorLocation() + GetActorForwardVector() * 90.f + GetActorRightVector() * (Leg == 0 ? -180.f : 180.f) - FVector(0.f, 0.f, HalfHeight - 4.f);
}

bool AHumanCharacter::Stun(float Seconds, AAntCharacter* By, const FString& What, uint8 Kind)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || bKnockedOut || bStunned || Now < NextStunTime || IsDown())
	{
		return false;
	}
	bStunned = true;
	StunKind = Kind;
	StunEnd = Now + Seconds;
	NextStunTime = StunEnd + 10.f;
	bSpraying = false;
	bStompPending = false;
	OnRep_KnockedOut();
	AntSfx::Everyone(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 600.f), 1.3f);
	UE_LOG(LogTemp, Display, TEXT("ANTSTUN the Human is stunned for %.1f s by %s"), Seconds, *What);
	AntWeapons::Drop(this, TEXT("оглушён"));
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>(); GM && Fig == HumanFig::Dad)
	{
		GM->OnHumanStunned(By, What);
	}
	return true;
}

void AHumanCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetupBody();
	BuildBody();
}

void AHumanCharacter::SetupBody()
{
	SetupJointedBody();
	if (bNewBody)
	{
		return;
	}
	const AntArt::FPart& Dad = AntArt::Get(TEXT("SM_Human_Dad"));
	if (!Dad.IsValid() || !BodyMesh)
	{
		return;   // no model: the fallback shapes are drawn and nobody can climb him
	}
	BodyMesh->SetStaticMesh(Dad.Mesh);
	if (Dad.Material)
	{
		BodyMesh->SetMaterial(0, Dad.Material);
	}
	const FVector E = Dad.Bounds.GetSize();
	const float S = 3350.f / FMath::Max(1.f, (float)E.Z);
	// Shoulders are the wide side: they go across (Y).
	const float Yaw = (E.X > E.Y ? 90.f : 0.f) + ModelYaw;
	const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
	const FVector C = Dad.Bounds.GetCenter();
	const FVector FeetCenter(C.X, C.Y, Dad.Bounds.Min.Z);
	BodyMesh->SetRelativeTransform(FTransform(Q, -Q.RotateVector(FeetCenter * S), FVector(S)));
	// Ants, the spider (and anything else on UAntMovementComponent) can climb up his legs and ride him.
	UAntMovementComponent::MakeClimbable(BodyMesh);
	ModelPivot = BodyPivot;
}

void AHumanCharacter::BuildBody()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (bNewBody)
	{
		BuildHeldThings();
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
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(C);
			if (Prim && Prim != BodyMesh)
			{
				Prim->SetVisibility(false);
			}
		}
		// The body model itself is BodyMesh (SetupBody): it's also the surface ants climb.
		BodyPivot->SetVisibility(true, true);

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

	// (The spray cloud is drawn by AntFx::SprayPuff in UpdateVisuals: translucent puffs that drift and linger.)
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
	Input->BindKey(EKeys::F, IE_Pressed, this, &AHumanCharacter::OnShakePressed);
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

void AHumanCharacter::OnShakePressed()
{
	ServerShake();
}

void AHumanCharacter::ServerShake_Implementation() { TryShakeOff(); }

int32 AHumanCharacter::CountClimbers() const
{
	int32 N = 0;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->bDead && It->GetRiddenHuman() == this)
		{
			++N;
		}
	}
	return N;
}

void AHumanCharacter::TryShakeOff()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || bKnockedOut || Now < ShakeReadyTime)
	{
		return;
	}
	ShakeReadyTime = Now + 6.f * (1.f - 0.15f * Rage);
	MulticastShake();
	if (bNewBody && !bStompPending)
	{
		StartAct(HumanAct::Shake, 0, GetActorLocation());
	}
	const FVector Axis = GetActorLocation();
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->GetRiddenHuman() != this)
		{
			continue;
		}
		// Flung away from the body, some of them land hard.
		FVector Out = (Ant->GetActorLocation() - Axis).GetSafeNormal2D();
		if (Out.IsNearlyZero())
		{
			Out = FMath::VRand().GetSafeNormal2D();
		}
		Ant->ApplyLaunch(Out * FMath::FRandRange(800.f, 1100.f) + FVector(0.f, 0.f, 350.f));
		UGameplayStatics::ApplyDamage(Ant, 25.f, GetController(), this, UDamageType::StaticClass());
	}
}

void AHumanCharacter::MulticastShake_Implementation()
{
	ShakeLocalStart = GetWorld()->GetTimeSeconds();
	AntSfx::Play(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 600.f), 1.f, FMath::FRandRange(0.95f, 1.1f));
}

void AHumanCharacter::AnimateBody(float DeltaSeconds)
{
	if (bNewBody)
	{
		PoseBody(DeltaSeconds);
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	PrevWalkPhase = WalkPhase;
	WalkPhase += DeltaSeconds * Speed / 140.f;

	// Knockout sway + a violent shudder when shaking ants off.
	float Roll = bKnockedOut ? FMath::Sin(Now * 2.5f) * 10.f : bStunned ? FMath::Sin(Now * 7.f) * 6.f : 0.f;   // stunned: he reels
	const float ST = Now - ShakeLocalStart;
	if (ST >= 0.f && ST < 0.7f)
	{
		Roll += FMath::Sin(ST * 45.f) * 7.f * (1.f - ST / 0.7f);
	}
	VisualRoot->SetRelativeRotation(FRotator(0.f, 0.f, Roll));

	if (ModelPivot)
	{
		// Static model: waddle while walking, lean into stomps.
		const float Walk = FMath::Min(1.f, Speed / 300.f);
		const float T = Now - LocalStompStart;
		float Lean = 0.f, Lift = FMath::Abs(FMath::Sin(WalkPhase)) * 35.f * Walk;
		if (T >= 0.f && T < 1.1f)
		{
			Lean = T < 0.45f ? -6.f * (T / 0.45f) : T < 0.6f ? FMath::Lerp(-6.f, 9.f, (T - 0.45f) / 0.15f) : FMath::Lerp(9.f, 0.f, (T - 0.6f) / 0.5f);
		}
		ModelPivot->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight + Lift));
		ModelPivot->SetRelativeRotation(FRotator(-Lean, 0.f, FMath::Sin(WalkPhase) * 3.f * Walk));
	}
}

void AHumanCharacter::ServerSwat_Implementation(FVector_NetQuantize Point) { TrySwat(Point); }
void AHumanCharacter::ServerGrabOrThrow_Implementation(FVector_NetQuantize Point, FVector_NetQuantizeNormal Dir) { GrabOrThrow(Point, Dir); }

void AHumanCharacter::TrySwat(const FVector& InPoint, int32 Force)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || bKnockedOut || Now < SwatReadyTime || IsBodyBusy())
	{
		return;
	}
	if (!AntVersus::Spend(this, AntVersus::SwingCost(this)))
	{
		return;   // versus: a player Human out of patience (AntVersus.h)
	}
	// Out of reach: slap as far as the arm goes.
	FVector Point = InPoint;
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 600.f);
	// The jointed body: a low target gets the bare left hand (he has to crouch for it), anything higher the rolled newspaper.
	// Both land only where the arm really gets to.
	const FHumanStyle& St = HumanBody::Style(Fig);
	const AntWeapons::FDef* Wd = Fig == HumanFig::Dad ? AntWeapons::Held(this) : nullptr;   // a weapon in the fist (AntWeapon.h): its wind-up, reach, radius
	const float SwatR = Wd ? Wd->Radius * (1.f + 0.15f * Rage) : St.SwatRadius;
	const float AntDmg = Wd ? Wd->AntDamage : 150.f;
	const bool bSlap = !Wd && bNewBody && Fig == HumanFig::Dad && (Force >= 0 ? Force == 0 : Point.Z - FeetZ() < 260.f);   // (grandma swats the floor with her swatter too)
	float Delay = 0.28f;
	if (bSlap)
	{
		FVector Flat = Point - GetActorLocation();
		Flat.Z = 0.f;
		if (Flat.Size() > 1350.f)
		{
			Point = FVector(GetActorLocation().X, GetActorLocation().Y, Point.Z) + Flat.GetSafeNormal() * 1350.f;
		}
		Delay = HumanAct::SlapHit;
	}
	else if (bNewBody)
	{
		const FVector Shoulder = GetActorLocation() + FVector(0.f, 0.f, 960.f * FigScale);
		const float Arm = (2500.f + (Wd ? Wd->Reach : 0.f)) * FigScale;
		if (FVector::Dist(Point, Shoulder) > Arm)
		{
			Point = Shoulder + (Point - Shoulder).GetSafeNormal() * Arm;
		}
		Delay = Wd ? FMath::Max(0.4f, Wd->Windup) : St.SwatHit;
	}
	else if (FVector::Dist(Point, Chest) > SwatRange)
	{
		Point = Chest + (Point - Chest).GetSafeNormal() * SwatRange;
	}
	SwatReadyTime = Now + (Wd ? Wd->Every : St.SwatEvery) * (1.f - 0.15f * Rage);
	MulticastSwat(Point);
	if (bNewBody)
	{
		StartAct(bSlap ? HumanAct::Slap : HumanAct::Swat, 0, Point);
	}
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Point, SwatR, AntDmg]()
	{
		AntSfx::Everyone(this, AntSfx::Swat, Point, FMath::FRandRange(0.9f, 1.05f));
		if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
		{
			Swarm->KillInSphere(Point, SwatR);
		}
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* Ant = *It;
			if (Ant && !Ant->bDead && !Ant->IsInNest() && FVector::Dist(Ant->GetActorLocation(), Point) < SwatR + AAntCharacter::BodyRadius)
			{
				if (InCover(GetWorld(), Ant->GetActorLocation()))
				{
					// Cover is honest: what his eyes cannot pick, his newspaper and his palm cannot hit either.
					UE_LOG(LogTemp, Display, TEXT("ANTV2 cover: %s is under cover, the swat at %s did nothing"), *Ant->GetDisplayName(), *Point.ToCompactString());
					continue;
				}
				if (AntDmg > 0.f) { UGameplayStatics::ApplyDamage(Ant, AntDmg, GetController(), this, UDamageType::StaticClass()); }
			}
		}
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			const FVector To = It->GetActorLocation() - Point;
			if (To.Size() < SwatR * 1.6f)
			{
				It->Kick(To.GetSafeNormal() * 500.f + FVector(0.f, 0.f, 300.f), It->GetActorLocation());
			}
		}
		if (Fig == HumanFig::Dad)
		{
			AntWeapons::OnSwingLanded(this, Point);   // the world part: furniture, fixtures, food (AntWreck.h)
		}
	}), Delay, false);
}

void AHumanCharacter::GrabOrThrow(const FVector& Point, const FVector& Dir)
{
	if (!HasAuthority() || bKnockedOut)
	{
		return;
	}
	if (IsBodyBusy() || GetWorldTimerManager().IsTimerActive(GrabTimer) || GetWorldTimerManager().IsTimerActive(ThrowTimer))
	{
		return;   // the hand is on its way already
	}
	if (!Held && Fig == HumanFig::Dad && AntWeapons::TryPickOrDrop(this, Point))
	{
		return;   // aimed at a weapon lying about: it goes into the right fist (AntWeapon.h)
	}
	if (Held)
	{
		// Throw it: food smashes when it lands hard (and bowls ants over).
		AKitchenLoot* L = Held;
		const FVector V = Dir.GetSafeNormal() * 2600.f + FVector(0.f, 0.f, 300.f);
		if (!bNewBody)
		{
			Held = nullptr;
			L->Throw(V);
			return;
		}
		// The arm goes back, whips forward and lets go.
		StartAct(HumanAct::Throw, 0, GetActorLocation() + FVector(0.f, 0.f, 900.f) + Dir.GetSafeNormal() * 3000.f);
		GetWorldTimerManager().SetTimer(ThrowTimer, FTimerDelegate::CreateWeakLambda(this, [this, V]()
		{
			AKitchenLoot* Mine = Held;
			Held = nullptr;
			if (IsValid(Mine) && Mine->bHeld)
			{
				Mine->Throw(V);
			}
		}), HumanAct::ThrowRelease, false);
		return;
	}
	AKitchenLoot* Best = nullptr;
	float BestD = 700.f;
	const float MaxFlat = bNewBody ? 1700.f : 2600.f;   // the jointed body only takes what the hand can get to
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L || L->bIsGear || L->bHeld || FVector::Dist2D(L->GetActorLocation(), GetActorLocation()) > MaxFlat)
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
	if (Best && !bNewBody)
	{
		Held = Best;
		Best->PickUpBy(VisualRoot, FVector(700.f, 420.f, 500.f));
	}
	else if (Best)
	{
		// The left hand goes to the food; it is his (and leaves the ants' jaws) the moment the fingers close on it.
		Held = Best;
		StartAct(HumanAct::Grab, 0, Best->GetActorLocation());
		GetWorldTimerManager().SetTimer(GrabTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			AKitchenLoot* L = Held;
			if (!IsValid(L) || L->bHeld || bKnockedOut)
			{
				Held = nullptr;
				return;
			}
			const float Rad = FMath::Clamp((float)L->GetSimpleCollisionRadius(), 10.f, 260.f);
			L->PickUpBy(Segs[HumanSeg::HandL], FVector(0.f, 30.f + Rad * 0.8f, -150.f) / HumanBody::Scale);   // on the open palm
		}), HumanAct::GrabGrip, false);
	}
}

uint8 AHumanCharacter::GetLastStompLeg() const
{
	if (bNewBody && (Act.Kind == HumanAct::Slap || Act.Kind == HumanAct::Swat) && ActTime() < 1.f)
	{
		return Act.Kind == HumanAct::Slap ? 2 : 3;
	}
	return PendingLeg;
}

bool AHumanCharacter::IsBodyBusy() const
{
	if (!bNewBody)
	{
		return false;
	}
	const float T = ActTime();
	if (T < 0.f)
	{
		return false;
	}
	switch (Act.Kind)
	{
	case HumanAct::Stomp: return T < HumanAct::StompHit;
	case HumanAct::Swat: return T < AntWeapons::SwingWindup(this, HumanBody::Style(Fig).SwatHit);
	case HumanAct::Rise: return T < HumanAct::SitTime * HumanBody::Style(Fig).SitSlow;
	case HumanAct::Sweep: return T < HumanAct::SweepHit + 0.5f;
	case HumanAct::Slap: return T < HumanAct::SlapHit + 0.3f;
	case HumanAct::Grab: return T < HumanAct::GrabGrip + 0.1f;
	case HumanAct::Throw: return T < HumanAct::ThrowRelease + 0.05f;
	case HumanAct::Fall: return T < HumanAct::FallUp;
	case HumanAct::Wipe: return T < Act.Arg * 0.1f;
	default: return false;
	}
}

bool AHumanCharacter::IsRooted() const
{
	if (bNewBody && (Act.Kind == HumanAct::Wipe || Act.Kind == HumanAct::Fall))
	{
		return IsBodyBusy();   // on the floor, or down on his haunches with the towel
	}
	if (!bNewBody || (Act.Kind != HumanAct::Slap && Act.Kind != HumanAct::Grab && Act.Kind != HumanAct::Reach))
	{
		return false;
	}
	const float T = ActTime();
	return T >= 0.f && T < (Act.Kind == HumanAct::Slap ? HumanAct::SlapHit + 0.7f : 0.8f);
}

void AHumanCharacter::StopPeer()
{
	if (HasAuthority() && Act.Kind == HumanAct::Peer)
	{
		StartAct(HumanAct::None, 0, GetActorLocation());
	}
}

void AHumanCharacter::DevAct(const FString& What, const FVector& Target)
{
	if (!HasAuthority())
	{
		return;
	}
	StompReadyTime = SwatReadyTime = ShakeReadyTime = 0.f;
	const FVector Me = GetActorLocation();
	if (What == TEXT("stomp0") || What == TEXT("stomp1")) { TryStomp(What.EndsWith(TEXT("1")) ? 1 : 0); }
	else if (What == TEXT("swat")) { TrySwat(Target); }
	else if (What == TEXT("grab")) { GrabOrThrow(Target, GetActorForwardVector()); }
	else if (What == TEXT("throw")) { GrabOrThrow(Target, (Target - Me).GetSafeNormal2D() + FVector(0.f, 0.f, 0.3f)); }
	else if (What == TEXT("reach")) { PlayReach(Target); }
	else if (What == TEXT("peer")) { PlayPeer(Target, 6.f); }
	else if (What == TEXT("shake")) { TryShakeOff(); }
	else if (What == TEXT("spray1")) { SprayFuel = 100.f; SetSpraying(true); }
	else if (What == TEXT("spray0")) { SetSpraying(false); }
	else if (What == TEXT("stun0") || What == TEXT("stun1")) { NextStunTime = 0.f; Stun(6.f, nullptr, TEXT("dev"), What.EndsWith(TEXT("1")) ? 1 : 0); }
	else if (What == TEXT("ko")) { KnockOut(nullptr); }
	else if (What.StartsWith(TEXT("rage"))) { SetRage((uint8)FCString::Atoi(*What.RightChop(4))); }
	else if (What == TEXT("slip")) { TrySlip(FVector(GetActorLocation().X, GetActorLocation().Y, FeetZ()), 1.f, true); }
	else if (What == TEXT("wipe")) { bTowel = true; PlayWipe(Target, 4.f); }
	else if (What == TEXT("towel1") || What == TEXT("towel0")) { bTowel = What.EndsWith(TEXT("1")); }
	else { UE_LOG(LogTemp, Warning, TEXT("ANTHUMAN dev: unknown action %s"), *What); }
	UE_LOG(LogTemp, Display, TEXT("ANTHUMAN dev %s -> %s"), *What, *DevDescribe());
}

void AHumanCharacter::MulticastSwat_Implementation(FVector_NetQuantize Point)
{
	SwatPoint = Point;
	SwatLocalStart = GetWorld()->GetTimeSeconds();
}

void AHumanCharacter::MoveForward(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value) && !IsRooted())   // down on the floor with a hand out: he gets up first
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
	}
}

void AHumanCharacter::MoveRight(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value) && !IsRooted())
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
	if (!HasAuthority() || bKnockedOut || bStompPending || bStunned || IsBodyBusy())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < StompReadyTime || !AntVersus::Spend(this, AntVersus::StompCost))
	{
		return;
	}
	PendingLeg = (uint8)(Leg < 0 ? NextLeg : FMath::Clamp(Leg, 0, 1));
	NextLeg = 1 - PendingLeg;
	bStompPending = true;
	StompImpactTime = Now + 0.55f;
	StompReadyTime = Now + 1.05f * (1.f - 0.18f * Rage);
	MulticastStomp(PendingLeg);
	if (bNewBody)
	{
		StartAct(HumanAct::Stomp, PendingLeg, GetStompPoint(PendingLeg));
	}
}

void AHumanCharacter::MulticastStomp_Implementation(uint8 Leg)
{
	LocalStompStart = GetWorld()->GetTimeSeconds();
	LocalStompLeg = Leg;
}

FVector AHumanCharacter::GetStompPoint(int32 Leg) const
{
	return GetActorLocation() + (GetActorForwardVector() * 480.f + GetActorRightVector() * (Leg == 0 ? -180.f : 180.f)) * FigScale - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

bool AHumanCharacter::InCover(const UWorld* World, const FVector& P)
{
	// Inside the fridge (and the gap under it), the oven, the cabinet under the sink.
	if (FBox(FVector(2700.f, 3000.f, 0.f), FVector(4700.f, 4750.f, 3700.f)).IsInside(P)
		|| FBox(FVector(-1500.f, 3000.f, 120.f), FVector(0.f, 4750.f, 1650.f)).IsInside(P)
		|| FBox(FVector(-4200.f, 3000.f, 0.f), FVector(-2800.f, 4750.f, 1100.f)).IsInside(P))
	{
		return true;
	}
	// The corridor behind the base cabinets, under the worktop.
	if (P.Y > 4750.f && P.Z < 1650.f && P.X < 2500.f && P.X > -7200.f)
	{
		return true;
	}
	if (World && KitchenContainers::SealedAt(World, P) >= 0)
	{
		return true;   // behind a shut door
	}
	// Level v2: the designed hiding places (docs/level/level_v2.json, human.sees.never_targets).
	return KitchenArt::Enabled() && LevelV2::CoverAt(P) != nullptr;
}

void AHumanCharacter::DoStompImpact()
{
	const FVector P = GetStompPoint(PendingLeg);
	const float SR = GetStompRadius();
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Swarm->KillInSphere(P, SR * 1.15f);
	}
	// The floor shakes: food nearby jumps (and carriers often lose it).
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		const FVector To = It->GetActorLocation() - P;
		if (To.Size2D() < SR * 2.6f && FMath::Abs(To.Z) < 300.f)
		{
			const float K = 1.f - (float)To.Size2D() / (SR * 2.6f);
			It->Kick(To.GetSafeNormal2D() * 650.f * K + FVector(0.f, 0.f, 800.f * K), It->GetActorLocation() + FMath::VRand() * 10.f);
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
		if (Ant->GetRiddenHuman() == this)
		{
			continue;   // can't stomp what's crawling up your own leg
		}
		const float D2 = (float)FVector::Dist2D(A, P);
		if (InCover(GetWorld(), A))
		{
			// Cover is honest: an ant inside a slipper, a bottle, the cereal box... is out of the slipper's reach, shockwave included.
			if (D2 < SR * 2.6f)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTV2 cover: %s is under cover, the stomp %.0f away did nothing"), *Ant->GetDisplayName(), D2);
			}
			continue;
		}
		if (D2 < SR && A.Z - P.Z < 220.f)
		{
			UGameplayStatics::ApplyDamage(Ant, 200.f, GetController(), this, UDamageType::StaticClass());
		}
		else if (D2 < SR * 2.6f && FMath::Abs(A.Z - P.Z) < 260.f && !Ant->IsAttached())
		{
			// A near miss: only what is right under the slipper dies, the shockwave throws the rest across the floor.
			const float K = 1.f - (D2 - SR) / (SR * 1.6f);
			const FVector Out = (A - P).GetSafeNormal2D();
			Ant->ApplyLaunch((Out * (450.f + 950.f * K) + FVector(0.f, 0.f, 380.f + 440.f * K)) * AntKit::SpeedK);
			if (Ant->Carrying && K > 0.45f)
			{
				Ant->LetGo();
				Ant->Toast(TEXT("Ударная волна выбила груз!"));
			}
			UE_LOG(LogTemp, Display, TEXT("ANTSHOCK %s thrown by the stomp shockwave (%.0f from the slipper)"), *Ant->GetDisplayName(), D2);
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
	if (bNewBody)
	{
		return BodyFrame().TransformPosition(SprayNozzleBody());   // the nozzle of the can in the outstretched right hand
	}
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
	if (Fig != HumanFig::Dad)
	{
		OnFolkHurt(Actual, EventInstigator);   // grandma and the kid are not knocked out by bites: they react
		return Actual;
	}
	Health = FMath::Max(0.f, Health - Actual);
	LastHurtTime = GetWorld()->GetTimeSeconds();
	if (DamageCauser && FVector::Dist(DamageCauser->GetActorLocation(), GetPalmLocation(1)) < 450.f && FMath::FRand() < 0.35f)
	{
		AntWeapons::Drop(this, TEXT("укусили за руку"));
	}
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
	AntSfx::Everyone(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 600.f), 1.f);
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
	bStunned = false;
	NextStunTime = 0.f;
	NextFallTime = 0.f;
	FallImpactAt = -1.f;
	BackAcheUntil = -100.f;
	bTowel = false;
	SlippedOn = nullptr;
	if (bAway) { SetAway(false, Location); }
	Task = FNpcTask();
	StickyK = 1.f;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Act = FHumanActState();
	OnRep_KnockedOut();
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
}

FString AHumanCharacter::GetDisplayName() const
{
	if (Fig != HumanFig::Dad)
	{
		return Fig == HumanFig::Grandma ? TEXT("Бабушка") : TEXT("Малыш");
	}
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
	if (!bLetThrough)
	{
		bLetThrough = AKitchenBuilder::LetThrough(this);   // level v2: he walks through the yarn, the garland and the other "ants only" pieces
	}

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

		if (FallImpactAt > 0.f && Now >= FallImpactAt)
		{
			FallImpactAt = -1.f;
			DoFallImpact();
		}
		if (bStunned && Now >= StunEnd)
		{
			bStunned = false;
			OnRep_KnockedOut();
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

	if (HasAuthority() && !IsPlayerControlled() && !bKnockedOut && !bDevPuppet)
	{
		// AI: ants crawling up the legs get shaken off after a while (sooner the more there are).
		const int32 N = CountClimbers();
		ClimberTime = N > 0 ? ClimberTime + DeltaSeconds * (0.6f + 0.4f * N) : FMath::Max(0.f, ClimberTime - DeltaSeconds);
		if (ClimberTime > 3.f && Now >= ShakeReadyTime)
		{
			ClimberTime = 0.f;
			TryShakeOff();
		}
	}

	if (bNewBody && Fig == HumanFig::Dad)
	{
		GetCharacterMovement()->MaxWalkSpeed = GetWalkSpeed() * StickyK;   // down after a slip: he does not move; his back hurts after: slow; in cola: stuck
	}
	if (HasAuthority() && !SitFrom.IsZero() && Act.Kind == HumanAct::Rise && !IsBodyBusy())
	{
		// Up from a seat the routine teleported him onto: if the feet stand in the furniture, a step back to where he came from.
		const UCapsuleComponent* Cap = GetCapsuleComponent();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(HumanStandUp), false, this);
		if (GetWorld()->OverlapBlockingTestByChannel(GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius() * 0.9f, Cap->GetScaledCapsuleHalfHeight() * 0.9f), Q)
			&& FVector::Dist2D(SitFrom, GetActorLocation()) < 1800.f)
		{
			SetActorLocation(FVector(SitFrom.X, SitFrom.Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
		SitFrom = FVector::ZeroVector;
	}
	if (LevelV3::Enabled())
	{
		// Level v3: the routine state, as this machine sees it (the client's log proves it replicated); the Dad holds the hall door.
		if (Task.Step != LoggedStep || Task.Phase != LoggedPhase || Task.React != LoggedReact)
		{
			LoggedStep = Task.Step; LoggedPhase = Task.Phase; LoggedReact = Task.React;
			if (GetNetMode() == NM_Client)
			{
				const TConstArrayView<NpcRoutine::FStep> St = NpcRoutine::Steps();
				UE_LOG(LogTemp, Display, TEXT("ANTRT client sees %s: step %s phase %d react %d stage %d act %d/%d"), Fig == HumanFig::Dad ? TEXT("dad") : Fig == HumanFig::Grandma ? TEXT("gran") : TEXT("kid"),
					St.IsValidIndex(Task.Step) ? St[Task.Step].Key : TEXT("-"), (int32)Task.Phase, (int32)Task.React, (int32)Task.Stage, (int32)Act.Kind, (int32)Act.Arg);
			}
		}
		if (Fig == HumanFig::Dad)
		{
			const float Want = Task.Door ? 1.f : 0.f;
			if (!FMath::IsNearlyEqual(HallDoorK, Want))
			{
				HallDoorK = FMath::FInterpConstantTo(HallDoorK, Want, DeltaSeconds, 0.8f);
				NpcRoutine::SetHallDoor(GetWorld(), HallDoorK);
			}
		}
	}
	if (bDevPuppet && HasAuthority() && !DevWalk.IsZero() && !IsDown() && !IsRooted())
	{
		// Dev: the puppet walks to a point (screenshots of the stride).
		FVector To = DevWalk - GetActorLocation();
		To.Z = 0.f;
		if (To.Size() > 120.f)
		{
			AddMovementInput(To.GetSafeNormal(), DevWalkScale);
			const FRotator Face = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, (float)To.Rotation().Yaw, 0.f), DeltaSeconds, 3.5f);
			SetActorRotation(Face);
			if (Controller) { Controller->SetControlRotation(Face); }
		}
		else
		{
			DevWalk = FVector::ZeroVector;
		}
	}

	AnimateBody(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
}

void AHumanCharacter::UpdateVisuals(float DeltaSeconds)
{
	if (bNewBody)
	{
		BodyEffects(DeltaSeconds);   // footfalls come from the feet themselves (OnFootfall)
		return;
	}
	if (GetNetMode() == NM_DedicatedServer || LegPivots.Num() < 2 || ArmPivots.Num() < 2)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = GetVelocity().Size2D();
	const float OldPhase = PrevWalkPhase;   // WalkPhase is advanced in AnimateBody
	if (Speed > 80.f && FMath::FloorToInt(OldPhase / PI) != FMath::FloorToInt(WalkPhase / PI))
	{
		// Heavy slipper footfall, heard from the floor.
		AntSfx::Play(this, AntSfx::HumanStep, GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()),
			FMath::Min(1.f, Speed / 400.f) * 0.9f, FMath::FRandRange(0.9f, 1.05f));
	}
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
		// (the body pose itself is set in AnimateBody, on every machine)
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

	// The cloud: puffs shot down the cone (the same 20 degrees that hurt), slowing and hanging in the air for a moment.
	if (bSpraying)
	{
		SprayFxAccum += DeltaSeconds * 26.f;
		int32 Burst = 0;
		while (SprayFxAccum >= 1.f && Burst++ < 4)
		{
			SprayFxAccum -= 1.f;
			AntFx::SprayPuff(this, Origin, Dir, GetSprayRange());
		}
		SprayFxAccum = FMath::Min(SprayFxAccum, 1.f);
	}
}

// ---------------------------------------------------------------------------------------------
// Slipping on a puddle: the fall, the landing, the back that hurts, the towel
// ---------------------------------------------------------------------------------------------

float AHumanCharacter::NetNow() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	return (HasAuthority() || !GS) ? W->GetTimeSeconds() : (float)GS->GetServerWorldTimeSeconds();
}

bool AHumanCharacter::IsDown() const
{
	if (!bNewBody || Act.Kind != HumanAct::Fall)
	{
		return false;
	}
	const float T = ActTime();
	return T >= 0.f && T < HumanAct::FallUp;
}

bool AHumanCharacter::HasBackAche() const
{
	return !IsDown() && NetNow() < BackAcheUntil;
}

bool AHumanCharacter::IsWiping() const
{
	return bNewBody && Act.Kind == HumanAct::Wipe && ActTime() < Act.Arg * 0.1f;
}

void AHumanCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (IsDown())
	{
		return;   // lying on his back: the body does not turn with the controller (it is posed in his own frame)
	}
	Super::FaceRotation(NewControlRotation, DeltaTime);
}

void AHumanCharacter::PlayWipe(const FVector& Point, float Seconds)
{
	if (bNewBody && HasAuthority() && !bKnockedOut && !bStunned && !IsDown())
	{
		StartAct(HumanAct::Wipe, (uint8)FMath::Clamp(FMath::RoundToInt(Seconds * 10.f), 5, 250), Point);
	}
}

void AHumanCharacter::CheckSlip(int32 Leg, bool bAttack)
{
	if (!HasAuthority() || !bNewBody || Fig != HumanFig::Dad || IsDown())
	{
		return;
	}
	const FVector At = Rig.Feet[Leg & 1].Pos;
	if (!AKitchenSlick::At(GetWorld(), At))
	{
		return;
	}
	// Not every step: a stroll may get across, a run never does; a stomp into it is likely to go wrong.
	const float Speed = (float)GetVelocity().Size2D();
	const float Run = HumanBody::Style(Fig).WalkSpeed * (1.f + 0.12f * Rage) * 0.8f;
	float Chance = Speed >= Run ? 1.f : Speed < 100.f ? 0.f : FMath::Clamp((Speed - 120.f) / FMath::Max(1.f, Run - 120.f), 0.12f, 1.f) * 0.85f;
	if (bAttack)
	{
		Chance = FMath::Max(Chance, 0.7f);
	}
	if (!TrySlip(At, Chance))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFALL foot %d in a puddle at speed %.0f (chance %.2f%s): kept his feet"), Leg, Speed, Chance, GetWorld()->GetTimeSeconds() < NextFallTime ? TEXT(", resting") : TEXT(""));
	}
}

bool AHumanCharacter::TrySlip(const FVector& At, float Chance, bool bForce, AActor* By)
{
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	if (!HasAuthority() || !bNewBody || Fig != HumanFig::Dad || bKnockedOut || bStunned || IsDown())
	{
		return false;
	}
	if (!bForce && (Now < NextFallTime || FMath::FRand() >= Chance))
	{
		return false;
	}
	// Which way: along where he was going (he goes down backwards, the feet shoot forward).
	FVector Fwd = GetVelocity().GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = GetActorForwardVector().GetSafeNormal2D();
	}
	const float SpeedBefore = (float)GetVelocity().Size2D();
	const FRotator Face(0.f, (float)Fwd.Rotation().Yaw, 0.f);
	SetActorRotation(Face);
	if (Controller) { Controller->SetControlRotation(Face); }
	GetCharacterMovement()->StopMovementImmediately();
	const int32 Leg = FVector::DistSquared2D(Rig.Feet[0].Pos, At) <= FVector::DistSquared2D(Rig.Feet[1].Pos, At) ? 0 : 1;
	const FVector Floor(GetActorLocation().X, GetActorLocation().Y, FeetZ());
	StartAct(HumanAct::Fall, (uint8)Leg, Floor - Fwd * 2300.f * FigScale);
	bSpraying = false;
	bStompPending = false;
	bTowel = false;
	FallImpactAt = Now + HumanAct::FallHit;
	NextFallTime = Now + HumanAct::FallUp + 45.f;
	BackAcheUntil = Now + HumanAct::FallUp + 10.f;
	if (Held)
	{
		// Whatever was in his hand flies.
		AKitchenLoot* L = Held;
		Held = nullptr;
		if (IsValid(L) && L->bHeld) { L->Throw(Fwd * 900.f + FVector(0.f, 0.f, 1500.f)); }
	}
	AntWeapons::Drop(this, TEXT("поскользнулся"));
	SlippedOn = AKitchenSlick::At(W, At, 250.f);
	// From now on he knows the puddles: he walks round them.
	for (TActorIterator<AKitchenSlick> It(W); It; ++It) { It->bKnown = true; }
	AntSfx::Everyone(this, AntSfx::HumanPanic, GetActorLocation() + FVector(0.f, 0.f, 600.f), 0.85f);
	AActor* Who = By ? By : (SlippedOn.IsValid() ? SlippedOn->By.Get() : nullptr);
	UE_LOG(LogTemp, Display, TEXT("ANTFALL the Human slips at %s (leg %d, speed %.0f, chance %.2f%s), falls towards yaw %.0f, puddle by %s"), *At.ToCompactString(), Leg,
		SpeedBefore, Chance, bForce ? TEXT(", forced") : TEXT(""), (float)(-Fwd).Rotation().Yaw, *GetNameSafe(Who));
	if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->OnHumanFell(Cast<AAntCharacter>(Who));
	}
	return true;
}

void AHumanCharacter::DoFallImpact()
{
	UWorld* W = GetWorld();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	const FVector Floor(GetActorLocation().X, GetActorLocation().Y, FeetZ());
	// Where his body lies: from the heels (ahead of where he stood) to the head (behind), about a shoulder wide.
	const FVector Heels = Floor + Fwd * 900.f * FigScale, Head = Floor - Fwd * 2300.f * FigScale;
	const float Wide = 420.f * FigScale;
	int32 Squashed = 0, Thrown = 0, Jumped = 0;
	if (AAntSwarm* Swarm = AAntSwarm::Get(W))
	{
		for (int32 i = 0; i <= 3; ++i) { Swarm->KillInSphere(FMath::Lerp(Heels, Head, i / 3.f), Wide); }
	}
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest())
		{
			continue;
		}
		const FVector A = Ant->GetActorLocation();
		if (Ant->GetRiddenHuman() == this)
		{
			Ant->ApplyLaunch((FVector(0.f, 0.f, 900.f) + FMath::VRand() * 300.f) * AntKit::SpeedK);   // riders fly off
			++Thrown;
			continue;
		}
		const FVector OnLine = FMath::ClosestPointOnSegment(A, Heels, Head);
		const float D = (float)FVector::Dist2D(A, OnLine);
		if (A.Z - Floor.Z > 400.f || InCover(W, A))
		{
			continue;
		}
		if (D < Wide)
		{
			UGameplayStatics::ApplyDamage(Ant, 400.f, GetController(), this, UDamageType::StaticClass());
			++Squashed;
			UE_LOG(LogTemp, Display, TEXT("ANTFALL %s squashed under the falling Human (%.0f from his spine)"), *Ant->GetDisplayName(), D);
		}
		else if (D < Wide * 3.2f && !Ant->IsAttached())
		{
			const float K = 1.f - (D - Wide) / (Wide * 2.2f);
			Ant->ApplyLaunch(((A - OnLine).GetSafeNormal2D() * (400.f + 900.f * K) + FVector(0.f, 0.f, 420.f + 500.f * K)) * AntKit::SpeedK);
			if (Ant->Carrying && K > 0.35f)
			{
				Ant->LetGo();
				Ant->Toast(TEXT("Человек грохнулся — груз выбило!"));
			}
			++Thrown;
		}
	}
	// The floor jumps: food on it, and the things standing on the low surfaces near him (tables, chairs, the worktop) jump too
	// and some of them go over the edge.
	const FVector Mid = (Heels + Head) * 0.5f;
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (L->bHeld)
		{
			continue;
		}
		const FVector P = L->GetActorLocation();
		const float D = (float)FVector::Dist2D(P, Mid);
		const float Up = (float)(P.Z - Floor.Z);
		if (D < 3600.f && Up < 2000.f)
		{
			const float K = FMath::Clamp(1.f - D / 3600.f, 0.f, 1.f);
			if (L->IsCarried() && K > 0.4f) { L->ReleaseAll(); }
			const FVector Out = FVector(P - Mid).GetSafeNormal2D();
			L->Kick((Out * 260.f + FMath::VRand() * 90.f) * (0.4f + K) + FVector(0.f, 0.f, (Up > 300.f ? 520.f : 700.f) * (0.35f + K)), P + FMath::VRand() * 8.f);
			++Jumped;
		}
	}
	for (TActorIterator<AKitchenProp> It(W); It; ++It)
	{
		const FVector P = It->GetActorLocation();
		const float D = (float)FVector::Dist2D(P, Mid);
		if (D < 3600.f && P.Z - Floor.Z < 2000.f)
		{
			const float K = FMath::Clamp(1.f - D / 3600.f, 0.f, 1.f);
			It->Jolt(FVector(P - Mid).GetSafeNormal2D() * 160.f * K + FVector(0.f, 0.f, 260.f + 240.f * K) + FMath::VRand() * 60.f, this);
			++Jumped;
		}
	}
	for (TActorIterator<ACritter> It(W); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), Mid) < 6000.f) { It->Scare(Mid, TEXT("грохота")); }
	}
	AntSfx::Everyone(this, AntSfx::Stomp, Mid, 0.62f);
	AntSfx::Everyone(this, AntSfx::Drop, Mid, 0.5f);
	UE_LOG(LogTemp, Display, TEXT("ANTFALL the Human hits the floor at %s: %d ants squashed, %d thrown, %d things jumped"), *Mid.ToCompactString(), Squashed, Thrown, Jumped);
}
