#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * The Human's jointed body: 19 rigid segments (Blender set in /Game/Human, Tools/blender/human_build.py), every one its
 * own solid mesh that ants crawl on and ride. No skeleton and no AnimBP: AHumanCharacter::PoseBody (HumanBody.cpp) places
 * the segments every frame from a handful of numbers, the way the ant's legs are done. -AntOldHuman keeps the old statue.
 */
namespace HumanSeg
{
	enum EId : uint8
	{
		Pelvis, Belly, Chest, Neck, Head,
		UpperArmL, ForearmL, HandL, FingersL,
		UpperArmR, ForearmR, HandR, FingersR,
		ThighL, ShinL, FootL,
		ThighR, ShinR, FootR,
		Count
	};
}

/** What the Human is doing with his body right now. Replicated with its start time, so every machine plays the same motion. */
namespace HumanAct
{
	enum EId : uint8
	{
		None,
		Stomp,    // Arg: leg. Wind-up 0.55 s, the slipper lands at GetStompPoint
		Swat,     // rolled newspaper (right hand) onto Target
		Slap,     // bare left hand onto the floor at Target
		Grab,     // left hand goes to the food at Target
		Throw,    // left hand throws what it holds towards Target
		Reach,    // left hand to a handle or a button at Target
		Shake,    // hopping and flailing: shaking ants off
		Peer,     // crouching to look under furniture at Target; Arg: seconds * 10
	};
	/** Seconds from the start to the hit / the grip / the release. */
	constexpr float StompHit = 0.55f;
	constexpr float SwatHit = 0.42f;
	constexpr float SlapHit = 0.50f;
	constexpr float GrabGrip = 0.40f;
	constexpr float ThrowRelease = 0.30f;
	constexpr float ReachTouch = 0.40f;
}

struct FHumanSegDef
{
	const TCHAR* Name;
	int32 Parent;
	FVector Joint;
};

/** One foot of the walk: planted in the world, or swinging to where it will be planted next. */
struct FHumanFoot
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

/** Everything the pose remembers between frames (local to each machine, never replicated). */
struct FHumanRig
{
	FHumanFoot Feet[2];
	bool bDriven[2] = { false, false };   // the foot is moved by an action (stomp, hop), not by the walk
	float GaitPhase = 0.f;
	bool bWasMoving = false;
	float NextIdleStep = 0.f;
	float Dip = 0.f, DipVel = 0.f;        // the pelvis sinking under a footfall: a spring
	float Sway = 0.f;                     // pelvis towards the standing leg
	float PelvisYaw = 0.f;
	float Crouch = 0.f, Bend = 0.f, Twist = 0.f, Lean = 0.f, Side = 0.f;
	float HeadYaw = 0.f, HeadPitch = -10.f;
	float Glance = 0.f, GlancePitch = -12.f, NextGlance = 0.f;
	float SprayW = 0.f, HoldW = 0.f, PanicW = 0.f, StunW = 0.f, RageW = 0.f, PeerW = 0.f;
	float Breath = 0.f, Clock = 0.f;
	float IdleShift = 0.f, NextShift = 0.f, ShiftTarget = 0.f;
	float HopPhase = 0.f;
	float SprayFx = 0.f;
	bool bPosed = false;
	FVector Shoulder[2] = { FVector::ZeroVector, FVector::ZeroVector };  // body space, of the last pose
	FVector Knee[2] = { FVector::ZeroVector, FVector::ZeroVector };
	/** The last pose, body space: origin on the floor under the capsule, X forward, Y right. */
	FVector SegPos[HumanSeg::Count];
	FQuat SegRot[HumanSeg::Count];
};

namespace HumanBody
{
	/** uu per mesh centimetre (1 m = 1900 uu). */
	constexpr float Scale = 19.f;
	ANTKITCHEN_API TConstArrayView<FHumanSegDef> Segs();
	ANTKITCHEN_API const TCHAR* SegName(int32 Seg);
	/** Loads SM_H_* (rooted cache, survives the map reload from the menu). */
	ANTKITCHEN_API class UStaticMesh* Mesh(int32 Seg);
	/** Is the jointed body in use? False with -AntOldHuman or when /Game/Human is missing from the build. */
	ANTKITCHEN_API bool Enabled();
	/** The game's surface looks on a segment, one shared material per slot name. */
	ANTKITCHEN_API void ApplyLook(UStaticMeshComponent* C);
}
