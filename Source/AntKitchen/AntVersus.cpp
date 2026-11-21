#include "AntVersus.h"
#include "AntWeapon.h"
#include "AntWreck.h"
#include "AntRaid.h"
#include "AntKitchenGameMode.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "HumanCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

namespace
{
	float NetNowOf(const UWorld* W)
	{
		const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
		return GS ? (float)GS->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	}

	bool IsPlayerHuman(const AHumanCharacter* H)
	{
		return H && H->GetController() && H->GetController()->IsA<APlayerController>();
	}
}

AAntVersus::AAntVersus()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(5.f);
}

void AAntVersus::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntVersus, bActive);
	DOREPLIFETIME(AAntVersus, Patience);
	DOREPLIFETIME(AAntVersus, XrayUntil);
	DOREPLIFETIME(AAntVersus, NextXray);
	DOREPLIFETIME(AAntVersus, Winner);
	DOREPLIFETIME(AAntVersus, HumanName);
}

AAntVersus* AAntVersus::Get(const UWorld* World)
{
	if (!World) { return nullptr; }
	for (TActorIterator<AAntVersus> It(World); It; ++It) { return *It; }
	return nullptr;
}

AAntVersus* AAntVersus::Start(UWorld* World, AAntPlayerController* HumanPC)
{
	AAntKitchenGameMode* GM = World ? World->GetAuthGameMode<AAntKitchenGameMode>() : nullptr;
	if (!GM)
	{
		return nullptr;
	}
	AAntVersus* V = Get(World);
	if (!V)
	{
		V = World->SpawnActor<AAntVersus>();
	}
	if (!V)
	{
		return nullptr;
	}
	V->bActive = true;
	V->Winner = 0;
	V->Patience = MaxPatience;
	V->Swings = V->Refused = 0;
	V->NextXray = NetNowOf(World) + XrayEvery;
	V->bWasRaid = false;
	AHumanCharacter* H = nullptr;
	for (TActorIterator<AHumanCharacter> It(World); It; ++It) { if (It->GetClass() == AHumanCharacter::StaticClass()) { H = *It; break; } }
	if (HumanPC && H && H->GetController() != HumanPC)
	{
		GM->ToggleHuman(HumanPC);
	}
	const AAntPlayerState* PS = HumanPC ? HumanPC->GetPlayerState<AAntPlayerState>() : nullptr;
	V->HumanName = PS ? PS->GetPlayerName() : FString(TEXT("ИИ"));
	bool bAnyWeapon = false;
	for (TActorIterator<AHumanWeapon> It(World); It; ++It) { bAnyWeapon = true; break; }
	if (!bAnyWeapon) { AntWeapons::SpawnAll(World); }
	GM->Broadcast(FString::Printf(TEXT("ЧЕЛОВЕК ПРОТИВ МУРАВЬЁВ: %s — Человек. Муравьи: вынесите квоту! Человек: не дай до конца рейда"), *V->HumanName), FLinearColor(1.f, 0.4f, 0.25f));
	UE_LOG(LogTemp, Display, TEXT("ANTVERSUS start: the Human is %s (%s)"), *V->HumanName, HumanPC ? (HumanPC->IsLocalController() ? TEXT("the host") : TEXT("a client")) : TEXT("AI"));
	return V;
}

void AAntVersus::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!HasAuthority())
	{
		// Client: what this machine sees of the mode (the log of a two-copy test; -AntClientShots: a picture every 10 s).
		static float NextLog = 0.f, NextShot = 20.f;
		static int32 ShotN = 0;
		const float Now = GetWorld()->GetTimeSeconds();
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (bActive && Now >= NextLog)
		{
			NextLog = Now + 3.f;
			const AHumanCharacter* Me = PC ? Cast<AHumanCharacter>(PC->GetPawn()) : nullptr;
			const AntWeapons::FDef* D = Me ? AntWeapons::Held(Me) : nullptr;
			UE_LOG(LogTemp, Display, TEXT("ANTVERSUS client: I play %s, patience %.0f, x-ray %d, weapon %s, winner %d"), Me ? TEXT("the Human") : TEXT("an ant"), Patience, AntVersus::XrayOn(GetWorld()) ? 1 : 0,
				D ? D->Name : TEXT("газета"), (int32)Winner);
		}
		static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("AntClientShots"));
		if (bShots && Now >= NextShot)
		{
			NextShot = Now + 10.f;
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("wreck") / FString::Printf(TEXT("client_%02d.png"), ShotN++), true, false);
		}
		return;
	}
	if (!bActive)
	{
		return;
	}
	UWorld* W = GetWorld();
	const float Now = W->GetTimeSeconds();
	if (Now - LastAttack > 1.2f)
	{
		Patience = FMath::Min(MaxPatience, Patience + Regen * Dt);
	}
	const float Net = NetNowOf(W);
	if (Net >= NextXray)
	{
		XrayUntil = Net + XrayFor;
		NextXray = Net + XrayEvery;
	}
	AAntRaid* R = AAntRaid::Get(W);
	if (!R)
	{
		return;
	}
	if (R->InRaid())
	{
		bWasRaid = true;
		return;
	}
	if (bWasRaid && Winner == 0)
	{
		const bool bAnts = R->Result.Banked >= R->Result.Ask || R->Result.Cake == AntCake::Home;
		Winner = bAnts ? 1 : 2;
		bWasRaid = false;
		if (AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>())
		{
			GM->Broadcast(bAnts ? FString::Printf(TEXT("МУРАВЬИ ПОБЕДИЛИ: вынесено %d из %d"), R->Result.Banked, R->Result.Ask)
				: FString::Printf(TEXT("ЧЕЛОВЕК ПОБЕДИЛ: муравьи вынесли только %d из %d"), R->Result.Banked, R->Result.Ask), FLinearColor(1.f, 0.85f, 0.3f));
		}
		UE_LOG(LogTemp, Display, TEXT("ANTVERSUS end: %s won (banked %d of %d, cake %d), Human swings %d, refused for patience %d"), bAnts ? TEXT("the ants") : TEXT("the Human"),
			R->Result.Banked, R->Result.Ask, (int32)R->Result.Cake, Swings, Refused);
	}
}

