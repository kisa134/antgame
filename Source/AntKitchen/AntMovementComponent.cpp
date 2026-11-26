#include "AntMovementComponent.h"
#include "AntCarry.h"
#include "AntCharacter.h"
#include "KitchenLoot.h"
#include "KitchenInteractables.h"
#include "GameFramework/Character.h"
#include "AntTypes.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
	P.AddObjectTypesToQuery(ECC_PhysicsBody);   // food and props lying around: climb the cake
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
			// Not for the engine's basic shapes: their body setup is shared by everything built from cubes, spheres and
			// cylinders, and a complex-as-simple body cannot simulate physics (it froze every physics prop and food crumb
			// in the air). Their simple collision is exact anyway.
			if (UBodySetup* BS = Mesh->GetBodySetup(); BS && BS->CollisionTraceFlag != CTF_UseComplexAsSimple && !Mesh->GetPathName().StartsWith(TEXT("/Engine/")))
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
	if (T == ECC_PhysicsBody)
	{
		// Food lying around is a surface; what you (or anyone) carry is not.
		const AKitchenLoot* L = Cast<AKitchenLoot>(C->GetOwner());
		if (L) { return !L->IsCarried() && !L->bHeld; }
		return C->GetOwner() && C->GetOwner()->IsA<AKitchenProp>();
	}
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

namespace
{
	bool RideLog()
	{
		static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("AntRideLog"));
		return bOn;
	}
}

