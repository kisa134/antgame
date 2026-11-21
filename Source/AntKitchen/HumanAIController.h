#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AntRun.h"
#include "HumanNav.h"
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
	/** Level v3: the raid's quiet start (he is not after the ants yet): only his routine and his mess (the phone on the sofa...). */
	bool bQuiet = false;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

private:
	AAntCharacter* PickTarget(const AHumanCharacter* H) const;
	/** Where to stroll to next: now and then right up to the worktops, the west wall or the window wall (found from the room itself). */
	FVector PickWander(const class AHumanCharacter* H) const;
	void MoveTowards(AHumanCharacter* H, const FVector& Goal, float Scale);
	/** After a fall: a towel (from the oven handle, the mop bucket, or his pocket), then down on his haunches over the puddle. True while busy with it. */
	bool TickChores(AHumanCharacter* H);
	int32 ChoreStage = 0;
	float ChoreAt = 0.f, ChoreGiveUp = 0.f;
	FVector TowelPoint = FVector::ZeroVector;
	TWeakObjectPtr<class AKitchenSlick> ChorePuddle;

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

	// Level v2: his way round the furniture and his round through both rooms.
	const FHumanNav* Nav = nullptr;   // the game mode's grid of the house
	FHumanNavFollower Follow;
	FVector PathGoal = FVector::ZeroVector;
	int32 PatrolIndex = -1;
	float PatrolPause = 0.f;
	float NextNavLog = 0.f;

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

	// ---- Level v3: his evening (NpcRoutine.h) and what he does about the mess the ants make
	/** The routine when he has nothing better to do. True while it has him (on the way, at it, out of the flat). */
	bool TickRoutine(AHumanCharacter* H, float Dt);
	/** A mess he has seen or heard: look, mutter, fetch the tool, clean up / put it back. True while busy with it. */
	bool TickReact(AHumanCharacter* H, float Dt);
	/** Sticky floor (cola, jam): slow, heavy steps. */
	void TickSticky(AHumanCharacter* H, float Dt);
	/** Stops whatever the routine has his body doing (gets up), for anything more urgent. */
	void BreakRoutine(AHumanCharacter* H, const TCHAR* Why);
	bool RoutineHolds(const AHumanCharacter* H) const { return RtPhase == 1 || RtPhase == 2 || RtPhase == 3; }
	int32 RtStep = -1;
	uint8 RtPhase = 0;          // 0 none, 1 going, 2 at it, 3 out of the flat, 4 done (waits for the next step)
	float RtSince = 0.f;
	float RtGiveUp = 0.f;
	float RtStandUpAt = 0.f;    // up from a seat: steps back to where he stood once the rise is over
	float GoneUntil = 0.f;
	uint8 GoneStage = 0;
	uint32 RtNudge = 0;
	int32 ReactMess = -1;
	uint8 ReactStage = 0;
	float ReactAt = 0.f, ReactGiveUp = 0.f;
	FVector ToolPoint = FVector::ZeroVector;
	FString ToolName;
	float NextMessScan = 0.f;
	float StickyStepAt = 0.f;
	bool bStickyTold = false;
};
