#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "KitchenInteractables.generated.h"

class AAntCharacter;
struct FAntEnv;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 * Base class for everything in the kitchen an ant can use ([E]) or that affects ants.
 * Spawned on the server by the GameMode and replicated; each machine builds the visuals itself.
 */
UCLASS(Abstract)
class ANTKITCHEN_API AKitchenInteractable : public AActor
{
	GENERATED_BODY()

public:
	AKitchenInteractable();

	virtual FString GetPrompt(const AAntCharacter* Ant) const { return FString(); }
	virtual bool CanInteract(const AAntCharacter* Ant) const { return true; }
	/** Server. */
	virtual void Interact(AAntCharacter* Ant) {}
	/** Server, 4x per second for every living ant: add heat, cold, water, damage... */
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const {}
	/** Distance from P to the closest spot this thing can be used from. */
	virtual float DistanceTo(const FVector& P) const;

	float InteractRadius = 230.f;

protected:
	UStaticMeshComponent* Part(USceneComponent* Parent, EAntShape Shape, const FVector& Center, const FVector& Size,
		const FLinearColor& Color, bool bCollide, const FRotator& Rot = FRotator::ZeroRotator);
	/** Box given by local min/max corners. */
	UStaticMeshComponent* BoxPart(USceneComponent* Parent, const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bCollide);
	void Broadcast(const FString& Text, const FLinearColor& Color) const;
	float Now() const;

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	TArray<FVector> UsePointsLocal;
};

// ---------------------------------------------------------------------------------------------

UENUM()
enum class EKitchenDoorKind : uint8
{
	Fridge,
	Oven,
	Drawer,
};

/** Fridge door (swings), oven door (folds down into a ramp), drawer (slides out). */
UCLASS()
class ANTKITCHEN_API AKitchenDoor : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenDoor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	/** Server: open/close; AutoClose < 0 keeps it open. */
	void SetOpen(bool bInOpen, float AutoClose = -1.f);
	bool IsOpen() const { return bOpen; }

	UPROPERTY(Replicated) EKitchenDoorKind Kind = EKitchenDoorKind::Fridge;
	UPROPERTY(Replicated) bool bOpen = false;

private:
	void Build();
	void ApplyAlpha();

	UPROPERTY() USceneComponent* Hinge = nullptr;
	UPROPERTY() UPointLightComponent* InnerLight = nullptr;

	float Alpha = 0.f;
	float AutoCloseAt = -1.f;
	bool bLootSpawned = false;
};

// ---------------------------------------------------------------------------------------------

/** The sink faucet: hot/cold valves, a water stream and a basin that fills up. */
UCLASS()
class ANTKITCHEN_API AKitchenFaucet : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenFaucet();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	/** Server: Human turns the tap on/off. */
	void SetWater(bool bOn, bool bHotWater);
	bool IsOn() const { return bOn; }

	UPROPERTY(Replicated) bool bOn = false;
	UPROPERTY(Replicated) bool bHot = false;
	UPROPERTY(Replicated) float WaterLevel = 1150.f;

	static constexpr float BasinFloor = 1150.f;
	static constexpr float BasinTop = 1620.f;
	static FBox Basin() { return FBox(FVector(-4100.f, 3300.f, BasinFloor), FVector(-2900.f, 4700.f, BasinTop)); }
	static FVector Spout() { return FVector(-3500.f, 4400.f, 2060.f); }
	static FVector HotValve() { return FVector(-3750.f, 4800.f, 1830.f); }
	static FVector ColdValve() { return FVector(-3250.f, 4800.f, 1830.f); }

private:
	UPROPERTY() UStaticMeshComponent* Water = nullptr;
	UPROPERTY() UStaticMeshComponent* StreamMesh = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* WaterMID = nullptr;
	float ShownLevel = 1150.f;
	mutable TMap<TWeakObjectPtr<AAntCharacter>, float> LastKnock;
};

// ---------------------------------------------------------------------------------------------

/** Wall light switch: lights off = the Human can barely see ants. */
UCLASS()
class ANTKITCHEN_API AKitchenSwitch : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenSwitch();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;

	void SetLights(bool bOn);
	static bool AreLightsOn(const UWorld* World);

	UPROPERTY(ReplicatedUsing = OnRep_Lights) bool bLightsOn = true;

private:
	UFUNCTION() void OnRep_Lights();
	UPROPERTY() USceneComponent* Toggle = nullptr;
};

// ---------------------------------------------------------------------------------------------

UENUM()
enum class EPassageLook : uint8
{
	Vent,
	Drain,
	Hole,
	Pipe,
};

/** A secret route: crawl in here, come out over there. */
UCLASS()
class ANTKITCHEN_API AKitchenPassage : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenPassage();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;

	UPROPERTY(Replicated) FVector Exit = FVector::ZeroVector;
	UPROPERTY(Replicated) FString RouteName;
	UPROPERTY(Replicated) EPassageLook Look = EPassageLook::Vent;
};

// ---------------------------------------------------------------------------------------------

/** Toaster: pull the lever, wait, and get catapulted to the upper cabinets. */
UCLASS()
class ANTKITCHEN_API AKitchenToaster : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenToaster();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	UPROPERTY(Replicated) bool bArmed = false;

	static FBox Slots() { return FBox(FVector(-6500.f, 4000.f, 2100.f), FVector(-6000.f, 4400.f, 2400.f)); }

private:
	void Pop();
	UFUNCTION(NetMulticast, Unreliable) void MulticastPop();

	UPROPERTY() USceneComponent* Lever = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Toasts;
	float PopAt = 0.f;
	float ReadyAt = 0.f;
	float ToastAnim = 0.f;
};

// ---------------------------------------------------------------------------------------------

/** Frayed wire behind the fridge: sparks now and then, electrifies the drip puddle. */
UCLASS()
class ANTKITCHEN_API AKitchenWire : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenWire();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return false; }
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	static FBox Puddle() { return FBox(FVector(2900.f, 3700.f, -50.f), FVector(4500.f, 4950.f, 70.f)); }

private:
	void Spark();
	UFUNCTION(NetMulticast, Unreliable) void MulticastSpark();

	UPROPERTY() UPointLightComponent* Flash = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Sparks;
	float NextSpark = 0.f;
	float FlashT = 0.f;
	float WarnAt = 0.f;
};

// ---------------------------------------------------------------------------------------------

/** Spring mouse trap with cheese bait. */
UCLASS()
class ANTKITCHEN_API AKitchenTrap : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenTrap();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return false; }

	UPROPERTY(ReplicatedUsing = OnRep_Armed) bool bArmed = true;

private:
	UFUNCTION() void OnRep_Armed();
	UPROPERTY() USceneComponent* Bar = nullptr;
	float RearmAt = 0.f;
	float BarAlpha = 0.f;
};

// ---------------------------------------------------------------------------------------------

/** A small physics object: pushable by ants, knocked around by acid. */
UCLASS()
class ANTKITCHEN_API AKitchenProp : public AActor
{
	GENERATED_BODY()

public:
	AKitchenProp();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) uint8 ShapeIndex = 1;
	UPROPERTY(Replicated) FVector Size = FVector(60.f);
	UPROPERTY(Replicated) FLinearColor Color = FLinearColor::White;
	UPROPERTY(Replicated) float Mass = 2.f;

private:
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
};
