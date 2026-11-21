#include "AntUiSound.h"
#include "AntUiSettings.h"
#include "AntTypes.h"
#include "Engine/World.h"

void AntUiSound(EAntUiSound Event, const UObject* Ctx)
{
	// TODO(AntAudio): the audio agent replaces this table with the real menu sounds (cardboard tap, stamp, paper slide).
	if (!Ctx || !Ctx->GetWorld())
	{
		return;
	}
	const FAntUiSettings& S = FAntUiSettings::Get();
	const float V = S.Vol[AntVol::Master] * S.Vol[AntVol::Ui];
	if (V <= 0.01f)
	{
		return;
	}
	struct FStandIn { uint8 Id; float Vol; float Pitch; };
	static const FStandIn Table[(int32)EAntUiSound::Count] =
	{
		{ AntSfx::Step, 0.25f, 1.9f },        // Hover
		{ AntSfx::Grab, 0.55f, 1.35f },       // Click
		{ AntSfx::Drop, 0.45f, 1.2f },        // Back
		{ AntSfx::PickupGear, 0.4f, 1.1f },   // Open
		{ AntSfx::Grab, 0.4f, 1.7f },         // Toggle
		{ AntSfx::Step, 0.2f, 2.2f },         // Slide
		{ AntSfx::PickupGear, 0.6f, 1.25f },  // Card
		{ AntSfx::Crunch, 0.6f, 0.8f },       // Stamp
		{ AntSfx::Buzz, 0.35f, 0.8f },        // Error
		{ AntSfx::Step, 0.15f, 2.6f },        // Tick
	};
	const FStandIn& E = Table[FMath::Clamp((int32)Event, 0, (int32)EAntUiSound::Count - 1)];
	AntSfx::Play2D(Ctx, E.Id, E.Vol * V, E.Pitch);
}
