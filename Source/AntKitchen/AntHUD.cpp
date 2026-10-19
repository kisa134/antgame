#include "AntHUD.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"

namespace HudColors
{
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Grey(0.7f, 0.7f, 0.7f);
	const FLinearColor Gold(1.f, 0.8f, 0.2f);
	const FLinearColor Red(1.f, 0.3f, 0.25f);
	const FLinearColor Green(0.4f, 1.f, 0.4f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.55f);
}
using namespace HudColors;

// ---------------------------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------------------------

void AAntHUD::Txt(const FString& Str, float X, float Y, const FLinearColor& C, UFont* F, float Scale, bool bCenter)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, F, Scale);
	const float DX = bCenter ? X - W * 0.5f : X;
	DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.8f * C.A), DX + 2.f, Y + 2.f, F, Scale);
	DrawText(Str, C, DX, Y, F, Scale);
}

void AAntHUD::TxtRight(const FString& Str, float RightX, float Y, const FLinearColor& C, UFont* F, float Scale)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, F, Scale);
	Txt(Str, RightX - W, Y, C, F, Scale);
}

void AAntHUD::Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& Fill)
{
	DrawRect(Panel, X, Y, W, H);
	DrawRect(Fill, X + 2.f, Y + 2.f, (W - 4.f) * FMath::Clamp(Frac, 0.f, 1.f), H - 4.f);
}

void AAntHUD::Marker(const FVector& World, const FString& Label, const FLinearColor& C, bool bClampToEdges)
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	const FVector Local = CamRot.UnrotateVector(World - CamLoc);
	const int32 Meters = FMath::RoundToInt(FVector::Dist(World, CamLoc) / 100.f);
	const FString Full = FString::Printf(TEXT("%s %dм"), *Label, Meters);

	FVector2D P;
	if (Local.X > 1.f)
	{
		const FVector Proj = Project(World);
		P = FVector2D(Proj.X, Proj.Y);
	}
	else
	{
		if (!bClampToEdges) return;
		P = FVector2D(Local.Y >= 0.f ? W * 0.95f : W * 0.05f, H * 0.5f);
	}

	const bool bOff = P.X < 0.f || P.X > W || P.Y < 0.f || P.Y > H;
	if (bOff && !bClampToEdges)
	{
		return;
	}
	P.X = FMath::Clamp(P.X, 60.f * S, W - 60.f * S);
	P.Y = FMath::Clamp(P.Y, 60.f * S, H - 60.f * S);
	DrawRect(C, P.X - 5.f * S, P.Y - 5.f * S, 10.f * S, 10.f * S);
	Txt(Full, P.X, P.Y + 8.f * S, C, GEngine->GetSmallFont(), 1.1f * S, true);
}