bool& AntVersus::Wanted()
{
	static bool b = FParse::Param(FCommandLine::Get(), TEXT("AntVersus"));
	return b;
}

bool AntVersus::Active(const UWorld* World)
{
	const AAntVersus* V = AAntVersus::Get(World);
	return V && V->bActive;
}

float AntVersus::SwingCost(const AHumanCharacter* H)
{
	const AntWeapons::FDef* D = AntWeapons::Held(H);
	return 10.f + 6.f * (D ? D->Tier : 0);
}

bool AntVersus::Spend(AHumanCharacter* H, float Cost)
{
	AAntVersus* V = AAntVersus::Get(H ? H->GetWorld() : nullptr);
	if (!V || !V->bActive || !IsPlayerHuman(H))
	{
		return true;
	}
	if (V->Patience < Cost)
	{
		++V->Refused;
		if (AAntPlayerController* PC = Cast<AAntPlayerController>(H->GetController())) { PC->ClientToast(TEXT("Терпение лопнуло: отдышись")); }
		UE_LOG(LogTemp, Display, TEXT("ANTVERSUS no patience: %.0f < %.0f"), V->Patience, Cost);
		return false;
	}
	V->Patience -= Cost;
	V->LastAttack = H->GetWorld()->GetTimeSeconds();
	++V->Swings;
	return true;
}

bool AntVersus::XrayOn(const UWorld* World)
{
	const AAntVersus* V = AAntVersus::Get(World);
	return !V || !V->bActive || NetNowOf(World) < V->XrayUntil;
}

FString AntVersus::HudLine(const UWorld* World, const AHumanCharacter* H, float& OutPatience01)
{
	const AntWeapons::FDef* D = AntWeapons::Held(H);
	FString S = FString::Printf(TEXT("В руке: %s (Q — удар, E у предмета — взять)"), D ? D->Name : TEXT("газета"));
	OutPatience01 = -1.f;
	const AAntVersus* V = AAntVersus::Get(World);
	if (V && V->bActive)
	{
		OutPatience01 = V->Patience / AAntVersus::MaxPatience;
		const float Net = NetNowOf(World);
		S += Net < V->XrayUntil ? FString(TEXT("  ·  ЧУТЬЁ: видишь всех с грузом!")) : FString::Printf(TEXT("  ·  чутьё через %d с"), FMath::CeilToInt(V->NextXray - Net));
	}
	return S;
}

bool AntVersus::DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() == 0 || !A[0].Equals(TEXT("versus"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	UWorld* W = GM->GetWorld();
	const FString Op = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
	if (Op == TEXT("start"))
	{
		// versus start [client|host|ai]: who plays the Human (default: the first client, else the host)
		const FString Who = A.IsValidIndex(2) ? A[2].ToLower() : FString(TEXT("client"));
		AAntPlayerController* Pick = nullptr;
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get());
			if (!PC) { continue; }
			if (Who == TEXT("host") && PC->IsLocalController()) { Pick = PC; break; }
			if (Who == TEXT("client") && !PC->IsLocalController()) { Pick = PC; break; }
		}
		if (!Pick && Who != TEXT("ai"))
		{
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It) { if ((Pick = Cast<AAntPlayerController>(It->Get())) != nullptr) { break; } }
		}
		AAntVersus::Start(W, Who == TEXT("ai") ? nullptr : Pick);
	}
	else if (Op == TEXT("end"))
	{
		if (AAntVersus* V = AAntVersus::Get(W)) { V->bActive = false; }
	}
	else if (Op == TEXT("patience"))
	{
		if (AAntVersus* V = AAntVersus::Get(W)) { V->Patience = FCString::Atof(*A[2]); }
	}
	else
	{
		const AAntVersus* V = AAntVersus::Get(W);
		UE_LOG(LogTemp, Display, TEXT("ANTVERSUS info: active %d, Human %s, patience %.0f, swings %d, refused %d, winner %d"), V && V->bActive ? 1 : 0, V ? *V->HumanName : TEXT("-"),
			V ? V->Patience : 0.f, V ? V->Swings : 0, V ? V->Refused : 0, V ? (int32)V->Winner : 0);
	}
	return true;
}
