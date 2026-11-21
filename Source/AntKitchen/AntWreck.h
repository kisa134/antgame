#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntWreck.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class AAntCharacter;
class AAntKitchenGameMode;
class AAntWreckBody;

/**
 * Wreck: every piece of furniture and every prop of the flat can be knocked about, damaged and broken (docs/FEATURES.md, section 23).
 *
 * Dormant until loaded. The kitchen builder's pieces stay cheap static meshes. A strike, a falling body, a group of ants or a
 * cola blast that loads a piece above its threshold (mass x class) turns it into an AAntWreckBody: a server-simulated physics body
 * made of a few boxes fitted to its own collision triangles (so a table is a top and four legs, not a solid block), replicated as a
 * pose at a modest rate and smoothed on clients. When it settles it goes back to sleep where it lies. At most MaxAwake bodies
 * simulate at once.
 *
 * Gradual damage. Health per object, parts = chunks of a seeded cut of its own collision mesh (the same cut on every machine).
 * Stage 1: cracked - the chunks part by a hair and the cut faces show (dents on metal); 2: bent - it leans or sags toward its
 * damaged side; 3: loose - a chunk falls off; 4: broken - every chunk falls and the chunks lie as one merged debris pile.
 * Falling bodies and chunks damage what they land on; a toppled bookcase dominoes; what stood on a piece goes over with it.
 */
namespace AntWreck
{
	/** What a piece is made of (decides mass, threshold, health and how it breaks). */
	enum EMat : uint8 { Wood, Board, Metal, Glass, China, Plastic, Cloth, Paper, Appliance, Food, Count };

	struct FClassDef
	{
		const TCHAR* Name;
		float Density;      // kg per m^2 of its bounding box's surface (furniture is shells and boards, not solid blocks)
		float WakeK;        // impulse (kg m/s) per kg it takes to tip it
		float HpPerKg;
		float MinHp;
		bool bBreaks;       // reaches stage 4 (else stops at 3)
		float Noise;        // crash loudness
	};
	ANTKITCHEN_API const FClassDef& Class(uint8 Mat);

	/** One piece of the kitchen builder's table that can wreck (built on every machine from the same table). */
	struct FPiece
	{
		int32 Inst = -1;
		uint8 Mat = Wood;
		float Mass = 1.f;          // kg
		float Hp = 50.f;
		bool bFixed = false;       // built in (cabinet runs, radiators): takes damage, never wakes
		FBox Box = FBox(ForceInit);   // world bounds where the builder put it
	};

	ANTKITCHEN_API bool Enabled();
	/** The catalog (lazily built from the kitchen builder on this machine). */
	ANTKITCHEN_API const TArray<FPiece>& Pieces(UWorld* World);
	ANTKITCHEN_API const FPiece* PieceOf(UWorld* World, int32 Inst);
	ANTKITCHEN_API int32 FindPiece(UWorld* World, const FString& NameOrIndex);
	/** The builder instance whose static copy C is (-1: not a piece that wrecks). The Human's body uses it (HumanPhys.cpp). */
	ANTKITCHEN_API int32 PieceOfComponent(UWorld* World, const class UPrimitiveComponent* C);

	/**
	 * Server: something struck the world at Point (radius Radius): Impulse is kg m/s along its direction, Damage in hit points.
	 * Every piece, fixture and wreck body in the radius takes its share (falloff with distance). Returns how many were affected.
	 */
	ANTKITCHEN_API int32 Hit(UWorld* World, const FVector& Point, float Radius, const FVector& Impulse, float Damage, AActor* Source, const TCHAR* Why);
	/** Server: one piece (builder instance) takes a hit. */
	ANTKITCHEN_API AAntWreckBody* HitPiece(UWorld* World, int32 Inst, const FVector& Point, const FVector& Impulse, float Damage, AActor* Source);

	/** Server: everything back as it was (a new raid). */
	ANTKITCHEN_API void ResetAll(UWorld* World);
	/** Dev: -AntDo commands "wreck ...". */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);

	/** Hook for the mess system: called (server) where something broke or spilled. Set by whoever wants it (StainAt later). */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnWreckMess, UWorld* /*World*/, const FVector& /*At*/, float /*Size*/, uint8 /*Mat*/);
	ANTKITCHEN_API FOnWreckMess& OnMess();

	/** Simultaneously simulating bodies, at most. */
	constexpr int32 MaxAwake = 40;
	/** The world's gravity under the bodies, uu/s^2 (the engine's 980 is a moon at this scale: 1 m = 1900 uu). */
	constexpr float Gravity = 9000.f;
	constexpr float UuPerM = 1900.f;
}

/** A dent (mesh-local). */
USTRUCT()
struct FWreckDent
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 At;
	UPROPERTY() FVector_NetQuantizeNormal Dir;
	UPROPERTY() float Radius = 0.f;
	UPROPERTY() float Depth = 0.f;
};

/** Something that falls along a parabola and lies (a chunk that came off, or the whole piece spilled): the same on every machine. */
USTRUCT()
struct FWreckDrop
{
	GENERATED_BODY()
	/** Chunk index, 255: the whole piece. */
	UPROPERTY() uint8 Chunk = 255;
	UPROPERTY() float Start = 0.f;
	UPROPERTY() float Time = 0.5f;
	UPROPERTY() FTransform From;
	UPROPERTY() FTransform To;
};

