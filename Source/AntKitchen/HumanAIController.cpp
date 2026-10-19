#include "HumanAIController.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "KitchenLoot.h"
#include "AntKitchenGameMode.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

AHumanAIController::AHumanAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = false;
}

void AHumanAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}
	const FRotator NewRot = FMath::RInterpTo(GetControlRotation(), DesiredRotation, DeltaTime, 3.5f);
	SetControlRotation(NewRot);
	if (bUpdatePawn)
	{
		P->FaceRotation(NewRot, DeltaTime);
	}
}

AAntCharacter* AHumanAIController::PickTarget(const AHumanCharacter* H) const
{
	AAntCharacter* Best = nullptr;
	float BestScore = 8000.f;
	const FVector Me = H->GetActorLocation();
	const bool bDark = !AKitchenSwitch::AreLightsOn(GetWorld());
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest())
		{
			continue;
		}
		const FVector AL = Ant->GetActorLocation();
		// Can't see ants hiding inside the fridge, the oven, the walls or behind the cabinets.
		if (AL.Y > 4750.f || FBox(FVector(2700.f, 3000.f, 0.f), FVector(4700.f, 4750.f, 3700.f)).IsInside(AL)
			|| FBox(FVector(-1500.f, 3000.f, 120.f), FVector(0.f, 4750.f, 1650.f)).IsInside(AL)
			|| FBox(FVector(-4200.f, 3000.f, 0.f), FVector(-2800.f, 4750.f, 1100.f)).IsInside(AL))
		{
			continue;
		}
		float Score = FVector::Dist2D(Me, AL);
		if (bDark && Score > (Ant->IsCarrying() ? 3200.f : 2200.f))
		{
			continue;   // lights are off: only notices ants close by
		}
		if (Ant->IsCarrying()) { Score -= 2500.f; }   // smells the sugar
		if (Ant->GetActorLocation().Z > 300.f) { Score += 800.f; }
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Ant;
		}
	}
	return Best;
}

void AHumanAIController::MoveTowards(AHumanCharacter* H, const FVector& Goal, float Scale)
{
	const FVector D = (Goal - H->GetActorLocation()).GetSafeNormal2D();
	if (!D.IsNearlyZero())
	{
		H->AddMovementInput(D, Scale);
		DesiredRotation.Yaw = D.Rotation().Yaw;
		bMovedThisTick = true;
	}
}

void AHumanAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AHumanCharacter* H = Cast<AHumanCharacter>(GetPawn());
	if (!H)
	{
		return;
	}
	bMovedThisTick = false;
	const FVector MyLoc = H->GetActorLocation();
	const float FloorZ = MyLoc.Z - H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FBox Area = AKitchenBuilder::PlayArea();

	if (H->IsKnockedOut())
	{
		H->SetSpraying(false);
		DesiredRotation.Pitch = 10.f;
		MoveTowards(H, AKitchenBuilder::HumanFleePoint(), 1.f);
		return;
	}

	if (UnstickTimer > 0.f)
	{
		UnstickTimer -= DeltaSeconds;
		H->SetSpraying(false);
		MoveTowards(H, WanderPoint, 1.f);
		return;
	}

	RetargetTimer -= DeltaSeconds;
	if (RetargetTimer <= 0.f)
	{
		RetargetTimer = 0.5f;
		Target = PickTarget(H);
	}

	AAntCharacter* T = Target.Get();
	if (T && (T->bDead || T->IsInNest()))
	{
		T = nullptr;
		Target = nullptr;
	}

	if (T)
	{
		const FVector TL = T->GetActorLocation();
		const FVector To = TL - MyLoc;
		const float Dist = To.Size2D();
		const float TargetYaw = (float)To.Rotation().Yaw;
		const float YawErr = FMath::Abs(FMath::FindDeltaAngleDegrees((float)H->GetActorRotation().Yaw, TargetYaw));
		const bool bOnFloor = (TL.Z - FloorZ) < 220.f;

		DesiredRotation.Yaw = TargetYaw;
		if (bOnFloor)
		{
			H->SetSpraying(false);
			DesiredRotation.Pitch = -35.f;
			if (Dist > 560.f)
			{
				MoveTowards(H, TL, 1.f);
			}
			else if (Dist < 250.f)
			{
				H->AddMovementInput(-To.GetSafeNormal2D(), 0.6f);
				bMovedThisTick = true;
			}
			DesiredRotation.Yaw = TargetYaw;
			if (Dist < 760.f && YawErr < 30.f)
			{
				const float Side = FVector::DotProduct(To, H->GetActorRightVector());
				H->TryStomp(Side < 0.f ? 0 : 1);
			}
		}
		else
		{
			const FVector Origin = H->GetSprayOrigin();
			DesiredRotation.Pitch = FMath::Clamp((float)(TL - Origin).Rotation().Pitch, -70.f, 15.f);
			if (Dist > 1500.f)
			{
				MoveTowards(H, TL, 1.f);
			}
			else if (Dist < 800.f)
			{
				H->AddMovementInput(-To.GetSafeNormal2D(), 0.5f);
				bMovedThisTick = true;
			}
			DesiredRotation.Yaw = TargetYaw;
			// Ant on a wall or a table: newspaper slap when ready, spray otherwise.
			const float Now = GetWorld()->GetTimeSeconds();
			if (Now >= H->SwatReadyTime && YawErr < 35.f && FVector::Dist(TL, MyLoc + FVector(0.f, 0.f, 600.f)) < AHumanCharacter::SwatRange)
			{
				H->SetSpraying(false);
				H->TrySwat(TL);
			}
			else
			{
				H->SetSpraying(FVector::Dist(Origin, TL) < H->GetSprayRange() - 100.f && YawErr < 15.f && H->SprayFuel > 10.f);
			}
		}
	}
	else if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()); Swarm && Swarm->FindStompTarget(MyLoc, 7000.f, SwarmPoint))
	{
		// No ant players around: go stomp the worker trails.
		H->SetSpraying(false);
		const FVector To = SwarmPoint - MyLoc;
		const float Dist = (float)To.Size2D();
		const float TargetYaw = (float)To.Rotation().Yaw;
		const float YawErr = FMath::Abs(FMath::FindDeltaAngleDegrees((float)H->GetActorRotation().Yaw, TargetYaw));
		DesiredRotation.Yaw = TargetYaw;
		DesiredRotation.Pitch = -35.f;
		if (Dist > 520.f)
		{
			MoveTowards(H, SwarmPoint, 0.8f);
		}
		if (Dist < 760.f && YawErr < 35.f)
		{
			const float Side = FVector::DotProduct(To, H->GetActorRightVector());
			H->TryStomp(Side < 0.f ? 0 : 1);
		}
	}
	else
	{
		H->SetSpraying(false);
		WanderTimer -= DeltaSeconds;
		if (WanderTimer <= 0.f || FVector::Dist2D(MyLoc, WanderPoint) < 400.f)
		{
			WanderTimer = 6.f;
			WanderPoint = FVector(FMath::FRandRange(Area.Min.X, Area.Max.X), FMath::FRandRange(Area.Min.Y, Area.Max.Y), MyLoc.Z);
		}
		DesiredRotation.Pitch = -15.f;
		MoveTowards(H, WanderPoint, 0.5f);
	}

	// Steal food the ants are dragging home and throw it across the kitchen.
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (H->Held && Now - HeldSince > 1.2f)
		{
			const FVector Away = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
			H->GrabOrThrow(MyLoc, (Away + FVector(0.f, 0.f, 0.35f)).GetSafeNormal());
		}
		else if (!H->Held && Now > NextStealTime)
		{
			NextStealTime = Now + 2.f;
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				AKitchenLoot* L = *It;
				if (L && L->IsCarried() && L->Value >= 50 && FVector::Dist2D(L->GetActorLocation(), MyLoc) < 1800.f && L->GetActorLocation().Z < 2000.f)
				{
					H->GrabOrThrow(L->GetActorLocation(), FVector::UpVector);
					if (H->Held)
					{
						HeldSince = Now;
						NextStealTime = Now + 14.f;
						if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
						{
							GM->Broadcast(FString::Printf(TEXT("Человек отобрал «%s» и швыряет его!"), *L->LootName), FLinearColor(1.f, 0.35f, 0.3f));
						}
					}
					break;
				}
			}
		}
	}

	// Bumped into furniture? Walk somewhere random for a moment.
	if (bMovedThisTick && H->GetVelocity().Size2D() < 40.f)
	{
		StuckTimer += DeltaSeconds;
		if (StuckTimer > 1.2f)
		{
			StuckTimer = 0.f;
			UnstickTimer = 1.6f;
			const FVector Dir = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
			FVector P = MyLoc + Dir * 2500.f;
			P.X = FMath::Clamp(P.X, Area.Min.X, Area.Max.X);
			P.Y = FMath::Clamp(P.Y, Area.Min.Y, Area.Max.Y);
			WanderPoint = P;
		}
	}
	else
	{
		StuckTimer = 0.f;
	}
}
