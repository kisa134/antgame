#pragma once

#include "CoreMinimal.h"
#include "AntNest.generated.h"

/**
 * The colony between raids: what is in the stores, how the Queen feels, what has been built. It belongs to the host and is
 * saved on the host's machine (Saved/Nest/colony.sav, with a backup of the save before it). Everybody else sees it replicated
 * inside AAntRaid. Rules and numbers live here; the screen is AntHUDNest.cpp.
 */

/** Branches of the nest. Every tier unlocks something a player can feel in the next raid. */
namespace AntUp
{
	enum EBranch : uint8 { Brood, Soldiers, Carriers, Engineers, Scouts, Tools, Count };
	constexpr int32 MaxTier = 3;
	struct FDef { const TCHAR* Name; const TCHAR* Text; int32 Food; int32 Jelly; int32 Mood; };
	ANTKITCHEN_API const TCHAR* BranchName(uint8 Branch);
	ANTKITCHEN_API const FDef& Def(uint8 Branch, int32 Tier);   // Tier 1..MaxTier
}

/** Consumables a player takes into a raid (two pockets, two charges each). */
namespace AntTool
{
	enum EId : uint8 { None, Bait, Alarm, Decoy, Noise, Count };
	constexpr int32 Pockets = 2;
	constexpr int32 MaxCharges = 2;
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
	ANTKITCHEN_API const TCHAR* Text(uint8 Id);
	ANTKITCHEN_API int32 Cost(uint8 Id);
	/** The Tools tier that unlocks it. */
	ANTKITCHEN_API int32 NeedTier(uint8 Id);
}

/** One-raid boosts the host buys for the whole team (they are the old run cards that were about the shift itself). */
namespace AntMod
{
	enum EId : uint8 { Phone, Padding, Greedy, Overtime, Count };
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
	ANTKITCHEN_API const TCHAR* Text(uint8 Id);
	ANTKITCHEN_API int32 Cost(uint8 Id);
	ANTKITCHEN_API uint8 Card(uint8 Id);
}

/** Threat tiers of the apartment, chosen in the nest. */
namespace AntTier
{
	constexpr int32 Max = 4;
	struct FDef
	{
		const TCHAR* Name;
		const TCHAR* Text;
		float Length;        // seconds of raid
		float Quiet;         // the Human is on his phone this long
		float Final;         // the last seconds when everyone is awake
		float LootMul;       // value of everything brought home
		int32 CakeValue;     // whole
		int32 CakeWeight;
		int32 CakeJelly;
		int32 Ask;           // side loot the Queen expects on top of the cake (one player; +40% for each more)
		int32 Npc[4];        // boldness of the dog, cat, kid, grandma in the first half
		int32 RageFloor;
		int32 DoorBias;      // doors count this many classes heavier
		int32 Specials;      // special finds hidden per raid
		int32 Events;
		// the rival clan
		int32 Scouts, Haulers, Escorts, Ambushers;
		float ClanFirst;     // seconds before their scouts leave the hill
		float ClanRespawn;
		float ClanHealth;    // multiplier
		float Window;        // seconds to take the cake back from their hill
	};
	ANTKITCHEN_API const FDef& Def(int32 Tier);
	/** How many cakes brought home at Tier unlock the next one. */
	ANTKITCHEN_API int32 CakesToUnlock(int32 Tier);
}

USTRUCT()
struct FAntNestState
{
	GENERATED_BODY()

	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 Food = 0;
	/** The rare kind: royal jelly, from the cake, whole trophies, special finds and contracts. Pays for the big unlocks. */
	UPROPERTY() int32 Jelly = 0;
	/** Shared lives: a larva is spent each time a player dies in a raid. Raised from food between raids. */
	UPROPERTY() int32 Larvae = 3;
	/** Workers the players split into squads. What is taken into a raid and does not come back is gone. */
	UPROPERTY() int32 Workers = 12;
	/** The Queen, 0..100. */
	UPROPERTY() int32 Mood = 50;
	/** Tier of every branch (AntUp::EBranch), 0..3. */
	UPROPERTY() TArray<uint8> Up;
	UPROPERTY() int32 MaxTier = 1;
	UPROPERTY() int32 Tier = 1;
	UPROPERTY() int32 Raids = 0;
	UPROPERTY() int32 CakesHome = 0;
	/** Cakes brought home at the highest unlocked tier (towards the next one). */
	UPROPERTY() int32 CakesAtMax = 0;
	UPROPERTY() int32 Wipes = 0;
	UPROPERTY() int32 BestHaul = 0;

	int32 Tier_(uint8 Branch) const { return Up.IsValidIndex(Branch) ? Up[Branch] : 0; }
};

namespace AntNest
{
	constexpr int32 LarvaCost = 60;
	constexpr int32 WorkerPack = 4;
	constexpr int32 WorkerPackCost = 20;
	constexpr int32 MaxLarvae = 12;
	constexpr int32 MaxWorkers = 120;

	ANTKITCHEN_API void Fresh(FAntNestState& S);
	/** Squad size one player may take. */
	ANTKITCHEN_API int32 SquadCap(const FAntNestState& S);
	/** Pull / carry strength of one squad worker. */
	ANTKITCHEN_API float WorkerStrength(const FAntNestState& S);
	ANTKITCHEN_API const TCHAR* MoodName(int32 Mood);
	/** 0 furious, 1 calm, 2 pleased, 3 delighted. */
	ANTKITCHEN_API int32 MoodBand(int32 Mood);
	/** Can the next tier of the branch be bought? Why: what is missing, for the screen. */
	ANTKITCHEN_API bool CanBuy(const FAntNestState& S, uint8 Branch, FString* Why = nullptr);
	ANTKITCHEN_API bool Buy(FAntNestState& S, uint8 Branch);
	/** The old run cards the built branches stand for (AntCard ids). */
	ANTKITCHEN_API void Cards(const FAntNestState& S, TArray<uint8>& Out);
	ANTKITCHEN_API bool ToolUnlocked(const FAntNestState& S, uint8 Tool);

	// ---- the save (host only)
	ANTKITCHEN_API FString SavePath();
	ANTKITCHEN_API FString BackupPath();
	/** True if a valid save (or its backup) was read. Otherwise S is a fresh colony. OutNote says what happened. */
	ANTKITCHEN_API bool Load(FAntNestState& S, FString* OutNote = nullptr);
	ANTKITCHEN_API bool Save(const FAntNestState& S);
	/** Moves the save and its backup aside (colony.old) so that the next Load starts a fresh colony. */
	ANTKITCHEN_API void Reset();
	/** One line for the main menu, or empty when there is no colony yet. */
	ANTKITCHEN_API FString Summary();
}
