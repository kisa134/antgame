#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "Critters.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class AAntCharacter;

/**
 * The household pets, built from primitives: Бадрик the French bulldog and a cat.
 * Server-driven: they sleep, wander between their spots, eat what lies by the bowl, and their paws kill ants.
 * The cat pounces on ants that come too close while it's awake and swats at flies.
 * Only location / rotation / State replicate; every machine animates legs, tail and ears itself.
 */
UCLASS()
class ANTKITCHEN_API ACritter : public AActor
{
	GENERATED_BODY()

public:
	ACritter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0 dog, 1 cat. */
	UPROPERTY(Replicated) uint8 Kind = 0;
	/** 0 sleep, 1 walk, 2 eat / sniff, 3 sit, 4 pounce (in the air). */
	UPROPERTY(Replicated) uint8 State = 0;
	FString DisplayName() const { return Kind == 0 ? TEXT("Бадрик") : TEXT("Кошка"); }

	float BodyLength() const { return Kind == 0 ? 950.f : 800.f; }
	float BodyHeight() const { return Kind == 0 ? 520.f : 440.f; }
	/** Server: an ant bit it. The dog yelps, shakes the riders off and after a few bites runs away to sulk;
	 *  the cat hisses and pounces straight back (or leaps away). */
	void Bitten(AAntCharacter* By);
	/** Server (the shift's event): the cat wakes up and stalks ants on the floor for that long. */
	void Hunt(float Seconds);

private:
	struct FSpot { FVector P; uint8 Do; float Time; };   // Do: 0 sleep, 1 sniff/eat, 2 sit
	void BuildVisual();
	void ServerThink(float Dt);
	void ServerMove(float Dt);
	void ServerHurtAnts(float Dt);
	void PickSpot();
	FVector PawWorld(int32 i) const;
	void AnimateVisual(float Dt);

	UPROPERTY() USceneComponent* Vis = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Legs;
	UPROPERTY() UStaticMeshComponent* Tail = nullptr;
	UPROPERTY() UStaticMeshComponent* Head = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Ears;
	UPROPERTY() TArray<UStaticMeshComponent*> Eyes;

	TArray<FSpot> Spots;
	int32 SpotIndex = -1;
	float StateTimer = 0.f;
	float ThinkTimer = 0.f;
	float HurtTimer = 0.f;
	float Speed = 0.f;
	FVector Goal = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	float Anim = 0.f;
	float Awake = 0.f;        // cat: seconds since it woke
	FVector PounceFrom, PounceTo;
	float PounceT = 0.f;
	float EatTimer = 0.f;
	int32 Patience = 3;
	float HuntUntil = 0.f;
	float NextHuntPounce = 0.f;
	float Fleeing = 0.f;
};

/**
 * People of the house besides the Human you fight: a kid on the sofa watching TV (drops crumbs,
 * swings his legs) and grandma asleep in the armchair (shifts her feet; a ball of yarn to climb).
 * Static primitives with a couple of timed habits; replicated so everyone sees the same pose.
 */
UCLASS()
class ANTKITCHEN_API AHouseholdNpc : public AActor
{
	GENERATED_BODY()

public:
	AHouseholdNpc();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0 kid on the sofa, 1 grandma in the armchair. */
	UPROPERTY(Replicated) uint8 Kind = 0;
	/** Kid: legs swinging (dangerous in front of the sofa). Grandma: which foot spot (0/1). */
	UPROPERTY(Replicated) uint8 Pose = 0;
	/** Set 1 s before the pose changes: the warning. */
	UPROPERTY(Replicated) bool bWarning = false;

private:
	void BuildVisual();
	UPROPERTY() USceneComponent* Vis = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Shins;
	UPROPERTY() TArray<UStaticMeshComponent*> Feet;
	UPROPERTY() UStaticMeshComponent* Chest = nullptr;
	float Timer = 0.f;
	float Habit = 20.f;
	float Anim = 0.f;
	float Swing = 0.f;
	float HurtTimer = 0.f;
};
