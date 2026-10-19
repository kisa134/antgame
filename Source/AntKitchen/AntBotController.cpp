#include "AntBotController.h"
#include "AntCharacter.h"
#include "AntMovementComponent.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AAntBotController::AAntBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
	bSetControlRotationFromPawnOrientation = false;
}

void AAntBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	FoodTarget = nullptr;
	ShootTarget = nullptr;
	bHasGoal = false;
	HeavyTimer = 0.f;
	ThinkTimer = FMath::FRandRange(0.f, 0.4f);
	if (InPawn)
	{
		LastCheckPos = InPawn->GetActorLocation();
		Desired = InPawn->GetActorRotation();
	}
}

void AAntBotController::SetOrder(uint8 Mode, AAntCharacter* By, const FVector& Point)
{
	OrderMode = Mode;
	OrderBy = By;
	OrderPoint = Point;
	OrderUntil = GetWorld()->GetTimeSeconds() + (Mode == 2 ? 40.f : 180.f);
	if (Mode != 0)
	{
		if (AAntCharacter* Me = Cast<AAntCharacter>(GetPawn()); Me && Me->Carrying && Mode == 2) { Me->LetGo(); }
		FoodTarget = nullptr;
	}
}

void AAntBotController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (APawn* P = GetPawn())
	{
		const FRotator R = FMath::RInterpTo(GetControlRotation(), Desired, DeltaTime, 8.f);
		SetControlRotation(R);
		if (bUpdatePawn)
		{
			P->FaceRotation(R, DeltaTime);
		}
	}
}

bool AAntBotController::CanSee(AAntCharacter* Ant, const AActor* Target) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSight), false, Ant);
	Params.AddIgnoredActor(Target);
	FVector To = Target->GetActorLocation();
	if (const AHumanCharacter* H = Cast<AHumanCharacter>(Target))
	{
		To.Z = FMath::Min(To.Z, Ant->GetActorLocation().Z + 600.f);   // aim at the legs
	}
	return !GetWorld()->LineTraceSingleByObjectType(Hit, Ant->GetMuzzleLocation(), To, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

AActor* AAntBotController::PickShootTarget(AAntCharacter* Ant) const
{
	const FVector Me = Ant->GetActorLocation();
	AActor* Best = nullptr;
	float BestD = 1600.f;
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* E = *It;
		if (!E || E->bDead || !Ant->IsEnemy(E) || E->IsInNest())
		{
			continue;
		}
		float D = (float)FVector::Dist(E->GetActorLocation(), Me);
		if (E->IsCarrying()) { D -= 300.f; }   // stop the thieves first
		if (D < BestD && CanSee(Ant, E))
		{
			BestD = D;
			Best = E;
		}
	}
	if (!Best && !Ant->IsMonster())
	{
		for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
		{
			AHumanCharacter* H = *It;
			if (H && !H->IsKnockedOut() && FVector::Dist2D(H->GetActorLocation(), Me) < 1900.f && CanSee(Ant, H))
			{
				Best = H;
			}
		}
	}
	return Best;
}

AKitchenLoot* AAntBotController::PickFood(AAntCharacter* Ant) const
{
	const FVector Me = Ant->GetActorLocation();
	const float Now = GetWorld()->GetTimeSeconds();
	AKitchenLoot* Best = nullptr;
	float BestScore = 9000.f;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (!L || L->bIsGear)
		{
			continue;
		}
		if (const float* Until = IgnoreUntil.Find(L); Until && *Until > Now)
		{
			continue;
		}
		const FVector P = L->GetActorLocation();
		if (P.Y > 4750.f || FVector::Dist2D(P, AKitchenBuilder::NestLocation(Ant->Team)) < AKitchenBuilder::NestRadius(Ant->Team))
		{
			continue;   // secret corridor (bots don't know it) / already home
		}
		float Score = (float)FVector::Dist2D(P, Me) + FMath::Abs((float)(P.Z - Me.Z)) * 1.3f;
		// Distance home matters too: the prize has to be dragged back.
		Score += 0.35f * (float)FVector::Dist2D(P, AKitchenBuilder::NestLocation(Ant->Team));
		Score -= L->Value * 6.f;
		if (L->IsCarried())
		{
			if (L->CarryTeam() == Ant->Team)
			{
				Score -= L->CarryRatio() < 1.f ? 2500.f : -1500.f;   // help with heavy stuff, leave light stuff alone
			}
			else
			{
				Score -= 600.f;   // tug of war!
			}
		}
		else if (L->AntsNeeded(Ant->GetStrength()) > 1)
		{
			// Too heavy alone: still go if somebody may help, but prefer things we can carry.
			Score += 900.f * (L->AntsNeeded(Ant->GetStrength()) - 1);
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = L;
		}
	}
	return Best;
}

