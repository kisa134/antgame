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
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "AntSession.h"
#include "AntMenu.h"
#include "KitchenLoot.h"
#include "Engine/PostProcessVolume.h"
#include "AntFx.h"
#include "EngineUtils.h"

void AAntPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		// Kitchen room tone: fridge hum and air. Off for now (too much noise under everything).
		if (USoundBase* Amb = nullptr)
		{
			UGameplayStatics::SpawnSound2D(this, Amb, 0.35f, 1.f, 0.f, nullptr, true, false);
		}
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		bShowMouseCursor = false;

		// A touch of grade over the whole kitchen: warmer, punchier, a soft vignette. -AntNoPP switches it off to compare.
		if (!FParse::Param(FCommandLine::Get(), TEXT("AntNoPP")))
		{
			if (APostProcessVolume* Grade = GetWorld()->SpawnActor<APostProcessVolume>())
			{
				Grade->bUnbound = true;
				Grade->Priority = 10.f;
				FPostProcessSettings& G = Grade->Settings;
				G.bOverride_ColorSaturation = true;
				G.ColorSaturation = FVector4(1.16f, 1.16f, 1.16f, 1.f);
				G.bOverride_ColorContrast = true;
				G.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.f);
				G.bOverride_WhiteTemp = true;
				G.WhiteTemp = 6900.f;
				G.bOverride_BloomIntensity = true;
				G.BloomIntensity = 0.9f;
				G.bOverride_VignetteIntensity = true;
				G.VignetteIntensity = 0.45f;
				G.bOverride_Sharpen = true;
				G.Sharpen = 0.5f;
			}
		}

		// Main menu on a fresh start. Launch flags and the map reload after a menu choice skip it.
		{
			const FString Cmd = FCommandLine::Get();
			UAntSession* Sess = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAntSession>() : nullptr;
			const bool bFlags = Cmd.Contains(TEXT("-AntHost")) || Cmd.Contains(TEXT("-AntJoin=")) || Cmd.Contains(TEXT("-AntNoMenu"))
				|| Cmd.Contains(TEXT("-AntTp=")) || Cmd.Contains(TEXT("-AntSwarmTest=")) || Cmd.Contains(TEXT("-AntGrabAt="));
			if (Sess && !Sess->bMenuDone && !bFlags && GetWorld()->GetNetMode() == NM_Standalone)
			{
				OpenMenu(false);
			}
		}

		// Co-op without the console: -AntHost makes this machine the listen server, -AntJoin=ip connects to a friend.
		FString JoinArg;
		if (FParse::Param(FCommandLine::Get(), TEXT("AntHost")) && GetWorld()->GetNetMode() == NM_Standalone)
		{
			FTimerHandle HH;
			GetWorldTimerManager().SetTimer(HH, FTimerDelegate::CreateWeakLambda(this, [this]() { HostGame(); }), 1.f, false);
		}
		else if (FParse::Value(FCommandLine::Get(), TEXT("AntJoin="), JoinArg, false) && GetWorld()->GetNetMode() == NM_Standalone)
		{
			FTimerHandle JH;
			GetWorldTimerManager().SetTimer(JH, FTimerDelegate::CreateWeakLambda(this, [this, JoinArg]() { JoinGame(JoinArg); }), 1.f, false);
		}

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
		// Dev/test: -AntFxTest plays every cosmetic effect in a row in front of where you stand, over and over.
		if (FParse::Param(FCommandLine::Get(), TEXT("AntFxTest")))
		{
			FTimerHandle FH;
			GetWorldTimerManager().SetTimer(FH, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				static FVector At = FVector::ZeroVector;
				static FRotator Rot = FRotator::ZeroRotator;
				static bool bPinned = false;
				if (!bPinned && PlayerCameraManager)
				{
					bPinned = true;
					At = PlayerCameraManager->GetCameraLocation();
					Rot = PlayerCameraManager->GetCameraRotation();
				}
				AntFx::TestAll(this, At, Rot);
			}), 0.25f, true, 6.f);
		}
		// Dev/test: -AntSmashAt=seconds smashes a handful of food, to see the fracture without climbing the fridge.
		float SmashAt = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntSmashAt="), SmashAt) && SmashAt > 0.f && HasAuthority())
		{
			FTimerHandle SH;
			GetWorldTimerManager().SetTimer(SH, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				TArray<AKitchenLoot*> Food;
				for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
				{
					if (It->CanBreak()) { Food.Add(*It); }
				}
				int32 Smashed = 0;
				for (AKitchenLoot* L : Food)
				{
					if (Smashed++ < 10) { L->Shatter(L->GetActorLocation()); }
				}
				UE_LOG(LogTemp, Display, TEXT("AntSmash: smashed %d of %d breakable food"), FMath::Min(Smashed, 10), Food.Num());
			}), SmashAt, false);
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
		ClientToast(FString::Printf(TEXT("Бот добавлен: %s (F9 — свой, F10 — враг)"), Team == 0 ? TEXT("союзник") : TEXT("огненный")));
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
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AAntPlayerController::ToggleMenu);
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AAntPlayerController::BecomeFly);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AAntPlayerController::BecomeSpider);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AAntPlayerController::SwitchTeam);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AAntPlayerController::AddAllyBot);
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AAntPlayerController::AddEnemyBot);
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
	// Hosting goes through Steam: a lobby friends can find and join without IPs or port forwarding.
	if (UAntSession* Sess = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAntSession>() : nullptr)
	{
		Sess->Host();
	}
}

void AAntPlayerController::JoinGame(const FString& Address)
{
	ClientTravel(Address, TRAVEL_Absolute);
}

void AAntPlayerController::OpenMenu(bool bPause)
{
	if (MenuWidget.IsValid() || !IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	TSharedRef<SAntMenu> Menu = SNew(SAntMenu).PC(this).bPause(bPause);
	MenuWidget = Menu;
	GEngine->GameViewport->AddViewportWidgetContent(Menu, 100);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}

void AAntPlayerController::CloseMenu()
{
	if (!MenuWidget.IsValid())
	{
		return;
	}
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
	}
	MenuWidget.Reset();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
}

void AAntPlayerController::ToggleMenu()
{
	if (MenuWidget.IsValid())
	{
		CloseMenu();
	}
	else
	{
		OpenMenu(true);
	}
}

void AAntPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Viewport widgets outlive the map, so the menu has to be taken down with its controller.
	if (MenuWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
	}
	MenuWidget.Reset();
	Super::EndPlay(EndPlayReason);
}