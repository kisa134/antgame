#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "PlaytestKitSubsystem.h"

class SWidget;
class SEditableTextBox;
class APlayerController;

/** Catches the marker key / overlay keys before the game sees them. */
class FPTKInputProcessor : public IInputProcessor
{
public:
	explicit FPTKInputProcessor(UPlaytestKitSubsystem* InOwner) : Owner(InOwner) {}
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("PlaytestKit"); }
	TWeakObjectPtr<UPlaytestKitSubsystem> Owner;
};

/** All the Slate overlays of the kit: toast, marker, micro question, survey window, consent. Immediate state, no UObjects. */
class FPTKUi : public TSharedFromThis<FPTKUi>
{
public:
	explicit FPTKUi(UPlaytestKitSubsystem* InOwner) : Owner(InOwner) {}
	~FPTKUi() { CloseAll(); }

	void Tick(double Now);
	void CloseAll();
	/** Loads the fonts / glyph shaping the overlays use, at startup, so the first F7 or survey does not hitch (~0.9 s measured on the first Cyrillic text). */
	void PreWarm();
	/** True: the key is consumed (the game does not see it). */
	bool HandleKey(const FKeyEvent& E);

	void Toast(const FString& Text, float Sec = 2.5f, const FLinearColor& C = FLinearColor(0.1f, 0.1f, 0.1f, 0.85f));
	void OpenMarker();
	bool MarkerOpen() const { return MarkerStep != 0; }
	void OpenAsk(const FPTKQuestion& Q, float TimeoutSec, const FString& SurveyId);
	void OpenSurvey(const FPTKSurveyDef& Def, TFunction<void()> OnDone);
	bool SurveyOpen() const { return Sv.IsValid(); }
	void OpenConsent();
	bool ConsentOpen() const { return ConsentW.IsValid(); }
	bool AskOpen() const { return AskW.IsValid(); }
	void AutoAnswer();
	void PickMarker(const FString& TagId);

private:
	struct FSurveyState
	{
		FPTKSurveyDef Def;
		TMap<FString, int32> Num;
		TMap<FString, FString> Str;
		TFunction<void()> Done;
		double Opened = 0.0;
		FString Warn;
	};
	void Add(const TSharedRef<SWidget>& W, int32 Z);
	void Remove(TSharedPtr<SWidget>& W);
	void EnterUiMode();
	void LeaveUiMode();
	void CloseMarker();
	void ShowMarkerNote();
	void SubmitMarkerNote();
	void CloseAsk(bool bAnswered);
	void CloseSurvey(bool bSubmitted);
	void BuildSurvey();

	UPlaytestKitSubsystem* Owner = nullptr;
	TSharedPtr<SWidget> ToastW, MarkerW, AskW, SurveyW, ConsentW;
	double ToastEnds = 0.0, MarkerEnds = 0.0, AskEnds = 0.0, AskOpened = 0.0;
	int32 MarkerStep = 0;          // 0 closed, 1 tags, 2 saved (Enter = comment), 3 typing
	int32 LastMarkerSeq = -1;
	double NoteShowAt = 0.0;
	FString LastMarkerTag, MarkerNote;
	TSharedPtr<SEditableTextBox> NoteBox;
	FPTKQuestion AskQ; FString AskSurveyId;
	TSharedPtr<FSurveyState> Sv;
	bool bModeSaved = false, bPrevCursor = false;
	TWeakObjectPtr<APlayerController> ModePC;
	friend class UPlaytestKitSubsystem;
};
