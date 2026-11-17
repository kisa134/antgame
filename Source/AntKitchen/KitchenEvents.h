#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "KitchenEvents.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * Something an ant switches on to make noise and pull the Human away: the microwave (Kind 0: [E] on its front starts it,
 * it hums for five seconds and dings) and the TV (Kind 1: [E] on the console toggles it; it blares until somebody
 * switches it off). The Human walks over, looks around and turns it off. An ant shut inside a running microwave cooks.
 */
UCLASS()
class ANTKITCHEN_API AKitchenNoiseBox : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenNoiseBox();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	/** Server: the Human (or the end of the shift) turns it off. */
	void SwitchOff();

	UPROPERTY(Replicated) uint8 Kind = 0;
	UPROPERTY(Replicated) bool bOn = false;

private:
	UPROPERTY() UPointLightComponent* Light = nullptr;
	UPROPERTY() UStaticMeshComponent* Button = nullptr;
	float OffAt = 0.f;        // server: the microwave is done
	float NextNoise = 0.f;    // server
	float Anim = 0.f;
};

/**
 * The robot vacuum (an event): roams the kitchen floor for a while, bumping and turning. It swallows small loose food
 * and runs over ants in its way. Its flat top is a ride: an ant that climbs on is carried around the kitchen.
 */
UCLASS()
class ANTKITCHEN_API ARobotVacuum : public AActor
{
	GENERATED_BODY()

public:
	ARobotVacuum();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static constexpr float Radius = 330.f;
	static constexpr float Height = 170.f;
	/** Server: seconds left before it drives off. */
	float Life = 45.f;

private:
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Body;
	UPROPERTY() UStaticMeshComponent* Lamp = nullptr;
	FVector Heading = FVector(-1.f, 0.f, 0.f);
	float TurnLeft = 0.f;      // seconds of turning on the spot
	float TurnSign = 1.f;
	float EatClock = 0.f;
	int32 Eaten = 0;
};

/** The sink overflowed (an event): a shallow pool spreads over the floor in front of it. Ants wade and get wet, food floats off. */
UCLASS()
class ANTKITCHEN_API AKitchenFlood : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenFlood();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return false; }
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	UPROPERTY(Replicated) float PoolRadius = 100.f;
	float Life = 40.f;
	static constexpr float MaxRadius = 1700.f;
	static constexpr float Depth = 14.f;

private:
	UPROPERTY() UStaticMeshComponent* Water = nullptr;
	float Age = 0.f;
	float FxClock = 0.f;
};

/** Pizza delivery (an event): the box on the floor. The slices on it are ordinary loot that disappears with the box. */
UCLASS()
class ANTKITCHEN_API APizzaBox : public AActor
{
	GENERATED_BODY()

public:
	APizzaBox();
	virtual void BeginPlay() override;
	static constexpr float Half = 800.f;
	static constexpr float Thick = 60.f;

private:
	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
};