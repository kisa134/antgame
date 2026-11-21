#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "AntUiSettings.h"

class AAntPlayerController;
class UAntSession;
struct FUiDraw;

/** The screens of the Slate front end (docs/UI_FLOW.md). */
namespace AntUiScreen { enum EId : uint8 { Boot, Main, Coop, Colony, Lobby, Settings, Credits, Pause, Loading, Count }; }

/**
 * Every menu screen in one immediate-mode widget: each paint lays the screen out, draws it and records the clickable items;
 * mouse, keyboard and gamepad work on that list (spatial focus, Enter / A, Esc / B, Left/Right for options and sliders).
 * Art comes from /Game/UI (AntUiStyle.h); with the art missing it still draws, in flat colours.
 */
class ANTKITCHEN_API SAntUiMenu : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAntUiMenu) : _Screen(AntUiScreen::Main) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AAntPlayerController>, PC)
		SLATE_ARGUMENT(uint8, Screen)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAntUiMenu();
	void Go(uint8 NewScreen);
	uint8 Current() const { return Screen; }
	void SetSettingsTab(int32 Tab) { SettingsTab = FMath::Clamp(Tab, 0, 3); }
	/** The studio card and the logo play once per launch. */
	static bool bBootShown;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.f, 1080.f); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** One clickable thing of the last paint. */
	struct FItem
	{
		FSlateRect R;
		int32 Id = 0;
		bool bEnabled = true;
		TFunction<void()> Click;
		TFunction<void(int32)> Adjust;      // Left / Right
		TFunction<void(float)> Drag;        // sliders: 0..1 under the mouse
	};

private:
	// ---- screens (AntUiMenu.cpp)
	void PaintBoot(FUiDraw& D) const;
	void PaintMain(FUiDraw& D) const;
	void PaintCoop(FUiDraw& D) const;
	void PaintColony(FUiDraw& D) const;
	void PaintLobby(FUiDraw& D) const;
	void PaintSettings(FUiDraw& D) const;
	void PaintCredits(FUiDraw& D) const;
	void PaintPause(FUiDraw& D) const;
	void PaintLoading(FUiDraw& D) const;
	void PaintBackdrop(FUiDraw& D, const TCHAR* Plate, float Dim, bool bDust) const;
	void PaintFooter(FUiDraw& D, bool bBack) const;

	// ---- widgets (immediate mode: drawn and registered in one call; true in the frame they are clicked is not needed here)
	void MenuButton(FUiDraw& D, const FString& Key, const FString& Label, float X, float Y, TFunction<void()> Fn, bool bEnabled = true) const;
	void Button(FUiDraw& D, const FString& Key, const FString& Label, float X, float Y, float W, float H, TFunction<void()> Fn, bool bPrimary = false, bool bEnabled = true, const TCHAR* IconName = nullptr) const;
	void OptionRow(FUiDraw& D, const FString& Key, const FString& Label, const FString& Value, float Y, TFunction<void(int32)> Adjust, const TCHAR* Help = nullptr) const;
	void SliderRow(FUiDraw& D, const FString& Key, const FString& Label, float Frac, const FString& Value, float Y, TFunction<void(float)> Set) const;
	void ToggleRow(FUiDraw& D, const FString& Key, const FString& Label, bool bOn, float Y, TFunction<void()> Flip) const;
	void EditField(FUiDraw& D, const FString& Key, FString* Value, float X, float Y, float W, float H, const FString& Hint) const;
	void Tabs(FUiDraw& D, const TArray<FString>& Names, int32 Active, float Y, TFunction<void(int32)> Pick) const;
	int32 AddItem(const FUiDraw& D, const FString& Key, float X, float Y, float W, float H, bool bEnabled, TFunction<void()> Click, TFunction<void(int32)> Adjust = nullptr, TFunction<void(float)> Drag = nullptr) const;
	float Anim(int32 Id) const;
	bool Hot(int32 Id) const { return Id == FocusId; }

	// ---- input
	void MoveFocus(const FVector2D& Dir);
	void Activate();
	void Back();
	const FItem* ItemAt(const FVector2D& Local) const;
	const FItem* FindItem(int32 Id) const;

	// ---- actions
	UAntSession* Session() const;
	void CloseOrBack();
	void SettingsBegin();
	void SettingsApply();
	void SettingsRevert();

	TWeakObjectPtr<AAntPlayerController> PC;
	uint8 Screen = AntUiScreen::Main;
	uint8 ReturnTo = AntUiScreen::Main;
	double ScreenStart = 0.0;
	double Now = 0.0;
	mutable TArray<FItem> Items;
	mutable int32 FocusId = 0;
	mutable TMap<int32, float> Anims;
	mutable FVector2D LocalSize = FVector2D(1920.f, 1080.f);
	FVector2D Mouse = FVector2D::ZeroVector;
	int32 DragId = 0;
	int32 EditId = 0;
	FString* EditTarget = nullptr;
	FString EditBefore;
	mutable int32 SettingsTab = 0;
	float CreditsScroll = 0.f;
	int32 LoadingArt = 0;
	int32 LoadingTip = 0;
	double NewColonyArmed = -100.0;
	FString JoinAddress;
	FString SeedText;
	// The settings screen edits copies; Apply writes them.
	FAntUiSettings EditUi;
	FAntGfx EditGfx;
	FAntUiSettings SavedUi;
	FAntGfx SavedGfx;
	bool bWorldHidden = false;
};
