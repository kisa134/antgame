#pragma once

#include "CoreMinimal.h"
#include "AntUiStyle.h"

class UCanvas;
class AHUD;
class UTexture2D;
class APlayerController;
class AAntGameState;
struct FAntRaidResult;

/**
 * The Canvas half of the UI (the HUD draws on the Canvas): textured panels, icons, text in the UI fonts, and the two big Canvas
 * screens: the snowball card pick and the raid debrief ("Боевое донесение"). Everything degrades to flat colours without art.
 */
namespace AntUiCanvas
{
	ANTKITCHEN_API void Rect(UCanvas* Cv, float X, float Y, float W, float H, const FLinearColor& Col);
	ANTKITCHEN_API void Tile(UCanvas* Cv, UTexture2D* Tex, float X, float Y, float W, float H, const FLinearColor& Tint, const FVector2D& UV0 = FVector2D(0.f, 0.f), const FVector2D& UV1 = FVector2D(1.f, 1.f), float RotDeg = 0.f);
	/** Text in a UI font. Pt: point size already multiplied by the screen scale. Align 0 left, 1 centre, 2 right. Returns the size. */
	ANTKITCHEN_API FVector2D Text(UCanvas* Cv, const FString& S, float X, float Y, AntUi::EFont F, float Pt, const FLinearColor& Col, int32 Align = 0, float Shadow = 0.f);
	ANTKITCHEN_API FVector2D TextSize(UCanvas* Cv, const FString& S, AntUi::EFont F, float Pt);
	/** Shrinks to MaxW. */
	ANTKITCHEN_API FVector2D TextFit(UCanvas* Cv, const FString& S, float X, float Y, float MaxW, AntUi::EFont F, float Pt, const FLinearColor& Col, int32 Align = 0, float Shadow = 0.f);
	ANTKITCHEN_API float Wrap(UCanvas* Cv, const FString& S, float X, float Y, float MaxW, AntUi::EFont F, float Pt, const FLinearColor& Col, int32 MaxLines = 6, float LineH = 1.25f, int32 VisibleChars = -1);
	ANTKITCHEN_API void Paper(UCanvas* Cv, float X, float Y, float W, float H, bool bTape = true);
	ANTKITCHEN_API void Glass(UCanvas* Cv, float X, float Y, float W, float H, float Alpha = 0.78f);
	ANTKITCHEN_API void Frame(UCanvas* Cv, float X, float Y, float W, float H, float Th, const FLinearColor& Col);
	/** A texture cropped to fill a rectangle (keeps its aspect). */
	ANTKITCHEN_API void Cover(UCanvas* Cv, UTexture2D* Tex, float X, float Y, float W, float H, const FLinearColor& Tint);
	ANTKITCHEN_API void Icon(UCanvas* Cv, const TCHAR* Name, float X, float Y, float Size, const FLinearColor& Tint = FLinearColor::White);
	ANTKITCHEN_API void Stamp(UCanvas* Cv, const FString& S, float CX, float CY, float Deg, const FLinearColor& Col, float Pt);
	/** A HUD bar: ink track, fill, a light edge on top, ticks every 10 %. */
	ANTKITCHEN_API void Bar(UCanvas* Cv, float X, float Y, float W, float H, float Frac, const FLinearColor& Fill);

	/** The snowball pick: three illustrated cards. Fills OutRects (screen space) for clicks. Now / OfferEnds: server time. */
	ANTKITCHEN_API void DrawCardPick(UCanvas* Cv, const TArray<uint8>& Offer, float OfferEnds, float Now, int32 PickNo, const FVector2D& Mouse, bool bMouseOn, FBox2D OutRects[3]);
	/** The raid debrief as a newsreel + a typed field report. SecondsLeft: until the nest; bWon picks the plate. */
	ANTKITCHEN_API void DrawDebrief(UCanvas* Cv, UWorld* World, const FAntRaidResult& R, const AAntGameState* GS, int32 SecondsLeft);
	/** Dev: -AntUiTest=cards | debrief | debrieflose draws the screen with made-up data over whatever is on screen. */
	ANTKITCHEN_API void DrawTests(AHUD* Hud, UCanvas* Cv);
	/** Rarity colour of a snowball pick (by its pool). */
	ANTKITCHEN_API FLinearColor PickRarity(uint8 PickId, FString* OutName = nullptr);
}
