#pragma once

#include "CoreMinimal.h"

class AAntCharacter;
class AController;
class APlayerController;
struct FAntUiSettings;
struct FAntGfx;

/**
 * Ant Kitchen <-> PlaytestKit adapter (Plugins/PlaytestKit, docs/PLAYTEST.md, docs/FEATURES.md section 39).
 * The game files carry only one-line hooks into this namespace; everything else is read from existing delegates
 * (AntMetaBus::OnRaidSummary, AntCarry::Events), from the replicated state (AAntRaid phase / result) and from the game's own
 * ANTxxx log lines (a table in AntPlaytest.cpp maps them to schema events; Development builds only, a Shipping build has no such logs).
 * Every call is a no-op when the kit is off (-PTKOff, consent declined).
 */
namespace AntPlaytest
{
	/** Server: an ant died (AAntKitchenGameMode::OnAntKilled): cause, killer, where. */
	ANTKITCHEN_API void OnDeath(AAntCharacter* Ant, AController* Killer, AActor* Causer);
	/** The tutorial's telemetry row (AAntTutorial::Telemetry): start / stage / done / skipped / fail / assist / finish / death. */
	ANTKITCHEN_API void OnTutorialTelemetry(const TCHAR* What, const TCHAR* BeatId, int32 BeatNo, float Seconds, float Par, int32 Retries, int32 Assists, int32 Players);
	/** The menu changed screen (SAntUiMenu::Go). */
	ANTKITCHEN_API void OnUiScreen(uint8 From, uint8 To);
	/** The settings screen applied (SAntUiMenu::SettingsApply): one setting_change event per changed value. */
	ANTKITCHEN_API void OnSettingsApplied(const FAntUiSettings& OldUi, const FAntUiSettings& NewUi, const FAntGfx& OldGfx, const FAntGfx& NewGfx);
	/** The main menu's Quit: true = the post-session survey is shown first and the game quits when it closes (the caller does nothing). */
	ANTKITCHEN_API bool InterceptQuit(APlayerController* PC);
}
