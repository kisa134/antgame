#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "AntTypes.h"
#include "AntBody.generated.h"

class AAntCharacter;
class AAntGibs;
class AAntKitchenGameMode;
class AController;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UPrimitiveComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * The ant as a jointed insect (20.11.2026). -AntOldAnt keeps the previous one (a fused body on six two-bone legs).
 *
 * The Meshy ant was cut into rigid pieces (Tools/blender/ant_build.py -> /Game/Ant): head, lower jaw, thorax, petiole,
 * gaster, and for every leg coxa - femur - tibia - tarsus; each feeler is scape + two halves of the flagellum.
 * There is no skeleton and no AnimBP: UAntBodyComponent poses the pieces every frame on every machine from what is
 * replicated anyway (place, velocity, facing, the surface, health, LostLegs, what it carries, who holds it) plus one
 * small replicated record, FAntBodyAct: "what just happened to the body" (a hit from there, a knock, a dash, a bite).
 * Nothing here changes gameplay: the capsule, the speeds, the damage are where they were.
 *
 * Death: the pose is handed to AAntCorpse (AntRagdoll.h), a ragdoll of the same pieces simulated on each machine.
 */

/** What just happened to the body. Replicated only when it changes; late joiners get the last one (and ignore it: it is old). */
USTRUCT()
struct FAntBodyAct
{
	GENERATED_BODY()

	UPROPERTY() uint8 Kind = 0;
	/** 0..255: how hard. */
	UPROPERTY() uint8 Power = 0;
	/** Server world time. */
	UPROPERTY() float Start = -100.f;
	/** World: where the blow pushes the ant / where it lunges. */
	UPROPERTY() FVector_NetQuantizeNormal Dir;
};

namespace AntAct
{
	enum : uint8 { None, Flinch, Knock, Dash, Bite };
}

/** AAntCharacter::DeathKind. 0..2 are the old meanings; the rest pick how the ragdoll starts. */
namespace AntDeath
{
	enum : uint8
	{
		Collapse = 0,   // shot, bitten: the legs give way
		Squashed = 1,   // flat, legs splayed; sticks to the slipper
		Torn = 2,       // two halves and strands of goo
		Flung = 3,      // swatted, batted, swept: flies off and tumbles
		Curl = 4,       // poisoned: staggers and curls up
		Fall = 5,       // died in the air: crumples where it lands
		Eaten = 6,      // nothing left
		Burnt = 7,      // curls up, charred
		Drowned = 8,    // curls up
		Frozen = 9,     // stiff, curls slowly
	};
	inline bool IsCurl(uint8 K) { return K == Curl || K == Burnt || K == Drowned || K == Frozen; }
}

/** The pieces and what they are drawn with. */
struct FAntBodyArt
{
	UStaticMesh* Head = nullptr;
	UStaticMesh* Jaw = nullptr;
	UStaticMesh* Thorax = nullptr;
	UStaticMesh* Petiole = nullptr;
	UStaticMesh* Gaster = nullptr;
	UStaticMesh* Tibia = nullptr;
	UStaticMesh* Tarsus = nullptr;
	UStaticMesh* FlagA = nullptr;
	UStaticMesh* FlagB = nullptr;
	/** The Meshy parts that are used as they are (copies: AntArt's own table may move when it grows). */
	AntArt::FPart Coxa, Femur, Scape;
	bool bValid = false;
};

/** Material slots of one ant (its own instances: the colony tint is a parameter). */
namespace AntMat
{
	enum : uint8 { Body, Coxa, Femur, Tibia, Scape, Flag, Count };
}

/** The numbers of the rig, in ant-local units (the body is 98 long; multiply by AAntCharacter::VisScale() for the world). */
namespace AntRig
{
	constexpr int32 NumLegs = 6;
	constexpr float CoxaLen = 6.f;
	constexpr float FemurLen = 23.f;
	constexpr float TibiaLen = 26.f;
	constexpr float TarsusLen = 8.5f;
	constexpr float TarsusAcross = 2.7f;   // the claws, tip to tip
	constexpr float ScapeLen = 17.f;
	constexpr float FlagALen = 10.5f;
	constexpr float FlagBLen = 13.5f;
	/** Front, middle, hind legs: the hind ones are the longest. */
	inline float PairLen(int32 Pair) { return Pair == 0 ? 0.92f : Pair == 1 ? 1.f : 1.14f; }

