#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "NpcBody.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class AAntCharacter;
class AAntKitchenGameMode;
class AActor;
class UWorld;

/**
 * What the jointed creatures of the house share (the Human, grandma, the kid, the cat, the dog): the segment table,
 * the looks, two-bone IK, a planted foot, the replicated "what the body is doing" state, and the house rules about
 * how dangerous each of them may be on which shift. The poses themselves live in HumanBody.cpp (people) and
 * QuadBody.cpp (the cat and the dog). -AntOldNpc keeps the old primitive-built pets and family.
 */

/** What a body is doing: the action, since when (server world time), at what. Replicated only when it changes: late joiners see the right pose. */
USTRUCT()
struct FNpcActState
{
	GENERATED_BODY()

	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() uint8 Arg = 0;
	UPROPERTY() float Start = -100.f;
	UPROPERTY() FVector_NetQuantize Target;
	/** Where the body was when the action started (jumps are flown from here to Target on every machine). */
	UPROPERTY() FVector_NetQuantize From;
};

/** One rigid segment: its mesh, its parent segment, its joint in the bind pose (body frame, uu). */
struct FNpcSegDef
{
	const TCHAR* Name;
	int32 Parent;
	FVector Joint;
};

/** Material slot -> the game's surface look. Tex null: plain paint. */
struct FNpcLook
{
	const TCHAR* Slot;
	const TCHAR* Tex;
	float Size;
	float R, G, B;
	float Rough;
	const TCHAR* Step;
};

/** One foot (or paw): planted in the world, or swinging to where it will be planted next. */
struct FNpcFoot
{
	FVector Pos = FVector::ZeroVector;    // world: the middle of the sole
	float Yaw = 0.f;                      // world
	FVector From = FVector::ZeroVector;
	float FromYaw = 0.f;
	float T = 1.f;                        // < 1: swinging
	float Dur = 0.5f;
	float Lift = 0.f;                     // height of the sole above the floor right now
	float Pitch = 0.f;                    // degrees, toes up > 0
	bool bInit = false;
};

namespace NpcRig
{
	inline FQuat RX(float Deg) { return FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Deg)); }
	/** About Y: something pointing up leans FORWARD for a positive angle, a hanging limb swings BACK, a nose goes DOWN. */
	inline FQuat RY(float Deg) { return FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Deg)); }
	inline FQuat RZ(float Deg) { return FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Deg)); }
	inline float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	inline float EaseIn(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T; }
	inline float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T); }
	inline float Ramp(float T, float A, float B) { return FMath::Clamp((T - A) / FMath::Max(1.e-4f, B - A), 0.f, 1.f); }
	inline void Approach(float& V, float Target, float Rate, float Dt) { V += (Target - V) * (1.f - FMath::Exp(-Rate * Dt)); }
	inline float Deg(float Rad) { return FMath::RadiansToDegrees(Rad); }
	/** A bump: 0 at A, 1 between B and C, 0 at D. */
	inline float Bump(float T, float A, float B, float C, float D) { return Ease(Ramp(T, A, B)) * (1.f - Ease(Ramp(T, C, D))); }

	/** A limb segment hangs along its local -Z: the rotation that points it along Dir, its front (local X) towards XHint. */
	inline FQuat BoneRot(const FVector& Dir, const FVector& XHint) { return FRotationMatrix::MakeFromZX(-Dir, XHint).ToQuat(); }
	/** A segment lying along its local +X: the rotation that points it along Dir, its top (local Z) towards ZHint. */
	ANTKITCHEN_API FQuat AlongX(const FVector& Dir, const FVector& ZHint);
	/** Two-bone IK: the middle joint of A -> T. T is pulled back into reach. OutPole: the side the joint sticks out to. */
	ANTKITCHEN_API FVector TwoBone(const FVector& A, FVector& T, float L1, float L2, const FVector& PoleHint, const FVector& Fallback, FVector& OutPole);

	/** One material per look, shared by every segment that uses it. Rooted: the cache is static and must survive the map reload from the menu. */
	ANTKITCHEN_API UMaterialInstanceDynamic* Material(const FNpcLook& L);
	/** Gives every material slot of the component's mesh its look from the table (by slot name). */
	ANTKITCHEN_API void ApplyLook(UStaticMeshComponent* C, TConstArrayView<FNpcLook> Looks);
	/** Loads <Dir>/<Name>/<Name> (rooted cache, survives map travel). Null when the build has no such mesh. */
	ANTKITCHEN_API UStaticMesh* LoadMesh(const TCHAR* Dir, const TCHAR* Name);
	/** The server's clock on every machine. */
	ANTKITCHEN_API float ServerNow(const AActor* A);
	/** Seconds since the action started. */
	ANTKITCHEN_API float ActTime(const AActor* A, const FNpcActState& Act);
}

