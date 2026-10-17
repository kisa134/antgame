#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AntHUD.generated.h"

class AAntCharacter;
class AHumanCharacter;
class AAntGameState;
class AAntPlayerController;
class UFont;

/** Canvas HUD: no UMG assets needed. */
UCLASS()
class ANTKITCHEN_API AAntHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Txt(const FString& S, float X, float Y, const FLinearColor& C, UFont* F, float Scale, bool bCenter = false);
	void TxtRight(const FString& S, float RightX, float Y, const FLinearColor& C, UFont* F, float Scale);
	void Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Fill);
	void Marker(const FVector& World, const FString& Label, const FLinearColor& C, bool bClampToEdges);
	void Crosshair();

	void DrawTop(AAntGameState* GS);
	void DrawFeed(AAntGameState* GS);
	void DrawAnt(AAntCharacter* Ant);
	void DrawHuman(AHumanCharacter* H, AAntGameState* GS);
	void DrawWorldTags(APawn* Me);
	void DrawToastsAndNumbers(AAntPlayerController* PC);
	void DrawScoreboard(AAntGameState* GS);
	void DrawHelp(APawn* Me);

	float S = 1.f;          // UI scale
	float LastHealth = -1.f;
	float HurtFlash = 0.f;
};
