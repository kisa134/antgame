#pragma once

#include "CoreMinimal.h"
#include "AntCharacter.h"
#include "AIController.h"
#include "CreatureCharacters.generated.h"

class UAudioComponent;
class AKitchenLoot;

/**
 * Housefly. Built on the ant (health, teams, carrying, legs, HUD), but it FLIES:
 * look where you want to go, W to fly, Space to climb. Grabs light food and airlifts it.
 * As AI it's a thief: steals crumbs and drops them from high up (they smash), buzzes the Human.
 * Killed, it falls and becomes "Дохлая муха" - heavy protein, worth dragging home.
 */
UCLASS()
class ANTKITCHEN_API AFlyCharacter : public AAntCharacter
{
	GENERATED_BODY()
public:
	AFlyCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetHoldPoint() const override;

	bool bAscend = false;
	bool bDescend = false;

protected:
	virtual void BuildBody() override;
	virtual void MoveForward(float Value) override;
	virtual void MoveRight(float Value) override;
	virtual void DoJump() override;
	virtual void DoStopJump() override;

	UPROPERTY() TArray<USceneComponent*> WingPivots;
	UPROPERTY() UAudioComponent* BuzzAudio = nullptr;
	float FlapTime = 0.f;
};

/**
 * House spider: the kitchen's predator. Lives in a web high on the east wall.
 * Crawls on walls and ceilings like an ant (same movement), 8 long legs,
 * shoots silk that sticks ants in place, bites hard, drops from the ceiling on a thread.
 * Hunts both colonies. Killing it gives big loot. A player can take it over (K).
 */
UCLASS()
class ANTKITCHEN_API ASpiderCharacter : public AAntCharacter
{
	GENERATED_BODY()
public:
	ASpiderCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BuildBody() override;

	UPROPERTY() UStaticMeshComponent* Silk = nullptr;
	FVector SilkAnchor = FVector::ZeroVector;
	FVector LastCrawlNormal = FVector::UpVector;
	bool bOnSilk = false;
};

/** AI for flies: steal crumbs, drop them from high up, annoy the Human, flee when hurt. */
UCLASS()
class ANTKITCHEN_API AFlyAIController : public AAIController
{
	GENERATED_BODY()
public:
	AFlyAIController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

private:
	void PickWander(const FVector& From);
	TWeakObjectPtr<AKitchenLoot> Target;
	FVector Goal = FVector::ZeroVector;
	float StateTime = 0.f;
	float ThinkTimer = 0.f;
	float LastHealth = -1.f;
	float FleeUntil = 0.f;
	float NextSteal = 8.f;
	float Seed = 0.f;
	FRotator Desired = FRotator::ZeroRotator;
};
