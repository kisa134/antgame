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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool bAscend = false;
	bool bDescend = false;
	/** Afterburner fuel (Shift). Refilled by eating dog poop / trash. */
	UPROPERTY(Replicated) float Fuel = 100.f;
	bool IsBoosting() const { return bSprinting && Fuel > 1.f && !bDead && !IsCrawling(); }

protected:
	virtual void BuildBody() override;
	virtual void MoveForward(float Value) override;
	virtual void MoveRight(float Value) override;
	virtual void DoJump() override;
	virtual void DoStopJump() override;

	UPROPERTY() TArray<USceneComponent*> WingPivots;
	UPROPERTY() UAudioComponent* BuzzAudio = nullptr;
	float FlapTime = 0.f;
	float BoostFov = 0.f;
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Spider-Man: shoot a silk line at a surface and swing on it. */
	UPROPERTY(Replicated) bool bSwinging = false;
	UPROPERTY(Replicated) FVector_NetQuantize SwingAnchor;
	/** Local (owner / AI on the server): try to start a swing towards this point. */
	bool StartSwing(const FVector& Target);
	void StopSwing(bool bFling);

protected:
	virtual void BuildBody() override;
	virtual void AltFirePressed() override;
	virtual void AltFireReleased() override;
	virtual void DoJump() override;
	virtual void DoStopJump() override;
	UFUNCTION(Server, Reliable) void ServerSetSwing(bool bOn, FVector_NetQuantize Anchor);
	void SpiderBite() { Bite(); }
	void TickSwing(float Dt);
	float RopeLen = 0.f;
	bool bReel = false;

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
	bool Hungry = false;
	float Seed = 0.f;
	FRotator Desired = FRotator::ZeroRotator;
};
