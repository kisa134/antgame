// Cosmetics of the fighter (docs/FIGHTER.md, "Облик"): the helmets, goggles, scarves, matchstick spears, packs, war paint,
// shell colours and patterns, the trail colour, the rank insignia and the emotes. Real cosmetics: what a player picked travels in
// his replicated loadout (AntFighter.h) and every machine dresses his ant from it (UAntDress, one per ant). The trailer's "cap kit"
// roles (code/ANT-capture, code/ANT-trailer3) are kept as Apply(Ant, Role) for the capture scripts.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AntCosmetics.generated.h"

class AAntCharacter;
struct FAntLoadout;

/** Dresses its ant whenever the loadout (or a veteran's rank) changes; plays the emotes. Every machine. */
UCLASS()
class ANTKITCHEN_API UAntDress : public UActorComponent
{
	GENERATED_BODY()

public:
	UAntDress();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** What was dressed last (loadout stamp, cosmetics, rank, insignia, rig): a change re-dresses. */
	uint32 Sig = 0;
	float NextCheck = 0.f;
	int32 Parts = 0;
	/** The emote being played (server: its steps; every machine: the crumbs). */
	float EmoteSeen = -100.f;
	int32 EmoteStep = -1;
	TArray<TWeakObjectPtr<class UStaticMeshComponent>> EmoteBits;
};

namespace AntCosmetics
{
	/** Adds the dress component (AAntCharacter::BeginPlay). */
	ANTKITCHEN_API void Attach(AAntCharacter* Ant);
	/** Dresses the ant from a loadout (null: only the veteran's insignia). Returns the number of parts. */
	ANTKITCHEN_API int32 Dress(AAntCharacter* Ant, const FAntLoadout* L);
	/** Removes every cosmetic part of this ant. */
	ANTKITCHEN_API void Clear(AAntCharacter* Ant);
	/** Trailer roles: helmet, sgt, hauler, scout, medic, recruit, sapper, none. */
	ANTKITCHEN_API int32 Apply(AAntCharacter* Ant, const FString& Role);
	/** sRGB hex -> linear. */
	ANTKITCHEN_API FLinearColor Col(uint32 Hex);
}
