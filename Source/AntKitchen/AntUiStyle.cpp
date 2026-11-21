#include "AntUiStyle.h"
#include "AntUiSettings.h"
#include "Fonts/CompositeFont.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

namespace AntUi
{
	FLinearColor Hex(uint32 RGB, float A)
	{
		FLinearColor L = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255));
		L.A = A;
		return L;
	}

	FLinearColor WithA(const FLinearColor& Col, float A)
	{
		return FLinearColor(Col.R, Col.G, Col.B, A);
	}

	namespace C
	{
		const FLinearColor Ink = Hex(0x1B120B);
		const FLinearColor InkSoft = Hex(0x2B1D12);
		const FLinearColor Cocoa = Hex(0x4A3020);
		const FLinearColor Amber = Hex(0xF2A93B);
		const FLinearColor AmberDeep = Hex(0xC8731E);
		const FLinearColor Honey = Hex(0xFFD27A);
		const FLinearColor Sugar = Hex(0xFBF5E6);
		const FLinearColor Paper = Hex(0xEFE3C8);
		const FLinearColor PaperShade = Hex(0xD9C7A3);
		const FLinearColor Graphite = Hex(0x33271F);
		const FLinearColor Muted = Hex(0xB3A285);
		const FLinearColor Leaf = Hex(0x86C95A);
		const FLinearColor LeafDeep = Hex(0x3F7D2A);
		const FLinearColor Jam = Hex(0xD9473B);
		const FLinearColor JamDeep = Hex(0x9A2620);
		const FLinearColor Acid = Hex(0xA8E04F);
		const FLinearColor Jelly = Hex(0xCDA8FF);
		const FLinearColor JellyDeep = Hex(0x6B3FA8);
		const FLinearColor Teal = Hex(0x5FB3B0);
		const FLinearColor Common = Hex(0xCFC3AA);
		const FLinearColor Rare = Hex(0x58A9E0);
		const FLinearColor Epic = Hex(0xB383EC);
		const FLinearColor Legendary = Hex(0xF5B735);
	}

	// ------------------------------------------------------------------------------------------------- fonts

	namespace
	{
		const TCHAR* FontFile(EFont F)
		{
			switch (F)
			{
			case EFont::Display: return TEXT("RussoOne-Regular.ttf");
			case EFont::Body: return TEXT("PTSansNarrow-Regular.ttf");
			case EFont::Bold: return TEXT("PTSansNarrow-Bold.ttf");
			case EFont::Mono: return TEXT("PTMono-Regular.ttf");
			case EFont::Hand: return TEXT("Neucha-Regular.ttf");
			default: return TEXT("PTSansNarrow-Regular.ttf");
			}
		}

		TSharedPtr<const FCompositeFont> Composite(EFont F)
		{
			static TSharedPtr<FStandaloneCompositeFont> Fonts[(int32)EFont::Count];
			TSharedPtr<FStandaloneCompositeFont>& Slot = Fonts[FMath::Clamp((int32)F, 0, (int32)EFont::Count - 1)];
			if (!Slot.IsValid())
			{
				const FString Path = FPaths::ProjectContentDir() / TEXT("UI/Fonts") / FontFile(F);
				if (!FPaths::FileExists(Path))
				{
					return nullptr;
				}
				Slot = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
			return Slot;
		}

	}

	FSlateFontInfo Font(EFont F, float Size)
	{
		TSharedPtr<const FCompositeFont> Comp = Composite(F);
		if (!Comp.IsValid())
		{
			return FCoreStyle::GetDefaultFontStyle(F == EFont::Body ? "Regular" : "Bold", FMath::RoundToInt(Size));
		}
		return FSlateFontInfo(Comp, Size);
	}

	UFont* CanvasFont(EFont F, int32 Size)
	{
		static TMap<int32, UFont*> Made;
		const int32 Key = (int32)F * 1000 + FMath::Clamp(Size, 4, 400);
		if (UFont** Found = Made.Find(Key))
		{
			return *Found;
		}
		TSharedPtr<const FCompositeFont> Comp = Composite(F);
		if (!Comp.IsValid())
		{
			return nullptr;
		}
		UFont* NewFont = NewObject<UFont>(GetTransientPackage(), NAME_None, RF_Transient);
		NewFont->FontCacheType = EFontCacheType::Runtime;
		NewFont->GetMutableInternalCompositeFont() = *Comp;
		NewFont->LegacyFontSize = FMath::Clamp(Size, 4, 400);
		NewFont->AddToRoot();
		Made.Add(Key, NewFont);
		return NewFont;
	}

	void InstallCanvasFonts()
	{
		static bool bDone = false;
		if (bDone || !GEngine || !FSlateApplication::IsInitialized() || IsRunningDedicatedServer() || FParse::Param(FCommandLine::Get(), TEXT("AntUiOldFonts")))
		{
			return;
		}
		bDone = true;
		for (const TCHAR* Name : { TEXT("SmallFont"), TEXT("MediumFont"), TEXT("LargeFont") })
		{
			FObjectProperty* Prop = FindFProperty<FObjectProperty>(UEngine::StaticClass(), Name);
			UFont* Old = Prop ? Cast<UFont>(Prop->GetObjectPropertyValue_InContainer(GEngine)) : nullptr;
			if (!Old)
			{
				continue;
			}
			// About the same size as the font it replaces, so every layout made for Roboto keeps working: a runtime font
			// by its point size (PT Sans Narrow is a little smaller per point), a bitmap font by its glyph height.
			const int32 OldH = Old->FontCacheType == EFontCacheType::Runtime ? Old->LegacyFontSize : FMath::RoundToInt(Old->GetMaxCharHeight());
			const int32 Size = Old->FontCacheType == EFontCacheType::Runtime ? FMath::RoundToInt(OldH * 1.1f) : FMath::RoundToInt(OldH * 0.8f);
			if (Size < 5)
			{
				continue;
			}
			if (UFont* NewFont = CanvasFont(EFont::Bold, Size))
			{
				Prop->SetObjectPropertyValue_InContainer(GEngine, NewFont);
				UE_LOG(LogTemp, Display, TEXT("AntUi: %s (%s %d) -> PT Sans Narrow Bold %d"), Name, Old->FontCacheType == EFontCacheType::Runtime ? TEXT("runtime pt") : TEXT("bitmap px"), OldH, Size);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------- textures

	UTexture2D* Tex(const TCHAR* Path)
	{
		static TMap<FString, UTexture2D*> Loaded;
		const FString Key(Path);
		if (UTexture2D** Found = Loaded.Find(Key))
		{
			return *Found;
		}
		const FString Leaf = FPaths::GetBaseFilename(Key);
		const FString Full = FString::Printf(TEXT("/Game/UI/%s.%s"), *Key, *Leaf);
		if (!IsInGameThread())
		{
			// The loading screen paints on its own thread: only what is already in memory (UAntUiSubsystem preloads it).
			return FindObject<UTexture2D>(nullptr, *Full);
		}
		UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Full, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (T)
		{
			T->AddToRoot();
		}
		Loaded.Add(Key, T);
		return T;
	}

	const FSlateBrush* Brush(const TCHAR* Path)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Brushes;
		const FString Key(Path);
		if (TSharedPtr<FSlateBrush>* Found = Brushes.Find(Key))
		{
			return Found->Get();
		}
		UTexture2D* T = Tex(Path);
		if (!IsInGameThread())
		{
			return nullptr;   // brushes are made on the game thread only (the loading screen gets the preloaded ones)
		}
		if (!T)
		{
			Brushes.Add(Key, nullptr);
			return nullptr;
		}
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
		B->SetResourceObject(T);
		B->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		B->DrawAs = ESlateBrushDrawType::Image;
		const FString Name = FPaths::GetBaseFilename(Key);
		if (Name.Contains(TEXT("Paper")) || Name.Contains(TEXT("Cardboard")) || Name.Contains(TEXT("Grain")) || Name.Contains(TEXT("TornEdge")))
		{
			B->Tiling = ESlateBrushTileType::Both;
		}
		Brushes.Add(Key, B);
		return B.Get();
	}

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	const FSlateBrush* Rounded(float Radius)
	{
		static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Made;
		const int32 Key = FMath::RoundToInt(Radius * 4.f);
		if (TSharedPtr<FSlateRoundedBoxBrush>* Found = Made.Find(Key))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateRoundedBoxBrush> B = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, Radius);
		Made.Add(Key, B);
		return B.Get();
	}

	const FButtonStyle& ButtonStyle()
	{
		static const FButtonStyle Style = []()
		{
			FButtonStyle B;
			B.SetNormal(FSlateRoundedBoxBrush(C::Paper, 6.f, C::Cocoa, 1.5f));
			B.SetHovered(FSlateRoundedBoxBrush(C::Honey, 6.f, C::AmberDeep, 2.f));
			B.SetPressed(FSlateRoundedBoxBrush(C::Amber, 6.f, C::AmberDeep, 2.f));
			B.SetDisabled(FSlateRoundedBoxBrush(WithA(C::Muted, 0.5f), 6.f));
			B.SetNormalPadding(FMargin(2.f, 2.f, 2.f, 2.f));
			B.SetPressedPadding(FMargin(2.f, 3.f, 2.f, 1.f));
			return B;
		}();
		return Style;
	}

	UTexture2D* Icon(const TCHAR* Name)
	{
		return Tex(*FString::Printf(TEXT("Icons/T_UI_Icon_%s"), Name));
	}

	const FSlateBrush* IconBrush(const TCHAR* Name)
	{
		return Brush(*FString::Printf(TEXT("Icons/T_UI_Icon_%s"), Name));
	}

	// ------------------------------------------------------------------------------------------------- settings, misc

	bool Classic()
	{
		// Not cached before the command line exists (static init of a monolithic build would lock it to "off").
		static int32 Cached = -1;
		if (Cached < 0)
		{
			if (!FCommandLine::IsInitialized()) { return false; }
			Cached = FParse::Param(FCommandLine::Get(), TEXT("AntUiClassic")) ? 1 : 0;
		}
		return Cached == 1;
	}

	float HudScale()
	{
		return FMath::Clamp(FAntUiSettings::Get().HudScale, 0.75f, 1.5f);
	}

	int32 Lang()
	{
		return FAntUiSettings::Get().Language;
	}

	FString Lf(const TCHAR* Key, const FStringFormatOrderedArguments& Args)
	{
		return FString::Format(*L(Key), Args);
	}

	FLinearColor PlayerColor(int32 Slot, const FLinearColor& Own)
	{
		static const uint32 Safe[8] = { 0xE69F00, 0x56B4E9, 0x009E73, 0xF0E442, 0x0072B2, 0xD55E00, 0xCC79A7, 0xBBBBBB };
		static const uint32 Warm[8] = { 0xF2A93B, 0x5FB3B0, 0x86C95A, 0xCDA8FF, 0xFFD27A, 0x58A9E0, 0xD9473B, 0xFBF5E6 };
		const int32 I = ((Slot % 8) + 8) % 8;
		if (FAntUiSettings::Get().bColorblind)
		{
			return Hex(Safe[I]);
		}
		// Ant bodies are near black: those colours cannot mark a slot, the warm palette does.
		return Own.GetLuminance() > 0.08f ? Own : Hex(Warm[I]);
	}

	const TCHAR* CastePortrait(int32 Caste)
	{
		switch (Caste)
		{
		case 1: return TEXT("Portraits/T_UI_PorZhvalov");
		case 2: return TEXT("Portraits/T_UI_PorUsik");
		case 3: return TEXT("Portraits/T_UI_PorTikhon");
		default: return TEXT("Portraits/T_UI_PorGruzd");
		}
	}

	const FString& CasteName(int32 Caste)
	{
		static const TCHAR* Keys[4] = { TEXT("caste.worker"), TEXT("caste.soldier"), TEXT("caste.scout"), TEXT("caste.barrel") };
		return L(Keys[FMath::Clamp(Caste, 0, 3)]);
	}

	const TCHAR* IconForLabel(const FString& Label)
	{
		struct FMap { const TCHAR* Part; const TCHAR* Icon; };
		static const FMap Map[] =
		{
			{ TEXT("орт"), TEXT("Cake") }, { TEXT("Именин"), TEXT("Cake") }, { TEXT("незд"), TEXT("NestEntrance") }, { TEXT("Вход"), TEXT("NestEntrance") },
			{ TEXT("Щель"), TEXT("NestEntrance") }, { TEXT("еловек"), TEXT("Human") }, { TEXT("игант"), TEXT("Human") }, { TEXT("Трофей"), TEXT("Medal") },
			{ TEXT("Сюда"), TEXT("Pin") }, { TEXT("Дверь"), TEXT("Compass") }, { TEXT("ошк"), TEXT("Cat") }, { TEXT("Муся"), TEXT("Cat") },
			{ TEXT("Бадрик"), TEXT("Dog") }, { TEXT("обак"), TEXT("Dog") }, { TEXT("абушк"), TEXT("GranSlipper") }, { TEXT("алыш"), TEXT("KidHand") },
			{ TEXT("асад"), TEXT("Ambush") }, { TEXT("холм"), TEXT("RedHill") }, { TEXT("Холм"), TEXT("RedHill") }, { TEXT("раснык"), TEXT("FireAnt") },
			{ TEXT("Тело"), TEXT("Tag") }, { TEXT("тело"), TEXT("Tag") }, { TEXT("олочко"), TEXT("Jelly") }, { TEXT("Еда"), TEXT("Cookie") },
		};
		for (const FMap& M : Map)
		{
			if (Label.Contains(M.Part, ESearchCase::CaseSensitive))
			{
				return M.Icon;
			}
		}
		return nullptr;
	}
}