	ANTKITCHEN_API bool Enabled();
	ANTKITCHEN_API const FAntBodyArt& Art();
	ANTKITCHEN_API FVector HipLocal(int32 Leg);
	ANTKITCHEN_API FVector HomeLocal(int32 Leg);
	ANTKITCHEN_API FVector FeelerLocal(int32 Side);          // on the head piece
	ANTKITCHEN_API FVector Joint(int32 Piece);               // 0 thorax 1 head 2 jaw 3 petiole 4 gaster (body frame)
	/** A limb piece of /Game/Ant (100 long from its joint along +X) from A to B. Across: the thickness in world units. */
	ANTKITCHEN_API void PlaceLimb(UStaticMeshComponent* C, const FVector& A, const FVector& B, const FVector& Up, float Across, float Natural);
	ANTKITCHEN_API UStaticMeshComponent* AddPiece(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Mat, bool bAbsolute);
}

/** The live pose handed to the corpse (world space). */
struct FAntBodySnapshot
{
	float VS = 0.158f;
	FTransform Thorax, Head, Petiole, Gaster;
	float JawOpen = 0.f;
	FVector Hip[6], Knee[6], Ankle[6], Toe[6];
	bool bLeg[6] = { true, true, true, true, true, true };
	FVector FeelerBase[2], FeelerElbow[2], FeelerMid[2], FeelerTip[2];
	FVector Velocity = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	FVector Fwd = FVector::ForwardVector;
	UMaterialInterface* Mats[AntMat::Count] = {};
	UMaterialInterface* GlandMat = nullptr;
	float LegThick = 1.f;
};

struct FAntRigLeg
{
	int32 Side = 1;             // -1 left, +1 right
	int32 Pair = 0;             // 0 front, 1 middle, 2 hind
	int32 Group = 0;            // the tripod it steps with
	float Len = 1.f;
	float Off = 0.f;            // its place in the gait cycle
	bool bLost = false;

	/** The foot: world, or in Base's space while it stands on something that moves. */
	FVector Foot = FVector::ZeroVector;
	TWeakObjectPtr<UPrimitiveComponent> Base;
	FVector FootW = FVector::ZeroVector;
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	TWeakObjectPtr<UPrimitiveComponent> ToBase;
	float T = 1.f;              // < 1: swinging
	float Dur = 0.1f;
	float Lift = 0.f;           // 0 on the ground .. 1 at the top of the swing
	float Phase = 0.f;
	bool bInit = false;
	bool bGround = false;       // the foot has a surface under it
	bool bRefined = false;

	FVector Knee = FVector::ZeroVector, Ankle = FVector::ZeroVector, HipW = FVector::ZeroVector, CoxaEnd = FVector::ZeroVector;

	UStaticMeshComponent* Coxa = nullptr;
	UStaticMeshComponent* Femur = nullptr;
	UStaticMeshComponent* Tibia = nullptr;
	UStaticMeshComponent* Tarsus = nullptr;
};

struct FAntRigFeeler
{
	float Yaw = 25.f, Pitch = 40.f, Elbow = -70.f, Tip = -10.f;
	float YawV = 0.f, PitchV = 0.f, ElbowV = 0.f, TipV = 0.f;
	float TapAt = 0.f;
	float TapUntil = 0.f;
	FVector BaseW = FVector::ZeroVector, ElbowW = FVector::ZeroVector, MidW = FVector::ZeroVector, TipW = FVector::ZeroVector;
	UStaticMeshComponent* Scape = nullptr;
	UStaticMeshComponent* FlagA = nullptr;
	UStaticMeshComponent* FlagB = nullptr;
};

