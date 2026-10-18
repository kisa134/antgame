#include "AntKitchenGameMode.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "AntGameState.h"
#include "AntHUD.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
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
	SpawnGadgets();
	SpawnInitialLoot();

	GetWorldTimerManager().SetTimer(MatchTimer, this, &AAntKitchenGameMode::MatchTick, 1.f, true);
	GetWorldTimerManager().SetTimer(RefillTimer, this, &AAntKitchenGameMode::RefillLoot, 10.f, true);
	GetWorldTimerManager().SetTimer(LifeTimer, this, &AAntKitchenGameMode::KitchenLifeEvent, 38.f, true, 25.f);
}

// ---------------------------------------------------------------------------------------------
// Kitchen gadgets (replicated actors): doors, faucet, light switch, vents, toaster, wire, trap, props
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::SpawnGadgets()
{
	UWorld* W = GetWorld();
	auto Spawn = [W](UClass* Class, const FVector& Loc, const FRotator& Rot, TFunctionRef<void(AActor*)> Setup) -> AActor*
	{
		const FTransform TM(Rot, Loc);
		AActor* A = W->SpawnActorDeferred<AActor>(Class, TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (A)
		{
			Setup(A);
			A->FinishSpawning(TM);
		}
		return A;
	};
	auto Door = [&Spawn](EKitchenDoorKind Kind, const FVector& Loc)
	{
		return Cast<AKitchenDoor>(Spawn(AKitchenDoor::StaticClass(), Loc, FRotator::ZeroRotator, [Kind](AActor* A) { CastChecked<AKitchenDoor>(A)->Kind = Kind; }));
	};
	auto Passage = [&Spawn](EPassageLook Look, const FVector& Loc, float Yaw, const FVector& Exit, const TCHAR* Name)
	{
		Spawn(AKitchenPassage::StaticClass(), Loc, FRotator(0.f, Yaw, 0.f), [Look, Exit, Name](AActor* A)
		{
			AKitchenPassage* P = CastChecked<AKitchenPassage>(A);
			P->Look = Look;
			P->Exit = Exit;
			P->RouteName = Name;
		});
	};
	auto NoSetup = [](AActor*) {};

	FridgeDoor = Door(EKitchenDoorKind::Fridge, FVector(4700.f, 3100.f, 120.f));
	Door(EKitchenDoorKind::Oven, FVector(-750.f, 3000.f, 300.f));
	Door(EKitchenDoorKind::Drawer, FVector(-4950.f, 3000.f, 1330.f));

	Faucet = Cast<AKitchenFaucet>(Spawn(AKitchenFaucet::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, NoSetup));
	Spawn(AKitchenSwitch::StaticClass(), FVector(-6995.f, 1900.f, 2300.f), FRotator::ZeroRotator, NoSetup);
	Spawn(AKitchenToaster::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, NoSetup);
	Spawn(AKitchenWire::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, NoSetup);
	Spawn(AKitchenTrap::StaticClass(), FVector(-3150.f, 3450.f, 140.f), FRotator(0.f, 90.f, 0.f), NoSetup);

	// Secret routes (each end leads to the other).
	Passage(EPassageLook::Vent, FVector(-3100.f, -4995.f, 0.f), 90.f, FVector(-4500.f, 4500.f, 4470.f), TEXT("вентиляция → на верхние шкафы"));
	Passage(EPassageLook::Vent, FVector(-4500.f, 4995.f, 4400.f), -90.f, FVector(-3100.f, -4750.f, 60.f), TEXT("вентиляция → к кошачьим мискам"));
	Passage(EPassageLook::Vent, FVector(6995.f, 400.f, 0.f), 180.f, FVector(3700.f, 4350.f, 3760.f), TEXT("вентиляция → на холодильник"));
	Passage(EPassageLook::Vent, FVector(3700.f, 4995.f, 3700.f), -90.f, FVector(6750.f, 400.f, 60.f), TEXT("вентиляция → в мусорный угол"));
	Passage(EPassageLook::Drain, FVector(-3500.f, 4000.f, 1150.f), 0.f, FVector(-3500.f, 3600.f, 200.f), TEXT("слив раковины → под мойку"));
	Passage(EPassageLook::Pipe, FVector(-3300.f, 4300.f, 140.f), 0.f, FVector(-3700.f, 3800.f, 1210.f), TEXT("труба → в раковину"));

	// Physics junk to push around and shoot at.
	struct FPropDef { EAntShape Shape; FVector Loc; FVector Size; FLinearColor Color; float Mass; };
	const FPropDef Props[] =
	{
		{ EAntShape::Cylinder, FVector(-1200.f, -3000.f, 30.f), FVector(120.f, 120.f, 30.f), FLinearColor(0.8f, 0.1f, 0.1f), 1.5f },  // bottle cap
		{ EAntShape::Sphere, FVector(200.f, -200.f, 1450.f), FVector(90.f), FLinearColor(0.5f, 0.85f, 0.3f), 1.f },               // grape on the table
		{ EAntShape::Sphere, FVector(-400.f, -1000.f, 1450.f), FVector(90.f), FLinearColor(0.5f, 0.85f, 0.3f), 1.f },
		{ EAntShape::Sphere, FVector(-3000.f, 1500.f, 40.f), FVector(55.f), FLinearColor(0.3f, 0.75f, 0.2f), 0.6f },              // peas
		{ EAntShape::Sphere, FVector(-2900.f, 1550.f, 40.f), FVector(55.f), FLinearColor(0.3f, 0.75f, 0.2f), 0.6f },
		{ EAntShape::Cylinder, FVector(-900.f, -300.f, 1450.f), FVector(70.f, 70.f, 80.f), FLinearColor(0.75f, 0.75f, 0.8f), 1.2f },   // thimble
		{ EAntShape::Cube, FVector(1700.f, 3500.f, 1750.f), FVector(70.f), FLinearColor(1.f, 1.f, 1.f), 0.8f },                    // sugar cubes
		{ EAntShape::Cube, FVector(1780.f, 3560.f, 1750.f), FVector(70.f), FLinearColor(1.f, 1.f, 1.f), 0.8f },
		{ EAntShape::Cylinder, FVector(3600.f, 3800.f, 20.f), FVector(70.f, 70.f, 10.f), FLinearColor(0.75f, 0.6f, 0.3f), 2.f },    // coin under the fridge
		{ EAntShape::Sphere, FVector(4000.f, -2500.f, 50.f), FVector(100.f), FLinearColor(0.9f, 0.5f, 0.1f), 1.5f },               // orange pea... a meatball
	};
	for (const FPropDef& D : Props)
	{
		Spawn(AKitchenProp::StaticClass(), D.Loc, FRotator::ZeroRotator, [&D](AActor* A)
		{
			AKitchenProp* P = CastChecked<AKitchenProp>(A);
			P->ShapeIndex = (uint8)D.Shape;
			P->Size = D.Size;
			P->Color = D.Color;
			P->Mass = D.Mass;
		});
	}
}

void AAntKitchenGameMode::KitchenLifeEvent()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	const int32 Roll = Rng.RandRange(0, 3);
	if (Roll == 0 && FridgeDoor && !FridgeDoor->IsOpen())
	{
		FridgeDoor->SetOpen(true, 9.f);
		Broadcast(TEXT("Человек полез в холодильник за молоком — дверь открыта!"), Info);
	}
	else if (Roll == 1 && Faucet && !Faucet->IsOn())
	{
		const bool bHot = Rng.FRand() < 0.4f;
		Faucet->SetWater(true, bHot);
		Broadcast(bHot ? TEXT("Человек моет посуду горячей водой — раковина наполняется кипятком!") : TEXT("Человек включил воду в раковине"), Info);
		TWeakObjectPtr<AKitchenFaucet> WeakF = Faucet;
		GetWorldTimerManager().SetTimer(FaucetOffTimer, FTimerDelegate::CreateWeakLambda(this, [WeakF]()
		{
			if (WeakF.IsValid()) { WeakF->SetWater(false, false); }
		}), 14.f, false);
	}
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
	// Inside the safe zone of the nest (the old spot was outside it and the Human camped it).
	const FVector Loc = Nest + FVector(FMath::FRandRange(180.f, 420.f), FMath::FRandRange(-300.f, 300.f), 60.f);
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

void AAntKitchenGameMode::TogglePeace()
{
	if (!Human)
	{
		return;
	}
	if (AController* C = Human->GetController())
	{
		if (C != HumanPlayer)
		{
			const bool bWasOn = C->IsActorTickEnabled();
			C->SetActorTickEnabled(!bWasOn);
			Human->SetSpraying(false);
			Broadcast(bWasOn ? TEXT("[dev] Человек отдыхает") : TEXT("[dev] Человек снова охотится"), Info);
		}
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
	if (!Ant->LastHazard.IsEmpty() && GetWorld()->GetTimeSeconds() - Ant->LastHazardTime < 3.f)
	{
		How = Ant->LastHazard;
	}
	if (const AHumanCharacter* H = Cast<AHumanCharacter>(Causer))
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
