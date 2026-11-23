#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class AAntKitchenGameMode;
class AKitchenFixture;

/**
 * Level v3 (docs/level/LEVEL_V3.md, section 7 "Распорядок"): the residents' evening. Every resident has a list of routine spots
 * (where, which way, how long, at which times of day, what the body does there); the server lays them out along the raid clock
 * into a plan (the raid seed, the threat tier and the time of day decide the order, the lengths and the free gaps between them:
 * the higher the tier, the shorter the chores and the longer the rounds). Each resident's own head (HumanAIController for the
 * Dad, FolkCharacters for grandma and the kid, CritterBrain for the cat and Бадрик) asks "what am I doing now" when it has
 * nothing better to do: hunting an ant, a noise, an event, a scare always come first, and the routine is picked up again after.
 *
 * Reactions to mess: a break, a spill, a fall, the knitting undone, a drawing off the fridge is a mess (AddMess). The resident
 * whose job it is (grandma: spills and broken things, the yarn; the kid: his drawing; the Dad: everything when grandma is out,
 * and putting back what fell) stops, looks, says a short line (a subtitle), fetches the tool and cleans it up (Done: the
 * fixture tidies itself on every machine). Server only; what the bodies do is replicated through their actions (Act) and FNpcTask.
 */
namespace NpcRoutine
{
	enum EWho : uint8 { Dad, Gran, Kid, Cat, Dog, NumWho };
	/** What the body does at the spot. */
	enum EPose : uint8
	{
		PSit,      // people: sits (Arg: HumanAct::Sit how); pets: lies down to sleep
		PBusy,     // people: stands at it with the hands busy (Arg: HumanBusy)
		PGone,     // the Dad: out of the flat through the hall door with the rubbish
		PRest,     // pets: Arg 0 asleep, 1 nose down (eats, drinks), 2 sits (grooms)
		PFollow,   // the dog: after the kid
		PBeg,      // pets: by the table / the cook, begging
		PDoor,     // the dog: waits at the hall door for the Dad
		PFloor,    // the kid: his old floor play (the house rule of v2)
		PKnit,     // grandma: her armchair (knits; dozes with Arg 1)
	};
	enum EWhen : uint8 { WDay = 1, WEve = 2, WNight = 4, WAll = 7 };

	struct FStep
	{
		const TCHAR* Key;
		uint8 Who;
		const TCHAR* Ru;       // what the HUD / the log calls it
		float X, Y, Z;         // where the feet (or the paws) are; Z only for the high spots of the cat
		float Yaw;             // which way it faces there (people)
		float Secs;            // how long, at tier 1
		uint8 When;
		uint8 Pose;
		uint8 Arg;
		float SeatX, SeatY;    // people sitting: where the seat is (the feet stay at X, Y); 0, 0: in front of the feet
	};
	struct FSlot { int32 Step; float From; float To; };

	/** Level v3 and not -AntNoRoutines. */
	ANTKITCHEN_API bool Enabled();
	ANTKITCHEN_API TConstArrayView<FStep> Steps();
	ANTKITCHEN_API int32 FindStep(const TCHAR* Key);
	ANTKITCHEN_API const TCHAR* WhoName(uint8 Who);
	/** Seconds into the raid (the raid's clock on the server; + the dev warp). */
	ANTKITCHEN_API float Clock(const UWorld* W);
	/** Server: lay out the plan for the current seed, tier and time of day (the raid start). */
	ANTKITCHEN_API void Rebuild(const UWorld* W);
	/** Server: the step Who is due to do now (index into Steps()), or -1 (free: his rounds, her armchair...). */
	ANTKITCHEN_API int32 Current(const UWorld* W, uint8 Who, float* OutLeft = nullptr);
	ANTKITCHEN_API TConstArrayView<FSlot> Plan(uint8 Who);
	/** Server: a timeline line "ANTRT t=123.4 dad cook: arrived (…)". */
	ANTKITCHEN_API void Log(const UWorld* W, uint8 Who, const TCHAR* What, int32 Step, const FVector& At);
	/** Server: a short line said aloud: a subtitle for everybody ("Человек: «...»"). */
	ANTKITCHEN_API void Say(UWorld* W, uint8 Who, const FString& Line);
	/** Is the Dad (the server's Human) out with the rubbish right now? */
	ANTKITCHEN_API bool DadAway(const UWorld* W);
	/** Every machine: the hall door, 0 shut .. 1 open (a stand-in leaf turned about its hinge). */
	ANTKITCHEN_API void SetHallDoor(UWorld* W, float Open);