void AAntBotController::Think(AAntCharacter* Ant)
{
	const float Now = GetWorld()->GetTimeSeconds();
	ShootTarget = PickShootTarget(Ant);

	if (Ant->Creature == 2)
	{
		// Spider: hunt the nearest ant of any colony; otherwise go back up to the web and wait.
		AAntCharacter* Prey = nullptr;
		float Best = 5000.f;
		for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
		{
			AAntCharacter* A = *It;
			if (!A || A == Ant || A->bDead || A->Creature == 2 || A->IsInNest())
			{
				continue;
			}
			const float D = (float)FVector::Dist(A->GetActorLocation(), Ant->GetActorLocation());
			if (D < Best) { Best = D; Prey = A; }
		}
		if (Prey)
		{
			Goal = Prey->GetActorLocation();
			if (!ShootTarget.IsValid() && Best < 1600.f) { ShootTarget = Prey; }
			if (Best < 130.f + AAntCharacter::BodyRadius * 2.f) { Ant->BotBite(); }
		}
		else
		{
			Goal = AKitchenBuilder::SpiderLair();
			ShootTarget = nullptr;
		}
		bHasGoal = true;
		return;
	}

	// Squad orders from the player commander.
	if (OrderMode != 0 && (Now > OrderUntil || (OrderMode == 1 && (!OrderBy.IsValid() || OrderBy->bDead))))
	{
		OrderMode = 0;
	}
	if (OrderMode == 1 && OrderBy.IsValid() && !Ant->Carrying)
	{
		const float Ang = (GetUniqueID() % 8) * 0.785f;
		Goal = OrderBy->GetActorLocation() + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.f) * 180.f;
		bHasGoal = true;
		return;
	}
	if (OrderMode == 2)
	{
		Goal = OrderPoint + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 0.f);
		bHasGoal = true;
		return;
	}

	AKitchenLoot* Carry = Ant->Carrying;
	if (!Carry && LastCarried.IsValid())
	{
		IgnoreUntil.Add(LastCarried.Get(), Now + 12.f);   // slipped out: try something else for a while
		FoodTarget = nullptr;
	}
	LastCarried = Carry;
	if (Carry)
	{
		// Dragging home. Give up if it doesn't move at all for a while.
		const float Moved = (float)FVector::Dist(Carry->GetActorLocation(), LastLootPos);
		LastLootPos = Carry->GetActorLocation();
		HeavyTimer = Moved < 25.f ? HeavyTimer + 0.5f : 0.f;
		if (HeavyTimer > 7.f)
		{
			IgnoreUntil.Add(Carry, Now + 20.f);
			Ant->LetGo();
			HeavyTimer = 0.f;
			FoodTarget = nullptr;
		}
		else
		{
			Goal = AKitchenBuilder::NestLocation(Ant->Team) + FVector(0.f, 0.f, 30.f);
			bHasGoal = true;
			return;
		}
	}

	// Enemy carrying our future dinner nearby: chase it.
	if (AAntCharacter* E = Cast<AAntCharacter>(ShootTarget.Get()); E && E->IsCarrying() && E->Carrying)
	{
		FoodTarget = E->Carrying;
	}
	else if (!FoodTarget.IsValid() || FoodTarget->bIsGear || FMath::FRand() < 0.15f)
	{
		FoodTarget = PickFood(Ant);
	}

	if (AKitchenLoot* L = FoodTarget.Get())
	{
		const float Reach = AAntCharacter::InteractRadius + L->GetRadius() * 0.5f;
		if (FVector::Dist(L->GetActorLocation(), Ant->GetActorLocation()) < Reach)
		{
			if (Ant->GrabLoot(L))
			{
				LastLootPos = L->GetActorLocation();
				HeavyTimer = 0.f;
				Goal = AKitchenBuilder::NestLocation(Ant->Team);
				bHasGoal = true;
				LastCarried = L;
				return;
			}
			IgnoreUntil.Add(L, Now + 10.f);
			FoodTarget = nullptr;
		}
		else
		{
			Goal = L->GetActorLocation();
			bHasGoal = true;
			return;
		}
	}

	// Nothing to do: patrol the middle of the kitchen.
	if (!bHasGoal || FVector::Dist2D(Goal, Ant->GetActorLocation()) < 300.f)
	{
		const FBox A = AKitchenBuilder::PlayArea();
		Goal = FVector(FMath::FRandRange(A.Min.X, A.Max.X), FMath::FRandRange(A.Min.Y, A.Max.Y), 30.f);
		bHasGoal = true;
	}
}

