#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntBody.h"
#include "AntRagdoll.generated.h"

class AAntCharacter;
class AKitchenLoot;
class UStaticMeshComponent;
class USceneComponent;

/**
 * A dead ant (or a leg it lost): the pieces of the jointed body (AntBody.h) as a ragdoll. Cosmetic, never replicated:
 * every machine that draws spawns its own from the same cause, direction and pose, so everyone sees much the same.
 *
 * No physics bodies (an ant is 15 units long, its tarsus half a unit: far below what the engine's solver is tuned for,
 * and dozens of them must cost nothing). The body is 33 points joined by rigid links, moved by position-based dynamics
 * (Verlet steps, a few relaxation passes, one line trace per point against the kitchen), and the pieces are drawn
 * between the points. It cannot explode: links only ever pull points together. A corpse that has come to rest stops
 * simulating; only so many may be awake, and only so many may lie about.
 *
 * - squashed: no simulation, the pose is pressed flat and travels with the slipper that made it;
 * - torn: the waist link is cut and the two halves fly apart with strands of goo between them;
 * - poisoned, burnt, drowned, frozen: it sways on its feet, they give way, the legs fold in;
 * - "bring the body home": the thorax follows the body loot, everything else dangles from it.
 */
UCLASS()
class ANTKITCHEN_API AAntCorpse : public AActor
{
	GENERATED_BODY()

public:
	AAntCorpse();
	virtual void Tick(float DeltaSeconds) override;

	static AAntCorpse* Spawn(UWorld* World, const FAntBodySnapshot& Pose, uint8 Kind, const FVector& Dir, AAntCharacter* Of);
	/** One leg torn off a living ant: it flies off, bounces and lies there. */
	static AAntCorpse* SpawnLeg(UWorld* World, const FAntBodySnapshot& Pose, int32 Leg, const FVector& Velocity);
	/** The body became loot (a teammate may carry it home): from now on the corpse is where that loot is. */
	void RideLoot();
	static FString Stats(const UWorld* World);
	/** Dev: where the newest dead body lies. */
	static bool NewestAt(const UWorld* World, FVector& Out);

	static constexpr int32 MaxCorpses = 18;   // lying about
	static constexpr int32 MaxAwake = 8;      // simulating
	static constexpr int32 MaxLegs = 24;      // loose legs

private:
	enum { T0, T1, TL, TR, TU, HD, P0, G0, G1, LegPts, FeelPts = LegPts + 18, NumPts = FeelPts + 6 };
	struct FPt
	{
		FVector X = FVector::ZeroVector;
		FVector Prev = FVector::ZeroVector;
		float Rad = 0.5f;
		bool bOn = false;
	};
	struct FLink
	{
		int32 A = 0, B = 0;
		float Len = 0.f;
		float WA = 0.5f;       // how much of the correction A takes
		bool bMin = false;     // only keeps them from coming closer than Len
	};

	void Build(const FAntBodySnapshot& Pose, int32 OnlyLeg);
	void Step(float H);
	void SolveCore();
	void Frames();
	void SolveLimbs(float Curl, float Hold);
	void Collide(FPt& P, float H);
	void Draw();
	void Flatten();
	void Sanitize();
	void AddVelocity(int32 From, int32 To, const FVector& Vel);
	void AddSpin(int32 From, int32 To, const FVector& Axis, float Rate);
	void Sleep();
	void TryRide();
	void Goo(int32 Count, float Speed, const FVector& At);
	static void Register(AAntCorpse* C);

	UPROPERTY() USceneComponent* Root = nullptr;
	UPROPERTY() USceneComponent* FlatRoot = nullptr;
	UPROPERTY() UStaticMeshComponent* Thorax = nullptr;
	UPROPERTY() UStaticMeshComponent* Head = nullptr;
	UPROPERTY() UStaticMeshComponent* Jaw = nullptr;
	UPROPERTY() UStaticMeshComponent* Petiole = nullptr;
	UPROPERTY() UStaticMeshComponent* Gaster = nullptr;
	UPROPERTY() UStaticMeshComponent* Gland = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> LegComps;      // 6 x (coxa, femur, tibia, tarsus)
	UPROPERTY() TArray<UStaticMeshComponent*> FeelComps;     // 2 x (scape, flag A, flag B)
	UPROPERTY() TArray<UStaticMeshComponent*> Strands;
	UPROPERTY() TArray<UMaterialInterface*> KeepMats;

