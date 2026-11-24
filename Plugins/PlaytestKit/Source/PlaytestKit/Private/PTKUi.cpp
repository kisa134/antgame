#include "PTKUi.h"
#include "PlaytestKit.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FSlateFontInfo Font(int32 Size, bool bBold = false) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); }

	int32 DigitOf(const FKey& K)
	{
		static const FKey Row[10] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		static const FKey Pad[10] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
		for (int32 i = 0; i < 10; ++i) { if (K == Row[i] || K == Pad[i]) { return i; } }
		return -1;
	}

	TSharedRef<SWidget> Panel(const TSharedRef<SWidget>& Inner, const FLinearColor& Bg, float Pad = 10.f)
	{
		return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(Bg).Padding(Pad)[Inner];
	}

	TSharedRef<STextBlock> Label(const FString& T, int32 Size = 14, bool bBold = false, const FLinearColor& C = FLinearColor::White)
	{
		return SNew(STextBlock).Text(FText::FromString(T)).Font(Font(Size, bBold)).ColorAndOpacity(C).AutoWrapText(true);
	}
}

bool FPTKInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& E)
{
	bool bHandled = false;
	if (UPlaytestKitSubsystem* S = Owner.Get()) { S->WantsKey(E, bHandled); }
	return bHandled;
}

void FPTKUi::Add(const TSharedRef<SWidget>& W, int32 Z)
{
	if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->AddViewportWidgetContent(W, Z); }
}

void FPTKUi::Remove(TSharedPtr<SWidget>& W)
{
	if (W.IsValid())
	{
		if (GEngine && GEngine->GameViewport) { GEngine->GameViewport->RemoveViewportWidgetContent(W.ToSharedRef()); }
		W.Reset();
	}
}

void FPTKUi::EnterUiMode()
{
	if (bModeSaved) { return; }
	APlayerController* PC = Owner->GetLocalPC();
	if (!PC) { return; }
	ModePC = PC;
	bModeSaved = true;
	bPrevCursor = PC->bShowMouseCursor;
	PC->SetShowMouseCursor(true);
	FInputModeGameAndUI M;
	M.SetHideCursorDuringCapture(false);
	M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(M);
}

void FPTKUi::LeaveUiMode()
{
	if (!bModeSaved) { return; }
	bModeSaved = false;
	if (APlayerController* PC = ModePC.Get())
	{
		PC->SetShowMouseCursor(bPrevCursor);
		if (!bPrevCursor) { PC->SetInputMode(FInputModeGameOnly()); }
	}
	if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); }
}

void FPTKUi::CloseAll()
{
	Remove(ToastW); Remove(MarkerW); Remove(AskW); Remove(SurveyW); Remove(ConsentW);
	MarkerStep = 0;
	Sv.Reset();
	LeaveUiMode();
}

void FPTKUi::PreWarm()
{
	if (!FSlateApplication::IsInitialized()) { return; }
	TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	static const TCHAR* Sample = TEXT("Aa Bb 0123 Аа Бб Рейд Опрос — 1/5 F7 [Enter]");
	for (int32 Size : { 13, 14, 15, 16, 18, 20, 24 })
	{
		M->Measure(FString(Sample), Font(Size, false));
		M->Measure(FString(Sample), Font(Size, true));
	}
}

void FPTKUi::Tick(double Now)
{
	if (ToastW.IsValid() && Now > ToastEnds) { Remove(ToastW); }
	if (MarkerStep == 2 && NoteShowAt > 0.0 && Now >= NoteShowAt) { NoteShowAt = 0.0; ShowMarkerNote(); }
	if (MarkerW.IsValid() && Now > MarkerEnds) { CloseMarker(); }
	if (AskW.IsValid() && Now > AskEnds) { CloseAsk(false); }
}

void FPTKUi::Toast(const FString& Text, float Sec, const FLinearColor& C)
{
	if (!GEngine || !GEngine->GameViewport) { return; }
	Remove(ToastW);
	ToastW = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 90)).Visibility(EVisibility::SelfHitTestInvisible)
		[Panel(Label(Text, 15), C, 8.f)];
	Add(ToastW.ToSharedRef(), 9000);
	ToastEnds = FPlatformTime::Seconds() + Sec;
}

