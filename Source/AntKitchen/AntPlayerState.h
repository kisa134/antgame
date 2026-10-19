#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AntTypes.h"
#include "AntPlayerState.generated.h"

UCLASS()
class ANTKITCHEN_API AAntPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) int32 Delivered = 0;   // sweetness brought to the nest
	UPROPERTY(Replicated) int32 Deaths = 0;
	UPROPERTY(Replicated) int32 Kills = 0;       // ants squashed while playing the Human
	UPROPERTY(Replicated) bool bIsHuman = false;
	UPROPERTY(Replicated) uint8 Team = 0;                     // 0 colony, 1 fire ants
	UPROPERTY(Replicated) EAntClass AntClass = EAntClass::Worker;
	UPROPERTY(Replicated) FLinearColor AntColor = FLinearColor(0.12f, 0.04f, 0.03f);
};
