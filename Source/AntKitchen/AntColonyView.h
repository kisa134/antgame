#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;
class AAntRaid;
class AAntGameState;
class AAntPlayerController;

/**
 * The colony screen «Муравейник» (docs/COLONY.md): the nest between raids as a living cutaway of the wall crack, drawn on the
 * Canvas like the rest of the HUD. Chambers along tunnels, ants walking them, the Queen on her throne, the stores piling up by
 * kind, larvae in the nursery, trophies on the walls, the names of the fallen, the chronicle. Click a chamber: its sheet with the
 * next tier. After every raid it plays the post-raid show (10-20 s, skippable). Everything it does goes to the server as
 * AAntPlayerController::ServerRaid, like the old sheet (AntHUDNest.cpp), which stays one click away ("ШТАБ").
 * Clients see the host's colony read-only.
 */
namespace AntColonyView
{
	ANTKITCHEN_API void Draw(AHUD* Hud, UCanvas* Cv, AAntRaid* Raid, AAntGameState* GS, AAntPlayerController* PC);
	/** Which screen the nest phase shows on this machine: the colony (true) or the planning sheet (false). */
	ANTKITCHEN_API bool IsOn();
	ANTKITCHEN_API void SetOn(bool bOn);
	/**
	 * This machine: the nest phase shows the colony screen (it is the default; the end of every raid switches to it and plays
	 * the post-raid show by itself). Chamber >= 0 (AntColony::EChamber) also opens that chamber's card, e.g. Stores for a tutorial.
	 */
	ANTKITCHEN_API void Open(int32 Chamber = -1);

	// ---- dev / admin / tests (this machine only)
	/** Opens a chamber's sheet (-1 closes it). */
	ANTKITCHEN_API void DevOpen(int32 Chamber);
	/** Zoom 1..2.5 around a point of the cutaway (0..1 of its width and height). */
	ANTKITCHEN_API void DevZoom(float Zoom, float X, float Y);
	/** The Queen falls asleep now (false: wakes her and restarts the idle clock). */
	ANTKITCHEN_API void DevSleep(bool bAsleep = true);
	/** Jumps the post-raid show to a moment (seconds from its start); a negative value stops it. */
	ANTKITCHEN_API void DevShowAt(float Seconds);
	/** Frame time of the screen and what it drew, for the log. */
	ANTKITCHEN_API FString DevStats();
}
