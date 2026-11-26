#include "AntUiMenu.h"
#include "AntUiStyle.h"
#include "AntUiSound.h"
#include "AntSession.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "AntGameState.h"
#include "AntCharacter.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "LevelV3.h"
#include "AntVersus.h"
#include "AntTutorial.h"
#include "AntDifficulty.h"
#include "AntColonyVote.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"

using namespace AntUi;
bool SAntUiMenu::bBootShown = false;

// =====================================================================================================================
// Drawing in a 1920x1080 design space, scaled to fit, centred (wider screens get more backdrop, never wider layouts).
// =====================================================================================================================

struct FUiDraw
{
	const FGeometry& G;
	FSlateWindowElementList& Out;
	int32* LayerCounter;   // one counter for every copy: each element gets the next layer, so later = on top
	float U;
	FVector2D O;
	FVector2D Size;
	float A = 1.f;
	float T = 0.f;      // seconds on this screen
	float Time = 0.f;   // seconds since the widget was made

	FVector2D P(float X, float Y) const { return O + FVector2D(X, Y) * U; }
	FPaintGeometry PG(const FVector2D& Pos, const FVector2D& Sz) const { return G.ToPaintGeometry(FVector2f(Sz), FSlateLayoutTransform(FVector2f(Pos))); }
	void BoxL(const FVector2D& Pos, const FVector2D& Sz, const FSlateBrush* B, FLinearColor Col) const
	{
		if (!B || Sz.X <= 0.f || Sz.Y <= 0.f) { return; }
		Col.A *= A;
		FSlateDrawElement::MakeBox(Out, ++(*LayerCounter), PG(Pos, Sz), B, ESlateDrawEffect::None, Col);
	}
	void Box(float X, float Y, float W, float H, const FSlateBrush* B, const FLinearColor& Col) const { BoxL(P(X, Y), FVector2D(W, H) * U, B, Col); }
	void Rect(float X, float Y, float W, float H, const FLinearColor& Col) const { Box(X, Y, W, H, White(), Col); }
	void RRect(float X, float Y, float W, float H, float R, const FLinearColor& Col) const { Box(X, Y, W, H, Rounded(R * U), Col); }
	void Frame(float X, float Y, float W, float H, float Th, const FLinearColor& Col) const
	{
		Rect(X, Y, W, Th, Col); Rect(X, Y + H - Th, W, Th, Col); Rect(X, Y, Th, H, Col); Rect(X + W - Th, Y, Th, H, Col);
	}
	void BoxRot(const FVector2D& CenterL, const FVector2D& SzL, float Deg, const FSlateBrush* B, FLinearColor Col) const
	{
		if (!B) { return; }
		Col.A *= A;
		const FGeometry Child = G.MakeChild(FVector2f(SzL), FSlateLayoutTransform(FVector2f(CenterL - SzL * 0.5f)), FSlateRenderTransform(FQuat2f(FMath::DegreesToRadians(Deg))), FVector2f(0.5f, 0.5f));
		FSlateDrawElement::MakeBox(Out, ++(*LayerCounter), Child.ToPaintGeometry(), B, ESlateDrawEffect::None, Col);
	}
	FVector2D Measure(const FString& S, const FSlateFontInfo& F) const
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F);
	}
	void TextL(const FString& S, const FVector2D& Pos, const FSlateFontInfo& F, FLinearColor Col) const
	{
		Col.A *= A;
		FSlateDrawElement::MakeText(Out, ++(*LayerCounter), PG(Pos, Measure(S, F)), S, F, ESlateDrawEffect::None, Col);
	}
	/** Align: 0 left, 1 centre, 2 right. Returns the size in design units. */
	FVector2D Text(const FString& S, float X, float Y, EFont Fn, float Sz, const FLinearColor& Col, int32 Align = 0, float Shadow = 0.f) const
	{
		const FSlateFontInfo F = Font(Fn, Sz * U);
		const FVector2D M = Measure(S, F);
		FVector2D Pos = P(X, Y);
		if (Align == 1) { Pos.X -= M.X * 0.5f; }
		else if (Align == 2) { Pos.X -= M.X; }
		if (Shadow > 0.f) { TextL(S, Pos + FVector2D(0.f, 2.f * U), F, FLinearColor(0.f, 0.f, 0.f, Shadow)); }
		TextL(S, Pos, F, Col);
		return M / U;
	}
	/** Text fitted into a width (shrinks, never below 70 %). */
	FVector2D TextFit(const FString& S, float X, float Y, float MaxW, EFont Fn, float Sz, const FLinearColor& Col, int32 Align = 0) const
	{
		const FVector2D M = Measure(S, Font(Fn, Sz * U)) / U;
		const float K = M.X > MaxW ? FMath::Max(0.7f, MaxW / M.X) : 1.f;
		return Text(S, X, Y, Fn, Sz * K, Col, Align);
	}
	void TextRot(const FString& S, const FVector2D& CenterL, float Deg, EFont Fn, float Sz, FLinearColor Col) const
	{
		const FSlateFontInfo F = Font(Fn, Sz * U);
		const FVector2D M = Measure(S, F);
		Col.A *= A;
		const FGeometry Child = G.MakeChild(FVector2f(M), FSlateLayoutTransform(FVector2f(CenterL - M * 0.5f)), FSlateRenderTransform(FQuat2f(FMath::DegreesToRadians(Deg))), FVector2f(0.5f, 0.5f));
		FSlateDrawElement::MakeText(Out, ++(*LayerCounter), Child.ToPaintGeometry(), S, F, ESlateDrawEffect::None, Col);
	}
	/** Word-wrapped text; returns the height used (design units). */
	float Wrap(const FString& S, float X, float Y, float MaxW, EFont Fn, float Sz, const FLinearColor& Col, int32 MaxLines = 12, float LineH = 1.28f, int32 Align = 0) const
	{
		const FSlateFontInfo F = Font(Fn, Sz * U);
		TArray<FString> Paras;
		S.ParseIntoArray(Paras, TEXT("\n"), false);
		int32 Lines = 0;
		const float H = Sz * LineH * 1.333f;
		for (const FString& Para : Paras)
		{
			TArray<FString> Words;
			Para.ParseIntoArray(Words, TEXT(" "));
			FString Line;
			auto Flush = [&]()
			{
				if (Lines < MaxLines)
				{
					const FVector2D M = Measure(Line, F) / U;
					const float LX = Align == 1 ? X + (MaxW - M.X) * 0.5f : Align == 2 ? X + MaxW - M.X : X;
					TextL(Line, P(LX, Y + Lines * H), F, Col);
				}
				++Lines;
				Line.Reset();
			};
			for (const FString& W : Words)
			{
				const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
				if (!Line.IsEmpty() && Measure(Try, F).X / U > MaxW) { Flush(); Line = W; }
				else { Line = Try; }
			}
			if (!Line.IsEmpty() || Words.Num() == 0) { Flush(); }
		}
		return FMath::Min(Lines, MaxLines) * H;
	}
	/** An image cropped to fill a local rectangle (zoom >= 1, pan in -1..1). */
	void Cover(const TCHAR* Path, const FVector2D& L0, const FVector2D& LS, float Zoom, const FVector2D& Pan, const FLinearColor& Col) const
	{
		const FSlateBrush* B = Brush(Path);
		if (!B) { BoxL(L0, LS, White(), FLinearColor(C::Ink.R, C::Ink.G, C::Ink.B, Col.A)); return; }
		const FVector2D Img = B->ImageSize.X > 0.f ? FVector2D(B->ImageSize) : FVector2D(1920.f, 1080.f);
		const float K = FMath::Max(LS.X / Img.X, LS.Y / Img.Y) * FMath::Max(1.f, Zoom);
		const FVector2D Sz = Img * K;
		const FVector2D Slack = (Sz - LS) * 0.5f;
		const FVector2D Pos = L0 - Slack + FVector2D(Pan.X * Slack.X, Pan.Y * Slack.Y);
		const FGeometry Clip = G.MakeChild(FVector2f(LS), FSlateLayoutTransform(FVector2f(L0)));
		Out.PushClip(FSlateClippingZone(Clip));
		BoxL(Pos, Sz, B, Col);
		Out.PopClip();
	}
	/** A sheet of receipt paper with tape at the top corners (the war-room look). */
	void Paper(float X, float Y, float W, float H, float Tilt = 0.f) const
	{
		RRect(X + 6.f, Y + 10.f, W, H, 6.f, FLinearColor(0.f, 0.f, 0.f, 0.35f));
		const FSlateBrush* PaperB = Brush(TEXT("Materials/T_UI_Paper"));
		Box(X, Y, W, H, PaperB ? PaperB : White(), PaperB ? FLinearColor::White : C::Paper);
		const FSlateBrush* Tape = Brush(TEXT("Materials/T_UI_Tape"));
		BoxRot(P(X + 34.f, Y + 4.f), FVector2D(110.f, 30.f) * U, -32.f + Tilt, Tape, FLinearColor(1.f, 1.f, 1.f, 0.85f));
		BoxRot(P(X + W - 34.f, Y + 4.f), FVector2D(110.f, 30.f) * U, 32.f + Tilt, Tape, FLinearColor(1.f, 1.f, 1.f, 0.85f));
	}
	/** A dark glass panel with a thin cocoa edge. */
	void Glass(float X, float Y, float W, float H, float Alpha = 0.82f) const
	{
		RRect(X, Y, W, H, 10.f, WithA(C::Cocoa, 0.9f * Alpha));
		RRect(X + 1.5f, Y + 1.5f, W - 3.f, H - 3.f, 9.f, WithA(C::Ink, Alpha));
	}
	/** A rubber stamp: rough ink frame + words, tilted. */
	void Stamp(const FString& S, float CX, float CY, float Deg, const FLinearColor& Col, float Sz = 26.f) const
	{
		const FVector2D M = Measure(S, Font(EFont::Display, Sz * U)) / U;
		const FSlateBrush* B = Brush(TEXT("Materials/T_UI_StampBox"));
		BoxRot(P(CX, CY), FVector2D(M.X + 40.f, M.Y + 22.f) * U, Deg, B ? B : White(), WithA(Col, B ? 0.95f : 0.25f));
		TextRot(S, P(CX, CY), Deg, EFont::Display, Sz, WithA(Col, 0.95f));
	}
	void Key(const FString& K, float X, float Y, float Sz = 16.f) const
	{
		const FVector2D M = Measure(K, Font(EFont::Bold, Sz * U)) / U;
		RRect(X, Y, M.X + 18.f, Sz * 1.6f, 5.f, WithA(C::Sugar, 0.92f));
		Text(K, X + 9.f, Y + Sz * 0.12f, EFont::Bold, Sz, C::Ink);
	}
};

namespace
{
	float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Pow(1.f - X, 3.f); }
	float EaseBack(float X) { X = FMath::Clamp(X, 0.f, 1.f); const float C1 = 1.70158f, C3 = C1 + 1.f; return 1.f + C3 * FMath::Pow(X - 1.f, 3.f) + C1 * FMath::Pow(X - 1.f, 2.f); }
	int32 IdOf(const FString& Key) { return (int32)(GetTypeHash(Key) | 1u); }
	bool IsOpaque(uint8 S) { return S == AntUiScreen::Boot || S == AntUiScreen::Main || S == AntUiScreen::Coop || S == AntUiScreen::Colony || S == AntUiScreen::Credits || S == AntUiScreen::Lobby || S == AntUiScreen::Training || S == AntUiScreen::DemoEnd; }

	const TCHAR* LoadingArts[] =
	{
		TEXT("Loading/T_UI_LoadChain"), TEXT("Loading/T_UI_LoadGruzd"), TEXT("Loading/T_UI_LoadCat"), TEXT("Loading/T_UI_LoadSpray"), TEXT("Loading/T_UI_LoadDog"),
		TEXT("Loading/T_UI_LoadMedic"), TEXT("Backgrounds/T_UI_BgCake"), TEXT("Backgrounds/T_UI_BgDawn"), TEXT("Backgrounds/T_UI_BgSlipper"),
	};
	const int32 NumTips = 16;

	/** The caste pick of the lobby: AAntCharacter::ServerSetClass is protected, so it is called through reflection (same RPC). */
	void SetCaste(APlayerController* PC, int32 Caste)
	{
		AAntCharacter* Ant = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
		UFunction* Fn = Ant ? Ant->FindFunction(TEXT("ServerSetClass")) : nullptr;
		if (Fn)
		{
			struct { uint8 NewClass; } Params{ (uint8)FMath::Clamp(Caste, 0, 4) };
			Ant->ProcessEvent(Fn, &Params);
		}
	}
}

