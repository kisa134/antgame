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
	void DrawWar(AAntGameState* GS, APawn* Me);
	void DrawTrophies(AAntGameState* GS, APawn* Me);
	void DrawLinkState(AAntCharacter* Ant);

	// ---- The run (AntHUDRun.cpp): orders, cards, greed, the event, pings, first-shift hints, the interlude ----
	void DrawRun(AAntGameState* GS, APawn* Me);
	void DrawInterlude(AAntGameState* GS);
	/** Is this point near the crosshair (within Degrees of the view direction)? */
	bool Aimed(const FVector& World, float Degrees) const;
	/** 0..1: how visible the long controls lines are (they fade after the first minute; F1 brings them back). */
	float HelpAlpha() const;
	int32 SeenEvent = 0;
	float FirstDrawTime = -1.f;
	// First shift: three steps for a new player, ticked off as they happen (local).
	bool bHintOut = false;
	bool bHintGrab = false;
	bool bHintHome = false;
	float HintsDoneAt = -1.f;
	int32 HintShift = -1;

	/** The shift ("heist run"): quota bar, larvae, what the Human is up to. */
	void DrawShift(AAntGameState* GS);
	/** A big line across the screen for a few seconds (stage change, quota met). */
	void Announce(const FString& Title, const FString& Sub, const FLinearColor& Color);
	void DrawAnnounce();
	/** A soft coloured frame along the screen edges (damage, low health). */
	void EdgeGlow(const FLinearColor& Color, float Strength, float Depth);

	float S = 1.f;          // UI scale
	float LastHealth = -1.f;
	float HurtFlash = 0.f;
	float HeartPhase = 0.f;

	// What this machine has already seen of the shift, to notice changes (all local, nothing is sent anywhere).
	int32 SeenShift = -1;
	int32 SeenStage = -1;
	bool bSeenQuota = false;
	int32 SeenScore = -1;
	int32 SeenLarvae = -1;
	float ScorePulse = 0.f;     // 1 -> 0 after score was added
	int32 ScoreGain = 0;
	float GainFromFrac = 0.f;
	float LarvaPop = 0.f;       // 1 -> 0 after a larva was spent
	int32 LarvaPopCount = 0;
	FString AnnTitle;
	FString AnnSub;
	FLinearColor AnnColor = FLinearColor::White;
	float AnnTime = -100.f;

	// -AntFpsLog: average frame rate to the log every few seconds.
	double FpsTime = 0.0;
	int32 FpsFrames = 0;
	float FpsWorst = 0.f;
};
