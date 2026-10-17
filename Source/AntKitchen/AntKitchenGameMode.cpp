#include "AntKitchenGameMode.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "AntGameState.h"
#include "AntHUD.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace GMColors
{
	const FLinearColor Good(0.4f, 1.f, 0.4f);
	const FLinearColor Bad(1.f, 0.35f, 0.3f);
	const FLinearColor Info(0.9f, 0.9f, 0.9f);
	const FLinearColor Gold(1.f, 0.8f, 0.2f);

	const FLinearColor AntColors[] =
	{
		FLinearColor(0.12f, 0.04f, 0.03f),  // classic black-brown
		FLinearColor(0.55f, 0.08f, 0.04f),  // fire ant red
		FLinearColor(0.60f, 0.40f, 0.05f),  // golden
		FLinearColor(0.10f, 0.20f, 0.45f),  // blue
		FLinearColor(0.15f, 0.40f, 0.10f),  // green
		FLinearColor(0.40f, 0.10f, 0.45f),  // purple
		FLinearColor(0.70f, 0.70f, 0.70f),  // albino
		FLinearColor(0.80f, 0.30f, 0.05f),  // orange
	};
}
using namespace GMColors;

AAntKitchenGameMode::AAntKitchenGameMode()
{
	DefaultPawnClass = AAntCharacter::StaticClass();
	PlayerControllerClass = AAntPlayerController::StaticClass();
	PlayerStateClass = AAntPlayerState::StaticClass();
	GameStateClass = AAntGameState::StaticClass();
	HUDClass = AAntHUD::StaticClass();
}

void AAntKitchenGameMode::BeginPlay()
{
	Super::BeginPlay();
	Rng.GenerateNewSeed();
	AKitchenBuilder::LootSpots(SpotLocations, SpotTiers);

	if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->TargetScore = TargetScore;
		GS->TimeRemaining = MatchLength;
		GS->Phase = 0;
	}

	SpawnHuman();
	SpawnInitialLoot();

	GetWorldTimerManager().SetTimer(MatchTimer, this, &AAntKitchenGameMode::MatchTick, 1.f, true);
	GetWorldTimerManager().SetTimer(RefillTimer, this, &AAntKitchenGameMode::RefillLoot, 10.f, true);
}

// ---------------------------------------------------------------------------------------------
// Players
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (AAntPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AAntPlayerState>() : nullptr)
	{
		PS->AntColor = AntColors[ColorIndex % UE_ARRAY_COUNT(AntColors)];
		++ColorIndex;
	}
	Super::PostLogin(NewPlayer);

	if (NewPlayer && NewPlayer->PlayerState)
	{
		Broadcast(FString::Printf(TEXT("%s заполз на кухню"), *NewPlayer->PlayerState->GetPlayerName()), Info);
	}
}

void AAntKitchenGameMode::Logout(AController* Exiting)
{
	if (Exiting && Exiting == HumanPlayer)
	{
		HandleHumanPlayerLeft(HumanPlayer);
	}
	Super::Logout(Exiting);
}

void AAntKitchenGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer) || NewPlayer == HumanPlayer)
	{
		return;
	}
	const FVector Nest = AKitchenBuilder::NestLocation();
	const FVector Loc = Nest + FVector(FMath::FRandRange(600.f, 900.f), FMath::FRandRange(-350.f, 350.f), 60.f);
	RestartPlayerAtTransform(NewPlayer, FTransform(FRotator::ZeroRotator, Loc));
}

void AAntKitchenGameMode::RespawnAnt(AController* C)
{
	if (!IsValid(C) || C == HumanPlayer)
	{
		return;
	}
	if (APawn* P = C->GetPawn())
	{
		AAntCharacter* Ant = Cast<AAntCharacter>(P);
		if (Ant && !Ant->bDead)
		{
			return;   // already alive (e.g. round reset happened)
		}
		C->UnPossess();
		P->Destroy();
	}
	RestartPlayer(C);
}

// ---------------------------------------------------------------------------------------------
// The Human
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::SpawnHuman()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Human = GetWorld()->SpawnActor<AHumanCharacter>(AHumanCharacter::StaticClass(), AKitchenBuilder::HumanSpawn(), FRotator(0.f, 180.f, 0.f), Params);
	if (Human && !Human->GetController())
	{
		Human->SpawnDefaultController();
	}
}