// =====================================================================================================================
// Life
// =====================================================================================================================

void SAntUiMenu::Construct(const FArguments& InArgs)
{
	PC = InArgs._PC;
	Now = FPlatformTime::Seconds();
	JoinAddress = TEXT("127.0.0.1:7797");
	LoadingArt = FMath::RandRange(0, UE_ARRAY_COUNT(LoadingArts) - 1);
	LoadingTip = FMath::RandRange(0, NumTips - 1);
	SetCanTick(true);
	Go(InArgs._Screen);
}

SAntUiMenu::~SAntUiMenu()
{
	if (bWorldHidden && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->bDisableWorldRendering = false;
	}
}

UAntSession* SAntUiMenu::Session() const
{
	UGameInstance* GI = PC.IsValid() ? PC->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAntSession>() : nullptr;
}

void SAntUiMenu::Go(uint8 NewScreen)
{
	if (NewScreen == AntUiScreen::Settings && Screen != AntUiScreen::Settings)
	{
		ReturnTo = Screen;
		SettingsBegin();
	}
	if (NewScreen == AntUiScreen::Boot) { bBootShown = true; }
	Screen = NewScreen;
	ScreenStart = FPlatformTime::Seconds();
	FocusId = 0;
	CreditsScroll = 0.f;
	EditId = 0;
	EditTarget = nullptr;
	// A full-screen menu covers the kitchen: stop drawing the world under it (menus must be cheap).
	if (PC.IsValid() && GEngine && GEngine->GameViewport)
	{
		const bool bHide = IsOpaque(Screen) || (Screen == AntUiScreen::Settings && IsOpaque(ReturnTo));
		GEngine->GameViewport->bDisableWorldRendering = bHide;
		bWorldHidden = bHide;
	}
	if (PC.IsValid() && Screen != AntUiScreen::Boot)
	{
		AntUiSound(EAntUiSound::Open, PC.Get());
	}
}

void SAntUiMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Now = FPlatformTime::Seconds();
	const float Dt = FMath::Min(InDeltaTime, 0.1f);
	for (auto It = Anims.CreateIterator(); It; ++It)
	{
		const float Want = It.Key() == FocusId ? 1.f : 0.f;
		It.Value() = FMath::FInterpTo(It.Value(), Want, Dt, 14.f);
	}
	const float T = (float)(Now - ScreenStart);
	if (Screen == AntUiScreen::Boot && T > 5.6f)
	{
		Go(AntUiScreen::Main);
	}
	if (Screen == AntUiScreen::Credits)
	{
		CreditsScroll += Dt * 42.f;
	}
	if (Screen == AntUiScreen::Loading && T > 0.f)
	{
		LoadingTip = (int32)(T / 6.f) % NumTips;
	}
	// The lobby closes by itself when the raid starts.
	if (Screen == AntUiScreen::Lobby && PC.IsValid())
	{
		if (const AAntRaid* Raid = AAntRaid::Get(PC->GetWorld()))
		{
			if (Raid->Phase == AntRaidPhase::Raid)
			{
				PC->CloseMenu();
			}
		}
	}
}

// =====================================================================================================================
// Items, focus, input
// =====================================================================================================================

int32 SAntUiMenu::AddItem(const FUiDraw& D, const FString& Key, float X, float Y, float W, float H, bool bEnabled, TFunction<void()> Click, TFunction<void(int32)> Adjust, TFunction<void(float)> Drag) const
{
	FItem It;
	It.Id = IdOf(Key);
	const FVector2D P0 = D.P(X, Y), P1 = D.P(X + W, Y + H);
	It.R = FSlateRect(P0.X, P0.Y, P1.X, P1.Y);
	It.bEnabled = bEnabled;
	It.Click = MoveTemp(Click);
	It.Adjust = MoveTemp(Adjust);
	It.Drag = MoveTemp(Drag);
	Items.Add(MoveTemp(It));
	if (!Anims.Contains(Items.Last().Id)) { Anims.Add(Items.Last().Id, 0.f); }
	return Items.Last().Id;
}

float SAntUiMenu::Anim(int32 Id) const
{
	const float* V = Anims.Find(Id);
	return V ? *V : 0.f;
}

const SAntUiMenu::FItem* SAntUiMenu::FindItem(int32 Id) const
{
	for (const FItem& It : Items) { if (It.Id == Id) { return &It; } }
	return nullptr;
}

const SAntUiMenu::FItem* SAntUiMenu::ItemAt(const FVector2D& L) const
{
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		const FItem& It = Items[i];
		if (L.X >= It.R.Left && L.X <= It.R.Right && L.Y >= It.R.Top && L.Y <= It.R.Bottom) { return &It; }
	}
	return nullptr;
}

void SAntUiMenu::MoveFocus(const FVector2D& Dir)
{
	const FItem* Cur = FindItem(FocusId);
	if (!Cur)
	{
		for (const FItem& It : Items) { if (It.bEnabled) { FocusId = It.Id; break; } }
		return;
	}
	const FVector2D From = Cur->R.GetCenter();
	float Best = TNumericLimits<float>::Max();
	int32 BestId = 0;
	for (const FItem& It : Items)
	{
		if (It.Id == Cur->Id || !It.bEnabled) { continue; }
		const FVector2D To = It.R.GetCenter() - From;
		const float Along = FVector2D::DotProduct(To, Dir);
		if (Along <= 4.f) { continue; }
		const float Across = FMath::Abs(FVector2D::CrossProduct(To, Dir));
		const float Score = Along + Across * 2.5f;
		if (Score < Best) { Best = Score; BestId = It.Id; }
	}
	if (BestId != 0)
	{
		FocusId = BestId;
		AntUiSound(EAntUiSound::Hover, PC.Get());
	}
}

void SAntUiMenu::Activate()
{
	if (const FItem* It = FindItem(FocusId))
	{
		if (It->bEnabled && It->Click)
		{
			AntUiSound(EAntUiSound::Click, PC.Get());
			TFunction<void()> Fn = It->Click;   // the item list is rebuilt by the next paint; keep a copy
			Fn();
		}
		else if (!It->bEnabled)
		{
			AntUiSound(EAntUiSound::Error, PC.Get());
		}
	}
}

void SAntUiMenu::CloseOrBack()
{
	if (PC.IsValid()) { PC->CloseMenu(); }
}

void SAntUiMenu::Back()
{
	AntUiSound(EAntUiSound::Back, PC.Get());
	switch (Screen)
	{
	case AntUiScreen::Boot: Go(AntUiScreen::Main); break;
	case AntUiScreen::Coop:
	case AntUiScreen::Colony:
	case AntUiScreen::Credits:
	case AntUiScreen::Training:
	case AntUiScreen::DemoEnd: Go(AntUiScreen::Main); break;
	case AntUiScreen::Settings: SettingsRevert(); Go(ReturnTo); break;
	case AntUiScreen::Pause:
	case AntUiScreen::Lobby: CloseOrBack(); break;
	default: break;
	}
}

FReply SAntUiMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (EditId != 0)
	{
		if (K == EKeys::Enter || K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Bottom)
		{
			if (K == EKeys::Escape && EditTarget) { *EditTarget = EditBefore; }
			EditId = 0;
			EditTarget = nullptr;
			AntUiSound(EAntUiSound::Click, PC.Get());
		}
		else if (K == EKeys::BackSpace && EditTarget && EditTarget->Len() > 0)
		{
			EditTarget->LeftChopInline(1);
		}
		return FReply::Handled();
	}
	if (Screen == AntUiScreen::Boot) { Go(AntUiScreen::Main); return FReply::Handled(); }
	if (K == EKeys::Up || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up || K == EKeys::W) { MoveFocus(FVector2D(0.f, -1.f)); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down || K == EKeys::S) { MoveFocus(FVector2D(0.f, 1.f)); return FReply::Handled(); }
	const bool bLeft = K == EKeys::Left || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftStick_Left || K == EKeys::A;
	const bool bRight = K == EKeys::Right || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_LeftStick_Right || K == EKeys::D;
	if (bLeft || bRight)
	{
		const FItem* It = FindItem(FocusId);
		if (It && It->Adjust && It->bEnabled)
		{
			TFunction<void(int32)> Fn = It->Adjust;
			Fn(bRight ? 1 : -1);
			AntUiSound(EAntUiSound::Slide, PC.Get());
		}
		else
		{
			MoveFocus(FVector2D(bRight ? 1.f : -1.f, 0.f));
		}
		return FReply::Handled();
	}
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::Gamepad_FaceButton_Bottom) { Activate(); return FReply::Handled(); }
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || (K == EKeys::Gamepad_Special_Right && Screen == AntUiScreen::Pause)) { Back(); return FReply::Handled(); }
	if (Screen == AntUiScreen::Settings && (K == EKeys::Q || K == EKeys::E || K == EKeys::Gamepad_LeftShoulder || K == EKeys::Gamepad_RightShoulder))
	{
		SettingsTab = (SettingsTab + ((K == EKeys::E || K == EKeys::Gamepad_RightShoulder) ? 1 : 3)) % 4;
		FocusId = 0;
		AntUiSound(EAntUiSound::Toggle, PC.Get());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAntUiMenu::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	if (EditId != 0 && EditTarget)
	{
		const TCHAR Ch = InCharacterEvent.GetCharacter();
		if (Ch >= 32 && Ch != 127 && EditTarget->Len() < 48)
		{
			EditTarget->AppendChar(Ch);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAntUiMenu::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (DragId != 0)
	{
		if (const FItem* It = FindItem(DragId))
		{
			if (It->Drag) { TFunction<void(float)> Fn = It->Drag; Fn(FMath::Clamp((Mouse.X - It->R.Left) / FMath::Max(1.f, It->R.Right - It->R.Left), 0.f, 1.f)); }
		}
		return FReply::Handled();
	}
	if (const FItem* It = ItemAt(Mouse))
	{
		if (It->Id != FocusId && It->bEnabled)
		{
			FocusId = It->Id;
			AntUiSound(EAntUiSound::Hover, PC.Get());
		}
	}
	return FReply::Handled();
}

FReply SAntUiMenu::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (Screen == AntUiScreen::Boot) { Go(AntUiScreen::Main); return FReply::Handled(); }
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton || MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton) { Back(); return FReply::Handled(); }
	if (const FItem* It = ItemAt(Mouse))
	{
		FocusId = It->Id;
		if (It->Drag && It->bEnabled)
		{
			DragId = It->Id;
			TFunction<void(float)> Fn = It->Drag;
			Fn(FMath::Clamp((Mouse.X - It->R.Left) / FMath::Max(1.f, It->R.Right - It->R.Left), 0.f, 1.f));
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	return FReply::Handled();
}

FReply SAntUiMenu::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Mouse = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (DragId != 0)
	{
		DragId = 0;
		AntUiSound(EAntUiSound::Slide, PC.Get());
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Handled(); }
	if (const FItem* It = ItemAt(Mouse))
	{
		if (It->Id == FocusId) { Activate(); }
	}
	return FReply::Handled();
}

FReply SAntUiMenu::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Screen == AntUiScreen::Credits) { CreditsScroll = FMath::Max(0.f, CreditsScroll - MouseEvent.GetWheelDelta() * 90.f); }
	return FReply::Handled();
}

// =====================================================================================================================
// Widgets
// =====================================================================================================================

void SAntUiMenu::MenuButton(FUiDraw& D, const FString& Key, const FString& Label, float X, float Y, TFunction<void()> Fn, bool bEnabled) const
{
	const float H = 58.f, W = 470.f;
	const int32 Id = AddItem(D, Key, X - 10.f, Y, W, H, bEnabled, MoveTemp(Fn));
	const float K = Ease(Anim(Id));
	const float Shift = 16.f * K;
	// Masking tape slides in behind the hot one; the words turn dark on it.
	if (K > 0.01f)
	{
		const FSlateBrush* Tape = Brush(TEXT("Materials/T_UI_Tape"));
		const float TW = (Label.Len() * 19.f + 90.f) * K;
		D.Box(X - 26.f, Y + 2.f, TW, H - 4.f, Tape ? Tape : White(), WithA(C::Honey, Tape ? 0.95f : 0.85f));
		D.Rect(X - 26.f, Y + 2.f, 6.f, H - 4.f, WithA(C::AmberDeep, K));
	}
	const FLinearColor Col = !bEnabled ? WithA(C::Muted, 0.5f) : FMath::Lerp(C::Sugar, C::Graphite, K);
	D.Text(Label, X + Shift, Y + 8.f, EFont::Display, 28.f, Col, 0, K < 0.5f ? 0.55f : 0.f);
}

