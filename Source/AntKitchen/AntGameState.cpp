#include "AntGameState.h"
#include "KitchenBuilder.h"
#include "KitchenLoot.h"
#include "AntCharacter.h"
#include "AntTypes.h"
#include "AntFx.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	DOREPLIFETIME(AAntGameState, bHeistMode);
	DOREPLIFETIME(AAntGameState, TrophiesWhole);
	DOREPLIFETIME(AAntGameState, TrophiesBroken);
	DOREPLIFETIME(AAntGameState, Shift);
	DOREPLIFETIME(AAntGameState, Larvae);
	DOREPLIFETIME(AAntGameState, ShiftStage);
	DOREPLIFETIME(AAntGameState, bQuotaMet);
	DOREPLIFETIME(AAntGameState, WarPool0);
	DOREPLIFETIME(AAntGameState, WarPool1);
	DOREPLIFETIME(AAntGameState, Cards);
	DOREPLIFETIME(AAntGameState, Interlude);
	DOREPLIFETIME(AAntGameState, InterludeEnds);
	DOREPLIFETIME(AAntGameState, CardOffer);
	DOREPLIFETIME(AAntGameState, CardVotes);
	DOREPLIFETIME(AAntGameState, CardPicked);
	DOREPLIFETIME(AAntGameState, Orders);
	DOREPLIFETIME(AAntGameState, OrdersDone);
	DOREPLIFETIME(AAntGameState, OrdersFailed);
	DOREPLIFETIME(AAntGameState, Greed);
	DOREPLIFETIME(AAntGameState, Awards);
	DOREPLIFETIME(AAntGameState, RunShifts);
	DOREPLIFETIME(AAntGameState, RunHaul);
	DOREPLIFETIME(AAntGameState, LarvaeStart);
	DOREPLIFETIME(AAntGameState, EventId);
	DOREPLIFETIME(AAntGameState, EventEnds);
	DOREPLIFETIME(AAntGameState, EventLoc);
	DOREPLIFETIME(AAntGameState, HumanGoal);
	DOREPLIFETIME(AAntGameState, HumanGoalText);
	DOREPLIFETIME(AAntGameState, HumanGoalLoc);
}

void AAntGameState::MulticastPing_Implementation(FVector_NetQuantize Loc, uint8 Kind, uint8 Team, const FString& Who)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FAntPingMark P;
	P.Loc = Loc;
	P.Kind = Kind;
	P.Team = Team;
	P.Who = Who;
	P.Time = GetWorld()->GetTimeSeconds();
	Pings.Add(P);
	if (Pings.Num() > 8)
	{
		Pings.RemoveAt(0);
	}
	AntSfx::Play2D(this, AntSfx::PickupGear, 0.5f, 1.6f);
}

namespace
{
	/** The best run of this machine: a two-number text file in the game's Saved folder ("shifts haul"). */
	FString BestRunPath()
	{
		return FPaths::ProjectSavedDir() / TEXT("BestRun.txt");
	}
}

void AAntGameState::LoadBestRun(int32& OutShifts, int32& OutHaul)
{
	OutShifts = 0;
	OutHaul = 0;
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *BestRunPath()))
	{
		TArray<FString> Parts;
		Text.ParseIntoArrayWS(Parts);
		if (Parts.Num() >= 2)
		{
			OutShifts = FMath::Max(0, FCString::Atoi(*Parts[0]));
			OutHaul = FMath::Max(0, FCString::Atoi(*Parts[1]));
		}
	}
}

void AAntGameState::SaveBestRun(int32 Shifts, int32 Haul)
{
	int32 BestShifts = 0, BestHaul = 0;
	LoadBestRun(BestShifts, BestHaul);
	if ((Shifts < BestShifts) || (Shifts == BestShifts && Haul <= BestHaul))
	{
		return;
	}
	const bool bOk = FFileHelper::SaveStringToFile(FString::Printf(TEXT("%d %d\n"), Shifts, Haul), *BestRunPath());
	UE_LOG(LogTemp, Display, TEXT("ANTRUN best run %s: shifts=%d haul=%d -> %s"), bOk ? TEXT("saved") : TEXT("NOT saved"), Shifts, Haul, *BestRunPath());
}

void AAntGameState::MulticastRunOver_Implementation(int32 Shifts, int32 Haul)
{
	UE_LOG(LogTemp, Display, TEXT("ANTRUN run over: shifts=%d haul=%d"), Shifts, Haul);
	if (GetNetMode() != NM_DedicatedServer)
	{
		SaveBestRun(Shifts, Haul);
	}
}

void AAntGameState::MulticastSfx_Implementation(uint8 Id, FVector_NetQuantize Location, float Pitch)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	AntSfx::Play(this, Id, Location, Id == AntSfx::Stomp ? 1.6f : 1.f, Pitch);
	// What you hear you also see (cosmetic, local): dust under the slipper, sparks, splashes, steam, the nest lighting up.
	const FVector At = Location;
	switch (Id)
	{
	case AntSfx::Stomp:
		AntFx::StompDust(this, At);
		break;
	case AntSfx::Zap:
		AntFx::Sparks(this, At + FVector(0.f, 0.f, 10.f), 16, 7.f);
		break;
	case AntSfx::Splash:
		AntFx::Splash(this, At, 5.f);
		break;
	case AntSfx::Sizzle:
		for (int32 i = 0; i < 5; ++i)
		{
			AntFx::Steam(this, At + FVector(0.f, 0.f, 4.f), 16.f, 45.f);
		}
		break;
	case AntSfx::Deliver:
	{
		const bool bRival = FVector::DistSquared2D(At, AKitchenBuilder::NestLocation(1)) < FVector::DistSquared2D(At, AKitchenBuilder::NestLocation(0));
		AntFx::DeliverPulse(this, At, bRival ? FLinearColor(1.f, 0.35f, 0.12f) : FLinearColor(1.f, 0.8f, 0.2f));
		break;
	}
	default:
		break;
	}
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
