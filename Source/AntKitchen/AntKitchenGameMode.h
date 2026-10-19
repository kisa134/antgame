#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AntTypes.h"
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
	/** "Муравьиная война" (territories + cashouts). -AntClassic on the command line turns it off. */
	UPROPERTY(EditDefaultsOnly, Category = "Match") bool bWar = true;

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
