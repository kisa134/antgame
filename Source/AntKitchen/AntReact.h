#pragma once

#include "CoreMinimal.h"
#include "KitchenFixture.h"
#include "AntReact.generated.h"

class AAntCharacter;
class AHumanCharacter;
class AKitchenLoot;
class AKitchenToaster;
class UStaticMeshComponent;
class UPointLightComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;
class AAntKitchenGameMode;
class AHUD;
class UCanvas;
class APlayerController;

/**
 * Materials and chain reactions 2.0 (docs/MATERIALS.md, docs/FEATURES.md section 37).
 *
 * One table of what things are made of (AntMaterials.cpp, Config/AntMaterials.json): grip, slip, stickiness, how much a thing
 * soaks up, whether it floats, conducts, melts, dissolves, rolls, bounces, holds a static charge, how it sounds. The systems
 * read it: the water (what floats), the heat (what melts), the electricity (what conducts), the Human's feet (what rolls),
 * the admin's «mat info» at the crosshair.
 *
 * On top of it the flat reacts by itself (AAntReactNet, server-authoritative, one per world):
 *  - heat: radiators, the burner when it is on, the toaster, the running microwave melt chocolate, butter, ice cream and ice;
 *  - powders: pepper and flour make clouds (pepper: everybody sneezes; flour: the Human is blinded, ants get white and leave prints);
 *  - electricity: water reaching the power strip, or a chewed cable, shorts it: the breaker trips, the flat goes dark, the
 *    fridge stops humming, appliances die, the Dad walks to the breaker box in the hall; ants in the live water are zapped;
 *  - rolling things (peas, grapes) under the Dad's feet: he goes down like on marbles;
 *  - soaking (the sponge, bread), dissolving (sugar), static (a balloon rubbed on the carpet sticks to the ceiling).
 * And the new reactive objects (AAntReactProp) the ants set off with the verbs they have ([E] together, push, bite, carry):
 * the chains C7..C20.
 */

// ------------------------------------------------------------------------------------------------------------------------
// The material table
// ------------------------------------------------------------------------------------------------------------------------

struct ANTKITCHEN_API FAntMatProps
{
	FString Id;                 // "wood"
	FString Ru;                 // "дерево"
	TArray<FString> Match;      // lower-case pieces of mesh names, look keys, Surf_ tags or food names that mean this material
	float AntGrip = 1.f;        // ants on it: 1 normal, < 1 they slide off its walls, > 1 velcro
	float HumanFriction = 1.f;  // a slipper on it, dry
	float WetSlip = 0.f;        // 0..1: how slippery it gets wet (the Dad's chance to go down)
	float Sticky = 0.f;         // 0..1: slows ants and feet
	float Absorb = 0.f;         // soaks liquid: litres per litre of itself (0: does not)
	float Density = 1.f;        // relative to water: < 1 floats
	float Conduct = 0.f;        // electricity 0..1
	float HeatTake = 0.3f;      // how fast it takes the heat of what it lies on, 0..1
	float MeltAt = -1000.f;     // °C it melts at (-1000: never)
	FString MeltsInto;          // "water", "honey", "oil", "cola"
	FString Stain;              // AntMess kind it leaves ("chocolate", "flour"...)
	bool bDissolves = false;    // in water (sugar, salt)
	bool bPowder = false;       // spills as a cloud
	bool bRolls = false;        // round: rolls on slopes and under feet
	bool bMagnetic = false;     // a magnet holds it
	float Static = 0.f;         // 0..1: rubbed, it charges (wool, carpet, latex)
	float Elastic = 0.f;        // 0..1: bounces, springs back (rubber: a catapult)
	FString Breaks;             // break class (docs/level/LEVEL_V3.md section 5): glass, ceramic, terracotta, ...
	FString Sound;              // AntAudio id of its knock / step
	float Noise = 0.f;          // how loud it is when it falls (0..1: the residents come)
	FString Does;               // what it does in the game, one line (Russian)
	// ---- matter (KAN-21, AntMatter.h): fire, water, the body of the thing
	float IgniteAt = -1.f;      // °C it catches fire at (-1: never burns)
	float BurnRate = 0.f;       // cm/s the flame front runs over it (upwards ~3x faster)
	FString BurnsInto;          // "ash" (burns away), "char" (stays black), "melt" (softens, drips, acrid smoke), "toast" (food: browns first)
	float WetSpeed = 0.3f;      // how fast it takes liquid it touches, 0..1 of full per second (a sponge 1, wood 0.03)
	float DrySpeed = 0.01f;     // how fast it dries in room air, 0..1 per second (faster when hot)
	float Stiffness = 0.6f;     // 0 limp (cloth, paper) .. 1 rigid (metal, glass)
	float Tear = 0.5f;          // tear strength 0..1 (wet paper and cardboard lose most of it)
	float Crumbly = 0.f;        // 0..1 falls apart (soaked bread, cake)
	float Plastic = 0.f;        // 0..1 plasticity: how soft it gets when hot (butter, chocolate, plastic)
	float SpoilRate = 0.f;      // food: 0..1 per minute it goes off at room temperature (x2 warm, /4 in the fridge)
	float Juicy = 0.f;          // food: 0..1 how much juice it gives when bitten or crushed
	FString Smell;              // food: who it draws ("ants,fly,dog,cat")
};

