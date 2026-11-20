#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Engine/NetSerialization.h"
#include "AntNest.h"
#include "AntRaid.generated.h"

class AAntKitchenGameMode;
class AAntGameState;
class AAntCharacter;
class AAntPlayerController;
class AAntPlayerState;
class AKitchenLoot;
class AController;
class AAntClan;

/** NEST -> RAID -> NEST. */
namespace AntRaidPhase { enum : uint8 { Nest, Raid, Results }; }

/** Where the main object (the slice of birthday cake) is. */
namespace AntCake { enum : uint8 { Lying, Ours, Clan, Home, InHill, Eaten, Broken }; }

/** How a raid ended. */
namespace AntRaidEnd { enum : uint8 { None, Left, Timeout, Wiped }; }

/** What a player asks of the raid (AAntPlayerController::ServerRaid). */
namespace AntRaidCmd
{
	enum : uint8 { Leave, Ready, Start, Buy, Squad, Tool, Mod, Tier, Contract, Larva, Workers, NewColony, UseTool, SquadOrder, Skip };
}

/** Values of AAntGameState::Phase the raid adds (0 is "playing"; 1-3 are the old ends of a round). */
namespace AntRaidGS { enum : uint8 { InNest = 4, Results = 5 }; }

USTRUCT()
struct FAntRaidResult
{
	GENERATED_BODY()

	UPROPERTY() uint8 End = 0;            // AntRaidEnd
	UPROPERTY() int32 RaidNo = 0;
	UPROPERTY() uint8 Tier = 1;
	UPROPERTY() int32 Banked = 0;         // reached a nest entrance during the raid
	UPROPERTY() int32 Kept = 0;           // went into the stores (a wipe: the rival clan takes its share)
	UPROPERTY() int32 Ask = 0;            // what the Queen expected
	UPROPERTY() int32 Jelly = 0;
	UPROPERTY() uint8 Cake = 0;           // AntCake at the end
	UPROPERTY() int32 Deaths = 0;
	UPROPERTY() int32 Lost = 0;           // players left outside when the time ran out
	UPROPERTY() int32 SquadOut = 0;
	UPROPERTY() int32 SquadBack = 0;
	UPROPERTY() int32 ContractsDone = 0;
	UPROPERTY() int32 ContractsTaken = 0;
	UPROPERTY() int32 MoodBefore = 50;
	UPROPERTY() int32 MoodAfter = 50;
	UPROPERTY() int32 LarvaeLaid = 0;
	UPROPERTY() int32 WorkersHatched = 0;
	UPROPERTY() int32 QueenAte = 0;
	UPROPERTY() int32 LarvaeLeft = 0;
	UPROPERTY() int32 NewTier = 0;        // threat tier this raid unlocked, 0 = none
	UPROPERTY() TArray<FString> Story;
};

/**
 * The extraction loop on top of the heist rules (the default mode; -AntShifts gives the old run of shifts).
 * One replicated actor carries everything players see of it: the phase, the main object, the leaving countdown, the colony,
 * the results. Rules run on the server only. Late joiners get the whole state with the actor.
 */
UCLASS()
class ANTKITCHEN_API AAntRaid : public AInfo
{
	GENERATED_BODY()

public:
	AAntRaid();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The raid of this world, or null (classic, war, -AntShifts, or not replicated yet on a client). */
	static AAntRaid* Get(const UWorld* World);
	static const TCHAR* CakeName();

	// ---- replicated
	UPROPERTY(Replicated) uint8 Phase = AntRaidPhase::Nest;
	UPROPERTY(Replicated) int32 RaidNo = 0;
	UPROPERTY(Replicated) uint8 Tier = 1;
	/** Server world time the current step ends: the results screen, the host's start countdown (0 = none). */
	UPROPERTY(Replicated) float PhaseEnds = 0.f;
	UPROPERTY(Replicated) float StartEnds = 0.f;
	UPROPERTY(Replicated) uint8 Cake = AntCake::Lying;
	UPROPERTY(Replicated) FVector_NetQuantize CakeLoc;
	/** Players know where the cake lies (Scouts, or somebody has seen it). */
	UPROPERTY(Replicated) bool bCakeKnown = false;
	UPROPERTY(Replicated) FString CakeHint;
	/** The cake is in the rival clan's hill: server time it is eaten for good, and how much of it is left (percent). */
	UPROPERTY(Replicated) float CakeWindowEnds = 0.f;
	UPROPERTY(Replicated) uint8 CakeLeft = 100;
	UPROPERTY(Replicated) int32 RaidJelly = 0;
	/** Leaving: server time the countdown ends (0 = nobody is leaving), who started it, how many are home / have to be. */
	UPROPERTY(Replicated) float ExitEnds = 0.f;
	UPROPERTY(Replicated) FString ExitBy;
	UPROPERTY(Replicated) uint8 ExitHome = 0;
	UPROPERTY(Replicated) uint8 ExitNeed = 0;
	/** The rival clan, for the HUD: 0 quiet, 1 scouts out, 2 they know where the cake is, 3 hauling it, 4 it is in their hill. */
	UPROPERTY(Replicated) uint8 ClanAlert = 0;
	/** An ambush: 0 none known, 1 the tell (they are about to spring), 2 sprung. Where. */
	UPROPERTY(Replicated) uint8 AmbushState = 0;
	UPROPERTY(Replicated) FVector_NetQuantize AmbushLoc;
	/** Where the clan's party with the cake is (for those who can see it). */
	UPROPERTY(Replicated) FVector_NetQuantize ClanPartyLoc;
	UPROPERTY(Replicated) uint8 ClanAlive = 0;
	/** The nest screen: contracts on offer (AntOrder ids), a bit per one taken; one-raid boosts bought (a bit per AntMod). */
	UPROPERTY(Replicated) TArray<uint8> ContractOffer;
	UPROPERTY(Replicated) uint8 ContractPick = 0;
	UPROPERTY(Replicated) uint8 Mods = 0;
	UPROPERTY(Replicated) FAntNestState Nest;
	UPROPERTY(Replicated) FAntRaidResult Result;
	/** A line for whoever opens the nest screen ("the save was damaged, the backup was read"...). */
	UPROPERTY(Replicated) FString NestNote;
	/** Food the rival clan carried into its hill during this raid. */
	UPROPERTY(Replicated) int32 Stolen = 0;
	/** Special finds still hidden in the house this raid. */
	UPROPERTY(Replicated) uint8 SpecialsLeft = 0;
	/** What the next raid costs at the door (one-raid boosts, the tools in everybody's pockets), for the nest screen. */
	UPROPERTY(Replicated) int32 StartCost = 0;