/** The pose of a body (server time stamped): clients interpolate between the last two. */
USTRUCT()
struct FWreckPose
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 Loc;
	UPROPERTY() FRotator Rot = FRotator::ZeroRotator;
	UPROPERTY() float T = 0.f;
	UPROPERTY() uint8 Seq = 0;
};

/**
 * One piece of the flat that has been touched: it hides the builder's static copy on every machine and takes over (physics,
 * damage stages, its chunks). Server-authoritative; replicated.
 */
UCLASS()
class ANTKITCHEN_API AAntWreckBody : public AActor
{
	GENERATED_BODY()

public:
	AAntWreckBody();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Builder instance (KitchenArt::Instances()) or, for a weapon prop, -1. */
	UPROPERTY(ReplicatedUsing = OnRep_Setup) int32 Inst = -1;
	UPROPERTY(Replicated) int32 Seed = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Look) uint8 Stage = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Look) uint16 LostMask = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Look) TArray<FWreckDent> Dents;
	UPROPERTY(ReplicatedUsing = OnRep_Drops) TArray<FWreckDrop> Drops;
	UPROPERTY(ReplicatedUsing = OnRep_Pose) FWreckPose Pose;
	UPROPERTY(Replicated) bool bAwake = false;
	/** Riding another body (books in the bookcase, a plate on the table) until it tips. */
	UPROPERTY(ReplicatedUsing = OnRep_Pose) AAntWreckBody* RideOn = nullptr;
	UPROPERTY(Replicated) FTransform RideRel;

	float Health = 1.f;
	float MaxHealth = 1.f;
	uint8 Mat = 0;
	float Mass = 1.f;
	bool bFixed = false;

	/** Server: take a hit. */
	void TakeHit(const FVector& Point, const FVector& Impulse, float Damage, AActor* Source);
	/** Server: become a physics body (false if it cannot: built in, the cap, already awake). */
	bool Wake(const FVector& Impulse, const FVector& At, bool bForce = false);
	void Sleep();
	/** Server: it falls along a parabola from where it is to the floor below Target (whole), the same on every machine. */
	void Spill(const FVector& Velocity);
	/** Server: breaks now. */
	void BreakApart(const FVector& Dir);
	bool IsBroken() const { return Stage >= 4; }
	FString PieceName() const;
	FBox WorldBox() const;
	float TiltDeg() const;
	static int32 NumAwake(UWorld* World);

	UFUNCTION() void OnRep_Setup();
	UFUNCTION() void OnRep_Look();
	UFUNCTION() void OnRep_Drops();
	UFUNCTION() void OnRep_Pose();

	/** Server: a dev/test setup from a mesh name (a weapon prop lying about). */
	UPROPERTY(ReplicatedUsing = OnRep_Setup) FName PropMesh;

	UPROPERTY() UBoxComponent* Root = nullptr;
	UPROPERTY() UStaticMeshComponent* Mesh = nullptr;
	UPROPERTY() UProceduralMeshComponent* Chunks = nullptr;
	UPROPERTY() TArray<UBoxComponent*> Hull;
	/** Mesh frame relative to Root. */
	FTransform MeshRel;

private:
	void Setup();
	void BuildHull();
	void RebuildLook();
	void ApplyLean(const FVector& HitDirWorld);
	void DropChunk(int32 Chunk, const FVector& Dir);
	void TickDrops(float Dt);
	void LandDrop(int32 i);
	void ServerPhysicsTick(float Dt);
	void SpillRiders(const FVector& Vel);
	void Crush(const FVector& At, float Radius, float Kg);
	/** Server: level v3 fixtures lying or standing on it (the tablecloth, a jar) go over with it. */
	void KnockFixtures(const FVector& Dir, const TCHAR* Why);
	void ClothOff();
public:
	/** A static cloth over it went (replicated so every machine hides it). */
	UPROPERTY(ReplicatedUsing = OnRep_Cloth) bool bClothGone = false;
	UFUNCTION() void OnRep_Cloth();
private:
	void PushPose();
	float NetNow() const;
	UFUNCTION() void OnPhysHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool bSetUp = false;
	float AwakeSince = -1.f;
	float StillFor = 0.f;
	float NextPoseAt = 0.f;
	float LastHitAt = -10.f;
	TMap<TWeakObjectPtr<AActor>, float> HitCool;
	TArray<bool> DropLanded;
	TArray<UProceduralMeshComponent*> DropFx;
	FWreckPose PrevPose;
	FWreckPose CurPose;
	bool bHavePrev = false;
	FTransform LeanBase;
	bool bLeanBaseSet = false;
	float LeanDeg = 0.f;
	FVector LeanAxis = FVector::ZeroVector;
	FVector LeanPivot = FVector::ZeroVector;
	uint8 LookStage = 255;
	uint16 LookLost = 0xffff;
	int32 LookDents = -1;
	bool bWasRiding = false;
};

/** Per-world bits: the merged debris piles (local on every machine) and the awake count. */
UCLASS()
class ANTKITCHEN_API AAntWreckPiles : public AActor
{
	GENERATED_BODY()
public:
	AAntWreckPiles();
	static AAntWreckPiles* Get(UWorld* World);
	/** Adds a lying chunk (sections in mesh space, placed at Where) to the pile of piece Key. */
	void AddToPile(int32 Key, const TArray<struct FProcMeshSection>& Sections, const TArray<class UMaterialInterface*>& Mats, const FTransform& Where);
	void Clear();
	UPROPERTY() TMap<int32, UProceduralMeshComponent*> Piles;
	UPROPERTY() USceneComponent* Origin = nullptr;
	int32 NumChunks = 0;
};