void AAntHUD::Crosshair()
{
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const FLinearColor C(1.f, 1.f, 1.f, 0.9f);
	DrawRect(C, CX - 1.f, CY - 10.f * S, 2.f, 6.f * S);
	DrawRect(C, CX - 1.f, CY + 4.f * S, 2.f, 6.f * S);
	DrawRect(C, CX - 10.f * S, CY - 1.f, 6.f * S, 2.f);
	DrawRect(C, CX + 4.f * S, CY - 1.f, 6.f * S, 2.f);

	// Hit / kill markers.
	if (const AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner))
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const float HitAge = Now - PC->HitMarkerTime;
		const float KillAge = Now - PC->KillMarkerTime;
		auto Cross = [this, CX, CY](float R, float Th, const FLinearColor& Col)
		{
			for (int32 sx = -1; sx <= 1; sx += 2)
			{
				for (int32 sy = -1; sy <= 1; sy += 2)
				{
					for (float k = 0.45f; k <= 1.f; k += 0.08f)
					{
						DrawRect(Col, CX + sx * R * k - Th * 0.5f, CY + sy * R * k - Th * 0.5f, Th, Th);
					}
				}
			}
		};
		if (KillAge < 0.45f)
		{
			Cross(30.f * S * (1.f + KillAge), 4.f * S, FLinearColor(1.f, 0.15f, 0.1f, 1.f - KillAge / 0.45f));
		}
		else if (HitAge < 0.18f)
		{
			Cross(18.f * S, 3.f * S, PC->bHitCrit ? FLinearColor(1.f, 0.85f, 0.1f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 1.f - HitAge / 0.18f));
		}
		if (KillAge < 2.f)
		{
			Txt(PC->KillText, CX, CY + 60.f * S, FLinearColor(1.f, 0.3f, 0.2f, FMath::Clamp(2.f - KillAge, 0.f, 1.f)), GEngine->GetMediumFont(), 1.3f * S, true);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	S = FMath::Max(0.7f, Canvas->ClipY / 1080.f);

	AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	AAntPlayerController* PC = Cast<AAntPlayerController>(PlayerOwner);
	APawn* Me = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;

	DrawWorldTags(Me);

	if (AAntCharacter* Ant = Cast<AAntCharacter>(Me))
	{
		DrawAnt(Ant);
	}
	else if (AHumanCharacter* H = Cast<AHumanCharacter>(Me))
	{
		DrawHuman(H, GS);
	}
	else
	{
		Txt(TEXT("Ожидание возрождения..."), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.45f, White, GEngine->GetLargeFont(), 1.4f * S, true);
	}

	DrawTop(GS);
	DrawFeed(GS);
	DrawToastsAndNumbers(PC);
	DrawHelp(Me);
	if (PC && PC->bShowScores)
	{
		DrawScoreboard(GS);
	}
}

void AAntHUD::DrawTop(AAntGameState* GS)
{
	if (!GS)
	{
		return;
	}
	const float W = Canvas->ClipX;
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();

	const FLinearColor Fire(1.f, 0.4f, 0.2f);
	Txt(FString::Printf(TEXT("КОЛОНИЯ %d"), GS->TeamScore), W * 0.5f - 140.f * S, 12.f * S, Gold, Med, 1.3f * S, true);
	Txt(FString::Printf(TEXT("ОГНЕННЫЕ %d"), GS->RivalScore), W * 0.5f + 140.f * S, 12.f * S, Fire, Med, 1.3f * S, true);
	Txt(FString::Printf(TEXT("до %d"), GS->TargetScore), W * 0.5f, 16.f * S, Grey, GEngine->GetSmallFont(), 1.1f * S, true);
	const float Tg = FMath::Max(1, GS->TargetScore);
	Bar(W * 0.5f - 270.f * S, 48.f * S, 260.f * S, 16.f * S, GS->TeamScore / Tg, Gold);
	Bar(W * 0.5f + 10.f * S, 48.f * S, 260.f * S, 16.f * S, GS->RivalScore / Tg, Fire);
	if (GS->HumanRage > 0)
	{
		Txt(FString::Printf(TEXT("ЯРОСТЬ ЧЕЛОВЕКА: %d / 3"), GS->HumanRage), W * 0.5f, 128.f * S, Red, GEngine->GetSmallFont(), 1.15f * S, true);
	}

	const int32 T = FMath::Max(0, FMath::CeilToInt(GS->TimeRemaining));
	Txt(FString::Printf(TEXT("%d:%02d"), T / 60, T % 60), W * 0.5f, 70.f * S, T < 60 ? Red : White, Med, 1.3f * S, true);
	Txt(FString::Printf(TEXT("Раздавлено муравьёв: %d    Человек в панике: %d раз"), GS->AntsSquashed, GS->HumanKnockouts), W * 0.5f, 104.f * S, Grey, GEngine->GetSmallFont(), 1.1f * S, true);

	if (GS->Phase != 0)
	{
		const FString Banner = GS->Phase == 1 ? TEXT("КОЛОНИЯ ПОБЕДИЛА!") : GS->Phase == 3 ? TEXT("ОГНЕННЫЕ МУРАВЬИ ПОБЕДИЛИ...") : TEXT("ЧЕЛОВЕК ПОБЕДИЛ!");
		DrawRect(Panel, 0.f, Canvas->ClipY * 0.3f, W, 140.f * S);
		Txt(Banner, W * 0.5f, Canvas->ClipY * 0.3f + 20.f * S, Gold, Big, 2.5f * S, true);
		Txt(TEXT("Новый раунд через несколько секунд..."), W * 0.5f, Canvas->ClipY * 0.3f + 95.f * S, White, Med, 1.f * S, true);
	}
}

