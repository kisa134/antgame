#include "AntHUD.h"
#include "AntRaid.h"
#include "AntRun.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"

// The nest between raids. (Milestone 1: a plain panel; the real screen with the stores, the Queen, upgrades and the squad comes with the nest.)
void AAntHUD::DrawNest(AAntRaid* Raid, AAntGameState* GS)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Big = GEngine->GetLargeFont();
	const FAntNestState& N = Raid->Nest;
	DrawRect(FLinearColor(0.02f, 0.015f, 0.01f, 0.9f), 0.f, 0.f, W, H);
	Txt(TEXT("ГНЕЗДО"), W * 0.5f, H * 0.22f, FLinearColor(1.f, 0.8f, 0.2f), Big, 2.2f * S, true);
	Txt(FString::Printf(TEXT("Рейдов: %d  ·  еда %d  ·  молочко %d  ·  личинки %d  ·  рабочие %d  ·  Королева %s"), N.Raids, N.Food, N.Jelly, N.Larvae, N.Workers, AntNest::MoodName(N.Mood)),
		W * 0.5f, H * 0.22f + 90.f * S, FLinearColor::White, Small, 1.2f * S, true);
	Txt(FString::Printf(TEXT("Следующий рейд: угроза %d «%s»"), (int32)Raid->Tier, Raid->TierDef().Name), W * 0.5f, H * 0.22f + 124.f * S, FLinearColor(0.8f, 0.8f, 0.8f), Small, 1.15f * S, true);
	const float Left = Raid->StartEnds > 0.f ? Raid->StartEnds - GS->GetServerWorldTimeSeconds() : -1.f;
	Txt(Left >= 0.f ? FString::Printf(TEXT("Выходим через %d"), FMath::CeilToInt(Left)) : FString(TEXT("[Enter] — в рейд")), W * 0.5f, H * 0.6f, FLinearColor(1.f, 0.8f, 0.2f), Big, 1.4f * S, true);
	if (AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner))
	{
		if (PC->WasInputKeyJustPressed(EKeys::Enter))
		{
			PC->ServerRaid(AntRaidCmd::Ready, 1, 0);
			PC->ServerRaid(AntRaidCmd::Start, 0, 0);
		}
	}
}
