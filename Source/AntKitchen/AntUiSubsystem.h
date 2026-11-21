#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "AntUiSubsystem.generated.h"

/**
 * The front end's housekeeping: fonts and settings at start, the loading screen around every map load, the lobby that opens
 * by itself in co-op before the first raid, and the dev switches for screenshots:
 *   -AntUiShow=Main|Coop|Colony|Lobby|Settings[0-3]|Credits|Pause|Loading   open that screen once the world is up
 *   -AntUiShots=Main,Settings1,Lobby,...   open each in turn, take a screenshot (Saved/Screenshots), -AntUiShotsQuit quits after
 */
UCLASS()
class ANTKITCHEN_API UAntUiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float Dt);
	void OnPreLoadMap(const FString& MapName);
	FTSTicker::FDelegateHandle TickHandle;
	TWeakObjectPtr<UWorld> LobbyShownIn;
	TArray<FString> Shots;
	int32 ShotIndex = -1;
	double ShotAt = 0.0;
	double WorldSeenAt = 0.0;
	TWeakObjectPtr<UWorld> SeenWorld;
	bool bShowDone = false;
	bool bShotTaken = false;
};
