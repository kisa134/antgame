#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "AntCarry.generated.h"

class AKitchenLoot;
class AAntCharacter;
class AAntArmy;
class AHUD;
class UCanvas;
class AAntKitchenGameMode;

/**
 * Carrying together (AntCarry.h/.cpp, docs/FEATURES.md section 23).
 *
 * The load is a real physics body on the server. Every carrier holds it at its own point of the surface (where its jaws
 * closed on it) and is a spring-damper from that grip to its jaws. The weight is shared out by the lever arms (a carrier near
 * the centre of mass bears more; on a slope the lower one bears more); each carrier's force is limited by its strength
 * (a small reserve, then that end dips and drags). Strength above the weight becomes speed: a crowd runs, but ants brake
 * badly (their feet slip), so a fast load keeps going - it overshoots, slams into walls, drags the carriers off their feet.
 *
 * The ant is held by its grip in turn: its jaws stay on the grip (a tether on the machine that moves the ant), its body faces
 * the grip, sinks, leans and shortens its steps with its share; a jerk from a teammate pulls it off balance or out of the grip.
 *
 * Rhythm: carriers that go the same way at about the same speed lock their stride ("the beat"), the load rides smoothly and
 * they get a small bonus; out of step the beat rocks the load, opposite pulls twist it and cost speed.
 *
 * Carriers are ants (AAntCharacter: players, bots) or WORKERS of an army/squad (instanced, no actor of their own: a virtual
 * jaw point simulated here). Both join through the same API below.
 *
 * Network: the server simulates. While anybody carries it, the load's own movement replication is off and FAntCarryNet sends
 * the transform at ~20 Hz; clients make the body kinematic and interpolate (short extrapolation). Carriers' poses come from
 * the replicated grips on every machine. No RPCs.
 */

USTRUCT()
struct FAntCarryGrip
{
	GENERATED_BODY()

	/** The ant (AAntCharacter) or the army (AAntArmy) whose worker #Worker holds here. */
	UPROPERTY() AActor* Who = nullptr;
	/** 255: Who is an ant. Otherwise the worker index in its army. */
	UPROPERTY() uint8 Worker = 255;
	/** Where its jaws hold the load, in the load's own space. */
	UPROPERTY() FVector_NetQuantize10 Local;
	/** How hard it works: force asked of it / its own strength, x100 (up to 250). */
	UPROPERTY() uint8 Strain = 0;
	/** Its share of the weight, 0..255. */
	UPROPERTY() uint8 Share = 0;
	/** The planar force it puts into the load (world X, Y), -127..127 = -1..1 of its strength. */
	UPROPERTY() int8 PullX = 0;
	UPROPERTY() int8 PullY = 0;
	/** 1: dragged off its feet, 2: at its limit (that end dips). */
	UPROPERTY() uint8 Flags = 0;
};

USTRUCT()
struct FAntCarryNet
{
	GENERATED_BODY()

	UPROPERTY() TArray<FAntCarryGrip> Grips;
	/** The load's transform and velocity at server time Stamp (sent while it is carried). */
	UPROPERTY() FVector_NetQuantize10 Pos;
	UPROPERTY() FRotator Rot = FRotator::ZeroRotator;
	UPROPERTY() FVector_NetQuantize10 Vel;
	UPROPERTY() float Stamp = 0.f;
	/** 0..255: how much the carriers move as one (same way, same speed). */
	UPROPERTY() uint8 Sync = 255;
	/** They have caught the rhythm (strides locked, a bonus). */
	UPROPERTY() bool bRhythm = false;
	/** Somebody holds it: the server drives the body, clients interpolate Pos/Rot. */
	UPROPERTY() bool bActive = false;
	/** A short shout from one of the carriers: what, a counter (a new shout), which grip. */
	UPROPERTY() uint8 Bark = 0;
	UPROPERTY() uint8 BarkSeq = 0;
	UPROPERTY() uint8 BarkBy = 0;
};

/** What a carrier's body feels (AntBody reads it every frame, on every machine). */
struct FAntCarryFeel
{
	FVector Grip = FVector::ZeroVector;   // world: where its jaws hold
	FVector Pull = FVector::ZeroVector;   // world, planar: where the load pulls the ant (grip - jaws, units)
	float Strain = 0.f;                   // 0..2.5
	float Share = 0.f;                    // 0..1
	float Sync = 1.f;                     // 0..1
	bool bRhythm = false;
	bool bDragged = false;
	int32 Carriers = 0;
};

/** One carrier, for the API (GetCarriers). */
struct FAntCarrierInfo
{
	AActor* Who = nullptr;                // the ant, or the army of a worker
	int32 Worker = -1;                    // -1: an ant
	FVector Grip = FVector::ZeroVector;   // world
	FVector Desired = FVector::ZeroVector;// world velocity it wants to go at (units/s); zero: holds / follows its leader
	float Strength = 0.f;
	float Strain = 0.f;
	float Share = 0.f;
	bool bDragged = false;
};

