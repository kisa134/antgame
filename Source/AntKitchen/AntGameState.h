#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/NetSerialization.h"
#include "AntGameState.generated.h"

struct FAntFeedLine
{
	FString Text;
	FLinearColor Color;
	float Time = 0.f;
};

/** A ping a teammate put into the world (middle mouse button). Local list, filled by MulticastPing. */
struct FAntPingMark
{
	FVector Loc = FVector::ZeroVector;
	uint8 Kind = 0;
	uint8 Team = 0;
	FString Who;
	float Time = 0.f;
};

UCLASS()
class ANTKITCHEN_API AAntGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) int32 TeamScore = 0;
	UPROPERTY(Replicated) int32 RivalScore = 0;      // fire ants
	UPROPERTY(Replicated) uint8 HumanRage = 0;       // 0..3
	UPROPERTY(Replicated) int32 RivalWorkers = 1500; // fire ants' reserve for their armies
	UPROPERTY(Replicated) int32 TargetScore = 1500;
	UPROPERTY(Replicated) float TimeRemaining = 600.f;
	UPROPERTY(Replicated) uint8 Phase = 0;            // 0 playing, 1 ants won, 2 human won, 3 fire ants won
	UPROPERTY(Replicated) int32 AntsSquashed = 0;
	UPROPERTY(Replicated) int32 HumanKnockouts = 0;
	/** "Муравьиная война": territories + cashouts, 128 army ants per side in a reserve pool. */
	UPROPERTY(Replicated) bool bWarMode = false;
	/** «Хайст»: the shift (default mode). Trophies delivered whole / smashed this shift. */
	UPROPERTY(Replicated) bool bHeistMode = true;
	UPROPERTY(Replicated) int32 TrophiesWhole = 0;
	UPROPERTY(Replicated) int32 TrophiesBroken = 0;
	/** The run: shift number, respawns left for the whole colony, the quota (TargetScore) reached, and what the Human is up to (0 away, 1 hunting, 2 clean-up, 3 last call). */
	UPROPERTY(Replicated) int32 Shift = 1;
	UPROPERTY(Replicated) int32 Larvae = 0;
	UPROPERTY(Replicated) uint8 ShiftStage = 0;
	UPROPERTY(Replicated) bool bQuotaMet = false;
	UPROPERTY(Replicated) int32 WarPool0 = 128;
	UPROPERTY(Replicated) int32 WarPool1 = 128;
	int32 WarPool(uint8 Team) const { return Team == 0 ? WarPool0 : WarPool1; }

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMessage(const FString& Text, FLinearColor Color);

	/** A sound everyone hears (AntSfx::EId). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSfx(uint8 Id, FVector_NetQuantize Location, float Pitch);

	/** Food broke: cosmetic crumbs fly everywhere. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastCrumbs(FVector_NetQuantize Location, FLinearColor Color, uint8 Count, float Size);

	/** Local kill feed, filled by MulticastMessage. */
	TArray<FAntFeedLine> Feed;

	// ---- The run (AntRun.h): everything players see of cards, orders, greed, awards, events, the Human's plan ----
	/** Cards the colony owns for this run (AntCard::EId). */
	UPROPERTY(Replicated) TArray<uint8> Cards;
	/** Between shifts: 0 nothing, 1 the results and awards, 2 the card vote. */
	UPROPERTY(Replicated) uint8 Interlude = 0;
	/** Server world time the current interlude step ends (compare with GetServerWorldTimeSeconds). */
	UPROPERTY(Replicated) float InterludeEnds = 0.f;
	UPROPERTY(Replicated) TArray<uint8> CardOffer;
	UPROPERTY(Replicated) TArray<uint8> CardVotes;
	/** The card that won the vote (255 = none yet). */
	UPROPERTY(Replicated) uint8 CardPicked = 255;
	/** The Queen's orders of this shift (AntOrder::EId), a bit per order when done / failed. */
	UPROPERTY(Replicated) TArray<uint8> Orders;
	UPROPERTY(Replicated) uint8 OrdersDone = 0;
	UPROPERTY(Replicated) uint8 OrdersFailed = 0;
	/** Greed: multiplier on what is delivered after the quota is met. */
	UPROPERTY(Replicated) float Greed = 1.f;
	UPROPERTY(Replicated) TArray<FString> Awards;
	/** This run so far: shifts survived, everything hauled. */
	UPROPERTY(Replicated) int32 RunShifts = 0;
	UPROPERTY(Replicated) int32 RunHaul = 0;
	UPROPERTY(Replicated) int32 LarvaeStart = 0;
	/** The shift's event (AntEvent::EId) while it runs, when it ends (server time), where it is. */
	UPROPERTY(Replicated) uint8 EventId = 0;
	UPROPERTY(Replicated) float EventEnds = 0.f;
	UPROPERTY(Replicated) FVector_NetQuantize EventLoc;
	/** What the AI Human is doing (AntHumanGoal::EKind) and where he is heading. */
	UPROPERTY(Replicated) uint8 HumanGoal = 0;
	UPROPERTY(Replicated) FString HumanGoalText;
	UPROPERTY(Replicated) FVector_NetQuantize HumanGoalLoc;

	bool HasCard(uint8 Id) const { return Cards.Contains(Id); }

	/** Level v3: the raid seed, seed * 8 + tier (-1: none yet). The host sets it (SetLevelSeed); every machine applies the same validated
	 *  table row (LevelV3::FindSeed) to its own world. */
	UPROPERTY(ReplicatedUsing = OnRep_LevelSeed) int32 LevelSeedKey = -1;
	UFUNCTION() void OnRep_LevelSeed();
	/** KAN-37: the level lab variant of this world ('A'.., 0 = the kitchen). Set by the host, applied on every machine (AntLab.h). */
	UPROPERTY(ReplicatedUsing = OnRep_LabVariant) uint8 LabVariant = 0;
	UFUNCTION() void OnRep_LabVariant();
	/** Server: use this validated seed of this tier from now on (and on this machine at once). */
	void SetLevelSeed(int32 Seed, int32 Tier);
	int32 LevelSeed() const { return LevelSeedKey < 0 ? -1 : LevelSeedKey / 8; }

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPing(FVector_NetQuantize Loc, uint8 Kind, uint8 Team, const FString& Who);
	/** The run is over: every machine keeps its own best run (shown in the main menu). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRunOver(int32 Shifts, int32 Haul);
	TArray<FAntPingMark> Pings;

	/** Best run saved on this machine. */
	static void LoadBestRun(int32& OutShifts, int32& OutHaul);
	static void SaveBestRun(int32 Shifts, int32 Haul);
};
