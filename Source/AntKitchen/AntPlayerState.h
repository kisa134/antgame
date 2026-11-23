#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AntTypes.h"
#include "AntFighter.h"
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

	// ---- The raid (AntRaid.h): what this player takes out of the nest, and where he is when the team leaves.
	/** Squad workers asked for (255 = as many as the nest allows). */
	UPROPERTY(Replicated) uint8 SquadWant = 255;
	/** Two pockets of tools (AntTool ids) and the charges left in each. */
	UPROPERTY(Replicated) uint8 Tool0 = 0;
	UPROPERTY(Replicated) uint8 Tool1 = 0;
	UPROPERTY(Replicated) uint8 Charges0 = 0;
	UPROPERTY(Replicated) uint8 Charges1 = 0;
	UPROPERTY(Replicated) bool bNestReady = false;
	/** Inside a nest entrance right now (the leaving countdown counts these). */
	UPROPERTY(Replicated) bool bHome = true;
	/** The fighter (AntFighter.h): rank, XP, perks, medals, cosmetics. Sent by his own machine from the local profile. */
	UPROPERTY(Replicated) FAntLoadout Loadout;

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
