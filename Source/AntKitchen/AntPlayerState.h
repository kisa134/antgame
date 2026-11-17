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

	// Server only: what this ant did this shift, for the awards at the end of it.
	int32 Squashed = 0;       // flattened by the slipper or a paw
	int32 TrophyBreaks = 0;   // let a trophy smash
	int32 LegsLost = 0;
	int32 Torn = 0;           // tore an enemy in two
	int32 DoorsOpened = 0;
	int32 BodiesHome = 0;
	int32 Rams = 0;
	float RideTime = 0.f;     // seconds spent riding the Human
	void ResetShiftStats() { Squashed = TrophyBreaks = LegsLost = Torn = DoorsOpened = BodiesHome = Rams = 0; RideTime = 0.f; }
};
