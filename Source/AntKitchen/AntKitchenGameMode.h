#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AntTypes.h"
#include "AntRun.h"
#include "HumanNav.h"
#include "AntKitchenGameMode.generated.h"

class AAntCharacter;
class AHumanCharacter;
class AAntPlayerController;
class AKitchenLoot;
class AKitchenDoor;
class AKitchenFaucet;
class AAntTerritory;
class AAntCashout;
class AAntBotController;
class AAntRaid;

/**
 * Server-side rules:
 *  - every player spawns as an ant at the nest;
 *  - one Human exists at all times (AI by default, a player can take it with H);
 *  - ants deliver food to the nest; the colony wins at TargetScore,
 *    the Human wins if the timer runs out.
 */
UCLASS()
class ANTKITCHEN_API AAntKitchenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAntKitchenGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	void ToggleHuman(AAntPlayerController* PC);
	/** Dev: AI Human stops hunting. */
	void TogglePeace();
	void HandleHumanPlayerLeft(AAntPlayerController* PC);

	void OnAntKilled(AAntCharacter* Ant, AController* Killer, AActor* Causer);
	/** A piece of food reached a nest (Team 0 = colony, 1 = fire ants). */
	void OnFoodDelivered(uint8 Team, AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers);
	/** Spawns an AI fly / the AI spider. */
	void SpawnFly();
	void SpawnSpider();
	/** A player becomes a fly (Kind 1) or takes over the spider (Kind 2); again = back to ant. */
	void ToggleCreature(AAntPlayerController* PC, uint8 Kind);
	/** Adds bot ants (Team 0 teammates, 1 rivals). */
	void AddBots(uint8 Team, int32 Count);
	void OnHumanKnockedOut(AController* By);
	/** The worker swarm brought food home. */
	void OnSwarmIncome(int32 Value);
	void Broadcast(const FString& Text, const FLinearColor& Color);
	void AddScore(uint8 Team, int32 Value);
	/** War mode: the reserve of army ants each side can send out (+ reinforcements, - armies sent). */
	void AddPool(uint8 Team, int32 N);
	/** Workers from a dissolved living bridge: back to the builder's army, else to the reserve. */
	void ReturnWorkers(uint8 Team, int32 N, AAntCharacter* Builder);
	/** [P] in the nest / while dead: play for the other colony. */
	void SwitchTeam(AAntPlayerController* PC);
	bool IsWar() const;

	AKitchenLoot* SpawnFood(const FVector& Location, int32 Tier);
	AKitchenLoot* SpawnGear(const FVector& Location, const FAntWeapon& Weapon);
	AKitchenLoot* SpawnStash(const FVector& Location, int32 Value, int32 Weight, int32 Items);

	UPROPERTY(EditDefaultsOnly, Category = "Match") int32 TargetScore = 1500;
	UPROPERTY(EditDefaultsOnly, Category = "Match") float MatchLength = 600.f;
	UPROPERTY(EditDefaultsOnly, Category = "Match") float RespawnDelay = 5.f;
	UPROPERTY(EditDefaultsOnly, Category = "Match") int32 AllyBots = 7;    // + you = 8 per colony
	UPROPERTY(EditDefaultsOnly, Category = "Match") int32 RivalBots = 8;
	/** Modes: «Хайст» (default: co-op heist, trophies up high), «Война» (-AntWar: territories + cashouts), «Набег» (-AntClassic). */
	UPROPERTY(EditDefaultsOnly, Category = "Match") bool bWar = false;
	UPROPERTY(EditDefaultsOnly, Category = "Match") bool bHeist = true;
	/** The heist as an extraction game: NEST -> RAID -> NEST (AntRaid.h). The default; -AntShifts gives the old run of shifts. */
	bool bRaid = false;
	UPROPERTY() AAntRaid* Raid = nullptr;
	friend class AAntRaid;
	/** Heist: keeps 3 trophies in the kitchen. */
	void HeistTick();
	/** The run: a quota per shift that grows, a shared pool of respawns, the Human escalating over the shift. */
	void StartShift();
	void ShiftTick();
	void SetHumanAwake(bool bAwake);
	bool AnyAllyPlayerAlive(bool bMustBeOutside) const;
	int32 ShiftNumber = 1;
	int32 BestShift = 0;
	void SpawnTrophy(int32 Slot);
	/** Level v2: the main object of the shift, a slice of birthday cake (weight 70, 600 when brought home whole). One per shift,
	 *  at one of the three designed places (LevelV2::MainSpots; the fridge crisper is the rare one). ForceSpot: dev. */
	void SpawnMainObject(int32 ForceSpot = -1);
	/** Level v2: the grid the Human, grandma and the kid find their way on (FHumanNav). Built on first use. Null in the box-built kitchen. */
	const FHumanNav* HouseNav();
	FHumanNav HouseNavGrid;
	static int32 MainWeight();
	TWeakObjectPtr<AKitchenLoot> MainObject;
	// Dev: the player's ant walks a list of points by itself (-AntDo "go x y z ...").
	TArray<FVector> DevGoPts;
	float DevGoStart = 0.f, DevGoBest = 0.f, DevGoBestAt = 0.f, DevGoLogAt = 0.f;
	// Dev: food on every spot of the table, and where each piece came to rest ("lootall", "lootrest").
	TArray<TWeakObjectPtr<AKitchenLoot>> DevLootPlaced;
	TArray<FVector> DevLootWanted;
	FTimerHandle HeistTimer;
	float TrophyLostAt = -100.f;

	// ---- The run on top of the shift (AntRun.cpp): noise, doors, cards, orders, greed, awards, events ----
	/** Something made a noise. The AI Human goes to check the loudest recent one. By: the ant that caused it, if any. */
	void MakeNoise(const FVector& Loc, float Loud, uint8 Kind, AActor* Source = nullptr, AAntCharacter* By = nullptr);
	/** For the Human AI: the noise worth checking right now, if any (not one he has just checked). */
	bool BestNoise(const FVector& From, FAntNoise& Out) const;
	/** He has looked: that source / kind does not fool him again for a while. */
	void NoiseChecked(const FAntNoise& N, bool bDistracted);
	/** What the Human is up to, for everyone's HUD. */
	void SetHumanGoal(uint8 Kind, const FString& Text, const FVector& Loc);
	void OnDoorOpened(AKitchenInteractable* Door, AAntCharacter* By, int32 Class);
	void OnTrophyBroken(AKitchenLoot* Trophy);
	void OnEnemyTorn(AAntCharacter* Victim, AAntCharacter* A, AAntCharacter* B);
	void OnHumanStunned(AAntCharacter* By, const FString& What);
	/** He slipped in a puddle and went down (By: the ant that spilled it, if any). */
	void OnHumanFell(AAntCharacter* By);
	/** A dead ant leaves a body that can be hauled home for a larva. */
	void SpawnBody(AAntCharacter* Ant);
	void CompleteOrder(uint8 OrderId);
	void FailOrder(uint8 OrderId);
	bool HasCard(uint8 Id) const;
	void VoteCard(AController* By, int32 Option);
	void Ping(class AAntPlayerController* By, const FVector& Loc);
	/** Dev: start the shift's event now (AntEvent::EId). */
	void StartEvent(uint8 Id);
	void EndEvent();
	FAntRunState Run;
	FTimerHandle RunTimer;
	FTimerHandle EventTimer;
	FTimerHandle DevTimer;

