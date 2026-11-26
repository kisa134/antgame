#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntMatter.generated.h"

class UPrimitiveComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class AAntKitchenGameMode;
struct FAntMatProps;

/**
 * Matter (KAN-21, docs/MATERIALS.md "Вещество"): everything physical in the flat reacts like the real thing, and shows it on
 * itself. Things get wet, catch fire, char, freeze, go live; water puts fire out with a cloud of steam, electricity runs through
 * water and wet things, burning oil flashes over its puddle, a flour cloud in a flame goes off with a bang, plastic melts and
 * drips with acrid smoke, food toasts before it burns, hot glass cracks when it is suddenly wet.
 *
 * Every reaction takes its numbers from the material table (AntMaterials, Config/AntMaterials.json: ignite_at, burn_rate,
 * burns_into, wet_speed, dry_speed, absorb, conduct, heat_take, melt_at, tear, crumbly...), never from per-object code: a new
 * cardboard thing behaves like cardboard by itself.
 *
 * SERVER-AUTHORITATIVE. AAntMatterNet (one per world) simulates the bodies that are not at rest (8 times a second, capped)
 * and replicates one small record per body: the overall values of AntState's kinds as bytes, up to five spots, the flame.
 * Every machine applies the records to the AntState layer (source Matter) and draws the flames, smoke, steam and sparks
 * itself from them. Burnt stays burnt within a raid (char never goes back); a new raid resets everything.
 *
 * A BODY is a static mesh component of the flat with a material in the table: a builder piece, a fixture's part, a piece of
 * food, a wreck body's mesh. Found on every machine by a registry (spatial grid) and, in the records, by its replicated owner
 * and component name or, for the builder's pieces that are not replicated, by its mesh and a point inside it.
 */

namespace AntMatterFlag
{
	enum : uint8 { Burning = 1, Live = 2, Gone = 4, Melting = 8, Smoking = 16, Steaming = 32 };
}

/** A spot of a record (AntState::FSpot on the wire). */
USTRUCT()
struct FAntMatterSpot
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 Local = FVector::ZeroVector;   // component-local
	UPROPERTY() uint16 Radius = 0;                                   // world uu
	UPROPERTY() uint8 Kind = 0;                                      // AntState::EKind
	UPROPERTY() uint8 Str = 0;                                       // 0..255
};

/** One body that is not at rest, as every machine needs to see it. */
USTRUCT()
struct FAntMatterRec
{
	GENERATED_BODY()
	UPROPERTY() uint16 Id = 0;
	/** The replicated owner of the component (a fixture, a piece of food, a wreck body), or null for a piece of the builder. */
	UPROPERTY() AActor* Actor = nullptr;
	UPROPERTY() FName Comp;
	UPROPERTY() FName Mesh;
	/** A world point inside it when the record was made (finds a builder piece on a client). */
	UPROPERTY() FVector_NetQuantize Anchor = FVector::ZeroVector;
	/** The overall value of each AntState kind, 0..255. */
	UPROPERTY() uint8 S[13];
	UPROPERTY() uint8 Flags = 0;
	/** The flame: its size 0..255 (x 4 uu = the radius of the burning patch) and where its front is (component-local). */
	UPROPERTY() uint8 Fire = 0;
	UPROPERTY() FVector_NetQuantize10 FireAt = FVector::ZeroVector;
	UPROPERTY() TArray<FAntMatterSpot> Spots;
	FAntMatterRec() { FMemory::Memzero(S, sizeof(S)); }
};

/** Burning liquid on a surface (an oil puddle that flashed), or the fireball of a flour cloud. */
USTRUCT()
struct FAntMatterPool
{
	GENERATED_BODY()
	UPROPERTY() uint8 Id = 0;
	UPROPERTY() uint8 Kind = 0;        // 0 burning oil, 1 a fireball (flour)
	UPROPERTY() FVector_NetQuantize Center = FVector::ZeroVector;
	UPROPERTY() float Radius = 60.f;
	UPROPERTY() float Born = 0.f;
	UPROPERTY() float Power = 1.f;     // 0..1 how hard it burns
};

