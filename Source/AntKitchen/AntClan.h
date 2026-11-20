#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "AntBotController.h"
#include "AntClan.generated.h"

class AAntClan;
class AAntRaid;
class AAntKitchenGameMode;
class AAntCharacter;
class AKitchenLoot;

/** What a red ant is in its clan, and what it is doing right now. */
namespace ClanRole { enum : uint8 { Scout, Hauler, Escort, Ambusher, Count }; }
namespace ClanStance { enum : uint8 { Free, Travel, Fetch, Carry, Escort, Hide, Rush, Flee }; }

/**
 * A red ant of the rival clan. The same body and the same rules as any bot ant (it carries, bites, rams, dies and hatches again
 * at its hill); the clan (AAntClan) tells it what to do: walk the route graph, take hold of the cake, wait in ambush, rush.
 * With nothing to do it forages like any other fire ant.
 */
UCLASS()
class ANTKITCHEN_API AAntClanBot : public AAntBotController
{
	GENERATED_BODY()

public:
	AAntClanBot();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

	uint8 Job = ClanRole::Scout;
	uint8 Stance = ClanStance::Free;
	/** Points to walk, in order (the route graph), and how far along it is. */
	TArray<FVector> Way;
	int32 WayAt = 0;
	/** Where to stand (an ambush spot, a place on the hill). */
	FVector Spot = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Prey;
	TWeakObjectPtr<AAntClan> Clan;
	float HealthMul = 1.f;
	/** World time until which it forages by itself (a thief with something to carry off). */
	float FreeUntil = 0.f;
	int32 ScoutSpot = 0;

	void GoAlong(const TArray<FVector>& Points, uint8 NewStance);
	void GoTo(const FVector& From, const FVector& To, uint8 NewStance, bool bSafe = true);
	bool Arrived() const { return WayAt >= Way.Num(); }
	AAntCharacter* Ant() const;

private:
	void ClanThink(AAntCharacter* Me);
	bool FollowWay(AAntCharacter* Me);
	float ClanTimer = 0.f;
	FVector ProgressAt = FVector::ZeroVector;
	float ProgressTime = 0.f;
	int32 StuckCount = 0;
	float NextDash = 0.f;
	float NextHop = 0.f;
	float BestToGo = 1.e9f;        // the closest it has been to the point it walks to
	float NearCakeSince = 0.f;     // world time it came close to the cake without getting hold of it
};

/**
 * The rival clan: the red ants of the hill in yard B play the same raid. Scouts look for the cake; a hauling party with
 * escorts carries it to the hill along the route graph; an ambush party waits at a junction on the players' way home, chosen
 * by what the clan has seen of them; they steal what is dropped, fall back when outnumbered, keep clear of the Human and the
 * pets. Server only; what players see of it is replicated by AAntRaid.
 */
UCLASS()
class ANTKITCHEN_API AAntClan : public AInfo
{
	GENERATED_BODY()

public:
	AAntClan();

	/** A raid begins: the roster for its tier (bots are made or reused), everybody at the hill. */
	void Begin(AAntKitchenGameMode* InGM, AAntRaid* InRaid);
	/** The raid is over: everybody forages again. */
	void Stop();
	/** The cake has just been put into the hill. */
	void OnCakeInHill();
	/** A pheromone decoy: red ants near it come and guard that spot for a while. */
	void Decoy(const FVector& At, float Seconds);
	bool DevCommand(const TArray<FString>& Args);

	AKitchenLoot* Cake() const;
	/** Where the carriers are heading right now. */
	FVector HaulGoal() const { return (HaulWay.IsValidIndex(HaulAt) && LowerIdx < 0) ? HaulWay[HaulAt] : FVector::ZeroVector; }
	/** The carrier the others walk with (the slowest of them: nobody is torn off the load). */
	AAntCharacter* HaulLeader() const { return Leader.Get(); }
	/** Something big and dangerous is close to this ant: the direction to back off in. */
	bool Avoid(const AAntCharacter* Ant, FVector& OutAway) const;
	AAntRaid* GetRaid() const { return Raid; }

	enum EPlan : uint8 { Idle, Scouting, Fetch, Haul, Chase, Guard, GiveUp };
	enum EAmbush : uint8 { AHome, AStaging, AHidden, ATell, ASprung, ARetreat };

private:
	void Think();
	void SpawnRoster();
	void CakeThink(AKitchenLoot* C, float Now);
	void AmbushThink(AKitchenLoot* C, float Now);
	void Sightings(AKitchenLoot* C, float Now);
	int32 ChooseAmbushNode(AKitchenLoot* C, float Now) const;
	void SetPlan(uint8 NewPlan, const TCHAR* Why);
	void BuildHaulWay(const FVector& From);
	/** Opens the container's door that is in front of a point (a pair of sliding doors: that one only). */
	void OpenFor(int32 Box, const FVector& At);
	/** Puts the cake somewhere, at rest and out of everybody's jaws; those who held it stand around it (they take hold again). */
	void PlaceCake(AKitchenLoot* C, const FVector& At);
	void Alive(uint8 Job, TArray<AAntClanBot*>& Out) const;
	AAntCharacter* PickPrey(const FVector& Near, float Range, AKitchenLoot* C) const;
	void Say(const FString& Text, bool bBad = true) const;

	UPROPERTY() AAntKitchenGameMode* GM = nullptr;
	UPROPERTY() AAntRaid* Raid = nullptr;
	UPROPERTY() TArray<AAntClanBot*> Bots;
	FTimerHandle ThinkTimer;

	uint8 Plan = Idle;
	float PlanSince = 0.f;
	bool bKnowsCake = false;
	bool bSaidSealed = false;
	TArray<FVector> HaulWay;
	int32 HaulAt = 0;
	FVector HaulProgressAt = FVector::ZeroVector;
	float HaulProgressTime = 0.f;
	int32 Nudges = 0;
	int32 NudgeFails = 0;
	// Getting the cake down from where it lies (a cabinet, the coffee table): the carriers hold it for a few seconds and it
	// is on the floor in front. (Crawling ants would lower it down the face; these, steering at a point, climbed the frame.)
	int32 LowerIdx = -1;           // the point of HaulWay that is the floor in front, while the lowering is still to be done
	float LowerSince = 0.f;
	FVector LastFloorAt = FVector::ZeroVector;
	TWeakObjectPtr<AAntCharacter> Leader;
	float LostSightAt = 0.f;
	float FirstGripAt = 0.f;

	// what they have seen of the players
	FVector SeenAt = FVector::ZeroVector;
	float SeenTime = -1000.f;
	bool bSeenWithCake = false;

	// the ambush party
	uint8 Ambush = AHome;
	int32 AmbushNode = -1;
	float AmbushSince = 0.f;
	float AmbushUntil = 0.f;
	int32 SprungWith = 0;
	int32 AmbushCount = 0;
	float NextRechoose = 0.f;

	FVector DecoyAt = FVector::ZeroVector;
	float DecoyUntil = 0.f;
};
