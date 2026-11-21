#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "AntMess.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class AKitchenLoot;
class AAntCharacter;
class AAntKitchenGameMode;

/**
 * The mess (docs/FEATURES.md, section 23): the apartment starts clean and the raid dirties it.
 *
 * - Stains (jam, honey, cola, oil, cream, crumbs ground in, mud, water, chocolate, shard scratches, sole prints, drag marks)
 *   are pooled deferred decals with one procedural material (/Game/Mess/M_MessStain): shape, colour, gloss and relief are
 *   parameters, so a stain projects onto whatever is under it (tiles, worktop, rug, cabinet fronts) and looks wet, not
 *   printed. Big stains are server events, replicated as data (kind, place, normal, size, seed, yaw) and drawn the same on
 *   every machine, late joiners included. Small things (ant footprints, smears of a sliding chunk, drag scuffs, the Human's
 *   sticky soles) are made locally on every machine from what it already sees: one instanced mesh of glossy blobs.
 * - Food that breaks gets a burst that matches it (crumbs, sticky droplets, fizz, sugar sparkle), soft food splats (a blob
 *   that squashes, spreads and sinks, plus the stain), pieces squash when they land and smear where they slide, ants that
 *   walk through it come out tinted and carry blobs for a while (cosmetic).
 * - Dust motes in the air (AntMessDust.cpp): quads wrapped round the camera, all motion in the shader; lit translucency,
 *   so they show where the light is (sun shafts, the fridge, lamps). Stirred by impacts and the Human's steps.
 *
 * API for other code: AntMess::StainAt and AntMess::CleanArea (server).
 */

namespace MessKind
{
	enum : uint8 { Jam, Honey, Cola, Oil, Cream, Crumbs, Mud, Water, Chocolate, Scratch, Print, Drag, Juice, Sugar, Count };
}

namespace MessShape
{
	enum : uint8 { Pool = 0, Splat = 1, Smear = 2, Scratch = 3, Crumbs = 4, Print = 5 };
}

USTRUCT()
struct FMessStain
{
	GENERATED_BODY()

	UPROPERTY() uint16 Id = 0;
	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() uint8 Shape = 0;
	UPROPERTY() uint8 Seed = 0;
	UPROPERTY() uint8 Yaw = 0;        // 0..255 -> 0..360 degrees round the normal
	UPROPERTY() uint8 Asp = 16;       // length / width x 16 (smears)
	UPROPERTY() uint8 Grow = 0;       // seconds to reach full size (a pool that keeps running)
	UPROPERTY() uint16 Size = 100;    // width, uu
	UPROPERTY() FColor Tint = FColor::White;   // food colour for crumbs and drag marks (white: the kind's own colour)
	UPROPERTY() FVector_NetQuantize10 Pos = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal Normal = FVector::UpVector;
	UPROPERTY() float Born = 0.f;     // server time
};

/** One per world, made by UAntMessSubsystem on the server (replicated); everything it draws is local. */
UCLASS()
class ANTKITCHEN_API AAntMess : public AActor
{
	GENERATED_BODY()

public:
	AAntMess();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static AAntMess* Get(const UObject* Ctx);

	/** Server. */
	int32 AddStain(uint8 Kind, uint8 Shape, const FVector& Pos, const FVector& Normal, float Size, int32 Seed, float YawDeg = -1.f, float Aspect = 1.f, float GrowSeconds = 0.f, const FColor& Tint = FColor::White);
	int32 Clean(const FVector& Center, float Radius);