/** A puddle frozen into a slab of ice: a cell mask over a square, solid and slippery on every machine. */
USTRUCT()
struct FAntMatterIce
{
	GENERATED_BODY()
	UPROPERTY() uint8 Id = 0;
	UPROPERTY() FVector_NetQuantize Center = FVector::ZeroVector;   // z = the top of the ice
	UPROPERTY() float Cell = 20.f;
	UPROPERTY() float Thick = 4.f;
	UPROPERTY() TArray<uint8> Mask;    // 16 x 16 bits, row by row
	UPROPERTY() float Born = 0.f;
	UPROPERTY() uint8 Left = 255;      // how much is still ice (it melts at room temperature)
};

namespace AntMatter
{
	constexpr int32 IceN = 16;
	/** Matter is on (level v3, not -AntNoMatter). */
	ANTKITCHEN_API bool Enabled();
	/** Server, a new raid (AntReact::ResetAll): nothing wet, hot, burning, frozen; burnt-away things back; the net made. */
	ANTKITCHEN_API void ResetAll(UWorld* W);
	/** Server, AAntReactNet::Short: what conducts around the short goes live (metal, wet things, the water), and sparks. */
	ANTKITCHEN_API void OnShort(UWorld* W, const FVector& Where, const TCHAR* Why);
	/** Every machine: is P on a frozen puddle (slippery for the Human and the ants)? */
	ANTKITCHEN_API bool IceAt(const UWorld* W, const FVector& P, float Tolerance = 40.f);
	/** Every machine: the strongest fire near P (0 none .. 1 a big one), and where it is. For AI and ants that keep away. */
	ANTKITCHEN_API float FireNear(const UWorld* W, const FVector& P, float Radius, FVector* OutAt = nullptr);
	/** -AntDo / admin: matter fire|heat|wet|soak|douse|freeze|short|spark|oil|flour|napkin|info|list|reset ... True if it was ours. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}

/**
 * A plain household thing for the matter tests and the admin (a paper napkin, a cotton rag, a steel pot, a cardboard box):
 * a replicated actor with one mesh of the game's own base shapes and the look and material of what it is. Dev only: never
 * placed by the level (owner's rule 3), made by "matter thing ..." and gone with the next raid.
 */
UCLASS()
class ANTKITCHEN_API AAntMatterThing : public AActor
{
	GENERATED_BODY()
public:
	AAntMatterThing();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** "napkin", "rag", "pot", "box", "bread" */
	UPROPERTY(ReplicatedUsing = OnRep_What) FName What;
	UPROPERTY(Replicated) FVector_NetQuantize10 Size = FVector(100.f);
	UFUNCTION() void OnRep_What();
	/** The material table id of what it is. */
	FString MatId() const;
	UPROPERTY() UStaticMeshComponent* Body = nullptr;
private:
	void Build();
	bool bBuilt = false;
};

/** The server's matter simulation and its replicated picture (one per world, spawned by AntMatter::ResetAll). */
UCLASS()
class ANTKITCHEN_API AAntMatterNet : public AActor
{
	GENERATED_BODY()

public:
	AAntMatterNet();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AAntMatterNet* Get(const UWorld* W);

	UPROPERTY(ReplicatedUsing = OnRep_Recs) TArray<FAntMatterRec> Recs;
	UPROPERTY(Replicated) TArray<FAntMatterPool> Pools;
	UPROPERTY(ReplicatedUsing = OnRep_Ice) TArray<FAntMatterIce> Ices;
	UFUNCTION() void OnRep_Recs();
	UFUNCTION() void OnRep_Ice();

	UFUNCTION(NetMulticast, Unreliable) void MulticastSteam(FVector_NetQuantize At, float Size);
	UFUNCTION(NetMulticast, Unreliable) void MulticastFlash(FVector_NetQuantize At, float Radius, uint8 Kind);
	UFUNCTION(NetMulticast, Unreliable) void MulticastSpark(FVector_NetQuantize At, FVector_NetQuantizeNormal Dir);

