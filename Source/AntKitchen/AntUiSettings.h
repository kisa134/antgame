#pragma once

#include "CoreMinimal.h"

class APlayerController;

/** Volume channels the settings screen sets. The audio code (AntAudio) reads them from the console variables AntAudio.Master,
 *  AntAudio.Music, AntAudio.Sfx, AntAudio.Voice, AntAudio.Ui (0..1). */
namespace AntVol { enum EId : uint8 { Master, Music, Sfx, Voice, Ui, Count }; }

/**
 * Everything the settings screen keeps that UGameUserSettings does not: language, subtitles, HUD scale, colour-blind colours,
 * mouse, volumes, dust, motion blur, anti-aliasing. Saved in GameUserSettings.ini, section [AntUi]. One copy per machine.
 */
struct ANTKITCHEN_API FAntUiSettings
{
	int32 Language = 0;          // 0 RU, 1 EN
	bool bSubtitles = true;
	float HudScale = 1.f;        // 0.75 .. 1.5
	bool bColorblind = false;
	float MouseSens = 1.f;       // x the default mouse sensitivity, 0.2 .. 3
	bool bInvertY = false;
	float Vol[AntVol::Count] = { 1.f, 0.8f, 1.f, 1.f, 0.8f };
	bool bDust = true;
	bool bMotionBlur = false;
	/** 0 off, 1 FXAA, 2 TAA, 3 TSR, 4 DLSS (only when the DLSS plugin is in). */
	int32 AntiAliasing = 3;

	static FAntUiSettings& Get();
	void Load();
	void Save() const;
	/** Console variables, mouse, render switches. PC may be null (then the mouse waits for the next player controller). */
	void Apply(APlayerController* PC) const;
	static bool HasDlss();
	bool operator==(const FAntUiSettings& O) const;
};

/** The graphics half of the settings screen: kept as an edit copy until "Apply" (UGameUserSettings underneath). */
struct ANTKITCHEN_API FAntGfx
{
	int32 Preset = 3;            // 0 low .. 3 epic, 4 cinematic
	FIntPoint Res = FIntPoint(1920, 1080);
	int32 Mode = 1;              // EWindowMode: 0 fullscreen, 1 windowed fullscreen, 2 windowed
	bool bVSync = false;
	int32 FrameCap = 0;          // 0 = no cap

	static FAntGfx Current();
	void Apply() const;
	static TArray<FIntPoint> Resolutions();
	bool operator==(const FAntGfx& O) const { return Preset == O.Preset && Res == O.Res && Mode == O.Mode && bVSync == O.bVSync && FrameCap == O.FrameCap; }
};
