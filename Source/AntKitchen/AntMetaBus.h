#pragma once

#include "CoreMinimal.h"

/**
 * The meta bus: the one place where the shared colony (AntColony, "Муравейник", host) and the personal layer (AntProfile: ranks,
 * medals, perks, cosmetics; AntRoster: squad veterans and the fallen) meet. Header-only, no UObjects, no includes of either side,
 * so each side compiles without the other.
 *
 *   raid ends (server) ──► AntMetaBus::OnRaidSummary().Broadcast(FAntRaidSummary)   — the colony fills it, anybody listens
 *   colony screen      ──► AntMetaBus::GetFallenVeterans()   — the Wall of Memory (Стена памяти)
 *   post-raid show     ──► AntMetaBus::GetPlayerHighlights() — rank-ups, medals, feats of each player
 *
 * The queries call whatever provider the personal layer has set (SetFallenProvider / SetHighlightsProvider); with none set they
 * return empty and the colony falls back to what it knows itself. Queries run on every machine (the colony screen is drawn on
 * clients too): a provider returns what this machine knows (replicated profile data), never blocks, and is cheap (called once
 * when the screen opens / the show starts, not per frame).
 */

/** Food kinds of the colony stores (loot names map to them in AntColony::KindOf). */
namespace AntFoodKind
{
	enum EKind : uint8 { Sugar, Protein, Fat, Crumbs, Count };
}

/** One player's raid. */
struct FAntRaidPlayerLine
{
	/** Display name, and a stable id (the unique net id as a string; the name when there is none). */
	FString Name;
	FString Id;
	/** Value delivered home by this player, by kind (AntFoodKind), after the threat multiplier. Shared carries are split evenly. */
	int32 Food[AntFoodKind::Count] = { 0, 0, 0, 0 };
	int32 Deaths = 0;
	/** Bodies of team-mates this player carried home. */
	int32 Rescues = 0;
	/** Feats, as ids: "cake_home" (carried the cake in), "trophy_home", "special_home", "contract", "last_out", "survived". */
	TArray<FName> Feats;
	bool bCameHome = false;
	int32 FoodTotal() const { int32 T = 0; for (int32 k = 0; k < AntFoodKind::Count; ++k) { T += Food[k]; } return T; }
};

/** What a raid brought, for everyone. Filled by the colony on the server when the raid ends (after the stores are counted). */
struct FAntRaidSummary
{
	int32 RaidNo = 0;
	uint8 Tier = 1;
	/** AntRaidEnd: 1 left with the loot, 2 time ran out, 3 wiped. */
	uint8 End = 0;
	bool bCakeHome = false;
	/** Food kept by the stores, royal jelly, larvae left. */
	int32 Kept = 0;
	int32 Jelly = 0;
	int32 Larvae = 0;
	/** Team totals by kind (AntFoodKind). */
	int32 Food[AntFoodKind::Count] = { 0, 0, 0, 0 };
	/** Trophies hung in the colony this raid (AntColony trophy ids, e.g. "slipper", "coin"). */
	TArray<FName> Trophies;
	TArray<FAntRaidPlayerLine> Players;
};

/** A fallen veteran for the Wall of Memory. */
struct FAntFallenVeteran
{
	FString Name;
	/** Rank as it should be written on the wall ("ефрейтор"). Empty: no rank. */
	FString Rank;
	int32 Raid = 0;
	/** How he died, short ("раздавлен тапком"). */
	FString Cause;
};

/** A line of the post-raid show about one player. */
struct FAntPlayerHighlight
{
	FString Player;
	/** Short title: "НОВОЕ ЗВАНИЕ", "МЕДАЛЬ", "ПОДВИГ". */
	FString Title;
	/** What happened: "Жвалов теперь ефрейтор", "«За сладкое» — донёс торт". */
	FString Line;
	/** Optional icon under /Game/UI (e.g. "Icons/T_UI_Icon_Medal"); empty: the colony picks one by Title. */
	FString Icon;
	/** Optional rank change (0 = none). */
	int32 RankBefore = 0;
	int32 RankAfter = 0;
};

namespace AntMetaBus
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRaidSummary, const FAntRaidSummary&);
	using FFallenProvider = TFunction<void(TArray<FAntFallenVeteran>& Out)>;
	using FHighlightsProvider = TFunction<void(TArray<FAntPlayerHighlight>& Out)>;

	/** Server: fired once per raid, when its results are counted (before the results screen). */
	inline FOnRaidSummary& OnRaidSummary() { static FOnRaidSummary D; return D; }
	/** The last summary this process saw (server; empty RaidNo 0 before the first raid). */
	inline FAntRaidSummary& LastSummary() { static FAntRaidSummary S; return S; }

	inline FFallenProvider& FallenProvider() { static FFallenProvider P; return P; }
	inline FHighlightsProvider& HighlightsProvider() { static FHighlightsProvider P; return P; }
	inline void SetFallenProvider(FFallenProvider P) { FallenProvider() = MoveTemp(P); }
	inline void SetHighlightsProvider(FHighlightsProvider P) { HighlightsProvider() = MoveTemp(P); }

	/** The fallen, newest first. Empty when no provider is set. */
	inline TArray<FAntFallenVeteran> GetFallenVeterans()
	{
		TArray<FAntFallenVeteran> Out;
		if (FallenProvider()) { FallenProvider()(Out); }
		return Out;
	}
	/** Highlights of the raid that just ended (rank-ups, medals, feats). Empty when no provider is set. */
	inline TArray<FAntPlayerHighlight> GetPlayerHighlights()
	{
		TArray<FAntPlayerHighlight> Out;
		if (HighlightsProvider()) { HighlightsProvider()(Out); }
		return Out;
	}
}