	// ---- server API (also what the dev commands call)
	/** Sets C alight at a world point (if its material burns and it is not soaked). */
	bool Ignite(UPrimitiveComponent* C, const FVector& At, const TCHAR* Why);
	/** Heat from outside: C is held at TempC near At for this tick (a flame, a hot pot, a dev heat gun). */
	void HeatBody(UPrimitiveComponent* C, const FVector& At, float TempC, float Dt);
	/** Liquid on C at a world point: a spot (Liquid: AntFluid::ELiquid) that soaks it by Amount (0..1). */
	void WetBody(UPrimitiveComponent* C, const FVector& At, float Radius, float Amount, uint8 Liquid);
	/** Splash of water (a glass thrown, a bottle tipped): every body in the sphere is wet where it is hit, fires in it go out. */
	int32 Douse(const FVector& At, float Radius, float Amount, uint8 Liquid = 0);
	/** Cold: bodies in the sphere frost over, the water of a puddle in it turns to ice. */
	int32 Freeze(const FVector& At, float Radius);
	/** Live: what conducts from At (metal, wet things, the water it stands in) carries the current for Seconds. */
	int32 Electrify(const FVector& At, float Radius, float Seconds, const TCHAR* Why);
	/** A spark (a short, a live edge, a lighter): lights what burns easily next to it, an oil film, a flour cloud. */
	void Spark(const FVector& At, const FVector& Dir, const TCHAR* Why);
	/** An oil film at At catches fire (if there is oil there). */
	bool LightPool(const FVector& At, const TCHAR* Why);
	/** A flour cloud at At goes up: a bang. */
	void Bang(const FVector& At, float Radius, const TCHAR* Why);
	/** Server: a heat source for Seconds (a dev heat gun, a hot thing put down): bodies within Radius are held near TempC. */
	void AddHeat(const FVector& At, float Radius, float TempC, float Seconds);
	struct FHeatSrc { FVector At = FVector::ZeroVector; float Radius = 100.f; float Temp = 22.f; float Until = 0.f; };
	TArray<FHeatSrc> HeatSrcs;
	/** Server: everything back as the raid starts. */
	void ResetMatter();
	FString Describe() const;
	/** Dev: timed steps of a test scene ("matter test ..."), on the server's clock. */
	TArray<TPair<float, TFunction<void()>>> Script;
	void Later(float Seconds, TFunction<void()> Fn) { Script.Add(TPair<float, TFunction<void()>>(LocalClock + Seconds, MoveTemp(Fn))); }
	float Clock() const { return LocalClock; }
	/** Server: info on the bodies near a point (dev). */
	void LogAround(const FVector& At, float Radius) const;

	// ---- every machine: the registry of bodies
	struct FReg
	{
		TWeakObjectPtr<UPrimitiveComponent> C;
		const FAntMatProps* M = nullptr;
		bool bDyn = false;     // moves on its own (food, a wreck body, a reactive prop): not in the grid
	};
	TArray<FReg> Reg;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> RegIndex;
	TMap<FIntPoint, TArray<int32>> Grid;
	TArray<int32> Dyn;
	void ScanRegistry(int32 Budget);
	/** Into the registry now (a thing just made), not at the next scan. */
	void RegisterNow(UPrimitiveComponent* C);
	/** Bodies whose bounds reach within Radius of P (the registry, current bounds). */
	void Query(const FVector& P, float Radius, TArray<int32>& Out) const;
	const FAntMatProps* MatOf(const UPrimitiveComponent* C);
	static bool IsCandidate(const UPrimitiveComponent* C);