void SAntUiMenu::Button(FUiDraw& D, const FString& Key, const FString& Label, float X, float Y, float W, float H, TFunction<void()> Fn, bool bPrimary, bool bEnabled, const TCHAR* IconName) const
{
	const int32 Id = AddItem(D, Key, X, Y, W, H, bEnabled, MoveTemp(Fn));
	const float K = Ease(Anim(Id));
	const float Lift = -3.f * K;
	D.RRect(X + 2.f, Y + 5.f, W, H, 10.f, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	FLinearColor Fill = bPrimary ? FMath::Lerp(C::Amber, C::Honey, K) : FMath::Lerp(WithA(C::InkSoft, 0.94f), WithA(C::Cocoa, 0.98f), K);
	if (!bEnabled) { Fill = WithA(C::InkSoft, 0.6f); }
	D.RRect(X, Y + Lift, W, H, 10.f, bPrimary ? C::AmberDeep : FMath::Lerp(C::Cocoa, C::Amber, K));
	D.RRect(X + 2.f, Y + Lift + 2.f, W - 4.f, H - 4.f, 9.f, Fill);
	const FLinearColor TextC = !bEnabled ? WithA(C::Muted, 0.6f) : bPrimary ? C::Ink : FMath::Lerp(C::Sugar, C::Honey, K);
	float TX = X + W * 0.5f;
	float MaxW = W - 20.f;
	if (IconName)
	{
		const float IS = H * 0.66f;
		MaxW = W - IS - 34.f;
		const float MX = FMath::Min(D.Measure(Label, Font(EFont::Display, 20.f * D.U)).X / D.U, MaxW);
		const float Total = IS + 10.f + MX;
		D.Box(X + (W - Total) * 0.5f, Y + Lift + (H - IS) * 0.5f, IS, IS, IconBrush(IconName), FLinearColor(1.f, 1.f, 1.f, bEnabled ? 1.f : 0.5f));
		TX = X + (W - Total) * 0.5f + IS + 10.f + MX * 0.5f;
	}
	D.TextFit(Label, TX, Y + Lift + (H - 28.f) * 0.5f, MaxW, EFont::Display, 20.f, TextC, 1);
}

void SAntUiMenu::OptionRow(FUiDraw& D, const FString& Key, const FString& Label, const FString& Value, float Y, TFunction<void(int32)> Adjust, const TCHAR* Help) const
{
	const float X = 300.f, W = 1320.f, H = 56.f;
	TFunction<void(int32)> AdjustCopy = Adjust;
	const int32 Id = AddItem(D, Key, X, Y, W, H, true, [AdjustCopy]() { AdjustCopy(1); }, MoveTemp(Adjust));
	const float K = Ease(Anim(Id));
	D.RRect(X, Y, W, H, 8.f, FMath::Lerp(WithA(C::Ink, 0.55f), WithA(C::Cocoa, 0.85f), K));
	D.Rect(X, Y + 8.f, 4.f, H - 16.f, WithA(C::Amber, K));
	D.Text(Label, X + 26.f, Y + 12.f, EFont::Bold, 21.f, FMath::Lerp(C::Sugar, C::Honey, K));
	const float VX = X + W - 330.f;
	D.Text(TEXT("‹"), VX, Y + 6.f, EFont::Display, 26.f, WithA(C::Amber, 0.5f + 0.5f * K));
	D.TextFit(Value, VX + 150.f, Y + 12.f, 250.f, EFont::Bold, 21.f, C::Sugar, 1);
	D.Text(TEXT("›"), VX + 290.f, Y + 6.f, EFont::Display, 26.f, WithA(C::Amber, 0.5f + 0.5f * K));
	if (Help && K > 0.5f)
	{
		D.Text(FString(Help), X + W * 0.5f, 950.f, EFont::Body, 18.f, WithA(C::Muted, K), 1);
	}
}

void SAntUiMenu::SliderRow(FUiDraw& D, const FString& Key, const FString& Label, float Frac, const FString& Value, float Y, TFunction<void(float)> Set) const
{
	const float X = 300.f, W = 1320.f, H = 56.f;
	const float TX = X + W - 560.f, TW = 420.f;
	TFunction<void(float)> SetA = Set;
	const float F0 = Frac;
	const int32 Id = AddItem(D, Key, TX - 20.f, Y, TW + 40.f, H, true, nullptr, [SetA, F0](int32 Dir) { SetA(FMath::Clamp(F0 + Dir * 0.05f, 0.f, 1.f)); }, MoveTemp(Set));
	const float K = Ease(Anim(Id));
	D.RRect(X, Y, W, H, 8.f, FMath::Lerp(WithA(C::Ink, 0.55f), WithA(C::Cocoa, 0.85f), K));
	D.Rect(X, Y + 8.f, 4.f, H - 16.f, WithA(C::Amber, K));
	D.Text(Label, X + 26.f, Y + 12.f, EFont::Bold, 21.f, FMath::Lerp(C::Sugar, C::Honey, K));
	D.RRect(TX, Y + 24.f, TW, 8.f, 4.f, WithA(C::Sugar, 0.18f));
	D.RRect(TX, Y + 24.f, TW * Frac, 8.f, 4.f, FMath::Lerp(C::AmberDeep, C::Amber, K));
	D.RRect(TX + TW * Frac - 11.f, Y + 17.f, 22.f, 22.f, 11.f, FMath::Lerp(C::Sugar, C::Honey, K));
	D.Text(Value, X + W - 30.f, Y + 12.f, EFont::Bold, 21.f, C::Sugar, 2);
}

void SAntUiMenu::ToggleRow(FUiDraw& D, const FString& Key, const FString& Label, bool bOn, float Y, TFunction<void()> Flip) const
{
	const float X = 300.f, W = 1320.f, H = 56.f;
	TFunction<void()> FlipA = Flip;
	const int32 Id = AddItem(D, Key, X, Y, W, H, true, MoveTemp(Flip), [FlipA](int32) { FlipA(); });
	const float K = Ease(Anim(Id));
	D.RRect(X, Y, W, H, 8.f, FMath::Lerp(WithA(C::Ink, 0.55f), WithA(C::Cocoa, 0.85f), K));
	D.Rect(X, Y + 8.f, 4.f, H - 16.f, WithA(C::Amber, K));
	D.Text(Label, X + 26.f, Y + 12.f, EFont::Bold, 21.f, FMath::Lerp(C::Sugar, C::Honey, K));
	const float SX = X + W - 120.f;
	D.RRect(SX, Y + 14.f, 72.f, 28.f, 14.f, bOn ? C::Leaf : WithA(C::Sugar, 0.2f));
	D.RRect(SX + (bOn ? 46.f : 4.f), Y + 18.f, 22.f, 20.f, 10.f, C::Sugar);
	D.Text(bOn ? L(TEXT("on")) : L(TEXT("off")), SX - 16.f, Y + 12.f, EFont::Bold, 19.f, bOn ? C::Leaf : C::Muted, 2);
}

void SAntUiMenu::EditField(FUiDraw& D, const FString& Key, FString* Value, float X, float Y, float W, float H, const FString& Hint) const
{
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	const int32 Id = AddItem(D, Key, X, Y, W, H, true, [Self, Key, Value]()
	{
		Self->EditId = IdOf(Key);
		Self->EditTarget = Value;
		Self->EditBefore = *Value;
	});
	const bool bEditing = EditId == Id;
	const float K = Ease(Anim(Id));
	D.RRect(X, Y, W, H, 8.f, bEditing ? C::Amber : FMath::Lerp(C::Cocoa, C::AmberDeep, K));
	D.RRect(X + 2.f, Y + 2.f, W - 4.f, H - 4.f, 7.f, C::Paper);
	const bool bEmpty = Value->IsEmpty();
	FString Shown = bEmpty && !bEditing ? Hint : *Value;
	if (bEditing && FMath::Fmod(D.Time, 1.f) < 0.55f) { Shown += TEXT("|"); }
	D.TextFit(Shown, X + 14.f, Y + (H - 30.f) * 0.5f, W - 28.f, EFont::Mono, 20.f, bEmpty && !bEditing ? WithA(C::Graphite, 0.45f) : C::Graphite);
}

void SAntUiMenu::Tabs(FUiDraw& D, const TArray<FString>& Names, int32 Active, float Y, TFunction<void(int32)> Pick) const
{
	const float TW = 250.f, Gap = 14.f;
	const float X0 = 960.f - (Names.Num() * TW + (Names.Num() - 1) * Gap) * 0.5f;
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		const float X = X0 + i * (TW + Gap);
		TFunction<void(int32)> PickA = Pick;
		const int32 Id = AddItem(D, FString::Printf(TEXT("tab%d"), i), X, Y, TW, 54.f, true, [PickA, i]() { PickA(i); });
		const float K = Ease(Anim(Id));
		const bool bOn = i == Active;
		D.RRect(X, Y, TW, 54.f, 10.f, bOn ? C::Amber : FMath::Lerp(WithA(C::Ink, 0.7f), WithA(C::Cocoa, 0.9f), K));
		D.Text(Names[i], X + TW * 0.5f, Y + 12.f, EFont::Display, 20.f, bOn ? C::Ink : FMath::Lerp(C::Sugar, C::Honey, K), 1);
	}
	D.Text(TEXT("Q"), X0 - 46.f, Y + 12.f, EFont::Bold, 20.f, WithA(C::Muted, 0.8f));
	D.Text(TEXT("E"), X0 + Names.Num() * (TW + Gap) + 14.f, Y + 12.f, EFont::Bold, 20.f, WithA(C::Muted, 0.8f));
}

// =====================================================================================================================
// Paint
// =====================================================================================================================

int32 SAntUiMenu::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Sz = AllottedGeometry.GetLocalSize();
	LocalSize = Sz;
	const float U = FMath::Min(Sz.X / 1920.f, Sz.Y / 1080.f);
	int32 Layer = LayerId;
	FUiDraw D{ AllottedGeometry, OutDrawElements, &Layer, U, FVector2D((Sz.X - 1920.f * U) * 0.5f, (Sz.Y - 1080.f * U) * 0.5f), Sz };
	D.T = (float)(FPlatformTime::Seconds() - ScreenStart);
	D.Time = (float)FPlatformTime::Seconds();
	Items.Reset();

	switch (Screen)
	{
	case AntUiScreen::Boot: PaintBoot(D); break;
	case AntUiScreen::Main: PaintMain(D); break;
	case AntUiScreen::Coop: PaintCoop(D); break;
	case AntUiScreen::Colony: PaintColony(D); break;
	case AntUiScreen::Lobby: PaintLobby(D); break;
	case AntUiScreen::Settings: PaintSettings(D); break;
	case AntUiScreen::Credits: PaintCredits(D); break;
	case AntUiScreen::Pause: PaintPause(D); break;
	case AntUiScreen::Loading: PaintLoading(D); break;
	case AntUiScreen::Training: PaintTraining(D); break;
	case AntUiScreen::DemoEnd: PaintDemoEnd(D); break;
	default: break;
	}

	if (!FindItem(FocusId))
	{
		for (const FItem& It : Items) { if (It.bEnabled) { FocusId = It.Id; break; } }
	}
	return Layer + 1;
}

