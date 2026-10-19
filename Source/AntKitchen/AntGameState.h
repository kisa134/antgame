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
};
