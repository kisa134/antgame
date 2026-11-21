#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Engine/NetSerialization.h"
#include "AntSnowball.generated.h"

class AAntCharacter;
class AAntRaid;
class AKitchenLoot;
class AAntPlayerController;
class AAntKitchenGameMode;
class APlayerState;
class AController;

/**
 * The in-raid snowball (docs/GAME.md, "Снежный ком"): every player starts a raid as one ant. Food brought home fills the team's
 * mass meter; at each threshold every player takes one of three quick picks: smart squad ants, workers for the pheromone trails,
 * trail / personal / squad upgrades, tools, colony cards. The nest between raids still decides the caps and perks.
 */
namespace AntPick
{
	enum EId : uint8
	{
		Recruit2, Recruit3, Veteran,                                  // squad
		Workers12, Workers25,                                         // workers
		TrailLong, TrailSecond, TrailFast, TrailHeavy, TrailForage,   // trails
		Bite, Speed, Carry, Acid, Grip,                               // you
		SquadStrong, SquadTough, SquadFast,                           // your squad
		ToolDecoy, ToolAlarm, ToolBait,                               // tools for the team
		CardJaws, CardLegs, CardQuiet, CardLever, CardChitin, CardSpring, CardNurse, CardPadding, CardGreedy, CardOvertime, CardBrood,
		Count
	};
	enum EPool : uint8 { PSquad, PWorkers, PTrail, PPersonal, PSquadUp, PTools, PCards, PoolCount };
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
	ANTKITCHEN_API const TCHAR* Text(uint8 Id);
	ANTKITCHEN_API uint8 Pool(uint8 Id);
	ANTKITCHEN_API const TCHAR* PoolName(uint8 Pool);
	/** The run card (AntCard) a colony pick hands out, or 255. */
	ANTKITCHEN_API uint8 Card(uint8 Id);
}

/** Levels a player has bought this raid (index of FAntSnowPlayer::Lv). */
namespace AntBoost
{
	enum EId : uint8 { Bite, Speed, Carry, Acid, Grip, TrailLen, TrailCount, WorkerSpeed, WorkerCarry, Forage, SquadStr, SquadHp, SquadSpeed, Count };
}

/** What a group of a player's squad has been told to do (AntRts.h). */
namespace AntTask
{
	enum EId : uint8 { Follow, Haul, Hold, Attack, Free, Count };
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
}

USTRUCT()
struct FAntRtsTask
{
	GENERATED_BODY()
	UPROPERTY() uint8 Order = 0;                 // AntTask
	UPROPERTY() AActor* Target = nullptr;        // the load to haul, the enemy to attack
	UPROPERTY() FVector_NetQuantize Point;       // where to hold / attack
	/** For the HUD: 0 on the way, 1 carrying, 2 too heavy (needs N more), 3 fighting, 4 done. */
	UPROPERTY() uint8 State = 0;
	UPROPERTY() uint8 Need = 0;
	UPROPERTY() uint8 Members = 0;
	UPROPERTY() uint8 Id = 0;                    // stable number for the HUD
};

USTRUCT()
struct FAntSnowPlayer
{
	GENERATED_BODY()
	UPROPERTY() APlayerState* PS = nullptr;
	/** Mass this player brought (his ant, his squad, his trails). */
	UPROPERTY() int32 Mass = 0;
	/** Thresholds this player has picked for. */
	UPROPERTY() uint8 PicksDone = 0;
	/** The three cards on offer right now (empty: none), and the server time the pick is made for him. */
	UPROPERTY() TArray<uint8> Offer;
	UPROPERTY() float OfferEnds = 0.f;
	UPROPERTY() TArray<uint8> Taken;
	UPROPERTY() TArray<uint8> Lv;
	/** Worker ants this player owns (on his trails + waiting in the nest). */
	UPROPERTY() int32 Workers = 0;
	UPROPERTY() TArray<AAntCharacter*> Squad;
	/** Per squad member: index into Tasks (255: follows him). */
	UPROPERTY() TArray<uint8> SquadTask;
	UPROPERTY() TArray<FAntRtsTask> Tasks;
	// server only
	TArray<TWeakObjectPtr<AAntCharacter>> Selected;
	int32 SquadEver = 0;
	int32 SquadLost = 0;
	uint8 NextTaskId = 1;
};