namespace AntMaterials
{
	/** The table: the defaults in code, then Config/AntMaterials.json over them (by id; new ids are added). */
	ANTKITCHEN_API void Load();
	ANTKITCHEN_API TConstArrayView<FAntMatProps> All();
	ANTKITCHEN_API const FAntMatProps* Find(const FString& Id);
	/** The material of a mesh name, look key, Surf_ tag or food name (first row whose Match piece it contains), or null. */
	ANTKITCHEN_API const FAntMatProps* ForName(const FString& Name);
	/** The material of a component (its tags, its mesh, its first material's look); OutWhy says what decided it. */
	ANTKITCHEN_API const FAntMatProps* ForComponent(const UPrimitiveComponent* C, FString* OutWhy = nullptr);
	/** Buoyancy for the water (UAntFluidWorld): lift per weight of a piece of food by its name (> 1 floats). */
	ANTKITCHEN_API float FloatOf(const FString& LootName);
	/** Lower-case including Cyrillic (FString::ToLower folds only Latin letters). */
	ANTKITCHEN_API FString Lower(const FString& S);
	/** One-line description of all its properties (for «mat info» and the docs). */
	ANTKITCHEN_API FString Describe(const FAntMatProps& M);
	/** Writes the table as JSON (Saved/ or Config/). */
	ANTKITCHEN_API bool DumpJson(const FString& Path);
}

// ------------------------------------------------------------------------------------------------------------------------
// Replicated state
// ------------------------------------------------------------------------------------------------------------------------

/** Smoke (KAN-21): the acrid smoke of melting plastic and of fires - drawn dark, makes people and pets sneeze like pepper. */
namespace AntCloudKind { enum : uint8 { Pepper = 0, Flour = 1, Steam = 2, Static = 3, Smoke = 4, Count }; }

/** A cloud of powder (or steam) hanging in the air: every machine draws it from these numbers. */
USTRUCT()
struct FAntReactCloud
{
	GENERATED_BODY()
	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() uint8 Id = 0;
	UPROPERTY() FVector_NetQuantize Center = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize Drift = FVector::ZeroVector;   // uu/s
	UPROPERTY() float Radius = 600.f;
	UPROPERTY() float Born = 0.f;                                   // server time
	UPROPERTY() float Life = 10.f;
	/** Where the powder comes from while it pours (an avalanche over an edge): Born .. Born + PourFor. */
	UPROPERTY() FVector_NetQuantize Source = FVector::ZeroVector;
	UPROPERTY() float PourFor = 0.f;
};

/** Something marked on a body for a while: 0 floured (white, leaves prints), 1 burnt (zapped), 2 blinded (the Human), 3 sneezing. */
USTRUCT()
struct FAntReactMark
{
	GENERATED_BODY()
	UPROPERTY() AActor* Who = nullptr;
	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() float Until = 0.f;
};

/** A piece of ice / chocolate / butter melting where it lies (drawn shrinking on every machine). */
USTRUCT()
struct FAntReactMelt
{
	GENERATED_BODY()
	UPROPERTY() uint8 Kind = 0;           // 0 ice cube, 1 chocolate, 2 butter, 3 ice cream
	UPROPERTY() FVector_NetQuantize Pos = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0.f;
	UPROPERTY() float Born = 0.f;
	UPROPERTY() float Life = 30.f;        // seconds to melt away at the heat it has now (server keeps it up to date)
	UPROPERTY() float Left = 1.f;         // 0..1 left (server)
};

namespace AntMark { enum : uint8 { Floured = 0, Burnt = 1, Blind = 2, Sneeze = 3, Charged = 4 }; }

