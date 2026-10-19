#include "CreatureCharacters.h"
#include "AntMovementComponent.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "HumanCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	FAntWeapon FlySpit()
	{
		FAntWeapon W;
		W.Name = TEXT("Мушиный плевок");
		W.Damage = 5.f;
		W.FireRate = 3.f;
		W.ProjectileSpeed = 2200.f;
		W.Spread = 3.f;
		W.Size = 0.08f;
		return W;
	}

	FAntWeapon SpiderSilk()
	{
		FAntWeapon W;
		W.Name = TEXT("Паутина");
		W.Damage = 8.f;
		W.FireRate = 0.9f;
		W.ProjectileSpeed = 1900.f;
		W.Gravity = 0.45f;
		W.Splash = 110.f;
		W.Size = 0.3f;
		W.bWeb = true;
		return W;
	}

	UStaticMeshComponent* Shape(AActor* Owner, USceneComponent* Parent, EAntShape S, const FVector& L, const FVector& Sc, const FLinearColor& C, const FRotator& R = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* M = AntKit::AddPart(Owner, Parent, S, L, Sc, C, R);
		M->SetCastShadow(true);
		return M;
	}
}

// =============================================================================================
// Fly
// =============================================================================================

AFlyCharacter::AFlyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Creature = 1;
	ExtraScale = 1.7f;
	HealthOverride = 60.f;
	StrengthOverride = 10.f;      // lifts crumbs, sugar cubes, a gummy
	LegLength = 1.15f;
	LegThickness = 0.5f;
	LegUpperArt = TEXT("SM_Fly_LegUpper");
	LegLowerArt = TEXT("SM_Fly_LegLower");
	Weapon = FlySpit();
	BodyColor = FLinearColor(0.08f, 0.08f, 0.09f);
	GetCapsuleComponent()->InitCapsuleSize(BodyRadius * 1.5f, BodyRadius * 1.5f);

	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->DefaultLandMovementMode = MOVE_Flying;
	CM->MaxFlySpeed = 950.f;
	CM->BrakingDecelerationFlying = 2200.f;
	CM->MaxAcceleration = 3200.f;
}

void AFlyCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* Buzz = AntSfx::Get(AntSfx::Buzz))
		{
			BuzzAudio = UGameplayStatics::SpawnSoundAttached(Buzz, RootComponent, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 0.7f);
		}
	}
}

