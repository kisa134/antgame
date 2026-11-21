#include "AntUiSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"

namespace
{
	const TCHAR* Section = TEXT("AntUi");
	const TCHAR* VolKey[AntVol::Count] = { TEXT("VolMaster"), TEXT("VolMusic"), TEXT("VolSfx"), TEXT("VolVoice"), TEXT("VolUi") };

	// The audio channels: AntAudio reads these. TODO(AntAudio): bind them to sound classes / submix volumes.
	TAutoConsoleVariable<float> CVarMaster(TEXT("AntAudio.Master"), 1.f, TEXT("Master volume 0..1 (settings screen)"));
	TAutoConsoleVariable<float> CVarMusic(TEXT("AntAudio.Music"), 0.8f, TEXT("Music volume 0..1 (settings screen)"));
	TAutoConsoleVariable<float> CVarSfx(TEXT("AntAudio.Sfx"), 1.f, TEXT("Effects volume 0..1 (settings screen)"));
	TAutoConsoleVariable<float> CVarVoice(TEXT("AntAudio.Voice"), 1.f, TEXT("Voice / barks volume 0..1 (settings screen)"));
	TAutoConsoleVariable<float> CVarUi(TEXT("AntAudio.Ui"), 0.8f, TEXT("Menu sounds volume 0..1 (settings screen)"));
	/** The dust in the air (AntMessDust can read it): 1 on, 0 off. */
	TAutoConsoleVariable<int32> CVarDust(TEXT("AntUi.Dust"), 1, TEXT("Floating dust on/off (settings screen)"));
	TAutoConsoleVariable<int32> CVarSubs(TEXT("AntUi.Subtitles"), 1, TEXT("Subtitles for barks and the radio (settings screen)"));

	void SetCVar(const TCHAR* Name, float V)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(V, ECVF_SetByGameSetting);
		}
	}
	void SetCVarInt(const TCHAR* Name, int32 V)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(V, ECVF_SetByGameSetting);
		}
	}
}

FAntUiSettings& FAntUiSettings::Get()
{
	static FAntUiSettings S;
	static bool bLoaded = false;
	if (!bLoaded && GConfig)
	{
		bLoaded = true;
		S.Load();
	}
	return S;
}

void FAntUiSettings::Load()
{
	if (!GConfig)
	{
		return;
	}
	const FString& Ini = GGameUserSettingsIni;
	GConfig->GetInt(Section, TEXT("Language"), Language, Ini);
	GConfig->GetBool(Section, TEXT("Subtitles"), bSubtitles, Ini);
	GConfig->GetFloat(Section, TEXT("HudScale"), HudScale, Ini);
	GConfig->GetBool(Section, TEXT("Colorblind"), bColorblind, Ini);
	GConfig->GetFloat(Section, TEXT("MouseSens"), MouseSens, Ini);
	GConfig->GetBool(Section, TEXT("InvertY"), bInvertY, Ini);
	for (int32 i = 0; i < AntVol::Count; ++i)
	{
		GConfig->GetFloat(Section, VolKey[i], Vol[i], Ini);
		Vol[i] = FMath::Clamp(Vol[i], 0.f, 1.f);
	}
	GConfig->GetBool(Section, TEXT("Dust"), bDust, Ini);
	GConfig->GetBool(Section, TEXT("MotionBlur"), bMotionBlur, Ini);
	GConfig->GetInt(Section, TEXT("AntiAliasing"), AntiAliasing, Ini);
	Language = FMath::Clamp(Language, 0, 1);
	HudScale = FMath::Clamp(HudScale, 0.75f, 1.5f);
	MouseSens = FMath::Clamp(MouseSens, 0.2f, 3.f);
	AntiAliasing = FMath::Clamp(AntiAliasing, 0, HasDlss() ? 4 : 3);
}

void FAntUiSettings::Save() const
{
	if (!GConfig)
	{
		return;
	}
	const FString& Ini = GGameUserSettingsIni;
	GConfig->SetInt(Section, TEXT("Language"), Language, Ini);
	GConfig->SetBool(Section, TEXT("Subtitles"), bSubtitles, Ini);
	GConfig->SetFloat(Section, TEXT("HudScale"), HudScale, Ini);
	GConfig->SetBool(Section, TEXT("Colorblind"), bColorblind, Ini);
	GConfig->SetFloat(Section, TEXT("MouseSens"), MouseSens, Ini);
	GConfig->SetBool(Section, TEXT("InvertY"), bInvertY, Ini);
	for (int32 i = 0; i < AntVol::Count; ++i)
	{
		GConfig->SetFloat(Section, VolKey[i], Vol[i], Ini);
	}
	GConfig->SetBool(Section, TEXT("Dust"), bDust, Ini);
	GConfig->SetBool(Section, TEXT("MotionBlur"), bMotionBlur, Ini);
	GConfig->SetInt(Section, TEXT("AntiAliasing"), AntiAliasing, Ini);
	GConfig->Flush(false, Ini);
}