/**
 * The world's reactions (server-authoritative; one per world, spawned by AntReact::ResetAll). Also a kitchen "gadget":
 * AffectAnt gives the ants heat near the hot things, cold by the ice, the zapped status.
 */
UCLASS()
class ANTKITCHEN_API AAntReactNet : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AAntReactNet();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return false; }
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;
	virtual float DistanceTo(const FVector& P) const override { return 1.e9f; }

	static AAntReactNet* Get(const UWorld* W);

	/** The breaker: the flat has power. */
	UPROPERTY(ReplicatedUsing = OnRep_Power) bool bPower = true;
	/** Times it tripped this raid. */
	UPROPERTY(Replicated) uint8 Trips = 0;
	/** Server time the power went (the blackout started) / came back. */
	UPROPERTY(Replicated) float PowerAt = -100.f;
	/** The power strip on the kitchen floor is unplugged (the Dad pulled it out after it tripped twice). */
	UPROPERTY(Replicated) bool bStripUnplugged = false;
	UPROPERTY(Replicated) TArray<FAntReactCloud> Clouds;
	UPROPERTY(Replicated) TArray<FAntReactMark> Marks;
	UPROPERTY(Replicated) TArray<FAntReactMelt> Melts;
	/** Admin: heat sources on (-1 as the game has it, 0 all cold, 1 every one hot). */
	UPROPERTY(Replicated) int8 HeatMode = -1;

	UFUNCTION() void OnRep_Power();

	// ---- server
	void SetPower(bool bOn, const TCHAR* Why, const FVector& Where);
	/** A short circuit at Where: sparks, the live water zaps the ants in it, the breaker trips. */
	void Short(const FVector& Where, const TCHAR* Why, AAntCharacter* By);
	int32 AddCloud(uint8 Kind, const FVector& Center, float Radius, float Life, const FVector& Drift, const FVector& Source = FVector::ZeroVector, float PourFor = 0.f, const TCHAR* Chain = nullptr);
	/** 0..1 how thick the clouds of Kind are at P now (the thickest one); OutId = that cloud's id, OutCenter its centre now. */
	float CloudThickness(uint8 Kind, const FVector& P, uint8* OutId = nullptr, FVector* OutCenter = nullptr, float* OutRadius = nullptr) const;
	/** Server: a cloud is gone at once (the flour that went up in a flash). */
	void EndCloud(uint8 Id);
	/** Server: which chain made each cloud (for the log). */
	TMap<uint8, FString> CloudChain;
	void Mark(AActor* Who, uint8 Kind, float Seconds);
	bool HasMark(const AActor* Who, uint8 Kind) const;
	void AddMelt(uint8 Kind, const FVector& Pos, float Life);
	void Zap(AAntCharacter* Ant, float Damage, const TCHAR* Why);
	/** Server: the Dad (or anyone) sneezes. */
	void Sneeze(AActor* Who, const TCHAR* Why);

	UFUNCTION(NetMulticast, Unreliable) void MulticastSparks(FVector_NetQuantize At, uint8 Count);
	UFUNCTION(NetMulticast, Unreliable) void MulticastPuff(uint8 Kind, FVector_NetQuantize At, float Size);

	float NetNow() const;
	FString Describe() const;

	/** Server: chain bookkeeping (docs/MATERIALS.md): when each chain started and what it did, for the log and the tests. */
	void ChainLog(const TCHAR* Chain, const FString& What);
	TMap<FString, float> ChainStart;

	// ---- server state of the systemic rules
	struct FPeaWatch { TWeakObjectPtr<AKitchenLoot> Loot; float Born = 0.f; };
	TArray<FPeaWatch> Rollers;
	TMap<TWeakObjectPtr<AKitchenLoot>, float> LootHeat;     // seconds a piece has been on something hot
	TMap<TWeakObjectPtr<AKitchenLoot>, float> LootWet;      // seconds a piece has been in water
	/** The material of each piece of food (looked up once: the name never changes). */
	const FAntMatProps* LootMat(const AKitchenLoot* L);
	TMap<TWeakObjectPtr<const AKitchenLoot>, const FAntMatProps*> MatCache;
	TMap<TWeakObjectPtr<AActor>, float> SneezeAcc;          // seconds in a pepper cloud
	TMap<TWeakObjectPtr<AActor>, float> NextSneeze;
	float MicroUntil = -1.f;                                // the microwave runs until
	float MicroBeepAt = -1.f;
	int32 MicroBeeps = 0;
	TWeakObjectPtr<AAntCharacter> MicroBy;
	float NextPowerCheck = 0.f;
	float WetStripSince = -1.f;
	float StripResetAt = -100.f;
	int32 PowerMess = -1;
	float DishDripUntil = -1.f;
	bool bDishWasOpen = false;
	float NextRuleTick = 0.f;
	FVector ShortAt = FVector::ZeroVector;
	float LiveWaterUntil = -1.f;
	float SweetAt = -1.f;
	FVector SweetPos = FVector::ZeroVector;

