#include "AntMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

UAntMovementComponent::UAntMovementComponent()
{
	bOrientRotationToMovement = false;
	NavAgentProps.bCanFly = true;
}

float UAntMovementComponent::Now() const
{
	const UWorld* W = GetWorld();
	return W ? W->GetTimeSeconds() : 0.f;
}

void UAntMovementComponent::BlockSticking(float Seconds)
{
	NoStickUntil = FMath::Max(NoStickUntil, Now() + Seconds);
	bPendingStick = false;
}

void UAntMovementComponent::StartCrawl(const FVector& Normal)
{
	SurfaceNormal = Normal.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = FVector::UpVector;
	}
	if (!IsCrawling())
	{
		SetMovementMode(MOVE_Custom, ANTMOVE_Crawl);
	}
	Velocity = FVector::VectorPlaneProject(Velocity, SurfaceNormal);
}

void UAntMovementComponent::Detach(const FVector& Push)
{
	Velocity += Push;
	SetMovementMode(MOVE_Falling);
	BlockSticking(0.25f);
}

FCollisionObjectQueryParams UAntMovementComponent::SurfaceObjects()
{
	FCollisionObjectQueryParams P(ECC_WorldStatic);
	P.AddObjectTypesToQuery(ECC_AntClimbable);
	return P;
}

void UAntMovementComponent::MakeClimbable(UPrimitiveComponent* C)
{
	if (!C)
	{
		return;
	}
	// Ants crawl on exactly what is drawn: the mesh's own triangles are its collision.
	if (UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C))
	{
		if (UStaticMesh* Mesh = SM->GetStaticMesh())
		{
			if (UBodySetup* BS = Mesh->GetBodySetup(); BS && BS->CollisionTraceFlag != CTF_UseComplexAsSimple)
			{
				BS->CollisionTraceFlag = CTF_UseComplexAsSimple;
				SM->RecreatePhysicsState();
			}
		}
	}
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionObjectType(ECC_AntClimbable);
	C->SetCollisionResponseToAllChannels(ECR_Ignore);
	C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);         // ants, spider bump into it and climb
	C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);   // you can aim at it
	C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);           // no physics: it never shoves food around
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->ComponentTags.AddUnique(TEXT("AntClimbable"));
}

UPrimitiveComponent* UAntMovementComponent::GetRiddenComponent() const
{
	UPrimitiveComponent* B = CharacterOwner ? CharacterOwner->GetMovementBase() : nullptr;
	return (B && B->GetCollisionObjectType() == ECC_AntClimbable) ? B : nullptr;
}

bool UAntMovementComponent::TraceSurface(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntCrawl), false, CharacterOwner);
	return GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, SurfaceObjects(), Params);
}

bool UAntMovementComponent::IsStickable(const FHitResult& Hit) const
{
	const UPrimitiveComponent* C = Hit.GetComponent();
	if (!C || (CharacterOwner && C->GetOwner() == CharacterOwner))
	{
		return false;
	}
	const ECollisionChannel T = C->GetCollisionObjectType();
	return T == ECC_WorldStatic || T == ECC_AntClimbable;
}

void UAntMovementComponent::SetCrawlBase(UPrimitiveComponent* C, FName Bone)
{
	// Only climbable moving bodies become a base: the ant then moves and turns with them,
	// and its position replicates relative to them. The static kitchen needs none of that.
	UPrimitiveComponent* Want = (C && C->GetCollisionObjectType() == ECC_AntClimbable && C->Mobility == EComponentMobility::Movable) ? C : nullptr;
	if (CharacterOwner && (CharacterOwner->GetMovementBase() != Want || CharacterOwner->GetBasedMovement().BoneName != (Want ? Bone : NAME_None)))
	{
		SetBase(Want, Want ? Bone : NAME_None);
	}
}

void UAntMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	if (IsCrawling() && HasValidData())
	{
		const UPrimitiveComponent* B = CharacterOwner->GetMovementBase();
		FVector NewLoc;
		FQuat NewQuat;
		if (MovementBaseUtility::UseRelativeLocation(B)
			&& MovementBaseUtility::GetMovementBaseTransform(B, CharacterOwner->GetBasedMovement().BoneName, NewLoc, NewQuat)
			&& !OldBaseQuat.Equals(NewQuat, 1e-6f))
		{
			SurfaceNormal = (NewQuat * OldBaseQuat.Inverse()).RotateVector(SurfaceNormal).GetSafeNormal();
		}
	}
	Super::UpdateBasedMovement(DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------

float UAntMovementComponent::GetMaxSpeed() const
{
	float Base;
	if (IsCrawling())
	{
		Base = MaxWalkSpeed;
		const float Up = SurfaceNormal.Z;
		if (Up < 0.6f)
		{
			Base *= (Up < -0.5f) ? 0.7f : 0.82f;   // walls are slower, ceilings slower still
		}
	}
	else
	{
		Base = Super::GetMaxSpeed();
	}
	return Base * SpeedMultiplier;
}

bool UAntMovementComponent::CanAttemptJump() const
{
	if (bNoJump)
	{
		return false;
	}
	if (IsCrawling())
	{
		return IsJumpAllowed() && !bWantsToCrouch;
	}
	return Super::CanAttemptJump();
}

bool UAntMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (!IsCrawling())
	{
		return Super::DoJump(bReplayingMoves, DeltaTime);
	}
	if (!CharacterOwner || !CharacterOwner->CanJump())
	{
		return false;
	}

	const FVector N = SurfaceNormal;
	FVector Push;
	if (N.Z > 0.6f)
	{
		Push = N * JumpZVelocity;                                        // floor: normal jump
	}
	else if (N.Z < -0.5f)
	{
		Push = N * 150.f;                                                // ceiling: just let go
	}
	else
	{
		Push = N * JumpZVelocity * 0.8f + FVector::UpVector * 280.f;    // wall: kick off and up
	}
	Velocity = FVector::VectorPlaneProject(Velocity, N) + Push;
	SetMovementMode(MOVE_Falling);
	BlockSticking(0.2f);
	return true;
}

void UAntMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
	if (IsFalling() && !bNoClimb && Now() >= NoStickUntil && IsStickable(Hit))
	{
		bPendingStick = true;
		PendingStickNormal = Hit.Normal;
	}
}

void UAntMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		// Landed on a floor: switch to crawling right away.
		FVector FN = CurrentFloor.HitResult.ImpactNormal;
		if (!CurrentFloor.IsWalkableFloor() || FN.IsNearlyZero())
		{
			FN = FVector::UpVector;
		}
		StartCrawl(FN);
	}
	else if (IsFalling() && bPendingStick)
	{
		bPendingStick = false;
		if (Now() >= NoStickUntil)
		{
			StartCrawl(PendingStickNormal);   // jumped onto a wall or the underside of something
		}
	}
	else if (!IsFalling())
	{
		bPendingStick = false;
	}
}

void UAntMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == ANTMOVE_Crawl)
	{
		PhysCrawl(DeltaTime);
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

// ---------------------------------------------------------------------------------------------
// Crawling
// ---------------------------------------------------------------------------------------------

void UAntMovementComponent::PhysCrawl(float DeltaTime)
{
	if (DeltaTime < MIN_TICK_TIME || !CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const float R = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector N = SurfaceNormal;

	// Frozen / no-climb ants can't hold on to walls and ceilings.
	if ((bNoClimb && N.Z < 0.5f) || (bHeavyLoad && N.Z < -0.5f))
	{
		Detach(N * 80.f);
		return;
	}

	// 1. Velocity in the surface plane.
	const float MaxAccel = FMath::Max(1.f, GetMaxAcceleration());
	const FVector In = FVector::VectorPlaneProject(Acceleration, N);
	const float InMag = FMath::Min(1.f, (float)In.Size() / MaxAccel);
	const FVector InDir = In.GetSafeNormal();
	const FVector Target = InDir * GetMaxSpeed() * InMag;
	FVector V = FVector::VectorPlaneProject(Velocity, N);
	V = FMath::VInterpConstantTo(V, Target, DeltaTime, InMag > 0.05f ? 4500.f : 6000.f);
	Velocity = V;

	const FQuat Rot = UpdatedComponent->GetComponentQuat();

	// 2. Move along the surface; bumping into a wall while pushing towards it = climb it.
	const FVector Delta = V * DeltaTime;
	if (!Delta.IsNearlyZero(0.01f))
	{
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, Rot, true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			const FVector HN = Hit.Normal;
			const bool bNewPlane = FVector::DotProduct(HN, N) < 0.7f;
			if (bNewPlane && !bNoClimb && IsStickable(Hit) && FVector::DotProduct(InDir, -HN) > 0.25f)
			{
				Velocity = FQuat::FindBetweenNormals(N, HN).RotateVector(V);
				N = HN;
			}
			else
			{
				SlideAlongSurface(Delta, 1.f - Hit.Time, HN, Hit, true);
			}
		}
	}

	// 3. Stay glued to the surface, or wrap around a convex edge.
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	FHitResult G;
	if (TraceSurface(Loc + N * 2.f, Loc - N * (R + 28.f), G) && IsStickable(G))
	{
		if (FVector::DotProduct(G.ImpactNormal, N) > 0.3f)
		{
			N = G.ImpactNormal;
		}
		const float Gap = (float)FVector::DotProduct(Loc - G.ImpactPoint, N) - R - 0.5f;
		if (Gap > 0.2f)
		{
			FHitResult H2;
			SafeMoveUpdatedComponent(-N * Gap, Rot, true, H2);
		}
		SetCrawlBase(G.GetComponent(), G.BoneName);
	}
	else
	{
		bool bWrapped = false;
		const FVector MoveDir = Velocity.GetSafeNormal();
		if (!MoveDir.IsNearlyZero() && !bNoClimb)
		{
			const FVector Below = Loc - N * (R + 14.f);
			FHitResult W;
			if (TraceSurface(Below, Below - MoveDir * (R * 2.f + 40.f), W) && IsStickable(W)
				&& FVector::DotProduct(W.ImpactNormal, N) < 0.8f)
			{
				const FVector NewN = W.ImpactNormal;
				UpdatedComponent->SetWorldLocation(W.ImpactPoint + NewN * (R + 0.5f), false, nullptr, ETeleportType::None);
				Velocity = FQuat::FindBetweenNormals(N, NewN).RotateVector(Velocity);
				N = NewN;
				bWrapped = true;
				SetCrawlBase(W.GetComponent(), W.BoneName);
			}
		}
		if (!bWrapped)
		{
			SurfaceNormal = N;
			Detach(N * 40.f);
			return;
		}
	}

	SurfaceNormal = N.GetSafeNormal();
}