private:
	void RunBeginPlay();
	void RunStartShift();
	/** Once a second while the shift runs. */
	void RunTick();
	void RunEndShift(bool bWon);
	void InterludeTick();
	void ResolveCards();
	void BuildAwards();
	void SpawnContainerLoot();
	void SpawnHighProps();
	void CheckTrapped();
	int32 GreedValue(int32 Value) const;
	void DevScriptTick();
	void DevCommand(const FString& Cmd);
	TArray<TPair<float, FString>> DevScript;
	/** Dev: "q <command>" - commands that run one after another, each once the walker ("go") has finished. */
	TArray<FString> DevQueue;
	float DevQueueAt = 0.f;
	int32 DevGoDetours = 0, DevGoDetourPts = 0;

public:

private:
	void SpawnHuman();
	void SpawnGadgets();
	void SpawnSwarm();
	void KitchenLifeEvent();
	void SpawnInitialLoot();
	void RefillLoot();
	void MatchTick();
	/** 1 colony won, 2 Human won, 3 fire ants won. */
	void EndMatch(uint8 Winner);
	void UpdateRage();
	void SpawnHeavyLoot();
	void RivalWarTick();
	void SpawnWarZones();
	void WarTick();
	void SpawnVault();
	void AssignWarRoles(uint8 Team);
	FTimerHandle AntWarTimer;
	float NextVault = 0.f;
	UPROPERTY() TArray<AAntTerritory*> Territories;
	UPROPERTY() TArray<AAntCashout*> Cashouts;
	FTimerHandle WarTimer;
	void ResetMatch();
	void RespawnAnt(AController* C);
	bool IsSpotFree(const FVector& Location) const;
	int32 RollRarity(int32 Tier);
	int32 RollTier(int32 Tier);

	UPROPERTY() AHumanCharacter* Human = nullptr;
	UPROPERTY() AAntPlayerController* HumanPlayer = nullptr;
	UPROPERTY() AKitchenDoor* FridgeDoor = nullptr;
	UPROPERTY() AKitchenFaucet* Faucet = nullptr;

	TArray<FVector> SpotLocations;
	TArray<int32> SpotTiers;
	FRandomStream Rng;
	int32 ColorIndex = 0;
	int32 BotCounter[2] = { 0, 0 };
	UPROPERTY() class ASpiderCharacter* Spider = nullptr;
	int32 FlyCount = 0;
	TMap<TWeakObjectPtr<AController>, FVector2D> Streaks;   // X = kills in a row, Y = last kill time

	FTimerHandle MatchTimer;
	FTimerHandle RefillTimer;
	FTimerHandle ResetTimer;
	FTimerHandle LifeTimer;
	FTimerHandle FaucetOffTimer;
};
