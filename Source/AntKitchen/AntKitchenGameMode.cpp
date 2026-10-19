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
#include "AntSwarm.h"
#include "AntBotController.h"
#include "CreatureCharacters.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
		FLinearColor(0.05f, 0.35f, 0.35f),  // teal (red is for the fire ants)
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
	SpawnSwarm();
	SpawnInitialLoot();

	// Bots: -AntBots=allies,rivals on the command line (default 2,3).
	FString BotArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntBots="), BotArg, false))
	{
		TArray<FString> Parts;
		BotArg.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 1) { AllyBots = FCString::Atoi(*Parts[0]); }
		if (Parts.Num() >= 2) { RivalBots = FCString::Atoi(*Parts[1]); }
	}
	FTimerHandle BotTimer;
	GetWorldTimerManager().SetTimer(BotTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		AddBots(0, AllyBots);
		AddBots(1, RivalBots);
		SpawnSpider();
		SpawnFly();
		SpawnFly();
	}), 1.5f, false);

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

void AAntKitchenGameMode::SpawnSwarm()
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AAntSwarm>(AAntSwarm::StaticClass(), FTransform::Identity, P);
}

void AAntKitchenGameMode::OnSwarmIncome(int32 Value)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0 || Value <= 0)
	{
		return;
	}
	if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
	{
		Swarm->AddPopulation(Value * 2);
	}
	AddScore(0, Value);
}

void AAntKitchenGameMode::AddScore(uint8 Team, int32 Value)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	(Team == 0 ? GS->TeamScore : GS->RivalScore) += Value;
	UpdateRage();
	if (GS->TeamScore >= GS->TargetScore)
	{
		EndMatch(1);
	}
	else if (GS->RivalScore >= GS->TargetScore)
	{
		EndMatch(3);
	}
}

void AAntKitchenGameMode::UpdateRage()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || !Human)
	{
		return;
	}
	const float Frac = (float)FMath::Max(GS->TeamScore, GS->RivalScore) / FMath::Max(1, GS->TargetScore);
	const uint8 NewRage = Frac >= 0.75f ? 3 : Frac >= 0.5f ? 2 : Frac >= 0.25f ? 1 : 0;
	if (NewRage > Human->Rage)
	{
		Human->SetRage(NewRage);
		GS->HumanRage = NewRage;
		static const TCHAR* Lines[] = { TEXT(""), TEXT("Человек заметил пропажи и ЗЛИТСЯ (ярость 1)"), TEXT("Человек В БЕШЕНСТВЕ: быстрее топчет и дальше брызгает (ярость 2)"), TEXT("ЧЕЛОВЕК ОБЪЯВИЛ ВОЙНУ МУРАВЬЯМ! (ярость 3)") };
		Broadcast(Lines[NewRage], Bad);
		AntSfx::Everyone(this, AntSfx::Rage, Human->GetActorLocation(), 1.f - 0.1f * NewRage);
	}
}

void AAntKitchenGameMode::SpawnFly()
{
	if (FlyCount >= 4)
	{
		return;
	}
	const FBox A = AKitchenBuilder::PlayArea();
	const FVector L(FMath::FRandRange(A.Min.X, A.Max.X), FMath::FRandRange(A.Min.Y, A.Max.Y), 2500.f);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AFlyCharacter* Fly = GetWorld()->SpawnActor<AFlyCharacter>(AFlyCharacter::StaticClass(), L, FRotator::ZeroRotator, P);
	if (Fly)
	{
		Fly->Team = 2;
		AFlyAIController* C = GetWorld()->SpawnActor<AFlyAIController>(AFlyAIController::StaticClass(), FTransform::Identity, P);
		if (C) { C->Possess(Fly); }
		++FlyCount;
	}
}

void AAntKitchenGameMode::SpawnSpider()
{
	if (Spider)
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector L = AKitchenBuilder::SpiderLair() - FVector(60.f, 0.f, 0.f);
	Spider = GetWorld()->SpawnActor<ASpiderCharacter>(ASpiderCharacter::StaticClass(), L, FRotator(0.f, 180.f, 0.f), P);
	if (Spider)
	{
		AAntBotController* C = GetWorld()->SpawnActor<AAntBotController>(AAntBotController::StaticClass(), FTransform::Identity, P);
		if (C)
		{
			if (AAntPlayerState* PS = C->GetPlayerState<AAntPlayerState>())
			{
				PS->Team = 2;
				PS->SetPlayerName(TEXT("Паук"));
				PS->AntColor = FLinearColor(0.2f, 0.12f, 0.07f);
			}
			C->Possess(Spider);
		}
		Broadcast(TEXT("В углу над мусоркой сплёл паутину ПАУК. Он охотится на всех муравьёв"), Bad);
	}
}

