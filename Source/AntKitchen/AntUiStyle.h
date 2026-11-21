#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class UTexture2D;
class UFont;

/**
 * The look of every screen (docs/UI_STYLE.md): the "kitchen war-room" palette, the fonts (OFL, Content/UI/Fonts), the textures
 * made with Higgsfield (/Game/UI/...), the one table of strings (RU/EN) and the settings that change how the UI is drawn.
 * Slate menus (AntUiMenu.cpp) and the Canvas HUD (AntUiCanvas.cpp) both draw from here.
 */
namespace AntUi
{
	/** sRGB hex -> linear colour (what both Slate and the Canvas expect). */
	ANTKITCHEN_API FLinearColor Hex(uint32 RGB, float A = 1.f);
	ANTKITCHEN_API FLinearColor WithA(const FLinearColor& C, float A);

	namespace C
	{
		extern ANTKITCHEN_API const FLinearColor Ink;        // dark chocolate: backdrops
		extern ANTKITCHEN_API const FLinearColor InkSoft;    // panels on dark
		extern ANTKITCHEN_API const FLinearColor Cocoa;      // cardboard shadow, outlines
		extern ANTKITCHEN_API const FLinearColor Amber;      // the colony's colour: accents, focus
		extern ANTKITCHEN_API const FLinearColor AmberDeep;
		extern ANTKITCHEN_API const FLinearColor Honey;      // hover / highlight text
		extern ANTKITCHEN_API const FLinearColor Sugar;      // text on dark
		extern ANTKITCHEN_API const FLinearColor Paper;      // receipt paper
		extern ANTKITCHEN_API const FLinearColor PaperShade;
		extern ANTKITCHEN_API const FLinearColor Graphite;   // text on paper
		extern ANTKITCHEN_API const FLinearColor Muted;      // secondary text on dark
		extern ANTKITCHEN_API const FLinearColor Leaf;       // good, health, medic
		extern ANTKITCHEN_API const FLinearColor LeafDeep;
		extern ANTKITCHEN_API const FLinearColor Jam;        // danger, stamps
		extern ANTKITCHEN_API const FLinearColor JamDeep;
		extern ANTKITCHEN_API const FLinearColor Acid;
		extern ANTKITCHEN_API const FLinearColor Jelly;      // royal jelly
		extern ANTKITCHEN_API const FLinearColor JellyDeep;
		extern ANTKITCHEN_API const FLinearColor Teal;       // cold accent: info, the Human's moves
		extern ANTKITCHEN_API const FLinearColor Common;     // card rarities
		extern ANTKITCHEN_API const FLinearColor Rare;
		extern ANTKITCHEN_API const FLinearColor Epic;
		extern ANTKITCHEN_API const FLinearColor Legendary;
	}

	enum class EFont : uint8 { Display, Body, Bold, Mono, Hand, Count };
	/** Slate font of a role at a size (points). */
	ANTKITCHEN_API FSlateFontInfo Font(EFont F, float Size);
	/** A runtime UFont of a role (for the Canvas HUD); made once per size and kept. */
	ANTKITCHEN_API UFont* CanvasFont(EFont F, int32 Size);
	/** Puts PT Sans Narrow Bold in place of the engine's Small/Medium/Large fonts (same height), so every Canvas line of the HUD
	 *  gets the new face without touching its code. -AntUiOldFonts keeps Roboto. Called once the game instance starts. */
	ANTKITCHEN_API void InstallCanvasFonts();

	/** A texture under /Game/UI ("Icons/T_UI_Icon_Cake"), or null when it is missing (callers draw a flat fallback). */
	ANTKITCHEN_API UTexture2D* Tex(const TCHAR* Path);
	ANTKITCHEN_API const FSlateBrush* Brush(const TCHAR* Path);
	ANTKITCHEN_API const FSlateBrush* White();
	ANTKITCHEN_API const FSlateBrush* Rounded(float Radius);
	/** A paper-tag button style for plain Slate buttons (the admin panel): paper, honey on hover, amber pressed. */
	ANTKITCHEN_API const FButtonStyle& ButtonStyle();
	/** "Icons/T_UI_Icon_" + Name. */
	ANTKITCHEN_API UTexture2D* Icon(const TCHAR* Name);
	ANTKITCHEN_API const FSlateBrush* IconBrush(const TCHAR* Name);

	/** -AntUiClassic: the old screens (plain menu, old card row, old results). */
	ANTKITCHEN_API bool Classic();
	/** The HUD scale setting (0.75 .. 1.5). */
	ANTKITCHEN_API float HudScale();
	/** 0 Russian, 1 English. */
	ANTKITCHEN_API int32 Lang();
	/** A string of the UI table (AntUiStrings.cpp) in the current language; the key itself when missing. */
	ANTKITCHEN_API const FString& L(const TCHAR* Key);
	/** The same with {0} {1} ... filled in. */
	ANTKITCHEN_API FString Lf(const TCHAR* Key, const FStringFormatOrderedArguments& Args);
	/** A player's colour as the UI shows it: his own, or a colour-blind-safe one by slot when that setting is on. */
	ANTKITCHEN_API FLinearColor PlayerColor(int32 Slot, const FLinearColor& Own);
	/** The portrait of a caste (EAntClass as int): forager Gruzd, soldier Zhvalov, scout Usik, barrel Tikhon. */
	ANTKITCHEN_API const TCHAR* CastePortrait(int32 Caste);
	ANTKITCHEN_API const FString& CasteName(int32 Caste);
	/** An icon name for a world marker label ("Торт", "Гнездо", ...), or null. */
	ANTKITCHEN_API const TCHAR* IconForLabel(const FString& Label);
}