// ------------------------------------------------------------------------------------------------- marker

namespace
{
	struct FTag { const TCHAR* Id; const TCHAR* Ru; const TCHAR* En; FLinearColor C; };
	static const FTag GTags[4] = {
		{ TEXT("bug"), TEXT("Баг"), TEXT("Bug"), FLinearColor(0.75f, 0.15f, 0.15f, 1.f) },
		{ TEXT("confusing"), TEXT("Непонятно"), TEXT("Confusing"), FLinearColor(0.80f, 0.55f, 0.10f, 1.f) },
		{ TEXT("boring"), TEXT("Скучно"), TEXT("Boring"), FLinearColor(0.35f, 0.35f, 0.55f, 1.f) },
		{ TEXT("fun"), TEXT("Круто"), TEXT("Fun"), FLinearColor(0.15f, 0.65f, 0.25f, 1.f) },
	};
}

void FPTKUi::OpenMarker()
{
	if (!GEngine || !GEngine->GameViewport || !Owner) { return; }
	CloseMarker();
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 i = 0; i < 4; ++i)
	{
		const FString L = FString::Printf(TEXT("%d  %s"), i + 1, *Owner->Tr(GTags[i].Ru, GTags[i].En));
		const FString Id = GTags[i].Id;
		Row->AddSlot().AutoWidth().Padding(3)
		[
			SNew(SButton).ButtonColorAndOpacity(GTags[i].C)
			.OnClicked_Lambda([this, Id]() { PickMarker(Id); return FReply::Handled(); })
			[Label(L, 15, true)]
		];
	}
	MarkerW = SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 60, 20, 0)).Visibility(EVisibility::SelfHitTestInvisible)
	[
		Panel(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(Owner->Tr(TEXT("МАРКЕР: что сейчас?"), TEXT("MARKER: what is it?")), 14, true)]
			+ SVerticalBox::Slot().AutoHeight()[Row]
			+ SVerticalBox::Slot().AutoHeight()[Label(Owner->Tr(TEXT("цифра 1-4  |  Esc - отмена  |  игра не на паузе"), TEXT("press 1-4  |  Esc cancels  |  the game is not paused")), 11, false, FLinearColor(0.75f, 0.75f, 0.75f))],
			FLinearColor(0.04f, 0.04f, 0.06f, 0.88f))
	];
	Add(MarkerW.ToSharedRef(), 9100);
	MarkerStep = 1;
	MarkerEnds = FPlatformTime::Seconds() + 8.0;
}

void FPTKUi::PickMarker(const FString& TagId)
{
	if (!Owner || MarkerStep != 1) { return; }
	if (MarkerW.IsValid()) { MarkerW->SetVisibility(EVisibility::Collapsed); }   // the overlay must not be in the screenshot
	LastMarkerTag = TagId;
	Owner->AddMarker(TagId);
	LastMarkerSeq = Owner->Seq;
	MarkerStep = 2;
	NoteShowAt = FPlatformTime::Seconds() + 0.45;
	MarkerEnds = NoteShowAt + 4.0;
}

void FPTKUi::AutoAnswer()
{
	if (!Sv.IsValid()) { return; }
	int32 n = 0;
	for (const FPTKQuestion& Q : Sv->Def.Questions)
	{
		++n;
		if (Q.Type == FPTKQuestion::QScale) { Sv->Num.Add(Q.Id, FMath::Clamp(Q.Min + 2 + (n % 3), Q.Min, Q.Max)); }
		else if (Q.Type == FPTKQuestion::QChoice) { Sv->Num.Add(Q.Id, n % FMath::Max(1, Q.Options.Num())); }
		else { Sv->Str.Add(Q.Id, FString::Printf(TEXT("auto test answer %s"), *Q.Id)); }
	}
	CloseSurvey(true);
}