	double LastTickMs = 0.0, AvgTickMs = 0.0, LastLocalMs = 0.0, AvgLocalMs = 0.0;
	int32 NumBurning = 0, NumActive = 0;

private:
	// ---- server simulation
	struct FSpotS { FVector Local = FVector::ZeroVector; float Radius = 0.f; uint8 Kind = 0; float Str = 0.f; };
	struct FBody
	{
		TWeakObjectPtr<UPrimitiveComponent> C;
		const FAntMatProps* M = nullptr;
		uint16 Id = 0;
		float T = 22.f;               // °C
		float HeatIn = -1000.f;       // the hottest outside heat this tick (°C), and where it comes from
		FVector HeatFrom = FVector::ZeroVector;
		float Wet = 0.f;              // 0..1 of what it can hold
		float Soak[4] = { 0.f, 0.f, 0.f, 0.f };   // oil, jam, cola, honey
		float Frost = 0.f;
		float Live = 0.f;             // seconds of current left
		float Char = 0.f;             // irreversible
		float Dmg = 0.f;
		float Fuel = 1.f;             // what is left to burn
		bool bBurning = false;
		FVector FireLocal = FVector::ZeroVector;   // the centre of the burnt patch (local)
		float FireR = 0.f;            // its radius, world uu
		float Flame = 0.f;            // 0..1
		float BurnFor = 0.f;
		bool bGone = false;
		bool bMelting = false;
		float MeltFor = 0.f;
		float SoakedFor = 0.f;
		float WetLastTick = 0.f;
		float Quiet = 0.f;            // seconds at rest
		float NextSpark = 0.f;
		float NextSmoke = 0.f;
		float NextZap = 0.f;
		float NoFireUntil = 0.f;
		float Static = 0.f;           // 0..1 static charge (a rubbed balloon)
		float BaseMass = -1.f;
		bool bTorn = false, bCrumbled = false, bCracked = false;
		TArray<FSpotS> Spots;
	};
	TArray<FBody> Bodies;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> BodyIndex;
	uint16 NextId = 1;
	uint8 NextPoolId = 1, NextIceId = 1;
	float SimClock = 0.f, NetClock = 0.f, ScanClock = 0.f, ProbeClock = 0.f, FlourClock = 0.f;
	int32 ProbeCursor = 0;
	FBody* BodyOf(UPrimitiveComponent* C, bool bCreate);
	void ServerStep(float Dt);
	void StepBody(FBody& B, float Dt);
	void StepFire(FBody& B, float Dt);
	void StepPools(float Dt);
	void StepIce(float Dt);
	/** Static: a charged thing (the rubbed balloon of C20) pulls light things to itself and they stick. */
	void StepStatic(float Dt);
	float StaticClock = 0.f;
	void ProbeWater(FBody& B, float Dt, bool bWasRest);
	void AddServerSpot(FBody& B, const FVector& World, float Radius, uint8 Kind, float Str);
	void Extinguish(FBody& B, const TCHAR* Why);
	void BurnAway(FBody& B);
	void WriteRecords();
	FVector FireWorld(const FBody& B) const;
	bool IsAtRest(const FBody& B) const;
	void HurtAround(const FVector& At, float Radius, float Dps, float Dt, const TCHAR* Why);
	TArray<FVector> SparkQueue;
	TSet<uint8> Flared;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Pending;
	/** A builder piece hidden because a wreck body took over: that body's mesh (else C). */
	UPrimitiveComponent* TakeOver(UPrimitiveComponent* C) const;

	// ---- every machine: applying the records, drawing
	struct FShown
	{
		TWeakObjectPtr<UPrimitiveComponent> C;
		bool bHidden = false;
		float SoundAt = 0.f;
	};
	TMap<uint16, FShown> Shown;
	bool bRecsDirty = true;
	void LocalTick(float Dt);
	void ApplyRecords();
	UPrimitiveComponent* Resolve(const FAntMatterRec& R);
	void DrawFires(float Dt);
	void DrawIce();
	void WatchMess();
	TMap<uint16, TArray<TWeakObjectPtr<UPrimitiveComponent>>> MessOn;   // stain id -> what it landed on
	UPROPERTY() TArray<UPointLightComponent*> Lights;
	UPROPERTY() UInstancedStaticMeshComponent* IceMesh = nullptr;
	bool bIceDirty = true;
	float FxAcc = 0.f, LocalClock = 0.f, MessClock = 0.f, FlashUntil = 0.f;
	int32 ScanCursor = 0;
	bool bScannedOnce = false;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> Seen;
};
