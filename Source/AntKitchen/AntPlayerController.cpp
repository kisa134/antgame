#include "AntPlayerController.h"
#include "AntKitchenGameMode.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "AntSwarm.h"
#include "AntGameState.h"
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
#include "AntRaid.h"
#include "AntSquad.h"
#include "EngineUtils.h"
#include "AntFilm.h"
#include "AntAdmin.h"
#include "GameFramework/HUD.h"
#include "AntSnowball.h"
#include "AntRts.h"

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

		// Filming mode for the marketer's clips: F8 (docs/FILMING.md).
		Film = NewObject<UAntFilm>(this, TEXT("AntFilm"));
		Film->RegisterComponent();
		Film->Init(this);
		UAntRtsInput::Attach(this);   // the snowball raid's squad selection and orders (AntRts.h)

		// Admin mode (AntAdmin.h): -AntAdmin opens the panel a moment after the start (not over the main menu);
		// -AntAdminDo="t:cmd;t:cmd" runs panel commands at those seconds through the same path as the buttons (tests).
		if (AntAdmin::Launched() && AntAdmin::CanUse(this))
		{
			FTimerHandle AH;
			GetWorldTimerManager().SetTimer(AH, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!MenuWidget.IsValid() && !AdminWidget.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("AntAdminClosed"))) { ToggleAdmin(); }
			}), 2.5f, false);
		}
		FString AdminDo;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntAdminDo="), AdminDo, false))
		{
			TArray<FString> Steps;
			AdminDo.ParseIntoArray(Steps, TEXT(";"));
			for (const FString& Step : Steps)
			{
				FString T, C;
				if (!Step.Split(TEXT(":"), &T, &C)) { continue; }
				FTimerHandle SH;
				GetWorldTimerManager().SetTimer(SH, FTimerDelegate::CreateWeakLambda(this, [this, C]()
				{
					UE_LOG(LogTemp, Display, TEXT("ANTADMIN script: %s"), *C);
					if (C == TEXT("panel")) { ToggleAdmin(); }
					else if (C.StartsWith(TEXT("aim ")))
					{
						Admin(C.Mid(4) + TEXT(" ") + FString::Printf(TEXT("%.0f %.0f %.0f"), CrosshairPoint().X, CrosshairPoint().Y, CrosshairPoint().Z));   // "aim killaim" -> killaim x y z
					}
					else { Admin(C); }
				}), FMath::Max(0.1f, FCString::Atof(*T)), false);
			}
		}

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
	InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AAntPlayerController::ToggleAdmin).bExecuteWhenPaused = true;   // admin panel (AntAdmin.h); enemy bots: in the panel
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AAntPlayerController::PingPressed);
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AAntPlayerController::ShowHelp);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AAntPlayerController::SquadHaul);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AAntPlayerController::SquadHold);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AAntPlayerController::UseTool0);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AAntPlayerController::UseTool1);
	// The card vote, for players without an ant right now (the ant routes its own 1-3 keys here while a vote is on).
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AAntPlayerController::Vote1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AAntPlayerController::Vote2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AAntPlayerController::Vote3);
	InputComponent->BindAction("ToggleHuman", IE_Pressed, this, &AAntPlayerController::RequestToggleHuman);
	InputComponent->BindAction("Scoreboard", IE_Pressed, this, &AAntPlayerController::ShowScores);
	InputComponent->BindAction("Scoreboard", IE_Released, this, &AAntPlayerController::HideScores);
}

void AAntPlayerController::RequestToggleHuman()
{
	ServerToggleHuman();
}

void AAntPlayerController::ShowHelp()
{
	HelpAskedAt = GetWorld()->GetTimeSeconds();
}

void AAntPlayerController::PingPressed()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextPingTime)
	{
		return;
	}
	NextPingTime = Now + 0.8f;
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);
	const FVector End = CamLoc + CamRot.Vector() * 30000.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntPing), false, GetPawn());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params);
	ServerPing(bHit ? Hit.ImpactPoint : CamLoc + CamRot.Vector() * 1500.f);
}

void AAntPlayerController::ServerPing_Implementation(FVector_NetQuantize Location)
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->Ping(this, Location);
	}
}

bool AAntPlayerController::TryVote(int32 Option)
{
	if (AntSnowball::TryPick(this, Option))
	{
		return true;   // keys 1/2/3 take the snowball pick first (AntSnowball.h)
	}
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	if (!GS || GS->Interlude != 2 || !GS->CardOffer.IsValidIndex(Option))
	{
		return false;
	}
	MyVote = Option;
	ServerVoteCard((uint8)Option);
	return true;
}

void AAntPlayerController::ServerVoteCard_Implementation(uint8 Option)
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->VoteCard(this, Option);
	}
}

FVector AAntPlayerController::CrosshairPoint() const
{
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AntSquadAim), false, GetPawn());
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamRot.Vector() * 20000.f, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	return CamLoc + CamRot.Vector() * 1500.f;
}

