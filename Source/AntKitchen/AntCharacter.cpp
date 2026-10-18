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
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

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
	CM->MaxWalkSpeed = 600.f;
	CM->MaxAcceleration = 4000.f;
	CM->JumpZVelocity = 620.f;
	CM->AirControl = 0.7f;
	CM->GravityScale = 1.4f;
	CM->MaxStepHeight = 20.f;
	CM->SetWalkableFloorAngle(55.f);
	CM->bOrientRotationToMovement = false;
	CM->bEnablePhysicsInteraction = true;
	CM->PushForceFactor = 120000.f;
	// Casual co-op: trust the client's position so crawling never rubber-bands.
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
	DOREPLIFETIME(AAntCharacter, Temperature);
	DOREPLIFETIME(AAntCharacter, StatusFlags);
	DOREPLIFETIME(AAntCharacter, TrailPoints);
	DOREPLIFETIME(AAntCharacter, bLayingTrail);
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

	// Food chunk carried on the back.
	CarryChunk = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-20.f, 0.f, 30.f), FVector(0.2f), FLinearColor(1.f, 0.75f, 0.2f));
	CarryChunk->SetVisibility(false);

	// Ice crust shown when frozen.
	FrostShell = AntKit::AddPart(this, VisualRoot, EAntShape::Sphere, FVector(-6.f, 0.f, 4.f), FVector(0.95f, 0.55f, 0.5f), FLinearColor(0.75f, 0.9f, 1.f));
	FrostShell->SetVisibility(false);
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
		VisualRoot->SetWorldLocationAndRotation(GetActorLocation() - VisNormal * 20.f, FRotationMatrix::MakeFromZX(VisNormal, VisHeading).ToQuat());
		VisualRoot->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.15f));   // squashed flat
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
	if (!Controller || bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}
	FVector F, R;
	SurfaceAxes(F, R);
	AddMovementInput(F, Value);
}

void AAntCharacter::MoveRight(float Value)
{
	if (!Controller || bDead || FMath::IsNearlyZero(Value))
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
	if (!bDead)
	{
		Jump();
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
	StatusFlags = E.Flags;

	float Dps = E.Dps;
	const TCHAR* Cause = *E.Cause;
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
		LastHazard = Temperature > 55.f ? TEXT("поджарился") : (Temperature < -4.f ? TEXT("замёрз насмерть") :
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
			TickTrail();
		}
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
// Looting and gadgets
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

	float GadgetDist = 0.f;
	AKitchenInteractable* Gadget = FindNearestInteractable(&GadgetDist);
	AKitchenLoot* Loot = FindNearestLoot(InteractRadius + 30.f);
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
		Toast(TEXT("Слишком тяжело! Отнеси добычу в муравейник"));
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
	if (bLayingTrail)
	{
		CancelTrail(TEXT("Ты погиб — тропа оборвалась"));
	}
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
	FVector Aim = GetActorForwardVector();
	if (Controller)
	{
		Aim = Controller->GetControlRotation().Vector();
	}
	return GetActorLocation() + VisNormal * 10.f + Aim * 40.f;
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
	const float Load = FMath::Max(0.55f, 1.f - 0.035f * CarriedWeight);
	M->MaxWalkSpeed = (bSprinting ? 950.f : 600.f) * Load;

	float Mult = 1.f;
	if (HasStatus(AntStatus::Sticky)) { Mult *= 0.35f; }
	if (HasStatus(AntStatus::Wet)) { Mult *= 0.7f; }
	if (Temperature < 8.f)
	{
		Mult *= FMath::Lerp(1.f, 0.45f, FMath::Clamp((8.f - Temperature) / 13.f, 0.f, 1.f));
	}
	if (IsFrozen()) { Mult *= 0.5f; }
	M->SpeedMultiplier = Mult;
	M->bNoJump = HasStatus(AntStatus::Sticky) || IsFrozen();
	M->bNoClimb = IsFrozen();
	M->bHeavyLoad = CarriedWeight >= 7;
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
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Me, Me + D * (BodyRadius + 22.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
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
	VisualRoot->SetWorldLocationAndRotation(GetActorLocation() - VisNormal * (2.f - Bob), FRotationMatrix::MakeFromZX(VisNormal, VisHeading).ToQuat());

	if (FrostShell)
	{
		FrostShell->SetVisibility(IsFrozen());
	}

	// Camera pivot sits "above" the ant relative to the surface it crawls on, so it never digs into a ceiling.
	if (IsLocallyControlled())
	{
		SpringArm->SocketOffset = FVector::ZeroVector;
		SpringArm->TargetOffset = FMath::VInterpTo(SpringArm->TargetOffset, VisNormal * 55.f, DeltaSeconds, 6.f);
		const float WantLen = VisNormal.Z < 0.6f ? 300.f : 230.f;
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, WantLen, DeltaSeconds, 4.f);
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
	const float StepDur = FMath::Clamp(0.15f - Speed * 0.00009f, 0.06f, 0.15f);
	const float Threshold = 16.f + Speed * 0.035f;
	const float StepHeight = 9.f + Speed * 0.006f;
	constexpr float L1 = 24.f;
	constexpr float L2 = 32.f;

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
			if (GetWorld()->LineTraceSingleByObjectType(Hit, RestW + N * 30.f, RestW - N * 40.f, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
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
			const FVector Wiggle = N * FMath::Sin(AnimTime * 18.f + i) * 3.f;
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
			if (Dist > 140.f)
			{
				L.Foot = Desired;   // teleported or fell far: snap
			}
			else if (Dist > Threshold && !GroupStepping[1 - L.Group])
			{
				L.From = L.Foot;
				L.To = Desired + Vel * StepDur * 0.6f;
				L.T = 0.f;
				GroupStepping[L.Group] = true;
			}
		}

		// Two-bone IK: knee bends up and outwards.
		const FVector Hip = BodyTM.TransformPosition(L.HipLocal);
		FVector ToFoot = L.Foot - Hip;
		float D = (float)ToFoot.Size();
		const FVector Dir = D > KINDA_SMALL_NUMBER ? ToFoot / D : -N;
		D = FMath::Clamp(D, 4.f, L1 + L2 - 0.5f);
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
			AntArt::PlaceAlong(LegFemurs[i], AntArt::Get(TEXT("SM_Ant_Femur")), Hip, Knee, KneeUp.IsNearlyZero() ? N : KneeUp, 7.5f, ArtFlipFemur, false);
			AntArt::PlaceAlong(LegTibias[i], AntArt::Get(TEXT("SM_Ant_Tibia")), Knee, FootDraw, KneeUp.IsNearlyZero() ? N : KneeUp, 5.5f, ArtFlipTibia, false);
			if (LegCoxas.IsValidIndex(i) && LegCoxas[i])
			{
				const FVector CoxaDir = (Knee - Hip).GetSafeNormal();
				AntArt::PlaceAlong(LegCoxas[i], AntArt::Get(TEXT("SM_Ant_Coxa")), Hip - CoxaDir * 3.f, Hip + CoxaDir * 6.f, N, 8.f, false, false);
			}
		}
		else
		{
			PlaceSegment(LegFemurs[i], Hip, Knee, 7.f);
			PlaceSegment(LegTibias[i], Knee, FootDraw, 5.f);
			if (LegKnees[i])
			{
				LegKnees[i]->SetWorldLocation(Knee);
			}
		}
	}
}