UCLASS()
class ANTKITCHEN_API AAntSnowball : public AInfo
{
	GENERATED_BODY()

public:
	AAntSnowball();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	static AAntSnowball* Get(const UWorld* World);

	UPROPERTY(Replicated) int32 Mass = 0;
	/** Thresholds reached so far, the mass of the last one and of the next one. */
	UPROPERTY(Replicated) int32 Level = 0;
	UPROPERTY(Replicated) int32 PrevAt = 0;
	UPROPERTY(Replicated) int32 NextAt = 20;
	UPROPERTY(Replicated) TArray<FAntSnowPlayer> Players;
	UPROPERTY(Replicated) bool bActive = false;

	FAntSnowPlayer* Find(const APlayerState* PS);
	const FAntSnowPlayer* Find(const APlayerState* PS) const;
	/** Server: the entry of this player (made if missing). */
	FAntSnowPlayer* For(APlayerState* PS);
	int32 LvOf(const APlayerState* PS, uint8 Boost) const;

	// ---- server
	void Begin(AAntRaid* Raid);
	void End();
	void Second();
	void AddMass(APlayerState* By, int32 Amount, const FString& What);
	void Pick(APlayerState* PS, int32 Option);
	void MakeOffer(FAntSnowPlayer& P);
	void Apply(FAntSnowPlayer& P, uint8 Id);
	int32 Threshold(int32 Index) const;
	/** Caps from the nest. */
	int32 SquadCap() const;
	int32 WorkerCap() const;
	float RaidClock() const;
	/** Dev: auto pick option N (0..2) at once when offered (scripted runs). -1: off. */
	int32 AutoPick = -1;
	float StartedAt = 0.f;
	float NextTimeline = 0.f;
	TArray<uint8> CardsAdded;
	FRandomStream Rng;
};

namespace AntSnowball
{
	/** The snowball runs in a raid unless -AntNoSnowball. Works on every machine. */
	ANTKITCHEN_API bool Enabled(const UObject* Ctx);
	// ---- raid hooks (server)
	ANTKITCHEN_API void OnWorldReset(AAntRaid* Raid);
	ANTKITCHEN_API void OnRaidStart(AAntRaid* Raid);
	ANTKITCHEN_API void OnRaidEnd(AAntRaid* Raid);
	ANTKITCHEN_API void RaidSecond(AAntRaid* Raid);
	/** Food reached our nest (after the tier's price). */
	ANTKITCHEN_API void OnDelivered(AAntKitchenGameMode* GM, AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers, int32 Value);
	/** Who gets the credit for a delivery without carriers (a trail's workers): set around the call. */
	ANTKITCHEN_API void SetCredit(APlayerState* PS);
	/** Keys 1/2/3 on the local machine: true if a pick was waiting and the key took it. */
	ANTKITCHEN_API bool TryPick(AAntPlayerController* PC, int32 Option);
	/** Server: a player picked. */
	ANTKITCHEN_API void ServerPick(AAntPlayerController* PC, int32 Option);
	/** Multipliers of an ant's own upgrades (players: theirs; squad ants: their owner's squad upgrades). Every machine. */
	ANTKITCHEN_API float BiteMul(const AAntCharacter* Ant);
	ANTKITCHEN_API float SpeedMul(const AAntCharacter* Ant);
	ANTKITCHEN_API float CarryMul(const AAntCharacter* Ant);
	ANTKITCHEN_API float AcidMul(const AAntCharacter* Ant);
	/** Blows and bites do not knock the load out of its jaws. */
	ANTKITCHEN_API bool Grip(const AAntCharacter* Ant);
	/** The player whose squad this ant is in (server and clients), or null. */
	ANTKITCHEN_API APlayerState* SquadOwnerOf(const AAntCharacter* Ant);
	/** Dev (-AntDo): snow ..., trail ..., squad+ ..., rts ..., perf ... True if handled. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& Args);
}
