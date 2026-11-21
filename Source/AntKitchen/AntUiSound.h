#pragma once

#include "CoreMinimal.h"

/** What the UI wants to be heard. The sounds themselves belong to AntAudio; until it is in, a few existing effects stand in. */
enum class EAntUiSound : uint8 { Hover, Click, Back, Open, Toggle, Slide, Card, Stamp, Error, Tick, Count };

/** Plays a UI sound (2D) at the UI volume. Ctx: anything with a world; null = no sound. */
ANTKITCHEN_API void AntUiSound(EAntUiSound Event, const UObject* Ctx);
