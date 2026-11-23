#pragma once

#include "CoreMinimal.h"
#include "AntMetaBus.h"

struct FAntNestState;
struct FAntRaidResult;

/** A gift into the colony from outside a raid (the tutorial's starter gift, events, the admin). Stable API: AntColony::GiveGift. */
struct FAntColonyGift
{
	/** Food by kind (AntFoodKind: Sugar, Protein, Fat, Crumbs). */
	int32 Food[AntFoodKind::Count] = { 0, 0, 0, 0 };
	/** Royal jelly (молочко). */
	int32 Jelly = 0;
	int32 Larvae = 0;
	int32 Workers = 0;
	/** A line for «Летопись колонии» ("Подарок Королевы новобранцам"); empty: none. */
	FString Chronicle;
	/** A line every player sees on screen (the broadcast); empty: none. */
	FString Announce;
};

/**
 * «Муравейник»: the colony as a place (docs/COLONY.md). The rules of the nest (AntNest.h) stay as they were; this adds the shape:
 * chambers along tunnels in the wall crack, tunnels the host digs (which way the colony grows), stores split by kind of food,
 * trophies on the walls, the chronicle, the fallen. One colony per co-op group: the host's, saved with the nest (colony.sav v2),
 * replicated inside AAntRaid::Nest. The screen is AntColonyView.cpp.
 */
namespace AntColony
{
	/** Chambers. Six of them are the old branches (buying a tier = building the chamber up), four are new, the throne grows by itself. */
	enum EChamber : uint8 { Throne, Stores, Nursery, Barracks, Arsenal, Fungus, Trophies, Workshop, Radio, Parade, Memorial, Count };
	/** The new chambers' tiers live in FAntNestState::Own (by this index). */
	enum EOwn : uint8 { OwnFungus, OwnTrophies, OwnRadio, OwnMemorial, OwnCount };
	/** Which way the colony digs from the hub: to the water pipe, to the wiring, to the gap under the floor. */
	enum EDig : uint8 { Pipe, Wiring, Gap, DigCount };
	constexpr int32 MaxTier = 3;
	constexpr int32 MaxChronicle = 14;
	constexpr int32 MaxFallen = 24;
	constexpr int32 SaveVersion = 2;

	struct FChamberDef
	{
		const TCHAR* Name;
		/** Short id for textures, logs and dev commands ("throne", "stores"...). */
		const TCHAR* Id;
		const TCHAR* About;
		int8 Branch;     // AntUp::EBranch, or -1
		int8 Own;        // EOwn, or -1
		int8 Dig;        // EDig, or -1 (the hub, dug from the start)
		/** Centre and size on the 1920x1080 cutaway. */
		float X, Y, W, H;
		/** Which shell picture (0: the round one with the seat, 1: the wide one with hollows), mirrored. */
		uint8 Shell;
		bool bFlip;
		const TCHAR* Icon;
	};
	ANTKITCHEN_API const FChamberDef& Def(uint8 Chamber);

	struct FDigDef { const TCHAR* Name; const TCHAR* Toward; const TCHAR* Text; };
	ANTKITCHEN_API const FDigDef& Dig(uint8 D);

	/** Tier 0..3 of a chamber (0: not built; the throne is always at least 1). */
	ANTKITCHEN_API int32 TierOf(const FAntNestState& S, uint8 Chamber);
	/** Name and effect of a tier (1..3). */
	ANTKITCHEN_API FString TierName(uint8 Chamber, int32 Tier);
	ANTKITCHEN_API FString TierText(uint8 Chamber, int32 Tier);
	/** Cost of the next tier. */
	ANTKITCHEN_API void Cost(const FAntNestState& S, uint8 Chamber, int32& Food, int32& Jelly, int32& Mood);
	ANTKITCHEN_API bool IsDug(const FAntNestState& S, uint8 D);
	/** The chamber's tunnel is dug (the hub always is). */
	ANTKITCHEN_API bool IsOpen(const FAntNestState& S, uint8 Chamber);
	ANTKITCHEN_API bool CanUpgrade(const FAntNestState& S, uint8 Chamber, FString* Why = nullptr);
	ANTKITCHEN_API bool Upgrade(FAntNestState& S, uint8 Chamber);
	ANTKITCHEN_API void DigCost(const FAntNestState& S, int32& Food, int32& Jelly);
	ANTKITCHEN_API bool CanDig(const FAntNestState& S, uint8 D, FString* Why = nullptr);
	ANTKITCHEN_API bool DoDig(FAntNestState& S, uint8 D);
	/** Everything built: tiers of all chambers + tunnels dug. 0..33. */
	ANTKITCHEN_API int32 Level(const FAntNestState& S);
	ANTKITCHEN_API int32 MaxLevel();
	/** A branch (AntUp) can be built only once its chamber's tunnel is dug. */
	ANTKITCHEN_API bool BranchOpen(const FAntNestState& S, uint8 Branch);
	ANTKITCHEN_API uint8 ChamberOfBranch(uint8 Branch);

