#pragma once

#include "CoreMinimal.h"
#include "NpcBody.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * The four-legged bodies: the cat and Бадрик the French bulldog. Rigid segments (Blender sets in /Game/Npc,
 * Tools/blender/npc_cat.py and npc_dog.py), every one a solid mesh that ants crawl on and ride. No skeleton and no
 * AnimBP: QuadBody::Pose places the segments every frame from the replicated state (where it is, how fast it moves,
 * its posture, its action and when that started, what it looks at), the way the Human's body is done.
 */
namespace QuadSeg
{
	enum EId : uint8
	{
		Hips, Belly, Chest, Neck, Head, Jaw, EarL, EarR, Tail1, Tail2, Tail3, Tongue,
		FLU, FLL, FLP,     // front left: upper, lower, paw
		FRU, FRL, FRP,
		HLU, HLL, HLP,     // hind left
		HRU, HRL, HRP,
		Count
	};
}

/** The posture the body keeps (ACritter::State, replicated). */
namespace QuadMode
{
	enum EId : uint8
	{
		Sleep,      // cat: curled up; dog: flat on his belly, legs splayed
		Move,       // standing, walking, trotting, galloping
		NoseDown,   // dog: nose to the floor (sniffing a trail, eating)
		Sit,
		Air,        // mid-jump (the action says where to)
		Stalk,      // cat: low to the floor, creeping
		Fright,     // cat: arched, puffed, sideways
		Count
	};
}

/** How it feels (ACritter::Mood, replicated): tail and ears. */
namespace QuadMood
{
	enum EId : uint8 { Calm, Hunting, Scared, Happy, Sulking };
}

/** What the body is doing right now (FNpcActState::Kind). */
namespace QuadAct
{
	enum EId : uint8
	{
		None,
		// the cat
		Stretch,    // waking up: the long bow, the yawn
		Wash,       // sitting: licks a paw, wipes its face. Arg: seconds * 10
		Wiggle,     // THE WARNING before the pounce: chest down, rump up and wiggling
		Pounce,     // flies From -> Target, lands on the front paws. Arg: flight seconds * 100
		Swipe,      // a front paw is raised (the warning), then bats across the floor at Target. Arg: paw (0 left, 1 right)
		Pin,        // a front paw comes down on Target and stays. Arg: seconds * 10
		Jump,       // onto furniture / down from it: crouch, then flies From -> Target. Arg: flight seconds * 100
		Startle,    // bitten or hit: leaps in place, back arched
		// the dog
		Twitch,     // THE WARNING before the tongue: the nose freezes over Target and twitches
		Lick,       // the tongue shoots out to Target and back
		Eat,        // head down at Target, chewing. Arg: seconds * 10
		Bark,       // Arg: how many barks
		Shake,      // shakes himself: ears and jowls flying
		Beg,        // sitting up at Target (the table). Arg: seconds * 10
		Yelp,       // bitten: jumps, tucks his tail
		Count
	};
	constexpr float WiggleTime = 0.6f;     // from the start of the wiggle to the leap
	constexpr float SwipeHit = 0.45f;      // from the paw going up to the bat
	constexpr float PinHit = 0.22f;
	constexpr float JumpCrouch = 0.35f;    // crouch before a jump onto furniture
	constexpr float TwitchTime = 0.55f;    // from the nose freezing to the tongue
	constexpr float LickHit = 0.12f;       // the tongue reaches the target
	constexpr float BarkEvery = 0.42f;
	/** How long the body is busy with it. */
	ANTKITCHEN_API float Duration(uint8 Kind, uint8 Arg);
}

