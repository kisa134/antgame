#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;
class APlayerController;
class AAntKitchenGameMode;

/**
 * KAN-29: the corner map of the flat, like GTA's. Top left during a raid, under the place line. It shows the floor plan of the
 * districts (level v3's table), the walls of the house, the nest and the south entrance, the cake once the team knows where it is,
 * every player of the colony as an arrow in his own colour (AntUi::PlayerColor: the lobby's colour, colour-blind safe when that
 * setting is on), his squad's workers as small dots of the same colour (AntSquad::GetSquadPositions) and the pheromone trails
 * (AntTrail::GetTrails). Everything it reads is replicated already, so a guest sees what the host sees.
 * [I] cycles the mode: near (turns with the camera, the player in the middle) / the whole flat (north up) / hidden. Local only.
 */
namespace AntMinimap
{
	enum EMode : int32 { Near = 0, Whole = 1, Off = 2, Count = 3 };

	/** S: the HUD's scale (AAntHUD::S). Call once a frame in a raid; reads the [I] key itself. */
	ANTKITCHEN_API void Draw(AHUD* Hud, UCanvas* Cv, APlayerController* PC, float S);
	ANTKITCHEN_API int32& Mode();
	/** Dev (-AntDo): minimap 0|1|2 (the host's mode), minimap info (what the host's map shows, to the log). */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
