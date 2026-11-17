#include "HumanAIController.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "KitchenLoot.h"
#include "AntKitchenGameMode.h"
#include "EngineUtils.h"
#include "AntTypes.h"
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
	// «Тихий шаг»: he notices ants from 30% closer.
	const float Notice = AntCard::Has(this, AntCard::Quiet) ? 0.7f : 1.f;
	float BestScore = 8000.f * Notice;
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
		if (KitchenContainers::SealedAt(GetWorld(), AL) >= 0)
		{
			continue;   // behind a shut door
		}
		float Score = FVector::Dist2D(Me, AL);
		if (bDark && Score > (Ant->IsCarrying() ? 3200.f : 2200.f) * Notice)
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
	const FVector D = (AKitchenBuilder::Route(H->GetActorLocation(), Goal) - H->GetActorLocation()).GetSafeNormal2D();
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

	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	const float TimeNow = GetWorld()->GetTimeSeconds();
	if (H->IsStunned())
	{
		// Something heavy landed on him: he just stands there and reels.
		H->SetSpraying(false);
		DesiredRotation.Pitch = 20.f;
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

	// What he does instead of hunting: checking a noise, shutting a door left open. An ant carrying loot right
	// in front of him is the one thing he will not walk away from.
	const float TargetDist = T ? (float)FVector::Dist2D(T->GetActorLocation(), MyLoc) : 1.e9f;
	const bool bHotChase = T && T->IsCarrying() && TargetDist < 2600.f;
	bool bBusy = false;
	if (GM && !bHotChase)
	{
		if (!bInvestigating && GM->BestNoise(MyLoc, Noise))
		{
			bInvestigating = true;
			LookUntil = 0.f;
			NoiseGiveUp = TimeNow + 25.f;
			DoorTarget = nullptr;
		}
		if (bInvestigating)
		{
			bBusy = true;
			H->SetSpraying(false);
			if (LookUntil <= 0.f)
			{
				GM->SetHumanGoal(AntHumanGoal::Noise, FString::Printf(TEXT("идёт на шум: %s"), AntNoise::Name(Noise.Kind)), Noise.Loc);
				DesiredRotation.Pitch = -10.f;
				if (FVector::Dist2D(Noise.Loc, MyLoc) > 1150.f && TimeNow < NoiseGiveUp)
				{
					MoveTowards(H, Noise.Loc, 1.f);
				}
				else
				{
					LookUntil = TimeNow + 5.f;
				}
			}
			else
			{
				// Looks around for whoever did it.
				GM->SetHumanGoal(AntHumanGoal::Look, FString::Printf(TEXT("осматривается: %s"), AntNoise::Name(Noise.Kind)), Noise.Loc);
				DesiredRotation.Yaw = FRotator::NormalizeAxis((float)DesiredRotation.Yaw + 75.f * DeltaSeconds);
				DesiredRotation.Pitch = -30.f;
				if (TimeNow >= LookUntil)
				{
					bInvestigating = false;
					GM->NoiseChecked(Noise, TargetDist > 2200.f);
				}
			}
		}
		else if (TargetDist > 1500.f && GM->bHeist)
		{
			if (TimeNow >= NextDoorScan && !DoorTarget.IsValid())
			{
				NextDoorScan = TimeNow + 1.f;
				float BestD = 7000.f;   // most of the room he is in
				for (TActorIterator<AKitchenInteractable> It(GetWorld()); It; ++It)
				{
					if (It->DoorClass() > 0 && It->IsDoorOpen() && It->HumanShuts() && TimeNow - It->OpenedAt > 4.f
						&& AKitchenBuilder::Region(It->GetActorLocation()) == AKitchenBuilder::Region(MyLoc))
					{
						const float D = (float)FVector::Dist2D(It->GetActorLocation(), MyLoc);
						if (D < BestD) { BestD = D; DoorTarget = *It; }
					}
				}
				if (DoorTarget.IsValid()) { DoorGiveUp = TimeNow + 12.f; }
			}
			if (AKitchenInteractable* Door = DoorTarget.Get())
			{
				if (!Door->IsDoorOpen())
				{
					DoorTarget = nullptr;
				}
				else
				{
					bBusy = true;
					H->SetSpraying(false);
					const FVector DL = Door->GetActorLocation();
					GM->SetHumanGoal(AntHumanGoal::Shut, FString::Printf(TEXT("идёт закрыть %s"), *Door->DoorNoun()), DL);
					DesiredRotation.Pitch = -20.f;
					if (FVector::Dist2D(DL, MyLoc) > 1300.f && TimeNow < DoorGiveUp)
					{
						MoveTowards(H, DL, 0.9f);
					}
					else
					{
						Door->SetDoorOpen(false);
						AntSfx::Everyone(this, AntSfx::FridgeDoor, DL, 1.5f);
						UE_LOG(LogTemp, Display, TEXT("ANTHUMAN shut %s"), *Door->GetName());
						DoorTarget = nullptr;
					}
				}
			}
		}
	}
	if (GM && !bBusy)
	{
		if (T)
		{
			GM->SetHumanGoal(AntHumanGoal::Hunt, FString::Printf(TEXT("гонится: %s"), *T->GetDisplayName()), T->GetActorLocation());
		}
		else
		{
			GM->SetHumanGoal(AntHumanGoal::Idle, FString(), MyLoc);
		}
	}

	if (bBusy)
	{
	}
	else if (T)
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
			WanderPoint = AKitchenBuilder::RandomFloorPoint() + FVector(0.f, 0.f, MyLoc.Z);
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
						if (GM)
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
			const bool bLiving = MyLoc.X > 7100.f;
			P.X = bLiving ? FMath::Clamp((float)P.X, 8000.f, 19000.f) : FMath::Clamp((float)P.X, (float)Area.Min.X, (float)Area.Max.X);
			P.Y = bLiving ? FMath::Clamp((float)P.Y, -8300.f, -500.f) : FMath::Clamp((float)P.Y, (float)Area.Min.Y, (float)Area.Max.Y);
			WanderPoint = P;
		}
	}
	else
	{
		StuckTimer = 0.f;
	}
}