/** Everything the pose remembers between frames (local to each machine, never replicated). */
struct FQuadRig
{
	FNpcFoot Feet[4];                     // FL, FR, HL, HR
	bool bDriven[4] = { false, false, false, false };
	float Gait = 0.f;
	bool bWasMoving = false;
	float NextIdleStep = 0.f;
	// smoothed pose channels
	float HipZ = 0.f, ShZ = 0.f, HipX = 0.f, ShX = 0.f, BodyY = 0.f, Curl = 0.f, Roll = 0.f, Arch = 0.f, HipPitch = 0.f, ChestPitch = 0.f, HipSway = 0.f;
	float HeadYaw = 0.f, HeadPitch = 0.f, HeadRoll = 0.f, Neck = 30.f, Jaw = 0.f, EarBack = 0.f, EarOut = 0.f;
	float TailLift = 30.f, TailCurl = 0.f, TailSide = 0.f, TailWag = 0.f, TailPuff = 1.f, TailPuffSet = 1.f;
	float Tongue = 0.f, TongueSet = -1.f;
	float Dip[2] = { 0.f, 0.f }, DipVel[2] = { 0.f, 0.f };   // front / hind: the body sinking under a footfall
	float Clock = 0.f, Breath = 0.f, WagPhase = 0.f;
	float Glance = 0.f, GlancePitch = 0.f, NextGlance = 0.f;
	float EarFlick[2] = { 0.f, 0.f }, NextEarFlick = 0.f;
	float YawRate = 0.f, LastYaw = 0.f, Lean = 0.f;
	bool bInit = false;
	/** The last pose, body space: origin on the floor under the creature, X forward, Y right. */
	FVector SegPos[QuadSeg::Count];
	FQuat SegRot[QuadSeg::Count];
};

/** What the pose is made from. */
struct FQuadIn
{
	uint8 Kind = 1;          // 0 dog, 1 cat
	uint8 Mode = QuadMode::Move;
	uint8 Mood = QuadMood::Calm;
	uint8 Act = QuadAct::None;
	uint8 Arg = 0;
	float ActT = 100.f;      // seconds since the action started
	FVector ActTarget = FVector::ZeroVector;   // world
	FVector LookAt = FVector::ZeroVector;      // world; zero: nothing in particular
	FTransform Body;         // the floor point under the creature, yawed the way it faces
	FVector Vel = FVector::ZeroVector;         // world
	float Now = 0.f;         // local world time
	float Dt = 0.f;
};

namespace QuadBody
{
	/** uu per mesh centimetre (1 m = 1900 uu). */
	constexpr float Scale = 19.f;
	ANTKITCHEN_API TConstArrayView<FNpcSegDef> Segs(uint8 Kind);
	/** "Hips", "FLP"... the same on every machine: component names. */
	ANTKITCHEN_API const TCHAR* SegName(int32 Seg);
	ANTKITCHEN_API UStaticMesh* Mesh(uint8 Kind, int32 Seg);
	/** Is the jointed body of this kind in use? False with -AntOldNpc or when its meshes are missing from the build. */
	ANTKITCHEN_API bool Enabled(uint8 Kind);
	ANTKITCHEN_API void ApplyLook(uint8 Kind, UStaticMeshComponent* C);
	/** Where the bind pose puts a segment (before the first pose). */
	ANTKITCHEN_API FVector BindJoint(uint8 Kind, int32 Seg);
	/** The path of a jump: T 0..1 from From to To. */
	ANTKITCHEN_API FVector AirPoint(const FVector& From, const FVector& To, float T);
	/** How long a jump of that length takes, seconds. */
	ANTKITCHEN_API float AirTime(const FVector& From, const FVector& To, bool bPounce);
	/** How far below a paw's joint its sole is (front / hind), and the point of the sole in the paw's mesh space. */
	ANTKITCHEN_API float PawDrop(uint8 Kind, bool bHind);
	/** The length of the tongue's mesh along its X, uu. */
	ANTKITCHEN_API float TongueLength(uint8 Kind);
	/** Places every segment for this frame. Footfall(leg, strength, world point) is called when a paw lands. */
	ANTKITCHEN_API void Pose(const FQuadIn& In, FQuadRig& R, TArrayView<UStaticMeshComponent*> Segs, TFunctionRef<void(int32, float, const FVector&)> Footfall);
}
