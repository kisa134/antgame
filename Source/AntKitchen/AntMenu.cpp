#include "AntMenu.h"
#include "AntSession.h"
#include "AntPlayerController.h"
#include "AntGameState.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "InputCoreTypes.h"

void SAntMenu::Construct(const FArguments& InArgs)
{
	PC = InArgs._PC;
	bPause = InArgs._bPause;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.03f, 0.02f, 0.01f, bPause ? 0.7f : 0.9f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(620.f)
			[
				SAssignNew(Box, SVerticalBox)
			]
		]
	];

	if (UAntSession* S = Session())
	{
		S->OnChanged.AddSP(this, &SAntMenu::Rebuild);
	}
	Rebuild();
}

UAntSession* SAntMenu::Session() const
{
	UGameInstance* GI = PC.IsValid() ? PC->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAntSession>() : nullptr;
}

void SAntMenu::AddLabel(const FString& Text, int32 Size, const FLinearColor& Color, float Pad)
{
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, Pad)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Text))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", Size))
		.ColorAndOpacity(Color)
		.Justification(ETextJustify::Center)
		.AutoWrapText(true)
		.WrapTextAt(600.f)
	];
}

void SAntMenu::AddButton(const FString& Text, TFunction<void()> Fn, const FLinearColor& Color)
{
	Box->AddSlot().AutoHeight().Padding(0.f, 6.f)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(14.f, 12.f))
		.ButtonColorAndOpacity(Color)
		.OnClicked_Lambda([Fn]() { Fn(); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
			.ColorAndOpacity(FLinearColor::Black)
		]
	];
}

void SAntMenu::Rebuild()
{
	if (!Box.IsValid())
	{
		return;
	}
	Box->ClearChildren();
	UAntSession* S = Session();
	TWeakObjectPtr<AAntPlayerController> WeakPC = PC;
	const FLinearColor Gold(1.f, 0.78f, 0.25f);
	const FLinearColor Grey(0.75f, 0.75f, 0.75f);
	const FLinearColor Dim(0.55f, 0.55f, 0.55f);

	AddLabel(TEXT("ANT KITCHEN"), 46, Gold, 2.f);
	AddLabel(bPause ? TEXT("Пауза") : TEXT("Тащи еду. Не попадись под тапок."), 16, Grey, 2.f);
	{
		// The best run of this machine (saved when a run ends).
		int32 BestShifts = 0, BestHaul = 0;
		AAntGameState::LoadBestRun(BestShifts, BestHaul);
		if (BestShifts > 0 || BestHaul > 0)
		{
			AddLabel(FString::Printf(TEXT("Лучший забег: смен пройдено %d, добыча %d"), BestShifts, BestHaul), 15, Gold, 4.f);
		}
		else if (!bPause)
		{
			AddLabel(TEXT("Лучший забег: пока нет. Пройди первую смену"), 13, Dim, 4.f);
		}
	}

	if (S && !S->Status.IsEmpty())
	{
		AddLabel(S->Status, 14, FLinearColor(1.f, 0.9f, 0.6f), 10.f);
	}
	else
	{
		AddLabel(TEXT(" "), 10, Grey, 6.f);
	}

	if (bPause)
	{
		AddButton(TEXT("Продолжить"), [WeakPC]() { if (WeakPC.IsValid()) { WeakPC->CloseMenu(); } });
		if (S && S->IsSteam() && S->HasSession())
		{
			AddButton(TEXT("Пригласить друзей"), [WeakPC, S]() { if (WeakPC.IsValid()) { S->Invite(); } });
		}
		AddButton(TEXT("В главное меню"), [WeakPC, S]() { if (WeakPC.IsValid() && S) { S->ReturnToMenu(); } }, FLinearColor(0.8f, 0.8f, 0.8f));
	}
	else
	{
		AddButton(TEXT("Играть одному"), [WeakPC, S]() { if (WeakPC.IsValid() && S) { S->PlaySolo(); } });
		AddButton(TEXT("Создать игру (кооп)"), [WeakPC, S]() { if (WeakPC.IsValid() && S) { S->Host(); } });
		AddButton(S && S->bSearching ? TEXT("Ищу...") : TEXT("Найти игры друзей"), [WeakPC, S]() { if (WeakPC.IsValid() && S) { S->Find(); } });
		if (S)
		{
			for (int32 i = 0; i < S->Games.Num(); ++i)
			{
				AddButton(FString::Printf(TEXT("Войти:  %s"), *S->Games[i].Label), [WeakPC, S, i]() { if (WeakPC.IsValid()) { S->Join(i); } }, FLinearColor(0.55f, 0.9f, 0.45f));
			}
		}
	}

	AddButton(TEXT("Выйти из игры"), [WeakPC]()
	{
		if (WeakPC.IsValid())
		{
			UKismetSystemLibrary::QuitGame(WeakPC.Get(), WeakPC.Get(), EQuitPreference::Quit, false);
		}
	}, FLinearColor(0.8f, 0.8f, 0.8f));

	if (S)
	{
		AddLabel(S->IsSteam()
			? FString::Printf(TEXT("Steam: %s"), *S->Nickname())
			: TEXT("Steam не запущен: кооп недоступен. Запусти Steam и перезапусти игру."), 12, S->IsSteam() ? Dim : FLinearColor(1.f, 0.45f, 0.35f), 12.f);
	}
	AddLabel(bPause ? TEXT("Esc: вернуться в игру") : TEXT("В игре Esc открывает паузу и приглашение друзей"), 11, Dim, 0.f);
}

FReply SAntMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (bPause && InKeyEvent.GetKey() == EKeys::Escape && PC.IsValid())
	{
		PC->CloseMenu();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}