	// ---- mess
	enum EMess : uint8 { Spill, Shards, Fallen, Yarn, Drawing, Power, Pickup, NumMess };   // Power: the breaker tripped (the Dad walks to the box in the hall); Pickup: peas all over the floor (AntReact)
	struct FMess
	{
		int32 Id = 0;
		uint8 Kind = 0;
		FVector Loc = FVector::ZeroVector;
		TWeakObjectPtr<AActor> Src;
		float Time = 0.f;
		int8 By = -1;          // who has taken it on
		bool bDone = false;
		bool bSeen[NumWho] = { false, false, false, false, false };
		FString What;
	};
	ANTKITCHEN_API const TCHAR* MessName(uint8 Kind);
	/** Server: something to clean up / put back appeared at Loc. */
	ANTKITCHEN_API void AddMess(UWorld* W, uint8 Kind, const FVector& Loc, AActor* Src, const FString& What);
	/** Server: the nearest open mess Who would take on (its job), within Range of From; null if none. */
	ANTKITCHEN_API FMess* FindJob(const UWorld* W, uint8 Who, const FVector& From, float Range);
	/** Is this mess Who's job (grandma: spills, broken things, the yarn; the kid: his drawing; the Dad: what fell, and the rest when grandma is out)? */
	ANTKITCHEN_API bool IsJob(const UWorld* W, uint8 Who, const FMess& M);
	/** Server: any mess within Range Who has not looked at yet (he stops and looks at it, mutters). */
	ANTKITCHEN_API FMess* FindUnseen(const UWorld* W, uint8 Who, const FVector& From, float Range);
	ANTKITCHEN_API FMess* GetMess(int32 Id);
	ANTKITCHEN_API void Claim(int32 Id, uint8 Who);
	ANTKITCHEN_API void Release(int32 Id);
	/** Server: cleaned up / put back: the fixture (or the puddle) tidies itself on every machine. */
	ANTKITCHEN_API void Finish(UWorld* W, int32 Id, uint8 Who);
	/** A line for the resident who found Kind (what he mutters). */
	ANTKITCHEN_API FString Mutter(uint8 Who, uint8 Kind, int32 Salt);
	/** Where the tool for Kind is (broom, towel, dustpan, screwdriver drawer) and what it is called. */
	ANTKITCHEN_API FVector ToolFor(uint8 Who, uint8 Kind, FString& OutName);
	/** Server, a new raid: no mess, no plan, no warp. */
	ANTKITCHEN_API void ResetRaid();

	/** Dev (-AntDo): routine plan|info|warp S|skip who|off|on, react <fixture> [who], mess list. True if it was one of them. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** Dev: the resident should drop what he does and take up its routine again now (skip / warp). */
	ANTKITCHEN_API uint32 Nudge(uint8 Who);
	/** Dev: routines switched off for the whole run (old behaviour). */
	ANTKITCHEN_API bool Paused();
}

/** What the hands do while a figure stands at a chore (HumanAct::Busy, Arg). */
namespace HumanBusy
{
	enum EId : uint8
	{
		Stir,      // at the stove: the right hand stirs the pot with a ladle, the left on the worktop
		Wash,      // at the sink: both hands in the basin, scrubbing
		Window,    // at the window: hands on the hips, looking out
		Water,     // watering a plant: the right arm out with the can, tipped
		Rummage,   // bent over a box or the bin: both hands down in it, one after the other
		Tug,       // the kid at the balloons: one arm up, tugging a ribbon
		Bag,       // the rubbish bag in the right hand, down at his side (he walks with it)
		Fridge,    // the fridge open: the left hand on the door, looking in
		Kettle,    // waiting by the kettle: arms folded
		Look,      // stopped, looking at the mess at Target, hands on the hips
		Wind,      // winding yarn back onto the ball: the hands circle each other in front
		Fix,       // up at the wall: both hands at Target (a screwdriver, putting a thing back)
		Count
	};
}