void AAntHUD::DrawFeed(AAntGameState* GS)
{
	if (!GS)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	float Y = 14.f * S;
	int32 Shown = 0;
	for (int32 i = GS->Feed.Num() - 1; i >= 0 && Shown < 7; --i)
	{
		const FAntFeedLine& L = GS->Feed[i];
		const float Age = Now - L.Time;
		if (Age > 9.f)
		{
			continue;
		}
		FLinearColor C = L.Color;
		C.A = FMath::Clamp(9.f - Age, 0.f, 1.f);
		TxtRight(L.Text, Canvas->ClipX - 20.f * S, Y, C, GEngine->GetSmallFont(), 1.15f * S);
		Y += 24.f * S;
		++Shown;
	}
}

// ---------------------------------------------------------------------------------------------
// Ant view
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawAnt(AAntCharacter* Ant)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	const float Dt = GetWorld()->GetDeltaSeconds();

	if (LastHealth >= 0.f && Ant->Health < LastHealth - 0.5f)
	{
		HurtFlash = 1.f;
	}
	LastHealth = Ant->Health;
	if (HurtFlash > 0.f)
	{
		DrawRect(FLinearColor(0.8f, 0.f, 0.f, 0.25f * HurtFlash), 0.f, 0.f, W, H);
		HurtFlash -= Dt * 2.5f;
	}

	if (Ant->bDead)
	{
		Txt(TEXT("ТЕБЯ РАЗДАВИЛИ"), W * 0.5f, H * 0.36f, Red, Big, 2.2f * S, true);
		Txt(TEXT("Возрождение в муравейнике через пару секунд..."), W * 0.5f, H * 0.36f + 80.f * S, White, Med, 1.1f * S, true);
		return;
	}

	Crosshair();

	// Health + cargo (bottom left)
	const float X = 30.f * S, Y = H - 140.f * S;
	const float HFrac = Ant->Health / FMath::Max(1.f, Ant->MaxHealth);
	Txt(FString::Printf(TEXT("ЗДОРОВЬЕ %d"), FMath::CeilToInt(Ant->Health)), X, Y - 28.f * S, White, Small, 1.2f * S);
	Bar(X, Y, 340.f * S, 24.f * S, HFrac, FMath::Lerp(Red, Green, HFrac));

	const FAntClassDef& Cls = Ant->GetClassDef();
	if (AKitchenLoot* C = Ant->Carrying)
	{
		const float R = C->CarryRatio();
		const bool bTug = C->CarryTeam() != Ant->Team;
		const FString State = bTug ? TEXT("ПЕРЕТЯГИВАНИЕ — враги сильнее!") : R >= 1.f ? TEXT("несём") : R >= 0.5f ? TEXT("волочём, тяжело") : TEXT("НЕ СДВИНУТЬ — нужна помощь");
		Txt(FString::Printf(TEXT("ТАЩИШЬ: %s (+%d)  вес %d, держат %d  — %s"), *C->LootName, C->Value, C->Weight, C->NumCarriers(), *State), X, Y + 34.f * S, bTug || R < 0.5f ? Red : Gold, Small, 1.2f * S);
		Bar(X, Y + 62.f * S, 340.f * S, 14.f * S, bTug ? 0.f : FMath::Min(1.f, R), R >= 1.f ? Gold : Red);
		Marker(AKitchenBuilder::NestLocation(Ant->Team) + FVector(0.f, 0.f, 200.f), TEXT("МУРАВЕЙНИК"), Gold, true);
	}
	else
	{
		Txt(FString::Printf(TEXT("%s — сила %.0f"), Cls.Name, Cls.Strength), X, Y + 34.f * S, Gold, Small, 1.2f * S);
	}
	if (Ant->IsInNest())
	{
		Txt(TEXT("В муравейнике безопасно.  Каста: 1 Фуражир (сила)  2 Солдат (броня, урон)  3 Разведчик (скорость, прыжок)  4 Бочка (лечит своих)"), X, Y + 84.f * S, Green, Small, 1.1f * S);
	}

	// Weapon card (bottom right)
	const FAntWeapon& Wp = Ant->Weapon;
	const FLinearColor RC = AntKit::RarityColor(Wp.Rarity);
	const float CW = 420.f * S, CH = 170.f * S;
	const float CX = W - CW - 30.f * S, CY = H - CH - 30.f * S;
	DrawRect(Panel, CX, CY, CW, CH);
	DrawRect(RC, CX, CY, 6.f * S, CH);
	Txt(Wp.Name, CX + 18.f * S, CY + 10.f * S, RC, Med, 1.f * S);
	Txt(FString::Printf(TEXT("Урон: %.0f%s"), Wp.Damage, Wp.Pellets > 1 ? *FString::Printf(TEXT(" x%d"), Wp.Pellets) : TEXT("")), CX + 18.f * S, CY + 50.f * S, White, Small, 1.15f * S);
	Txt(FString::Printf(TEXT("Скорострельность: %.1f /с"), Wp.FireRate), CX + 18.f * S, CY + 76.f * S, White, Small, 1.15f * S);
	Txt(FString::Printf(TEXT("DPS: %.0f"), AntKit::WeaponDps(Wp)), CX + 18.f * S, CY + 102.f * S, Gold, Small, 1.15f * S);
	FString Traits;
	if (Wp.bBounce) Traits += TEXT("Рикошет  ");
	if (Wp.Splash > 0.f) Traits += FString::Printf(TEXT("Взрыв %.0f  "), Wp.Splash);
	if (Wp.Gravity > 0.6f) Traits += TEXT("Навесом  ");
	if (!Traits.IsEmpty())
	{
		Txt(Traits, CX + 18.f * S, CY + 130.f * S, RC, Small, 1.1f * S);
	}

	// Status effects: temperature, honey, water
	{
		const float T = Ant->Temperature;
		if (T < 10.f)
		{
			const float A = FMath::Clamp((10.f - T) / 25.f, 0.f, 1.f);
			DrawRect(FLinearColor(0.5f, 0.8f, 1.f, 0.28f * A), 0.f, 0.f, W, H);
		}
		else if (T > 40.f)
		{
			const float A = FMath::Clamp((T - 40.f) / 60.f, 0.f, 1.f);
			DrawRect(FLinearColor(1.f, 0.35f, 0.05f, 0.25f * A), 0.f, 0.f, W, H);
		}
		FString St = FString::Printf(TEXT("%.0f°C"), T);
		FLinearColor SC = White;
		if (Ant->IsFrozen()) { St += TEXT("  ЗАМЁРЗ! медленно, не прыгнуть, не залезть на стену"); SC = FLinearColor(0.6f, 0.85f, 1.f); }
		else if (T < 8.f) { St += TEXT("  холодно: медленнее бегаешь и стреляешь"); SC = FLinearColor(0.6f, 0.85f, 1.f); }
		else if (T > 55.f) { St += TEXT("  ГОРИШЬ!"); SC = Red; }
		else if (T > 40.f) { St += TEXT("  жарко"); SC = Gold; }
		if (Ant->HasStatus(AntStatus::Sticky)) { St += TEXT("  | липкий: медленно, без прыжков"); SC = Gold; }
		if (Ant->HasStatus(AntStatus::Wet)) { St += TEXT("  | мокрый"); }
		if (Ant->HasStatus(AntStatus::Drowning)) { St += TEXT("  | ТОНЕШЬ!"); SC = Red; }
		if (Ant->IsBurning()) { St += TEXT("  | ГОРИШЬ! в воду / поджигай врагов"); SC = FLinearColor(1.f, 0.5f, 0.1f); }
		Txt(St, X, Y - 56.f * S, SC, Small, 1.15f * S);
	}

	// Colony swarm
	if (const AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Txt(FString::Printf(TEXT("КОЛОНИЯ: %d рабочих  |  в гнезде %d  |  на тропах %d  (%d троп)"),
			Swarm->Population, Swarm->Idle, Swarm->AntsOnTrails(), Swarm->Streams.Num()),
			X, Y - 84.f * S, FLinearColor(0.55f, 1.f, 0.6f), Small, 1.1f * S);
	}
	if (Ant->bLayingTrail)
	{
		Txt(FString::Printf(TEXT("ФЕРОМОНОВАЯ ТРОПА: %d / %d — ползи в муравейник  (T — отменить)"), Ant->TrailPoints.Num(), AAntCharacter::TrailMaxPoints),
			W * 0.5f, H * 0.5f + 175.f * S, FLinearColor(0.4f, 1.f, 0.45f), Med, 1.f * S, true);
		Marker(AKitchenBuilder::NestLocation() + FVector(0.f, 0.f, 200.f), TEXT("МУРАВЕЙНИК"), FLinearColor(0.4f, 1.f, 0.45f), true);
	}

	// Interaction prompt: gadgets win over loot when closer.
	float GadgetDist = 0.f;
	AKitchenInteractable* Gadget = Ant->FindNearestInteractable(&GadgetDist);
	AKitchenLoot* NearLoot = Ant->FindNearestLoot(AAntCharacter::InteractRadius);
	if (Gadget && (!NearLoot || GadgetDist < FVector::Dist(NearLoot->GetActorLocation(), Ant->GetActorLocation())))
	{
		Txt(FString::Printf(TEXT("[E] %s"), *Gadget->GetPrompt(Ant)), W * 0.5f, H * 0.5f + 70.f * S, FLinearColor(0.6f, 1.f, 0.9f), Med, 1.05f * S, true);
	}
	else if (AKitchenLoot* L = NearLoot)
	{
		const float PX = W * 0.5f, PY = H * 0.5f + 70.f * S;
		if (L->bIsGear)
		{
			const float NewDps = AntKit::WeaponDps(L->Gear);
			const float OldDps = AntKit::WeaponDps(Wp);
			Txt(FString::Printf(TEXT("[E] Взять: %s"), *L->Gear.Name), PX, PY, AntKit::RarityColor(L->Gear.Rarity), Med, 1.05f * S, true);
			Txt(FString::Printf(TEXT("DPS %.0f  (%s%.0f)"), NewDps, NewDps >= OldDps ? TEXT("+") : TEXT(""), NewDps - OldDps), PX, PY + 34.f * S, NewDps >= OldDps ? Green : Red, Small, 1.2f * S, true);
		}
		else
		{
			const int32 Need = L->AntsNeeded(Ant->GetStrength());
			const bool bMine = Ant->Carrying == L;
			const FString What = bMine ? FString::Printf(TEXT("[E] Отпустить: %s"), *L->LootName)
				: FString::Printf(TEXT("[E] Схватить: %s  +%d  (вес %d)"), *L->LootName, L->Value, L->Weight);
			Txt(What, PX, PY, Need <= 1 ? White : Gold, Med, 1.05f * S, true);
			if (!bMine)
			{
				const FString Hint = L->IsCarried()
					? (L->CarryTeam() == Ant->Team ? FString::Printf(TEXT("Свои тащат (%d) — помоги!"), L->NumCarriers()) : TEXT("Это тащат ЧУЖИЕ — хватай и тяни к себе!"))
					: Need <= 1 ? TEXT("Унесёшь один") : FString::Printf(TEXT("Тяжело: нужно %d таких муравья (один — только волочить или никак)"), Need);
				Txt(Hint, PX, PY + 34.f * S, Need <= 1 ? Green : Gold, Small, 1.1f * S, true);
			}
			if (!Ant->bLayingTrail)
			{
				Txt(TEXT("[T] проложить феромоновую тропу — рой будет таскать это сам"), PX, PY + 62.f * S, FLinearColor(0.5f, 1.f, 0.55f), Small, 1.05f * S, true);
			}
		}
	}

	if (Ant->IsClimbing())
	{
		Txt(Ant->GetVisualUp().Z < -0.5f ? TEXT("~ ползёшь по потолку ~  (Пробел — отцепиться)") : TEXT("~ ползёшь по стене ~  (Пробел — оттолкнуться)"), W * 0.5f, H * 0.5f + 140.f * S, Grey, Small, 1.1f * S, true);
	}

	// Rival nest marker
	Marker(AKitchenBuilder::NestLocation(1 - Ant->Team) + FVector(0.f, 0.f, 300.f), Ant->Team == 0 ? TEXT("гнездо огненных") : TEXT("гнездо чёрных"), FLinearColor(1.f, 0.4f, 0.2f), false);

	// Loot radar: gear and juicy food nearby
	const FVector Me = Ant->GetActorLocation();
	int32 Count = 0;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It && Count < 12; ++It)
	{
		AKitchenLoot* L = *It;
		const float D = FVector::Dist(L->GetActorLocation(), Me);
		if (D > 3000.f || D < AAntCharacter::InteractRadius)
		{
			continue;
		}
		if (L->bIsGear)
		{
			Marker(L->GetActorLocation() + FVector(0.f, 0.f, 40.f), AntKit::RarityName(L->Gear.Rarity), AntKit::RarityColor(L->Gear.Rarity), false);
			++Count;
		}
		else if (L->Value >= 60)
		{
			Marker(L->GetActorLocation() + FVector(0.f, 0.f, 40.f + L->GetRadius()),
				L->IsCarried() ? FString::Printf(TEXT("+%d (тащат %d)"), L->Value, L->NumCarriers()) : FString::Printf(TEXT("+%d  x%d"), L->Value, L->AntsNeeded(Ant->GetStrength())),
				L->IsCarried() && L->CarryTeam() != Ant->Team ? FLinearColor(1.f, 0.4f, 0.2f) : Gold, false);
			++Count;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Human view
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawHuman(AHumanCharacter* H, AAntGameState* GS)
{
	const float W = Canvas->ClipX, Hh = Canvas->ClipY;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Big = GEngine->GetLargeFont();

	Crosshair();

	const float X = 30.f * S, Y = Hh - 170.f * S;
	Txt(TEXT("ТЫ — ЧЕЛОВЕК"), X, Y - 40.f * S, Red, GEngine->GetMediumFont(), 1.2f * S);
	Txt(FString::Printf(TEXT("Нервы: %d / %d"), FMath::CeilToInt(H->Health), FMath::CeilToInt(H->MaxHealth)), X, Y, White, Small, 1.2f * S);
	Bar(X, Y + 26.f * S, 380.f * S, 22.f * S, H->Health / FMath::Max(1.f, H->MaxHealth), Red);
	Txt(TEXT("Дихлофос"), X, Y + 56.f * S, White, Small, 1.2f * S);
	Bar(X, Y + 82.f * S, 380.f * S, 14.f * S, H->SprayFuel / 100.f, FLinearColor(0.5f, 0.9f, 0.3f));

	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const bool bStompReady = ServerNow >= H->StompReadyTime;
	Txt(bStompReady ? TEXT("ЛКМ — ТОПНУТЬ: готово") : TEXT("ЛКМ — ТОПНУТЬ: замах..."), X, Y + 106.f * S, bStompReady ? Green : Grey, Small, 1.2f * S);
	const bool bSwatReady = ServerNow >= H->SwatReadyTime;
	Txt(bSwatReady ? TEXT("Q — ГАЗЕТОЙ по прицелу (стены, столы): готово") : TEXT("Q — газета: замах..."), X, Y + 130.f * S, bSwatReady ? Green : Grey, Small, 1.2f * S);
	Txt(H->Held ? TEXT("E — ШВЫРНУТЬ еду (разобьётся!)") : TEXT("E — схватить еду у прицела"), X, Y + 154.f * S, H->Held ? Gold : White, Small, 1.2f * S);
	// Where the slippers will land.
	for (int32 Leg = 0; Leg < 2; ++Leg)
	{
		Marker(H->GetStompPoint(Leg) + FVector(0.f, 0.f, 20.f), TEXT("тапок"), bStompReady ? FLinearColor(1.f, 0.5f, 0.2f) : Grey, false);
	}

	if (H->IsKnockedOut())
	{
		Txt(TEXT("ТЫ В ПАНИКЕ! Муравьи тебя довели..."), W * 0.5f, Hh * 0.4f, Red, Big, 1.6f * S, true);
	}

	// Smell the sugar: ants carrying food are highlighted.
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && Ant->IsCarrying() && FVector::Dist(Ant->GetActorLocation(), H->GetActorLocation()) < 7000.f)
		{
			Marker(Ant->GetActorLocation() + FVector(0.f, 0.f, 60.f), TEXT("сладкое!"), FLinearColor(1.f, 0.6f, 0.1f), false);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Shared
// ---------------------------------------------------------------------------------------------

void AAntHUD::DrawWorldTags(APawn* Me)
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	UFont* Small = GEngine->GetSmallFont();

	auto InFront = [&Cam, &CamRot](const FVector& P) { return CamRot.UnrotateVector(P - Cam).X > 1.f; };

	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (!Ant || Ant == Me || Ant->bDead)
		{
			continue;
		}
		const FVector P = Ant->GetActorLocation() + FVector(0.f, 0.f, 55.f);
		if (FVector::Dist(P, Cam) > 3000.f || !InFront(P))
		{
			continue;
		}
		const FVector S2 = Project(P);
		const AAntCharacter* MyAnt = Cast<AAntCharacter>(Me);
		const bool bEnemy = MyAnt ? MyAnt->IsEnemy(Ant) : Ant->Team != 0;
		Txt(Ant->GetDisplayName(), S2.X, S2.Y - 18.f * S, bEnemy ? FLinearColor(1.f, 0.4f, 0.25f) : FLinearColor(0.8f, 0.9f, 1.f), Small, 1.f * S, true);
		if (bEnemy)
		{
			Bar(S2.X - 40.f * S, S2.Y + 2.f * S, 80.f * S, 6.f * S, Ant->Health / FMath::Max(1.f, Ant->MaxHealth), FLinearColor(1.f, 0.3f, 0.2f));
		}
	}

	for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
	{
		AHumanCharacter* Hu = *It;
		if (!Hu || Hu == Me)
		{
			continue;
		}
		const FVector P = Hu->GetActorLocation() + FVector(0.f, 0.f, 1900.f);
		if (!InFront(P))
		{
			continue;
		}
		const FVector S2 = Project(P);
		Txt(Hu->GetDisplayName(), S2.X, S2.Y - 40.f * S, Red, Small, 1.2f * S, true);
		Bar(S2.X - 120.f * S, S2.Y - 12.f * S, 240.f * S, 14.f * S, Hu->Health / FMath::Max(1.f, Hu->MaxHealth), Red);
		if (Hu->IsKnockedOut())
		{
			Txt(TEXT("В ПАНИКЕ!"), S2.X, S2.Y + 6.f * S, Gold, Small, 1.2f * S, true);
		}
	}
}

void AAntHUD::DrawToastsAndNumbers(AAntPlayerController* PC)
{
	if (!PC)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	float Y = Canvas->ClipY * 0.66f;
	for (const FAntToast& T : PC->Toasts)
	{
		const float Age = Now - T.Time;
		if (Age > 3.f)
		{
			continue;
		}
		Txt(T.Text, Canvas->ClipX * 0.5f, Y, FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(3.f - Age, 0.f, 1.f)), GEngine->GetMediumFont(), 1.f * S, true);
		Y += 32.f * S;
	}

	if (!PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PlayerOwner->PlayerCameraManager->GetCameraRotation();
	for (const FAntDamageNumber& N : PC->DamageNumbers)
	{
		const float Age = Now - N.Time;
		if (Age > 1.2f)
		{
			continue;
		}
		const FVector World = N.Location + FVector(0.f, 0.f, 40.f + Age * 120.f);
		if (CamRot.UnrotateVector(World - Cam).X <= 1.f)
		{
			continue;
		}
		const FVector P = Project(World);
		const FLinearColor C = N.bCrit ? FLinearColor(1.f, 0.9f, 0.1f, 1.2f - Age) : FLinearColor(1.f, 1.f, 1.f, 1.2f - Age);
		const FString Str = N.bCrit ? FString::Printf(TEXT("%d КРИТ!"), N.Amount) : FString::FromInt(N.Amount);
		Txt(Str, P.X, P.Y, C, GEngine->GetMediumFont(), (N.bCrit ? 1.4f : 1.0f) * S, true);
	}
}

void AAntHUD::DrawScoreboard(AAntGameState* GS)
{
	if (!GS)
	{
		return;
	}
	UFont* Small = GEngine->GetSmallFont();
	const float W = 760.f * S;
	const float X = Canvas->ClipX * 0.5f - W * 0.5f;
	float Y = Canvas->ClipY * 0.22f;
	const float RowH = 30.f * S;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), X, Y, W, RowH * (GS->PlayerArray.Num() + 2) + 20.f * S);

	Y += 10.f * S;
	Txt(TEXT("Игрок"), X + 20.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Роль"), X + 300.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Принёс"), X + 430.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Смерти"), X + 550.f * S, Y, Gold, Small, 1.2f * S);
	Txt(TEXT("Давил"), X + 660.f * S, Y, Gold, Small, 1.2f * S);
	Y += RowH;

	for (APlayerState* Base : GS->PlayerArray)
	{
		AAntPlayerState* PS = Cast<AAntPlayerState>(Base);
		if (!PS)
		{
			continue;
		}
		DrawRect(PS->AntColor, X + 6.f * S, Y + 6.f * S, 8.f * S, 16.f * S);
		Txt(PS->GetPlayerName(), X + 20.f * S, Y, White, Small, 1.15f * S);
		Txt(PS->bIsHuman ? TEXT("Человек") : PS->Team == 0 ? AntKit::ClassDef(PS->AntClass).Name : *FString::Printf(TEXT("огн. %s"), AntKit::ClassDef(PS->AntClass).Name),
			X + 300.f * S, Y, PS->bIsHuman ? Red : PS->Team == 0 ? Green : FLinearColor(1.f, 0.4f, 0.2f), Small, 1.15f * S);
		Txt(FString::FromInt(PS->Delivered), X + 430.f * S, Y, White, Small, 1.15f * S);
		Txt(FString::FromInt(PS->Deaths), X + 550.f * S, Y, White, Small, 1.15f * S);
		Txt(FString::FromInt(PS->Kills), X + 660.f * S, Y, White, Small, 1.15f * S);
		Y += RowH;
	}
}

