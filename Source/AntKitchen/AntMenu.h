#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AAntPlayerController;
class UAntSession;
class SVerticalBox;

/** Main menu (solo / host / find friends' games) and the Esc pause menu. Plain Slate: the project has no UMG assets. */
class SAntMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAntMenu) : _bPause(false) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AAntPlayerController>, PC)
		SLATE_ARGUMENT(bool, bPause)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void Rebuild();
	UAntSession* Session() const;
	void AddLabel(const FString& Text, int32 Size, const FLinearColor& Color, float Pad = 4.f);
	void AddButton(const FString& Text, TFunction<void()> Fn, const FLinearColor& Color = FLinearColor(1.f, 0.72f, 0.2f));

	TWeakObjectPtr<AAntPlayerController> PC;
	bool bPause = false;
	TSharedPtr<SVerticalBox> Box;
};