void FPTKUi::ShowMarkerNote()
{
	Remove(MarkerW);
	NoteBox.Reset();
	MarkerNote.Reset();
	MarkerW = SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 60, 20, 0)).Visibility(EVisibility::SelfHitTestInvisible)
	[
		Panel(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(Owner->Tr(TEXT("Маркер сохранён (скриншот сделан)."), TEXT("Marker saved (screenshot taken)."))
				, 14, true, FLinearColor(0.6f, 1.f, 0.6f))]
			+ SVerticalBox::Slot().AutoHeight()[Label(Owner->Tr(TEXT("Enter - добавить комментарий  |  иначе исчезнет"), TEXT("Enter - add a comment  |  or it fades")), 11, false, FLinearColor(0.75f, 0.75f, 0.75f))],
			FLinearColor(0.04f, 0.06f, 0.04f, 0.88f))
	];
	Add(MarkerW.ToSharedRef(), 9100);
	MarkerStep = 2;
	MarkerEnds = FPlatformTime::Seconds() + 4.0;
}

void FPTKUi::CloseMarker()
{
	Remove(MarkerW);
	NoteBox.Reset();
	if (MarkerStep == 3 && FSlateApplication::IsInitialized()) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); }
	MarkerStep = 0;
}

void FPTKUi::SubmitMarkerNote()
{
	if (NoteBox.IsValid())
	{
		const FString T = NoteBox->GetText().ToString().TrimStartAndEnd();
		if (!T.IsEmpty() && Owner)
		{
			Owner->Track(TEXT("marker_note"), FPTKProps().I("marker_seq", LastMarkerSeq).S("tag", LastMarkerTag).S("text", T), TEXT("marker"));
			Toast(Owner->Tr(TEXT("Комментарий сохранён"), TEXT("Comment saved")), 1.5f);
		}
	}
	CloseMarker();
}

// ------------------------------------------------------------------------------------------------- micro question

void FPTKUi::OpenAsk(const FPTKQuestion& Q, float TimeoutSec, const FString& SurveyId)
{
	if (!GEngine || !GEngine->GameViewport || !Owner) { return; }
	Remove(AskW);
	AskQ = Q;
	AskSurveyId = SurveyId;
	FString Hint;
	if (Q.Type == FPTKQuestion::QScale)
	{
		Hint = FString::Printf(TEXT("Alt+%d ... Alt+%d"), Q.Min, Q.Max >= 10 ? 0 : Q.Max);
		if (!Q.LowLabel.IsEmpty() || !Q.HighLabel.IsEmpty()) { Hint += FString::Printf(TEXT("   (%d = %s, %d = %s)"), Q.Min, *Q.LowLabel, Q.Max, *Q.HighLabel); }
	}
	else
	{
		for (int32 i = 0; i < Q.Options.Num(); ++i) { Hint += FString::Printf(TEXT("Alt+%d %s    "), i + 1, *Q.Options[i]); }
	}
	Hint += Owner->Tr(TEXT("   |   Alt+X - пропустить"), TEXT("   |   Alt+X - skip"));
	AskW = SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(20, 0, 0, 130)).Visibility(EVisibility::SelfHitTestInvisible)
	[
		Panel(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(Q.Text, 15, true)]
			+ SVerticalBox::Slot().AutoHeight()[Label(Hint, 12, false, FLinearColor(0.8f, 0.9f, 1.f))],
			FLinearColor(0.03f, 0.05f, 0.09f, 0.9f))
	];
	Add(AskW.ToSharedRef(), 9050);
	AskOpened = FPlatformTime::Seconds();
	AskEnds = AskOpened + TimeoutSec;
}

void FPTKUi::CloseAsk(bool bAnswered)
{
	if (!AskW.IsValid()) { return; }
	if (!bAnswered && Owner)
	{
		Owner->Track(TEXT("survey_skip"), FPTKProps().S("survey_id", AskSurveyId).S("qid", AskQ.Id).S("why", FPlatformTime::Seconds() >= AskEnds ? TEXT("timeout") : TEXT("skipped")), TEXT("survey"));
	}
	Remove(AskW);
}

// ------------------------------------------------------------------------------------------------- consent

