// The fighter's screens (docs/FIGHTER.md, "Экраны"), drawn on the Canvas over the nest in the war-room style (docs/UI_STYLE.md):
// the profile (rank and XP, the caste trees, the medals wall, the look), the veterans' roster and draft with the memorial wall,
// the post-raid highlights over the debrief, and the medal toasts in a raid. All local: what a click changes goes to the profile
// (and the loadout to the server) or to the server as AAntPlayerController::ServerVeteran.
#pragma once

#include "CoreMinimal.h"

class AHUD;
class UCanvas;
class AAntPlayerController;

struct FAntFighterToast
{
	FString Title;
	FString Line;
	FString Icon;
	uint32 Color = 0xF2A93B;
	/** 0 rank, 1 new medal, 2 a feat again / admin. */
	uint8 Kind = 0;
	double Time = 0.0;
};

namespace AntFighterUi
{
	namespace Screen { enum : uint8 { None, Profile, Roster }; }
	ANTKITCHEN_API TArray<FAntFighterToast>& Toasts();
	ANTKITCHEN_API bool IsOpen();
	ANTKITCHEN_API void Open(AAntPlayerController* PC, uint8 Which, int32 Tab);
	ANTKITCHEN_API void Close();
	/** Which caste the «Касты» tab shows (-1: the player's own). */
	ANTKITCHEN_API void SetCaste(int32 Caste);
	/** Over the nest (bResults false: the two buttons and the open screen) or the raid's results (the highlights). */
	ANTKITCHEN_API void DrawNest(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC, bool bResults);
	/** In a raid: the medal toasts (and the profile's own tick). */
	ANTKITCHEN_API void DrawRaid(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC);
	/** A medal as a bottle-cap badge: Dot texture, crimped rim, the icon, a strip of tape. */
	ANTKITCHEN_API void Badge(UCanvas* Cv, float X, float Y, float Size, const TCHAR* Icon, uint32 Color, bool bLocked, float TapeDeg = -12.f);
}
