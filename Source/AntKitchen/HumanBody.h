#pragma once

#include "CoreMinimal.h"
#include "NpcBody.h"

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

/** Who wears the figure: the Dad, or one of the family built on the same 19 segments with their own tables (Tools/blender/npc_folk.py). */
namespace HumanFig
{
	enum EId : uint8 { Dad, Grandma, Kid, Count };
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
		// the family
		Sit,      // down onto the seat at Target and stays there. Arg: 0 knitting, 1 dozing, 2 playing on the floor
		Rise,     // up from the seat at Target
		Sweep,    // grandma's broom: drawn back (the warning), then one long stroke across the floor through Target
		Fall,     // slipped in a puddle: the feet go out, he lands flat on his back, lies, sits up, gets up. Arg: the slipping leg
		Wipe,     // down on his haunches, wiping the floor at Target with a towel. Arg: seconds * 10
	};
	/** The fall (HumanAct::Fall), seconds from the slip: the arms flail (the tell), the back hits the floor, he sits up, he stands. */
	constexpr float FallHit = 0.92f;
	constexpr float FallSitUp = 3.4f;
	constexpr float FallUp = 6.0f;
	constexpr float SitTime = 1.1f;       // times the figure's own slowness
	constexpr float SweepHit = 0.85f;
	/** Seconds from the start to the hit / the grip / the release. */
	constexpr float StompHit = 0.55f;
	constexpr float SwatHit = 0.42f;
	constexpr float SlapHit = 0.50f;
	constexpr float GrabGrip = 0.40f;
	constexpr float ThrowRelease = 0.30f;
	constexpr float ReachTouch = 0.40f;
}

// The segment table, the planted foot, the looks, two-bone IK and the easing helpers are shared with the other
// jointed creatures of the house (NpcBody.h): grandma and the kid are this same figure with their own tables,
// the cat and the dog use the same kit on four legs (QuadBody.h).
using FHumanSegDef = FNpcSegDef;
/** One foot of the walk: planted in the world, or swinging to where it will be planted next. */
using FHumanFoot = FNpcFoot;

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
	float SprayW = 0.f, HoldW = 0.f, PanicW = 0.f, StunW = 0.f, RageW = 0.f, PeerW = 0.f, SitW = 0.f, AcheW = 0.f;
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

/** How one of the figures carries itself: size, tempo, posture, how it hits. */
struct FHumanStyle
{
	float Scale;          // the whole figure, on top of the model (the Dad 1)
	float CapsuleR, CapsuleH;
	float WalkSpeed;
	float Tempo;          // cadence and how fast the body follows
	float Hunch;          // degrees bent forward all the time
	float ArmsOut;        // degrees the arms are held out from the body
	float SwatHit;        // seconds from the wind-up to the hit
	float SwatRadius;
	float SwatEvery;
	float SitSlow;        // sitting down / getting up takes HumanAct::SitTime times this
};

namespace HumanBody
{
	/** uu per mesh centimetre (1 m = 1900 uu). */
	constexpr float Scale = 19.f;
	ANTKITCHEN_API TConstArrayView<FHumanSegDef> Segs(uint8 Fig = HumanFig::Dad);
	ANTKITCHEN_API const TCHAR* SegName(int32 Seg);
	/** Loads the segment's mesh (rooted cache, survives the map reload from the menu). */
	ANTKITCHEN_API class UStaticMesh* Mesh(int32 Seg, uint8 Fig = HumanFig::Dad);
	/** Is the jointed body in use? The Dad: false with -AntOldHuman or when /Game/Human is missing. The family: false with -AntOldNpc or when /Game/Npc is. */
	ANTKITCHEN_API bool Enabled(uint8 Fig = HumanFig::Dad);
	/** The game's surface looks on a segment, one shared material per slot name. */
	ANTKITCHEN_API void ApplyLook(UStaticMeshComponent* C, uint8 Fig = HumanFig::Dad);
	ANTKITCHEN_API const FHumanStyle& Style(uint8 Fig);
}