	// ---- the stores by kind of food
	ANTKITCHEN_API const TCHAR* KindName(uint8 Kind);
	ANTKITCHEN_API uint8 KindOf(const FString& LootName);
	/** Keeps Stock in step with Food (what was spent comes out of every kind alike; food of no known kind is crumbs). */
	ANTKITCHEN_API void Sync(FAntNestState& S);
	ANTKITCHEN_API void AddFood(FAntNestState& S, uint8 Kind, int32 Amount);
	/** What a kind holds, for the screen (Stock scaled to Food, so it is right on clients between syncs too). */
	ANTKITCHEN_API int32 Held(const FAntNestState& S, uint8 Kind);

	// ---- trophies
	struct FTrophyDef { const TCHAR* Id; const TCHAR* Name; const TCHAR* How; const TCHAR* Tex; };
	ANTKITCHEN_API int32 TrophyCount();
	ANTKITCHEN_API const FTrophyDef& Trophy(int32 Index);
	ANTKITCHEN_API bool HasTrophy(const FAntNestState& S, int32 Index);
	/** Adds it if new. True when it was new. */
	ANTKITCHEN_API bool GiveTrophy(FAntNestState& S, int32 Index);
	/** How many trophies the trophy hall can hang (the rest wait in a crate). */
	ANTKITCHEN_API int32 TrophySlots(const FAntNestState& S);
	/** Multiplier of food value from the trophies on the walls (+2% each). */
	ANTKITCHEN_API float LootBonus(const FAntNestState& S);

	// ---- what the chambers do between raids
	ANTKITCHEN_API int32 FungusFood(const FAntNestState& S);
	ANTKITCHEN_API int32 FungusJelly(const FAntNestState& S);
	/** Extra mood after every raid (trophy hall III, radio III). */
	ANTKITCHEN_API int32 MoodPerRaid(const FAntNestState& S);
	/** Mood each death costs (memorial halves it). */
	ANTKITCHEN_API int32 DeathMood(const FAntNestState& S, int32 Deaths);
	/** Share of the banked food kept when the team is wiped (1/2; memorial II: 2/3). */
	ANTKITCHEN_API float WipeKeep(const FAntNestState& S);
	/** Food a finished order of the Queen pays, given the base (radio I doubles it). */
	ANTKITCHEN_API int32 OrderFood(const FAntNestState& S, int32 Base);

	ANTKITCHEN_API void Chronicle(FAntNestState& S, const FString& Line);
	ANTKITCHEN_API void AddFallen(FAntNestState& S, const FString& Name, int32 Raid, const FString& Cause);
	/** The fallen for the wall: from the roster (AntMetaBus) when it has any, else the colony's own list. Newest first. */
	ANTKITCHEN_API TArray<FAntFallenVeteran> Fallen(const FAntNestState& S);

	/** Queen's face: 0 furious, 1 grumpy, 2 happy, 3 delighted, 4 sleeping (the screen decides on sleep). */
	ANTKITCHEN_API int32 QueenFace(int32 Mood);
	/** Throne room tier from the colony level (1..3). */
	ANTKITCHEN_API int32 ThroneTier(const FAntNestState& S);

	// ---- the save (v2 keys on top of AntNest's v1 lines)
	ANTKITCHEN_API FString EncodeKeys(const FAntNestState& S);
	ANTKITCHEN_API void DecodeKeys(const TMap<FString, FString>& KV, int32 Version, FAntNestState& S);
	/** A v1 colony gets its shape: stores split by kind, tunnels dug where its branches already stand, a first line of the chronicle. */
	ANTKITCHEN_API void Migrate(FAntNestState& S, int32 FromVersion);

	/** Dev / admin: a raid "played" on paper (income of a tier-T raid, the Queen, trophies), then the cheapest things built. Fills R for the post-raid show. */
	ANTKITCHEN_API void SimulateRaid(FAntNestState& S, FAntRaidResult& R, int32 Seed, bool bBuild);
	ANTKITCHEN_API FString Info(const FAntNestState& S);

	// ---- for other systems (tutorial, events): world-level calls, server only
	/** Server: adds the gift to the host's colony and saves it (replicates to every machine). False on a client or with no raid. */
	ANTKITCHEN_API bool GiveGift(const UObject* WorldContext, const FAntColonyGift& Gift);
	/** Server: every machine's colony screen plays the post-raid show of the last raid again (nothing to show before raid 1). */
	ANTKITCHEN_API bool ReplayShow(const UObject* WorldContext);
}
