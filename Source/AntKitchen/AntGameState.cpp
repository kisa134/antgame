#include "AntGameState.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "AntCharacter.h"
#include "AntTypes.h"
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
	DOREPLIFETIME(AAntGameState, RivalScore);
	DOREPLIFETIME(AAntGameState, HumanRage);
	DOREPLIFETIME(AAntGameState, RivalWorkers);
	DOREPLIFETIME(AAntGameState, TargetScore);
	DOREPLIFETIME(AAntGameState, TimeRemaining);
	DOREPLIFETIME(AAntGameState, Phase);
	DOREPLIFETIME(AAntGameState, AntsSquashed);
	DOREPLIFETIME(AAntGameState, HumanKnockouts);
	DOREPLIFETIME(AAntGameState, bWarMode);
	DOREPLIFETIME(AAntGameState, WarPool0);
	DOREPLIFETIME(AAntGameState, WarPool1);
}

void AAntGameState::MulticastSfx_Implementation(uint8 Id, FVector_NetQuantize Location, float Pitch)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	AntSfx::Play(this, Id, Location, Id == AntSfx::Stomp ? 1.6f : 1.f, Pitch);
	// Big hits shake the local camera.
	if (Id == AntSfx::Stomp || Id == AntSfx::Crunch || Id == AntSfx::Zap || Id == AntSfx::Swat)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (AAntCharacter* Me = Cast<AAntCharacter>(PC->GetPawn()))
			{
				const float D = (float)FVector::Dist(Me->GetActorLocation(), Location);
				const float Range = Id == AntSfx::Stomp ? 3500.f : 900.f;
				Me->AddTrauma(FMath::Clamp(1.f - D / Range, 0.f, 1.f) * (Id == AntSfx::Stomp ? 1.f : 0.4f));
			}
		}
	}
}

void AAntGameState::MulticastCrumbs_Implementation(FVector_NetQuantize Location, FLinearColor Color, uint8 Count, float Size)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AFoodCrumbs* C = GetWorld()->SpawnActor<AFoodCrumbs>(AFoodCrumbs::StaticClass(), FTransform(Location), P))
	{
		C->Burst(Color, Count, Size);
	}
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