void SAntUiMenu::PaintBackdrop(FUiDraw& D, const TCHAR* Plate, float Dim, bool bDust) const
{
	const float t = D.Time;
	// Slow camera drift + a little parallax from the mouse.
	const FVector2D MouseN = LocalSize.X > 0.f ? FVector2D(Mouse.X / LocalSize.X - 0.5f, Mouse.Y / LocalSize.Y - 0.5f) : FVector2D::ZeroVector;
	const float Zoom = 1.07f + 0.025f * FMath::Sin(t * 0.07f);
	const FVector2D Pan(0.35f * FMath::Sin(t * 0.045f) - MouseN.X * 0.25f, 0.25f * FMath::Cos(t * 0.06f) - MouseN.Y * 0.2f);
	D.BoxL(FVector2D::ZeroVector, D.Size, White(), C::Ink);
	const float K = FMath::Pow(FMath::Clamp(1.f - Dim, 0.f, 1.f), 2.2f);   // tints are linear, the eye is not
	D.Cover(Plate, FVector2D::ZeroVector, D.Size, Zoom, Pan, FLinearColor(K, K, K, 1.f));
	if (bDust)
	{
		const FSlateBrush* Dot = Brush(TEXT("Materials/T_UI_Dot"));
		for (int32 i = 0; i < 70; ++i)
		{
			const float S1 = FMath::Frac(FMath::Sin(i * 12.9898f) * 43758.5453f);
			const float S2 = FMath::Frac(FMath::Sin(i * 78.233f) * 12543.123f);
			const float S3 = FMath::Frac(FMath::Sin(i * 37.719f) * 9876.543f);
			const float Depth = 0.3f + S3 * 0.7f;
			const float X = FMath::Frac(S1 + t * 0.006f * Depth + 0.02f * FMath::Sin(t * 0.3f + i)) * D.Size.X - MouseN.X * 40.f * Depth * D.U;
			const float Y = FMath::Frac(S2 - t * 0.01f * Depth) * D.Size.Y - MouseN.Y * 30.f * Depth * D.U;
			const float R = (3.f + 9.f * Depth * Depth) * D.U;
			const float Tw = 0.35f + 0.65f * FMath::Abs(FMath::Sin(t * (0.4f + S1) + i));
			D.BoxL(FVector2D(X - R, Y - R), FVector2D(R, R) * 2.f, Dot ? Dot : Rounded(R), FLinearColor(1.f, 0.86f, 0.6f, 0.38f * Tw * Depth));
		}
	}
	const FSlateBrush* Vig = Brush(TEXT("Materials/T_UI_Vignette"));
	D.BoxL(FVector2D::ZeroVector, D.Size, Vig, FLinearColor(1.f, 1.f, 1.f, 0.9f));
}

void SAntUiMenu::PaintFooter(FUiDraw& D, bool bBack) const
{
	float X = 70.f;
	const float Y = 1020.f;
	auto Hint = [&](const FString& K, const FString& What)
	{
		D.Key(K, X, Y);
		X += D.Measure(K, Font(EFont::Bold, 16.f * D.U)).X / D.U + 28.f;
		X += D.Text(What, X, Y + 1.f, EFont::Body, 18.f, WithA(C::Sugar, 0.75f)).X + 30.f;
	};
	Hint(TEXT("Enter"), L(TEXT("hint.select")));
	if (bBack) { Hint(TEXT("Esc"), L(TEXT("hint.back"))); }
	Hint(L(TEXT("hint.arrows")), L(TEXT("hint.move")));
}

// ---------------------------------------------------------------------------------------------------- boot

void SAntUiMenu::PaintBoot(FUiDraw& D) const
{
	const float t = D.T;
	D.BoxL(FVector2D::ZeroVector, D.Size, White(), FLinearColor::Black);
	if (t < 2.9f)
	{
		// The studio card: the matchbox ant under its lamp.
		const float In = Ease((t - 0.3f) / 0.8f), Out = 1.f - Ease((t - 2.3f) / 0.5f);
		const float A = In * Out;
		const FSlateBrush* Studio = Brush(TEXT("Logo/T_UI_Studio"));
		const float S = 560.f + 20.f * t;
		D.Box(960.f - S * 0.5f, 470.f - S * 0.5f, S, S, Studio ? Studio : White(), FLinearColor(1.f, 1.f, 1.f, A));
		D.Text(TEXT("ANT COLONY"), 960.f, 800.f, EFont::Display, 40.f, WithA(C::Honey, A), 1);
		D.Text(L(TEXT("boot.presents")), 960.f, 856.f, EFont::Hand, 26.f, WithA(C::Sugar, A * 0.8f), 1);
		return;
	}
	const float t2 = t - 2.9f;
	const float Fade = Ease(t2 / 1.2f);
	{
		FUiDraw D2 = D;
		D2.A = Fade;
		PaintBackdrop(D2, TEXT("Backgrounds/T_UI_BgKitchen"), 0.55f, true);
	}
	const FSlateBrush* Logo = Brush(TEXT("Logo/T_UI_Logo"));
	const float K = EaseBack(t2 / 0.9f);
	const float W = 900.f * FMath::Max(0.01f, K), H = W * 0.71f;
	if (Logo) { D.Box(960.f - W * 0.5f, 470.f - H * 0.5f, W, H, Logo, FLinearColor(1.f, 1.f, 1.f, Ease(t2 / 0.4f))); }
	else { D.Text(TEXT("ANT KITCHEN"), 960.f, 420.f, EFont::Display, 110.f * K, C::Amber, 1, 0.6f); }
	D.Text(L(TEXT("tagline")), 960.f, 860.f, EFont::Hand, 34.f, WithA(C::Sugar, Ease((t2 - 0.9f) / 0.6f)), 1, 0.6f);
	D.Text(L(TEXT("boot.skip")), 960.f, 1010.f, EFont::Body, 18.f, WithA(C::Muted, 0.6f * Ease((t2 - 1.2f) / 0.5f)), 1);
}

// ---------------------------------------------------------------------------------------------------- main

void SAntUiMenu::PaintMain(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgKitchen"), 0.12f, true);
	// The left third darkens under the menu.
	const FSlateBrush* FadeH = Brush(TEXT("Materials/T_UI_FadeH"));
	// One smooth shade from the screen's left edge (on 21:9 the menu column sits further in; no hard edge at the 16:9 line).
	D.BoxL(FVector2D::ZeroVector, FVector2D(D.O.X + 900.f * D.U, D.Size.Y), FadeH ? FadeH : White(), WithA(C::Ink, FadeH ? 0.92f : 0.5f));
	const FSlateBrush* FadeV = Brush(TEXT("Materials/T_UI_FadeV"));
	D.BoxL(FVector2D(0.f, D.Size.Y * 0.72f), FVector2D(D.Size.X, D.Size.Y * 0.28f), FadeV ? FadeV : White(), WithA(C::Ink, FadeV ? 0.85f : 0.3f));

	const float In = Ease(D.T / 0.6f);
	FUiDraw DC = D;
	DC.A = In;
	const float Bob = 4.f * FMath::Sin(D.Time * 1.3f);
	if (const FSlateBrush* Logo = Brush(TEXT("Logo/T_UI_Logo")))
	{
		DC.Box(84.f, 40.f + Bob, 560.f, 398.f, Logo, FLinearColor::White);
	}
	else
	{
		DC.Text(TEXT("ANT KITCHEN"), 100.f, 120.f, EFont::Display, 72.f, C::Amber, 0, 0.6f);
	}

	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	UAntSession* S = Session();
	TWeakObjectPtr<AAntPlayerController> WPC = PC;
	const float BX = 120.f;
	float Y = 450.f;
	// «Учебка» first for a new recruit (auto-suggested until it is done); after it: the real raid, the chapters, the sandbox.
	const AntTut::FProgress& Tut = AntTut::Progress();
	const bool bTutDone = Tut.bDone;
	const float Step = bTutDone ? 56.f : 62.f;
	int32 N = 0;
	auto Slide = [&](float Yb) { return Yb + 24.f * (1.f - Ease((D.T - 0.08f * N++) / 0.5f)); };
	if (!bTutDone)
	{
		MenuButton(DC, TEXT("m.tut"), AntTut::Tx(TEXT("Учебка Сержанта Жвалова"), TEXT("Sergeant Zhvalov's Training")), BX, Slide(Y), [S]() { AntTut::SetWanted(AntTut::Training, 0); if (S) { S->PlaySolo(); } }); Y += Step;
	}
	MenuButton(DC, TEXT("m.play"), bTutDone ? FString(AntTut::Tx(TEXT("Настоящий рейд"), TEXT("The Real Raid"))) : L(TEXT("main.play")), BX, Slide(Y), [S]() { AntTut::SetWanted(AntTut::Off); AntVersus::Wanted() = false; if (S) { S->PlaySolo(); } }); Y += Step;
	MenuButton(DC, TEXT("m.coop"), L(TEXT("main.coop")), BX, Slide(Y), [Self]() { AntTut::SetWanted(AntTut::Off); Self->Go(AntUiScreen::Coop); }); Y += Step;
	MenuButton(DC, TEXT("m.colony"), L(TEXT("main.colony")), BX, Slide(Y), [Self]() { Self->Go(AntUiScreen::Colony); }); Y += Step;
	if (bTutDone && AntTut::IsDemo())
	{
		// The demo's versus teaser: one round as the Giant against the colony's ants.
		MenuButton(DC, TEXT("m.giant"), AntTut::Tx(TEXT("Сыграть за Гиганта (1 раунд)"), TEXT("Play the Giant (1 round)")), BX, Slide(Y), [S]() { AntTut::SetWanted(AntTut::Off); AntVersus::Wanted() = true; if (S) { S->PlaySolo(); } }, !AntTut::Progress().bDemoVersus); Y += Step;
	}
	if (bTutDone)
	{
		MenuButton(DC, TEXT("m.tutch"), AntTut::Tx(TEXT("Учебка: главы"), TEXT("Training: chapters")), BX, Slide(Y), [Self]() { Self->Go(AntUiScreen::Training); }); Y += Step;
		MenuButton(DC, TEXT("m.sandbox"), AntTut::IsDemo() ? FString(AntTut::Tx(TEXT("Песочница — в полной версии"), TEXT("Sandbox — full game"))) : FString(AntTut::Tx(TEXT("Песочница"), TEXT("Sandbox"))), BX, Slide(Y),
			[S]() { AntTut::SetWanted(AntTut::Sandbox); if (S) { S->PlaySolo(); } }, !AntTut::IsDemo()); Y += Step;
	}
	else
	{
		// The recruit's note: why the training first.
		DC.Paper(860.f, 470.f, 430.f, 150.f, 2.f);
		DC.Text(AntTut::Tx(TEXT("НОВОБРАНЕЦ?"), TEXT("NEW RECRUIT?")), 888.f, 494.f, EFont::Display, 22.f, C::JamDeep);
		DC.Wrap(AntTut::Tx(TEXT("Начни с учебки: 25 минут — и ты знаешь всё, что умеет муравей. Можно вдвоём."), TEXT("Start with the training: 25 minutes and you know everything an ant can do. Co-op works too.")),
			888.f, 532.f, 380.f, EFont::Hand, 20.f, C::Graphite, 3, 1.15f);
	}
	MenuButton(DC, TEXT("m.settings"), L(TEXT("main.settings")), BX, Slide(Y), [Self]() { Self->Go(AntUiScreen::Settings); }); Y += Step;
	MenuButton(DC, TEXT("m.credits"), L(TEXT("main.credits")), BX, Slide(Y), [Self]() { Self->Go(AntUiScreen::Credits); }); Y += Step;
	MenuButton(DC, TEXT("m.quit"), L(TEXT("main.quit")), BX, Slide(Y), [WPC]() { if (WPC.IsValid()) { UKismetSystemLibrary::QuitGame(WPC.Get(), WPC.Get(), EQuitPreference::Quit, false); } });

	// A receipt pinned at the bottom right: the colony, the best run, Steam.
	{
		const float NX = 1390.f, NY = 700.f, NW = 440.f, NH = 250.f;
		DC.Paper(NX, NY, NW, NH, -2.f);
		float LY = NY + 30.f;
		DC.Text(L(TEXT("main.note.title")), NX + 28.f, LY, EFont::Display, 20.f, C::Graphite);
		LY += 38.f;
		const FString Colony = AntNest::Summary();
		LY += DC.Wrap(Colony.IsEmpty() ? L(TEXT("main.note.nocolony")) : Colony, NX + 28.f, LY, NW - 56.f, EFont::Mono, 15.f, C::Graphite, 4, 1.2f) + 8.f;
		int32 BestShifts = 0, BestHaul = 0;
		AAntGameState::LoadBestRun(BestShifts, BestHaul);
		if (BestShifts > 0 || BestHaul > 0)
		{
			DC.Text(Lf(TEXT("main.note.best"), { BestShifts, BestHaul }), NX + 28.f, LY, EFont::Mono, 15.f, C::Graphite);
			LY += 26.f;
		}
		const bool bSteam = S && S->IsSteam();
		DC.TextFit(bSteam ? Lf(TEXT("main.note.steam"), { S->Nickname() }) : L(TEXT("main.note.nosteam")), NX + 28.f, NY + NH - 40.f, NW - 56.f, EFont::Hand, 20.f, bSteam ? C::LeafDeep : C::JamDeep);
	}
	if (S && !S->Status.IsEmpty())
	{
		DC.Glass(120.f, 900.f, 900.f, 60.f, 0.75f);
		DC.TextFit(S->Status, 140.f, 914.f, 860.f, EFont::Body, 20.f, C::Honey);
	}
	PaintFooter(DC, false);
}

// ---------------------------------------------------------------------------------------------------- co-op

