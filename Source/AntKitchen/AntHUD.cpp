#include "AntHUD.h"
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

	Txt(FString::Printf(TEXT("КОЛОНИЯ: %d / %d сладости"), GS->TeamScore, GS->TargetScore), W * 0.5f, 12.f * S, Gold, Med, 1.3f * S, true);
	Bar(W * 0.5f - 260.f * S, 48.f * S, 520.f * S, 16.f * S, GS->TargetScore > 0 ? (float)GS->TeamScore / GS->TargetScore : 0.f, Gold);

	const int32 T = FMath::Max(0, FMath::CeilToInt(GS->TimeRemaining));
	Txt(FString::Printf(TEXT("%d:%02d"), T / 60, T % 60), W * 0.5f, 70.f * S, T < 60 ? Red : White, Med, 1.3f * S, true);
	Txt(FString::Printf(TEXT("Раздавлено муравьёв: %d    Человек в панике: %d раз"), GS->AntsSquashed, GS->HumanKnockouts), W * 0.5f, 104.f * S, Grey, GEngine->GetSmallFont(), 1.1f * S, true);

	if (GS->Phase != 0)
	{
		const FString Banner = GS->Phase == 1 ? TEXT("МУРАВЬИ ПОБЕДИЛИ!") : TEXT("ЧЕЛОВЕК ПОБЕДИЛ!");
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

	Txt(FString::Printf(TEXT("ГРУЗ %d / %d   сладость: %d"), Ant->CarriedWeight, AAntCharacter::MaxCarry, Ant->CarriedValue), X, Y + 34.f * S, Gold, Small, 1.2f * S);
	Bar(X, Y + 62.f * S, 340.f * S, 14.f * S, (float)Ant->CarriedWeight / AAntCharacter::MaxCarry, Gold);
	if (Ant->IsInNest())
	{
		Txt(TEXT("Ты в муравейнике: тут безопасно"), X, Y + 84.f * S, Green, Small, 1.1f * S);
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

	// Interaction prompt
	if (AKitchenLoot* L = Ant->FindNearestLoot(AAntCharacter::InteractRadius))
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
			const bool bFits = Ant->CarriedWeight + L->Weight <= AAntCharacter::MaxCarry;
			Txt(FString::Printf(TEXT("[E] Подобрать: %s  +%d  (вес %d)"), *L->LootName, L->Value, L->Weight), PX, PY, bFits ? White : Red, Med, 1.05f * S, true);
			if (!bFits)
			{
				Txt(TEXT("Не влезет! Отнеси груз в муравейник"), PX, PY + 34.f * S, Red, Small, 1.1f * S, true);
			}
		}
	}

	if (Ant->IsClimbing())
	{
		Txt(TEXT("~ карабкаешься ~  (Пробел — оттолкнуться)"), W * 0.5f, H * 0.5f + 140.f * S, Grey, Small, 1.1f * S, true);
	}

	// Nest marker while carrying
	if (Ant->CarriedWeight > 0)
	{
		Marker(AKitchenBuilder::NestLocation() + FVector(0.f, 0.f, 200.f), TEXT("МУРАВЕЙНИК"), Gold, true);
	}

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
			Marker(L->GetActorLocation() + FVector(0.f, 0.f, 40.f), FString::Printf(TEXT("+%d"), L->Value), Gold, false);
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

	if (H->IsKnockedOut())
	{
		Txt(TEXT("ТЫ В ПАНИКЕ! Муравьи тебя довели..."), W * 0.5f, Hh * 0.4f, Red, Big, 1.6f * S, true);
	}

	// Smell the sugar: ants carrying food are highlighted.
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* Ant = *It;
		if (Ant && !Ant->bDead && Ant->CarriedWeight > 0 && FVector::Dist(Ant->GetActorLocation(), H->GetActorLocation()) < 7000.f)
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
		Txt(Ant->GetDisplayName(), S2.X, S2.Y - 18.f * S, FLinearColor(0.8f, 0.9f, 1.f), Small, 1.f * S, true);
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
		Txt(PS->bIsHuman ? TEXT("Человек") : TEXT("Муравей"), X + 300.f * S, Y, PS->bIsHuman ? Red : Green, Small, 1.15f * S);
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
		Help = TEXT("WASD — ходить | Мышь — смотреть | ЛКМ — топнуть | ПКМ (держать) — дихлофос | H — снова муравей | Tab — счёт");
	}
	else
	{
		Help = TEXT("WASD — бег | ЛКМ — плевок | E — подобрать | Shift — спринт | Пробел — прыжок | W в стену — лезть | H — стать Человеком | Tab — счёт");
	}
	Txt(Help, Canvas->ClipX * 0.5f, Canvas->ClipY - 26.f * S, FLinearColor(1.f, 1.f, 1.f, 0.6f), GEngine->GetSmallFont(), 1.f * S, true);
}