void AAntPlayerController::SquadHaul()
{
	if (Cast<AAntCharacter>(GetPawn()) && AAntRaid::Get(GetWorld())) { ServerSquad(AntSquad::Haul, CrosshairPoint()); }
}

void AAntPlayerController::SquadHold()
{
	if (Cast<AAntCharacter>(GetPawn()) && AAntRaid::Get(GetWorld())) { ServerSquad(AntSquad::Hold, CrosshairPoint()); }
}

void AAntPlayerController::UseTool0()
{
	if (Cast<AAntCharacter>(GetPawn()) && AAntRaid::Get(GetWorld())) { ServerSquad(100, CrosshairPoint()); }
}

void AAntPlayerController::UseTool1()
{
	if (Cast<AAntCharacter>(GetPawn()) && AAntRaid::Get(GetWorld())) { ServerSquad(101, CrosshairPoint()); }
}

void AAntPlayerController::ServerSquad_Implementation(uint8 Order, FVector_NetQuantize Aim)
{
	AAntRaid* Raid = AAntRaid::Get(GetWorld());
	if (Raid && Raid->InRaid() && Order >= 100)
	{
		Raid->UseTool(this, Order - 100, Aim);   // a tool out of a pocket travels the same way: it is aimed too
	}
	else if (Raid && Raid->InRaid())
	{
		AntSquad::Order(Cast<AAntCharacter>(GetPawn()), Order, Aim);
	}
}

void AAntPlayerController::ServerRaid_Implementation(uint8 Cmd, int32 A, int32 B)
{
	if (AAntRaid* Raid = AAntRaid::Get(GetWorld()))
	{
		Raid->PlayerCommand(this, Cmd, A, B);
	}
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
	if (AdminWidget.IsValid())
	{
		ToggleAdmin();   // Esc closes the admin panel first
		return;
	}
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
	if (AdminWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(AdminWidget.ToSharedRef());
	}
	AdminWidget.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------
// Admin mode (AntAdmin.h)
// ---------------------------------------------------------------------------------------------

void AAntPlayerController::ToggleAdmin()
{
	if (!IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (AdminWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(AdminWidget.ToSharedRef());
		AdminWidget.Reset();
		if (!MenuWidget.IsValid())
		{
			FInputModeGameOnly Mode;
			SetInputMode(Mode);
			bShowMouseCursor = false;
		}
		UE_LOG(LogTemp, Display, TEXT("ANTADMIN panel closed"));
		return;
	}
	if (!AntAdmin::CanUse(this))
	{
		ClientToast(TEXT("Админ-панель есть только у хоста или в одиночной игре"));
		UE_LOG(LogTemp, Display, TEXT("ANTADMIN panel refused: a client of an online game"));
		return;
	}
	AdminWidget = AntAdmin::MakePanel(this);
	GEngine->GameViewport->AddViewportWidgetContent(AdminWidget.ToSharedRef(), 90);
	// The game keeps running and keeps the keyboard (WASD); the mouse is for the panel.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
	UE_LOG(LogTemp, Display, TEXT("ANTADMIN panel opened"));
}

void AAntPlayerController::Admin(const FString& Cmd)
{
	const FString C = Cmd.TrimStartAndEnd();
	if (C.IsEmpty())
	{
		return;
	}
	AntAdmin::MarkUsed();
	const FString Low = C.ToLower();
	// This machine's own: the engine console, the HUD, the filming mode.
	if (Low.StartsWith(TEXT("con ")) || Low.StartsWith(TEXT("stat ")) || Low.StartsWith(TEXT("highresshot")))
	{
		const FString Console = Low.StartsWith(TEXT("con ")) ? C.Mid(4) : C;
		UE_LOG(LogTemp, Display, TEXT("ANTADMIN local console: %s"), *Console);
		ConsoleCommand(Console, false);
		return;
	}
	if (Low == TEXT("hudtoggle"))
	{
		if (AHUD* H = GetHUD()) { H->bShowHUD = !H->bShowHUD; UE_LOG(LogTemp, Display, TEXT("ANTADMIN local hud %d"), H->bShowHUD ? 1 : 0); }
		return;
	}
	if ((Low == TEXT("film") || Low == TEXT("freecam")) && Film)
	{
		if (Low == TEXT("film") || !Film->IsOn()) { Film->OnKey(EKeys::F8); }
		if (Low == TEXT("freecam")) { Film->OnKey(EKeys::F5); }
		UE_LOG(LogTemp, Display, TEXT("ANTADMIN local %s: filming %d"), *Low, Film->IsOn() ? 1 : 0);
		return;
	}
	ServerAdmin(C);
}

void AAntPlayerController::ServerAdmin_Implementation(const FString& Cmd)
{
	if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
	{
		GM->AdminCommand(this, Cmd.Left(400));
	}
}
