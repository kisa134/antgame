#include "AntPlayerController.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "AntSwarm.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "AntTypes.h"
#include "Engine/World.h"

void AAntPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		// Kitchen room tone: fridge hum and air, under everything.
		if (USoundBase* Amb = AntSfx::Get(AntSfx::Ambience))
		{
			UGameplayStatics::SpawnSound2D(this, Amb, 0.35f, 1.f, 0.f, nullptr, true, false);
		}
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		bShowMouseCursor = false;

		// Dev/test switches.
		FString TpArg;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntTp="), TpArg, false))
		{
			TArray<FString> Parts;
			TpArg.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() >= 3)
			{
				const FVector L(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
				const bool bYaw = Parts.Num() >= 4;
				const float Yaw = bYaw ? FCString::Atof(*Parts[3]) : 0.f;
				const float Pitch = Parts.Num() >= 5 ? FCString::Atof(*Parts[4]) : -10.f;
				FTimerHandle H;
				GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, L, bYaw, Yaw, Pitch]()
				{
					ServerTp(L);
					if (bYaw) { SetControlRotation(FRotator(Pitch, Yaw, 0.f)); }
				}), 3.f, false);
			}
		}
		int32 TestAnts = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntSwarmTest="), TestAnts) && TestAnts > 0)
		{
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, TestAnts]() { ServerSwarmTest(TestAnts); }), 2.f, false);
		}
		float GrabAt = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntGrabAt="), GrabAt) && GrabAt > 0.f)
		{
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { Grab(); }), GrabAt, false);
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("AntPeace")))
		{
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { ServerPeace(); }), 1.f, false);
		}
	}
}

void AAntPlayerController::Tp(float X, float Y, float Z)
{
	ServerTp(FVector(X, Y, Z));
}

void AAntPlayerController::SwarmTest(int32 Ants)
{
	ServerSwarmTest(Ants);
}

void AAntPlayerController::ServerSwarmTest_Implementation(int32 Ants)
{
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Swarm->DebugSpawnTestStreams(FMath::Clamp(Ants, 0, 200000));
	}
}

void AAntPlayerController::AddBot(int32 Team, int32 Count)
{
	ServerAddBot(Team, FMath::Max(1, Count));
}

void AAntPlayerController::ServerAddBot_Implementation(int32 Team, int32 Count)
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->AddBots((uint8)FMath::Clamp(Team, 0, 1), FMath::Clamp(Count, 1, 8));
	}
}

void AAntPlayerController::Grab()
{
	ServerGrab();
}

void AAntPlayerController::ServerGrab_Implementation()
{
	if (AAntCharacter* Ant = Cast<AAntCharacter>(GetPawn()))
	{
		Ant->DevInteract();
	}
}

void AAntPlayerController::Peace()
{
	ServerPeace();
}

void AAntPlayerController::ServerTp_Implementation(FVector Location)
{
	if (AAntCharacter* Ant = Cast<AAntCharacter>(GetPawn()))
	{
		Ant->ApplyTeleport(Location);
	}
}

void AAntPlayerController::ServerPeace_Implementation()
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->TogglePeace();
	}
}

void AAntPlayerController::ServerToggleCreature_Implementation(uint8 Kind)
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->ToggleCreature(this, Kind == 2 ? 2 : 1);
	}
}

void AAntPlayerController::ServerSwitchTeam_Implementation()
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->SwitchTeam(this);
	}
}

void AAntPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AAntPlayerController::BecomeFly);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AAntPlayerController::BecomeSpider);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AAntPlayerController::SwitchTeam);
	InputComponent->BindAction("ToggleHuman", IE_Pressed, this, &AAntPlayerController::RequestToggleHuman);
	InputComponent->BindAction("Scoreboard", IE_Pressed, this, &AAntPlayerController::ShowScores);
	InputComponent->BindAction("Scoreboard", IE_Released, this, &AAntPlayerController::HideScores);
}

void AAntPlayerController::RequestToggleHuman()
{
	ServerToggleHuman();
}

void AAntPlayerController::ShowScores() { bShowScores = true; }
void AAntPlayerController::HideScores() { bShowScores = false; }

void AAntPlayerController::ServerToggleHuman_Implementation()
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->ToggleHuman(this);
	}
}

void AAntPlayerController::PawnLeavingGame()
{
	// Never destroy the shared Human when its player leaves: hand it back to the AI.
	if (Cast<AHumanCharacter>(GetPawn()))
	{
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->HandleHumanPlayerLeft(this);
		}
		else
		{
			UnPossess();
		}
		return;
	}
	Super::PawnLeavingGame();
}

void AAntPlayerController::ClientToast_Implementation(const FString& Text)
{
	FAntToast T;
	T.Text = Text;
	T.Time = GetWorld()->GetTimeSeconds();
	Toasts.Add(T);
	if (Toasts.Num() > 5)
	{
		Toasts.RemoveAt(0);
	}
}

void AAntPlayerController::ClientKillConfirm_Implementation(const FString& Victim, const FString& Streak)
{
	KillMarkerTime = GetWorld()->GetTimeSeconds();
	KillText = Streak.IsEmpty() ? FString::Printf(TEXT("УБИТ: %s"), *Victim) : FString::Printf(TEXT("%s  —  %s"), *Streak, *Victim);
	AntSfx::Play2D(this, AntSfx::Kill, 0.9f, Streak.IsEmpty() ? 1.f : 1.25f);
}

void AAntPlayerController::ClientDamageNumber_Implementation(FVector_NetQuantize Location, int32 Amount, bool bCrit)
{
	HitMarkerTime = GetWorld()->GetTimeSeconds();
	bHitCrit = bCrit;
	AntSfx::Play2D(this, bCrit ? AntSfx::Crit : AntSfx::Hit, bCrit ? 0.8f : 0.55f, FMath::FRandRange(0.95f, 1.1f));
	FAntDamageNumber N;
	N.Location = Location;
	N.Amount = Amount;
	N.bCrit = bCrit;
	N.Time = GetWorld()->GetTimeSeconds();
	DamageNumbers.Add(N);
	if (DamageNumbers.Num() > 40)
	{
		DamageNumbers.RemoveAt(0);
	}
}

void AAntPlayerController::HostGame()
{
	const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetPackage()->GetName());
	UGameplayStatics::OpenLevel(this, FName(*Map), true, TEXT("listen"));
}

void AAntPlayerController::JoinGame(const FString& Address)
{
	ClientTravel(Address, TRAVEL_Absolute);
}