	// ---- the rival clan (AntClan.h)
	UPROPERTY() AAntClan* Clan = nullptr;
	/** Server: the red ants brought the cake to their hill. It stays there, being eaten, for the tier's window. */
	bool OnClanCakeHome(AKitchenLoot* CakeLoot);
	/** Server: the stolen cake is in the red hill right now, so the hill is no safe zone for its ants. */
	static bool HillOpen();
	float ClanRespawn() const;

	bool InRaid() const { return Phase == AntRaidPhase::Raid; }
	const AntTier::FDef& TierDef() const { return AntTier::Def(Tier); }
	/** Is this ant inside a nest entrance (the nest in the wall or the south entrance at the house corner)? Works on every machine. */
	static bool IsHome(const AAntCharacter* Ant);

	// ---- server: called by the game mode
	void ServerInit(AAntKitchenGameMode* InGM);
	/** Once a second while the raid runs (the game mode's match clock). */
	void RaidSecond();
	/** The world has been reset for a new raid (loot, players, the Human): set the raid up. */
	void OnWorldReset();
	/** Food reached a nest. False: the game mode goes on with its own scoring. Value may be changed (tier, special finds). */
	bool OnDelivered(uint8 Team, AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers, int32& Value);
	void OnPlayerDied(AAntCharacter* Ant, const FString& How);
	void OnBodyHome(AController* BodyOwner, const FString& Name, const TArray<AAntCharacter*>& Carriers);
	void OnCakeBroken(AAntCharacter* By);
	/** What a finished contract pays (food). */
	int32 ContractBonus(uint8 OrderId);
	void EndRaid(uint8 How);
	/** Boldness of a house creature right now (NpcWho), by tier and by how far the raid has got. */
	int32 NpcLevel(uint8 Who) const;
	void PlayerCommand(AAntPlayerController* PC, uint8 Cmd, int32 A, int32 B);
	/** A tool out of a pocket (0 or 1), aimed at a point. */
	void UseTool(AAntPlayerController* PC, int32 Pocket, const FVector& Aim);
	void OnPlayerJoined(AController* C);
	/** Dev (-AntDo): "raid ...", "nest ...". True if the command was one of them. */
	bool DevCommand(const TArray<FString>& Args);
	/** A line of the raid's story (the results screen). */
	void Note(const FString& Line);
	FString PlaceName(const FVector& P) const;
	float RaidTime() const;
	/** Seconds since the raid started, as m:ss. */
	FString Clock() const;
	bool bAllAwake = false;

	void StartRaid();
	void EnterNest(bool bFirst);
	void SaveNest();

private:
	void PhaseSecond();
	void TrackCake();
	void HillTick();
	void StartClan();
	void StartSquads();
	void StartSpecials();
	void SpawnGuards();
	void CountStartCost();
	int32 MoodGift = 0;
	bool bSecondEvent = false;
	int32 CakeBaseHalf = 0;
	void ExitTick();
	void CancelExit(const FString& Why);
	void BuildResult(uint8 How);
	void PickContracts();
	int32 LivingPlayers(int32* OutHome = nullptr, FString* OutAway = nullptr) const;
	void Say(const FString& Text, const FLinearColor& Color) const;
	AAntGameState* GS() const;

	UPROPERTY() AAntKitchenGameMode* GM = nullptr;
	FTimerHandle PhaseTimer;
	TArray<FString> Story;
	TSet<FString> Variety;
	TSet<TWeakObjectPtr<AController>> ExitOk;
	TWeakObjectPtr<AController> ExitStarter;
	int32 Deaths = 0;
	int32 TrophiesHome = 0;
	int32 SpecialsHome = 0;
	bool bCakeLifted = false;
	float RaidStartedAt = 0.f;
	uint8 LastStage = 0;
	bool bHalfSaid = false;
};