void FPTKUi::OpenConsent()
{
	if (!GEngine || !GEngine->GameViewport || !Owner || ConsentW.IsValid()) { return; }
	const FString Body = Owner->Tr(
		TEXT("Это ВНУТРЕННИЙ плейтест. Пока ты играешь, игра записывает: события (смерти, награды, меню, настройки), FPS и пинг, позицию твоего персонажа, "
			"твои метки (F7) со скриншотом и ответы на опросы. НЕ записываются: имя, e-mail, звук, камера, содержимое других программ. "
			"Тебя отличает только случайный анонимный ID на этом компьютере. Данные видит только команда игры и нужны только для улучшения игры. "
			"Отказаться можно сейчас - игра останется полностью играбельной."),
		TEXT("This is an INTERNAL playtest. While you play the game records: events (deaths, rewards, menus, settings), FPS and ping, your character position, "
			"your markers (F7) with a screenshot, and your survey answers. NOT recorded: name, e-mail, audio, camera, other programs. "
			"You are identified only by a random anonymous ID on this machine. Only the game team sees the data, only to improve the game. "
			"You can decline now - the game stays fully playable."));
	ConsentW = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.82f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(820.f)
		[
			Panel(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[Label(Owner->Tr(TEXT("Плейтест: согласие на сбор данных"), TEXT("Playtest: data collection consent")), 22, true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[Label(Body, 15)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(4)
					[
						SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.15f, 0.6f, 0.25f)).OnClicked_Lambda([this]() { Owner->OnConsentAnswer(true); return FReply::Handled(); })
						[Label(Owner->Tr(TEXT("  Согласен (Enter)  "), TEXT("  I agree (Enter)  ")), 16, true)]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4)
					[
						SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f)).OnClicked_Lambda([this]() { Owner->OnConsentAnswer(false); return FReply::Handled(); })
						[Label(Owner->Tr(TEXT("  Не участвовать (Esc)  "), TEXT("  No thanks (Esc)  ")), 16, true)]
					]
				],
				FLinearColor(0.06f, 0.07f, 0.1f, 1.f), 18.f)
		]
	];
	Add(ConsentW.ToSharedRef(), 9500);
	EnterUiMode();
}

// ------------------------------------------------------------------------------------------------- survey window

void FPTKUi::OpenSurvey(const FPTKSurveyDef& Def, TFunction<void()> OnDone)
{
	if (!GEngine || !GEngine->GameViewport || !Owner || Sv.IsValid()) { if (OnDone) { OnDone(); } return; }
	Sv = MakeShared<FSurveyState>();
	Sv->Def = Def;
	Sv->Done = MoveTemp(OnDone);
	Sv->Opened = FPlatformTime::Seconds();
	BuildSurvey();
	EnterUiMode();
	Owner->Track(TEXT("survey_open"), FPTKProps().S("survey_id", Def.Id), TEXT("survey"));
}