private:
	void ServerTick(float Dt);
	void TickClouds(float Dt);
	void TickHeat(float Dt);
	void TickPower(float Dt);
	void TickRollers(float Dt);
	void TickWet(float Dt);
	void TickMicrowave(float Dt);
	void TickVacuums(float Dt);
	void LocalTick(float Dt);
	void ApplyBlackout(bool bDark);
	void UpdateCatEyes(float Dt);
	void DrawMelts();
	float CloudClock = 0.f;
	float LocalClock = 0.f;
	bool bDarkApplied = false;
	TMap<TWeakObjectPtr<class ULightComponent>, float> SavedLights;
	UPROPERTY() TArray<UStaticMeshComponent*> CatEyes;
	UPROPERTY() UPointLightComponent* CatGlow = nullptr;
	UPROPERTY() class UInstancedStaticMeshComponent* MeltMesh[4] = { nullptr, nullptr, nullptr, nullptr };
	float FxAcc = 0.f;
	float PerfMs = 0.f, PerfMax = 0.f;
	int32 PerfN = 0;
	float PerfAt = 0.f;
public:
	double LastTickMs = 0.0;
	double AvgTickMs = 0.0;
};

// ------------------------------------------------------------------------------------------------------------------------
// The reactive objects
// ------------------------------------------------------------------------------------------------------------------------

namespace AntProp
{
	enum EKind : uint8
	{
		Pepper = 0,     // C7: a pepper shaker on the table: two ants push it over -> pepper cloud -> sneezing
		Flour,          // C8: a flour sack on the worktop: an ant chews it open -> avalanche over the edge -> white-out
		IceTray,        // C9: an ice tray on the worktop edge: pushed off -> cubes melt -> water -> power strip -> blackout
		PeaBowl,        // C11: a bowl of peas on the table: pushed off -> peas roll -> the Dad slips
		Slingshot,      // C10: the kid's slingshot on the floor: three ants pull the band -> a team flies onto the table
		Chocolate,      // C12: a chocolate bar on the windowsill: pushed onto the radiator -> melts -> a sticky river
		ToasterSpring,  // C13: the toaster's lever wound right down by three ants -> super pop -> toast -> spice jar -> pepper
		BookRow,        // C14: books on top of the bookcase: push the first -> domino -> the vase -> water -> Бадрик slips
		MicroButton,    // C15: the microwave's start button: it runs empty, beeps until the Dad comes
		Mentos,         // C18: mentos on the coffee table edge -> into the cola bottle below -> geyser -> rolls -> skittles
		Sponge,         // C19: the sponge on the sink rim: soaks -> heavy -> falls on the dish rack -> the plate breaks
		Balloon,        // C20: a balloon on the carpet: three ants rub it -> static -> it sticks to the ceiling with them
		PowerStrip,     // C9/C16: the power strip on the kitchen floor (its cable can be chewed: a spark, a short)
		Breaker,        // the breaker box in the hall: three ants pull the main switch down themselves
		Count
	};
	ANTKITCHEN_API const TCHAR* Name(uint8 Kind);
	ANTKITCHEN_API const TCHAR* Ru(uint8 Kind);
}

/** One reactive object of the chains (server-authoritative; the look and the motion are built on every machine). */
UCLASS()
class ANTKITCHEN_API AAntReactProp : public AKitchenHeld
{
	GENERATED_BODY()

public:
	AAntReactProp();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual float DistanceTo(const FVector& P) const override;
	virtual FString DoorNoun() const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override;
	virtual bool CanPull(AAntCharacter* Ant, bool bTell) const override;