	/** Server -> everyone: a burst at a break (particles, the splat blob, the camera kick). Kind = MessKind. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastBurst(uint8 Kind, FVector_NetQuantize At, FVector_NetQuantizeNormal N, uint16 Size, FColor Col, uint8 Seed);
	UFUNCTION(NetMulticast, Reliable) void MulticastClean(FVector_NetQuantize At, float Radius);
	UFUNCTION(NetMulticast, Reliable) void MulticastDust(bool bOn);

	UPROPERTY(ReplicatedUsing = OnRep_Stains) TArray<FMessStain> Stains;
	UFUNCTION() void OnRep_Stains();

	// ---- local (every machine that draws)
	void Burst(uint8 Kind, const FVector& At, const FVector& N, float Size, const FLinearColor& Col, int32 Seed);
	void LocalStain(uint8 Kind, uint8 Shape, const FVector& Pos, const FVector& Normal, float Size, int32 Seed, float YawDeg, float Aspect, const FLinearColor& Tint, float Life);
	/** A glossy blob on the instanced mesh (footprints, smears, droplets). Life <= 0: stays. */
	void Speck(const FVector& At, const FVector& N, float Size, float Flat, const FLinearColor& Col, float Rough, float Life, float YawDeg = 0.f, float Stretch = 1.f);
	void LocalClean(const FVector& At, float Radius);
	/** The kind of stain under P (within Margin of its edge), or 255. Local, from the replicated stains. */
	uint8 StainUnder(const FVector& P, float Margin, FLinearColor* OutColor = nullptr) const;
	void Stir(const FVector& At, float Strength, float Radius);
	void SetDust(bool bOn);
	bool IsDustOn() const { return bDustOn; }

	/** Food piece tick (every machine that draws): squash on landing, smears while sliding, scuffs while dragged. */
	void FoodTick(AKitchenLoot* Loot, float Dt, USceneComponent* Visual);
	/** The Human's foot came down (every machine): sticky prints after he stepped in something, a little dust. */
	void HumanStep(AActor* Who, const FVector& Foot, float YawDeg, float SoleLen);

	FString Describe() const;
	/** Dev: hide every decal and speck (A/B cost measurements). */
	void SetMessHidden(bool bHide);
	/** Dev: the scripted walker and the cost probe (AntMess::DevCommand). */
	void DevTick(float Dt, double TickMs);
	/** Dev: GPU / CPU cost probe. */
	int32 NumDecals() const;
	int32 NumSpecks() const;

	static FLinearColor KindColor(uint8 Kind);
	static FLinearColor KindColorThin(uint8 Kind);
	static bool IsSticky(uint8 Kind) { return Kind == MessKind::Jam || Kind == MessKind::Honey || Kind == MessKind::Cola || Kind == MessKind::Cream || Kind == MessKind::Chocolate || Kind == MessKind::Oil || Kind == MessKind::Juice; }

private:
	struct FDecal
	{
		UDecalComponent* C = nullptr;
		UMaterialInstanceDynamic* M = nullptr;
		int32 Id = -1;            // stain id (>= 0x10000: local)
		uint8 Kind = 0;
		float Born = 0.f;         // local time
		float Grow = 0.f;
		float Life = 0.f;         // local: > 0 fades out at the end
		float FadeOut = -1.f;     // >= 0: being cleaned, seconds since
		float Size = 100.f;
		FVector Pos = FVector::ZeroVector;
	};
	struct FSpeckInfo { float Born = 0.f; float Life = 0.f; FTransform T; };
	struct FFood
	{
		FVector LastPos = FVector::ZeroVector;
		float LastVz = 0.f;
		float Squash = 0.f;
		float SquashT = 10.f;
		FVector BaseScale = FVector::OneVector;
		TWeakObjectPtr<USceneComponent> Visual;
		float SmearDist = 0.f;
		float DragDist = 0.f;
		uint8 Kind = 255;
		bool bInit = false;
	};
	struct FDirt
	{
		TWeakObjectPtr<AAntCharacter> Ant;
		uint8 FeetKind = 255;
		float Feet = 0.f;           // 0..1: sticky feet, decays per step
		FLinearColor FeetColor = FLinearColor::White;
		float Body = 0.f;           // 0..1: tint and blobs, fades over ~14 s
		FLinearColor BodyColor = FLinearColor::White;
		FVector LastStamp = FVector::ZeroVector;
		int32 Step = 0;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Blobs;
		float TintApplied = 0.f;
	};
	struct FSplat
	{
		UStaticMeshComponent* C = nullptr;
		UMaterialInstanceDynamic* M = nullptr;
		float T = 100.f;
		float Size = 1.f;
		FVector At = FVector::ZeroVector;
		FVector N = FVector::UpVector;
		float Yaw = 0.f;
		float Sx = 1.f, Sy = 1.f;
	};

