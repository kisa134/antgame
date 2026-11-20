#pragma once

#include "CoreMinimal.h"

class AAntCharacter;
class AAntArmy;
class AAntRaid;
class AController;
class AKitchenInteractable;
class UWorld;

/**
 * A player's squad in a raid: the workers he takes out of the nest (a small AAntArmy, drawn as instances like any army).
 * How many and how good comes from the nest; taking them is his stake: what does not come home is gone.
 * Orders: follow, attack what I aim at, free (forage), haul (pick up what I point at: with me, or home on its own along the
 * route graph) and hold (guard a spot, keep a door open). The moving, carrying and fighting of a squad is in AntSquad.cpp as
 * methods of AAntArmy (bSquad).
 */
namespace AntSquad
{
	enum EOrder : uint8 { Follow, Attack, Free, Haul, Hold, Count };
	ANTKITCHEN_API const TCHAR* OrderName(uint8 Order);

	/** The squad this ant commands, or null. Works on every machine. */
	ANTKITCHEN_API AAntArmy* Find(const AAntCharacter* Commander);
	/** Server: the old commander keys (F, R, Z, X, C) in a raid. True: handled here. */
	ANTKITCHEN_API bool HandleCommand(AAntCharacter* Ant, uint8 Cmd, const FVector& Point);
	/** Server: an order to the ant's squad. Aim: the point under the crosshair. */
	ANTKITCHEN_API void Order(AAntCharacter* Ant, uint8 Order, const FVector& Aim);
	/** Server: the raid starts; every player's squad leaves the nest with him. Returns how many workers went out. */
	ANTKITCHEN_API int32 SpawnAll(AAntRaid* Raid);
	/** Server: its commander died. Half scatter at once; the rest wait by the body. */
	ANTKITCHEN_API void OnCommanderDied(AAntCharacter* Ant);
	/** Server: the body was brought home in time: what is left of the squad is his again. */
	ANTKITCHEN_API void OnBodyHome(AController* Owner);
	/** Server: the raid is over. How many workers come home (the rest are lost). bWiped: none. */
	ANTKITCHEN_API int32 CountBack(AAntRaid* Raid, bool bWiped, bool bOnlyHome);
	/** Server: the Human slams a door a squad is holding open: it costs the squad two workers. */
	ANTKITCHEN_API void OnDoorSlammed(AKitchenInteractable* Door);
	/** Server: the squad comes to its commander at once (the alarm scent). */
	ANTKITCHEN_API bool Recall(AAntCharacter* Ant);
}
