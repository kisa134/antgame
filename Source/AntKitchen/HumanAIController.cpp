#include "HumanAIController.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "KitchenLoot.h"
#include "AntKitchenGameMode.h"
#include "EngineUtils.h"
#include "NpcBody.h"
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
	const FVector Eyes = Me + FVector(0.f, 0.f, H->GetSimpleCollisionHalfHeight() * 0.8f);
	const bool bDark = !AKitchenSwitch::AreLightsOn(GetWorld());
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest())
		{
			continue;
		}
		const FVector AL = Ant->GetActorLocation();
		// He cannot follow an ant out through a crack in the wall: what is out in the yard or the street is none of his business
		// (he used to pick such an ant and walk into the wall at the crack).
		if (AKitchenBuilder::Region(AL) >= 2)
		{
			continue;
		}
		// Can't see ants hiding inside the fridge, the oven, the walls or behind the cabinets. Past the line of the cabinet fronts he sees
		// only what is in plain sight from where he stands (the back of the worktop, the tap, the spice shelves, the rail, the cabinet tops).
		if ((AL.Y > 4750.f && !NpcWorld::CanSee(GetWorld(), Eyes, AL + FVector(0.f, 0.f, 10.f))) || FBox(FVector(2700.f, 3000.f, 0.f), FVector(4700.f, 4750.f, 3700.f)).IsInside(AL)
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

FVector AHumanAIController::PickWander(const AHumanCharacter* H) const
{
	FVector P = AKitchenBuilder::RandomFloorPoint();
	const FBox Area = AKitchenBuilder::PlayArea();
	// The box he strolls in stops short of the worktops and of the west and south walls. Some of his kitchen walks now end
	// right at one of them: wherever the room itself says it is (a trace from the edge of the box), less his own width.
	if (P.X >= Area.Min.X && P.X <= Area.Max.X && P.Y >= Area.Min.Y && P.Y <= Area.Max.Y && FMath::FRand() < 0.4f)
	{
		const float Roll = FMath::FRand();
		FVector From = P, Dir;
		if (Roll < 0.6f) { From.Y = Area.Max.Y; Dir = FVector(0.f, 1.f, 0.f); }          // up to the cabinets
		else if (Roll < 0.8f) { From.X = Area.Min.X; Dir = FVector(-1.f, 0.f, 0.f); }    // the west wall
		else { From.Y = Area.Min.Y; Dir = FVector(0.f, -1.f, 0.f); }                     // under the window
		From.Z = 320.f;
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From + Dir * 1800.f, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(HumanWander), false, H)))
		{
			P = Hit.ImpactPoint - Dir * (H->GetSimpleCollisionRadius() + 70.f);
			P.Z = 0.f;
		}
	}
	return P;
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
	if (H->bDevPuppet)
	{
		return;   // dev: a puppet for screenshots
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

	if (H->IsRooted())
	{
		// Down on the floor with a hand out (a slap, a grab, a handle): he does not walk until he is up again.
		H->SetSpraying(false);
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
		LastTarget = T;
		LastSeen = T->GetActorLocation();
		LastSeenTime = TimeNow;
		if (PeerEnd > 0.f || !PeerLoc.IsZero())
		{
			H->StopPeer();   // there it is!
			PeerLoc = FVector::ZeroVector;
			PeerEnd = 0.f;
		}
	}
	else if (H->HasJointedBody() && PeerLoc.IsZero() && TimeNow >= NextPeerTime && TimeNow - LastSeenTime < 1.5f)
	{
		// It was right there a moment ago and it is still alive somewhere he cannot see: under the furniture, most likely.
		const AAntCharacter* Lost = LastTarget.Get();
		if (Lost && !Lost->bDead && !Lost->IsInNest() && FVector::Dist2D(LastSeen, MyLoc) < 3500.f && LastSeen.Z - FloorZ < 700.f)
		{
			PeerLoc = LastSeen;
			PeerGiveUp = TimeNow + 9.f;
			PeerEnd = 0.f;
		}
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
			bNoiseReached = false;
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
				if (!bNoiseReached && TimeNow >= LookUntil - 0.45f && Noise.Source.IsValid() && FVector::Dist2D(Noise.Loc, MyLoc) < 1700.f)
				{
					bNoiseReached = true;
					H->PlayReach(Noise.Loc);   // the hand goes to the tap / the button he is about to switch off
				}
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
						ReachAt = 0.f;
					}
					else if (H->HasJointedBody() && ReachAt <= 0.f)
					{
						// The hand goes out to the door; it shuts when the hand gets there.
						H->PlayReach(DL);
						ReachAt = TimeNow + HumanAct::ReachTouch;
						DesiredRotation.Yaw = (float)(DL - MyLoc).Rotation().Yaw;
					}
					else if (TimeNow >= ReachAt)
					{
						Door->SetDoorOpen(false);
						AntSfx::Everyone(this, AntSfx::FridgeDoor, DL, 1.5f);
						UE_LOG(LogTemp, Display, TEXT("ANTHUMAN shut %s"), *Door->GetName());
						DoorTarget = nullptr;
						ReachAt = 0.f;
					}
				}
			}
		}
	}
	if (GM && !bBusy && (T || PeerLoc.IsZero()))
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

	if (!bBusy && !T && !PeerLoc.IsZero())
	{
		// Peering under the furniture where the ant went.
		bBusy = true;
		H->SetSpraying(false);
		if (GM)
		{
			GM->SetHumanGoal(AntHumanGoal::Look, TEXT("заглядывает под мебель"), PeerLoc);
		}
		const FVector To = PeerLoc - MyLoc;
		DesiredRotation.Yaw = (float)To.Rotation().Yaw;
		DesiredRotation.Pitch = -40.f;
		if (PeerEnd <= 0.f && To.Size2D() > 1150.f && TimeNow < PeerGiveUp)
		{
			MoveTowards(H, PeerLoc, 0.8f);
		}
		else if (PeerEnd <= 0.f)
		{
			H->PlayPeer(PeerLoc, 3.2f);
			PeerEnd = TimeNow + 3.2f;
			UE_LOG(LogTemp, Display, TEXT("ANTHUMAN peers under the furniture at %s"), *PeerLoc.ToCompactString());
		}
		else if (TimeNow >= PeerEnd)
		{
			PeerLoc = FVector::ZeroVector;
			PeerEnd = 0.f;
			NextPeerTime = TimeNow + 14.f;
		}
	}
	else if (bBusy && !PeerLoc.IsZero() && PeerEnd <= 0.f)
	{
		PeerLoc = FVector::ZeroVector;   // something louder came up
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
			else if (H->HasJointedBody() && Dist >= 760.f && Dist < 1250.f && YawErr < 35.f && TimeNow >= H->SwatReadyTime && TimeNow >= NextSlapTime)
			{
				// Just out of the slipper's reach: he bends down and slaps the floor with his bare hand (not more than once in 6 s).
				NextSlapTime = TimeNow + 6.f;
				H->TrySwat(TL);
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
	else if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()); Swarm && Swarm->FindStompTarget(MyLoc, 7000.f, SwarmPoint) && AKitchenBuilder::Region(SwarmPoint) < 2)
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
			WanderPoint = PickWander(H) + FVector(0.f, 0.f, MyLoc.Z);
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
				if (L && L->IsCarried() && L->Value >= 50 && FVector::Dist2D(L->GetActorLocation(), MyLoc) < (H->HasJointedBody() ? 1500.f : 1800.f) && L->GetActorLocation().Z < 2000.f)
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
