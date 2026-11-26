#include "AntKitchenGameMode.h"
#include "HumanAIController.h"
#include "NpcRoutine.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "AntPlayerController.h"
#include "AntPlayerState.h"
#include "AntGameState.h"
#include "AntHUD.h"
#include "KitchenLoot.h"
#include "LevelV3.h"
#include "KitchenFixture.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "AntSwarm.h"
#include "AntBotController.h"
#include "CreatureCharacters.h"
#include "AntArmy.h"
#include "AntChain.h"
#include "WarMode.h"
#include "Critters.h"
#include "FolkCharacters.h"
#include "NpcBody.h"
#include "KitchenGadgets.h"
#include "KitchenArt.h"
#include "KitchenMover.h"
#include "AntRaid.h"
#include "KitchenSlick.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AntSnowball.h"
#include "AntRts.h"
#include "AntFighter.h"
#include "AntTutorial.h"
#include "AntLab.h"

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

void AAntKitchenGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// KAN-37: the level lab variant of this world (-AntLab=X or the menu), before anything asks where things are. The game state
	// carries it to the clients (AAntGameState::LabVariant).
	AntLab::Activate(AntLab::Wanted());
}

void AAntKitchenGameMode::BeginPlay()
{
	Super::BeginPlay();
	Rng.GenerateNewSeed();
	AKitchenBuilder::LootSpots(SpotLocations, SpotTiers);
	if (FParse::Param(FCommandLine::Get(), TEXT("AntArtProbe")))   // dev: log what lies under every loot spot (see KitchenArt::ProbeSpots)
	{
		FTimerHandle PH;
		GetWorldTimerManager().SetTimer(PH, FTimerDelegate::CreateWeakLambda(this, [this]() { KitchenArt::ProbeSpots(GetWorld(), SpotLocations); }), 5.f, false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AntArtTour")))
	{
		// Dev: puts the player's ant on every kind of surface of the modelled kitchen in turn and logs where it ended up
		// (ARTPOS lines): does it stand on the worktop, inside the fridge, in the drawer... or fall through?
		// (kept clear of the loot spots: an ant put inside a piece of food is pushed out of it, sometimes through a wall)
		static const FVector Tour[] =
		{
			FVector(-6300.f, 3300.f, 1740.f),   // 0 worktop A
			FVector(1500.f, 3300.f, 1740.f),    // 1 worktop B
			FVector(3400.f, 4350.f, 960.f),     // 2 fridge, shelf 1
			FVector(3800.f, 4000.f, 265.f),     // 3 fridge, in the crisper (pulled out 700 with -AntArtOpen: still over the tray)
			FVector(4300.f, 3900.f, 2590.f),    // 4 freezer
			FVector(3900.f, 3900.f, 30.f),      // 5 under the fridge
			FVector(-3700.f, 4000.f, 170.f),    // 6 under the sink
			FVector(-900.f, 3600.f, 330.f),     // 7 oven floor
			FVector(600.f, 3900.f, 190.f),      // 8 dishwasher floor
			FVector(1500.f, 4400.f, 1790.f),    // 9 microwave plate
			FVector(-4900.f, 3400.f, 1380.f),   // 10 top drawer (shut: inside the unit; open: 1100 further south)
			FVector(-4900.f, 2300.f, 1380.f),   // 11 top drawer pulled out (only with -AntArtOpen)
			FVector(-4900.f, 2300.f, 850.f),    // 12 second drawer pulled out (only with -AntArtOpen)
			FVector(-2150.f, 3900.f, 200.f),    // 13 inside the base cabinet left of the oven
			FVector(-5125.f, 4640.f, 3050.f),   // 14 inside a wall cabinet
			FVector(4200.f, 4300.f, 3730.f),    // 15 fridge top
			FVector(5700.f, -1300.f, 60.f),     // 16 pedal bin floor
			FVector(-3800.f, 3700.f, 1180.f),   // 17 sink bottom
			FVector(-6000.f, 4870.f, 30.f),     // 18 corridor behind the cabinets
			FVector(-700.f, 4870.f, 30.f),      // 19 corridor behind the oven
			FVector(-750.f, 3700.f, 1770.f),    // 20 stove top
			FVector(-200.f, 200.f, 1430.f),     // 21 table
			FVector(2000.f, 300.f, 45.f),       // 22 rug under the table
			FVector(200.f, -2600.f, 910.f),     // 23 chair seat
			FVector(-6780.f, -400.f, 2710.f),   // 24 wall shelf
			FVector(-5800.f, 4650.f, 4430.f),   // 25 top of the wall cabinets
			FVector(-5300.f, 800.f, 50.f),      // 26 inside the cereal box
			FVector(-300.f, -4910.f, 2230.f),   // 27 window sill
			FVector(-600.f, 4100.f, 945.f),     // 28 oven tray
			FVector(-750.f, 2500.f, 345.f),     // 29 open oven door (only with -AntArtOpen)
			FVector(650.f, 2300.f, 160.f),      // 30 open dishwasher door (only with -AntArtOpen)
			FVector(-6400.f, 4200.f, 2180.f),   // 31 toaster top, between the slots
			FVector(6550.f, -4250.f, 230.f),    // 32 inside the shoe
			FVector(-6300.f, 2600.f, 900.f),    // 33 on an open base cabinet door (only with -AntArtOpen): falls to the floor when shut
			FVector(3700.f, 3000.f, 1000.f),    // 34 by the fridge door (open: a door bin; shut: the floor in front)
			FVector(650.f, 2500.f, 470.f),      // 35 dishwasher lower rack rolled out (only with -AntArtOpen)
			FVector(6000.f, -1500.f, 1600.f),   // 36 on the bin lid (shut) / on the bag (open)
			FVector(-2800.f, -3850.f, 60.f),    // 37 pet mat
			FVector(6200.f, 3700.f, 680.f),     // 38 water in the mop bucket
			// ---- living room, yard, street
			FVector(13500.f, 300.f, 990.f),     // 39 sofa seat cushion
			FVector(13000.f, 700.f, 30.f),      // 40 under the sofa
			FVector(13750.f, -3000.f, 1010.f),  // 41 coffee table
			FVector(14000.f, -5500.f, 40.f),    // 42 carpet
			FVector(12500.f, -8300.f, 1130.f),  // 43 TV stand top
			FVector(15000.f, -8350.f, 130.f),   // 44 inside the TV stand (open bay)
			FVector(9250.f, -5500.f, 830.f),    // 45 armchair seat
			FVector(19650.f, -3000.f, 4690.f),  // 46 top of the bookshelf
			FVector(7100.f, -4000.f, 30.f),     // 47 the doorway
			FVector(-7350.f, 1000.f, 9000.f),   // 48 in the roof gutter
			FVector(-13550.f, 1000.f, -200.f),  // 49 under the car
			FVector(-10200.f, 200.f, 30.f),     // 50 pavement
			FVector(-13000.f, -2000.f, -200.f), // 51 road
			FVector(600.f, -6400.f, 50.f),      // 52 stepping stone in the yard
			FVector(4200.f, -13000.f, 2900.f),  // 53 on the bags in the yard bin
			FVector(-6600.f, -12500.f, 5300.f), // 54 the tree's branch
		};
		TSharedRef<int32> Step = MakeShared<int32>(-1);
		FTimerHandle TH;
		GetWorldTimerManager().SetTimer(TH, FTimerDelegate::CreateWeakLambda(this, [this, Step]()
		{
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			AAntCharacter* Ant = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
			if (!Ant)
			{
				return;
			}
			const int32 N = UE_ARRAY_COUNT(Tour);
			if (*Step >= 0 && *Step < N)
			{
				KitchenArt::ProbeAnt(Ant, *Step, Tour[*Step]);
			}
			if (++*Step < N)
			{
				Ant->ApplyTeleport(Tour[*Step]);
			}
		}), 2.f, true, 6.f);
	}
	// Mode: heist by default; -AntWar, -AntClassic switch.
	bWar = FParse::Param(FCommandLine::Get(), TEXT("AntWar"));
	bHeist = !bWar && !FParse::Param(FCommandLine::Get(), TEXT("AntClassic"));
	// The heist is an extraction raid now (the level v2 flat only: the main object lives there). -AntShifts: the old run of shifts.
	bRaid = bHeist && KitchenArt::Enabled() && !FParse::Param(FCommandLine::Get(), TEXT("AntShifts"));
	if (bWar)
	{
		TargetScore = 3000;
		MatchLength = 900.f;
		AllyBots = 11;    // + you = 12 per side, plus 128 army ants each
		RivalBots = 12;
	}
	else if (bHeist)
	{
		// The shift: 10 minutes, no score target; friends + a couple of bots, add more with F9/F10.
		TargetScore = 300;      // the quota; StartShift sets the real one
		MatchLength = 480.f;
		AllyBots = 1;
		RivalBots = bRaid ? 0 : 2;   // a raid: the red clan brings its own parties (AntClan.cpp)
	}

	if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->bWarMode = bWar;
		GS->bHeistMode = bHeist;
		GS->WarPool0 = 128;
		GS->WarPool1 = 128;
		GS->TargetScore = TargetScore;
		GS->TimeRemaining = MatchLength;
		GS->Phase = 0;
	}

	SpawnHuman();
	const bool bLab = AntLab::Get() != nullptr;
	if (!bLab)   // KAN-37 lab: a grey box with the Human only (no gadgets, residents, pets, bottles, background swarm)
	{
		SpawnGadgets();
		AKitchenSpillBottle::SpawnAll(GetWorld());   // level v2: the oil and the water bottle ants can tip over
	}
	RunBeginPlay();
	if (!bLab)
	{
		SpawnSwarm();
	}
	else
	{
		AntLab::SpawnHatches(GetWorld());
		AntLab::LogCheck();   // the automatic check, in every lab log
	}
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
		if (AntLab::Get())
		{
			return;   // the lab: no fire ants, spider or flies
		}
		AddBots(1, RivalBots);
		SpawnSpider();
		SpawnFly();
		SpawnFly();
	}), 1.5f, false);

	if (bWar)
	{
		SpawnWarZones();
		NextVault = GetWorld()->GetTimeSeconds() + 45.f;
		GetWorldTimerManager().SetTimer(AntWarTimer, this, &AAntKitchenGameMode::WarTick, 1.f, true, 3.f);
		Broadcast(TEXT("МУРАВЬИНАЯ ВОЙНА: держите территории, тащите сейф-сахарницу на кассу. P — сменить сторону (в муравейнике)"), Gold);
	}
	else
	{
		GetWorldTimerManager().SetTimer(WarTimer, this, &AAntKitchenGameMode::RivalWarTick, 50.f, true, 70.f);
	}
	if (bHeist)
	{
		for (int32 i = 0; i < 3; ++i) { SpawnTrophy(i); }
		GetWorldTimerManager().SetTimer(HeistTimer, this, &AAntKitchenGameMode::HeistTick, 5.f, true, 10.f);
		if (bRaid)
		{
			// The colony is loaded and everybody starts in the nest; the raid (and its cake) begins when the host says so.
			FActorSpawnParameters RP;
			RP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Raid = GetWorld()->SpawnActor<AAntRaid>(AAntRaid::StaticClass(), FTransform::Identity, RP);
			if (Raid) { Raid->ServerInit(this); }
			if (AntTut::Wanted() != AntTut::Off)
			{
				// «Учебка» / «Песочница» (AntTutorial.h): a director that owns this raid world.
				if (AAntTutorial* Tut = GetWorld()->SpawnActor<AAntTutorial>(AAntTutorial::StaticClass(), FTransform::Identity, RP))
				{
					Tut->Mode = AntTut::Wanted();
					Tut->StartBeat = AntTut::WantedBeat();
				}
			}
		}
		else
		{
			StartShift();
			SpawnMainObject();
			Broadcast(TEXT("ХАЙСТ: трофеи лежат высоко — снимите и дотащите домой ЦЕЛЫМИ (×2). Разбитый — куски за обычную цену"), Gold);
		}
	}
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
	const bool bArt = KitchenArt::Enabled();   // the modelled kitchen: a few gadgets stand where its pieces are
	Door(EKitchenDoorKind::Drawer, FVector(bArt ? -4900.f : -4950.f, 3000.f, 1330.f));   // the modelled drawer unit is centred 50 further east
	Door(EKitchenDoorKind::Dishwasher, FVector(650.f, 3000.f, 120.f));
	Door(EKitchenDoorKind::Microwave, FVector(1300.f, 4000.f, 1740.f));
	Spawn(AKitchenKnob::StaticClass(), bArt ? FVector(-920.f, 2962.f, 1612.f) : FVector(-920.f, 2945.f, 1600.f), FRotator::ZeroRotator, NoSetup);        // stove knob + the pot of soup
	Spawn(AKitchenPedalBin::StaticClass(), FVector(6000.f, -1500.f, 0.f), FRotator::ZeroRotator, NoSetup);     // the bin's lid and pedal
	Spawn(AKitchenFan::StaticClass(), FVector(500.f, 3400.f, 1710.f), FRotator::ZeroRotator, NoSetup);         // desk fan on the counter

	Faucet = Cast<AKitchenFaucet>(Spawn(AKitchenFaucet::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, NoSetup));
	Spawn(AKitchenSwitch::StaticClass(), FVector(-6995.f, 1900.f, 2300.f), FRotator::ZeroRotator, NoSetup);
	Spawn(AKitchenToaster::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, NoSetup);
	Spawn(AKitchenWire::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, NoSetup);
	Spawn(AKitchenTrap::StaticClass(), FVector(-3150.f, 3450.f, 140.f), FRotator(0.f, 90.f, 0.f), NoSetup);
	if (bArt)
	{
		// Every other moving part of the modelled kitchen: cabinet doors, drawers, the freezer door, the crisper,
		// the dishwasher racks, the matchbox tray. One generic actor each, standing at the part's own origin.
		const TConstArrayView<FKitchenArtMover> Movers = KitchenArt::Movers();
		for (int32 i = 0; i < Movers.Num(); ++i)
		{
			const FKitchenArtMover& M = Movers[i];
			Spawn(AKitchenMover::StaticClass(), FVector(KitchenArt::Instances()[M.Inst].Loc), FRotator::ZeroRotator, [i, &M](AActor* A)
			{
				AKitchenMover* Mv = CastChecked<AKitchenMover>(A);
				Mv->PartIndex = i;
				Mv->bOpen = (M.Flags & KitchenArt::StartsOpen) != 0;
			});
		}
	}

	// Secret routes (each end leads to the other). The air ducts are real now: crawl in through the grilles (KitchenHouse.cpp).
	Passage(EPassageLook::Drain, FVector(-3500.f, 4000.f, 1150.f), 0.f, FVector(-3500.f, 3600.f, 200.f), TEXT("слив раковины → под мойку"));
	Passage(EPassageLook::Pipe, FVector(-3300.f, 4300.f, 140.f), 0.f, FVector(-3700.f, 3800.f, 1210.f), TEXT("труба → в раковину"));
	// The pets and the rest of the family (primitives for now).
	Spawn(ACritter::StaticClass(), FVector(600.f, -600.f, 0.f), FRotator::ZeroRotator, [](AActor* A) { CastChecked<ACritter>(A)->Kind = 0; });
	Spawn(ACritter::StaticClass(), FVector(bArt ? 15500.f : 13500.f, 400.f, 950.f), FRotator(0.f, -90.f, 0.f), [](AActor* A) { CastChecked<ACritter>(A)->Kind = 1; });   // level v2: the east cushion (not inside the kid)
	// The kid and grandma: the jointed figures with heads of their own (FolkCharacters.h), or the old seated ones (-AntOldNpc / no models).
	if (NpcWorld::NewNpcs() && HumanBody::Enabled(HumanFig::Kid))
	{
		Spawn(AKidCharacter::StaticClass(), FVector(13800.f, -1200.f, 800.f), FRotator(0.f, -90.f, 0.f), [](AActor*) {});
	}
	else
	{
		Spawn(AHouseholdNpc::StaticClass(), FVector(13500.f, 300.f, 950.f), FRotator::ZeroRotator, [](AActor* A) { CastChecked<AHouseholdNpc>(A)->Kind = 0; });
	}
	if (NpcWorld::NewNpcs() && HumanBody::Enabled(HumanFig::Grandma))
	{
		Spawn(AGrandmaCharacter::StaticClass(), FVector(9910.f, -5800.f, 1480.f), FRotator::ZeroRotator, [](AActor*) {});
	}
	else
	{
		Spawn(AHouseholdNpc::StaticClass(), FVector(9250.f, -5800.f, 800.f), FRotator::ZeroRotator, [](AActor* A) { CastChecked<AHouseholdNpc>(A)->Kind = 1; });
	}
	// Grandma's ball of yarn on the floor with the thread running up to her lap: a climb.
	// (geometry lives in KitchenBuilder so every client has it; the stash under the loose tile is loot.)
	SpawnStash(FVector(5000.f, 2000.f, KitchenArt::Enabled() ? -265.f : -350.f), 90, 16, 3);   // level v2: the pit is 300 deep, the cargo lies on its floor (it sat inside the floor slab)

	// Fly food: dog poop on the lawn, the outdoor bin, the kitchen trash.
	auto Feeder = [&Spawn](uint8 Kind, const FVector& Loc)
	{
		Spawn(AFlyFeeder::StaticClass(), Loc, FRotator::ZeroRotator, [Kind](AActor* A) { CastChecked<AFlyFeeder>(A)->Kind = Kind; });
	};
	Feeder(0, AKitchenBuilder::YardPoop());
	Feeder(1, AKitchenBuilder::YardBin() + FVector(0.f, 0.f, 2650.f));
	Feeder(1, FVector(6000.f, -1500.f, 1600.f));

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
	if (bHeist)
	{
		// «Натоптанная тропа»: the trail workers bring half as much again. Greed counts for them too.
		Value = GreedValue(HasCard(AntCard::Trail) ? FMath::RoundToInt(Value * 1.5f) : Value);
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
	if (bRaid)
	{
		return;   // a raid has no quota: what counts is getting home (AntRaid.cpp)
	}
	if (bHeist)
	{
		// The shift: hitting the quota does not end it. Everyone still has to make it home (ShiftTick), or stay for more.
		if (!GS->bQuotaMet && GS->TeamScore >= GS->TargetScore)
		{
			GS->bQuotaMet = true;
			Broadcast(TEXT("КВОТА ЕСТЬ! Все в гнездо — и смена ваша. Кто остаётся снаружи, тому всё дороже: жадность растёт."), Gold);
		}
		return;
	}
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

void AAntKitchenGameMode::RivalWarTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0 || GS->RivalWorkers < 400 || bRaid)
	{
		return;   // (a raid: the rival clan plays the raid with its own parties, AntClan.cpp; no army of hundreds)
	}
	for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
	{
		if (It->Team == 1) { return; }   // one raid at a time
	}
	// The fire ants send an army: raid our nest, or hunt our army / our ants.
	const int32 N = FMath::Min(GS->RivalWorkers / 2, 3500);
	GS->RivalWorkers -= N;
	FVector Target = AKitchenBuilder::NestLocation(0);
	for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
	{
		if (It->Team == 0 && FMath::RandBool()) { Target = It->GetActorLocation(); break; }
	}
	const FVector Start = AKitchenBuilder::NestLocation(1) + FVector(0.f, 0.f, 10.f);
	AAntArmy* A = GetWorld()->SpawnActorDeferred<AAntArmy>(AAntArmy::StaticClass(), FTransform(Start), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (A)
	{
		A->Team = 1;
		A->Count = N;
		A->Mode = EArmyMode::Attack;
		A->Target = Target;
		A->FinishSpawning(FTransform(Start));
		Broadcast(FString::Printf(TEXT("ОГНЕННЫЕ ВЫСТУПИЛИ: армия из %d муравьёв идёт на нас! (F — своя армия)"), N), Bad);
	}
}