void SAntUiMenu::PaintCoop(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgDawn"), 0.45f, true);
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.35f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	UAntSession* S = Session();
	TWeakObjectPtr<AAntPlayerController> WPC = PC;
	DC.Text(L(TEXT("coop.title")), 960.f, 70.f, EFont::Display, 54.f, C::Honey, 1, 0.6f);
	DC.Text(L(TEXT("coop.sub")), 960.f, 150.f, EFont::Body, 24.f, C::Sugar, 1, 0.6f);

	const float CW = 500.f, CH = 600.f, Gap = 40.f, X0 = 960.f - (CW * 3.f + Gap * 2.f) * 0.5f, Y0 = 230.f;
	struct FCard { const TCHAR* Icon; const TCHAR* Title; const TCHAR* Text; };
	const FCard Cards[3] = { { TEXT("Squad"), TEXT("coop.host"), TEXT("coop.host.text") }, { TEXT("Friends"), TEXT("coop.find"), TEXT("coop.find.text") }, { TEXT("Radio"), TEXT("coop.ip"), TEXT("coop.ip.text") } };
	for (int32 i = 0; i < 3; ++i)
	{
		const float X = X0 + i * (CW + Gap), Y = Y0 + 30.f * (1.f - Ease((D.T - 0.07f * i) / 0.45f));
		DC.Glass(X, Y, CW, CH, 0.86f);
		DC.Rect(X + 24.f, Y + 24.f, CW - 48.f, 3.f, WithA(C::Amber, 0.7f));
		DC.Box(X + 24.f, Y + 46.f, 96.f, 96.f, IconBrush(Cards[i].Icon), FLinearColor::White);
		DC.Text(L(Cards[i].Title), X + 136.f, Y + 64.f, EFont::Display, 28.f, C::Sugar);
		DC.Wrap(L(Cards[i].Text), X + 28.f, Y + 168.f, CW - 56.f, EFont::Body, 20.f, WithA(C::Sugar, 0.85f), 5);
	}
	const bool bSteam = S && S->IsSteam();
	// The mode of the hosted game: a raid, or the Human against the ants (AntVersus.h: the first friend who joins plays the Human).
	{
		const bool bVs = AntVersus::Wanted();
		Button(DC, TEXT("c.mode"), bVs ? L(TEXT("coop.mode.versus")) : L(TEXT("coop.mode.raid")), X0 + 28.f, Y0 + CH - 176.f, CW - 56.f, 64.f, []() { AntVersus::Wanted() = !AntVersus::Wanted(); }, false, true, bVs ? TEXT("Human") : TEXT("Squad"));
	}
	// 1: host over Steam.
	Button(DC, TEXT("c.host"), L(TEXT("coop.host.btn")), X0 + 28.f, Y0 + CH - 96.f, CW - 56.f, 64.f, [S]() { if (S) { S->Host(); } }, true, bSteam, TEXT("Ready"));
	// 2: friends' games.
	{
		const float X = X0 + CW + Gap;
		float Y = Y0 + 330.f;
		if (S)
		{
			for (int32 i = 0; i < FMath::Min(S->Games.Num(), 3); ++i)
			{
				Button(DC, FString::Printf(TEXT("c.join%d"), i), Lf(TEXT("coop.join"), { S->Games[i].Label }), X + 28.f, Y, CW - 56.f, 52.f, [S, i]() { S->Join(i); }, false, true);
				Y += 62.f;
			}
		}
		Button(DC, TEXT("c.find"), S && S->bSearching ? L(TEXT("coop.searching")) : L(TEXT("coop.find.btn")), X + 28.f, Y0 + CH - 96.f, CW - 56.f, 64.f, [S]() { if (S) { S->Find(); } }, true, bSteam && !(S && S->bSearching), TEXT("Lens"));
	}
	// 3: by address (tests without Steam: -NOSTEAM, port 7797).
	{
		const float X = X0 + 2.f * (CW + Gap);
		EditField(DC, TEXT("c.addr"), &Self->JoinAddress, X + 28.f, Y0 + 330.f, CW - 56.f, 58.f, TEXT("127.0.0.1:7797"));
		Button(DC, TEXT("c.connect"), L(TEXT("coop.connect")), X + 28.f, Y0 + CH - 176.f, CW - 56.f, 64.f, [WPC, Self, S]()
		{
			if (!WPC.IsValid()) { return; }
			if (S) { S->bMenuDone = true; }
			WPC->JoinGame(Self->JoinAddress);
		}, true, true, TEXT("Radio"));
		Button(DC, TEXT("c.listen"), L(TEXT("coop.listen")), X + 28.f, Y0 + CH - 96.f, CW - 56.f, 64.f, [WPC, S]()
		{
			if (!WPC.IsValid()) { return; }
			if (S) { S->bMenuDone = true; }
			UGameplayStatics::OpenLevel(WPC.Get(), FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen?Port=7797"));
		}, false, true);
	}
	if (S && !S->Status.IsEmpty())
	{
		DC.Glass(X0, Y0 + CH + 30.f, CW * 3.f + Gap * 2.f, 60.f, 0.8f);
		DC.TextFit(S->Status, 960.f, Y0 + CH + 44.f, CW * 3.f, EFont::Body, 21.f, C::Honey, 1);
	}
	Button(DC, TEXT("c.back"), L(TEXT("back")), 70.f, 930.f, 220.f, 60.f, [Self]() { Self->Back(); }, false, true);
	PaintFooter(DC, true);
}

// ---------------------------------------------------------------------------------------------------- colony

void SAntUiMenu::PaintColony(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgNest"), 0.5f, true);
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.35f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	UAntSession* S = Session();
	DC.Text(L(TEXT("colony.title")), 960.f, 60.f, EFont::Display, 54.f, C::Honey, 1, 0.6f);

	// The Queen behind her curtain.
	DC.RRect(150.f, 180.f, 520.f, 600.f, 12.f, WithA(C::Ink, 0.85f));
	DC.Box(170.f, 200.f, 480.f, 480.f, Brush(TEXT("Portraits/T_UI_PorQueen")), FLinearColor::White);
	DC.Text(L(TEXT("colony.queen")), 410.f, 700.f, EFont::Display, 22.f, C::Honey, 1);
	DC.Text(L(TEXT("colony.queen.sub")), 410.f, 738.f, EFont::Hand, 22.f, WithA(C::Sugar, 0.85f), 1);

	// The colony's state on a receipt.
	DC.Paper(740.f, 190.f, 1030.f, 380.f);
	float Y = 226.f;
	DC.Text(L(TEXT("colony.report")), 780.f, Y, EFont::Display, 26.f, C::Graphite);
	Y += 52.f;
	const FString Sum = AntNest::Summary();
	Y += DC.Wrap(Sum.IsEmpty() ? L(TEXT("main.note.nocolony")) : Sum, 780.f, Y, 950.f, EFont::Mono, 19.f, C::Graphite, 6) + 14.f;
	int32 BestShifts = 0, BestHaul = 0;
	AAntGameState::LoadBestRun(BestShifts, BestHaul);
	if (BestShifts > 0 || BestHaul > 0) { DC.Text(Lf(TEXT("main.note.best"), { BestShifts, BestHaul }), 780.f, Y, EFont::Mono, 19.f, C::Graphite); Y += 34.f; }
	DC.Wrap(L(TEXT("colony.hint")), 780.f, 500.f, 950.f, EFont::Hand, 22.f, C::JamDeep, 2);
	if (!Sum.IsEmpty()) { DC.Stamp(L(TEXT("colony.stamp")), 1630.f, 260.f, -9.f, C::JamDeep, 22.f); }

	// The cast: who the radio talks about.
	DC.Text(L(TEXT("colony.cast")), 740.f, 612.f, EFont::Display, 24.f, C::Sugar, 0, 0.5f);
	const TCHAR* Cast[6] = { TEXT("PorZhvalov"), TEXT("PorGruzd"), TEXT("PorUsik"), TEXT("PorTikhon"), TEXT("PorKapelka"), TEXT("PorVintik") };
	const TCHAR* CastKey[6] = { TEXT("cast.zhvalov"), TEXT("cast.gruzd"), TEXT("cast.usik"), TEXT("cast.tikhon"), TEXT("cast.kapelka"), TEXT("cast.vintik") };
	for (int32 i = 0; i < 6; ++i)
	{
		const float X = 740.f + i * 174.f, PY = 660.f;
		const float Tilt = (i % 2 ? 2.f : -2.f);
		DC.BoxRot(DC.P(X + 78.f, PY + 84.f), FVector2D(160.f, 176.f) * DC.U, Tilt, White(), C::Sugar);
		DC.BoxRot(DC.P(X + 78.f, PY + 76.f), FVector2D(144.f, 144.f) * DC.U, Tilt, Brush(*FString::Printf(TEXT("Portraits/T_UI_%s"), Cast[i])), FLinearColor::White);
		DC.TextFit(L(CastKey[i]), X + 78.f, PY + 178.f, 166.f, EFont::Hand, 19.f, C::Sugar, 1);
	}

	const bool bArmed = FPlatformTime::Seconds() - NewColonyArmed < 5.0;
	Button(DC, TEXT("col.new"), bArmed ? L(TEXT("colony.new.sure")) : L(TEXT("colony.new")), 1350.f, 930.f, 420.f, 60.f, [Self, S]()
	{
		if (FPlatformTime::Seconds() - Self->NewColonyArmed < 5.0)
		{
			Self->NewColonyArmed = -100.0;
			AntNest::Reset();
			if (S) { S->Status = L(TEXT("colony.new.done")); S->OnChanged.Broadcast(); }
		}
		else { Self->NewColonyArmed = FPlatformTime::Seconds(); }
	}, false, !Sum.IsEmpty());
	Button(DC, TEXT("col.play"), L(TEXT("main.play")), 1000.f, 930.f, 320.f, 60.f, [S]() { if (S) { S->PlaySolo(); } }, true, true);
	Button(DC, TEXT("col.back"), L(TEXT("back")), 70.f, 930.f, 220.f, 60.f, [Self]() { Self->Back(); }, false, true);
	PaintFooter(DC, true);
}

// ---------------------------------------------------------------------------------------------------- lobby

void SAntUiMenu::PaintLobby(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgWarRoom"), 0.35f, false);
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.35f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	TWeakObjectPtr<AAntPlayerController> WPC = PC;
	UWorld* World = PC.IsValid() ? PC->GetWorld() : nullptr;
	const AAntGameState* GS = World ? World->GetGameState<AAntGameState>() : nullptr;
	AAntRaid* Raid = World ? AAntRaid::Get(World) : nullptr;
	const bool bHost = World && World->GetNetMode() != NM_Client;
	const AAntPlayerState* Me = PC.IsValid() ? PC->GetPlayerState<AAntPlayerState>() : nullptr;
	UAntSession* S = Session();

	DC.Text(L(TEXT("lobby.title")), 70.f, 40.f, EFont::Display, 52.f, C::Honey, 0, 0.6f);
	const FString Net = World && World->GetNetMode() == NM_Standalone ? L(TEXT("lobby.solo")) : (S && S->IsSteam() ? L(TEXT("lobby.steam")) : L(TEXT("lobby.ip")));
	DC.Text(Net, 74.f, 116.f, EFont::Body, 22.f, C::Sugar, 0, 0.5f);

	// ---- eight slots
	TArray<APlayerState*> Players;
	if (GS) { for (APlayerState* P : GS->PlayerArray) { if (P && !P->IsABot()) { Players.Add(P); } } }
	Players.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	const float SW = 300.f, SH = 236.f, SG = 18.f, SX0 = 70.f, SY0 = 170.f;
	for (int32 i = 0; i < 8; ++i)
	{
		const float X = SX0 + (i % 4) * (SW + SG), Y = SY0 + (i / 4) * (SH + SG) + 20.f * (1.f - Ease((D.T - 0.04f * i) / 0.4f));
		const AAntPlayerState* P = Players.IsValidIndex(i) ? Cast<AAntPlayerState>(Players[i]) : nullptr;
		if (!P)
		{
			DC.RRect(X, Y, SW, SH, 12.f, WithA(C::Ink, 0.55f));
			DC.Frame(X + 10.f, Y + 10.f, SW - 20.f, SH - 20.f, 2.f, WithA(C::Sugar, 0.15f));
			DC.Text(L(TEXT("lobby.free")), X + SW * 0.5f, Y + SH * 0.5f - 16.f, EFont::Hand, 26.f, WithA(C::Sugar, 0.4f), 1);
			continue;
		}
		const FLinearColor Col = PlayerColor(i, P->AntColor);
		DC.Glass(X, Y, SW, SH, 0.9f);
		DC.Rect(X, Y, SW, 6.f, Col);
		DC.Box(X + 16.f, Y + 22.f, 120.f, 120.f, Brush(CastePortrait((int32)P->AntClass)), FLinearColor::White);
		DC.Frame(X + 16.f, Y + 22.f, 120.f, 120.f, 2.f, Col);
		DC.TextFit(P->GetPlayerName(), X + 150.f, Y + 26.f, SW - 164.f, EFont::Bold, 24.f, C::Sugar);
		DC.Text(CasteName((int32)P->AntClass), X + 150.f, Y + 62.f, EFont::Body, 20.f, C::Muted);
		if (i == 0) { DC.RRect(X + 150.f, Y + 100.f, 76.f, 26.f, 6.f, C::Amber); DC.Text(L(TEXT("lobby.host")), X + 188.f, Y + 101.f, EFont::Bold, 17.f, C::Ink, 1); }
		if (P->bIsHuman) { DC.RRect(X + 232.f, Y + 100.f, 56.f, 26.f, 6.f, C::Teal); DC.Text(L(TEXT("lobby.giant")), X + 260.f, Y + 101.f, EFont::Bold, 17.f, C::Ink, 1); }
		if (P == Me) { DC.Text(L(TEXT("lobby.you")), X + SW - 16.f, Y + 150.f, EFont::Hand, 20.f, C::Honey, 2); }
		if (P->bNestReady) { DC.Stamp(L(TEXT("lobby.ready")), X + SW * 0.5f, Y + 192.f, -6.f, C::Leaf, 24.f); }
		else { DC.Text(L(TEXT("lobby.notready")), X + SW * 0.5f, Y + 178.f, EFont::Display, 20.f, WithA(C::Sugar, 0.35f), 1); }
	}
	const int32 Ready = Players.FilterByPredicate([](const APlayerState* P) { const AAntPlayerState* A = Cast<AAntPlayerState>(P); return A && A->bNestReady; }).Num();
	DC.Text(Lf(TEXT("lobby.count"), { Players.Num(), Ready }), 70.f, 700.f, EFont::Bold, 24.f, C::Sugar, 0, 0.5f);
	if (S && S->IsSteam() && S->HasSession())
	{
		Button(DC, TEXT("l.invite"), L(TEXT("pause.invite")), 900.f, 690.f, 368.f, 52.f, [S]() { S->Invite(); }, false, true, TEXT("Friends"));
	}

	// ---- my ant
	const float RX = 1350.f;
	DC.Glass(RX, 170.f, 500.f, 236.f, 0.9f);
	DC.Text(L(TEXT("lobby.mine")), RX + 24.f, 186.f, EFont::Display, 22.f, C::Honey);
	{
		const int32 Caste = Me ? (int32)Me->AntClass : 0;
		const int32 Id = AddItem(DC, TEXT("l.caste"), RX + 20.f, 228.f, 460.f, 64.f, Me != nullptr, [WPC, Caste]()
		{
			SetCaste(WPC.Get(), (Caste + 1) % 5);
		}, [WPC, Caste](int32 Dir)
		{
			SetCaste(WPC.Get(), (Caste + Dir + 5) % 5);
		});
		const float K = Ease(Anim(Id));
		DC.RRect(RX + 20.f, 228.f, 460.f, 64.f, 8.f, FMath::Lerp(WithA(C::Cocoa, 0.6f), WithA(C::Cocoa, 1.f), K));
		DC.Box(RX + 28.f, 232.f, 56.f, 56.f, IconBrush(Caste == 1 ? TEXT("Soldier") : Caste == 2 ? TEXT("Scout") : Caste == 3 ? TEXT("Barrel") : Caste == 4 ? TEXT("Wrench") : TEXT("Forager")), FLinearColor::White);
		DC.Text(TEXT("‹"), RX + 100.f, 236.f, EFont::Display, 28.f, WithA(C::Amber, 0.5f + 0.5f * K));
		DC.Text(CasteName(Caste), RX + 270.f, 244.f, EFont::Bold, 24.f, FMath::Lerp(C::Sugar, C::Honey, K), 1);
		DC.Text(TEXT("›"), RX + 450.f, 236.f, EFont::Display, 28.f, WithA(C::Amber, 0.5f + 0.5f * K));
	}
	const bool bReady = Me && Me->bNestReady;
	Button(DC, TEXT("l.ready"), bReady ? L(TEXT("lobby.unready")) : L(TEXT("lobby.ready.btn")), RX + 20.f, 314.f, 460.f, 72.f, [WPC, bReady]()
	{
		if (WPC.IsValid()) { WPC->ServerRaid(AntRaidCmd::Ready, bReady ? 0 : 1, 0); }
	}, !bReady, Raid != nullptr, TEXT("Ready"));

	// ---- the flat (seed) and the mode
	DC.Glass(RX, 424.f, 500.f, 236.f, 0.9f);
	DC.Text(L(TEXT("lobby.seed")), RX + 24.f, 440.f, EFont::Display, 22.f, C::Honey);
	const FString Seed = LevelV3::Enabled() ? LevelV3::ShortText() : L(TEXT("lobby.seed.none"));
	DC.Wrap(Seed, RX + 24.f, 472.f, 452.f, EFont::Mono, 15.f, C::Sugar, 2, 1.15f);
	EditField(DC, TEXT("l.seedtxt"), &Self->SeedText, RX + 20.f, 520.f, 220.f, 52.f, L(TEXT("lobby.seed.hint")));
	Button(DC, TEXT("l.seedgo"), L(TEXT("lobby.seed.apply")), RX + 250.f, 520.f, 230.f, 52.f, [WPC, Self]()
	{
		if (WPC.IsValid() && Self->SeedText.IsNumeric()) { WPC->ServerRaid(AntRaidCmd::Seed, FCString::Atoi(*Self->SeedText), 0); }
	}, false, bHost && LevelV3::Enabled());
	Button(DC, TEXT("l.reroll"), L(TEXT("lobby.seed.reroll")), RX + 20.f, 588.f, 460.f, 52.f, [WPC]() { if (WPC.IsValid()) { WPC->ServerRaid(AntRaidCmd::Seed, -1, 0); } }, false, bHost && LevelV3::Enabled(), TEXT("Dice"));

	DC.Glass(RX, 678.f, 500.f, 206.f, 0.9f);
	DC.Text(L(TEXT("lobby.mode")), RX + 24.f, 694.f, EFont::Display, 22.f, C::Honey);
	// The host picks the raid or "the Human against the ants" (AntVersus.h); anybody can ask to be the Giant himself.
	const bool bVs = AntVersus::Wanted() || AntVersus::Active(World);
	Button(DC, TEXT("l.raid"), L(TEXT("lobby.mode.raid")), RX + 20.f, 734.f, 225.f, 58.f, []() { AntVersus::Wanted() = false; }, !bVs, bHost, TEXT("Squad"));
	Button(DC, TEXT("l.versus"), L(TEXT("lobby.mode.versus")), RX + 255.f, 734.f, 225.f, 58.f, []() { AntVersus::Wanted() = true; }, bVs, bHost, TEXT("Human"));
	const bool bGiant = Me && Me->bIsHuman;
	Button(DC, TEXT("l.giant"), bGiant ? L(TEXT("lobby.giant.off")) : L(TEXT("lobby.giant.on")), RX + 20.f, 804.f, 460.f, 58.f, [WPC]() { if (WPC.IsValid()) { WPC->ServerToggleHuman(); } }, false, true, TEXT("Slipper"));

	// ---- the difficulty (KAN-30) and who decides for the colony (KAN-31): the host's switches, replicated, shown to everybody
	{
		const uint8 Diff = World ? AntDifficulty::Level(World) : (uint8)AntDiff::Normal;
		DC.Glass(70.f, 750.f, 620.f, 160.f, 0.9f);
		DC.Text(L(TEXT("lobby.diff")), 94.f, 764.f, EFont::Display, 22.f, C::Honey);
		if (!bHost) { DC.Text(L(TEXT("lobby.diff.host")), 666.f, 768.f, EFont::Hand, 18.f, C::Muted, 2); }
		for (int32 i = 0; i < AntDiff::Count; ++i)
		{
			Button(DC, FString::Printf(TEXT("l.diff%d"), i), L(*FString::Printf(TEXT("lobby.diff.%d"), i)), 90.f + 196.f * i, 800.f, 186.f, 52.f, [WPC, i]()
			{
				if (WPC.IsValid()) { WPC->ServerRaid(AntRaidCmdX::Difficulty, i, 0); }
			}, Diff == i, bHost && Raid != nullptr);
		}
		const TArray<FString> Lines = AntDifficulty::Lines(Diff);
		DC.Wrap(FString::Join(Lines, TEXT(" · ")), 94.f, 862.f, 580.f, EFont::Body, 15.f, AntDifficulty::Color(Diff), 2, 1.1f);
	}
	if (World && World->GetNetMode() != NM_Standalone)
	{
		const bool bVote = AntColonyVote::Mode(World) == AntVoteMode::Vote;
		DC.Glass(710.f, 750.f, 600.f, 160.f, 0.9f);
		DC.Text(L(TEXT("lobby.vote")), 734.f, 764.f, EFont::Display, 22.f, C::Honey);
		Button(DC, TEXT("l.vote0"), L(TEXT("lobby.vote.host")), 730.f, 800.f, 276.f, 52.f, [WPC]() { if (WPC.IsValid()) { WPC->ServerRaid(AntRaidCmdX::VoteMode, 0, 0); } }, !bVote, bHost && Raid != nullptr);
		Button(DC, TEXT("l.vote1"), L(TEXT("lobby.vote.squad")), 1016.f, 800.f, 276.f, 52.f, [WPC]() { if (WPC.IsValid()) { WPC->ServerRaid(AntRaidCmdX::VoteMode, 1, 0); } }, bVote, bHost && Raid != nullptr);
		DC.Wrap(L(bVote ? TEXT("lobby.vote.text.1") : TEXT("lobby.vote.text.0")), 734.f, 862.f, 560.f, EFont::Body, 15.f, C::Sugar, 2, 1.1f);
	}

	// ---- start
	const float Left = Raid && Raid->StartEnds > 0.f && GS ? Raid->StartEnds - GS->GetServerWorldTimeSeconds() : -1.f;
	if (Left > 0.f)
	{
		DC.Text(Lf(TEXT("lobby.countdown"), { FMath::CeilToInt(Left) }), 660.f, 690.f, EFont::Display, 44.f, C::Amber, 1, 0.6f);   // (above the two panels)
	}
	if (bHost)
	{
		Button(DC, TEXT("l.go"), Left > 0.f ? L(TEXT("lobby.go.cancel")) : L(TEXT("lobby.go")), RX, 900.f, 500.f, 84.f, [WPC]() { if (WPC.IsValid()) { WPC->ServerRaid(AntRaidCmd::Start, 0, 0); } }, true, Raid != nullptr, TEXT("Pin"));
	}
	else
	{
		DC.Glass(RX, 900.f, 500.f, 84.f, 0.8f);
		DC.Text(L(TEXT("lobby.wait")), RX + 250.f, 924.f, EFont::Display, 22.f, C::Muted, 1);
	}
	Button(DC, TEXT("l.hq"), L(TEXT("lobby.hq")), 70.f, 930.f, 360.f, 60.f, [Self]() { Self->CloseOrBack(); }, false, true, TEXT("Clipboard"));
	PaintFooter(DC, true);
}

// ---------------------------------------------------------------------------------------------------- settings

void SAntUiMenu::SettingsBegin()
{
	SavedUi = FAntUiSettings::Get();
	SavedGfx = FAntGfx::Current();
	EditUi = SavedUi;
	EditGfx = SavedGfx;
}

void SAntUiMenu::SettingsApply()
{
	const bool bGfx = !(EditGfx == SavedGfx);
	FAntUiSettings::Get() = EditUi;
	EditUi.Save();
	EditUi.Apply(PC.Get());
	if (bGfx) { EditGfx.Apply(); }
	SavedUi = EditUi;
	SavedGfx = EditGfx;
	AntUiSound(EAntUiSound::Stamp, PC.Get());
}

void SAntUiMenu::SettingsRevert()
{
	EditUi = SavedUi;
	EditGfx = SavedGfx;
}

void SAntUiMenu::PaintSettings(FUiDraw& D) const
{
	if (IsOpaque(ReturnTo)) { PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgWarRoom"), 0.62f, false); }
	else { D.BoxL(FVector2D::ZeroVector, D.Size, White(), WithA(C::Ink, 0.86f)); }
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.3f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	DC.Text(L(TEXT("settings.title")), 960.f, 40.f, EFont::Display, 50.f, C::Honey, 1, 0.6f);
	Tabs(DC, { L(TEXT("settings.gfx")), L(TEXT("settings.audio")), L(TEXT("settings.controls")), L(TEXT("settings.game")) }, SettingsTab, 128.f, [Self](int32 i) { Self->SettingsTab = i; });

	DC.Glass(270.f, 200.f, 1380.f, 720.f, 0.9f);
	FAntUiSettings& U = Self->EditUi;
	FAntGfx& G = Self->EditGfx;
	float Y = 220.f;
	const float Step = 66.f;
	auto Cycle = [](int32 V, int32 Dir, int32 N) { return (V + Dir + N) % N; };
	if (SettingsTab == 0)
	{
		static const TCHAR* PresetKeys[5] = { TEXT("q.low"), TEXT("q.med"), TEXT("q.high"), TEXT("q.epic"), TEXT("q.cine") };
		OptionRow(DC, TEXT("s.preset"), L(TEXT("s.preset")), L(PresetKeys[FMath::Clamp(G.Preset, 0, 4)]), Y, [&G, Cycle](int32 Dir) { G.Preset = Cycle(G.Preset, Dir, 5); }); Y += Step;
		const TArray<FIntPoint> Res = FAntGfx::Resolutions();
		OptionRow(DC, TEXT("s.res"), L(TEXT("s.res")), FString::Printf(TEXT("%d × %d"), G.Res.X, G.Res.Y), Y, [&G, Res](int32 Dir)
		{
			int32 I = Res.IndexOfByKey(G.Res);
			I = I == INDEX_NONE ? Res.Num() - 1 : (I + Dir + Res.Num()) % Res.Num();
			if (Res.IsValidIndex(I)) { G.Res = Res[I]; }
		}); Y += Step;
		static const TCHAR* ModeKeys[3] = { TEXT("w.full"), TEXT("w.borderless"), TEXT("w.window") };
		OptionRow(DC, TEXT("s.mode"), L(TEXT("s.mode")), L(ModeKeys[FMath::Clamp(G.Mode, 0, 2)]), Y, [&G, Cycle](int32 Dir) { G.Mode = Cycle(G.Mode, Dir, 3); }); Y += Step;
		ToggleRow(DC, TEXT("s.vsync"), L(TEXT("s.vsync")), G.bVSync, Y, [&G]() { G.bVSync = !G.bVSync; }); Y += Step;
		static const int32 Caps[6] = { 0, 30, 60, 120, 144, 240 };
		int32 CapI = 0;
		for (int32 i = 0; i < 6; ++i) { if (Caps[i] == G.FrameCap) { CapI = i; } }
		OptionRow(DC, TEXT("s.cap"), L(TEXT("s.cap")), G.FrameCap <= 0 ? L(TEXT("s.cap.none")) : FString::Printf(TEXT("%d"), G.FrameCap), Y, [&G, CapI, Cycle](int32 Dir) { G.FrameCap = Caps[Cycle(CapI, Dir, 6)]; }); Y += Step;
		const int32 NumAA = FAntUiSettings::HasDlss() ? 5 : 4;
		static const TCHAR* AAKeys[5] = { TEXT("aa.off"), TEXT("aa.fxaa"), TEXT("aa.taa"), TEXT("aa.tsr"), TEXT("aa.dlss") };
		OptionRow(DC, TEXT("s.aa"), L(TEXT("s.aa")), L(AAKeys[FMath::Clamp(U.AntiAliasing, 0, 4)]), Y, [&U, NumAA, Cycle](int32 Dir) { U.AntiAliasing = Cycle(U.AntiAliasing, Dir, NumAA); }, NumAA == 4 ? *L(TEXT("s.aa.nodlss")) : nullptr); Y += Step;
		ToggleRow(DC, TEXT("s.blur"), L(TEXT("s.blur")), U.bMotionBlur, Y, [&U]() { U.bMotionBlur = !U.bMotionBlur; }); Y += Step;
		ToggleRow(DC, TEXT("s.dust"), L(TEXT("s.dust")), U.bDust, Y, [&U]() { U.bDust = !U.bDust; }); Y += Step;
	}
	else if (SettingsTab == 1)
	{
		static const TCHAR* VolKeys[AntVol::Count] = { TEXT("v.master"), TEXT("v.music"), TEXT("v.sfx"), TEXT("v.voice"), TEXT("v.ui") };
		for (int32 i = 0; i < AntVol::Count; ++i)
		{
			SliderRow(DC, VolKeys[i], L(VolKeys[i]), U.Vol[i], FString::Printf(TEXT("%d%%"), FMath::RoundToInt(U.Vol[i] * 100.f)), Y, [&U, i](float F) { U.Vol[i] = FMath::RoundToFloat(F * 20.f) / 20.f; });
			Y += Step;
		}
		DC.Wrap(L(TEXT("s.audio.note")), 300.f, Y + 20.f, 1320.f, EFont::Body, 19.f, C::Muted, 3);
	}
	else if (SettingsTab == 2)
	{
		SliderRow(DC, TEXT("s.sens"), L(TEXT("s.sens")), (U.MouseSens - 0.2f) / 2.8f, FString::Printf(TEXT("×%.2f"), U.MouseSens), Y, [&U](float F) { U.MouseSens = FMath::RoundToFloat((0.2f + F * 2.8f) * 20.f) / 20.f; }); Y += Step;
		ToggleRow(DC, TEXT("s.invert"), L(TEXT("s.invert")), U.bInvertY, Y, [&U]() { U.bInvertY = !U.bInvertY; }); Y += Step + 10.f;
		// The key list: actions from the input settings (what really is bound), then the keys bound in code.
		TArray<TPair<FString, FString>> Keys;
		if (const UInputSettings* IS = UInputSettings::GetInputSettings())
		{
			for (const TCHAR* Action : { TEXT("Fire"), TEXT("AltFire"), TEXT("Interact"), TEXT("Jump"), TEXT("Sprint"), TEXT("LayTrail"), TEXT("RecallSwarm"), TEXT("Scoreboard"), TEXT("ToggleHuman") })
			{
				TArray<FInputActionKeyMapping> Maps;
				IS->GetActionMappingByName(FName(Action), Maps);
				FString KeysText;
				for (const FInputActionKeyMapping& M : Maps)
				{
					if (M.Key.IsGamepadKey()) { continue; }
					KeysText += (KeysText.IsEmpty() ? TEXT("") : TEXT(" / ")) + M.Key.GetDisplayName().ToString();
				}
				if (!KeysText.IsEmpty()) { Keys.Emplace(L(*(FString(TEXT("act.")) + Action)), KeysText); }
			}
		}
		Keys.Emplace(L(TEXT("act.Move")), TEXT("W A S D"));
		for (const TCHAR* K : { TEXT("Esc"), TEXT("Q"), TEXT("B"), TEXT("5 / 6"), TEXT("1 / 2 / 3"), TEXT("Caps Lock"), TEXT("L"), TEXT("F1"), TEXT("F8") })
		{
			Keys.Emplace(L(*(FString(TEXT("key.")) + K)), K);
		}
		for (int32 i = 0; i < Keys.Num(); ++i)
		{
			const float KX = 300.f + (i % 2) * 670.f, KY = Y + (i / 2) * 44.f;
			DC.Key(Keys[i].Value, KX, KY, 16.f);
			DC.TextFit(Keys[i].Key, KX + 190.f, KY + 1.f, 460.f, EFont::Body, 19.f, C::Sugar);
		}
	}
	else
	{
		OptionRow(DC, TEXT("s.lang"), L(TEXT("s.lang")), U.Language == 0 ? TEXT("Русский") : TEXT("English"), Y, [&U](int32) { U.Language = 1 - U.Language; }); Y += Step;
		ToggleRow(DC, TEXT("s.subs"), L(TEXT("s.subs")), U.bSubtitles, Y, [&U]() { U.bSubtitles = !U.bSubtitles; }); Y += Step;
		SliderRow(DC, TEXT("s.hud"), L(TEXT("s.hud")), (U.HudScale - 0.75f) / 0.75f, FString::Printf(TEXT("%d%%"), FMath::RoundToInt(U.HudScale * 100.f)), Y, [&U](float F) { U.HudScale = FMath::RoundToFloat((0.75f + F * 0.75f) * 20.f) / 20.f; }); Y += Step;
		ToggleRow(DC, TEXT("s.cb"), L(TEXT("s.cb")), U.bColorblind, Y, [&U]() { U.bColorblind = !U.bColorblind; }); Y += Step;
		// The colour-blind palette, shown.
		for (int32 i = 0; i < 8; ++i)
		{
			static const uint32 Safe[8] = { 0xE69F00, 0x56B4E9, 0x009E73, 0xF0E442, 0x0072B2, 0xD55E00, 0xCC79A7, 0xBBBBBB };
			static const uint32 Warm[8] = { 0xF2A93B, 0x5FB3B0, 0x86C95A, 0xCDA8FF, 0xFFD27A, 0x58A9E0, 0xD9473B, 0xFBF5E6 };
			const FLinearColor Col = Hex(U.bColorblind ? Safe[i] : Warm[i]);
			DC.RRect(300.f + i * 74.f, Y + 6.f, 60.f, 34.f, 6.f, Col);
		}
		DC.Text(L(TEXT("s.cb.note")), 900.f, Y + 10.f, EFont::Body, 19.f, C::Muted);
		Y += Step;
		DC.Wrap(L(TEXT("s.lang.note")), 300.f, Y + 10.f, 1320.f, EFont::Body, 19.f, C::Muted, 3);
	}

	const bool bDirty = !(EditUi == SavedUi) || !(EditGfx == SavedGfx);
	Button(DC, TEXT("s.apply"), L(TEXT("s.apply")), 1430.f, 940.f, 260.f, 64.f, [Self]() { Self->SettingsApply(); }, true, bDirty, TEXT("Stamp"));
	Button(DC, TEXT("s.revert"), L(TEXT("s.revert")), 1150.f, 940.f, 260.f, 64.f, [Self]() { Self->SettingsRevert(); }, false, bDirty);
	Button(DC, TEXT("s.back"), L(TEXT("back")), 230.f, 940.f, 220.f, 64.f, [Self]() { Self->Back(); }, false, true);
	if (bDirty) { DC.Text(L(TEXT("s.unsaved")), 1140.f, 958.f, EFont::Hand, 22.f, C::Honey, 2); }
}

// ---------------------------------------------------------------------------------------------------- credits

void SAntUiMenu::PaintCredits(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgNest"), 0.7f, true);
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.4f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	struct FLine { int32 Kind; const TCHAR* Key; };   // 0 heading, 1 name, 2 small, 3 gap
	static const FLine Lines[] =
	{
		{ 4, nullptr }, { 3, nullptr },
		{ 0, TEXT("cr.team") }, { 1, TEXT("cr.yura") }, { 1, TEXT("cr.kirill") }, { 1, TEXT("cr.danila") }, { 1, TEXT("cr.denis") }, { 3, nullptr },
		{ 0, TEXT("cr.code") }, { 1, TEXT("cr.claude") }, { 2, TEXT("cr.claude.note") }, { 3, nullptr },
		{ 0, TEXT("cr.art") }, { 1, TEXT("cr.higgsfield") }, { 2, TEXT("cr.ai.note") }, { 1, TEXT("cr.meshy") }, { 3, nullptr },
		{ 0, TEXT("cr.music") }, { 1, TEXT("cr.music.tbd") }, { 1, TEXT("cr.sfx.tbd") }, { 3, nullptr },
		{ 0, TEXT("cr.fonts") }, { 1, TEXT("cr.font.russo") }, { 1, TEXT("cr.font.pt") }, { 1, TEXT("cr.font.neucha") }, { 2, TEXT("cr.font.ofl") }, { 3, nullptr },
		{ 0, TEXT("cr.engine") }, { 1, TEXT("cr.ue") }, { 3, nullptr },
		{ 0, TEXT("cr.thanks") }, { 1, TEXT("cr.thanks.text") }, { 3, nullptr }, { 3, nullptr },
		{ 2, TEXT("cr.end") },
	};
	float Y = 1000.f - CreditsScroll;
	for (const FLine& Ln : Lines)
	{
		switch (Ln.Kind)
		{
		case 0: DC.Text(L(Ln.Key), 960.f, Y, EFont::Display, 30.f, C::Amber, 1, 0.6f); Y += 56.f; break;
		case 1: DC.Text(L(Ln.Key), 960.f, Y, EFont::Bold, 26.f, C::Sugar, 1, 0.6f); Y += 42.f; break;
		case 2: Y += DC.Wrap(L(Ln.Key), 560.f, Y, 800.f, EFont::Body, 20.f, C::Muted, 4, 1.25f, 1) + 8.f; break;
		case 3: Y += 50.f; break;
		case 4:
			if (const FSlateBrush* Logo = Brush(TEXT("Logo/T_UI_Logo"))) { DC.Box(960.f - 280.f, Y, 560.f, 398.f, Logo, FLinearColor::White); }
			Y += 420.f;
			break;
		}
	}
	if (Y < -200.f) { Self->CreditsScroll = 0.f; }
	Button(DC, TEXT("cr.back"), L(TEXT("back")), 70.f, 930.f, 220.f, 60.f, [Self]() { Self->Back(); }, false, true);
}

// ---------------------------------------------------------------------------------------------------- pause

void SAntUiMenu::PaintPause(FUiDraw& D) const
{
	D.BoxL(FVector2D::ZeroVector, D.Size, White(), WithA(C::Ink, 0.62f * Ease(D.T / 0.25f)));
	const FSlateBrush* FadeH = Brush(TEXT("Materials/T_UI_FadeH"));
	D.BoxL(FVector2D::ZeroVector, FVector2D(D.O.X + 900.f * D.U, D.Size.Y), FadeH ? FadeH : White(), WithA(C::Ink, 0.8f));
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.25f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	UAntSession* S = Session();
	TWeakObjectPtr<AAntPlayerController> WPC = PC;
	UWorld* World = PC.IsValid() ? PC->GetWorld() : nullptr;
	const bool bNet = World && World->GetNetMode() != NM_Standalone;
	DC.Text(L(TEXT("pause.title")), 120.f, 160.f, EFont::Display, 64.f, C::Honey, 0, 0.6f);
	if (bNet) { DC.Stamp(L(TEXT("pause.live")), 560.f, 200.f, -7.f, C::Jam, 20.f); }
	float Y = 330.f;
	const float Step = 66.f;
	MenuButton(DC, TEXT("p.resume"), L(TEXT("pause.resume")), 120.f, Y, [Self]() { Self->CloseOrBack(); }); Y += Step;
	MenuButton(DC, TEXT("p.settings"), L(TEXT("main.settings")), 120.f, Y, [Self]() { Self->Go(AntUiScreen::Settings); }); Y += Step;
	if (S && S->IsSteam() && S->HasSession()) { MenuButton(DC, TEXT("p.invite"), L(TEXT("pause.invite")), 120.f, Y, [S]() { S->Invite(); }); Y += Step; }
	const AAntRaid* Raid = World ? AAntRaid::Get(World) : nullptr;
	if (Raid && Raid->Phase == AntRaidPhase::Nest) { MenuButton(DC, TEXT("p.lobby"), L(TEXT("pause.lobby")), 120.f, Y, [Self]() { Self->Go(AntUiScreen::Lobby); }); Y += Step; }
	MenuButton(DC, TEXT("p.menu"), L(TEXT("pause.menu")), 120.f, Y, [S]() { if (S) { S->ReturnToMenu(); } }); Y += Step;
	MenuButton(DC, TEXT("p.quit"), L(TEXT("main.quit")), 120.f, Y, [WPC]() { if (WPC.IsValid()) { UKismetSystemLibrary::QuitGame(WPC.Get(), WPC.Get(), EQuitPreference::Quit, false); } });
	if (bNet) { DC.Wrap(L(TEXT("pause.live.note")), 120.f, 860.f, 640.f, EFont::Body, 19.f, C::Muted, 3); }
	PaintFooter(DC, true);
}

// ---------------------------------------------------------------------------------------------------- loading

void SAntUiMenu::PaintLoading(FUiDraw& D) const
{
	const float t = D.T;
	D.BoxL(FVector2D::ZeroVector, D.Size, White(), C::Ink);
	D.Cover(LoadingArts[LoadingArt % UE_ARRAY_COUNT(LoadingArts)], FVector2D::ZeroVector, D.Size, 1.04f + t * 0.006f, FVector2D(-0.4f + t * 0.02f, 0.f), FLinearColor(1.f, 1.f, 1.f, Ease(t / 0.5f)));
	const FSlateBrush* FadeV = Brush(TEXT("Materials/T_UI_FadeV"));
	D.BoxL(FVector2D(0.f, D.Size.Y * 0.55f), FVector2D(D.Size.X, D.Size.Y * 0.45f), FadeV ? FadeV : White(), WithA(C::Ink, 0.95f));
	// The radio of the Crack: an amber dial, the tip, the progress.
	D.Box(120.f, 820.f, 76.f, 76.f, IconBrush(TEXT("Radio")), FLinearColor::White);
	D.Text(L(TEXT("radio.name")), 214.f, 822.f, EFont::Display, 24.f, C::Amber);
	const FString Tip = L(*FString::Printf(TEXT("tip.%d"), LoadingTip));
	D.Wrap(Lf(TEXT("radio.tip"), { Tip }), 214.f, 864.f, 1300.f, EFont::Body, 24.f, C::Sugar, 2);
	const float Prog = 1.f - FMath::Exp(-t * 0.45f);
	D.RRect(120.f, 980.f, 1680.f, 8.f, 4.f, WithA(C::Sugar, 0.15f));
	D.RRect(120.f, 980.f, 1680.f * Prog, 8.f, 4.f, C::Amber);
	const float AntX = 120.f + 1680.f * Prog;
	D.Box(AntX - 26.f, 940.f + 3.f * FMath::Sin(t * 14.f), 52.f, 52.f, IconBrush(TEXT("Carry")), FLinearColor::White);
	D.Text(L(TEXT("loading")), 1800.f, 1000.f, EFont::Hand, 22.f, WithA(C::Muted, 0.6f + 0.4f * FMath::Sin(t * 3.f)), 2);
}

// ---------------------------------------------------------------------------------------------------- «Учебка»: chapters

void SAntUiMenu::PaintTraining(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgWarRoom"), 0.5f, true);
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.35f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	UAntSession* S = Session();
	const AntTut::FProgress& P = AntTut::Progress();
	DC.Text(AntTut::Tx(TEXT("УЧЕБКА СЕРЖАНТА ЖВАЛОВА"), TEXT("SERGEANT ZHVALOV'S TRAINING")), 960.f, 56.f, EFont::Display, 50.f, C::Honey, 1, 0.6f);
	DC.Text(P.bDone ? FString::Printf(TEXT("%s · %d XP"), *AntTut::RankName(P.Rank), P.Xp) : FString(AntTut::Tx(TEXT("Пройди с начала — или выбери главу"), TEXT("Start from the top — or pick a chapter"))),
		960.f, 128.f, EFont::Body, 22.f, C::Sugar, 1, 0.6f);
	// The beats, chapter by chapter, two columns.
	const int32 NB = AntTut::NumBeats();
	for (int32 i = 0; i < NB; ++i)
	{
		const int32 Col = i < (NB + 1) / 2 ? 0 : 1, Row = Col == 0 ? i : i - (NB + 1) / 2;
		const float X = 150.f + Col * 560.f, Y = 190.f + Row * 74.f;
		const bool bDone = (P.BeatsDone & (1u << i)) != 0;
		const AntTut::FBeat& B = AntTut::Beat(i);
		const FString Label = FString::Printf(TEXT("%d. %s%s"), i, *AntTut::BeatTitle(i), AntTut::BranchMerged(B.Branch) ? TEXT("") : TEXT(" *"));
		Button(DC, FString::Printf(TEXT("t.beat%d"), i), Label, X, Y, 520.f, 60.f, [S, i]() { AntTut::SetWanted(AntTut::Training, i); if (S) { S->PlaySolo(); } }, false, true, bDone ? TEXT("Ready") : nullptr);
	}
	// Right: from the top, together, the sandbox.
	const float RX = 1320.f;
	DC.Glass(RX - 20.f, 190.f, 500.f, 560.f, 0.8f);
	Button(DC, TEXT("t.start"), AntTut::Tx(TEXT("С начала"), TEXT("From the top")), RX, 220.f, 460.f, 66.f, [S]() { AntTut::SetWanted(AntTut::Training, 0); if (S) { S->PlaySolo(); } }, true, true, TEXT("Clipboard"));
	Button(DC, TEXT("t.coop"), AntTut::Tx(TEXT("Вдвоём–вчетвером (кооп)"), TEXT("2–4 players (co-op)")), RX, 306.f, 460.f, 66.f, [Self]() { AntTut::SetWanted(AntTut::Training, 0); Self->Go(AntUiScreen::Coop); }, false, true, TEXT("Squad"));
	Button(DC, TEXT("t.sand"), AntTut::IsDemo() ? FString(AntTut::Tx(TEXT("Песочница — в полной версии"), TEXT("Sandbox — full game"))) : FString(AntTut::Tx(TEXT("Песочница"), TEXT("Sandbox"))), RX, 392.f, 460.f, 66.f,
		[S]() { AntTut::SetWanted(AntTut::Sandbox); if (S) { S->PlaySolo(); } }, false, P.bDone && !AntTut::IsDemo(), TEXT("Wrench"));
	DC.Wrap(AntTut::Tx(TEXT("Песочница — вся квартира и инструменты: еда у прицела, Гигант стоп/ход, время суток, цепные реакции, потоп. Открывается после учебки. * — упрощённо, пока в игру не влита ветка."),
		TEXT("Sandbox: the whole flat and tools — food at the crosshair, the Giant stop/go, time of day, chain reactions, flood. Opens after the training. * — simplified until its branch is merged.")),
		RX, 486.f, 460.f, EFont::Body, 18.f, WithA(C::Sugar, 0.75f), 8, 1.2f);
	Button(DC, TEXT("t.back"), L(TEXT("back")), 70.f, 990.f, 220.f, 60.f, [Self]() { Self->Back(); }, false, true);
	PaintFooter(DC, true);
}

// ---------------------------------------------------------------------------------------------------- the demo's end

void SAntUiMenu::PaintDemoEnd(FUiDraw& D) const
{
	PaintBackdrop(D, TEXT("Backgrounds/T_UI_BgCake"), 0.45f, true);
	FUiDraw DC = D;
	DC.A = Ease(D.T / 0.5f);
	SAntUiMenu* Self = const_cast<SAntUiMenu*>(this);
	TWeakObjectPtr<AAntPlayerController> WPC = PC;
	if (const FSlateBrush* Logo = Brush(TEXT("Logo/T_UI_Logo"))) { DC.Box(960.f - 300.f, 40.f, 600.f, 426.f, Logo, FLinearColor::White); }
	DC.Text(AntTut::Tx(TEXT("Спасибо, что играли!"), TEXT("Thanks for playing!")), 960.f, 500.f, EFont::Display, 56.f, C::Honey, 1, 0.6f);
	DC.Wrap(AntTut::Tx(TEXT("Это была демо-версия. В полной игре — четыре угрозы до «Дня рождения», вся колония и её камеры, режим «Человек против муравьёв», песочница и ветераны отряда."),
		TEXT("That was the demo. The full game has four threats up to «Birthday», the whole colony and its chambers, Human vs Ants, the sandbox and squad veterans.")),
		560.f, 580.f, 800.f, EFont::Body, 24.f, C::Sugar, 4, 1.25f);
	Button(DC, TEXT("d.wish"), AntTut::Tx(TEXT("Добавь в желаемое в Steam"), TEXT("Wishlist on Steam")), 660.f, 740.f, 600.f, 76.f, []()
	{
		FPlatformProcess::LaunchURL(TEXT("https://store.steampowered.com/app/0000000/AntKitchen/"), nullptr, nullptr);   // placeholder: the store page id goes here
	}, true, true, TEXT("Ready"));
	Button(DC, TEXT("d.menu"), AntTut::Tx(TEXT("В главное меню"), TEXT("Main menu")), 660.f, 836.f, 290.f, 60.f, [Self]() { Self->Go(AntUiScreen::Main); }, false, true);
	Button(DC, TEXT("d.quit"), L(TEXT("main.quit")), 970.f, 836.f, 290.f, 60.f, [WPC]() { if (WPC.IsValid()) { UKismetSystemLibrary::QuitGame(WPC.Get(), WPC.Get(), EQuitPreference::Quit, false); } }, false, true);
	PaintFooter(DC, true);
}