void AAntHUD::DrawHelp(APawn* Me)
{
	FString Help;
	if (Cast<AHumanCharacter>(Me))
	{
		Help = TEXT("WASD — ходить | ЛКМ — топнуть | ПКМ (держать) — дихлофос | Q — газета по прицелу | E — схватить/швырнуть еду | H — снова муравей | Tab — счёт");
	}
	else if (const AAntCharacter* C = Cast<AAntCharacter>(Me); C && C->Creature == 1)
	{
		Help = TEXT("МУХА: WASD — лететь куда смотришь | Пробел — вверх | E — схватить/бросить крошку | ЛКМ — плевок | ПКМ — укус | J — снова муравей");
	}
	else if (const AAntCharacter* C2 = Cast<AAntCharacter>(Me); C2 && C2->Creature == 2)
	{
		Help = TEXT("ПАУК: по стенам и потолку | ЛКМ — паутина (опутывает) | ПКМ — укус | Пробел на потолке — спуск на нити | K — снова муравей");
	}
	else
	{
		Help = TEXT("J — муха | K — паук | WASD — ползти | ЛКМ — плевок | E — схватить/отпустить еду, взять оружие, гаджет | ПКМ — укус / откусить кусок | V — bodycam | 1-4 — каста (в гнезде) | T — тропа роя | G — отозвать рой | Shift — спринт | Пробел — прыжок | H — Человек | Tab — счёт");
	}
	Txt(Help, Canvas->ClipX * 0.5f, Canvas->ClipY - 26.f * S, FLinearColor(1.f, 1.f, 1.f, 0.6f), GEngine->GetSmallFont(), 1.f * S, true);
}