void FPTKUi::BuildSurvey()
{
	Remove(SurveyW);
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	TWeakPtr<FSurveyState> WS = Sv;
	for (const FPTKQuestion& Q : Sv->Def.Questions)
	{
		const FString Id = Q.Id;
		FString Title = Q.Text;
		if (Q.bRequired) { Title += TEXT(" *"); }
		Col->AddSlot().AutoHeight().Padding(0, 10, 0, 3)[Label(Title, 15, true)];
		if (Q.Type == FPTKQuestion::QText)
		{
			Col->AddSlot().AutoHeight()
			[
				SNew(SBox).HeightOverride(64.f)
				[
					SNew(SMultiLineEditableTextBox).Font(Font(14)).AutoWrapText(true)
					.OnTextChanged_Lambda([WS, Id](const FText& T) { if (TSharedPtr<FSurveyState> S = WS.Pin()) { S->Str.Add(Id, T.ToString()); } })
				]
			];
			continue;
		}
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		const int32 N = Q.Type == FPTKQuestion::QScale ? (Q.Max - Q.Min + 1) : Q.Options.Num();
		for (int32 i = 0; i < N; ++i)
		{
			const int32 Val = Q.Type == FPTKQuestion::QScale ? Q.Min + i : i;
			const FString Txt = Q.Type == FPTKQuestion::QScale ? FString::FromInt(Val) : Q.Options[i];
			Row->AddSlot().AutoWidth().Padding(2)
			[
				SNew(SButton)
				.ButtonColorAndOpacity_Lambda([WS, Id, Val]() { TSharedPtr<FSurveyState> S = WS.Pin(); const int32* P = S.IsValid() ? S->Num.Find(Id) : nullptr; return (P && *P == Val) ? FLinearColor(0.15f, 0.65f, 0.3f) : FLinearColor(0.35f, 0.35f, 0.4f); })
				.OnClicked_Lambda([WS, Id, Val]() { if (TSharedPtr<FSurveyState> S = WS.Pin()) { S->Num.Add(Id, Val); } return FReply::Handled(); })
				[SNew(SBox).MinDesiredWidth(Q.Type == FPTKQuestion::QScale ? 26.f : 0.f).HAlign(HAlign_Center)[Label(Txt, 14, true)]]
			];
		}
		if (Q.Type == FPTKQuestion::QScale && (!Q.LowLabel.IsEmpty() || !Q.HighLabel.IsEmpty()))
		{
			Row->AddSlot().AutoWidth().Padding(10, 0, 0, 0).VAlign(VAlign_Center)[Label(FString::Printf(TEXT("%s  ...  %s"), *Q.LowLabel, *Q.HighLabel), 11, false, FLinearColor(0.7f, 0.7f, 0.7f))];
		}
		Col->AddSlot().AutoHeight()[Row];
	}
	TSharedRef<SHorizontalBox> Btns = SNew(SHorizontalBox);
	Btns->AddSlot().AutoWidth().Padding(4)
	[
		SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.15f, 0.6f, 0.25f)).OnClicked_Lambda([this]() { CloseSurvey(true); return FReply::Handled(); })
		[Label(Owner->Tr(TEXT("  Отправить  "), TEXT("  Submit  ")), 16, true)]
	];
	Btns->AddSlot().AutoWidth().Padding(4)
	[
		SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f)).OnClicked_Lambda([this]() { CloseSurvey(false); return FReply::Handled(); })
		[Label(Owner->Tr(TEXT("  Пропустить (Esc)  "), TEXT("  Skip (Esc)  ")), 16, true)]
	];
	Btns->AddSlot().AutoWidth().Padding(14, 0, 0, 0).VAlign(VAlign_Center)
	[
		SNew(STextBlock).Text_Lambda([WS]() { TSharedPtr<FSurveyState> S = WS.Pin(); return FText::FromString(S.IsValid() ? S->Warn : FString()); }).Font(Font(13)).ColorAndOpacity(FLinearColor(1.f, 0.5f, 0.4f))
	];
	SurveyW = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.86f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
	[
		SNew(SBox).WidthOverride(900.f).MaxDesiredHeight(900.f)
		[
			Panel(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Label(Sv->Def.Title, 22, true)]
				+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Col]]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[Btns],
				FLinearColor(0.06f, 0.07f, 0.1f, 1.f), 18.f)
		]
	];
	Add(SurveyW.ToSharedRef(), 9400);
}

void FPTKUi::CloseSurvey(bool bSubmitted)
{
	if (!Sv.IsValid()) { return; }
	if (bSubmitted)
	{
		for (const FPTKQuestion& Q : Sv->Def.Questions)
		{
			if (Q.bRequired && !Sv->Num.Contains(Q.Id) && Sv->Str.FindRef(Q.Id).TrimStartAndEnd().IsEmpty())
			{
				Sv->Warn = Owner->Tr(TEXT("Ответь на вопросы со звёздочкой *"), TEXT("Please answer the questions marked *"));
				return;
			}
		}
		for (const FPTKQuestion& Q : Sv->Def.Questions)
		{
			if (Q.Type == FPTKQuestion::QText)
			{
				const FString T = Sv->Str.FindRef(Q.Id).TrimStartAndEnd();
				if (!T.IsEmpty()) { Owner->SurveyAnswered(Sv->Def.Id, Q, T, 0.0); }
			}
			else if (const int32* V = Sv->Num.Find(Q.Id))
			{
				Owner->SurveyAnswered(Sv->Def.Id, Q, Q.Type == FPTKQuestion::QChoice ? Q.Options[*V] : FString::FromInt(*V), (double)*V);
			}
		}
		Owner->Track(TEXT("survey_submit"), FPTKProps().S("survey_id", Sv->Def.Id).N("seconds", FPlatformTime::Seconds() - Sv->Opened), TEXT("survey"));
	}
	else
	{
		Owner->Track(TEXT("survey_skip"), FPTKProps().S("survey_id", Sv->Def.Id).S("why", "skipped"), TEXT("survey"));
	}
	TFunction<void()> Done = MoveTemp(Sv->Done);
	Remove(SurveyW);
	Sv.Reset();
	LeaveUiMode();
	if (bSubmitted) { Toast(Owner->Tr(TEXT("Спасибо! Ответы сохранены."), TEXT("Thanks! Answers saved.")), 2.5f); }
	if (Done) { Done(); }
}

