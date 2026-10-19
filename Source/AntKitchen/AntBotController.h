#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AntBotController.generated.h"

class AAntCharacter;
class AKitchenLoot;

/**
 * A bot ant, a teammate or a rival fire ant. Plays like a player:
 * grabs food and drags it home, helps others with heavy loot, spits at enemies and the Human.
 * No navmesh: ants walk on every surface, so it steers straight and climbs over furniture.
 */
UCLASS()
class ANTKITCHEN_API AAntBotController : public AAIController
{
	GENERATED_BODY()

public:
	AAntBotController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
	virtual void OnPossess(APawn* InPawn) override;
	/** Squad order from a player commander: 0 free, 1 follow him, 2 attack a point. */
	void SetOrder(uint8 Mode, AAntCharacter* By, const FVector& Point);
	/** War mode: a commander (leads an army column to an objective) or a squad member following one. */
	void SetWarRole(bool bCommander, AAntCharacter* Leader);
	void SetWarObjective(const FVector& Point, AActor* Obj);
	bool IsWarCommander() const { return bWarCommander; }
	bool IsFree() const { return OrderMode == 0 && !bWarCommander && !WarLeader.IsValid(); }

private:
	void Think(AAntCharacter* Ant);
	bool ThinkWar(AAntCharacter* Ant);
	bool ThinkChain(AAntCharacter* Ant);
	void Steer(AAntCharacter* Ant, const FVector& Goal, float Dt);
	void Shoot(AAntCharacter* Ant, float Dt);
	AKitchenLoot* PickFood(AAntCharacter* Ant) const;
	AActor* PickShootTarget(AAntCharacter* Ant) const;
	bool CanSee(AAntCharacter* Ant, const AActor* Target) const;

	TWeakObjectPtr<AKitchenLoot> FoodTarget;
	uint8 OrderMode = 0;
	TWeakObjectPtr<AAntCharacter> OrderBy;
	FVector OrderPoint = FVector::ZeroVector;
	float OrderUntil = 0.f;
	TWeakObjectPtr<AKitchenLoot> LastCarried;
	TWeakObjectPtr<AActor> ShootTarget;
	TMap<TWeakObjectPtr<AKitchenLoot>, float> IgnoreUntil;   // gave up on these (too heavy alone)

	FVector Goal = FVector::ZeroVector;
	bool bHasGoal = false;
	FRotator Desired = FRotator::ZeroRotator;
	float ThinkTimer = 0.f;
	float StuckCheckTimer = 0.f;
	FVector LastCheckPos = FVector::ZeroVector;
	float UnstickTime = 0.f;
	FVector UnstickDir = FVector::ZeroVector;
	float HeavyTimer = 0.f;     // carrying something that won't move
	FVector LastLootPos = FVector::ZeroVector;
	float StrafeSign = 1.f;
	float StrafeTimer = 0.f;
	bool bWarCommander = false;
	TWeakObjectPtr<AAntCharacter> WarLeader;
	TWeakObjectPtr<AActor> WarObj;
	FVector WarPoint = FVector::ZeroVector;
	bool bHasWarPoint = false;
	float AttachedFor = 0.f;
};