void AAntKitchenGameMode::ToggleHuman(AAntPlayerController* PC)
{
	if (!PC || !Human)
	{
		return;
	}
	AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>();
	const FString Name = PS ? PS->GetPlayerName() : FString(TEXT("Игрок"));

	if (HumanPlayer == PC)
	{
		PC->UnPossess();
		HumanPlayer = nullptr;
		if (PS) { PS->bIsHuman = false; }
		if (!Human->GetController()) { Human->SpawnDefaultController(); }
		RestartPlayer(PC);
		Broadcast(FString::Printf(TEXT("%s снова муравей. Человеком рулит ИИ"), *Name), Info);
		return;
	}

	if (HumanPlayer)
	{
		PC->ClientToast(TEXT("Человеком уже управляет другой игрок"));
		return;
	}

	if (APawn* Old = PC->GetPawn())
	{
		if (AAntCharacter* Ant = Cast<AAntCharacter>(Old))
		{
			Ant->DropEverything();
		}
		PC->UnPossess();
		Old->Destroy();
	}
	if (AController* AI = Human->GetController())
	{
		AI->UnPossess();
		AI->Destroy();
	}

	PC->Possess(Human);
	HumanPlayer = PC;
	if (PS) { PS->bIsHuman = true; }
	Broadcast(FString::Printf(TEXT("%s теперь ЧЕЛОВЕК! Муравьи, берегитесь тапка!"), *Name), Bad);
}

void AAntKitchenGameMode::HandleHumanPlayerLeft(AAntPlayerController* PC)
{
	if (!PC || PC != HumanPlayer)
	{
		return;
	}
	if (Human && Human->GetController() == PC)
	{
		PC->UnPossess();
	}
	if (AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>())
	{
		PS->bIsHuman = false;
	}
	HumanPlayer = nullptr;
	if (Human && !Human->GetController())
	{
		Human->SpawnDefaultController();
	}
	Broadcast(TEXT("Игрок-Человек ушёл. Тапком снова управляет ИИ"), Info);
}

// ---------------------------------------------------------------------------------------------
// Loot
// ---------------------------------------------------------------------------------------------

int32 AAntKitchenGameMode::RollTier(int32 Tier)
{
	return FMath::Clamp(Tier + (Rng.FRand() < 0.1f ? 1 : 0), 0, 3);
}

int32 AAntKitchenGameMode::RollRarity(int32 Tier)
{
	const float R = Rng.FRand();
	return FMath::Clamp(Tier - 1 + (R > 0.6f ? 1 : 0) + (R > 0.9f ? 1 : 0), 0, 3);
}

AKitchenLoot* AAntKitchenGameMode::SpawnFood(const FVector& Location, int32 Tier)
{
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitFood(AntKit::RollFood(Tier, Rng));
		L->FinishSpawning(FTransform(Location));
	}
	return L;
}

AKitchenLoot* AAntKitchenGameMode::SpawnGear(const FVector& Location, const FAntWeapon& Weapon)
{
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitGear(Weapon);
		L->FinishSpawning(FTransform(Location));
	}
	return L;
}

AKitchenLoot* AAntKitchenGameMode::SpawnStash(const FVector& Location, int32 Value, int32 Weight, int32 Items)
{
	FAntFoodDef D;
	D.Name = FString::Printf(TEXT("Брошенный груз (%d шт.)"), Items);
	D.Value = Value;
	D.Weight = Weight;
	D.Color = FLinearColor(1.f, 0.75f, 0.2f);
	D.Shape = EAntShape::Sphere;
	D.Scale = FVector(0.35f);

	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitFood(D);
		L->FinishSpawning(FTransform(Location));
	}
	return L;
}

bool AAntKitchenGameMode::IsSpotFree(const FVector& Location) const
{
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), Location) < 120.f)
		{
			return false;
		}
	}
	return true;
}

void AAntKitchenGameMode::SpawnInitialLoot()
{
	const int32 N = SpotLocations.Num();
	for (int32 i = 0; i < N; ++i)
	{
		if (Rng.FRand() < 0.8f)
		{
			SpawnFood(SpotLocations[i], RollTier(SpotTiers[i]));
		}
	}
	for (int32 g = 0; g < 10 && N > 0; ++g)
	{
		const int32 i = Rng.RandRange(0, N - 1);
		const FVector Loc = SpotLocations[i] + FVector(Rng.FRandRange(-70.f, 70.f), Rng.FRandRange(-70.f, 70.f), 0.f);
		SpawnGear(Loc, AntKit::RollWeapon(RollRarity(SpotTiers[i]), Rng));
	}
}