void AAntKitchenGameMode::SpawnFly()
{
	if (FlyCount >= 4)
	{
		return;
	}
	const FVector L = AKitchenBuilder::RandomFloorPoint() + FVector(0.f, 0.f, 2500.f);
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
	if (Spider || FParse::Param(FCommandLine::Get(), TEXT("AntNoSpider")))   // -AntNoSpider: dev runs that time a haul without her
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
	if (!GS || GS->Phase != 0 || AntLab::Get())
	{
		return;
	}
	const int32 Roll = Rng.RandRange(0, 4);
	if (Roll == 2 && AKitchenMover::HumanOpensOne(GetWorld(), Rng, 20.f))
	{
		Broadcast(TEXT("Человек полез в шкаф и не закрыл дверцу — внутри открыто!"), Info);
		return;
	}
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
	if (Raid) { Raid->OnPlayerJoined(NewPlayer); }

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
	if (AntLab::Get() && Team == 0)
	{
		RestartPlayerAtTransform(NewPlayer, FTransform(FRotator(0.f, FMath::FRandRange(-180.f, 180.f), 0.f), AntLab::SpawnPoint()));   // KAN-37: in front of the lab nest
		return;
	}
	const FVector Nest = AKitchenBuilder::NestLocation(Team);
	// Inside the safe zone of the nest; both nests open towards the middle of the kitchen.
	const float Out = Team == 0 ? 1.f : -1.f;
	// Level v2: the fire ants come out of the red anthill in the yard (a solid mound 427 high): they appear on top of it, in the crater.
	const FVector Loc = Nest + FVector(Out * FMath::FRandRange(180.f, 380.f), FMath::FRandRange(-280.f, 280.f), (Team == 1 && KitchenArt::Enabled()) ? 520.f : 60.f);
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
	if (AntRts::RespawnInSquad(C))
	{
		return;   // the snowball raid: he is back as one of his squad ants near where he fell (KAN-32)
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
	SpawnContainerLoot();
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
	if (Team == 0 && !Food->bBody)
	{
		AAntCharacter* By = nullptr;
		for (AAntCharacter* A : Carriers) { if (A && (!By || A->IsPlayerControlled())) { By = A; } }
		AntTut::Note(this, AntTut::EvDeliver, By, Food);
	}
	if (Food->bBody)
	{
		// A dead teammate brought home: the colony gets a larva back (or the player himself, if he was out for good).
		if (Team != 0 || !bHeist)
		{
			return;
		}
		AController* BodyOwner = Food->BodyOf.Get();
		const FString Name = Food->LootName.Replace(TEXT("Тело: "), TEXT(""));
		if (bRaid && Raid) { Raid->OnBodyHome(BodyOwner, Name, Carriers); }
		AntFighter::OnBodyHome(BodyOwner, Carriers);   // rescues (AntFighter.h)
		for (AAntCharacter* A : Carriers)
		{
			if (AAntPlayerState* PS = A ? A->GetPlayerState<AAntPlayerState>() : nullptr) { PS->BodiesHome++; }
		}
		if (BodyOwner && Run.OutPlayers.Contains(BodyOwner))
		{
			Run.OutPlayers.Remove(BodyOwner);
			Broadcast(FString::Printf(TEXT("Тело дома: %s снова в строю!"), *Name), Good);
			RespawnAnt(BodyOwner);
		}
		else if (GS->Larvae < GS->LarvaeStart)
		{
			GS->Larvae++;
			Broadcast(FString::Printf(TEXT("Тело дома (%s): колония вернула личинку. Теперь их %d"), *Name, GS->Larvae), Good);
		}
		else
		{
			Broadcast(FString::Printf(TEXT("Тело дома (%s). Личинок и так полный запас"), *Name), Info);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTBODY delivered: %s, larvae now %d of %d"), *Name, GS->Larvae, GS->LarvaeStart);
		CompleteOrder(AntOrder::Body);
		return;
	}
	int32 Value = Food->Value;
	if (Team == 0 && bHeist)
	{
		// The Queen's orders that are about what was brought and from where.
		if (Food->bTrophy) { CompleteOrder(AntOrder::TrophyWhole); }
		if (Food->Origin == KitchenContainers::Freezer + 1) { CompleteOrder(AntOrder::Freezer); }
		if (Food->Origin == KitchenContainers::Fridge + 1) { CompleteOrder(AntOrder::Fridge); }
		if (Food->bFromLiving && ++Run.LivingDelivered >= 3) { CompleteOrder(AntOrder::Living3); }
	}
	if (Food->bTrophy)
	{
		Value *= 2;
		GS->TrophiesWhole++;
		AntSfx::Everyone(this, AntSfx::Legendary, Food->GetActorLocation(), 1.f);
		if (Food->bMain)
		{
			// Level v2: the main object of the shift is home in one piece.
			Broadcast(Team == 0 ? FString::Printf(TEXT("ИМЕНИННЫЙ ТОРТ ДОМА! Целый кусок: +%d. Королева в восторге"), Value)
				: FString::Printf(TEXT("Огненные утащили именинный торт в свой холм: +%d им"), Value), Team == 0 ? Gold : Bad);
			UE_LOG(LogTemp, Display, TEXT("ANTV2 cake delivered whole by team %d: %d"), (int32)Team, Value);
		}
		else
		{
			TrophyLostAt = GetWorld()->GetTimeSeconds();
			Broadcast(FString::Printf(TEXT("ЦЕЛЫЙ ТРОФЕЙ! «%s» доставлен без единой трещины: ×2 = %d"), *Food->LootName, Value), Gold);
		}
	}
	if (bRaid && Raid)
	{
		Raid->OnDelivered(Team, Food, Carriers, Value);   // the tier's price, the cake, what the Queen thinks of it
		if (Team == 0) { AntSnowball::OnDelivered(this, Food, Carriers, Value); }   // the in-raid snowball's mass (AntSnowball.h)
		if (Team == 0) { AntFighter::OnDelivered(Food, Carriers, Value); }   // each carrier's share, the cake (AntFighter.h)
	}
	if (Team == 0)
	{
		Value = GreedValue(Value);   // after the quota: the longer somebody stays out, the more everything is worth
	}
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
	else
	{
		GS->RivalWorkers += Value * 4;
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
	AntFighter::OnAntKilled(Ant, Killer, Causer);   // kills, deaths, a veteran to the wall (AntFighter.h)
	AntTut::Note(this, AntTut::EvKill, Killer ? Killer->GetPawn() : nullptr, Ant);
	if (AntRts::OnSquadBotKilled(Ant))
	{
		return;   // a squad ant of the snowball raid: gone for good, no respawn, no kill feed line (AntRts.cpp)
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
		How = H->Fig == HumanFig::Grandma ? TEXT("прихлопнут бабушкиной мухобойкой") : H->Fig == HumanFig::Kid ? TEXT("растоптан малышом") : H->bSpraying ? TEXT("задохнулся дихлофосом") : TEXT("раздавлен тапком");
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

	// The shift: every death of a player's ant costs the colony a larva. None left = nobody hatches.
	if (bHeist && GS && Ant->Team == 0 && Ant->Creature == 0 && Cast<APlayerController>(Ant->GetController()))
	{
		++Run.DeathsThisShift;
		FailOrder(AntOrder::NoDeaths);
		if (bRaid && Raid) { Raid->OnPlayerDied(Ant, How); }
		// The body stays behind for a minute (after the pancake has had its ride on the slipper): bring it home for a larva.
		{
			TWeakObjectPtr<AAntCharacter> WeakBody = Ant;
			FTimerHandle BodyHandle;
			GetWorldTimerManager().SetTimer(BodyHandle, FTimerDelegate::CreateWeakLambda(this, [this, WeakBody]()
			{
				SpawnBody(WeakBody.Get());
			}), FMath::Max(0.5f, RespawnDelay - 0.3f), false);
		}
		if (GS->Larvae <= 0)
		{
			Run.OutPlayers.Add(Ant->GetController());
			Ant->Toast(bRaid ? TEXT("Личинок не осталось: ты выбыл до конца рейда. Вернуть тебя могут, если принесут тело домой")
				: TEXT("Личинок не осталось: ты выбыл до конца смены. Вернуть тебя могут, если принесут тело домой"));
			if (!AnyAllyPlayerAlive(false))
			{
				EndMatch(2);
			}
			return;
		}
		GS->Larvae--;
		if (GS->Larvae == 0)
		{
			Broadcast(TEXT("ПОСЛЕДНЯЯ ЛИЧИНКА ВЫЛУПИЛАСЬ. Больше возрождений нет!"), Bad);
		}
	}
	TWeakObjectPtr<AController> WeakC = Ant->GetController();
	FTimerHandle Handle;
	// (a raid: the rival clan's dead come back as reinforcements, later the lower the threat)
	const float Delay = (bRaid && Raid && Ant->Team == 1 && Ant->Creature == 0 && !Cast<APlayerController>(Ant->GetController())) ? Raid->ClanRespawn() : RespawnDelay;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakC]()
	{
		RespawnAnt(WeakC.Get());
	}), Delay, false);
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
	if (Team == 0)
	{
		CompleteOrder(AntOrder::KnockOut);
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
	GS->RivalWorkers += 4;   // their queen keeps laying eggs
	if (bHeist)
	{
		if (bRaid && Raid) { Raid->RaidSecond(); } else { ShiftTick(); }
		return;
	}
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
	if (bRaid && Raid)
	{
		// (asked for by a dev command, or the last ant died with no larva left)
		Raid->EndRaid(Winner == 1 ? AntRaidEnd::Left : AntRaidEnd::Wiped);
		return;
	}
	if (bHeist)
	{
		// Orders decided at the end, the awards, the run totals (while the score still counts).
		RunEndShift(Winner == 1);
		GS->Phase = Winner;
		if (Winner == 1)
		{
			BestShift = FMath::Max(BestShift, ShiftNumber);
			Broadcast(FString::Printf(TEXT("СМЕНА %d ПРОЙДЕНА! Королева довольна. Дальше квота больше."), ShiftNumber), Gold);
			++ShiftNumber;
			// The interlude: the results and the awards, then the colony votes for a card, then the next shift.
			const float T = FParse::Param(FCommandLine::Get(), TEXT("AntFastVote")) ? 4.f : 10.f;
			GS->Interlude = 1;
			GS->InterludeEnds = GS->GetServerWorldTimeSeconds() + T;
			GetWorldTimerManager().SetTimer(RunTimer, this, &AAntKitchenGameMode::InterludeTick, T, false);
			return;
		}
		Broadcast(FString::Printf(TEXT("КОЛОНИЯ ГОЛОДАЕТ. Забег окончен: пройдено смен %d, добыча за забег %d."), GS->RunShifts, GS->RunHaul), Bad);
		ShiftNumber = 1;
		GetWorldTimerManager().SetTimer(ResetTimer, this, &AAntKitchenGameMode::ResetMatch, 14.f, false);
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
	if (LevelV3::Enabled())
	{
		AKitchenBuilder::LootSpots(SpotLocations, SpotTiers);   // level v3: the seed may lift some spots (loot_z)
		V3BreakRage = 0.f;
		V3Breaks = 0;
	}
	SpawnInitialLoot();
	SpawnV3Scatter();

	if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->TeamScore = 0;
		GS->RivalScore = 0;
		GS->RivalWorkers = 1500;
		GS->WarPool0 = 128;
		GS->WarPool1 = 128;
		GS->TrophiesWhole = 0;
		GS->TrophiesBroken = 0;
		if (bHeist) { for (int32 i = 0; i < 3; ++i) { SpawnTrophy(i); } SpawnMainObject(); }
		for (TActorIterator<AAntChain> It(GetWorld()); It; ++It) { It->Dissolve(); It->Destroy(); }
		for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It) { It->Destroy(); }
		for (AAntTerritory* T : Territories) { if (T) { T->ResetZone(); } }
		for (AAntCashout* C : Cashouts) { if (C) { C->ResetStation(); } }
		NextVault = GetWorld()->GetTimeSeconds() + 45.f;
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
	if (bHeist)
	{
		// The jars and mugs that were pushed off their edges go back up.
		for (TActorIterator<AKitchenProp> It(GetWorld()); It; ++It)
		{
			if (!It->PropName.IsEmpty()) { It->Destroy(); }
		}
		SpawnHighProps();
		if (bRaid && Raid) { Raid->OnWorldReset(); } else { StartShift(); }
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
	if (!bRaid)
	{
		Broadcast(bWar ? TEXT("Новый раунд войны! Захватывайте территории!") : bHeist ? TEXT("Новая смена! Трофеи снова на местах — тащите целыми") : TEXT("Новый раунд! Тащите всё сладкое в муравейник!"), Gold);
	}
}

// ---------------------------------------------------------------------------------------------
// "Муравьиная война": territories with supply lines, cashouts for the sugar-bowl safe,
// commanders leading army columns out of the nests, switching sides.
// ---------------------------------------------------------------------------------------------

bool AAntKitchenGameMode::IsWar() const
{
	const AAntGameState* GS = GetGameState<AAntGameState>();
	return GS && GS->bWarMode;
}

void AAntKitchenGameMode::AddPool(uint8 Team, int32 N)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || Team > 1)
	{
		return;
	}
	int32& Pool = Team == 0 ? GS->WarPool0 : GS->WarPool1;
	Pool = FMath::Clamp(Pool + N, 0, 400);
}

void AAntKitchenGameMode::ReturnWorkers(uint8 Team, int32 N, AAntCharacter* Builder)
{
	if (N <= 0 || Team > 1)
	{
		return;
	}
	if (AAntArmy* A = AAntArmy::FindFor(Builder))
	{
		A->Count += N;
		return;
	}
	if (IsWar())
	{
		AddPool(Team, N);
	}
	else if (Team == 0)
	{
		if (AAntSwarm* S = AAntSwarm::Get(GetWorld())) { S->Idle += N; }
	}
	else if (AAntGameState* GS = GetGameState<AAntGameState>())
	{
		GS->RivalWorkers += N;
	}
}

void AAntKitchenGameMode::SwitchTeam(AAntPlayerController* PC)
{
	if (!PC || PC == HumanPlayer)
	{
		return;
	}
	AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>();
	AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
	if (!PS || (Ant && !Ant->CanSwitchTeam()))
	{
		if (Ant) { Ant->Toast(TEXT("Сменить сторону можно только в своём муравейнике (или пока мёртв)")); }
		return;
	}
	if (AAntArmy* A = AAntArmy::FindFor(Ant))
	{
		A->Disband();
	}
	PS->Team = PS->Team == 0 ? 1 : 0;
	PS->AntColor = AntKit::TeamColor(PS->Team, ColorIndex++);
	if (Ant)
	{
		PC->UnPossess();
		Ant->Destroy();
	}
	RestartPlayer(PC);
	Broadcast(FString::Printf(TEXT("%s теперь воюет за %s"), *PS->GetPlayerName(), PS->Team == 0 ? TEXT("колонию (чёрные)") : TEXT("огненных муравьёв (красные)")),
		PS->Team == 0 ? Good : Bad);
}

void AAntKitchenGameMode::SpawnWarZones()
{
	struct FZone { const TCHAR* Name; FVector Loc; float R; };
	static const FZone Zones[] =
	{
		{ TEXT("Столешница"), FVector(-4000.f, 4200.f, 1710.f), 900.f },
		{ TEXT("Двор"), FVector(-2500.f, -9000.f, 0.f), 1500.f },
		{ TEXT("Стол"), FVector(500.f, -500.f, 1400.f), 800.f },
		{ TEXT("Ковёр"), FVector(500.f, -2000.f, 0.f), 1000.f },
		{ TEXT("Холодильник"), FVector(4000.f, 2700.f, 0.f), 900.f },
		{ TEXT("Диван"), FVector(13500.f, -4000.f, 0.f), 1300.f },
	};
	for (const FZone& Z : Zones)
	{
		AAntTerritory* T = GetWorld()->SpawnActorDeferred<AAntTerritory>(AAntTerritory::StaticClass(), FTransform(Z.Loc), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (T)
		{
			T->ZoneName = Z.Name;
			T->Radius = Z.R;
			T->FinishSpawning(FTransform(Z.Loc));
			Territories.Add(T);
		}
	}
	static const FZone Stations[] =
	{
		{ TEXT("Касса у плиты"), FVector(-2500.f, -500.f, 0.f), 0.f },
		{ TEXT("Касса под раковиной"), FVector(3000.f, -3500.f, 0.f), 0.f },
	};
	for (const FZone& Z : Stations)
	{
		AAntCashout* C = GetWorld()->SpawnActorDeferred<AAntCashout>(AAntCashout::StaticClass(), FTransform(Z.Loc), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (C)
		{
			C->ZoneName = Z.Name;
			C->FinishSpawning(FTransform(Z.Loc));
			Cashouts.Add(C);
		}
	}
}

void AAntKitchenGameMode::SpawnVault()
{
	static const FVector Spots[] =
	{
		FVector(800.f, 4300.f, 1760.f),     // counter, by the sugar jar
		FVector(500.f, -500.f, 1460.f),     // dining table
		FVector(-6500.f, -9500.f, 60.f),    // the lawn
		FVector(15000.f, -2000.f, 60.f),    // living room
		FVector(4000.f, -1500.f, 60.f),     // middle of the kitchen
	};
	const FVector Loc = Spots[Rng.RandRange(0, UE_ARRAY_COUNT(Spots) - 1)];
	FAntFoodDef D;
	D.Name = TEXT("Сейф-сахарница");
	D.Value = AAntCashout::Reward;
	D.Weight = 36;
	D.Color = FLinearColor(0.95f, 0.85f, 0.35f);
	D.Shape = EAntShape::Cylinder;
	D.Scale = FVector(0.9f, 0.9f, 0.7f);
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Loc), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitFood(D);
		L->bVault = true;
		L->FinishSpawning(FTransform(Loc));
		Broadcast(TEXT("СЕЙФ-САХАРНИЦА появилась! Тащите её на кассу (нужно 3 муравья)"), Gold);
		AntSfx::Everyone(this, AntSfx::Legendary, Loc, 1.f);
	}
}

void AAntKitchenGameMode::AssignWarRoles(uint8 Team)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	TArray<AAntBotController*> Bots;
	for (TActorIterator<AAntBotController> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = Cast<AAntCharacter>(It->GetPawn());
		if (A && !A->bDead && A->Creature == 0 && A->Team == Team)
		{
			Bots.Add(*It);
		}
	}
	if (Bots.Num() == 0 || !GS)
	{
		return;
	}
	Bots.Sort([](const AAntBotController& A, const AAntBotController& B) { return A.GetUniqueID() < B.GetUniqueID(); });
	const int32 NumCmd = FMath::Clamp(Bots.Num() / 4, 1, 3);

	// What is worth fighting for, most urgent first.
	struct FObj { FVector P; AActor* A; float Score; };
	TArray<FObj> Objs;
	const FVector Nest = AKitchenBuilder::NestLocation(Team);
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (It->bVault && !(It->IsCarried() && It->CarryTeam() == Team))
		{
			Objs.Add({ It->GetActorLocation(), *It, 0.f });
		}
		else if (It->bVault)
		{
			// Ours is on the move: escort it.
			Objs.Add({ It->GetActorLocation(), *It, 0.5f });
		}
	}
	for (AAntCashout* C : Cashouts)
	{
		if (C && C->bActive)
		{
			Objs.Add({ C->GetActorLocation(), C, C->CashTeam == Team ? 2.f : 1.f });
		}
	}
	for (AAntTerritory* T : Territories)
	{
		if (!T)
		{
			continue;
		}
		const float Far = (float)FVector::Dist(T->GetActorLocation(), Nest) / 10000.f;
		if (T->OwnerTeam != Team)
		{
			Objs.Add({ T->GetActorLocation(), T, 3.f + Far });
		}
		else if (T->bContested || T->bLineCut)
		{
			Objs.Add({ T->GetActorLocation(), T, 2.5f + Far });
		}
	}
	if (Objs.Num() == 0)
	{
		for (AAntTerritory* T : Territories)
		{
			if (T) { Objs.Add({ T->GetActorLocation(), T, 5.f }); }
		}
	}
	Objs.Sort([](const FObj& A, const FObj& B) { return A.Score < B.Score; });

	for (int32 i = 0; i < Bots.Num(); ++i)
	{
		AAntBotController* B = Bots[i];
		AAntCharacter* Me = Cast<AAntCharacter>(B->GetPawn());
		if (i < NumCmd)
		{
			const FObj& O = Objs.Num() > 0 ? Objs[FMath::Min(i, Objs.Num() - 1)] : FObj{ Nest, nullptr, 0.f };
			B->SetWarRole(true, nullptr);
			B->SetWarObjective(O.P, O.A);
			// A commander leads an army column out of the nest.
			if (!AAntArmy::FindFor(Me) && GS->WarPool(Team) >= 12 && Me->IsInNest())
			{
				const int32 N = FMath::Min(32, GS->WarPool(Team));
				AddPool(Team, -N);
				const FVector Start = Nest + FVector(0.f, 0.f, 10.f);
				if (AAntArmy* A = GetWorld()->SpawnActorDeferred<AAntArmy>(AAntArmy::StaticClass(), FTransform(Start), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					A->Team = Team;
					A->Count = N;
					A->Commander = Me;
					A->Mode = EArmyMode::Follow;
					A->FinishSpawning(FTransform(Start));
				}
			}
		}
		else
		{
			// Squads: the rest follow the commanders, round robin.
			AAntBotController* Lead = Bots[(i - NumCmd) % NumCmd];
			B->SetWarRole(false, Cast<AAntCharacter>(Lead->GetPawn()));
		}
	}
}

