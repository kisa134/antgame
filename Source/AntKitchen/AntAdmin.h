#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "AntAdmin.generated.h"

class AHUD;
class UCanvas;
class SWidget;
class APlayerController;
class AAntPlayerController;

/**
 * Admin mode (docs/ADMIN.md). F10 opens a panel (Slate, mouse) in any solo or host game; -AntAdmin opens it at the start and
 * shows the badge. Every button and the command line send one text command to the server (AAntPlayerController::ServerAdmin),
 * which runs it through AAntKitchenGameMode::AdminCommand (AntAdmin.cpp) and, for anything it does not know, through the dev
 * command set of -AntDo (AntRun.cpp DevCommand and the branch commands). Only the host's own controller is obeyed: a client's
 * request is refused on the server. Every action is logged with the prefix ANTADMIN.
 */

/** The little that every machine must know of the admin's doings (replicated): a forced time of day, the "show everyone" view. */
UCLASS()
class ANTKITCHEN_API AAntAdminState : public AInfo
{
	GENERATED_BODY()

public:
	AAntAdminState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	static AAntAdminState* Get(const UWorld* World, bool bCreate = false);

	/** -1: the seed's own time of day; 0 day, 1 evening, 2 night (lights on every machine). */
	UPROPERTY(Replicated) int8 Tod = -1;
	/** Markers over every NPC, red ant and big piece of food, through walls. */
	UPROPERTY(Replicated) bool bReveal = false;
	UPROPERTY(Replicated) bool bNpcFrozen = false;
	/** The last thing the admin did, for the panel. */
	UPROPERTY(Replicated) FString Last;
	int8 AppliedTod = -2;
};

namespace AntAdmin
{
	/** -AntAdmin on the command line. */
	ANTKITCHEN_API bool Launched();
	/** May this (local) controller use the admin panel: solo, or the host of a listen server. */
	ANTKITCHEN_API bool CanUse(const APlayerController* PC);
	/** The panel widget for this controller (made by AAntPlayerController::ToggleAdmin). */
	ANTKITCHEN_API TSharedRef<SWidget> MakePanel(AAntPlayerController* PC);
	/** Keyboard focus to the panel's command line. */
	ANTKITCHEN_API void FocusPanel(const TSharedPtr<SWidget>& Panel);
	/** AAntHUD::DrawHUD: the АДМИН badge, the "show everyone" markers. */
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Canvas, AAntPlayerController* PC);
	/** Every command the command line knows, with a line of help each. */
	ANTKITCHEN_API const TArray<FString>& Help();
	/** The admin has been used on this machine (the badge stays). */
	ANTKITCHEN_API void MarkUsed();
}