void AAntKitchenGameMode::ToggleCreature(AAntPlayerController* PC, uint8 Kind)
{
	if (!PC || PC == HumanPlayer)
	{
		return;
	}
	APawn* Old = PC->GetPawn();
	AAntCharacter* OldAnt = Cast<AAntCharacter>(Old);
	if (OldAnt && OldAnt->bDead)
	{
		return;
	}
	const FVector Where = Old ? Old->GetActorLocation() : AKitchenBuilder::NestLocation(0);

	// Back to being an ant.
	if (OldAnt && OldAnt->Creature == Kind)
	{
		OldAnt->DropEverything();
		PC->UnPossess();
		if (Kind == 2)
		{
			// Give the spider back to the AI.
			if (AAntBotController* C = GetWorld()->SpawnActor<AAntBotController>(AAntBotController::StaticClass(), FTransform::Identity))
			{
				if (AAntPlayerState* PS = C->GetPlayerState<AAntPlayerState>()) { PS->Team = 2; PS->SetPlayerName(TEXT("Паук")); }
				C->Possess(OldAnt);
			}
		}
		else
		{
			OldAnt->Destroy();
		}
		RestartPlayer(PC);
		return;
	}

	APawn* NewPawn = nullptr;
	if (Kind == 1)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		NewPawn = GetWorld()->SpawnActor<AFlyCharacter>(AFlyCharacter::StaticClass(), Where + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, P);
	}
	else
	{
		if (!Spider || Spider->bDead)
		{
			PC->ClientToast(TEXT("Паука сейчас нет (он возродится в своей паутине)"));
			return;
		}
		if (Spider->GetController() && Spider->GetController()->IsPlayerController())
		{
			PC->ClientToast(TEXT("Пауком уже управляет другой игрок"));
			return;
		}
		if (AController* AI = Spider->GetController())
		{
			AI->UnPossess();
			AI->Destroy();
		}
		NewPawn = Spider;
	}
	if (!NewPawn)
	{
		return;
	}
	if (OldAnt)
	{
		OldAnt->DropEverything();
	}
	PC->UnPossess();
	if (Old && Old != NewPawn)
	{
		Old->Destroy();
	}
	PC->Possess(NewPawn);
	const FString Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(TEXT("Игрок"));
	Broadcast(Kind == 1 ? FString::Printf(TEXT("%s превратился в МУХУ"), *Name) : FString::Printf(TEXT("%s теперь ПАУК! Прячьтесь все"), *Name), Kind == 1 ? Info : Bad);
}

void AAntKitchenGameMode::AddBots(uint8 Team, int32 Count)
{
	static const TCHAR* AllyNames[] = { TEXT("Жужа"), TEXT("Крошкин"), TEXT("Тяга"), TEXT("Сахарок"), TEXT("Лапкин"), TEXT("Усач"), TEXT("Кислый"), TEXT("Шустрик") };
	static const TCHAR* RivalNames[] = { TEXT("Огнёвка"), TEXT("Жгучка"), TEXT("Пламя"), TEXT("Искра"), TEXT("Уголёк"), TEXT("Пепел"), TEXT("Перец"), TEXT("Жар") };
	static const EAntClass Rotation[] = { EAntClass::Worker, EAntClass::Soldier, EAntClass::Worker, EAntClass::Scout, EAntClass::Nurse, EAntClass::Worker, EAntClass::Soldier };
	Team = FMath::Min<uint8>(Team, 1);
	for (int32 i = 0; i < FMath::Clamp(Count, 0, 12); ++i)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAntBotController* C = GetWorld()->SpawnActor<AAntBotController>(AAntBotController::StaticClass(), FTransform::Identity, P);
		if (!C)
		{
			continue;
		}
		const int32 N = BotCounter[Team]++;
		const EAntClass Cls = Rotation[N % UE_ARRAY_COUNT(Rotation)];
		if (AAntPlayerState* PS = C->GetPlayerState<AAntPlayerState>())
		{
			PS->Team = Team;
			PS->AntClass = Cls;
			PS->AntColor = AntKit::TeamColor(Team, N);
			const TCHAR* Base = Team == 0 ? AllyNames[N % 8] : RivalNames[N % 8];
			PS->SetPlayerName(FString::Printf(TEXT("[бот] %s %s"), Base, AntKit::ClassDef(Cls).Name));
		}
		RestartPlayer(C);
	}
	if (Count > 0)
	{
		Broadcast(Team == 0 ? FString::Printf(TEXT("В колонию вылупились помощники: %d"), Count)
			: FString::Printf(TEXT("Из-под мусорки лезут огненные муравьи: %d! Они тоже за едой"), Count), Team == 0 ? Good : Bad);
	}
}

