#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AntRun.h"
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
	FVector SwarmPoint = FVector::ZeroVector;
	float RetargetTimer = 0.f;
	float WanderTimer = 0.f;
	float HeldSince = 0.f;
	float NextStealTime = 20.f;
	float StuckTimer = 0.f;
	float UnstickTimer = 0.f;
	bool bMovedThisTick = false;

	// Noise: he walks to the loudest recent one, looks around for a few seconds, switches the thing off.
	FAntNoise Noise;
	bool bInvestigating = false;
	float LookUntil = 0.f;
	float NoiseGiveUp = 0.f;
	// Doors and drawers left open: he goes and shuts them.
	TWeakObjectPtr<class AKitchenInteractable> DoorTarget;
	float NextDoorScan = 0.f;
	float DoorGiveUp = 0.f;
	/** The hand is out to the handle: the door shuts when it gets there. */
	float ReachAt = 0.f;
	/** The bare-hand slap on the floor is an extra attack the jointed body brought: the AI keeps it rare. */
	float NextSlapTime = 0.f;
	bool bNoiseReached = false;
	// Lost sight of an ant that is still out there (under the cabinets, behind a door): he goes and peers under the furniture.
	TWeakObjectPtr<AAntCharacter> LastTarget;
	FVector LastSeen = FVector::ZeroVector;
	float LastSeenTime = -100.f;
	FVector PeerLoc = FVector::ZeroVector;
	float PeerGiveUp = 0.f;
	float PeerEnd = 0.f;
	float NextPeerTime = 0.f;
};
