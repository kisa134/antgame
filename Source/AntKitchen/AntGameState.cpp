#include "AntGameState.h"
#include "KitchenBuilder.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

void AAntGameState::BeginPlay()
{
	Super::BeginPlay();

	// Every machine (server and each client) builds its own identical copy of the kitchen.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AKitchenBuilder>(AKitchenBuilder::StaticClass(), FTransform::Identity, Params);
}

void AAntGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntGameState, TeamScore);
	DOREPLIFETIME(AAntGameState, TargetScore);
	DOREPLIFETIME(AAntGameState, TimeRemaining);
	DOREPLIFETIME(AAntGameState, Phase);
	DOREPLIFETIME(AAntGameState, AntsSquashed);
	DOREPLIFETIME(AAntGameState, HumanKnockouts);
}

void AAntGameState::MulticastMessage_Implementation(const FString& Text, FLinearColor Color)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FAntFeedLine Line;
	Line.Text = Text;
	Line.Color = Color;
	Line.Time = GetWorld()->GetTimeSeconds();
	Feed.Add(Line);
	if (Feed.Num() > 12)
	{
		Feed.RemoveAt(0);
	}
}