void AFlyCharacter::BuildBody()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USceneComponent* R = VisualRoot;
	const FLinearColor Grey(0.16f, 0.16f, 0.17f), Dark(0.04f, 0.04f, 0.05f), Eye(0.55f, 0.06f, 0.03f), WingC(0.75f, 0.8f, 0.85f);

	const AntArt::FPart& ArtBody = AntArt::Get(TEXT("SM_Fly_Body"));
	if (ArtBody.IsValid())
	{
		UStaticMeshComponent* B = AntArt::AddArt(this, R, ArtBody);
		B->SetRelativeTransform(AntArt::Fit(ArtBody, FVector(0.f, 0.f, 8.f), 95.f, false));
		B->SetCastShadow(true);
	}
	else
	{
		Shape(this, R, EAntShape::Sphere, FVector(2.f, 0.f, 8.f), FVector(0.36f, 0.32f, 0.30f), Grey);                     // thorax
		Shape(this, R, EAntShape::Cube, FVector(4.f, 5.f, 22.f), FVector(0.26f, 0.025f, 0.02f), Dark);                     // stripes
		Shape(this, R, EAntShape::Cube, FVector(4.f, -5.f, 22.f), FVector(0.26f, 0.025f, 0.02f), Dark);
		Shape(this, R, EAntShape::Sphere, FVector(-26.f, 0.f, 5.f), FVector(0.42f, 0.33f, 0.27f), FLinearColor(0.22f, 0.2f, 0.15f));   // abdomen
		for (int32 i = 0; i < 3; ++i)
		{
			Shape(this, R, EAntShape::Sphere, FVector(-18.f - i * 9.f, 0.f, 5.f), FVector(0.05f, 0.34f - i * 0.04f, 0.28f - i * 0.03f), Dark);   // bands
		}
		Shape(this, R, EAntShape::Sphere, FVector(24.f, 0.f, 8.f), FVector(0.22f, 0.30f, 0.24f), Grey);                    // head
		for (int32 s = -1; s <= 1; s += 2)
		{
			Shape(this, R, EAntShape::Sphere, FVector(27.f, 10.f * s, 11.f), FVector(0.15f, 0.13f, 0.19f), Eye);            // compound eyes
		}
		Shape(this, R, EAntShape::Cylinder, FVector(31.f, 0.f, -3.f), FVector(0.04f, 0.04f, 0.14f), Dark, FRotator(-25.f, 0.f, 0.f));   // proboscis
	}

	// Wings on pivots: they beat ~fast in Tick.
	const AntArt::FPart& ArtWing = AntArt::Get(TEXT("SM_Fly_Wing"));
	for (int32 s = -1; s <= 1; s += 2)
	{
		USceneComponent* P = AntKit::AddPivot(this, R, FVector(4.f, 7.f * s, 21.f), FRotator(0.f, -25.f * s, 0.f));
		if (ArtWing.IsValid())
		{
			UStaticMeshComponent* W = AntArt::AddArt(this, P, ArtWing);
			W->SetRelativeTransform(AntArt::Fit(ArtWing, FVector(-20.f, 22.f * s, 0.f), 52.f, false, FRotator(0.f, 70.f * s, 0.f)));
			W->SetCastShadow(false);
		}
		else
		{
			UStaticMeshComponent* W = Shape(this, P, EAntShape::Sphere, FVector(-18.f, 22.f * s, 0.f), FVector(0.50f, 0.20f, 0.01f), WingC);
			W->SetCastShadow(false);
			Shape(this, P, EAntShape::Cylinder, FVector(-14.f, 18.f * s, 0.5f), FVector(0.006f, 0.006f, 0.42f), Dark, FRotator(90.f, 0.f, 0.f) + FRotator(0.f, 20.f * s, 0.f));   // vein
		}
		WingPivots.Add(P);
	}

	// Six thin hairy legs.
	const float HipX[3] = { 10.f, 3.f, -5.f };
	const float RestX[3] = { 30.f, 4.f, -24.f };
	const AntArt::FPart& LU = AntArt::Get(LegUpperArt);
	const AntArt::FPart& LL = AntArt::Get(LegLowerArt);
	bArtLegs = LU.IsValid() && LL.IsValid();
	for (int32 i = 0; i < 3; ++i)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			FAntLeg L;
			L.HipLocal = FVector(HipX[i], 7.f * s, -4.f);
			L.RestLocal = FVector(RestX[i], 32.f * s, -24.f);
			L.Side = s;
			L.Group = (i + (s > 0 ? 1 : 0)) % 2;
			Legs.Add(L);
			if (bArtLegs)
			{
				LegFemurs.Add(AntArt::AddArt(this, R, LU));
				LegTibias.Add(AntArt::AddArt(this, R, LL));
			}
			else
			{
				LegFemurs.Add(AntKit::AddPart(this, R, EAntShape::Cylinder, FVector::ZeroVector, FVector(0.05f), Dark));
				LegTibias.Add(AntKit::AddPart(this, R, EAntShape::Cylinder, FVector::ZeroVector, FVector(0.04f), Dark));
			}
			LegKnees.Add(nullptr);
			LegCoxas.Add(nullptr);
		}
	}
	BuildStatusFx();
}

void AFlyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UCharacterMovementComponent* CM = GetCharacterMovement();
	if (!bDead)
	{
		// Always airborne (a knock-back makes it fall for a moment).
		if ((HasAuthority() || IsLocallyControlled()) && CM->MovementMode != MOVE_Flying && CM->Velocity.Z > -50.f)
		{
			CM->SetMovementMode(MOVE_Flying);
		}
		if (IsLocallyControlled() || (HasAuthority() && !IsPlayerControlled()))
		{
			if (bAscend) { AddMovementInput(FVector::UpVector, 1.f); }
			if (bDescend) { AddMovementInput(-FVector::UpVector, 1.f); }
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector V = GetVelocity();
	const float Speed = (float)V.Size();
	FlapTime += DeltaSeconds;
	const float Beat = bDead ? 0.f : FMath::Sin(FlapTime * 95.f);   // too fast to see: a blur, like a real fly
	for (int32 i = 0; i < WingPivots.Num(); ++i)
	{
		const float s = i == 0 ? -1.f : 1.f;
		WingPivots[i]->SetRelativeRotation(FRotator(0.f, -25.f * s, (bDead ? 10.f : 35.f * Beat) * s));
	}
	if (!bDead && !IsCrawling())
	{
		// Bank and pitch with the flight, hover wobble.
		const FVector Local = GetActorTransform().InverseTransformVectorNoScale(V);
		const float Pitch = FMath::Clamp((float)-Local.X * 0.015f, -20.f, 20.f) + FMath::Clamp((float)-V.Z * 0.01f, -10.f, 10.f);
		const float Roll = FMath::Clamp((float)Local.Y * 0.03f, -30.f, 30.f);
		const FVector Wobble(0.f, 0.f, FMath::Sin(FlapTime * 7.f) * 2.f);
		VisualRoot->SetWorldLocationAndRotation(GetActorLocation() + Wobble, FRotator(Pitch, GetActorRotation().Yaw, Roll));
	}
	if (BuzzAudio)
	{
		BuzzAudio->SetPitchMultiplier(0.9f + FMath::Min(Speed / 900.f, 1.f) * 0.35f);
		if (bDead && BuzzAudio->IsPlaying()) { BuzzAudio->Stop(); }
		if (!bDead && !BuzzAudio->IsPlaying()) { BuzzAudio->Play(); }
	}
}

FVector AFlyCharacter::GetHoldPoint() const
{
	// Food dangles under the fly's legs.
	return GetActorLocation() - FVector(0.f, 0.f, BodyRadius * 1.5f + 22.f);
}

void AFlyCharacter::MoveForward(float Value)
{
	if (!Controller || bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}
	if (IsCrawling())
	{
		Super::MoveForward(Value);
		return;
	}
	AddMovementInput(Controller->GetControlRotation().Vector(), Value);   // fly where you look
}

void AFlyCharacter::MoveRight(float Value)
{
	if (!Controller || bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}
	if (IsCrawling())
	{
		Super::MoveRight(Value);
		return;
	}
	AddMovementInput(FRotationMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y), Value);
}

void AFlyCharacter::DoJump()
{
	if (bDead)
	{
		return;
	}
	if (IsCrawling())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);   // take off
		LaunchCharacter(FVector(0.f, 0.f, 400.f), false, true);
	}
	bAscend = true;
}

void AFlyCharacter::DoStopJump()
{
	bAscend = false;
}

// =============================================================================================
// Spider
// =============================================================================================

ASpiderCharacter::ASpiderCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Creature = 2;
	Team = 2;
	ExtraScale = 2.4f;
	SpeedScale = 1.25f;
	DamageScale = 2.2f;
	HealthOverride = 420.f;
	StrengthOverride = 0.f;       // doesn't carry food, it eats ants
	LegLength = 2.1f;
	LegThickness = 0.9f;
	LegUpperArt = TEXT("SM_Spider_LegUpper");
	LegLowerArt = TEXT("SM_Spider_LegLower");
	Weapon = SpiderSilk();
	BodyColor = FLinearColor(0.09f, 0.06f, 0.04f);
	GetCapsuleComponent()->InitCapsuleSize(BodyRadius * 2.f, BodyRadius * 2.f);
	GetCharacterMovement()->GravityScale = 0.55f;   // floats down on its silk
}