void AAntKitchenGameMode::RefillLoot()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0 || SpotLocations.Num() == 0)
	{
		return;
	}
	int32 Food = 0, Gear = 0;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		(It->bIsGear ? Gear : Food)++;
	}

	const int32 Want = FMath::RoundToInt(SpotLocations.Num() * 0.6f);
	for (int32 n = 0; n < 4 && Food < Want; ++n)
	{
		const int32 i = Rng.RandRange(0, SpotLocations.Num() - 1);
		if (IsSpotFree(SpotLocations[i]))
		{
			SpawnFood(SpotLocations[i], RollTier(SpotTiers[i]));
			++Food;
		}
	}
	if (Gear < 6)
	{
		const int32 i = Rng.RandRange(0, SpotLocations.Num() - 1);
		if (IsSpotFree(SpotLocations[i]))
		{
			SpawnGear(SpotLocations[i], AntKit::RollWeapon(RollRarity(SpotTiers[i]), Rng));
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::Broadcast(const FString& Text, const FLinearColor& Color)
{
	if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->MulticastMessage(Text, Color);
	}
}

void AAntKitchenGameMode::OnLootDelivered(AAntCharacter* Ant, int32 Value, int32 Items)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || !Ant || GS->Phase != 0)
	{
		return;
	}
	GS->TeamScore += Value;
	if (AAntPlayerState* PS = Ant->GetPlayerState<AAntPlayerState>())
	{
		PS->Delivered += Value;
	}
	if (AAntPlayerController* PC = Cast<AAntPlayerController>(Ant->GetController()))
	{
		PC->ClientToast(FString::Printf(TEXT("+%d в общак колонии!"), Value));
	}
	Broadcast(FString::Printf(TEXT("%s притащил в муравейник %d шт. (+%d)"), *Ant->GetDisplayName(), Items, Value), Good);

	if (GS->TeamScore >= GS->TargetScore)
	{
		EndMatch(true);
	}
}

void AAntKitchenGameMode::OnAntKilled(AAntCharacter* Ant, AController* Killer, AActor* Causer)
{
	if (!Ant)
	{
		return;
	}
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (GS)
	{
		GS->AntsSquashed++;
	}

	FString How = TEXT("погиб");
	if (Causer && Causer->IsA<AKitchenBuilder>())
	{
		How = TEXT("поджарился на плите");
	}
	else if (const AHumanCharacter* H = Cast<AHumanCharacter>(Causer))
	{
		How = H->bSpraying ? TEXT("задохнулся дихлофосом") : TEXT("раздавлен тапком");
	}
	if (Killer)
	{
		if (AAntPlayerState* KPS = Killer->GetPlayerState<AAntPlayerState>())
		{
			KPS->Kills++;
		}
	}
	Broadcast(FString::Printf(TEXT("%s %s"), *Ant->GetDisplayName(), *How), Bad);

	TWeakObjectPtr<AController> WeakC = Ant->GetController();
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakC]()
	{
		RespawnAnt(WeakC.Get());
	}), RespawnDelay, false);
}

void AAntKitchenGameMode::OnHumanKnockedOut(AController* By)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	GS->HumanKnockouts++;
	GS->TeamScore += 100;
	FString Who;
	if (By && By->PlayerState)
	{
		Who = FString::Printf(TEXT(" Добил: %s"), *By->PlayerState->GetPlayerName());
	}
	Broadcast(FString::Printf(TEXT("ЧЕЛОВЕК В ПАНИКЕ УБЕГАЕТ! Колония +100.%s"), *Who), Gold);

	if (GS->TeamScore >= GS->TargetScore)
	{
		EndMatch(true);
	}
}

// ---------------------------------------------------------------------------------------------
// Match flow
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::MatchTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	GS->TimeRemaining = FMath::Max(0.f, GS->TimeRemaining - 1.f);
	if (GS->TimeRemaining <= 0.f)
	{
		EndMatch(false);
	}
}

void AAntKitchenGameMode::EndMatch(bool bAntsWin)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	GS->Phase = bAntsWin ? 1 : 2;
	Broadcast(bAntsWin ? TEXT("КОЛОНИЯ НАЖРАЛАСЬ! МУРАВЬИ ПОБЕДИЛИ!") : TEXT("ВРЕМЯ ВЫШЛО! ЧЕЛОВЕК ОТСТОЯЛ КУХНЮ!"), Gold);
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AAntKitchenGameMode::ResetMatch, 12.f, false);
}

void AAntKitchenGameMode::ResetMatch()
{
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	SpawnInitialLoot();

	if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->TeamScore = 0;
		GS->TimeRemaining = MatchLength;
		GS->Phase = 0;
		GS->AntsSquashed = 0;
		GS->HumanKnockouts = 0;
	}
	if (Human)
	{
		Human->ResetHuman(AKitchenBuilder::HumanSpawn());
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>())
		{
			PS->Delivered = 0;
			PS->Deaths = 0;
			PS->Kills = 0;
		}
		if (PC == HumanPlayer)
		{
			continue;
		}
		if (APawn* P = PC->GetPawn())
		{
			PC->UnPossess();
			P->Destroy();
		}
		RestartPlayer(PC);
	}
	Broadcast(TEXT("Новый раунд! Тащите всё сладкое в муравейник!"), Gold);
}
