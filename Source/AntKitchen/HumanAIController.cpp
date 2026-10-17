#include "HumanAIController.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "KitchenBuilder.h"
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
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant->bDead || Ant->IsInNest())
		{
			continue;
		}
		float Score = FVector::Dist2D(Me, Ant->GetActorLocation());
		if (Ant->CarriedWeight > 0) { Score -= 2500.f; }   // smells the sugar
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
			H->SetSpraying(FVector::Dist(Origin, TL) < AHumanCharacter::SprayRange - 100.f && YawErr < 15.f && H->SprayFuel > 10.f);
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