void ASpiderCharacter::BuildBody()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USceneComponent* R = VisualRoot;
	const FLinearColor Brown(0.16f, 0.1f, 0.06f), Dark(0.05f, 0.035f, 0.025f), Light(0.45f, 0.33f, 0.2f), Eye(0.01f, 0.01f, 0.01f);

	const AntArt::FPart& ArtBody = AntArt::Get(TEXT("SM_Spider_Body"));
	if (ArtBody.IsValid())
	{
		UStaticMeshComponent* B = AntArt::AddArt(this, R, ArtBody);
		B->SetRelativeTransform(AntArt::Fit(ArtBody, FVector(-12.f, 0.f, 10.f), 100.f, false));
		B->SetCastShadow(true);
	}
	else
	{
		Shape(this, R, EAntShape::Sphere, FVector(10.f, 0.f, 6.f), FVector(0.40f, 0.34f, 0.22f), Brown);            // cephalothorax
		Shape(this, R, EAntShape::Sphere, FVector(-30.f, 0.f, 13.f), FVector(0.58f, 0.48f, 0.44f), Brown);          // abdomen
		for (int32 i = 0; i < 4; ++i)   // chevron pattern on the abdomen
		{
			Shape(this, R, EAntShape::Sphere, FVector(-18.f - i * 9.f, 0.f, 34.f - i * 1.5f), FVector(0.07f, 0.26f - i * 0.04f, 0.03f), Light);
		}
		Shape(this, R, EAntShape::Sphere, FVector(-4.f, 0.f, 6.f), FVector(0.1f), Dark);                             // pedicel
		// Eight eyes in two rows.
		const FVector Eyes[8] = { FVector(28.f, 4.f, 12.f), FVector(28.f, -4.f, 12.f), FVector(26.f, 9.f, 11.f), FVector(26.f, -9.f, 11.f),
			FVector(25.f, 3.f, 16.f), FVector(25.f, -3.f, 16.f), FVector(23.f, 7.f, 15.f), FVector(23.f, -7.f, 15.f) };
		for (int32 i = 0; i < 8; ++i)
		{
			Shape(this, R, EAntShape::Sphere, Eyes[i], FVector(i < 2 ? 0.07f : 0.045f), Eye);
		}
		for (int32 s = -1; s <= 1; s += 2)
		{
			Shape(this, R, EAntShape::Cone, FVector(31.f, 4.f * s, -2.f), FVector(0.08f, 0.08f, 0.16f), Dark, FRotator(150.f, 0.f, 0.f));   // fangs
			Shape(this, R, EAntShape::Cylinder, FVector(30.f, 9.f * s, 2.f), FVector(0.04f, 0.04f, 0.18f), Brown, FRotator(-60.f, 20.f * s, 0.f));   // pedipalps
		}
	}

	// Eight legs around the cephalothorax, long and spread wide.
	const float HipX[4] = { 18.f, 12.f, 6.f, 0.f };
	const float RestX[4] = { 58.f, 22.f, -14.f, -48.f };
	const float RestY[4] = { 46.f, 58.f, 58.f, 46.f };
	const AntArt::FPart& LU = AntArt::Get(LegUpperArt);
	const AntArt::FPart& LL = AntArt::Get(LegLowerArt);
	bArtLegs = LU.IsValid() && LL.IsValid();
	for (int32 i = 0; i < 4; ++i)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			FAntLeg L;
			L.HipLocal = FVector(HipX[i], 9.f * s, 2.f);
			L.RestLocal = FVector(RestX[i], RestY[i] * s, -22.f);
			L.Side = s;
			L.Group = (i + (s > 0 ? 1 : 0)) % 2;
			Legs.Add(L);
			if (bArtLegs)
			{
				LegFemurs.Add(AntArt::AddArt(this, R, LU));
				LegTibias.Add(AntArt::AddArt(this, R, LL));
				LegKnees.Add(nullptr);
			}
			else
			{
				LegFemurs.Add(AntKit::AddPart(this, R, EAntShape::Cylinder, FVector::ZeroVector, FVector(0.07f), Brown));
				LegTibias.Add(AntKit::AddPart(this, R, EAntShape::Cylinder, FVector::ZeroVector, FVector(0.05f), Dark));
				LegKnees.Add(AntKit::AddPart(this, R, EAntShape::Sphere, FVector::ZeroVector, FVector(0.075f), Dark));
			}
			LegCoxas.Add(nullptr);
		}
	}

	// The silk thread (only while dropping from a ceiling).
	Silk = AntKit::AddPart(this, R, EAntShape::Cylinder, FVector::ZeroVector, FVector(0.02f), FLinearColor(0.9f, 0.9f, 0.95f));
	Silk->SetUsingAbsoluteLocation(true);
	Silk->SetUsingAbsoluteRotation(true);
	Silk->SetUsingAbsoluteScale(true);
	Silk->SetCastShadow(false);
	Silk->SetVisibility(false);
	BuildStatusFx();
}

void ASpiderCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || !Silk)
	{
		return;
	}
	// Let go of a ceiling -> hang on a thread from where you were.
	if (IsCrawling())
	{
		LastCrawlNormal = GetVisualUp();
		bOnSilk = false;
	}
	else if (!bOnSilk && LastCrawlNormal.Z < -0.5f && !bDead)
	{
		bOnSilk = true;
		SilkAnchor = GetActorLocation() - LastCrawlNormal * BodyRadius * 2.2f;
		LastCrawlNormal = FVector::UpVector;
	}
	if (bOnSilk && !bDead)
	{
		const FVector A = SilkAnchor, B = GetActorLocation();
		const float Len = (float)FVector::Dist(A, B);
		Silk->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(B - A).ToQuat());
		Silk->SetWorldScale3D(FVector(0.015f, 0.015f, Len / 100.f));
		Silk->SetVisibility(true);
	}
	else
	{
		Silk->SetVisibility(false);
	}
}

// =============================================================================================
// Fly AI
// =============================================================================================

AFlyAIController::AFlyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
	bSetControlRotationFromPawnOrientation = false;
	Seed = FMath::FRand() * 100.f;
}

void AFlyAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (APawn* P = GetPawn())
	{
		const FRotator R = FMath::RInterpTo(GetControlRotation(), Desired, DeltaTime, 6.f);
		SetControlRotation(R);
		if (bUpdatePawn) { P->FaceRotation(R, DeltaTime); }
	}
}

void AFlyAIController::PickWander(const FVector& From)
{
	const FBox A = AKitchenBuilder::PlayArea();
	Goal = FVector(FMath::FRandRange(A.Min.X, A.Max.X), FMath::FRandRange(A.Min.Y, A.Max.Y), FMath::FRandRange(500.f, 3600.f));
	StateTime = 0.f;
}

void AFlyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AFlyCharacter* Fly = Cast<AFlyCharacter>(GetPawn());
	if (!Fly || Fly->bDead)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector Me = Fly->GetActorLocation();
	StateTime += DeltaSeconds;

	// Hurt: zoom away up high.
	if (LastHealth >= 0.f && Fly->Health < LastHealth - 0.5f)
	{
		FleeUntil = Now + 2.5f;
		Goal = Me + FVector(FMath::FRandRange(-1500.f, 1500.f), FMath::FRandRange(-1500.f, 1500.f), 1200.f);
		Goal.Z = FMath::Min((float)Goal.Z, 4800.f);
	}
	LastHealth = Fly->Health;

	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f && Now > FleeUntil)
	{
		ThinkTimer = 0.4f;
		if (Fly->Carrying)
		{
			// Carry it somewhere high and drop it: it smashes.
			if (StateTime > 6.f || FVector::Dist(Me, Goal) < 200.f)
			{
				Fly->LetGo();
				NextSteal = Now + FMath::FRandRange(10.f, 18.f);
				PickWander(Me);
			}
		}
		else if (AKitchenLoot* L = Target.Get())
		{
			if (L->bIsGear || L->bHeld || L->Weight > 10 || (L->IsCarried() && L->CarryTeam() != Fly->Team))
			{
				Target = nullptr;
			}
			else if (FVector::Dist(Me, L->GetActorLocation()) < 90.f + L->GetRadius())
			{
				if (Fly->GrabLoot(L))
				{
					Goal = FVector(Me.X + FMath::FRandRange(-3000.f, 3000.f), Me.Y + FMath::FRandRange(-3000.f, 3000.f), FMath::FRandRange(2500.f, 4200.f));
					StateTime = 0.f;
				}
				Target = nullptr;
			}
			else
			{
				Goal = L->GetActorLocation() + FVector(0.f, 0.f, 40.f);
			}
		}
		else if (Now > NextSteal)
		{
			// Look for a light snack nobody is holding.
			float Best = 4500.f;
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				AKitchenLoot* Cand = *It;
				if (!Cand || Cand->bIsGear || Cand->bHeld || Cand->Weight > 10 || Cand->IsCarried())
				{
					continue;
				}
				const float D = (float)FVector::Dist(Cand->GetActorLocation(), Me);
				if (D < Best) { Best = D; Target = Cand; }
			}
			NextSteal = Now + 3.f;
		}
		else if (StateTime > 7.f || FVector::Dist(Me, Goal) < 250.f)
		{
			// Sometimes buzz around the Human's head, otherwise wander.
			AHumanCharacter* H = nullptr;
			for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It) { H = *It; break; }
			if (H && FMath::FRand() < 0.35f)
			{
				Goal = H->GetActorLocation() + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 1300.f);
				StateTime = 0.f;
			}
			else
			{
				PickWander(Me);
			}
		}
	}

	// Fly there in an erratic zig-zag, avoid walls.
	FVector To = Goal - Me;
	const float T = Now + Seed;
	const FVector Jitter(FMath::Sin(T * 3.1f), FMath::Cos(T * 2.3f), FMath::Sin(T * 4.7f) * 0.6f);
	FVector Dir = To.GetSafeNormal() + Jitter * (To.Size() > 300.f ? 0.45f : 0.15f);
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(FlyAvoid), false, Fly);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Me, Me + Fly->GetVelocity().GetSafeNormal() * 260.f, FCollisionObjectQueryParams(ECC_WorldStatic), P))
	{
		Dir += Hit.ImpactNormal * 1.5f + FVector(0.f, 0.f, 0.5f);
	}
	Fly->AddMovementInput(Dir.GetSafeNormal(), 1.f);
	const FVector Face = Fly->GetVelocity().SizeSquared2D() > 100.f ? Fly->GetVelocity() : To;
	Desired = FRotator(0.f, (float)Face.Rotation().Yaw, 0.f);
}