	FDecal* AcquireDecal();
	void ApplyDecal(FDecal& D, uint8 Kind, uint8 Shape, const FVector& Pos, const FVector& Normal, float Size, int32 Seed, float YawDeg, float Aspect, const FLinearColor& Tint);
	void SyncStains();
	void TickDecals(float Dt);
	void TickSpecks(float Dt);
	void TickAnts(float Dt);
	void TickSplats(float Dt);
	void ApplyAntTint(FDirt& D, float Amount);
	void DirtyAnt(AAntCharacter* Ant, uint8 Kind, const FLinearColor& Col, float Feet, float Body);
	void SplatBlob(const FVector& At, const FVector& N, float Size, const FLinearColor& Col, float Rough, int32 Seed);
	float LocalNow() const;

	UPROPERTY() TArray<UDecalComponent*> DecalComps;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> DecalMids;
	UPROPERTY() UInstancedStaticMeshComponent* Specks = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> SplatComps;
	UPROPERTY() TArray<UStaticMeshComponent*> AntBlobComps;
	UPROPERTY() UMaterialInterface* StainMat = nullptr;
	UPROPERTY() UMaterialInterface* BlobMat = nullptr;
	TArray<FDecal> Decals;
	TArray<FSpeckInfo> SpeckInfo;
	int32 SpeckCursor = 0;
	TArray<FSplat> Splats;
	TArray<FDirt> Dirt;
	TMap<TWeakObjectPtr<AKitchenLoot>, FFood> Foods;
	TMap<TWeakObjectPtr<AActor>, FVector4> Soles;   // the Human (and the family): xyz colour, w stickiness left
	TMap<TWeakObjectPtr<AActor>, uint8> SoleKinds;
	uint16 NextId = 1;
	int32 NextLocalId = 0x10000;
	float AntScan = 0.f;
	float SpeckScan = 0.f;
	bool bLocalBuilt = false;
	void BuildLocal();

	// ---- dust (AntMessDust.cpp)
public:
	struct FDustLayer
	{
		UProceduralMeshComponent* C = nullptr;
		UMaterialInstanceDynamic* M = nullptr;
		FVector Box = FVector(800.f);
		bool bBand = false;
	};
private:
	void BuildDust();
	void TickDust(float Dt);
	UPROPERTY() TArray<UProceduralMeshComponent*> DustComps;
	TArray<FDustLayer> Dust;
	bool bDustOn = true;
	FVector StirAt = FVector(0.f, 0.f, -1.e6f);
	float StirTime = -100.f;
	float StirStrength = 0.f;
	float StirRadius = 600.f;
	float DustIntensity = 1.f;
};

/** Makes the AAntMess of a game world on its server (listen, dedicated or standalone): nothing else has to call anything. */
UCLASS()
class ANTKITCHEN_API UAntMessSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};

namespace AntMess
{
	/** Server: a stain that every player sees. Type = MessKind. Size = width in uu. Seed picks the shape. Returns its id (0: none). */
	ANTKITCHEN_API int32 StainAt(const UObject* Ctx, uint8 Type, const FVector& Location, const FVector& Normal, float Size, int32 Seed);
	/** Server: wipes every stain (and local specks) within Radius of Center on every machine (grandma / dad cleaning). Returns how many stains went. */
	ANTKITCHEN_API int32 CleanArea(const UObject* Ctx, const FVector& Center, float Radius);
	/** Server: food broke (KitchenLoot::Shatter): the stain and the burst that match it. */
	ANTKITCHEN_API void OnFoodBreak(AKitchenLoot* Food, const FVector& At, int32 Seed);
	/** Server: a v3 fixture broke or spilled (KitchenFixture::Spill). */
	ANTKITCHEN_API void OnFixtureSpill(AActor* Fixture, const FString& Id, const FVector& At, bool bBroken, int32 Mat);
	/** Every machine: food tick (KitchenLoot::Tick). */
	ANTKITCHEN_API void FoodTick(AKitchenLoot* Loot, float Dt, USceneComponent* Visual);
	/** Every machine: a foot of the Human / the family came down. */
	ANTKITCHEN_API void HumanStep(AActor* Who, const FVector& Foot, float YawDeg, float SoleLen);
	/** Every machine: a hard impact (stomp, a fall): stirs the dust. */
	ANTKITCHEN_API void Stir(const UObject* Ctx, const FVector& At, float Strength, float Radius);
	/** What kind of food this is for the mess (MessKind; Crumbs for plain dry food). */
	ANTKITCHEN_API uint8 FoodKind(const FString& LootName);
	/** -AntDo "mess ...". */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