namespace AntCarryEvent
{
	enum : uint8 { Delivered, Dropped, Broken, Snatched, Joined, Left };
}
/** Server: (load, AntCarryEvent, team). Delivered/Broken/Snatched come right before the load goes away or into the Human's hand. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAntCarryEventSignature, AKitchenLoot*, uint8, uint8);

namespace AntCarry
{
	// ------------------------------------------------------------------ API (server) for orders / group control
	/** An ant (Who = AAntCharacter) or worker #Worker of an army (Who = AAntArmy) takes hold. Grip: world point on (or near) the
	 *  surface; zero = picks the side nearest to it. False: no room / too far / not allowed. Ants go through GrabLoot. */
	ANTKITCHEN_API bool AddCarrier(AKitchenLoot* L, AActor* Who, int32 Worker = -1, const FVector& Grip = FVector::ZeroVector);
	ANTKITCHEN_API void RemoveCarrier(AKitchenLoot* L, AActor* Who, int32 Worker = -1);
	/** Up to N workers of the army (the first free indices) take hold all round it. Returns how many did. */
	ANTKITCHEN_API int32 AddArmy(AKitchenLoot* L, AAntArmy* Army, int32 N);
	ANTKITCHEN_API void RemoveArmy(AKitchenLoot* L, AAntArmy* Army);
	/** How many workers of this army hold it. */
	ANTKITCHEN_API int32 ArmyOn(const AKitchenLoot* L, const AAntArmy* Army);
	ANTKITCHEN_API void GetCarriers(const AKitchenLoot* L, TArray<FAntCarrierInfo>& Out);
	/** Where this carrier wants to go (world velocity, units/s; zero: stand / follow). Bots ordered this way walk it; workers do. */
	ANTKITCHEN_API void SetDesired(AKitchenLoot* L, AActor* Who, int32 Worker, const FVector& Velocity);
	/** Every worker of this army on the load heads for Goal at SpeedMul of their pace (squads, orders). */
	ANTKITCHEN_API void SetArmyGoal(AKitchenLoot* L, AAntArmy* Army, const FVector& Goal, float SpeedMul);
	ANTKITCHEN_API FVector GetDesired(const AKitchenLoot* L, const AActor* Who, int32 Worker = -1);
	ANTKITCHEN_API FAntCarryEventSignature& Events();
	ANTKITCHEN_API void Notify(AKitchenLoot* L, uint8 Kind, uint8 Team);

	// ------------------------------------------------------------------ hooks for the existing classes
	/** Server, AKitchenLoot::ServerCarryTick: the forces, shares, rhythm, yanks, workers, the replicated state. */
	ANTKITCHEN_API void ServerTick(AKitchenLoot* L, float Dt, const float* Extra, int32 WinTeam, const float* TeamStr);
	/** Server: the ant list changed (grab / release): movement replication, grips, the Dropped event. */
	ANTKITCHEN_API void ServerCarriersChanged(AKitchenLoot* L);
	/** Server: workers that hold it (they count like ants: home check, strength). */
	ANTKITCHEN_API bool HasWorkers(const AKitchenLoot* L);
	/** Server, AKitchenLoot::ReleaseAll: the workers let go too. */
	ANTKITCHEN_API void ReleaseAllWorkers(AKitchenLoot* L);
	ANTKITCHEN_API void AddWorkerStrength(const AKitchenLoot* L, float* TeamStr);
	/** Server: the commanders of the workers that carried it home get the credit too. */
	ANTKITCHEN_API void CreditAnts(const AKitchenLoot* L, uint8 Team, TArray<AAntCharacter*>& InOut);
	/** Server, TryGrab: room for one more ant (a worker steps aside for it if need be). */
	ANTKITCHEN_API bool MakeRoom(AKitchenLoot* L);
	/** Clients: the load follows the replicated state while it is carried. */
	ANTKITCHEN_API void ClientTick(AKitchenLoot* L, float Dt);
	/** Where a grip is taken: the point of the surface the jaws reach (server, TryGrab). */
	ANTKITCHEN_API FVector PickGrip(const AKitchenLoot* L, const FVector& From, const FVector& Closest);
	/** How many carriers fit round it. */
	ANTKITCHEN_API int32 MaxCarriers(const AKitchenLoot* L);

	ANTKITCHEN_API bool GripWorld(const AAntCharacter* Ant, FVector& Out);
	/** AAntCharacter::GetHoldPoint for a carrier: the jaws, turned towards its grip. False: the usual hold point. */
	ANTKITCHEN_API bool HoldPoint(const AAntCharacter* Ant, FVector& Out);
	/** The ant's movement (on the machine that moves it): the pull of the load, a velocity in the surface plane. */
	ANTKITCHEN_API FVector Tether(const AAntCharacter* Ant, const FVector& SurfaceN, float Dt);
	/** AAntCharacter::UpdateSpeed: the load factor for a carrier (OldLoad: the factor by the old rule). */
	ANTKITCHEN_API float SpeedMul(const AAntCharacter* Ant, float OldLoad);
	/** The body faces its grip. False: not carrying. */
	ANTKITCHEN_API bool Heading(const AAntCharacter* Ant, const FVector& N, FVector& InOutDesired);
	ANTKITCHEN_API bool Feel(const AAntCharacter* Ant, FAntCarryFeel& Out);
	/** Server: lost its grip to a yank a moment ago (bots take hold again instead of giving the load up). */
	ANTKITCHEN_API bool JustTumbled(const AAntCharacter* Ant);
	/** AntArmy drawing: worker i of the army stands at a grip. Pos on the floor, Dir towards the grip, Lift > 0 when dragged. */
	ANTKITCHEN_API bool WorkerPose(const AAntArmy* Army, int32 Worker, FVector& Pos, FVector& Dir, float& Lift);

	/** HUD: strain marks on loads near the camera, the carriers' shouts. */
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Canvas, APawn* Me);

	/** Dev (-AntDo "carry ..."). True if it was ours. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** Bots: an order or a dev script steers this carrier. False: the bot thinks for itself. */
	ANTKITCHEN_API bool DevSteer(const AAntCharacter* Ant, FVector& OutDir, float& OutScale);

	/** The cake: weight multiplier for this many players (+20 % each up to 4, +12 % each from 5 to 8). */
	ANTKITCHEN_API float CakePlayerMul(int32 Players);
}
