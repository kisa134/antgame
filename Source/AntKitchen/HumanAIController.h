#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "HumanAIController.generated.h"

class AAntCharacter;
class AHumanCharacter;

/**
 * Simple steering AI for the Human (no navmesh needed):
 * hunts the juiciest ant (carriers first), stomps ants on the floor,
 * sprays ants on counters/tables, wanders when nobody is around,
 * and flees in panic when knocked out.
 */
UCLASS()
class ANTKITCHEN_API AHumanAIController : public AAIController
{
	GENERATED_BODY()

public:
	AHumanAIController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

private:
	AAntCharacter* PickTarget(const AHumanCharacter* H) const;
	void MoveTowards(AHumanCharacter* H, const FVector& Goal, float Scale);

	TWeakObjectPtr<AAntCharacter> Target;
	FRotator DesiredRotation = FRotator::ZeroRotator;
	FVector WanderPoint = FVector::ZeroVector;
	float RetargetTimer = 0.f;
	float WanderTimer = 0.f;
	float StuckTimer = 0.f;
	float UnstickTimer = 0.f;
	bool bMovedThisTick = false;
};
