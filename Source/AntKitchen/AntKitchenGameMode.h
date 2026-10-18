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
	void OnLootDelivered(AAntCharacter* Ant, int32 Value, int32 Items);
	void OnHumanKnockedOut(AController* By);
	void Broadcast(const FString& Text, const FLinearColor& Color);

	AKitchenLoot* SpawnFood(const FVector& Location, int32 Tier);
	AKitchenLoot* SpawnGear(const FVector& Location, const FAntWeapon& Weapon);
	AKitchenLoot* SpawnStash(const FVector& Location, int32 Value, int32 Weight, int32 Items);

	UPROPERTY(EditDefaultsOnly, Category = "Match") int32 TargetScore = 1500;
	UPROPERTY(EditDefaultsOnly, Category = "Match") float MatchLength = 600.f;
	UPROPERTY(EditDefaultsOnly, Category = "Match") float RespawnDelay = 5.f;

private:
	void SpawnHuman();
	void SpawnGadgets();
	void KitchenLifeEvent();
	void SpawnInitialLoot();
	void RefillLoot();
	void MatchTick();
	void EndMatch(bool bAntsWin);
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

	FTimerHandle MatchTimer;
	FTimerHandle RefillTimer;
	FTimerHandle ResetTimer;
	FTimerHandle LifeTimer;
	FTimerHandle FaucetOffTimer;
};
