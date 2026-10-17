#include "AntPlayerController.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void AAntPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		bShowMouseCursor = false;
	}
}

void AAntPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
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

void AAntPlayerController::ClientDamageNumber_Implementation(FVector_NetQuantize Location, int32 Amount, bool bCrit)
{
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
