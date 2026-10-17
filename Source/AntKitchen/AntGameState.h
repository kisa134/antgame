#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
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
	UPROPERTY(Replicated) int32 TargetScore = 1500;
	UPROPERTY(Replicated) float TimeRemaining = 600.f;
	UPROPERTY(Replicated) uint8 Phase = 0;            // 0 playing, 1 ants won, 2 human won
	UPROPERTY(Replicated) int32 AntsSquashed = 0;
	UPROPERTY(Replicated) int32 HumanKnockouts = 0;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMessage(const FString& Text, FLinearColor Color);

	/** Local kill feed, filled by MulticastMessage. */
	TArray<FAntFeedLine> Feed;
};
