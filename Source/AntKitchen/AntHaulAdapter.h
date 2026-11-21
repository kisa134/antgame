#pragma once

#include "CoreMinimal.h"

class AAntCharacter;
class AKitchenLoot;

/**
 * The only place the squad's orders ("тащи это") touch carrying. Today it speaks to the existing carry code of AKitchenLoot
 * (TryGrab / Release / Carriers, at most 8 carriers, every carrier walks for itself). The carry rewrite (branch code/ANT-carry:
 * grip points, shared load, AddCarrier / RemoveCarrier / GetCarriers / a desired direction / a delivered event) replaces the
 * bodies of these functions after both branches are merged; nothing else of the swarm code calls the carry code directly.
 * See the merge notes at the top of AntHaulAdapter.cpp.
 */
namespace AntHaul
{
	/** Server: the ant takes hold of the load. False: it could not (too many carriers, in a hand, a door...). */
	ANTKITCHEN_API bool AddCarrier(AKitchenLoot* Load, AAntCharacter* Ant);
	/** Server: lets go. */
	ANTKITCHEN_API void RemoveCarrier(AKitchenLoot* Load, AAntCharacter* Ant);
	ANTKITCHEN_API void GetCarriers(const AKitchenLoot* Load, TArray<AAntCharacter*>& Out);
	ANTKITCHEN_API int32 NumCarriers(const AKitchenLoot* Load);
	/** How many ants can hold it at once. */
	ANTKITCHEN_API int32 MaxCarriers(const AKitchenLoot* Load);
	ANTKITCHEN_API bool IsCarrying(const AAntCharacter* Ant, const AKitchenLoot* Load);
	/** Strength of the side that holds it / its weight: >= 1 lifted, 0.5..1 dragged, less: stuck. */
	ANTKITCHEN_API float Ratio(const AKitchenLoot* Load);
	/** Server: where the carrying group wants the load to go (flat unit vector; zero: stand). The old carry has no such input
	 *  (the load follows its carriers' jaws), so today this only remembers it; the leader of the group walks the way. */
	ANTKITCHEN_API void SetDesiredDirection(AKitchenLoot* Load, const FVector& Dir);
	/** True once the carry code moves the load by the desired direction by itself (the new API): the carriers then only keep their grip. */
	ANTKITCHEN_API bool LoadSteersItself();
	/** May a group be ordered to haul this (food on the floor or a shelf: not gear, not in a hand, not a body)? */
	ANTKITCHEN_API bool CanHaul(const AKitchenLoot* Load);
	/** Server: the load reached a nest entrance (called from the game mode's delivery; the new API fires its own event). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDelivered, AKitchenLoot*, uint8);
	ANTKITCHEN_API FOnDelivered& OnDelivered();
	ANTKITCHEN_API void NotifyDelivered(AKitchenLoot* Load, uint8 Team);
}
