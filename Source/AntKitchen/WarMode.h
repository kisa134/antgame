#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KitchenInteractables.h"
#include "WarMode.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * "Муравьиная война": a piece of the apartment worth holding.
 * Stand in it with more forces than the enemy -> the pheromone mark turns your color.
 * An owned zone feeds the colony through a living supply line (workers walking nest <-> zone):
 * points + reinforcements every 10 s. Enemies standing on the line cut it.
 */
UCLASS()
class ANTKITCHEN_API AAntTerritory : public AActor
{
	GENERATED_BODY()

public:
	AAntTerritory();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) FString ZoneName;
	UPROPERTY(Replicated) float Radius = 1000.f;
	/** 255 = nobody, 0 colony, 1 fire ants. */
	UPROPERTY(Replicated) uint8 OwnerTeam = 255;
	/** +1 fully colony .. -1 fully fire ants. */
	UPROPERTY(Replicated) float Capture = 0.f;
	UPROPERTY(Replicated) bool bContested = false;
	UPROPERTY(Replicated) bool bLineCut = false;

	bool Contains(const FVector& P) const;
	/** Forces of both colonies in a circle (players/bots count 3, army workers 1/4, flies 1). */
	static void Presence(UWorld* World, const FVector& C, float R, float ZTol, float& Out0, float& Out1);
	void ResetZone();

	static constexpr float IncomeEvery = 10.f;

private:
	void ServerTick(float Dt);
	void BuildLine();
	void UpdateVisual(float Dt);

	UPROPERTY() UStaticMeshComponent* Ring = nullptr;
	UPROPERTY() UStaticMeshComponent* Pole = nullptr;
	UPROPERTY() UStaticMeshComponent* Flag = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* Lane = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* Crumbs = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* RingMID = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* FlagMID = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* LaneMID = nullptr;

	TArray<FVector> Line;        // nest -> zone, on the ground
	TArray<float> LineDist;
	float LineLen = 0.f;
	uint8 LineFor = 254;
	float LineDelay = 1.f;
	float Accum = 0.f;
	float IncomeTimer = 0.f;
	float CutCheck = 0.f;
	float InstScale = 0.3f;
	bool bVisualBuilt = false;
	uint8 LastOwner = 255;
	bool bWasCut = false;
};

/**
 * Cashout ("Касса"): bring the sugar-bowl safe here, then hold the spot while it counts.
 * Enemies who outnumber you on the spot steal it (it starts counting for them).
 */
UCLASS()
class ANTKITCHEN_API AAntCashout : public AActor
{
	GENERATED_BODY()

public:
	AAntCashout();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) FString ZoneName;
	UPROPERTY(Replicated) uint8 CashTeam = 255;
	UPROPERTY(Replicated) float Progress = 0.f;   // 0..1
	UPROPERTY(Replicated) bool bActive = false;
	UPROPERTY(Replicated) bool bContested = false;

	static constexpr float Radius = 420.f;
	static constexpr float Duration = 45.f;
	static constexpr int32 Reward = 400;
	void ResetStation();

private:
	void ServerTick(float Dt);
	void BuildVisual();

	UPROPERTY() UStaticMeshComponent* Base = nullptr;
	UPROPERTY() UStaticMeshComponent* Bowl = nullptr;
	UPROPERTY() UStaticMeshComponent* RingC = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* RingMID = nullptr;
	UPROPERTY() UPointLightComponent* Light = nullptr;
	bool bBuilt = false;
};

/** Dog poop on the lawn / the trash: a fly eats there (heals, refills the afterburner). */
UCLASS()
class ANTKITCHEN_API AFlyFeeder : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AFlyFeeder();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;

	/** 0 dog poop, 1 trash. */
	UPROPERTY(Replicated) uint8 Kind = 0;
};