void AAntKitchenGameMode::SpawnHeavyLoot()
{
	TArray<FVector> Spots;
	AKitchenBuilder::HeavySpots(Spots);
	for (int32 i = 0; i < Spots.Num(); ++i)
	{
		const FVector L = Spots[i] + FVector(0.f, 0.f, 40.f);
		AKitchenLoot* Food = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(L), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Food)
		{
			Food->InitFood(AntKit::HeavyFood(i));
			Food->FinishSpawning(FTransform(L));
		}
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
	const AAntPlayerState* PS = NewPlayer->GetPlayerState<AAntPlayerState>();
	const uint8 Team = PS ? PS->Team : 0;
	const FVector Nest = AKitchenBuilder::NestLocation(Team);
	// Inside the safe zone of the nest; both nests open towards the middle of the kitchen.
	const float Out = Team == 0 ? 1.f : -1.f;
	const FVector Loc = Nest + FVector(Out * FMath::FRandRange(180.f, 380.f), FMath::FRandRange(-280.f, 280.f), 60.f);
	RestartPlayerAtTransform(NewPlayer, FTransform(FRotator(0.f, Team == 0 ? 0.f : 180.f, 0.f), Loc));
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
	SpawnHeavyLoot();
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
	int32 Heavy = 0;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (!It->bIsGear && It->Weight >= 20) { ++Heavy; }
	}
	if (Heavy < 3)
	{
		TArray<FVector> Spots;
		AKitchenBuilder::HeavySpots(Spots);
		const int32 k = Rng.RandRange(0, Spots.Num() - 1);
		if (IsSpotFree(Spots[k] + FVector(0.f, 0.f, 40.f)))
		{
			AKitchenLoot* Big = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Spots[k] + FVector(0.f, 0.f, 40.f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Big)
			{
				Big->InitFood(AntKit::HeavyFood(Rng.RandRange(0, 3)));
				Big->FinishSpawning(FTransform(Spots[k] + FVector(0.f, 0.f, 40.f)));
				Broadcast(FString::Printf(TEXT("Человек уронил «%s»! Это тяжело — тащите вместе"), *Big->LootName), Gold);
			}
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

void AAntKitchenGameMode::OnFoodDelivered(uint8 Team, AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || !Food || GS->Phase != 0)
	{
		return;
	}
	const int32 Value = Food->Value;
	FString Names;
	const int32 Share = FMath::Max(1, Value / FMath::Max(1, Carriers.Num()));
	for (AAntCharacter* A : Carriers)
	{
		if (!A) { continue; }
		if (AAntPlayerState* PS = A->GetPlayerState<AAntPlayerState>())
		{
			PS->Delivered += Share;
		}
		if (AAntPlayerController* PC = Cast<AAntPlayerController>(A->GetController()))
		{
			PC->ClientToast(FString::Printf(TEXT("+%d в общак колонии!"), Value));
		}
		Names += (Names.IsEmpty() ? TEXT("") : TEXT(" + ")) + A->GetDisplayName();
	}
	if (Team == 0)
	{
		if (AAntSwarm* Swarm = AAntSwarm::Get(GetWorld()))
		{
			Swarm->AddPopulation(Value * 4);   // food feeds the brood: more workers hatch
		}
	}
	Broadcast(FString::Printf(TEXT("%s: %s притащили «%s» (+%d)"), Team == 0 ? TEXT("КОЛОНИЯ") : TEXT("ОГНЕННЫЕ"), *Names, *Food->LootName, Value), Team == 0 ? Good : Bad);
	AddScore(Team, Value);
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
		// Kill streaks: kills within 5 seconds of each other.
		const float Now = GetWorld()->GetTimeSeconds();
		FVector2D& St = Streaks.FindOrAdd(Killer);
		St.X = (Now - St.Y < 5.f) ? St.X + 1.f : 1.f;
		St.Y = Now;
		const int32 N = (int32)St.X;
		const TCHAR* StreakName = N == 2 ? TEXT("ДВОЙНОЕ!") : N == 3 ? TEXT("ТРОЙНОЕ!!") : N >= 4 ? TEXT("МУРАВЬИНАЯ БОЙНЯ!!!") : TEXT("");
		if (AAntPlayerController* KPC = Cast<AAntPlayerController>(Killer))
		{
			KPC->ClientKillConfirm(Ant->GetDisplayName(), StreakName);
		}
		if (N >= 3 && Killer->PlayerState)
		{
			Broadcast(FString::Printf(TEXT("%s: %s"), *Killer->PlayerState->GetPlayerName(), StreakName), Gold);
		}
	}
	Broadcast(FString::Printf(TEXT("%s %s"), *Ant->GetDisplayName(), *How), Bad);

	// Creatures drop rewards; AI creatures come back later on their own.
	if (Ant->Creature != 0)
	{
		const FVector At = Ant->GetActorLocation();
		uint8 KillerTeam = 255;
		if (Killer)
		{
			if (const AAntCharacter* KA = Cast<AAntCharacter>(Killer->GetPawn())) { KillerTeam = KA->Team; }
		}
		FAntFoodDef D;
		D.Shape = EAntShape::Sphere;
		if (Ant->Creature == 1)
		{
			D.Name = TEXT("Дохлая муха");
			D.Value = 70; D.Weight = 12; D.Color = FLinearColor(0.1f, 0.1f, 0.1f); D.Scale = FVector(0.3f);
			if (AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				L->InitFood(D);
				L->FinishSpawning(FTransform(At));
			}
		}
		else
		{
			D.Name = TEXT("Паучья лапка");
			D.Value = 90; D.Weight = 14; D.Color = FLinearColor(0.2f, 0.12f, 0.07f); D.Scale = FVector(0.5f, 0.12f, 0.12f);
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector P = At + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), 30.f);
				if (AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(P), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					L->InitFood(D);
					L->FinishSpawning(FTransform(P));
				}
			}
			SpawnGear(At + FVector(0.f, 0.f, 40.f), AntKit::RollWeapon(3, Rng));
			if (KillerTeam < 2) { AddScore(KillerTeam, 200); }
			Broadcast(KillerTeam < 2 ? FString::Printf(TEXT("ПАУК ПОВЕРЖЕН! %s +200 и легендарка"), KillerTeam == 0 ? TEXT("Колония") : TEXT("Огненные"))
				: FString(TEXT("Паук погиб")), Gold);
		}
		AController* C = Ant->GetController();
		if (C && !C->IsPlayerController())
		{
			const uint8 Kind = Ant->Creature;
			TWeakObjectPtr<AController> WeakAI = C;
			TWeakObjectPtr<AAntCharacter> WeakBody = Ant;
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, WeakAI, WeakBody, Kind]()
			{
				if (WeakBody.IsValid()) { WeakBody->Destroy(); }
				if (WeakAI.IsValid()) { WeakAI->Destroy(); }
				if (Kind == 1) { --FlyCount; SpawnFly(); } else { Spider = nullptr; SpawnSpider(); }
			}), Kind == 1 ? 40.f : 90.f, false);
			return;
		}
		if (Ant->Creature == 2)
		{
			Spider = nullptr;
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { SpawnSpider(); }), 90.f, false);
		}
	}

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
	FString Who;
	if (By && By->PlayerState)
	{
		Who = FString::Printf(TEXT(" Добил: %s"), *By->PlayerState->GetPlayerName());
	}
	uint8 Team = 0;
	if (By)
	{
		if (const AAntPlayerState* PS = By->GetPlayerState<AAntPlayerState>()) { Team = PS->Team; }
	}
	Broadcast(FString::Printf(TEXT("ЧЕЛОВЕК В ПАНИКЕ УБЕГАЕТ! %s +100.%s"), Team == 0 ? TEXT("Колония") : TEXT("Огненные"), *Who), Gold);
	AddScore(Team, 100);
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
		// Time's up: the richer colony wins; nobody ahead - the Human defended his kitchen.
		EndMatch(GS->TeamScore > GS->RivalScore ? 1 : GS->RivalScore > GS->TeamScore ? 3 : 2);
	}
}