	FPt Pts[NumPts];
	TArray<FLink> Core;
	bool bLeg[6] = { false, false, false, false, false, false };
	bool bFeel[2] = { false, false };
	bool bBody = false;          // false: a loose leg
	bool bTorn = false;
	FVector HipLocal[6];         // in the thorax piece's space
	FVector HipW[6];
	FVector FeelLocal[2];        // in the head piece's space
	FVector FeelW[2];
	FVector LimbRest[NumPts];    // limb points in the thorax's space, as they were at death
	FVector CurlAt[NumPts];      // the same, folded in
	FVector HoldAt[TU + 1];      // world: where the thorax stood
	FVector ToePin[6];
	float LenHK[6], LenKA[6], LenAE[6];
	FQuat RelThorax = FQuat::Identity, RelHead = FQuat::Identity, RelPetiole = FQuat::Identity, RelGaster = FQuat::Identity;
	FQuat ThoraxQ = FQuat::Identity, HeadQ = FQuat::Identity;
	FVector ThoraxO = FVector::ZeroVector;
	float VS = 0.158f;
	float LegThick = 1.f;
	float JawOpen = 20.f;

	uint8 Kind = 0;
	float Age = 0.f;
	float Life = 45.f;
	float HoldFor = 0.f;         // how long it sways on its feet before they give way
	float CurlFor = 1.2f;
	float CurlMax = 0.f;
	float Still = 0.f;
	float Awake = 0.f;
	bool bAsleep = false;
	bool bFlat = false;
	bool bRolled = false;
	bool bHidden = false;
	FVector Up0 = FVector::UpVector;
	FVector Fwd0 = FVector::ForwardVector;
	FVector FlatOffset = FVector::ZeroVector;
	FVector FlatVel = FVector::ZeroVector;
	bool bFlatFalling = false;
	float StepAcc = 0.f;
	int32 StepCount = 0;
	int32 RestIdx[3] = { 0, 0, 0 };
	FVector RestRef[3];
	float RestClock = 0.f;
	int32 RestHits = 0;
	float RideDrop = 0.f, RideDropWant = 0.f, NextDropCheck = 0.f;
	uint32 Seed = 0;

	TWeakObjectPtr<AAntCharacter> Of;
	bool bHadOwner = false;
	bool bWantLoot = false;
	float LootSearch = 0.f;
	float NextLootTry = 0.f;
	FVector OfLastAt = FVector::ZeroVector;
	TWeakObjectPtr<AKitchenLoot> Riding;
	FTransform RideLocal[TU + 1];
	float RideBlend = 0.f;
};

/**
 * The cheap death of the instanced ants (the army, the workers on a trail): no rig, no ragdoll. The ones a stomp or a
 * newspaper just killed stay where they fell for a while: tossed over onto their backs, then gone. One instanced mesh
 * for the whole world, capped; cosmetic and local like everything here.
 */
UCLASS()
class ANTKITCHEN_API AAntDeadPile : public AActor
{
	GENERATED_BODY()

public:
	AAntDeadPile();
	virtual void Tick(float DeltaSeconds) override;
	/** Killed instanced ants died around At just now (called on every machine from AAntSwarm::MulticastSplat). */
	static void Scatter(const UObject* WorldContext, const FVector& At, int32 Killed);

	static constexpr int32 MaxDead = 150;
	static constexpr float Life = 14.f;

private:
	struct FDead
	{
		FVector At = FVector::ZeroVector;
		FVector Up = FVector::UpVector;
		float Yaw = 0.f;
		float Age = 0.f;
		float Side = 1.f;
	};
	void Place(int32 Index, bool bDirty);

	UPROPERTY() class UInstancedStaticMeshComponent* Mesh = nullptr;
	TArray<FDead> Dead;
	float Scale = 0.05f;
	float HalfHeight = 1.f;
};