	UPROPERTY(Replicated) uint8 Kind = 0;
	/** 0 resting, 1 working (pulled, charging), 2 falling / flying, 3 landed / done, 4 broken / gone, 5 special (melting, stuck up). */
	UPROPERTY(ReplicatedUsing = OnRep_State) uint8 State = 0;
	UPROPERTY(Replicated) float StateAt = -100.f;
	/** A flight: from, to, how long, how high the arc goes, how many turns it makes on the way. */
	UPROPERTY(Replicated) FVector_NetQuantize FlyFrom = FVector::ZeroVector;
	UPROPERTY(Replicated) FVector_NetQuantize FlyTo = FVector::ZeroVector;
	UPROPERTY(Replicated) float FlyTime = 0.6f;
	UPROPERTY(Replicated) float FlyArc = 0.f;
	UPROPERTY(Replicated) FRotator FlySpin = FRotator::ZeroRotator;
	UPROPERTY(Replicated) FRotator RestRot = FRotator::ZeroRotator;
	/** 0..1 something it builds up: the sponge's water, the balloon's charge, the band's pull. */
	UPROPERTY(Replicated) float Level = 0.f;
	/** A position the look needs (the slingshot's aim point, the balloon's ceiling, a book's tip direction). */
	UPROPERTY(Replicated) FVector_NetQuantize Aux = FVector::ZeroVector;
	/** Row index (books, blocks). */
	UPROPERTY(Replicated) uint8 Index = 0;

	UFUNCTION() void OnRep_State();

	// ---- server
	/** Fly to To over Secs with an arc Arc high and Spin turns; it lands there (OnLanded). */
	void Fly(const FVector& To, float Secs, float Arc, const FRotator& Spin);
	void SetState(uint8 S);
	/** Back to how the raid starts. */
	void ResetProp();
	TWeakObjectPtr<AAntCharacter> By;
	TWeakObjectPtr<AAntReactProp> Link;          // the next thing in the chain (a book's next book, the row's vase, the sponge's plate)
	TArray<TWeakObjectPtr<AAntReactProp>> Kids;  // the row's books, the bottle's blocks
	FTransform Home;
	float Timer = -1.f;
	int32 Step = 0;

	/** Every machine: where it is drawn right now. */
	FTransform PoseNow() const;
	static AAntReactProp* Find(const UWorld* W, uint8 Kind, int32 Index = -1);

protected:
	virtual void OnDone() override;

private:
	void Build();
	void ServerTick(float Dt);
	void OnLanded();
	void LookTick(float Dt);
	float NetNow() const;
	UPROPERTY() UStaticMeshComponent* Body = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Parts;
	UPROPERTY() UMaterialInstanceDynamic* Tint = nullptr;
	UPROPERTY() UPointLightComponent* Light = nullptr;
	FVector BodyScale = FVector::OneVector;
	FTransform BodyRel = FTransform::Identity;
	TArray<FTransform> PartRel;
	FVector UseLocal = FVector::ZeroVector;
	float UsePad = 0.f;
public:
	/** C13: the toaster is wound for the super pop. */
	bool bSuper = false;
	/** Every machine: the use point in the world. */
	FVector UsePoint() const;
private:
	float AnimT = 0.f;
	float LeverK = 0.f;
	bool bLandedServer = false;
	uint8 ShownState = 255;
};

namespace AntReact
{
	/** Level v3 is in play and not -AntNoReact. */
	ANTKITCHEN_API bool Enabled();
	/** The flat has power (no reaction net: yes). Every machine. */
	ANTKITCHEN_API bool HasPower(const UObject* Ctx);
	/** The Human cannot see (flour in his eyes). Every machine. */
	ANTKITCHEN_API bool IsBlind(const AActor* Who);
	/** The ant is white with flour (the Human sees it from further, even in the dark). */
	ANTKITCHEN_API bool IsFloured(const AActor* Who);
	/** Server, a new raid (AKitchenFixture::ResetAll): power on, clouds gone, the reactive objects back in their places. */
	ANTKITCHEN_API void ResetAll(UWorld* W);
	/** Server, AKitchenToaster::Pop: the super pop of C13. */
	ANTKITCHEN_API void OnToasterPop(AKitchenToaster* Toaster);
	/** Server, NpcRoutine::Finish: a resident dealt with a mess that came from one of ours. True if it was ours. */
	ANTKITCHEN_API bool OnTidied(UWorld* W, AActor* Src, uint8 MessKind, uint8 Who);
	/** Every machine, AAntHUD::DrawHUD: the white-out of the blinded Human, the blackout line. */
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Canvas, APlayerController* PC);
	/** -AntDo / admin: chain C7..C20, mat info|list|dump, react ..., power ..., heat ... True if it was ours. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** The heat a point gets from the hot things of the flat right now (°C, 22 = room). Every machine (server for gameplay). */
	ANTKITCHEN_API float HeatAt(const UWorld* W, const FVector& P);
}