void UAntMovementComponent::OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldLocation, const FHitResult& MoveOnBaseHit)
{
	Super::OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
	if (RideLog() && CharacterOwner && CharacterOwner->IsPlayerControlled())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTRIDE could not follow %s by %.1f: blocked by %s of %s (start penetrating %d)"),
			CharacterOwner->GetMovementBase() ? *CharacterOwner->GetMovementBase()->GetName() : TEXT("-"), DeltaPosition.Size(),
			MoveOnBaseHit.GetComponent() ? *MoveOnBaseHit.GetComponent()->GetName() : TEXT("?"), MoveOnBaseHit.GetActor() ? *MoveOnBaseHit.GetActor()->GetName() : TEXT("?"), MoveOnBaseHit.bStartPenetrating ? 1 : 0);
	}
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
		Push = N * 150.f * AntKit::JumpK;                         // ceiling: just let go
	}
	else
	{
		Push = N * JumpZVelocity * 0.8f + FVector::UpVector * 280.f * AntKit::JumpK;    // wall: kick off and up
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

bool UAntMovementComponent::StepOverLip(const FHitResult& Hit, const FVector& N, const FVector& InDir, const FVector& HitN, float R)
{
	// dev (-AntStuckLog): why a step over a low edge was refused (a few lines a second at most)
	static const bool bLog = FParse::Param(FCommandLine::Get(), TEXT("AntStuckLog"));
	auto No = [this, &Hit](const TCHAR* Why, float V)
	{
		static double LastAt = 0.0;
		static int32 InSecond = 0;
		const double T = FPlatformTime::Seconds();
		if (T - LastAt > 1.0) { LastAt = T; InSecond = 0; }
		if (bLog && ++InSecond <= 4 && CharacterOwner)
		{
			const UPrimitiveComponent* C = Hit.GetComponent();
			const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
			UE_LOG(LogTemp, Display, TEXT("ANTSTEP no %s (%.2f): %s at %s on %s type %d"), Why, V, *CharacterOwner->GetName(), *CharacterOwner->GetActorLocation().ToCompactString(),
				SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : *GetNameSafe(C), C ? (int32)C->GetCollisionObjectType() : -1);
		}
		return false;
	};
	// Into the thing, along the floor; it must be pushing into it at all. (-AntNoStepLip: off, the A/B of the map test.)
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoStepLip"));
	const FVector Into = FVector::VectorPlaneProject(-HitN, N).GetSafeNormal();
	if (bOff || Into.IsNearlyZero() || FVector::DotProduct(InDir, Into) < 0.05f)
	{
		return false;
	}
	if (!IsStickable(Hit))
	{
		return No(TEXT("surface"), 0.f);
	}
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const FVector Feet = Loc - N * R;
	const float MaxLip = R * 5.f;   // the pet mat's lip is 15 high, the ant's ball 4: a step of up to five radii
	// The top of what it ran into, a little way in past its edge: flat, low, and nothing over it.
	const FVector Probe = Loc + Into * (R * 2.5f);
	FHitResult Top;
	if (!TraceSurface(Probe + N * (MaxLip + R * 2.f), Probe - N * R, Top))
	{
		return No(TEXT("top"), 0.f);
	}
	if (Top.bStartPenetrating || !IsStickable(Top) || FVector::DotProduct(Top.ImpactNormal, N) < 0.8f)
	{
		return No(TEXT("flat"), (float)FVector::DotProduct(Top.ImpactNormal, N));
	}
	const float H = (float)FVector::DotProduct(Top.ImpactPoint - Feet, N);
	if (H < R * 0.3f || H > MaxLip)
	{
		return No(TEXT("height"), H);
	}
	// A step back off the lip (an undercut lip hangs over the ant: straight up it would hit it), up, then over, all swept: if
	// any is blocked it stays where it was.
	const FQuat Rot = UpdatedComponent->GetComponentQuat();
	FHitResult Hb;
	SafeMoveUpdatedComponent(-Into * (R * 1.5f), Rot, true, Hb);
	FHitResult Hu;
	SafeMoveUpdatedComponent(N * (H + 0.6f), Rot, true, Hu);
	if (Hu.IsValidBlockingHit() && Hu.Time < 0.95f)
	{
		UpdatedComponent->SetWorldLocation(Loc, false, nullptr, ETeleportType::None);
		return No(TEXT("room above"), Hu.Time);
	}
	FHitResult Ho;
	const FVector Over = Top.ImpactPoint + N * (R + 0.6f) - UpdatedComponent->GetComponentLocation();
	SafeMoveUpdatedComponent(FVector::VectorPlaneProject(Over, N), Rot, true, Ho);
	if (Ho.IsValidBlockingHit() && Ho.Time < 0.3f)
	{
		UpdatedComponent->SetWorldLocation(Loc, false, nullptr, ETeleportType::None);
		return No(TEXT("room over"), Ho.Time);
	}
	if (bLog && CharacterOwner)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTSTEP up %.1f onto %s: %s at %s"), H, *GetNameSafe(Top.GetComponent()), *CharacterOwner->GetName(), *Loc.ToCompactString());
	}
	return true;
}

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
		Detach(N * 80.f * AntKit::JumpK);
		return;
	}

	// 1. Velocity in the surface plane.
	const float MaxAccel = FMath::Max(1.f, GetMaxAcceleration());
	const FVector In = FVector::VectorPlaneProject(Acceleration, N);
	const float InMag = FMath::Min(1.f, (float)In.Size() / MaxAccel);
	const FVector InDir = In.GetSafeNormal();
	const FVector Target = InDir * GetMaxSpeed() * InMag;
	FVector V = FVector::VectorPlaneProject(Velocity, N);
	V = FMath::VInterpConstantTo(V, Target, DeltaTime, InMag > 0.05f ? 4500.f * AntKit::SpeedK : 6000.f * AntKit::SpeedK);
	Velocity = V;

	const FQuat Rot = UpdatedComponent->GetComponentQuat();

	// 2. Move along the surface; bumping into a wall while pushing towards it = climb it.
	// A carrier is held by its grip: the load pulls it (AntCarry::Tether; a correction, not added to the velocity it keeps).
	const FVector Pull = FVector::VectorPlaneProject(AntCarry::Tether(Cast<AAntCharacter>(CharacterOwner), N, DeltaTime), N);
	const FVector Delta = (V + Pull) * DeltaTime;
	if (!Delta.IsNearlyZero(0.01f))
	{
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, Rot, true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			// the face hit, not the capsule's contact normal: at a corner (wall to ceiling, wall to a shelf's underside) the
			// contact normal is a 45-degree blend, and a crawl along a blended normal lost both faces and fell (map v4 test)
			FVector HN = Hit.Normal;
			if (!Hit.ImpactNormal.IsNearlyZero() && FVector::DotProduct(Hit.ImpactNormal, Hit.Normal) > 0.5f)
			{
				HN = Hit.ImpactNormal.GetSafeNormal();
			}
			const bool bNewPlane = FVector::DotProduct(HN, N) < 0.7f;
			if (bNewPlane && N.Z > 0.6f && !bNoClimb && StepOverLip(Hit, N, InDir, HN, R))
			{
				// walked up onto a low edge (a mat, a newspaper, a lid): no climbing its lip face, at any slant (KAN-27)
			}
			else if (bNewPlane && !bNoClimb && IsStickable(Hit) && FVector::DotProduct(InDir, -HN) > 0.25f)
			{
				Velocity = FQuat::FindBetweenNormals(N, HN).RotateVector(V);
				N = HN;
			}
			else
			{
				// An undercut face (the rounded lip of a mat, the belly of a bowl: it leans over the surface we walk on) slid
				// along as it is pushes the ant into its own floor, and it stood wedged in the crease for good (KAN-27: squads
				// stuck at the pet mat and the bowls). Slide along it as if it stood square to our surface.
				FVector SlideN = HN;
				static const bool bOldSlide = FParse::Param(FCommandLine::Get(), TEXT("AntNoStepLip"));
				if (!bOldSlide && FVector::DotProduct(HN, N) < -0.05f)
				{
					const FVector Square = FVector::VectorPlaneProject(HN, N).GetSafeNormal();
					if (!Square.IsNearlyZero()) { SlideN = Square; }
				}
				SlideAlongSurface(Delta, 1.f - Hit.Time, SlideN, Hit, true);
			}
		}
	}

	// 3. Stay glued to the surface, or wrap around a convex edge.
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	FHitResult G;
	// a thin ray first; then a small ball along the same line, which also finds the narrow bevel of a frame, an outlet or
	// a wall-to-ceiling corner that a ray slips past (map v4 test: ants dropped off walls at such corners)
	bool bGlued = TraceSurface(Loc + N * 2.f, Loc - N * (R + 28.f * AntKit::AntShrink), G) && IsStickable(G);
	if (!bGlued && !bNoClimb)
	{
		FCollisionQueryParams Ps(SCENE_QUERY_STAT(AntCrawlBall), false, CharacterOwner);
		bGlued = GetWorld()->SweepSingleByObjectType(G, Loc + N * 2.f, Loc - N * (R + 20.f * AntKit::AntShrink), FQuat::Identity, SurfaceObjects(), FCollisionShape::MakeSphere(R * 0.6f), Ps)
			&& IsStickable(G) && FVector::DotProduct(G.ImpactNormal, N) > 0.3f;
	}
	if (bGlued)
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
			const FVector Below = Loc - N * (R + 14.f * AntKit::AntShrink);
			FHitResult W;
			if (TraceSurface(Below, Below - MoveDir * (R * 2.f + 40.f * AntKit::AntShrink), W) && IsStickable(W)
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
		if (!bWrapped && !bNoClimb)
		{
			// still a blended corner normal: the real face lies along its strongest axis (wall or ceiling); hold on to that
			const FVector A = FVector(FMath::Abs(N.X) >= FMath::Abs(N.Y) && FMath::Abs(N.X) >= FMath::Abs(N.Z) ? FMath::Sign(N.X) : 0.f,
				FMath::Abs(N.Y) > FMath::Abs(N.X) && FMath::Abs(N.Y) >= FMath::Abs(N.Z) ? FMath::Sign(N.Y) : 0.f,
				FMath::Abs(N.Z) > FMath::Abs(N.X) && FMath::Abs(N.Z) > FMath::Abs(N.Y) ? FMath::Sign(N.Z) : 0.f);
			FHitResult G2;
			if (!A.IsNearlyZero() && FVector::DotProduct(A, N) < 0.97f && TraceSurface(Loc + A * 2.f, Loc - A * (R + 34.f * AntKit::AntShrink), G2) && IsStickable(G2))
			{
				N = G2.ImpactNormal.GetSafeNormal();
				const float Gap = (float)FVector::DotProduct(Loc - G2.ImpactPoint, N) - R - 0.5f;
				if (Gap > 0.2f)
				{
					FHitResult H3;
					SafeMoveUpdatedComponent(-N * Gap, Rot, true, H3);
				}
				SetCrawlBase(G2.GetComponent(), G2.BoneName);
				bWrapped = true;
			}
		}
		if (!bWrapped)
		{
			if (RideLog() && CharacterOwner && CharacterOwner->IsPlayerControlled() && GetRiddenComponent())
			{
				UE_LOG(LogTemp, Display, TEXT("ANTRIDE lost the surface of %s (normal %s, speed %.0f)"), *GetRiddenComponent()->GetName(), *N.ToCompactString(), Velocity.Size());
			}
			SurfaceNormal = N;
			Detach(N * 40.f * AntKit::JumpK);
			return;
		}
	}

	SurfaceNormal = N.GetSafeNormal();
}