void AAntKitchenGameMode::WarTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || !GS->bWarMode || GS->Phase != 0)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Eggs hatch: a trickle of reinforcements for both sides.
	if (FMath::FloorToInt(Now) % 5 == 0)
	{
		AddPool(0, 1);
		AddPool(1, 1);
	}
	// The safe: one at a time, a new one some time after the last was cashed.
	bool bVault = false;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (It->bVault) { bVault = true; break; }
	}
	for (AAntCashout* C : Cashouts)
	{
		if (C && C->bActive) { bVault = true; }
	}
	if (bVault)
	{
		NextVault = Now + 150.f;
	}
	else if (Now > NextVault)
	{
		SpawnVault();
		NextVault = Now + 150.f;
	}
	// Commanders re-think every few seconds.
	if (FMath::FloorToInt(Now) % 4 == 0)
	{
		AssignWarRoles(0);
		AssignWarRoles(1);
	}
}

// ---------------------------------------------------------------------------------------------
// «Хайст»: three big prizes up high; delivered whole they pay double. A lost one comes back after a while.
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::SpawnTrophy(int32 Slot)
{
	if (AntLab::Get())
	{
		return;   // KAN-37 lab: one cake and the variant's food, nothing on the kitchen's trophy spots
	}
	struct FTrophy { const TCHAR* Name; int32 Value; int32 Weight; FLinearColor Color; EAntShape Shape; FVector Scale; FVector Where; };
	static const FTrophy Defs[] =
	{
		{ TEXT("Кусок торта"), 160, 38, FLinearColor(1.f, 0.55f, 0.70f), EAntShape::Cube, FVector(0.55f, 0.55f, 0.45f), FVector(200.f, -600.f, 1480.f) },      // on the dining table
		{ TEXT("Золотой мармеладный мишка"), 260, 48, FLinearColor(1.f, 0.75f, 0.10f), EAntShape::Sphere, FVector(0.6f), FVector(3600.f, 3700.f, 3780.f) }, // on top of the fridge
		{ TEXT("Шоколадный батончик"), 200, 34, FLinearColor(0.25f, 0.12f, 0.05f), EAntShape::Cube, FVector(0.8f, 0.35f, 0.18f), FVector(1850.f, 4450.f, 2420.f) },   // on the microwave
		{ TEXT("Котлета из микроволновки"), 220, 44, FLinearColor(0.55f, 0.35f, 0.2f), EAntShape::Cylinder, FVector(0.5f, 0.5f, 0.25f), FVector(1760.f, 4425.f, 1950.f) },   // inside the microwave
		{ TEXT("Полбутерброда"), 240, 52, FLinearColor(0.92f, 0.78f, 0.55f), EAntShape::Cube, FVector(0.7f, 0.5f, 0.3f), FVector(5900.f, -1500.f, 1060.f) },   // in the pedal bin
		{ TEXT("Печенье с противня"), 150, 30, FLinearColor(0.75f, 0.5f, 0.25f), EAntShape::Cylinder, FVector(0.6f, 0.6f, 0.15f), FVector(-750.f, 3900.f, 980.f) },   // on the oven tray
		{ TEXT("Кубик сахара с чаем"), 120, 26, FLinearColor(1.f, 1.f, 1.f), EAntShape::Cube, FVector(0.4f), FVector(19700.f, -4500.f, 4740.f) },   // level v2: on top of the bookshelf, under the garland (it lay on the wall shelf over the nest, 13 s from home)
		// Behind doors (the modelled kitchen): somebody has to get the door open first.
		{ TEXT("Кусок сыра с полки"), 230, 40, FLinearColor(1.f, 0.82f, 0.25f), EAntShape::Cube, FVector(0.6f, 0.45f, 0.35f), FVector(3400.f, 4300.f, 1010.f) },      // fridge shelf
		{ TEXT("Брикет пломбира"), 280, 46, FLinearColor(0.97f, 0.95f, 0.88f), EAntShape::Cube, FVector(0.7f, 0.45f, 0.3f), FVector(3900.f, 3800.f, 2640.f) },          // freezer
		{ TEXT("Банка сгущёнки"), 240, 42, FLinearColor(0.3f, 0.5f, 0.85f), EAntShape::Cylinder, FVector(0.45f, 0.45f, 0.5f), FVector(-5125.f, 4640.f, 3100.f) },       // wall cabinet
		{ TEXT("Пачка печенья"), 190, 34, FLinearColor(0.8f, 0.55f, 0.25f), EAntShape::Cube, FVector(0.7f, 0.4f, 0.3f), FVector(-2150.f, 4000.f, 250.f) },              // base cabinet left of the oven
		{ TEXT("Забытая шоколадка"), 210, 36, FLinearColor(0.3f, 0.15f, 0.07f), EAntShape::Cube, FVector(0.8f, 0.4f, 0.15f), FVector(13200.f, -8400.f, 180.f) },        // TV stand, behind its door (level v2: the stand stands 600 further west)
		{ TEXT("Плитка шоколада из шкафа"), 200, 36, FLinearColor(0.28f, 0.14f, 0.06f), EAntShape::Cube, FVector(0.8f, 0.4f, 0.15f), FVector(1900.f, 4000.f, 250.f) }, // base cabinet under the microwave
	};
	constexpr int32 NumDefs = UE_ARRAY_COUNT(Defs);
	constexpr int32 FirstBehindDoors = 7;
	const bool bArt = KitchenArt::Enabled();
	// Pick a free spot (no trophy within 400 of it). Two of the three trophies lie inside something that has to be opened.
	int32 Inside = 0;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (It->bTrophy && KitchenContainers::At(It->GetActorLocation()) >= 0) { ++Inside; }
	}
	TArray<int32> Free, FreeInside;
	for (int32 i = 0; i < (bArt ? NumDefs : FirstBehindDoors); ++i)
	{
		bool bTaken = false;
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			if (It->bTrophy && FVector::Dist(It->GetActorLocation(), Defs[i].Where) < 400.f) { bTaken = true; break; }
		}
		if (!bTaken)
		{
			(KitchenContainers::At(Defs[i].Where) >= 0 ? FreeInside : Free).Add(i);
		}
	}
	if (bArt && Inside < 2 && FreeInside.Num() > 0)
	{
		Free = FreeInside;
	}
	else if (Free.Num() == 0)
	{
		Free = FreeInside;
	}
	if (Free.Num() == 0)
	{
		return;
	}
	const FTrophy& T = Defs[Free[Rng.RandRange(0, Free.Num() - 1)]];
	FAntFoodDef D;
	D.Name = T.Name;
	D.Value = T.Value;
	D.Weight = T.Weight;
	D.Color = T.Color;
	D.Shape = T.Shape;
	D.Scale = T.Scale;
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(T.Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitFood(D);
		L->bTrophy = true;
		L->FinishSpawning(FTransform(T.Where));
		const int32 In = KitchenContainers::At(T.Where);
		UE_LOG(LogTemp, Display, TEXT("ANTLOOT trophy «%s» %s"), T.Name, In >= 0 ? KitchenContainers::Get(In).Where : TEXT("на виду"));
	}
}