bool FAntUiSettings::HasDlss()
{
	return IConsoleManager::Get().FindConsoleVariable(TEXT("r.NGX.DLSS.Enable")) != nullptr;
}

bool FAntUiSettings::operator==(const FAntUiSettings& O) const
{
	for (int32 i = 0; i < AntVol::Count; ++i)
	{
		if (!FMath::IsNearlyEqual(Vol[i], O.Vol[i])) { return false; }
	}
	return Language == O.Language && bSubtitles == O.bSubtitles && FMath::IsNearlyEqual(HudScale, O.HudScale) && bColorblind == O.bColorblind
		&& FMath::IsNearlyEqual(MouseSens, O.MouseSens) && bInvertY == O.bInvertY && bDust == O.bDust && bMotionBlur == O.bMotionBlur && AntiAliasing == O.AntiAliasing;
}

void FAntUiSettings::Apply(APlayerController* PC) const
{
	SetCVar(TEXT("AntAudio.Master"), Vol[AntVol::Master]);
	SetCVar(TEXT("AntAudio.Music"), Vol[AntVol::Music]);
	SetCVar(TEXT("AntAudio.Sfx"), Vol[AntVol::Sfx]);
	SetCVar(TEXT("AntAudio.Voice"), Vol[AntVol::Voice]);
	SetCVar(TEXT("AntAudio.Ui"), Vol[AntVol::Ui]);
	SetCVarInt(TEXT("AntUi.Dust"), bDust ? 1 : 0);
	SetCVarInt(TEXT("AntUi.Subtitles"), bSubtitles ? 1 : 0);
	SetCVarInt(TEXT("r.MotionBlurQuality"), bMotionBlur ? 4 : 0);
	// Anti-aliasing: r.AntiAliasingMethod 0 none, 1 FXAA, 2 TAA, 4 TSR. DLSS: the plugin's own switch on top of TSR's slot.
	static const int32 Methods[5] = { 0, 1, 2, 4, 4 };
	SetCVarInt(TEXT("r.AntiAliasingMethod"), Methods[FMath::Clamp(AntiAliasing, 0, 4)]);
	if (HasDlss())
	{
		SetCVarInt(TEXT("r.NGX.DLSS.Enable"), AntiAliasing == 4 ? 1 : 0);
	}
	if (PC && PC->PlayerInput)
	{
		// 0.07 is the engine's default mouse sensitivity (BaseInput.ini).
		PC->PlayerInput->SetMouseSensitivity(0.07f * MouseSens);
		if (PC->PlayerInput->GetInvertAxisKey(EKeys::MouseY) != bInvertY)
		{
			PC->PlayerInput->InvertAxisKey(EKeys::MouseY);
		}
	}
}

FAntGfx FAntGfx::Current()
{
	FAntGfx G;
	if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		G.Preset = FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 4);
		if (G.Preset < 0) { G.Preset = 3; }
		G.Res = S->GetScreenResolution();
		G.Mode = (int32)S->GetFullscreenMode();
		G.bVSync = S->IsVSyncEnabled();
		G.FrameCap = FMath::RoundToInt(S->GetFrameRateLimit());
	}
	return G;
}

void FAntGfx::Apply() const
{
	UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!S)
	{
		return;
	}
	S->SetOverallScalabilityLevel(Preset);
	S->SetScreenResolution(Res);
	S->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(Mode));
	S->SetVSyncEnabled(bVSync);
	S->SetFrameRateLimit((float)FrameCap);
	S->ApplySettings(false);
	S->SaveSettings();
}

TArray<FIntPoint> FAntGfx::Resolutions()
{
	TArray<FIntPoint> Out;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Out);
	if (Out.Num() == 0)
	{
		Out = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(2560, 1440), FIntPoint(3440, 1440), FIntPoint(3840, 2160) };
	}
	Out.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
	return Out;
}
