#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "KitchenGadgets.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * The stove knob: turns the front burner on and off. On, the burner glows and the pot of soup on it
 * comes to the boil after a while - hot steam above it, bubbles, and the soup itself is scalding.
 * Off, the stove top is just a warm metal plain (and the pot a hiding place).
 */
UCLASS()
class ANTKITCHEN_API AKitchenKnob : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenKnob();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	UPROPERTY(Replicated) bool bOn = false;
	/** 0..1, how close the soup is to boiling (server; replicated for the bubbles). */
	UPROPERTY(Replicated) float Boil = 0.f;
	static bool IsBurnerOn(UWorld* World);

private:
	UPROPERTY() UStaticMeshComponent* Knob = nullptr;
	UPROPERTY() UStaticMeshComponent* Coil = nullptr;
	UPROPERTY() UStaticMeshComponent* Soup = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Bubbles;
	UPROPERTY() UPointLightComponent* Glow = nullptr;
	float Anim = 0.f;
};

/** The pedal bin: step on the pedal and the lid swings up for a while; inside, the good stuff. */
UCLASS()
class ANTKITCHEN_API AKitchenPedalBin : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenPedalBin();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;

	UPROPERTY(Replicated) bool bOpen = false;

private:
	UPROPERTY() USceneComponent* Hinge = nullptr;
	UPROPERTY() UStaticMeshComponent* Pedal = nullptr;
	float Alpha = 0.f;
	float CloseAt = -1.f;
};

/**
 * A desk fan on the counter. Press the button: it spins for a while and blows a strong wind in front
 * of it - ants are pushed back (hold on to the counter edge...), crumbs fly off the counter.
 * Pushing is done on each owner's machine (movement is owner-authoritative).
 */
UCLASS()
class ANTKITCHEN_API AKitchenFan : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenFan();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;

	UPROPERTY(Replicated) bool bOn = false;

private:
	UPROPERTY() USceneComponent* Head = nullptr;
	UPROPERTY() USceneComponent* Blades = nullptr;
	float Spin = 0.f;
	float OffAt = -1.f;
	float Sweep = 0.f;
};