void AAntKitchenGameMode::EndMatch(uint8 Winner)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	GS->Phase = Winner;
	Broadcast(Winner == 1 ? TEXT("КОЛОНИЯ НАЖРАЛАСЬ! МЫ ПОБЕДИЛИ!") : Winner == 3 ? TEXT("ОГНЕННЫЕ МУРАВЬИ УТАЩИЛИ ВСЮ КУХНЮ...") : TEXT("ВРЕМЯ ВЫШЛО! ЧЕЛОВЕК ОТСТОЯЛ КУХНЮ!"), Gold);
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AAntKitchenGameMode::ResetMatch, 12.f, false);
}

void AAntKitchenGameMode::ResetMatch()
{
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		It->LetGo();
	}
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	SpawnInitialLoot();

	if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->TeamScore = 0;
		GS->RivalScore = 0;
		GS->HumanRage = 0;
		GS->TimeRemaining = MatchLength;
		GS->Phase = 0;
		GS->AntsSquashed = 0;
		GS->HumanKnockouts = 0;
	}
	if (Human)
	{
		Human->ResetHuman(AKitchenBuilder::HumanSpawn());
		Human->SetRage(0);
	}
	for (TActorIterator<AAntBotController> It(GetWorld()); It; ++It)
	{
		if (Cast<ASpiderCharacter>(It->GetPawn()))
		{
			continue;   // the spider stays in its web
		}
		if (APawn* P = It->GetPawn())
		{
			It->UnPossess();
			P->Destroy();
		}
		RestartPlayer(*It);
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