void AAntKitchenGameMode::HeistTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || !bHeist || GS->Phase != 0)
	{
		return;
	}
	int32 Count = 0;
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		if (It->bTrophy && !It->bMain) { ++Count; }
	}
	// A new prize appears a while after one was delivered or smashed.
	if (Count < 3 && GetWorld()->GetTimeSeconds() - TrophyLostAt > 40.f)
	{
		SpawnTrophy(Count);
		TrophyLostAt = GetWorld()->GetTimeSeconds();
		Broadcast(TEXT("Новый трофей появился на кухне"), Info);
	}
}

// ---------------------------------------------------------------------------------------------
// The run (heist): quota, shared lives, the Human escalating
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::SetHumanAwake(bool bAwake)
{
	if (!Human)
	{
		return;
	}
	AController* C = Human->GetController();
	if (AHumanAIController* AI = Cast<AHumanAIController>(C); AI && C != HumanPlayer && NpcRoutine::Enabled())
	{
		// Level v3: asleep to the ants, awake to his evening (NpcRoutine.h): the routine runs from the first second.
		AI->bQuiet = !bAwake;
		C->SetActorTickEnabled(true);
		if (!bAwake) { Human->SetSpraying(false); }
		return;
	}
	if (C && C != HumanPlayer)
	{
		C->SetActorTickEnabled(bAwake);
		if (!bAwake)
		{
			C->StopMovement();
			Human->SetSpraying(false);
		}
	}
}

