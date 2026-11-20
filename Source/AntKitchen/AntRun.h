#pragma once

#include "CoreMinimal.h"

class UWorld;
class UObject;
class AActor;
class AController;
class AAntCharacter;
class AKitchenInteractable;

/**
 * The heist run on top of the shift: upgrade cards voted between shifts, the Queen's orders, greed, awards,
 * one kitchen event per shift, noise the Human goes to check, containers (what lies behind which door).
 * Rules run on the server (AAntKitchenGameMode, AntRun.cpp); what players need to see is replicated on AAntGameState.
 */

/** Upgrades for the rest of the run. Picked by vote after a won shift, gone when the run is lost. */
namespace AntCard
{
	enum EId : uint8
	{
		Jaws,       // stronger carrying
		Legs,       // faster when unladen
		Brood,      // +2 larvae every shift
		Quiet,      // the Human notices ants later
		Acid,       // spits cost less
		Lever,      // doors count one class lighter
		Trail,      // trail workers haul more
		Chitin,     // the first bite of a life does not tear a leg off
		Phone,      // longer quiet phase
		Padding,    // trophies survive harder falls
		Spring,     // shorter dash cooldown
		Nurse,      // stronger Nurse aura
		Greedy,     // greed grows faster
		Overtime,   // longer shift
		Count
	};
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
	ANTKITCHEN_API const TCHAR* Text(uint8 Id);
	/** Does the colony own this card? Works on every machine (reads the replicated game state). */
	ANTKITCHEN_API bool Has(const UObject* WorldContext, uint8 Id);
}

/** The Queen's bonus objectives: three per shift. */
namespace AntOrder
{
	enum EId : uint8
	{
		TrophyWhole,    // deliver a trophy whole
		NoDeaths,       // nobody dies this shift (checked at the end)
		Freezer,        // bring something out of the freezer
		Living3,        // three things from the living room
		KnockOut,       // knock the Human out or stun him
		Early,          // finish with two minutes to spare (checked at the end)
		Tear,           // tear an enemy in two
		NoBreak,        // break no trophy (checked at the end)
		Doors5,         // open five doors
		Fridge,         // bring something out of the fridge
		Body,           // bring a dead teammate home
		DropOnHuman,    // drop something on the Human
		Distract,       // send the Human after a noise twice
		Overfill,       // 150% of the quota
		Count
	};
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
	/** Decided only when the shift is won. */
	ANTKITCHEN_API bool AtEnd(uint8 Id);
}

/** Things that make noise. The AI Human walks to the loudest recent one. */
namespace AntNoise
{
	enum EKind : uint8 { None, Faucet, Toaster, Microwave, TV, Crash, Door, Trap, Bark, Scream, Count };
	ANTKITCHEN_API const TCHAR* Name(uint8 Kind);
}

struct FAntNoise
{
	FVector Loc = FVector::ZeroVector;
	float Loud = 0.f;
	float Time = -100.f;
	uint8 Kind = 0;
	TWeakObjectPtr<AActor> Source;
};

/** One random event per shift. */
namespace AntEvent
{
	enum EId : uint8 { None, Blackout, Cat, Vacuum, Flood, Pizza, FridgeOpen, Zoomies, Count };
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
	ANTKITCHEN_API const TCHAR* Text(uint8 Id);
	ANTKITCHEN_API float Length(uint8 Id);
}

/** What the Human is up to, for the HUD. */
namespace AntHumanGoal
{
	enum EKind : uint8 { Idle, Hunt, Noise, Shut, Look, Stunned };
}

/** Ping words (middle mouse button). */
namespace AntPing
{
	enum EKind : uint8 { Here, Trophy, Human, Door, Count };
	ANTKITCHEN_API const TCHAR* Name(uint8 Kind);
}

/** Door weight classes: how much pulling strength it takes to open one. */
namespace AntDoor
{
	enum EClass { None = 0, Light = 1, Medium = 2, Heavy = 3 };
	/** Light: any ant. Medium: about two workers. Heavy: about three. */
	inline float Need(int32 Class) { return Class >= Heavy ? 40.f : Class == Medium ? 24.f : Class == Light ? 6.f : 0.f; }
	/** How many more workers it would take. */
	inline int32 Missing(float Need, float Have) { return Need > Have ? FMath::Max(1, FMath::CeilToInt((Need - Have) / 14.f)) : 0; }
}

/** Closed spaces food can lie in: the fridge, the freezer, the oven, cabinets, drawers... and the doors that hide them. */
namespace KitchenContainers
{
	struct FDef
	{
		const TCHAR* Where;      // "в холодильнике"
		FVector Min, Max;        // the space inside, world
		int32 DoorKind;          // EKitchenDoorKind of its AKitchenDoor, or -1
		int32 Movers[2];         // AKitchenMover part indices, or -1
		bool bTrap;              // shut inside = in trouble (cold, heat): announced to the team
	};
	enum EId { Fridge, Freezer, Oven, Microwave, Dishwasher, TopDrawer, Drawer2, CabA, CabB, CabC, Wall0, Wall1, Wall2, Wall3, WallE, TVStand, Count };
	ANTKITCHEN_API const FDef& Get(int32 Index);
	/** The container whose inside holds P, or -1. */
	ANTKITCHEN_API int32 At(const FVector& P);
	/** One of its doors is open (or it has no door in this kitchen). */
	ANTKITCHEN_API bool IsOpen(const UWorld* World, int32 Index);
	/** P lies inside a container that is shut: its index. Otherwise -1. */
	ANTKITCHEN_API int32 SealedAt(const UWorld* World, const FVector& P);
	/** Server: open / shut every door of it. */
	ANTKITCHEN_API void SetOpen(UWorld* World, int32 Index, bool bOpen);
}

/** Server-only bookkeeping of the run, owned by the game mode. */
struct FAntRunState
{
	TArray<FAntNoise> Noises;
	/** Sources and kinds the Human has just checked: not fooled again until that time. */
	TMap<TWeakObjectPtr<AActor>, float> CheckedSource;
	float CheckedKind[AntNoise::Count] = {};
	TMap<TWeakObjectPtr<AController>, int32> Votes;
	TMap<TWeakObjectPtr<AAntCharacter>, int32> Trapped;   // ant -> container it is shut in
	/** Players with no larva left to hatch from: out until the shift ends, or until somebody brings their body home. */
	TSet<TWeakObjectPtr<AController>> OutPlayers;
	int32 DoorsOpened = 0;
	int32 LivingDelivered = 0;
	int32 Distractions = 0;
	int32 DeathsThisShift = 0;
	float GreedClock = 0.f;
	float EventAt = -1.f;        // seconds into the shift
	uint8 EventPlanned = 0;
	bool bEventDone = false;
	float InterludeStep = 0.f;   // world time of the next interlude step
	bool bDevWon = false;
	TArray<TWeakObjectPtr<AActor>> EventActors;
	bool bLightsBefore = true;
	float NextDropTest = -1.f;
};