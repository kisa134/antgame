#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "KitchenArt.h"
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

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** KAN-40, a flat (AntLab): the variant this gadget was spawned in and the scale of its art group's home (KitchenArt::FHome::Tm).
	 *  A guest may get a gadget before the game state: the gadget makes the variant known and takes its scale before it builds itself. */
	UPROPERTY(Replicated) uint8 LabId = 0;
	UPROPERTY(Replicated) FVector HomeScale = FVector::OneVector;

	// ---- Doors (AntDoor in AntRun.h): the heavy ones need several ants holding [E] together ----
	/** AntDoor::EClass of this thing; 0 = not a door, a plain [E]. */
	virtual int32 DoorClass() const { return 0; }
	virtual bool IsDoorOpen() const { return true; }
	/** Server: open or shut it for good (no timer), whoever did it: ants, the Human, an event. */
	virtual void SetDoorOpen(bool bInOpen) {}
	/** The Human shuts this when he notices it open. */
	virtual bool HumanShuts() const { return false; }
	/** What it is, as in "идёт закрыть <что>": "холодильник", "дверцу шкафа". */
	virtual FString DoorNoun() const { return TEXT("дверь"); }
	/** May this ant pull it from where it stands? (The cat flap is latched: it only opens from the living-room side.) Server: says why not. */
	virtual bool CanPull(AAntCharacter* Ant, bool bTell) const { return true; }
	/** Server: Ant starts pulling ([E] pressed). True = it opened right away; otherwise it pulls until [E] is let go. */
	bool Pull(AAntCharacter* Ant);
	/** Strength of everyone pulling right now: ants holding [E], idle bots of theirs at the door, their armies. */
	float PullStrength(int32* OutHands = nullptr, AAntCharacter** OutFirst = nullptr) const;
	/** Strength it takes right now (the «Рычаг» card makes doors one class lighter). */
	float PullNeed() const;
	/** How many more workers it takes; 0 = nobody pulls. Replicated: the door strains, the HUD says how many are missing. */
	UPROPERTY(Replicated) uint8 PullMissing = 0;
	/** Server world time it was last opened (the Human notices doors left open). */
	float OpenedAt = -100.f;
	/** Server: a raid squad stands at it and keeps it open until this world time (refreshed while it holds). Nothing shuts it meanwhile. */
	float HeldOpenUntil = -100.f;
	bool IsHeldOpen() const;
	/** Could it be pulled open by something standing at P? (The cat flap: only from the living-room side.) */
	virtual bool CanPullFrom(const FVector& P) const { return true; }

protected:
	/** Server, from the door's Tick: ants still pulling, helpers arriving. */
	void PullTick(float Dt);
	virtual bool TryOpenByPull();
	/** Every machine, from the door's Tick: how far (0..1 of its travel) the straining door is cracked open right now. */
	float StrainNow(float Dt);
	float PullClock = 0.f;
	float StrainT = 0.f;
	float StrainSfxAt = 0.f;

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
	Dishwasher,   // folds down into a ramp like the oven; wet racks of dirty plates inside
	Microwave,    // swings sideways on the left edge; a plate of leftovers inside
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
	virtual int32 DoorClass() const override;
	virtual bool IsDoorOpen() const override { return bOpen; }
	virtual void SetDoorOpen(bool bInOpen) override { SetOpen(bInOpen); }
	virtual bool HumanShuts() const override { return true; }
	virtual FString DoorNoun() const override;

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
	/** Server: the Human is told about it (the floor floods): he comes to shut it. */
	void Alarm() { if (bOn) { bNoisy = true; NextNoise = 0.f; } }
	/** The real water (UAntFluidWorld) draws the stream and the sink; the old stand-ins are off. */
	static bool RealWater();

	UPROPERTY(Replicated) bool bOn = false;
	UPROPERTY(Replicated) bool bHot = false;
	UPROPERTY(Replicated) float WaterLevel = 1150.f;

	static constexpr float BasinFloor = 1150.f;
	static constexpr float BasinTop = 1620.f;
	// (KAN-40: where the sink unit stands in this world; the numbers are the kitchen's)
	static FBox Basin() { return KitchenArt::Home(TEXT("sink")).Box(FBox(FVector(-4100.f, 3300.f, BasinFloor), FVector(-2900.f, 4700.f, BasinTop))); }
	static FVector Spout() { return KitchenArt::Home(TEXT("sink")).At(FVector(-3500.f, 4400.f, 2060.f)); }
	static FVector HotValve() { return KitchenArt::Home(TEXT("sink")).At(FVector(-3750.f, 4800.f, 1830.f)); }
	static FVector ColdValve() { return KitchenArt::Home(TEXT("sink")).At(FVector(-3250.f, 4800.f, 1830.f)); }

private:
	UPROPERTY() UStaticMeshComponent* Water = nullptr;
	UPROPERTY() UStaticMeshComponent* StreamMesh = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* WaterMID = nullptr;
	UPROPERTY() class UAudioComponent* FlowAudio = nullptr;
	float ShownLevel = 1150.f;
	bool bNoisy = false;      // server: an ant turned it on (the Human's own dish-washing does not attract him)
	float NextNoise = 0.f;
	float SplashAccum = 0.f;   // cosmetic: droplets, ripples and steam where the stream lands
	float RippleAccum = 0.f;
	/** Modelled kitchen: the hot and the cold tap, turned while their water runs. */
	UPROPERTY() TArray<UStaticMeshComponent*> Taps;
	TArray<FQuat> TapBase;
	float TapTurn[2] = { 0.f, 0.f };
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
	/** Server: armed without an ant (the chain C13, dev). */
	void ForceArm();

	static FBox Slots();

private:
	void Pop();
	UFUNCTION(NetMulticast, Unreliable) void MulticastPop();

	UPROPERTY() USceneComponent* Lever = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Toasts;
	float PopAt = 0.f;
	float ReadyAt = 0.f;
	/** Level v2: after it has dropped toast it bakes none for 40 s. */
	float NextFoodAt = 0.f;
	float ToastAnim = 0.f;
	bool bToastArt = false;   // the slices are the modelled ones (origin at the bottom)
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

	static FBox Puddle() { return KitchenArt::Home(TEXT("wire")).Box(FBox(FVector(2900.f, 3700.f, -50.f), FVector(4500.f, 4950.f, 70.f))); }

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
	/** What it is, for the feed when it lands on somebody ("Банка"). Empty: just junk. */
	UPROPERTY(Replicated) FString PropName;
	/** Server (dev tests): counts as pushed over by an ant even though nobody touched it. */
	bool bDevPushed = false;
	/** Server: an ant dashes into it. True if it was in reach and got shoved along Dir (harder the stronger the ant). */
	bool TryShove(AAntCharacter* By, const FVector& From, const FVector& Dir, float Reach);
	/** Server: the floor jumped under it (the Human fell nearby): a kick of this speed, uu/s. Not counted as pushed by an ant. */
	void Jolt(const FVector& Velocity, AActor* By);

	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION() void OnPropHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	/** Server: the ant that pushed it last (who dropped it on the Human). */
	TWeakObjectPtr<AAntCharacter> LastPusher;
	FVector LastVelocity = FVector::ZeroVector;
	float NextCrash = 0.f;
};