bool AAntKitchenGameMode::AnyAllyPlayerAlive(bool bMustBeOutside) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AAntCharacter* A = It->IsValid() ? Cast<AAntCharacter>((*It)->GetPawn()) : nullptr;
		if (A && !A->bDead && A->Team == 0 && (!bMustBeOutside || !A->IsInNest()))
		{
			return true;
		}
	}
	return false;
}

void AAntKitchenGameMode::StartShift()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	const int32 Players = FMath::Max(1, GetNumPlayers());
	// More players: more mouths. Every shift survived: the queen wants 30% more. -AntQuota=N overrides for tuning.
	int32 Quota = FMath::RoundToInt((300.f + 120.f * (Players - 1)) * FMath::Pow(1.3f, (float)(ShiftNumber - 1)) / 10.f) * 10;
	int32 Forced = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntQuota="), Forced) && Forced > 0)
	{
		Quota = Forced;
	}
	GS->TargetScore = Quota;
	GS->Shift = ShiftNumber;
	GS->Larvae = 3 + 2 * Players;
	GS->ShiftStage = 0;
	GS->bQuotaMet = false;
	RunStartShift();   // cards, the Queen's orders, the event, the clock
	SetHumanAwake(false);
	Broadcast(FString::Printf(TEXT("СМЕНА %d. Королеве нужно %d еды. Личинок в запасе: %d. Человек пока залип в телефон."), ShiftNumber, Quota, GS->Larvae), Gold);
}