void AAntBotController::Shoot(AAntCharacter* Ant, float Dt)
{
	AActor* T = ShootTarget.Get();
	if (!T)
	{
		return;
	}
	FVector Aim = T->GetActorLocation();
	if (AHumanCharacter* H = Cast<AHumanCharacter>(T))
	{
		Aim.Z = FMath::Min(Aim.Z, Ant->GetActorLocation().Z + 600.f);
	}
	// Lead the target a bit, then miss a bit.
	const float Flight = (float)FVector::Dist(Aim, Ant->GetMuzzleLocation()) / FMath::Max(500.f, Ant->Weapon.ProjectileSpeed);
	Aim += T->GetVelocity() * Flight * 0.7f;
	Aim.Z += Flight * Flight * 0.5f * 980.f * Ant->Weapon.Gravity;   // arc
	const FVector Dir = FMath::VRandCone((Aim - Ant->GetMuzzleLocation()).GetSafeNormal(), FMath::DegreesToRadians(4.f));
	Ant->BotFire(Dir);
}

void AAntBotController::Steer(AAntCharacter* Ant, const FVector& Target, float Dt)
{
	const FVector Me = Ant->GetActorLocation();
	const FVector N = Ant->GetVisualUp();
	FVector To = Target - Me;

	// Fight: keep a comfortable distance and strafe.
	if (AAntCharacter* E = Cast<AAntCharacter>(ShootTarget.Get()); E && !Ant->IsCarrying() && !FoodTarget.IsValid() && Ant->Creature != 2)
	{
		const FVector ToE = E->GetActorLocation() - Me;
		const float D = (float)ToE.Size();
		StrafeTimer -= Dt;
		if (StrafeTimer <= 0.f)
		{
			StrafeTimer = FMath::FRandRange(0.8f, 2.f);
			StrafeSign = FMath::RandBool() ? 1.f : -1.f;
		}
		const FVector Side = FVector::CrossProduct(N, ToE.GetSafeNormal()) * StrafeSign;
		To = (D > 900.f ? ToE : D < 450.f ? -ToE : FVector::ZeroVector).GetSafeNormal() * 300.f + Side * 300.f;
	}

	if (UnstickTime > 0.f)
	{
		UnstickTime -= Dt;
		To = UnstickDir * 500.f;
	}

	const bool bOnWall = Ant->IsCrawling() && N.Z < 0.6f;
	const float Below = (float)(Me.Z - Target.Z);
	if (bOnWall && Below > 250.f && (N.Z < -0.4f || FVector::Dist2D(Me, Target) < Below * 1.5f) && !Ant->IsCarrying()
		&& (Ant->Creature != 2 || FVector::Dist2D(Me, Target) < 900.f))
	{
		Ant->Jump();   // let go of the wall / ceiling and fall towards the target
	}

	FVector D = To - N * FVector::DotProduct(To, N);
	if (bOnWall && FVector::DotProduct(To, N) < -40.f)
	{
		// The target is behind this wall: climb over it.
		FVector UpOnWall = FVector::UpVector - N * N.Z;
		if (UpOnWall.SizeSquared() < 0.01f)
		{
			UpOnWall = D;
		}
		D += UpOnWall.GetSafeNormal() * (float)To.Size();
	}
	if (D.SizeSquared() < 1.f)
	{
		D = Ant->GetActorForwardVector();
	}
	Ant->AddMovementInput(D.GetSafeNormal(), 1.f);

	FVector Face = ShootTarget.IsValid() ? (ShootTarget->GetActorLocation() - Me) : To;
	if (Face.SizeSquared2D() < 1.f)
	{
		Face = D;
	}
	Desired = FRotator(0.f, (float)Face.Rotation().Yaw, 0.f);
	if (ShootTarget.IsValid())
	{
		Desired.Pitch = FMath::Clamp((float)Face.Rotation().Pitch, -60.f, 60.f);
	}

	// Stuck?
	StuckCheckTimer += Dt;
	if (StuckCheckTimer > 1.2f)
	{
		const bool bSlowOnPurpose = Ant->IsCarrying() && Ant->Carrying && Ant->Carrying->CarryRatio() < 1.f;
		if (!bSlowOnPurpose && FVector::Dist(Me, LastCheckPos) < 70.f && UnstickTime <= 0.f)
		{
			UnstickTime = 1.f;
			UnstickDir = FVector::CrossProduct(N, D.GetSafeNormal()) * (FMath::RandBool() ? 1.f : -1.f) + N * 0.2f;
			if (!Ant->IsCarrying()) { Ant->Jump(); }
		}
		LastCheckPos = Me;
		StuckCheckTimer = 0.f;
	}
}

void AAntBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AAntCharacter* Ant = Cast<AAntCharacter>(GetPawn());
	if (!Ant || Ant->bDead)
	{
		return;
	}
	ThinkTimer -= DeltaSeconds;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.5f;
		Think(Ant);
	}
	if (bHasGoal)
	{
		Steer(Ant, Goal, DeltaSeconds);
	}
	Shoot(Ant, DeltaSeconds);
}