UCLASS()
class ANTKITCHEN_API UAntBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Builds the pieces under Parent (the ant's VisualRoot). Null with -AntOldAnt or when /Game/Ant is not in the build. */
	static UAntBodyComponent* Build(AAntCharacter* Ant, USceneComponent* Parent);

	/** Every frame, from AAntCharacter::UpdateVisuals (after VisNormal and VisHeading are known). */
	void Update(float Dt, bool bNear);
	/** Something happened to the body (replicated FAntBodyAct, or predicted by the owner). */
	void OnAct(const FAntBodyAct& Act);
	/** The ant died: the live pieces vanish and a corpse takes the pose. */
	void Die();
	/** The body became loot ("bring the body home"): the corpse rides that loot from now on. */
	void CorpseToLoot();

	/** Where the gait cycle is, 0..1 (ants hauling one load step together). */
	float GetPhase() const { return Phase; }
	int32 GetLod() const { return Lod; }
	FString Describe() const;
	FVector HeadLocation() const;
	bool Snapshot(FAntBodySnapshot& Out) const;
	/** A rigid piece: 0 thorax, 1 head, 2 jaw, 3 petiole, 4 gaster (cosmetics hang on them, AntCosmetics.cpp). */
	UStaticMeshComponent* GetPiece(int32 Which) const { return Which == 0 ? Thorax : Which == 1 ? Head : Which == 2 ? Jaw : Which == 3 ? Petiole : Which == 4 ? Gaster : nullptr; }

	/** Dev: hold the pose (contact sheets). */
	bool bDevFreeze = false;
	/** Dev: 0 none, 1.. force an idle act. */
	int32 DevIdle = 0;
	int32 DevLod = -1;            // dev: force the level of detail (-1: by distance)

private:
	void BuildPieces(USceneComponent* Parent);
	void UpdateLod(bool bNear);
	void UpdateGait(float Dt);
	void UpdateBodyFrame(float Dt);
	void UpdateSpine(float Dt);
	void UpdateLegs(float Dt);
	void UpdateFeelers(float Dt);
	void UpdateInjuries(float Dt);
	void PlacePieces();
	void ResetPose();
	bool FindFoothold(const FAntRigLeg& L, const FVector& Want, FVector& OutPoint, UPrimitiveComponent*& OutComp) const;
	FVector HomeWorld(const FAntRigLeg& L) const;
	FVector LegVelocity(const FAntRigLeg& L) const;
	void StartStep(FAntRigLeg& L, float Dur, bool bFromPhase);
	void SetFoot(FAntRigLeg& L, const FVector& World, UPrimitiveComponent* Comp);
	FVector FootWorld(const FAntRigLeg& L) const;
	void ApplyLostLegs();
	void SetLegsVisible(bool bVisible);
	void Retime();
	float Hash01(int32 Salt) const;

	UPROPERTY() AAntCharacter* Ant = nullptr;
	UPROPERTY() UStaticMeshComponent* Thorax = nullptr;
	UPROPERTY() UStaticMeshComponent* Head = nullptr;
	UPROPERTY() UStaticMeshComponent* Jaw = nullptr;
	UPROPERTY() UStaticMeshComponent* Petiole = nullptr;
	UPROPERTY() UStaticMeshComponent* Gaster = nullptr;
	UPROPERTY() UStaticMeshComponent* Gland = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Limbs;     // legs and feelers (kept alive here; the structs below hold raw pointers)
	UPROPERTY() TArray<UStaticMeshComponent*> Wounds;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> Mats;
	UPROPERTY() UMaterialInterface* GlandMat = nullptr;

	FAntRigLeg Legs[6];
	FAntRigFeeler Feelers[2];

	// ---- what the ant is doing, read once per update
	FVector P = FVector::ZeroVector, N = FVector::UpVector, H = FVector::ForwardVector, R = FVector::RightVector;
	FVector V = FVector::ZeroVector, VPrev = FVector::ZeroVector, Acc = FVector::ZeroVector;
	FVector HPrev = FVector::ForwardVector;
	FVector PPrev = FVector::ZeroVector, Moved = FVector::ZeroVector;
	float YawRate = 0.f;
	float VS = 0.158f;
	float Ground = 26.f;          // ant-local: how far below the capsule's centre the surface is
	float SpeedAL = 0.f;
	float Time = 0.f;
	float LodDt = 0.f;
	int32 Lod = 0;
	int32 Frame = 0;
	bool bCrawl = true, bAir = false, bHeld = false, bWasAir = false, bLegsShown = true;
	uint8 Mode = 0;               // see AntBody.cpp
	float ModeTime = 0.f;
	float AirTime = 0.f;
	FVector AirVel = FVector::ZeroVector;

	// ---- gait
	float Phase = 0.f;
	float Stride = 60.f, Duty = 0.6f;
	float Wall = 0.f;             // 0 floor .. 1 wall or ceiling
	int32 NumAlive = 6;
	uint8 ShownLost = 0;
	bool bPhaseGait = false;
	float StepClock = 0.f;

	// ---- the body on its legs
	FQuat BodyQ = FQuat::Identity;
	FVector BodyO = FVector::ZeroVector;
	float FitA = 0.f, FitB = 0.f, FitC = 0.f, FitAV = 0.f, FitBV = 0.f, FitCV = 0.f;
	FVector Lean = FVector::ZeroVector, LeanV = FVector::ZeroVector;     // world: where the top of the body is pushed
	FVector Shove = FVector::ZeroVector, ShoveV = FVector::ZeroVector;   // world: the body knocked off its feet for a moment
	float Dip = 0.f, DipV = 0.f;
	float Spin = 0.f, SpinRate = 0.f;
	FVector SpinAxis = FVector::RightVector;
	float Flip = 0.f;             // 1 = on its back
	float Effort = 0.f, Wound = 0.f, Stun = 0.f, Strain = 0.f;
	float Sag = 0.f;
	FVector SagDir = FVector::ZeroVector;

	// ---- spine
	FVector GasterTip = FVector::ZeroVector, GasterTipV = FVector::ZeroVector;
	bool bGasterInit = false;
	FQuat HeadQ = FQuat::Identity;
	float HeadYaw = 0.f, HeadPitch = 0.f, HeadYawV = 0.f, HeadPitchV = 0.f, HeadRoll = 0.f;
	float JawOpen = 0.f, JawV = 0.f, JawWant = 0.f;
	FVector LookAt = FVector::ZeroVector;
	bool bLookAt = false;
	float NextLook = 0.f;
	FTransform ThoraxW, HeadW, PetioleW, GasterW;

	// ---- acts and idle life
	FAntBodyAct Act;
	float ActAge = 100.f;
	float LastActStart = -100.f;
	float StillTime = 0.f;
	int32 Idle = 0;
	float IdleT = 0.f, IdleDur = 0.f, NextIdle = 3.f;
	int32 IdleSide = 0;

	// ---- injuries
	float LastHealth = -1.f;
	float Hurt = 0.f;             // a burst right after losing health
	float NextDrip = 0.f;
	int32 LimpLeg = -1;
	int32 BrokenFeeler = -1;
	TWeakObjectPtr<AAntGibs> Drips;
	float DripsBorn = 0.f;
	TWeakObjectPtr<class AAntCorpse> Corpse;

	friend class AAntCorpse;
};

/** Rules and dev tools that sit next to the body. */
namespace AntBodyRules
{
	/** Server, inside AAntCharacter::Die: what kind of death this is for the ragdoll (may refine DeathKind and DeathDir). */
	ANTKITCHEN_API void ClassifyDeath(AAntCharacter* Ant, const AController* Killer, const AActor* Causer);
	/** Dev (-AntDo): "ant ..." commands. True if the command was one of ours. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** Dev: the next death is of this kind (-1: decided by the rules). */
	ANTKITCHEN_API void ForceDeath(int32 Kind, const FVector& Dir);
}