void AAntKitchenGameMode::ShiftTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	const float Left = GS->TimeRemaining;
	const float Gone = MatchLength + (HasCard(AntCard::Overtime) ? 45.f : 0.f) - Left;
	// Quiet start to learn the room, then the Human walks in, then he cleans up in earnest, then the last call.
	float Quiet = 75.f + (HasCard(AntCard::Phone) ? 30.f : 0.f);
	FParse::Value(FCommandLine::Get(), TEXT("AntQuiet="), Quiet);   // dev: -AntQuiet=5 wakes the Human almost at once
	const uint8 Stage = Left <= 30.f ? 3 : Left <= 120.f ? 2 : Gone >= Quiet ? 1 : 0;
	RunTick();
	if (GS->Phase != 0)
	{
		return;
	}
	if (Stage != GS->ShiftStage)
	{
		GS->ShiftStage = Stage;
		if (Stage >= 1)
		{
			SetHumanAwake(true);
		}
		if (Stage == 1)
		{
			Broadcast(TEXT("ЧЕЛОВЕК ЗАШЁЛ НА КУХНЮ. Теперь осторожно."), Bad);
		}
		else if (Stage == 2)
		{
			if (Human)
			{
				Human->SetRage(3);
				GS->HumanRage = 3;
			}
			Broadcast(TEXT("УБОРКА! Человек взялся за дихлофос всерьёз. Осталось 2 минуты."), Bad);
		}
		else if (Stage == 3)
		{
			Broadcast(TEXT("ПОСЛЕДНИЕ 30 СЕКУНД: все в гнездо!"), Gold);
		}
	}
	// Quota in the bank and nobody left outside: the shift is won. (Staying out for more is the gamble.)
	if (GS->bQuotaMet && !AnyAllyPlayerAlive(true))
	{
		EndMatch(1);
		return;
	}
	if (Left <= 0.f)
	{
		EndMatch(GS->bQuotaMet ? 1 : 2);
	}
}