#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "InputCoreTypes.h"
#include "Engine/DeveloperSettings.h"
#include "PlaytestKit.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPTK, Log, All);

class FPlaytestKitModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

/**
 * PlaytestKit settings: Config/DefaultPlaytestKit.ini, section [/Script/PlaytestKit.PlaytestKitSettings].
 * Command line overrides: -PTKOff, -PTKEndpoint=URL, -PTKKey=KEY, -PTKConsent=yes|no, -PTKScript="sec:cmd;sec:cmd".
 */
UCLASS(config = PlaytestKit, defaultconfig, meta = (DisplayName = "PlaytestKit"))
class PLAYTESTKIT_API UPlaytestKitSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPlaytestKitSettings() { CategoryName = TEXT("Plugins"); }

	/** Master switch (also -PTKOff on the command line). */
	UPROPERTY(Config, EditAnywhere, Category = "General") bool bEnabled = true;
	/** Project id written into every batch (a slug: "ant-kitchen"). */
	UPROPERTY(Config, EditAnywhere, Category = "General") FString ProjectId = TEXT("unknown");
	/** Free text build version; empty: the project version from Project Settings. */
	UPROPERTY(Config, EditAnywhere, Category = "General") FString BuildVersion;
	/** Git commit; stamped by PlaytestKit/tools/stamp_build.ps1 before packaging. */
	UPROPERTY(Config, EditAnywhere, Category = "General") FString GitSha;
	/** "ru" or "en": language of the plugin's own overlays. */
	UPROPERTY(Config, EditAnywhere, Category = "General") FString Language = TEXT("ru");

	/** Ingest URL (Supabase Edge Function .../functions/v1/ingest). Empty: local files only, nothing is sent. */
	UPROPERTY(Config, EditAnywhere, Category = "Upload") FString Endpoint;
	/** Value of the x-ptk-key header (shared secret of the ingest function). Not a user secret: it ships with the build. */
	UPROPERTY(Config, EditAnywhere, Category = "Upload") FString IngestKey;
	UPROPERTY(Config, EditAnywhere, Category = "Upload") bool bUploadScreenshots = true;
	UPROPERTY(Config, EditAnywhere, Category = "Upload") int32 UploadBatchKB = 128;
	UPROPERTY(Config, EditAnywhere, Category = "Upload") float UploadIntervalSec = 5.f;

	/** Ask for consent on the first launch (the answer is kept in Saved/Playtest/consent.json). */
	UPROPERTY(Config, EditAnywhere, Category = "Privacy") bool bRequireConsent = true;
	/** Hash the Steam id (SHA1 + salt) into player.steam when the game provides one. */
	UPROPERTY(Config, EditAnywhere, Category = "Privacy") bool bHashSteamId = false;
	UPROPERTY(Config, EditAnywhere, Category = "Privacy") FString SteamSalt = TEXT("ptk");

	UPROPERTY(Config, EditAnywhere, Category = "Capture") float PerfIntervalSec = 5.f;
	UPROPERTY(Config, EditAnywhere, Category = "Capture") float NetIntervalSec = 5.f;
	UPROPERTY(Config, EditAnywhere, Category = "Capture") float BreadcrumbIntervalSec = 2.f;
	UPROPERTY(Config, EditAnywhere, Category = "Capture") float HitchMs = 100.f;
	/** Stop recording hitch/breadcrumb events beyond this rate (events per second, whole kit). */
	UPROPERTY(Config, EditAnywhere, Category = "Capture") int32 MaxEventsPerSec = 300;

	UPROPERTY(Config, EditAnywhere, Category = "Markers") FKey MarkerKey = EKeys::F7;
	UPROPERTY(Config, EditAnywhere, Category = "Markers") bool bMarkerScreenshot = true;

	UPROPERTY(Config, EditAnywhere, Category = "Storage") int32 MaxLocalMB = 300;
	UPROPERTY(Config, EditAnywhere, Category = "Storage") int32 MaxLocalSessions = 30;
	UPROPERTY(Config, EditAnywhere, Category = "Storage") int32 MaxSessionMB = 60;

	/** Features the post-session survey asks to rate 1-5 (the game may also call SetSurveyFeatures). */
	UPROPERTY(Config, EditAnywhere, Category = "Survey") TArray<FString> SurveyFeatures;
};