/** Who lives in the house besides the Human. */
namespace NpcWho
{
	enum EId : uint8 { Dog, Cat, Grandma, Kid, Count };
}

/** Server-side helpers the creatures' brains share. */
namespace NpcWorld
{
	/** False with -AntOldNpc: the old primitive pets and family, with their old habits. */
	ANTKITCHEN_API bool NewNpcs();
	/**
	 * How bold a creature is right now, by shift (the heist run): 0 minds its own business (defends itself, reacts to what
	 * comes right up to it), 1 keeps to its own corner, 2 goes looking for trouble. Classic and war modes: always 1.
	 * -AntNpcLevel=N and the dev command "npclevel N" force it.
	 */
	ANTKITCHEN_API int32 Level(const UWorld* World, uint8 Who);
	ANTKITCHEN_API void ForceLevel(int32 Level);

	/** Damages every ant within R of C (and ArmyKills of the army ants there). */
	ANTKITCHEN_API void HurtAntsNear(UWorld* W, AActor* By, const FVector& C, float R, float Dmg, const TCHAR* Cause, int32 ArmyKills);
	/** Something solid of the house hangs low over this point (under a sofa, a cabinet, a shoe): a big paw or a hand cannot get there. */
	ANTKITCHEN_API bool UnderCover(const UWorld* W, const FVector& P, float Clearance);
	/** Nothing of the fixed house stands between the two points. */
	ANTKITCHEN_API bool CanSee(const UWorld* W, const FVector& From, const FVector& To);
	/** The top of whatever is under P (searching from Up above it down to Down below it); P.Z when nothing is found. */
	ANTKITCHEN_API float SurfaceZ(const UWorld* W, const FVector& P, float Up, float Down);
	/** A way round furniture: the direction to walk from From towards To for something this wide (flat, normalised; zero when boxed in). */
	ANTKITCHEN_API FVector Steer(const UWorld* W, const AActor* Me, const FVector& From, const FVector& To, float Radius, float Height, float& InOutSide, float& InOutSideUntil);

	/** Where ants have walked lately (stronger where food was dragged): what the dog's nose follows. Server only. */
	ANTKITCHEN_API void ScentTick(UWorld* W, float Dt);
	/** Scent at a point, 0..1+. */
	ANTKITCHEN_API float ScentAt(const UWorld* W, const FVector& P);
	/** The strongest scent within Radius of From that is at least MinStrength (and fresher towards where the ant went). False: nothing to follow. */
	ANTKITCHEN_API bool BestScent(const UWorld* W, const FVector& From, float Radius, float MinStrength, FVector& OutPoint, float& OutStrength);
	/** The nose has been here: this patch is sniffed out. */
	ANTKITCHEN_API void ScentClear(const UWorld* W, const FVector& P, float Radius);

	/** An ant bit something of the house within its reach. True if it bit a pet or one of the family (they react). */
	ANTKITCHEN_API bool AntBites(AAntCharacter* Ant, float Reach);
	/** A held ant struggles ([Space]): whoever holds it may lose its grip. */
	ANTKITCHEN_API void Struggle(AAntCharacter* Ant);
	/** Dev (-AntDo): commands for the creatures. True if the command was one of ours. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
