#include "AntUiSubsystem.h"
#include "AntUiStyle.h"
#include "AntUiSettings.h"
#include "AntUiMenu.h"
#include "AntPlayerController.h"
#include "AntRaid.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "MoviePlayer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	uint8 ScreenByName(const FString& N)
	{
		static const TCHAR* Names[AntUiScreen::Count] = { TEXT("Boot"), TEXT("Main"), TEXT("Coop"), TEXT("Colony"), TEXT("Lobby"), TEXT("Settings"), TEXT("Credits"), TEXT("Pause"), TEXT("Loading"), TEXT("Training"), TEXT("DemoEnd") };
		for (uint8 i = 0; i < AntUiScreen::Count; ++i)
		{
			if (N.StartsWith(Names[i])) { return i; }
		}
		return 255;
	}

	AAntPlayerController* LocalPC(UGameInstance* GI)
	{
		return GI ? Cast<AAntPlayerController>(GI->GetFirstLocalPlayerController()) : nullptr;
	}

	FAutoConsoleCommandWithWorld CmdLobby(TEXT("AntLobby"), TEXT("Opens the squad muster screen (co-op lobby)."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (AAntPlayerController* PC = World ? Cast<AAntPlayerController>(World->GetFirstPlayerController()) : nullptr) { PC->OpenUiScreen(AntUiScreen::Lobby); }
	}));
}

void UAntUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IsRunningDedicatedServer())
	{
		return;
	}
	FAntUiSettings::Get().Apply(nullptr);
	AntUi::InstallCanvasFonts();
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UAntUiSubsystem::OnPreLoadMap);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAntUiSubsystem::Tick), 0.1f);
	FString List;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntUiShots="), List, false))
	{
		List.ParseIntoArray(Shots, TEXT(","));
	}
}

void UAntUiSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::Deinitialize();
}

namespace
{
	/** Everything the loading screen draws, loaded on the game thread before it starts (it paints on its own thread). */
	void PreloadLoadingArt()
	{
		for (const TCHAR* P : { TEXT("Loading/T_UI_LoadChain"), TEXT("Loading/T_UI_LoadGruzd"), TEXT("Loading/T_UI_LoadCat"), TEXT("Loading/T_UI_LoadSpray"),
			TEXT("Loading/T_UI_LoadDog"), TEXT("Loading/T_UI_LoadMedic"), TEXT("Backgrounds/T_UI_BgCake"), TEXT("Backgrounds/T_UI_BgDawn"),
			TEXT("Backgrounds/T_UI_BgSlipper"), TEXT("Materials/T_UI_FadeV"), TEXT("Icons/T_UI_Icon_Radio"), TEXT("Icons/T_UI_Icon_Carry") })
		{
			AntUi::Brush(P);
		}
	}
}

void UAntUiSubsystem::OnPreLoadMap(const FString& MapName)
{
	PreloadLoadingArt();
	if (AntUi::Classic() || IsRunningDedicatedServer() || !FSlateApplication::IsInitialized() || !IsMoviePlayerEnabled())
	{
		return;
	}
	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = true;
	Attr.bWaitForManualStop = false;
	Attr.MinimumLoadingScreenDisplayTime = 1.2f;
	Attr.WidgetLoadingScreen = SNew(SAntUiMenu).Screen(AntUiScreen::Loading);
	GetMoviePlayer()->SetupLoadingScreen(Attr);
}

bool UAntUiSubsystem::Tick(float Dt)
{
	AntUi::InstallCanvasFonts();   // no-op once done; covers a start where Slate came up after the game instance
	UGameInstance* GI = GetGameInstance();
	AAntPlayerController* PC = LocalPC(GI);
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	if (SeenWorld.Get() != World)
	{
		SeenWorld = World;
		WorldSeenAt = Now;
		FAntUiSettings::Get().Apply(PC);   // the mouse lives on the player controller: set it for each new one
	}

	// Co-op, before the first raid of this world: the squad muster opens by itself once.
	if (!AntUi::Classic() && World->GetNetMode() != NM_Standalone && LobbyShownIn.Get() != World && Now - WorldSeenAt > 1.5)
	{
		const AAntRaid* Raid = AAntRaid::Get(World);
		if (Raid && Raid->Phase == AntRaidPhase::Nest && Raid->RaidNo == 0 && !PC->MenuWidget.IsValid())
		{
			LobbyShownIn = World;
			PC->OpenUiScreen(AntUiScreen::Lobby);
		}
	}

	// Dev: one screen on request.
	FString Show;
	if (!bShowDone && Now - WorldSeenAt > 3.0 && FParse::Value(FCommandLine::Get(), TEXT("AntUiShow="), Show))
	{
		bShowDone = true;
		const uint8 S = ScreenByName(Show);
		if (S != 255) { PC->OpenUiScreen(S); if (S == AntUiScreen::Settings && Show.Len() > 8) { PC->SetUiSettingsTab(FCString::Atoi(*Show.RightChop(8))); } }
	}
	// Dev: a screenshot of each listed screen (shown 2.8 s, shot, then the next one 0.8 s later so the shot is of this screen).
	if (Shots.Num() > 0 && Now - WorldSeenAt > 6.0)
	{
		if (ShotIndex >= 0 && ShotIndex < Shots.Num() && !bShotTaken && Now - ShotAt > 2.8)
		{
			bShotTaken = true;
			const FString Name = FString::Printf(TEXT("ui_%02d_%s"), ShotIndex, *Shots[ShotIndex]);
			FScreenshotRequest::RequestScreenshot(Name, true, false);
			UE_LOG(LogTemp, Display, TEXT("AntUi: screenshot %s"), *Name);
		}
		if (ShotIndex < 0 || Now - ShotAt > 3.6)
		{
			++ShotIndex;
			ShotAt = Now;
			bShotTaken = false;
			if (ShotIndex < Shots.Num())
			{
				const FString& N = Shots[ShotIndex];
				const uint8 S = ScreenByName(N);
				if (S == 255) { PC->CloseMenu(); }   // "HUD": no menu
				else
				{
					PC->OpenUiScreen(S);
					if (S == AntUiScreen::Settings && N.Len() > 8) { PC->SetUiSettingsTab(FCString::Atoi(*N.RightChop(8))); }
				}
			}
			else
			{
				Shots.Reset();
				if (FParse::Param(FCommandLine::Get(), TEXT("AntUiShotsQuit"))) { UKismetSystemLibrary::QuitGame(PC, PC, EQuitPreference::Quit, false); }
			}
		}
	}
	return true;
}