// ------------------------------------------------------------------------------------------------- keys

bool FPTKUi::HandleKey(const FKeyEvent& E)
{
	const FKey K = E.GetKey();
	if (ConsentW.IsValid())
	{
		if (K == EKeys::Enter || K == EKeys::SpaceBar) { Owner->OnConsentAnswer(true); }
		else if (K == EKeys::Escape) { Owner->OnConsentAnswer(false); }
		return true;
	}
	if (Sv.IsValid())
	{
		if (K == EKeys::Escape) { CloseSurvey(false); return true; }
		return false;   // typing goes to the text boxes
	}
	if (MarkerStep == 3)
	{
		if (K == EKeys::Enter) { SubmitMarkerNote(); return true; }
		if (K == EKeys::Escape) { CloseMarker(); return true; }
		return false;
	}
	if (MarkerStep == 2)
	{
		if (K == EKeys::Enter && !E.IsRepeat())
		{
			// comment box
			Remove(MarkerW);
			NoteBox = SNew(SEditableTextBox).Font(Font(15)).HintText(FText::FromString(Owner->Tr(TEXT("коротко: что случилось"), TEXT("short: what happened"))));
			MarkerW = SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 60, 20, 0)).Visibility(EVisibility::SelfHitTestInvisible)
			[
				SNew(SBox).WidthOverride(460.f)
				[Panel(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Label(Owner->Tr(TEXT("Комментарий (Enter - сохранить, Esc - отмена)"), TEXT("Comment (Enter saves, Esc cancels)")), 13, true)]
					+ SVerticalBox::Slot().AutoHeight()[NoteBox.ToSharedRef()], FLinearColor(0.04f, 0.04f, 0.06f, 0.92f))]
			];
			Add(MarkerW.ToSharedRef(), 9100);
			MarkerStep = 3;
			MarkerEnds = FPlatformTime::Seconds() + 40.0;
			if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().SetKeyboardFocus(NoteBox, EFocusCause::SetDirectly); }
			return true;
		}
		if (K == EKeys::Escape) { CloseMarker(); return true; }
	}
	if (MarkerStep == 1)
	{
		const int32 D = DigitOf(K);
		if (D >= 1 && D <= 4) { PickMarker(GTags[D - 1].Id); return true; }
		if (K == EKeys::Escape || K == Owner->MarkerKeyCached) { CloseMarker(); return true; }
		return false;
	}
	if (AskW.IsValid() && E.IsAltDown())
	{
		const int32 D = DigitOf(K);
		if (K == EKeys::X) { CloseAsk(false); return true; }
		if (D >= 0)
		{
			const int32 N = D == 0 ? 10 : D;
			if (AskQ.Type == FPTKQuestion::QScale && N >= AskQ.Min && N <= AskQ.Max)
			{
				Owner->SurveyAnswered(AskSurveyId, AskQ, FString::FromInt(N), (double)N);
				CloseAsk(true);
				return true;
			}
			if (AskQ.Type == FPTKQuestion::QChoice && N >= 1 && N <= AskQ.Options.Num())
			{
				Owner->SurveyAnswered(AskSurveyId, AskQ, AskQ.Options[N - 1], (double)(N - 1));
				CloseAsk(true);
				return true;
			}
		}
	}
	if (K == Owner->MarkerKeyCached && !E.IsRepeat() && !E.IsAltDown() && !E.IsControlDown())
	{
		OpenMarker();
		return true;
	}
	return false;
}
