#include "AntRaid.h"
#include "AntRts.h"
#include "AntRun.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "AntBotController.h"
#include "AntArmy.h"
#include "AntChain.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "KitchenFixture.h"
#include "NpcBody.h"
#include "AntClan.h"
#include "AntSquad.h"
#include "AntSwarm.h"
#include "AntRoutes.h"
#include "AntHUD.h"
#include "Critters.h"
#include "FolkCharacters.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "NpcRoutine.h"
#include "AntSnowball.h"
#include "AntColony.h"
#include "AntFighter.h"
#include "AntTutorial.h"

namespace
{
	const FLinearColor RaidGood(0.4f, 1.f, 0.4f);
	const FLinearColor RaidBad(1.f, 0.35f, 0.3f);
	const FLinearColor RaidInfo(0.9f, 0.9f, 0.9f);
	const FLinearColor RaidGold(1.f, 0.8f, 0.2f);

	/** v14 wire-up: the colony's Казарма (Soldiers) and Ясли (Brood) set the veterans' draft cap - refresh it the moment one is built. */
	void RefreshVetDraftCap(UWorld* W)
	{
		if (AAntFighterState* FS = AAntFighterState::Get(W, false)) { FS->DraftCap = (uint8)AntVets::DraftCapFor(W); }
	}

	bool FastScreens()
	{
		static const bool b = FParse::Param(FCommandLine::Get(), TEXT("AntFastVote"));
		return b;
	}
	/** Server: the stolen cake lies in the red hill (set and cleared by the raid of the current world). */
	bool GHillOpen = false;

	/** Special finds: ordinary pieces of food that turn out to be something the nest wants badly. */
	namespace Find { enum : uint8 { None, Jelly, Larva, Gift, Gold, Count }; }
	const TCHAR* FindName(uint8 Id)
	{
		static const TCHAR* N[Find::Count] = { TEXT(""), TEXT("Капля маточного молочка"), TEXT("Чужая личинка"), TEXT("Засахаренный лепесток"), TEXT("Золотая крошка") };
		return N[Id < Find::Count ? Id : 0];
	}
}

bool AAntRaid::HillOpen()
{
	return GHillOpen;
}

float AAntRaid::ClanRespawn() const
{
	return InRaid() ? TierDef().ClanRespawn : 5.f;
}

AAntRaid::AAntRaid()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(5.f);
	Nest.Up.Init(0, AntUp::Count);
}

void AAntRaid::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntRaid, Phase);
	DOREPLIFETIME(AAntRaid, RaidNo);
	DOREPLIFETIME(AAntRaid, Tier);
	DOREPLIFETIME(AAntRaid, PhaseEnds);
	DOREPLIFETIME(AAntRaid, StartEnds);
	DOREPLIFETIME(AAntRaid, Cake);
	DOREPLIFETIME(AAntRaid, CakeLoc);
	DOREPLIFETIME(AAntRaid, bCakeKnown);
	DOREPLIFETIME(AAntRaid, CakeHint);
	DOREPLIFETIME(AAntRaid, CakeWindowEnds);
	DOREPLIFETIME(AAntRaid, CakeLeft);
	DOREPLIFETIME(AAntRaid, RaidJelly);
	DOREPLIFETIME(AAntRaid, ExitEnds);
	DOREPLIFETIME(AAntRaid, ExitBy);
	DOREPLIFETIME(AAntRaid, ExitHome);
	DOREPLIFETIME(AAntRaid, ExitNeed);
	DOREPLIFETIME(AAntRaid, ClanAlert);
	DOREPLIFETIME(AAntRaid, AmbushState);
	DOREPLIFETIME(AAntRaid, AmbushLoc);
	DOREPLIFETIME(AAntRaid, ClanPartyLoc);
	DOREPLIFETIME(AAntRaid, ClanAlive);
	DOREPLIFETIME(AAntRaid, ContractOffer);
	DOREPLIFETIME(AAntRaid, ContractPick);
	DOREPLIFETIME(AAntRaid, Mods);
	DOREPLIFETIME(AAntRaid, Nest);
	DOREPLIFETIME(AAntRaid, Result);
	DOREPLIFETIME(AAntRaid, NestNote);
	DOREPLIFETIME(AAntRaid, Stolen);
	DOREPLIFETIME(AAntRaid, SpecialsLeft);
	DOREPLIFETIME(AAntRaid, StartCost);
	DOREPLIFETIME(AAntRaid, ColonyShow);
}

AAntRaid* AAntRaid::Get(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<AAntRaid> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

const TCHAR* AAntRaid::CakeName()
{
	return TEXT("Кусок именинного торта");
}

bool AAntRaid::IsHome(const AAntCharacter* Ant)
{
	if (!Ant || Ant->bDead || Ant->Team != 0)
	{
		return false;
	}
	const FVector L = Ant->GetActorLocation();
	if (FVector::Dist2D(L, AKitchenBuilder::NestLocation(0)) < AKitchenBuilder::NestRadius(0) && L.Z < 400.f)
	{
		return true;
	}
	return KitchenArt::Enabled() && FVector::Dist2D(L, LevelV2::SouthEntrance()) < LevelV2::SouthEntranceRadius() && L.Z < 500.f;
}

AAntGameState* AAntRaid::GS() const
{
	return GetWorld() ? GetWorld()->GetGameState<AAntGameState>() : nullptr;
}

void AAntRaid::Say(const FString& Text, const FLinearColor& Color) const
{
	if (GM) { GM->Broadcast(Text, Color); }
}

float AAntRaid::RaidTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() - RaidStartedAt : 0.f;
}

FString AAntRaid::Clock() const
{
	const int32 T = FMath::Max(0, FMath::FloorToInt(RaidTime()));
	return FString::Printf(TEXT("%d:%02d"), T / 60, T % 60);
}

FString AAntRaid::PlaceName(const FVector& P) const
{
	if (!KitchenArt::Enabled())
	{
		return FString();
	}
	if (const TCHAR* C = LevelV2::CalloutNear(P, 1300.f)) { return C; }
	if (const TCHAR* D = LevelV2::DistrictAt(P)) { return D; }
	return FString();
}

void AAntRaid::Note(const FString& Line)
{
	if (InRaid() && Story.Num() < 40)
	{
		Story.Add(Line);
		UE_LOG(LogTemp, Display, TEXT("ANTRAID story: %s"), *Line);
	}
}

// ---------------------------------------------------------------------------------------------
// Start-up, the nest, starting a raid
// ---------------------------------------------------------------------------------------------

void AAntRaid::ServerInit(AAntKitchenGameMode* InGM)
{
	GM = InGM;
	GHillOpen = false;
	static bool bResetDone = false;   // once per process: the menu reloads the map
	if (!bResetDone && FParse::Param(FCommandLine::Get(), TEXT("AntNestReset")))
	{
		bResetDone = true;
		AntNest::Reset();
		UE_LOG(LogTemp, Display, TEXT("ANTNEST reset asked on the command line: the save is moved aside"));
	}
	FString Why;
	const bool bHad = AntNest::Load(Nest, &Why);
	if (Why.Contains(TEXT("damaged")))
	{
		NestNote = bHad ? TEXT("Сохранение колонии было повреждено: загружена копия от прошлого рейда")
			: TEXT("Сохранение колонии и его копия повреждены: начата новая колония");
	}
	int32 ForcedTier = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntTier="), ForcedTier) && ForcedTier >= 1)   // dev
	{
		Nest.MaxTier = FMath::Clamp(FMath::Max(Nest.MaxTier, ForcedTier), 1, AntTier::Max);
		Nest.Tier = FMath::Clamp(ForcedTier, 1, AntTier::Max);
	}
	if (AntTut::IsDemo()) { Nest.MaxTier = 1; Nest.Tier = 1; }   // the demo: tier 1 only
	AntTut::ApplyGift(this);   // the Queen's gift after the training (once, into the real colony)
	RaidNo = Nest.Raids;
	UE_LOG(LogTemp, Display, TEXT("ANTNEST %s (%s): food %d jelly %d larvae %d workers %d mood %d raids %d tier %d of %d, up %d %d %d %d %d %d"), bHad ? TEXT("loaded") : TEXT("fresh"), *Why,
		Nest.Food, Nest.Jelly, Nest.Larvae, Nest.Workers, Nest.Mood, Nest.Raids, Nest.Tier, Nest.MaxTier,
		(int32)Nest.Tier_(0), (int32)Nest.Tier_(1), (int32)Nest.Tier_(2), (int32)Nest.Tier_(3), (int32)Nest.Tier_(4), (int32)Nest.Tier_(5));
	EnterNest(true);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &AAntRaid::PhaseSecond, 1.f, true);
	if (FParse::Param(FCommandLine::Get(), TEXT("AntRaidNow")))   // dev / tests: straight into a raid with what the colony has
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, this, &AAntRaid::StartRaid, 3.f, false);
	}
}

void AAntRaid::SaveNest()
{
	AntColony::Sync(Nest);
	const bool bOk = AntNest::Save(Nest);
	UE_LOG(LogTemp, Display, TEXT("ANTNEST save %s: food %d jelly %d larvae %d workers %d mood %d raids %d -> %s"), bOk ? TEXT("ok") : TEXT("FAILED"),
		Nest.Food, Nest.Jelly, Nest.Larvae, Nest.Workers, Nest.Mood, Nest.Raids, *AntNest::SavePath());
}

void AAntRaid::PickContracts()
{
	// Three demands of the Queen for the next raid; the team takes up to two of them.
	TArray<uint8> Pool;
	for (uint8 i = 0; i < AntOrder::Count; ++i)
	{
		if (i != AntOrder::Overfill || true) { Pool.Add(i); }
	}
	ContractOffer.Reset();
	ContractPick = 0;
	FString Forced;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntOrders="), Forced, false))   // dev: -AntOrders=0,8,11 (all taken)
	{
		TArray<FString> Ids;
		Forced.ParseIntoArray(Ids, TEXT(","));
		for (const FString& Id : Ids)
		{
			const uint8 O = (uint8)FMath::Clamp(FCString::Atoi(*Id), 0, AntOrder::Count - 1);
			if (ContractOffer.Num() < 3 && Pool.Remove(O) > 0) { ContractPick |= (uint8)(1 << ContractOffer.Num()); ContractOffer.Add(O); }
		}
	}
	while (ContractOffer.Num() < 3 && Pool.Num() > 0)
	{
		const int32 k = FMath::RandRange(0, Pool.Num() - 1);
		ContractOffer.Add(Pool[k]);
		Pool.RemoveAtSwap(k);
	}
}

void AAntRaid::EnterNest(bool bFirst)
{
	Phase = AntRaidPhase::Nest;
	PhaseEnds = 0.f;
	StartEnds = 0.f;
	ExitEnds = 0.f;
	Mods = 0;
	Tier = (uint8)FMath::Clamp(Nest.Tier, 1, Nest.MaxTier);
	RaidNo = Nest.Raids;
	if (AAntGameState* S = GS())
	{
		S->Phase = AntRaidGS::InNest;
		S->Interlude = 0;
		S->HumanGoal = AntHumanGoal::Idle;
		S->HumanGoalText = FString();
	}
	if (GM) { GM->SetHumanAwake(false); }
	PickContracts();
	PickLevelSeed(-1);
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		It->bNestReady = false;
		It->bHome = true;
		It->Charges0 = 0;
		It->Charges1 = 0;
	}
	SpecialsLeft = 0;
	CountStartCost();
	if (!bFirst && GM)
	{
		for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It) { It->Destroy(); }
		for (TActorIterator<AAntChain> It(GetWorld()); It; ++It) { It->Destroy(); }   // (bridges: their workers have been counted home)
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC) { continue; }
			AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
			if (Ant && !Ant->bDead && Ant->Creature == 0)
			{
				Ant->LetGo();
				Ant->ApplyTeleport(AKitchenBuilder::NestLocation(0) + FVector(FMath::FRandRange(150.f, 350.f), FMath::FRandRange(-250.f, 250.f), 60.f));
			}
			else if (!Cast<AHumanCharacter>(PC->GetPawn()) && (!Ant || Ant->bDead))
			{
				GM->RespawnAnt(PC);
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRAID in the nest: raids done %d, tier %d of %d, food %d jelly %d larvae %d workers %d mood %d"), Nest.Raids, (int32)Tier, Nest.MaxTier,
		Nest.Food, Nest.Jelly, Nest.Larvae, Nest.Workers, Nest.Mood);
}

void AAntRaid::PickLevelSeed(int32 Want)
{
	AAntGameState* S = GS();
	if (!LevelV3::Enabled() || !S)
	{
		return;
	}
	int32 Dev = -1;
	if (Want < 0 && FParse::Value(FCommandLine::Get(), TEXT("AntSeed="), Dev) && Dev >= 0)
	{
		Want = Dev;   // -AntSeed=N: every raid of this session on seed N (of its tier)
	}
	const int32 T = FMath::Clamp((int32)Tier, 1, AntTier::Max);
	if (Want < 0)
	{
		FRandomStream R(FPlatformTime::Cycles() ^ (uint32)(RaidNo * 7919));
		const FLevelV3Seed* P = LevelV3::PickSeed(T, R);
		Want = P ? P->Seed : 0;
	}
	S->SetLevelSeed(Want, T);
	if (GM) { GM->ApplyV3Doors(); }
	ApplyWhoIsHome();
}

void AAntRaid::ApplyWhoIsHome()
{
	if (!LevelV3::Enabled())
	{
		return;
	}
	auto SetAway = [this](AActor* A, bool bAway)
	{
		if (!A) { return; }
		const bool bWasAway = AwaySpots.Contains(A);
		if (bAway == bWasAway) { return; }
		ACharacter* C = Cast<ACharacter>(A);
		if (bAway)
		{
			AwaySpots.Add(A, A->GetActorTransform());
			A->SetActorHiddenInGame(true);
			A->SetActorEnableCollision(false);
			A->SetActorTickEnabled(false);
			if (C && C->GetCharacterMovement()) { C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->SetComponentTickEnabled(false); }
			if (C && C->GetController()) { C->GetController()->SetActorTickEnabled(false); }
			A->SetActorLocation(FVector(0.f, 30000.f + 2000.f * AwaySpots.Num(), -20000.f), false, nullptr, ETeleportType::TeleportPhysics);   // out of the flat: no ear, no paw reaches the game
		}
		else
		{
			const FTransform T = AwaySpots.FindAndRemoveChecked(A);
			A->SetActorTransform(T, false, nullptr, ETeleportType::TeleportPhysics);
			A->SetActorHiddenInGame(false);
			A->SetActorEnableCollision(true);
			A->SetActorTickEnabled(true);
			if (C && C->GetCharacterMovement()) { C->GetCharacterMovement()->SetComponentTickEnabled(true); }
			if (C && C->GetController()) { C->GetController()->SetActorTickEnabled(true); }
		}
	};
	FString Out;
	for (TActorIterator<AGrandmaCharacter> It(GetWorld()); It; ++It) { SetAway(*It, !LevelV3::IsHome(LevelV3::Grandma)); }
	for (TActorIterator<AKidCharacter> It(GetWorld()); It; ++It) { SetAway(*It, !LevelV3::IsHome(LevelV3::Kid)); }
	for (TActorIterator<AHouseholdNpc> It(GetWorld()); It; ++It) { SetAway(*It, !LevelV3::IsHome(It->Kind == 0 ? LevelV3::Kid : LevelV3::Grandma)); }
	for (TActorIterator<ACritter> It(GetWorld()); It; ++It) { if (It->Kind <= 1) { SetAway(*It, !LevelV3::IsHome(It->Kind == 0 ? LevelV3::Dog : LevelV3::Cat)); } }
	for (const TPair<TWeakObjectPtr<AActor>, FTransform>& KV : AwaySpots) { if (KV.Key.IsValid()) { Out += KV.Key->GetClass()->GetName() + TEXT(" "); } }
	UE_LOG(LogTemp, Display, TEXT("ANTSEED who is home: away %s"), Out.IsEmpty() ? TEXT("nobody") : *Out);
}

void AAntRaid::StartRaid()
{
	if (InRaid() || !GM)
	{
		return;
	}
	if (Nest.Larvae <= 0)
	{
		// Never a dead end: the Queen always has one more egg.
		Nest.Larvae = 1;
		Say(TEXT("Личинок не было: Королева отложила последнюю. Берегите её"), RaidInfo);
	}
	const int32 MinWorkers = FMath::Max(6, 2 * FMath::Clamp(GM ? GM->GetNumPlayers() : 1, 1, 8));   // (5-8 players: two each at least)
	if (Nest.Workers < MinWorkers)
	{
		// Never a dead end either: a raid cut short (the game closed or crashed after the squads left) loses the workers taken out,
		// and with none at all a tier-1 cake (48) cannot be lifted. The nest always has six to send (two a player with 5-8).
		UE_LOG(LogTemp, Display, TEXT("ANTNEST only %d workers left: the nest makes it %d for this raid"), Nest.Workers, MinWorkers);
		Nest.Workers = MinWorkers;
	}
	Tier = (uint8)FMath::Clamp(Nest.Tier, 1, Nest.MaxTier);
	RaidNo = Nest.Raids + 1;
	StartEnds = 0.f;
	if (LevelV3::Enabled() && GS() && GS()->LevelSeedKey % 8 != Tier)
	{
		GS()->SetLevelSeed(GS()->LevelSeed(), Tier);   // the tier changed since the seed was drawn: the same number, the tier's own row
	}
	// What the team takes along is paid for at the door, out of the stores (and is at risk from now on).
	int32 Paid = 0;
	for (uint8 i = 0; i < AntMod::Count; ++i)
	{
		if (!(Mods & (1 << i))) { continue; }
		if (Nest.Food >= AntMod::Cost(i)) { Nest.Food -= AntMod::Cost(i); Paid += AntMod::Cost(i); }
		else
		{
			Mods &= ~(1 << i);
			Say(FString::Printf(TEXT("На «%s» не хватило еды: рейд без него"), AntMod::Name(i)), RaidInfo);
		}
	}
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		for (int32 Pocket = 0; Pocket < AntTool::Pockets; ++Pocket)
		{
			uint8& Tool = Pocket == 0 ? It->Tool0 : It->Tool1;
			uint8& Charges = Pocket == 0 ? It->Charges0 : It->Charges1;
			Charges = 0;
			if (Tool == AntTool::None) { continue; }
			if (AntNest::ToolUnlocked(Nest, Tool) && Nest.Food >= AntTool::Cost(Tool))
			{
				Nest.Food -= AntTool::Cost(Tool);
				Paid += AntTool::Cost(Tool);
				Charges = AntTool::MaxCharges;
			}
			else
			{
				Tool = AntTool::None;
			}
		}
	}
	if (Paid > 0) { UE_LOG(LogTemp, Display, TEXT("ANTNEST paid %d food at the door (boosts %d, tools), %d left"), Paid, (int32)Mods, Nest.Food); }
	GM->MatchLength = TierDef().Length;
	float ForcedLen = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntRaidLen="), ForcedLen) && ForcedLen > 20.f) { GM->MatchLength = ForcedLen; }   // dev
	GM->ShiftNumber = RaidNo;
	Phase = AntRaidPhase::Raid;
	GM->ResetMatch();   // loot, props, the Human, everybody back in the nest; calls OnWorldReset()
}

void AAntRaid::OnWorldReset()
{
	AAntGameState* S = GS();
	if (!S || !GM)
	{
		return;
	}
	AntSnowball::OnWorldReset(this);   // the last raid's squad ants and trails go (AntSnowball.cpp)
	const AntTier::FDef& T = TierDef();
	const int32 Players = FMath::Max(1, GM->GetNumPlayers());
	S->Shift = RaidNo;
	S->Larvae = Nest.Larvae;
	S->ShiftStage = 0;
	S->bQuotaMet = false;
	// the cake, and side loot for every mouth (+40 % a player up to four, +25 % each from five to eight)
	S->TargetScore = T.CakeValue + FMath::RoundToInt(T.Ask * (1.f + 0.4f * (FMath::Min(Players, 4) - 1) + 0.25f * FMath::Clamp(Players - 4, 0, 4)) / 10.f) * 10;
	// What the nest has built (and the boosts bought for this raid) acts through the old run cards.
	TArray<uint8> Cards;
	AntNest::Cards(Nest, Cards);
	for (uint8 i = 0; i < AntMod::Count; ++i)
	{
		if (Mods & (1 << i)) { Cards.AddUnique(AntMod::Card(i)); }
	}
	FString Grant;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntCards="), Grant, false))   // dev
	{
		TArray<FString> Ids;
		Grant.ParseIntoArray(Ids, TEXT(","));
		for (const FString& Id : Ids) { Cards.AddUnique((uint8)FMath::Clamp(FCString::Atoi(*Id), 0, AntCard::Count - 1)); }
	}
	S->Cards = Cards;
	GM->RunStartShift();   // the clock, the run's counters, the event plan
	// The contracts taken in the nest are this raid's orders.
	S->Orders.Reset();
	for (int32 i = 0; i < ContractOffer.Num(); ++i)
	{
		if (ContractPick & (1 << i)) { S->Orders.Add(ContractOffer[i]); }
	}
	S->OrdersDone = 0;
	S->OrdersFailed = 0;

	Cake = AntCake::Lying;
	bCakeKnown = Nest.Tier_(AntUp::Scouts) >= 1 || Tier <= 1;   // (on a weekday the Queen's own scouts have seen it: the first raids need no search)
	CakeHint = FString();
	CakeWindowEnds = 0.f;
	CakeLeft = 100;
	GHillOpen = false;
	Stolen = 0;
	CakeBaseHalf = 0;
	if (AKitchenLoot* C = GM->MainObject.Get())
	{
		CakeLoc = C->GetActorLocation();
		CakeBaseHalf = C->Value;
		float Best = 1.e12f;
		for (const FLevelV2Main& M : LevelV2::MainSpots())
		{
			const float D = (float)FVector::DistSquared(FVector(M.X, M.Y, M.Z), C->GetActorLocation());
			if (D < Best) { Best = D; CakeHint = M.Where; }
		}
	}
	ApplyWhoIsHome();
	if (LevelV3::Enabled()) { GM->ApplyV3Doors(); UE_LOG(LogTemp, Display, TEXT("%s"), *LevelV3::Describe()); }
	RaidJelly = 0;
	ExitEnds = 0.f;
	ExitBy = FString();
	ExitOk.Reset();
	ExitStarter = nullptr;
	Story.Reset();
	Variety.Reset();
	Deaths = 0;
	FMemory::Memzero(RaidKinds);
	RaidLines.Reset();
	RaidTrophies.Reset();
	bCakeWasInHill = false;
	TrophiesHome = 0;
	SpecialsHome = 0;
	bCakeLifted = false;
	bAllAwake = false;
	bHalfSaid = false;
	LastStage = 0;
	ClanAlert = 0;
	AmbushState = 0;
	ClanAlive = 0;
	Result = FAntRaidResult();
	RaidStartedAt = GetWorld()->GetTimeSeconds();
	if (NpcRoutine::Enabled())
	{
		// Level v3: the residents' evening for this seed, tier and time of day; no mess yet.
		NpcRoutine::ResetRaid();
		NpcRoutine::Rebuild(GetWorld());
	}
	GM->SetHumanAwake(false);
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		It->bNestReady = false;
		It->bHome = true;
	}
	{
		// The rival clan gets its parties once the bots of the old round are back at their hill (the reset respawns them after this).
		FTimerHandle CH;
		GetWorldTimerManager().SetTimer(CH, this, &AAntRaid::StartClan, 1.2f, false);
		FTimerHandle SH;
		GetWorldTimerManager().SetTimer(SH, this, &AAntRaid::StartSquads, 0.8f, false);
		FTimerHandle FH;
		GetWorldTimerManager().SetTimer(FH, this, &AAntRaid::StartSpecials, 1.6f, false);
	}
	MoodGift = 0;
	bSecondEvent = false;
	SpecialsLeft = 0;
	Say(FString::Printf(TEXT("РЕЙД %d · угроза %d «%s». Цель — кусок именинного торта. Личинок: %d. Уйти: удерживай [L] у входа в гнездо"), RaidNo, (int32)Tier, T.Name, S->Larvae), RaidGold);
	UE_LOG(LogTemp, Display, TEXT("ANTRAID start %d: tier %d, length %.0f, quiet %.0f, final %.0f, ask %d, larvae %d, cake «%s» w %d v %d at %s, known %d, cards %d, contracts %d, players %d"), RaidNo, (int32)Tier,
		S->TimeRemaining, T.Quiet, T.Final, S->TargetScore, S->Larvae, *CakeHint, GM->MainObject.IsValid() ? GM->MainObject->Weight : 0, GM->MainObject.IsValid() ? GM->MainObject->Value * 2 : 0,
		*FVector(CakeLoc).ToCompactString(), bCakeKnown ? 1 : 0, S->Cards.Num(), S->Orders.Num(), Players);
}

void AAntRaid::StartSquads()
{
	if (InRaid() && AntSnowball::Enabled(this))
	{
		// The snowball raid: everybody goes out alone; squads and workers come from the picks (AntSnowball.h).
		AntSnowball::OnRaidStart(this);
		AntFighter::OnRaidStart(this);   // the tally; drafted veterans join their squads (AntFighter.h)
		SpawnGuards();
		SaveNest();
		return;
	}
	if (InRaid() && !FParse::Param(FCommandLine::Get(), TEXT("AntNoSquad")))   // (dev: a raid without squads)
	{
		Result.SquadOut = AntSquad::SpawnAll(this);
		SpawnGuards();
	}
	// What has left the nest is not in the nest: whoever quits in the middle of a raid finds the colony without it.
	if (InRaid()) { SaveNest(); }
}

void AAntRaid::SpawnGuards()
{
	// Soldiers II: six guards at each way into the nest. They bite whatever red comes close; they are not anybody's squad.
	if (Nest.Tier_(AntUp::Soldiers) < 2)
	{
		return;
	}
	const FVector Posts[2] = { AntRoutes::Point(AntRoutes::NestNode()), LevelV2::SouthEntrance() };
	for (const FVector& Post : Posts)
	{
		const FVector At = Post + FVector(0.f, 0.f, 10.f);
		AAntArmy* A = GetWorld()->SpawnActorDeferred<AAntArmy>(AAntArmy::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A) { continue; }
		A->bSquad = true;
		A->bGuard = true;
		A->GuardPost = At;
		A->Team = 0;
		A->Count = 6;
		A->Taken = 6;
		A->Mode = EArmyMode::Hold;
		A->Target = At;
		A->BiteMul = 2.f;
		A->FinishSpawning(FTransform(At));
		UE_LOG(LogTemp, Display, TEXT("ANTSQUAD guards: 6 at %s"), *At.ToCompactString());
	}
}

void AAntRaid::StartSpecials()
{
	if (!InRaid() || !GM || AntTut::OwnsRaid(this))   // (the training and the sandbox: no special finds)
	{
		return;
	}
	TArray<AKitchenLoot*> Pool;
	const FVector Home = AKitchenBuilder::NestLocation(0);
	for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
	{
		AKitchenLoot* L = *It;
		if (L->bIsGear || L->bVault || L->bTrophy || L->bBody || L->bMain || L->bHeld || L->IsCarried() || L->Weight > 14 || L->Special != 0)
		{
			continue;
		}
		if (FVector::Dist2D(L->GetActorLocation(), Home) < 3000.f)
		{
			continue;   // not on the doorstep
		}
		Pool.Add(L);
	}
	const int32 Want = TierDef().Specials;
	uint8 Kind = (uint8)FMath::RandRange(1, Find::Count - 1);
	for (int32 i = 0; i < Want && Pool.Num() > 0; ++i)
	{
		const int32 k = FMath::RandRange(0, Pool.Num() - 1);
		AKitchenLoot* L = Pool[k];
		Pool.RemoveAtSwap(k);
		L->Special = Kind;
		L->LootName = FindName(Kind);
		L->Value = Kind == Find::Gold ? 150 : 10;
		L->ForceNetUpdate();
		++SpecialsLeft;
		UE_LOG(LogTemp, Display, TEXT("ANTRAID special find %d «%s» at %s (%s)"), (int32)Kind, FindName(Kind), *L->GetActorLocation().ToCompactString(), *PlaceName(L->GetActorLocation()));
		Kind = (uint8)(Kind % (Find::Count - 1) + 1);
	}
	if (SpecialsLeft > 0)
	{
		Say(FString::Printf(TEXT("В доме %s: %d. Вблизи видно по метке"), SpecialsLeft == 1 ? TEXT("спрятана особая находка") : TEXT("спрятаны особые находки"), (int32)SpecialsLeft), RaidInfo);
	}
}

void AAntRaid::CountStartCost()
{
	int32 Cost = 0;
	for (uint8 i = 0; i < AntMod::Count; ++i)
	{
		if (Mods & (1 << i)) { Cost += AntMod::Cost(i); }
	}
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		if (AntNest::ToolUnlocked(Nest, It->Tool0)) { Cost += AntTool::Cost(It->Tool0); }
		if (AntNest::ToolUnlocked(Nest, It->Tool1)) { Cost += AntTool::Cost(It->Tool1); }
	}
	StartCost = Cost;
}

void AAntRaid::StartClan()
{
	if (!InRaid() || !GM || FParse::Param(FCommandLine::Get(), TEXT("AntNoClan")) || AntTut::OwnsRaid(this))   // (dev: a raid without the red clan; the training)
	{
		return;
	}
	if (!Clan)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Clan = GetWorld()->SpawnActor<AAntClan>(AAntClan::StaticClass(), FTransform::Identity, P);
	}
	if (Clan) { Clan->Begin(GM, this); }
}

bool AAntRaid::OnClanCakeHome(AKitchenLoot* C)
{
	AAntGameState* S = GS();
	if (!InRaid() || !C || !S)
	{
		return false;
	}
	C->ReleaseAll();
	// It lies in the crater on top of the hill, in plain sight, while they eat it: 100% down to 40% over the tier's window.
	const FVector Top = AKitchenBuilder::NestLocation(1) + FVector(0.f, 0.f, 470.f);
	C->SetActorLocation(Top, false, nullptr, ETeleportType::TeleportPhysics);
	const float Window = TierDef().Window * FMath::Clamp((CakeLeft - 40) / 60.f, 0.05f, 1.f);
	CakeWindowEnds = S->GetServerWorldTimeSeconds() + Window;
	Cake = AntCake::InHill;
	bCakeWasInHill = true;
	CakeLoc = Top;
	bCakeKnown = true;
	GHillOpen = true;
	ClanAlert = 4;
	Say(FString::Printf(TEXT("КРАСНЫЕ УНЕСЛИ ТОРТ В СВОЙ ХОЛМ! Его едят. Набег на холм (двор за гостиной): отбейте за %d:%02d"), (int32)Window / 60, (int32)Window % 60), RaidBad);
	Note(FString::Printf(TEXT("%s — красные унесли торт в свой холм"), *Clock()));
	UE_LOG(LogTemp, Display, TEXT("ANTRAID the cake is in the red hill at %s: window %.0f s, %d%% left"), *Clock(), Window, (int32)CakeLeft);
	if (Clan) { Clan->OnCakeInHill(); }
	return true;
}

void AAntRaid::HillTick()
{
	AAntGameState* S = GS();
	AKitchenLoot* C = GM ? GM->MainObject.Get() : nullptr;
	if (!C)
	{
		Cake = AntCake::Eaten;
		GHillOpen = false;
		return;
	}
	if ((C->IsCarried() && C->CarryTeam() == 0) || (C->bHeld && C->SquadHolder.IsValid()))
	{
		// Taken back: the clock stops, what is left of it is ours to carry home.
		Cake = AntCake::Ours;
		GHillOpen = false;
		CakeWindowEnds = 0.f;
		ClanAlert = 3;
		FString Names;
		for (const AAntCharacter* A : C->Carriers)
		{
			if (A && A->Team == 0) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); }
		}
		Say(FString::Printf(TEXT("ТОРТ ОТБИТ ИЗ ХОЛМА (%d%% осталось): %s. Теперь домой — они гонятся!"), (int32)CakeLeft, *Names), RaidGold);
		Note(FString::Printf(TEXT("%s — набег на красный холм: торт отбили %s (осталось %d%%)"), *Clock(), *Names, (int32)CakeLeft));
		UE_LOG(LogTemp, Display, TEXT("ANTRAID the cake was taken back from the red hill at %s by %s, %d%% left"), *Clock(), *Names, (int32)CakeLeft);
		return;
	}
	const float Left = CakeWindowEnds - S->GetServerWorldTimeSeconds();
	CakeLeft = (uint8)FMath::Clamp(FMath::RoundToInt(40.f + 60.f * Left / FMath::Max(1.f, TierDef().Window)), 40, 100);
	const int32 Value = FMath::Max(1, CakeBaseHalf * CakeLeft / 100);
	if (Value != C->Value)
	{
		C->Value = Value;
	}
	CakeLoc = C->GetActorLocation();
	if (Left <= 0.f)
	{
		C->ReleaseAll();
		C->Destroy();
		Cake = AntCake::Eaten;
		GHillOpen = false;
		CakeWindowEnds = 0.f;
		ClanAlert = 0;
		Say(TEXT("Красные доели торт. Берите, что осталось в квартире, и уходите живыми"), RaidBad);
		Note(FString::Printf(TEXT("%s — красные доели торт в своём холме"), *Clock()));
		UE_LOG(LogTemp, Display, TEXT("ANTRAID the cake was eaten in the red hill at %s"), *Clock());
	}
}

void AAntRaid::OnPlayerJoined(AController* C)
{
	if (AAntPlayerState* PS = C ? C->GetPlayerState<AAntPlayerState>() : nullptr)
	{
		PS->bNestReady = false;
		PS->bHome = true;
	}
}

// ---------------------------------------------------------------------------------------------
// The raid, second by second
// ---------------------------------------------------------------------------------------------

int32 AAntRaid::NpcLevel(uint8 Who) const
{
	if (!InRaid())
	{
		return 0;   // in the nest and on the results screen the house minds its own business
	}
	if (bAllAwake)
	{
		return 2;
	}
	const AntTier::FDef& T = TierDef();
	// (the table is dog, cat, kid, grandma)
	int32 L = Who == NpcWho::Dog ? T.Npc[0] : Who == NpcWho::Cat ? T.Npc[1] : Who == NpcWho::Kid ? T.Npc[2] : Who == NpcWho::Grandma ? T.Npc[3] : 1;
	const AAntGameState* S = GS();
	if (Tier <= 1 && Who == NpcWho::Dog && S && S->ShiftStage == 0)
	{
		return 0;   // a weekday: while the Human is on his phone the dog sleeps too (he does not track scent)
	}
	if (S && GM && S->TimeRemaining < GM->MatchLength * 0.5f && (Who == NpcWho::Cat || Who == NpcWho::Kid))
	{
		L = FMath::Min(2, L + 1);   // the second half: the cat and the kid get bolder
	}
	return L;
}

int32 AAntRaid::LivingPlayers(int32* OutHome, FString* OutAway) const
{
	int32 Living = 0, Home = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || (GM && GM->Run.OutPlayers.Contains(PC)))
		{
			continue;   // out for good (no larva): nobody waits for them
		}
		const AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
		if (!Ant)
		{
			continue;   // playing the Human, or between pawns
		}
		if (Ant->Team != 0 || Ant->Creature != 0)
		{
			continue;
		}
		++Living;
		// A dead ant hatches again inside the nest: it is not somebody the others have to wait for.
		const bool bIn = Ant->bDead || IsHome(Ant);
		if (bIn)
		{
			++Home;
		}
		else if (OutAway)
		{
			*OutAway += (OutAway->IsEmpty() ? TEXT("") : TEXT(", ")) + Ant->GetDisplayName();
		}
		if (AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>())
		{
			PS->bHome = bIn;
		}
	}
	if (OutHome) { *OutHome = Home; }
	return Living;
}

void AAntRaid::RaidSecond()
{
	AAntGameState* S = GS();
	if (!S || !GM || !InRaid() || S->Phase != 0)
	{
		return;
	}
	if (AntTut::OwnsRaid(this))
	{
		// The training / the sandbox own the clock: no stages, no events, no timeout; the snowball still runs (picks, squad tasks).
		AntSnowball::RaidSecond(this);
		return;
	}
	const AntTier::FDef& T = TierDef();
	const float Left = S->TimeRemaining;
	const float Length = GM->MatchLength + (GM->HasCard(AntCard::Overtime) ? 45.f : 0.f);
	const float Gone = Length - Left;
	float Quiet = T.Quiet + (GM->HasCard(AntCard::Phone) ? 30.f : 0.f);
	FParse::Value(FCommandLine::Get(), TEXT("AntQuiet="), Quiet);   // dev
	const uint8 Stage = Left <= 30.f ? 3 : Left <= T.Final ? 2 : Gone >= Quiet ? 1 : 0;
	GM->RunTick();   // greed, the event, who is shut in the fridge, what the Human is up to
	AntSnowball::RaidSecond(this);   // picks, squad tasks, the timeline
	AntFighter::RaidSecond(this);   // cake seconds, the Giant's chase, doctrines, the flood (AntFighter.h)
	if (S->Phase != 0 || !InRaid())
	{
		return;
	}
	if (Stage != S->ShiftStage)
	{
		S->ShiftStage = Stage;
		if (Stage >= 1) { GM->SetHumanAwake(true); }
		if (Stage == 1)
		{
			if (GM->Human && T.RageFloor > GM->Human->Rage)
			{
				GM->Human->SetRage((uint8)T.RageFloor);
				S->HumanRage = (uint8)T.RageFloor;
			}
			Say(TEXT("ЧЕЛОВЕК ЗАШЁЛ НА КУХНЮ. Теперь осторожно."), RaidBad);
		}
		else if (Stage == 2)
		{
			bAllAwake = true;
			if (GM->Human)
			{
				GM->Human->SetRage(3);
				S->HumanRage = 3;
			}
			Say(FString::Printf(TEXT("ВЕСЬ ДОМ НА НОГАХ! Человек с дихлофосом, кошка, Бадрик, бабушка и малыш ищут вас. Осталось %d:%02d — пора домой"), (int32)Left / 60, (int32)Left % 60), RaidBad);
		}
		else if (Stage == 3)
		{
			Say(TEXT("ПОСЛЕДНИЕ 30 СЕКУНД: кто не в гнезде, тот пропал!"), RaidGold);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTRAID stage %d at %s (left %.0f)"), (int32)Stage, *Clock(), Left);
	}
	if (T.Events >= 2 && !bSecondEvent && GM->Run.bEventDone && S->EventId == 0 && Stage == 1 && Gone > GM->Run.EventAt + 80.f && Left > T.Final + 60.f)
	{
		// The harder days have a second event.
		bSecondEvent = true;
		const uint8 Was = GM->Run.EventPlanned;
		do { GM->Run.EventPlanned = (uint8)FMath::RandRange(1, AntEvent::Count - 1); } while (GM->Run.EventPlanned == Was && AntEvent::Count > 2);
		GM->Run.EventAt = Gone + 12.f;
		GM->Run.bEventDone = false;
		UE_LOG(LogTemp, Display, TEXT("ANTRAID a second event is planned: %s at %.0f s"), AntEvent::Name(GM->Run.EventPlanned), GM->Run.EventAt);
	}
	if (Cake == AntCake::Clan && !bCakeKnown && Nest.Tier_(AntUp::Scouts) >= 3)
	{
		bCakeKnown = true;   // Scouts III: the red party with the cake is always seen
	}
	if (!bHalfSaid && Left < Length * 0.5f && Stage == 1)
	{
		bHalfSaid = true;
		Say(TEXT("Полрейда позади: кошка и малыш осмелели"), RaidInfo);
	}
	TrackCake();
	ExitTick();
	if (!InRaid())
	{
		return;
	}
	if (Left <= 0.f)
	{
		EndRaid(AntRaidEnd::Timeout);
	}
}

void AAntRaid::TrackCake()
{
	if (Cake == AntCake::InHill)
	{
		HillTick();
		return;
	}
	if (Cake == AntCake::Home || Cake == AntCake::Eaten || Cake == AntCake::Broken)
	{
		return;
	}
	AKitchenLoot* C = GM ? GM->MainObject.Get() : nullptr;
	if (!C)
	{
		Cake = AntCake::Eaten;   // gone without being delivered or smashed (fell out of the world)
		Note(FString::Printf(TEXT("%s — торт пропал"), *Clock()));
		return;
	}
	const FVector L = C->GetActorLocation();
	if (FVector::DistSquared(L, FVector(CakeLoc)) > FMath::Square(120.f))
	{
		CakeLoc = L;
	}
	const bool bSquadHas = C->bHeld && C->SquadHolder.IsValid();
	const uint8 New = bSquadHas ? AntCake::Ours : C->IsCarried() ? (C->CarryTeam() == 0 ? AntCake::Ours : AntCake::Clan) : AntCake::Lying;
	if (New == AntCake::Ours && !bCakeLifted)
	{
		bCakeLifted = true;
		FString Names = bSquadHas ? FString(TEXT("отряд")) : FString();
		for (const AAntCharacter* A : C->Carriers)
		{
			if (A && A->Team == 0) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); }
		}
		Note(FString::Printf(TEXT("%s — торт взяли: %s (%s)"), *Clock(), *Names, *PlaceName(L)));
		Say(FString::Printf(TEXT("ТОРТ У НАС: %s. Несите к гнезду или к южному входу"), *Names), RaidGold);
	}
	if (!bCakeKnown)
	{
		// Somebody of ours has come close enough to see it: from now on everybody knows where it is.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const AAntCharacter* A = It->IsValid() ? Cast<AAntCharacter>((*It)->GetPawn()) : nullptr;
			if (A && !A->bDead && A->Team == 0 && FVector::Dist(A->GetActorLocation(), L) < 1500.f && KitchenContainers::SealedAt(GetWorld(), L) < 0)
			{
				bCakeKnown = true;
				Say(FString::Printf(TEXT("%s нашёл торт: %s"), *A->GetDisplayName(), *CakeHint), RaidGold);
				Note(FString::Printf(TEXT("%s — %s нашёл торт %s"), *Clock(), *A->GetDisplayName(), *CakeHint));
				break;
			}
		}
	}
	Cake = New;
}

void AAntRaid::OnCakeBroken(AAntCharacter* By)
{
	if (!InRaid() || Cake == AntCake::Home)
	{
		return;
	}
	Cake = AntCake::Broken;
	Note(FString::Printf(TEXT("%s — торт разбился%s"), *Clock(), By ? *FString::Printf(TEXT(": уронил %s"), *By->GetDisplayName()) : TEXT("")));
}

// ---------------------------------------------------------------------------------------------
// Leaving
// ---------------------------------------------------------------------------------------------

void AAntRaid::CancelExit(const FString& Why)
{
	if (ExitEnds > 0.f)
	{
		ExitEnds = 0.f;
		ExitOk.Reset();
		ExitStarter = nullptr;
		Say(Why, RaidInfo);
		UE_LOG(LogTemp, Display, TEXT("ANTRAID leaving cancelled: %s"), *Why);
	}
}

void AAntRaid::ExitTick()
{
	int32 Home = 0;
	FString Away;
	const int32 Living = LivingPlayers(&Home, &Away);
	ExitHome = (uint8)FMath::Clamp(Home, 0, 255);
	ExitNeed = (uint8)FMath::Clamp(Living, 0, 255);
	if (ExitEnds <= 0.f)
	{
		return;
	}
	if (Living == 0)
	{
		CancelExit(TEXT("Уходить некому"));
		return;
	}
	const APlayerController* St = Cast<APlayerController>(ExitStarter.Get());
	const AAntCharacter* StAnt = St ? Cast<AAntCharacter>(St->GetPawn()) : nullptr;
	if (!StAnt || (!StAnt->bDead && !IsHome(StAnt)))
	{
		CancelExit(FString::Printf(TEXT("%s вышел из гнезда: уход отменён"), *ExitBy));
		return;
	}
	int32 Ok = 0;
	for (const TWeakObjectPtr<AController>& C : ExitOk) { Ok += C.IsValid() ? 1 : 0; }
	if ((Home == Living && Ok >= Living) || GS()->GetServerWorldTimeSeconds() >= ExitEnds)
	{
		if (Home == Living)
		{
			EndRaid(AntRaidEnd::Left);
		}
		else
		{
			CancelExit(FString::Printf(TEXT("Не все дома: снаружи %s. Уход отменён"), *Away));
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Hooks from the game mode
// ---------------------------------------------------------------------------------------------

bool AAntRaid::OnDelivered(uint8 Team, AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers, int32& Value)
{
	if (!InRaid() || !Food)
	{
		return false;
	}
	if (Team != 0)
	{
		Stolen += Value;   // what the red clan carries into its hill is gone
		return false;
	}
	Variety.Add(Food->LootName);
	FString Names;
	int32 NumCarriers = 0;
	for (const AAntCharacter* A : Carriers)
	{
		if (A) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); ++NumCarriers; }
	}
	// The colony: what kind of food it was and who brought it (the cake's value is counted at the end, by the tier).
	const uint8 Kind = AntColony::KindOf(Food->LootName);
	auto Credit = [&](int32 Amount, const TCHAR* Feat)
	{
		for (const AAntCharacter* A : Carriers)
		{
			if (!A) { continue; }
			FAntRaidPlayerLine& L = LineOf(A);
			L.Food[Kind] += Amount / FMath::Max(1, NumCarriers);
			if (Feat) { L.Feats.AddUnique(Feat); }
		}
	};
	if (Food->bMain)
	{
		Credit(0, TEXT("cake_home"));
		Cake = AntCake::Home;
		bCakeKnown = true;
		GHillOpen = false;
		const int32 CakeJelly = FMath::Max(1, TierDef().CakeJelly * CakeLeft / 100);   // (less of it if the red clan had it for a while)
		RaidJelly += CakeJelly;
		if (AAntGameState* S = GS()) { S->bQuotaMet = true; }   // from now on greed grows while somebody stays outside
		const bool bSouth = KitchenArt::Enabled() && FVector::Dist2D(Food->GetActorLocation(), LevelV2::SouthEntrance()) < LevelV2::SouthEntranceRadius() + 100.f;
		Note(FString::Printf(TEXT("%s — торт дома: %s%s"), *Clock(), *Names, bSouth ? TEXT(" (через южный вход)") : TEXT("")));
		Say(FString::Printf(TEXT("ТОРТ ДОМА: +%d молочка. Можно уходить — или остаться за добавкой: жадность растёт"), CakeJelly), RaidGold);
		UE_LOG(LogTemp, Display, TEXT("ANTRAID cake home at %s by %s, jelly %d"), *Clock(), *Names, RaidJelly);
		return false;   // its value is set by the tier already
	}
	if (Food->bTrophy)
	{
		++TrophiesHome;
		RaidJelly += 1;
		const FString TN = Food->LootName.ToLower();
		RaidTrophies.AddUnique(TN.Contains(TEXT("мишк")) ? 6 : TN.Contains(TEXT("шоколад")) ? 7 : TN.Contains(TEXT("сыр")) ? 8 : 255);
		Note(FString::Printf(TEXT("%s — целый трофей «%s»: %s"), *Clock(), *Food->LootName, *Names));
	}
	Value = FMath::RoundToInt(Value * TierDef().LootMul * AntColony::LootBonus(Nest));
	if (Food->Special != 0)
	{
		++SpecialsHome;
		SpecialsLeft = (uint8)FMath::Max(0, (int32)SpecialsLeft - 1);
		FString What;
		AAntGameState* S = GS();
		switch (Food->Special)
		{
		case Find::Jelly: RaidJelly += 2; What = TEXT("+2 молочка"); break;
		case Find::Larva: if (S) { S->Larvae = FMath::Min(AntNest::MaxLarvae, S->Larvae + 1); } What = TEXT("+1 личинка, прямо сейчас"); break;
		case Find::Gift: MoodGift += 8; What = TEXT("подарок Королеве: она подобреет"); break;
		default: What = FString::Printf(TEXT("+%d еды"), Value); break;
		}
		Say(FString::Printf(TEXT("ОСОБАЯ НАХОДКА ДОМА: %s — %s"), *Food->LootName, *What), RaidGold);
		Note(FString::Printf(TEXT("%s — особая находка «%s»: %s"), *Clock(), *Food->LootName, *Names));
		UE_LOG(LogTemp, Display, TEXT("ANTRAID special find home: %d «%s» (%s) by %s"), (int32)Food->Special, *Food->LootName, *What, *Names);
		RaidTrophies.AddUnique(9);
	}
	RaidKinds[Kind] += Value;
	Credit(Value, Food->bTrophy ? TEXT("trophy_home") : Food->Special != 0 ? TEXT("special_home") : nullptr);
	return false;
}

void AAntRaid::OnBodyHome(AController* BodyOwner, const FString& Name, const TArray<AAntCharacter*>& Carriers)
{
	FString Names;
	for (const AAntCharacter* A : Carriers)
	{
		if (A) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); }
	}
	Note(FString::Printf(TEXT("%s — %s донёс тело до гнезда: %s спасён"), *Clock(), Names.IsEmpty() ? TEXT("кто-то") : *Names, *Name));
	AntSquad::OnBodyHome(BodyOwner);   // what is left of his squad is his again
	for (const AAntCharacter* A : Carriers) { if (A) { LineOf(A).Rescues++; } }
}

void AAntRaid::OnPlayerDied(AAntCharacter* Ant, const FString& How)
{
	if (!InRaid() || !Ant)
	{
		return;
	}
	++Deaths;
	LineOf(Ant).Deaths++;
	AntColony::AddFallen(Nest, Ant->GetDisplayName(), RaidNo, How);
	AntSquad::OnCommanderDied(Ant);   // half of his squad scatter, the rest wait by the body
	AntRts::NoteOwnerDied(Ant);       // he comes back as one of his squad ants near here, if one is alive (AntRts::RespawnInSquad)
	const FString Place = PlaceName(Ant->GetActorLocation());
	Note(FString::Printf(TEXT("%s — %s %s%s"), *Clock(), *Ant->GetDisplayName(), *How, Place.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Place)));
}

int32 AAntRaid::ContractBonus(uint8 OrderId)
{
	RaidJelly += 1;
	Note(FString::Printf(TEXT("%s — контракт Королевы: %s"), *Clock(), AntOrder::Name(OrderId)));
	return AntColony::OrderFood(Nest, FMath::RoundToInt(40.f * TierDef().LootMul));
}

// ---------------------------------------------------------------------------------------------
// The end of a raid
// ---------------------------------------------------------------------------------------------

void AAntRaid::EndRaid(uint8 How)
{
	AAntGameState* S = GS();
	if (!InRaid() || !GM || !S || How == AntRaidEnd::None)
	{
		return;
	}
	Phase = AntRaidPhase::Results;   // first: what follows may kill ants, and their deaths must not end the raid again
	AntFighter::OnRaidEnding(this, How);   // which squad ants (veterans) are alive, before they go (AntFighter.h)
	AntSnowball::OnRaidEnd(this);   // squad ants and trails go; the story gets each player's snowball
	int32 Home = 0;
	FString Away;
	const int32 Living = LivingPlayers(&Home, &Away);
	int32 Lost = 0;
	if (How == AntRaidEnd::Timeout)
	{
		// Whoever is still outside is lost with what he carries.
		TArray<AAntCharacter*> Out;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			AAntCharacter* A = It->IsValid() ? Cast<AAntCharacter>((*It)->GetPawn()) : nullptr;
			if (A && !A->bDead && A->Team == 0 && A->Creature == 0 && !IsHome(A)) { Out.Add(A); }
		}
		Lost = Out.Num();
		for (AAntCharacter* A : Out)
		{
			Story.Add(FString::Printf(TEXT("%s — %s не успел домой (%s)"), *Clock(), *A->GetDisplayName(), *PlaceName(A->GetActorLocation())));
			A->HurtBy(100000.f, TEXT("не успел домой: остался в квартире"));
		}
		if (Home == 0 || Living == 0)
		{
			How = AntRaidEnd::Wiped;   // nobody made it
		}
	}
	GHillOpen = false;
	if (Clan) { Clan->Stop(); }
	GM->RunEndShift(How != AntRaidEnd::Wiped);   // contracts decided at the end, the awards
	const int32 SquadOut = Result.SquadOut;
	const int32 SquadBack = AntSquad::CountBack(this, How == AntRaidEnd::Wiped, How == AntRaidEnd::Timeout);
	Nest.Workers = FMath::Min(AntNest::MaxWorkers, Nest.Workers + SquadBack);
	// A pocket nobody opened goes back into the stores, if its owner came home.
	int32 Refund = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* P = It->Get();
		AAntPlayerState* PS = P ? P->GetPlayerState<AAntPlayerState>() : nullptr;
		const AAntCharacter* A = P ? Cast<AAntCharacter>(P->GetPawn()) : nullptr;
		if (!PS) { continue; }
		if (How != AntRaidEnd::Wiped && A && !A->bDead && IsHome(A))
		{
			if (PS->Tool0 != AntTool::None && PS->Charges0 >= AntTool::MaxCharges) { Refund += AntTool::Cost(PS->Tool0); }
			if (PS->Tool1 != AntTool::None && PS->Charges1 >= AntTool::MaxCharges) { Refund += AntTool::Cost(PS->Tool1); }
		}
		PS->Charges0 = 0;
		PS->Charges1 = 0;
	}
	if (Refund > 0)
	{
		Nest.Food += Refund;
		UE_LOG(LogTemp, Display, TEXT("ANTNEST untouched tools are back in the stores: +%d food"), Refund);
	}
	BuildResult(How);
	Result.SquadOut = SquadOut;
	Result.SquadBack = SquadBack;
	UE_LOG(LogTemp, Display, TEXT("ANTSQUAD raid over: %d of %d workers came home, the nest has %d"), SquadBack, SquadOut, Nest.Workers);
	SaveNest();
	Result.Lost = Lost;
	S->Phase = AntRaidGS::Results;
	PhaseEnds = S->GetServerWorldTimeSeconds() + (FastScreens() ? 7.f : 18.f);
	ExitEnds = 0.f;
	ExitOk.Reset();
	GM->SetHumanAwake(false);
	GM->SetHumanGoal(AntHumanGoal::Idle, FString(), FVector::ZeroVector);
	Say(How == AntRaidEnd::Left ? FString::Printf(TEXT("РЕЙД %d ОКОНЧЕН: ушли с добычей %d"), RaidNo, Result.Kept)
		: How == AntRaidEnd::Timeout ? FString::Printf(TEXT("ВРЕМЯ ВЫШЛО. Дома осталось: %d, пропало: %d"), Home, Lost)
		: FString::Printf(TEXT("ОТРЯД ПОГИБ. Красные растащили половину добычи: в кладовой %d из %d"), Result.Kept, Result.Banked), How == AntRaidEnd::Left ? RaidGold : RaidBad);
}

void AAntRaid::BuildResult(uint8 How)
{
	AAntGameState* S = GS();
	const AntTier::FDef& T = TierDef();
	FAntRaidResult R;
	R.End = How;
	R.RaidNo = RaidNo;
	if (LevelV3::Enabled() && GS()) { R.Seed = GS()->LevelSeed(); R.SeedText = LevelV3::ShortText(); }
	R.Tier = Tier;
	R.Banked = S->TeamScore;
	const bool bWiped = How == AntRaidEnd::Wiped;
	R.Kept = bWiped ? FMath::RoundToInt(R.Banked * AntColony::WipeKeep(Nest)) : R.Banked;
	AntColony::Sync(Nest);
	R.StockBefore = Nest.Stock;
	R.JellyBefore = Nest.Jelly;
	R.Ask = S->TargetScore;
	R.Jelly = bWiped ? 0 : RaidJelly;
	R.Cake = Cake;
	R.Deaths = Deaths;
	R.ContractsTaken = S->Orders.Num();
	for (int32 i = 0; i < S->Orders.Num(); ++i) { R.ContractsDone += (S->OrdersDone & (1 << i)) ? 1 : 0; }
	R.MoodBefore = Nest.Mood;

	// The Queen: variety and quality of what came home, and what it cost.
	int32 Mood = 0;
	const bool bCakeHome = Cake == AntCake::Home && !bWiped;
	Mood += bCakeHome ? 12 : (Cake == AntCake::Eaten ? -8 : -4);
	Mood += R.Kept >= R.Ask * 3 / 2 ? 8 : R.Kept >= R.Ask ? 5 : R.Kept * 2 < R.Ask ? -4 : 0;
	Mood += R.ContractsDone * 4;
	Mood += Variety.Num() >= 10 ? 6 : Variety.Num() >= 6 ? 3 : 0;
	Mood += TrophiesHome * 2 + SpecialsHome * 2 + (bWiped ? 0 : MoodGift);
	Mood -= AntColony::DeathMood(Nest, Deaths);
	Mood -= bWiped ? 14 : 0;
	Mood += AntColony::MoodPerRaid(Nest);
	Nest.Mood = FMath::Clamp(Nest.Mood + Mood, 0, 100);
	R.MoodAfter = Nest.Mood;

	// The stores, by kind: what the carriers brought, the cake (sugar), the rest (orders, refunds, untracked) as crumbs.
	{
		int32 K[AntFoodKind::Count];
		FMemory::Memcpy(K, RaidKinds, sizeof(K));
		if (Cake == AntCake::Home) { K[AntFoodKind::Sugar] += FMath::Max(0, R.Banked - K[0] - K[1] - K[2] - K[3]); }
		int32 Tracked = K[0] + K[1] + K[2] + K[3];
		if (Tracked < R.Banked) { K[AntFoodKind::Crumbs] += R.Banked - Tracked; Tracked = R.Banked; }
		R.Kinds.SetNumZeroed(AntFoodKind::Count);
		int32 Given = 0;
		for (int32 k = 0; k < AntFoodKind::Count; ++k) { R.Kinds[k] = Tracked > 0 ? (int32)((int64)K[k] * R.Kept / Tracked) : 0; Given += R.Kinds[k]; }
		R.Kinds[AntFoodKind::Crumbs] += R.Kept - Given;
		for (int32 k = 0; k < AntFoodKind::Count; ++k) { AntColony::AddFood(Nest, (uint8)k, R.Kinds[k]); }
	}
	Nest.Jelly += R.Jelly;
	Nest.Larvae = FMath::Clamp(S->Larvae, 0, AntNest::MaxLarvae);
	R.LarvaeLeft = Nest.Larvae;
	Nest.Raids = RaidNo;
	Nest.Wipes += bWiped ? 1 : 0;
	Nest.BestHaul = FMath::Max(Nest.BestHaul, R.Kept);
	// What the Queen does about it.
	const int32 Band = AntNest::MoodBand(Nest.Mood);
	R.LarvaeLaid = (Band >= 3 ? 2 : Band >= 2 ? 1 : 0) + (Nest.Tier_(AntUp::Brood) >= 3 ? 1 : 0);
	R.WorkersHatched = Band >= 3 ? 6 : Band >= 2 ? 3 : 1;
	if (Band == 0)
	{
		R.QueenAte = Nest.Food * 15 / 100;
		Nest.Food -= R.QueenAte;
	}
	R.LarvaeLaid += (Nest.Own_(AntColony::OwnMemorial) >= 3 && Deaths > 0) ? 1 : 0;   // the guard of honour
	if (Nest.Larvae + R.LarvaeLaid < 1) { R.LarvaeLaid = 1; }   // never a dead end
	Nest.Larvae = FMath::Min(AntNest::MaxLarvae, Nest.Larvae + R.LarvaeLaid);
	Nest.Workers = FMath::Min(AntNest::MaxWorkers, Nest.Workers + R.WorkersHatched);
	if (Nest.Workers < 6) { R.WorkersHatched += 6 - Nest.Workers; Nest.Workers = 6; }
	// A new threat tier.
	if (bCakeHome)
	{
		Nest.CakesHome++;
		if (Tier >= Nest.MaxTier && Nest.MaxTier < AntTier::Max && ++Nest.CakesAtMax >= AntTier::CakesToUnlock(Nest.MaxTier))
		{
			Nest.MaxTier++;
			Nest.CakesAtMax = 0;
			R.NewTier = Nest.MaxTier;
		}
	}
	// The story of the raid: what was noted on the way, the last lines about how it ended.
	R.Story = Story;
	if (How == AntRaidEnd::Left) { R.Story.Add(FString::Printf(TEXT("%s — все дома, вход запечатан"), *Clock())); }
	else if (How == AntRaidEnd::Wiped) { R.Story.Add(FString::Printf(TEXT("%s — живых не осталось. Красные вынесли из кладовой %d"), *Clock(), R.Banked - R.Kept)); }
	else { R.Story.Add(FString::Printf(TEXT("%s — время вышло, вход запечатан"), *Clock())); }
	while (R.Story.Num() > 9) { R.Story.RemoveAt(R.Story.Num() / 2); }   // keep the beginning and the end
	ColonyAfterRaid(R, How);
	Result = R;
	UE_LOG(LogTemp, Display, TEXT("ANTRAID end %d: how %d, banked %d kept %d ask %d, jelly %d, cake %d, deaths %d, contracts %d/%d, variety %d, mood %d -> %d, larvae left %d +%d, workers +%d, queen ate %d, new tier %d"),
		RaidNo, (int32)How, R.Banked, R.Kept, R.Ask, R.Jelly, (int32)R.Cake, R.Deaths, R.ContractsDone, R.ContractsTaken, Variety.Num(), R.MoodBefore, R.MoodAfter, R.LarvaeLeft, R.LarvaeLaid,
		R.WorkersHatched, R.QueenAte, R.NewTier);
	SaveNest();
	// The meta bus (AntMetaBus.h): ColonyAfterRaid above fires OnRaidSummary exactly once (v14: the fighter's fallback fire is gone).
}

void AAntRaid::PhaseSecond()
{
	const AAntGameState* S = GS();
	if (!S || !GM)
	{
		return;
	}
	const float Now = S->GetServerWorldTimeSeconds();
	if (Phase == AntRaidPhase::Results && PhaseEnds > 0.f && Now >= PhaseEnds)
	{
		EnterNest(false);
	}
	else if (Phase == AntRaidPhase::Nest && StartEnds > 0.f && Now >= StartEnds)
	{
		StartRaid();
	}
}

// ---------------------------------------------------------------------------------------------
// What players ask for
// ---------------------------------------------------------------------------------------------

void AAntRaid::PlayerCommand(AAntPlayerController* PC, uint8 Cmd, int32 A, int32 B)
{
	AAntGameState* S = GS();
	if (!PC || !S || !GM)
	{
		return;
	}
	AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>();
	const bool bHost = PC->IsLocalController();
	const FString Name = PS ? PS->GetPlayerName() : FString(TEXT("?"));
	const float Now = S->GetServerWorldTimeSeconds();
	switch (Cmd)
	{
	case AntRaidCmd::Leave:
	{
		const AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
		if (!InRaid() || !Ant || Ant->bDead)
		{
			return;
		}
		if (!IsHome(Ant))
		{
			if (ExitEnds > 0.f) { CancelExit(FString::Printf(TEXT("%s: рано уходить! Уход отменён"), *Name)); }
			else { PC->ClientToast(TEXT("Уйти можно только у входа в гнездо")); }
			return;
		}
		if (ExitEnds <= 0.f)
		{
			const int32 Living = LivingPlayers();
			ExitEnds = Now + (Living <= 1 ? 4.f : 15.f);
			ExitBy = Name;
			ExitStarter = PC;
			ExitOk.Reset();
			ExitOk.Add(PC);
			if (Living > 1) { Say(FString::Printf(TEXT("%s зовёт домой: уходим через 15 с. [L] у входа — согласен, [L] снаружи — рано"), *Name), RaidGold); }
			UE_LOG(LogTemp, Display, TEXT("ANTRAID leaving started by %s, living %d"), *Name, Living);
		}
		else
		{
			ExitOk.Add(PC);
		}
		break;
	}
	case AntRaidCmd::Ready:
		if (Phase == AntRaidPhase::Nest && PS) { PS->bNestReady = A != 0; }
		break;
	case AntRaidCmd::Start:
		if (Phase == AntRaidPhase::Nest && bHost)
		{
			if (StartEnds > 0.f)
			{
				StartEnds = 0.f;   // pressed again: not yet
				break;
			}
			bool bAll = true;
			int32 Players = 0;
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				const AAntPlayerState* P = It->IsValid() ? (*It)->GetPlayerState<AAntPlayerState>() : nullptr;
				if (P) { ++Players; bAll &= P->bNestReady || It->Get() == PC; }
			}
			if (Players <= 1) { StartRaid(); }
			else
			{
				StartEnds = Now + (bAll ? 3.f : 20.f);
				Say(bAll ? TEXT("Все готовы: выходим!") : TEXT("Хост начинает рейд через 20 с. Кто не готов — пойдёт с тем, что выбрал"), RaidGold);
			}
		}
		break;
	case AntRaidCmd::Skip:
		if (Phase == AntRaidPhase::Results && bHost) { PhaseEnds = Now; }
		break;

	// ---- the nest screen. The colony is the host's: he builds and decides; everybody chooses his own squad and pockets.
	case AntRaidCmd::Squad:
		if (Phase == AntRaidPhase::Nest && PS) { PS->SquadWant = (uint8)FMath::Clamp(A, 0, AntNest::SquadCap(Nest)); }
		break;
	case AntRaidCmd::Tool:
		if (Phase == AntRaidPhase::Nest && PS && A >= 0 && A < AntTool::Pockets && (B == AntTool::None || AntNest::ToolUnlocked(Nest, (uint8)FMath::Clamp(B, 0, 255))))
		{
			(A == 0 ? PS->Tool0 : PS->Tool1) = (uint8)B;
			CountStartCost();
		}
		break;
	case AntRaidCmd::Seed:
		// Level v3, the host in the nest: "play seed N" (A >= 0) or a new random one (A < 0).
		if (Phase == AntRaidPhase::Nest && bHost && LevelV3::Enabled())
		{
			PickLevelSeed(A);
			Say(FString::Printf(TEXT("Квартира на этот рейд: %s"), *LevelV3::ShortText()), RaidInfo);
		}
		else if (!bHost)
		{
			PC->ClientToast(TEXT("Сид выбирает хост"));
		}
		break;
	case AntRaidCmd::Buy:
	case AntRaidCmd::Mod:
	case AntRaidCmd::Dig:
	case AntRaidCmd::Chamber:
	case AntRaidCmd::Tier:
	case AntRaidCmd::Contract:
	case AntRaidCmd::Larva:
	case AntRaidCmd::Workers:
	case AntRaidCmd::NewColony:
	{
		if (Phase != AntRaidPhase::Nest)
		{
			break;
		}
		if (!bHost)
		{
			PC->ClientToast(TEXT("Колония хоста: строит и решает он. Твоё — отряд и карманы"));
			break;
		}
		if (Cmd == AntRaidCmd::Buy)
		{
			FString Why;
			if (A >= 0 && A < AntUp::Count && AntNest::CanBuy(Nest, (uint8)A, &Why))
			{
				AntNest::Buy(Nest, (uint8)A);
				const AntUp::FDef& D = AntUp::Def((uint8)A, Nest.Tier_((uint8)A));
				Say(FString::Printf(TEXT("ПОСТРОЕНО: %s %d — «%s». %s"), AntUp::BranchName((uint8)A), (int32)Nest.Tier_((uint8)A), D.Name, D.Text), RaidGold);
				UE_LOG(LogTemp, Display, TEXT("ANTNEST built branch %d tier %d «%s»: food %d jelly %d left"), A, (int32)Nest.Tier_((uint8)A), D.Name, Nest.Food, Nest.Jelly);
				const uint8 Room = AntColony::ChamberOfBranch((uint8)A);
				AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: построено — %s %d «%s»"), Nest.Raids, AntColony::Def(Room).Name, (int32)Nest.Tier_((uint8)A), D.Name));
				SaveNest();
				RefreshVetDraftCap(GetWorld());
			}
			else
			{
				PC->ClientToast(FString::Printf(TEXT("Не построить: %s"), *Why));
			}
		}
		else if (Cmd == AntRaidCmd::Dig)
		{
			FString Why;
			if (AntColony::CanDig(Nest, (uint8)FMath::Clamp(A, 0, 255), &Why))
			{
				AntColony::DoDig(Nest, (uint8)A);
				const AntColony::FDigDef& D = AntColony::Dig((uint8)A);
				AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: прорыт %s — %s. %s"), Nest.Raids, D.Name, D.Toward, D.Text));
				Say(FString::Printf(TEXT("ПРОРЫТ %s (%s): %s"), D.Name, D.Toward, D.Text), RaidGold);
				UE_LOG(LogTemp, Display, TEXT("ANTCOLONY dug %d %s: food %d jelly %d left"), A, D.Name, Nest.Food, Nest.Jelly);
				SaveNest();
			}
			else
			{
				PC->ClientToast(FString::Printf(TEXT("Не прорыть: %s"), *Why));
			}
		}
		else if (Cmd == AntRaidCmd::Chamber)
		{
			FString Why;
			const uint8 C = (uint8)FMath::Clamp(A, 0, 255);
			if (C < AntColony::Count && AntColony::CanUpgrade(Nest, C, &Why))
			{
				AntColony::Upgrade(Nest, C);
				const int32 T = AntColony::TierOf(Nest, C);
				AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: построено — %s %d «%s»"), Nest.Raids, AntColony::Def(C).Name, T, *AntColony::TierName(C, T)));
				Say(FString::Printf(TEXT("ПОСТРОЕНО: %s %d — «%s». %s"), AntColony::Def(C).Name, T, *AntColony::TierName(C, T), *AntColony::TierText(C, T)), RaidGold);
				UE_LOG(LogTemp, Display, TEXT("ANTCOLONY built %s tier %d: food %d jelly %d left, level %d"), AntColony::Def(C).Id, T, Nest.Food, Nest.Jelly, AntColony::Level(Nest));
				SaveNest();
				RefreshVetDraftCap(GetWorld());
			}
			else
			{
				PC->ClientToast(FString::Printf(TEXT("Не построить: %s"), *Why));
			}
		}
		else if (Cmd == AntRaidCmd::Mod)
		{
			if (A >= 0 && A < AntMod::Count) { Mods ^= (uint8)(1 << A); }
		}
		else if (Cmd == AntRaidCmd::Tier)
		{
			if (AntTut::IsDemo() && A > 1) { Say(TEXT("Угрозы 2–4 — в полной версии игры"), RaidInfo); }
			else if (A >= 1 && A <= Nest.MaxTier) { Nest.Tier = A; Tier = (uint8)A; SaveNest(); PickLevelSeed(S->LevelSeed()); }
			else { PC->ClientToast(TEXT("Эта угроза ещё закрыта: принесите торт домой на предыдущей")); }
		}
		else if (Cmd == AntRaidCmd::Contract)
		{
			if (ContractOffer.IsValidIndex(A))
			{
				const uint8 Bit = (uint8)(1 << A);
				if (ContractPick & Bit) { ContractPick &= ~Bit; }
				else if (FMath::CountBits((uint64)ContractPick) < 2) { ContractPick |= Bit; }
				else { PC->ClientToast(TEXT("Не больше двух приказов на рейд")); }
			}
		}
		else if (Cmd == AntRaidCmd::Larva)
		{
			if (Nest.Larvae >= AntNest::MaxLarvae) { PC->ClientToast(TEXT("Камера полна: больше личинок не вырастить")); }
			else if (Nest.Food < AntNest::LarvaCost) { PC->ClientToast(FString::Printf(TEXT("На личинку нужно %d еды"), AntNest::LarvaCost)); }
			else { Nest.Food -= AntNest::LarvaCost; ++Nest.Larvae; SaveNest(); }
		}
		else if (Cmd == AntRaidCmd::Workers)
		{
			if (Nest.Workers >= AntNest::MaxWorkers) { PC->ClientToast(TEXT("В гнезде нет места для новых рабочих")); }
			else if (Nest.Food < AntNest::WorkerPackCost) { PC->ClientToast(FString::Printf(TEXT("На рабочих нужно %d еды"), AntNest::WorkerPackCost)); }
			else { Nest.Food -= AntNest::WorkerPackCost; Nest.Workers = FMath::Min(AntNest::MaxWorkers, Nest.Workers + AntNest::WorkerPack); SaveNest(); }
		}
		else if (Cmd == AntRaidCmd::NewColony && A == 7777)
		{
			AntNest::Reset();   // the old colony is kept aside as colony.old
			AntNest::Fresh(Nest);
			AntColony::Chronicle(Nest, TEXT("Основана Великая Колония Щели. Королева-Мать заняла земляной трон"));
			Result = FAntRaidResult();
			NestNote = TEXT("Новая колония. Старая лежит рядом с сохранением как colony.old");
			SaveNest();
			EnterNest(true);
			for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It) { It->Tool0 = It->Tool1 = 0; It->SquadWant = 255; }
			Say(TEXT("НОВАЯ КОЛОНИЯ: всё с начала"), RaidGold);
			UE_LOG(LogTemp, Display, TEXT("ANTNEST a new colony was started from the nest screen"));
		}
		CountStartCost();
		break;
	}
	default:
		break;
	}
}

void AAntRaid::UseTool(AAntPlayerController* PC, int32 Pocket, const FVector& Aim)
{
	AAntPlayerState* PS = PC ? PC->GetPlayerState<AAntPlayerState>() : nullptr;
	AAntCharacter* Ant = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	if (!InRaid() || !GM || !PS || !Ant || Ant->bDead || Ant->Team != 0 || Pocket < 0 || Pocket >= AntTool::Pockets)
	{
		return;
	}
	const uint8 Tool = Pocket == 0 ? PS->Tool0 : PS->Tool1;
	uint8& Charges = Pocket == 0 ? PS->Charges0 : PS->Charges1;
	if (Tool == AntTool::None || Charges == 0)
	{
		PC->ClientToast(TEXT("Карман пуст"));
		return;
	}
	// Thrown no further than an ant can spit.
	const FVector From = Ant->GetActorLocation();
	const FVector At = From + (Aim - From).GetClampedToMaxSize(2500.f);
	bool bUsed = false;
	switch (Tool)
	{
	case AntTool::Bait:
		if (AKitchenLoot* L = GM->SpawnFood(At + FVector(0.f, 0.f, 25.f), 0))
		{
			L->LootName = TEXT("Крошка-приманка");
			L->Value = 1;
			L->ForceNetUpdate();
			bool bDog = false;
			for (TActorIterator<ACritter> It(GetWorld()); It; ++It)
			{
				if (It->Kind == 0) { It->BaitWith(L); bDog = true; }
			}
			PC->ClientToast(bDog ? TEXT("Приманка брошена: Бадрик учует её, если он рядом") : TEXT("Приманка брошена, но Бадрика в доме нет"));
			bUsed = true;
		}
		break;
	case AntTool::Alarm:
		bUsed = AntSquad::Recall(Ant);
		PC->ClientToast(bUsed ? TEXT("Тревожный запах: отряд бросил всё и рядом с тобой") : TEXT("Отряда нет: звать некого"));
		break;
	case AntTool::Decoy:
		if (Clan)
		{
			Clan->Decoy(At, 35.f);
			PC->ClientToast(TEXT("Обманка: засада красных уйдёт сторожить это место"));
			bUsed = true;
		}
		else { PC->ClientToast(TEXT("Красных в этом рейде нет")); }
		break;
	case AntTool::Noise:
		GM->MakeNoise(At, 0.95f, AntNoise::Crash, nullptr, nullptr);
		PC->ClientToast(TEXT("Шумелка трещит: Человек пойдёт посмотреть"));
		bUsed = true;
		break;
	default:
		break;
	}
	if (bUsed)
	{
		--Charges;
		AntSfx::Everyone(this, AntSfx::Drop, At, 1.3f);
		UE_LOG(LogTemp, Display, TEXT("ANTRAID tool %d «%s» used by %s at %s, %d left in pocket %d"), (int32)Tool, AntTool::Name(Tool), *Ant->GetDisplayName(), *At.ToCompactString(), (int32)Charges, Pocket);
	}
}

// ---------------------------------------------------------------------------------------------
// Dev commands (-AntDo)
// ---------------------------------------------------------------------------------------------

bool AAntRaid::DevCommand(const TArray<FString>& A)
{
	if (A.Num() < 2 || !GM)
	{
		return false;
	}
	if (Clan && Clan->DevCommand(A))
	{
		return true;   // clan ...
	}
	const FString Op = A[0].ToLower(), Sub = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	AAntGameState* S = GS();
	AAntPlayerController* PC = Cast<AAntPlayerController>(GetWorld()->GetFirstPlayerController());
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	if (Op == TEXT("seed"))
	{
		// level v3: seed N | seed random | seed info. A new seed in the middle of a raid re-lays the flat at once (doors, things); loot follows at the next reset.
		if (Sub == TEXT("info")) { UE_LOG(LogTemp, Display, TEXT("%s | %s"), *LevelV3::Describe(), *LevelV3::ShortText()); }
		else { PickLevelSeed(Sub == TEXT("random") ? -1 : FCString::Atoi(*Sub)); }
		return true;
	}
	if (Op == TEXT("fastener") || Op == TEXT("fix") || Op == TEXT("break") || Op == TEXT("chain") || Op == TEXT("state"))
	{
		return DevFixture(A);
	}
	if (Op == TEXT("raid"))
	{
		if (Sub == TEXT("start")) { StartRaid(); }
		else if (Sub == TEXT("end")) { EndRaid(A.Num() > 2 && A[2] == TEXT("wipe") ? AntRaidEnd::Wiped : A.Num() > 2 && A[2] == TEXT("time") ? AntRaidEnd::Timeout : AntRaidEnd::Left); }
		else if (Sub == TEXT("leave") && PC) { PlayerCommand(PC, AntRaidCmd::Leave, 0, 0); }
		else if (Sub == TEXT("ready") && PC) { PlayerCommand(PC, AntRaidCmd::Ready, 1, 0); }
		else if (Sub == TEXT("go") && PC) { PlayerCommand(PC, AntRaidCmd::Start, 0, 0); }
		else if (Sub == TEXT("skip") && PC) { PlayerCommand(PC, AntRaidCmd::Skip, 0, 0); }
		else if (Sub == TEXT("carryto") && Me && Me->Carrying && A.Num() >= 5)
		{
			// the ant and what it holds are put somewhere (a test's way past a climb the scripted walker cannot do)
			const FVector To(Num(2), Num(3), Num(4));
			const FVector Rel = Me->GetActorLocation() - Me->Carrying->GetActorLocation();
			Me->Carrying->SetActorLocation(To + FVector(0.f, 0.f, 80.f), false, nullptr, ETeleportType::TeleportPhysics);
			Me->ApplyTeleport(To + FVector(Rel.X, Rel.Y, 6.f));
		}
		else if (Sub == TEXT("tool") && PC && Me) { UseTool(PC, (int32)Num(2), A.Num() >= 6 ? FVector(Num(3), Num(4), Num(5)) : Me->GetActorLocation() + Me->GetActorForwardVector() * 300.f); }
		else if (Sub == TEXT("time") && S) { S->TimeRemaining = Num(2, 60.f); }
		else if (Sub == TEXT("home") && Me) { Me->ApplyTeleport((Num(2) > 0.5f ? LevelV2::SouthEntrance() : AKitchenBuilder::NestLocation(0) + FVector(300.f, 0.f, 0.f)) + FVector(0.f, 0.f, 40.f)); }
		else if (Sub == TEXT("cake") && Me && GM->MainObject.IsValid()) { Me->ApplyTeleport(GM->MainObject->GetActorLocation() + FVector(70.f, 0.f, 30.f)); }
		else if (Sub == TEXT("info"))
		{
			int32 Home = 0;
			const int32 Living = LivingPlayers(&Home);
			UE_LOG(LogTemp, Display, TEXT("ANTRAID info: phase %d gsphase %d raid %d tier %d left %.0f stage %d banked %d/%d larvae %d jelly %d cake %d at %s known %d exit %.0f home %d/%d greed %.1f npc d%d c%d g%d k%d me %s home %d"),
				(int32)Phase, S ? (int32)S->Phase : -1, RaidNo, (int32)Tier, S ? S->TimeRemaining : 0.f, S ? (int32)S->ShiftStage : -1, S ? S->TeamScore : 0, S ? S->TargetScore : 0, S ? S->Larvae : 0, RaidJelly,
				(int32)Cake, *FVector(CakeLoc).ToCompactString(), bCakeKnown ? 1 : 0, ExitEnds, Home, Living, S ? S->Greed : 0.f,
				NpcLevel(NpcWho::Dog), NpcLevel(NpcWho::Cat), NpcLevel(NpcWho::Grandma), NpcLevel(NpcWho::Kid), Me ? *Me->GetActorLocation().ToCompactString() : TEXT("-"), IsHome(Me) ? 1 : 0);
			for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTRAID info: army team %d count %d mode %d at %s, commander %s, %.0f from me"), (int32)It->Team, It->Count, (int32)It->Mode, *It->GetActorLocation().ToCompactString(),
					It->Commander ? *It->Commander->GetDisplayName() : TEXT("-"), Me ? FVector::Dist(Me->GetActorLocation(), It->GetActorLocation()) : -1.f);
			}
		}
		else { return false; }
		return true;
	}
	if (Op == TEXT("squad") && Me)
	{
		// squad follow|attack|free|haul|hold [x y z] (no point: at the nearest loot for haul, at the ant's feet otherwise); squad info; squad recall; squad kill N
		FVector At = A.Num() >= 5 ? FVector(Num(2), Num(3), Num(4)) : Me->GetActorLocation();
		AAntArmy* Sq = AntSquad::Find(Me);
		if (Sub == TEXT("follow")) { AntSquad::Order(Me, AntSquad::Follow, At); }
		else if (Sub == TEXT("attack")) { AntSquad::Order(Me, AntSquad::Attack, At); }
		else if (Sub == TEXT("free")) { AntSquad::Order(Me, AntSquad::Free, At); }
		else if (Sub == TEXT("hold")) { AntSquad::Order(Me, AntSquad::Hold, At); }
		else if (Sub == TEXT("haul"))
		{
			if (A.Num() < 5)
			{
				if (AKitchenLoot* L = Me->FindNearestLoot(3000.f)) { At = L->GetActorLocation(); }
			}
			AntSquad::Order(Me, AntSquad::Haul, At);
		}
		else if (Sub == TEXT("cake") && GM->MainObject.IsValid()) { AntSquad::Order(Me, AntSquad::Haul, GM->MainObject->GetActorLocation()); }
		else if (Sub == TEXT("recall")) { AntSquad::Recall(Me); }
		else if (Sub == TEXT("bridge") && Sq && A.Num() >= 5)
		{
			// what [G] does with a squad: a bridge from the ant's feet to a point
			const AAntChain* Ch = Sq->BuildBridge(Me->GetActorLocation(), FVector(Num(2), Num(3), Num(4)));
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD dev bridge to %s: %s, squad now %d"), *FVector(Num(2), Num(3), Num(4)).ToCompactString(), Ch ? TEXT("built") : TEXT("refused"), Sq->Count);
		}
		else if (Sub == TEXT("kill") && Sq) { Sq->Lose((int32)Num(2, 1.f)); }
		else if (Sub == TEXT("probe") && A.Num() >= 6)
		{
			// squad probe x0 y0 x1 y1 [ztop]: every solid thing in that floor box (to see what a squad ant gets caught on)
			const FVector Lo(FMath::Min(Num(2), Num(4)), FMath::Min(Num(3), Num(5)), -50.f);
			const FVector Hi(FMath::Max(Num(2), Num(4)), FMath::Max(Num(3), Num(5)), Num(6, 400.f));
			TArray<FOverlapResult> Ov;
			FCollisionObjectQueryParams Obj(FCollisionObjectQueryParams::AllObjects);
			GetWorld()->OverlapMultiByObjectType(Ov, (Lo + Hi) * 0.5f, FQuat::Identity, Obj, FCollisionShape::MakeBox((Hi - Lo) * 0.5f));
			for (const FOverlapResult& O : Ov)
			{
				const UPrimitiveComponent* C = O.GetComponent();
				if (!C || Cast<APawn>(O.GetActor())) { continue; }
				const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
				const FBox B = C->Bounds.GetBox();
				UE_LOG(LogTemp, Display, TEXT("ANTSQUAD probe: %s / %s mesh %s profile %s type %d enabled %d mobility %d pawn-block %d box (%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)"), *GetNameSafe(O.GetActor()), *C->GetName(),
					SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : TEXT("-"), *C->GetCollisionProfileName().ToString(), (int32)C->GetCollisionObjectType(), (int32)C->GetCollisionEnabled(),
					(int32)C->Mobility, C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block ? 1 : 0, B.Min.X, B.Min.Y, B.Min.Z, B.Max.X, B.Max.Y, B.Max.Z);
			}
			UE_LOG(LogTemp, Display, TEXT("ANTSQUAD probe: %d hits in %s-%s"), Ov.Num(), *Lo.ToCompactString(), *Hi.ToCompactString());
		}
		else if (Sub == TEXT("info"))
		{
			for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It)
			{
				if (!It->bSquad) { continue; }
				UE_LOG(LogTemp, Display, TEXT("ANTSQUAD info: count %d of %d, mode %d, strength %.0f, at %s (%.0f from me), commander %s, orphan %d, foraging %d, cargo %s held %d home %d way %d/%d, lead %d, catch-up %d, swarming %d"),
					It->Count, It->Taken, (int32)It->Mode, It->Strength(), *It->GetActorLocation().ToCompactString(), FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()),
					It->Commander ? *It->Commander->GetDisplayName() : TEXT("-"), It->bOrphan ? 1 : 0, It->bForaging ? 1 : 0, It->Cargo ? *It->Cargo->LootName : TEXT("-"), It->bCargoHeld ? 1 : 0, It->bCargoHome ? 1 : 0,
					It->WayAt, It->Way.Num(), It->Lead.Num(), It->bCatchUp ? 1 : 0, It->bSwarming ? 1 : 0);
			}
			// The squad ants themselves (AntRts): how far each is from me, to see a straggler in a test log.
			for (TActorIterator<AAntSquadBot> It(GetWorld()); It; ++It)
			{
				if (const APawn* Pw = It->GetPawn())
				{
					UE_LOG(LogTemp, Display, TEXT("ANTSQUAD info: squad ant %s at %s, %.0f from me"), *Pw->GetName(), *Pw->GetActorLocation().ToCompactString(), FVector::Dist(Pw->GetActorLocation(), Me->GetActorLocation()));
				}
			}
		}
		else { return false; }
		return true;
	}
	if (Op == TEXT("colony"))
	{
		return ColonyCommand(A);
	}
	if (Op == TEXT("nest"))
	{
		const int32 N = (int32)Num(2);
		if (Sub == TEXT("food")) { Nest.Food = FMath::Max(0, Nest.Food + N); }
		else if (Sub == TEXT("jelly")) { Nest.Jelly = FMath::Max(0, Nest.Jelly + N); }
		else if (Sub == TEXT("larvae")) { Nest.Larvae = FMath::Clamp(N, 0, AntNest::MaxLarvae); }
		else if (Sub == TEXT("workers")) { Nest.Workers = FMath::Clamp(N, 0, AntNest::MaxWorkers); }
		else if (Sub == TEXT("mood")) { Nest.Mood = FMath::Clamp(N, 0, 100); }
		else if (Sub == TEXT("tier")) { Nest.MaxTier = FMath::Clamp(FMath::Max(Nest.MaxTier, N), 1, AntTier::Max); Nest.Tier = FMath::Clamp(N, 1, AntTier::Max); Tier = (uint8)Nest.Tier; }
		else if (Sub == TEXT("save")) { SaveNest(); }
		// what the screen's buttons send: nest buy B | squad N | tool P T | mod M | settier T | contract I | larva | pack | new | click X Y (the mouse, 0..1 of the screen)
		else if (Sub == TEXT("buy") && PC) { PlayerCommand(PC, AntRaidCmd::Buy, N, 0); }
		else if (Sub == TEXT("squad") && PC) { PlayerCommand(PC, AntRaidCmd::Squad, N, 0); }
		else if (Sub == TEXT("tool") && PC) { PlayerCommand(PC, AntRaidCmd::Tool, N, (int32)Num(3)); }
		else if (Sub == TEXT("mod") && PC) { PlayerCommand(PC, AntRaidCmd::Mod, N, 0); }
		else if (Sub == TEXT("settier") && PC) { PlayerCommand(PC, AntRaidCmd::Tier, N, 0); }
		else if (Sub == TEXT("contract") && PC) { PlayerCommand(PC, AntRaidCmd::Contract, N, 0); }
		else if (Sub == TEXT("larva") && PC) { PlayerCommand(PC, AntRaidCmd::Larva, 0, 0); }
		else if (Sub == TEXT("pack") && PC) { PlayerCommand(PC, AntRaidCmd::Workers, 0, 0); }
		else if (Sub == TEXT("new") && PC) { PlayerCommand(PC, AntRaidCmd::NewColony, 7777, 0); }
		else if (Sub == TEXT("click")) { AAntHUD::DevClickAt = FVector2D(Num(2), Num(3)); AAntHUD::DevClickFrames = 3; }
		else if (Sub == TEXT("up")) { if (Nest.Up.Num() < AntUp::Count) { Nest.Up.SetNumZeroed(AntUp::Count); } Nest.Up[FMath::Clamp(N, 0, AntUp::Count - 1)] = (uint8)FMath::Clamp((int32)Num(3), 0, AntUp::MaxTier); }
		else if (Sub == TEXT("info"))
		{
			UE_LOG(LogTemp, Display, TEXT("ANTNEST info: food %d jelly %d larvae %d workers %d mood %d (%s) raids %d cakes %d tier %d of %d (%d towards the next), up %d %d %d %d %d %d, squad cap %d"),
				Nest.Food, Nest.Jelly, Nest.Larvae, Nest.Workers, Nest.Mood, AntNest::MoodName(Nest.Mood), Nest.Raids, Nest.CakesHome, Nest.Tier, Nest.MaxTier, Nest.CakesAtMax,
				(int32)Nest.Tier_(0), (int32)Nest.Tier_(1), (int32)Nest.Tier_(2), (int32)Nest.Tier_(3), (int32)Nest.Tier_(4), (int32)Nest.Tier_(5), AntNest::SquadCap(Nest));
		}
		else { return false; }
		return true;
	}
	return false;
}

bool AAntRaid::DevFixture(const TArray<FString>& A)
{
	UWorld* W = GetWorld();
	const FString Op = A[0].ToLower(), Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString();
	const FString Arg = A.IsValidIndex(2) ? A[2] : FString();
	AAntPlayerController* PC = Cast<AAntPlayerController>(W->GetFirstPlayerController());
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	auto UndoFastener = [W, Me](const FString& Id)
	{
		int32 N = 0;
		for (TActorIterator<AKitchenFastener> It(W); It; ++It)
		{
			const TConstArrayView<FLevelV3FastenerDef> All = LevelV3::Fasteners();
			if (!All.IsValidIndex(It->Def) || It->bDone) { continue; }
			const FString FId = All[It->Def].Id;
			const FString FixName = LevelV3::Fixtures().IsValidIndex(All[It->Def].Fixture) ? LevelV3::Fixtures()[All[It->Def].Fixture].Id : TEXT("");
			if (Id == TEXT("all") || FId.Equals(Id, ESearchCase::IgnoreCase) || FixName.Equals(Id, ESearchCase::IgnoreCase)) { It->ForceDone(Me); ++N; }
		}
		return N;
	};
	auto Push = [W, Me](const FString& Id)
	{
		AKitchenFixture* F = AKitchenFixture::Find(W, Id);
		if (F) { F->ForceDone(Me); }
		return F != nullptr;
	};
	auto PushDir = [W, &A](const FString& Id)
	{
		if (AKitchenFixture* F = AKitchenFixture::Find(W, Id))
		{
			if (A.IsValidIndex(4)) { F->PushDir = FVector(FCString::Atof(*A[3]), FCString::Atof(*A[4]), 0.f); }
		}
	};
	if (Op == TEXT("fastener") && Sub == TEXT("list"))
	{
		for (TActorIterator<AKitchenFastener> It(W); It; ++It)
		{
			const TConstArrayView<FLevelV3FastenerDef> All = LevelV3::Fasteners();
			if (All.IsValidIndex(It->Def))
			{
				UE_LOG(LogTemp, Display, TEXT("ANTFIX fastener %s of %s: %s, need %.0f, %.1f s, loose %d, progress %.2f, done %d at %s"), All[It->Def].Id, LevelV3::Fixtures()[All[It->Def].Fixture].Id,
					*AKitchenFastener::KindName(All[It->Def].Kind), It->Need, It->Seconds, It->bLoose ? 1 : 0, It->Progress, It->bDone ? 1 : 0, *It->GetActorLocation().ToCompactString());
			}
		}
		for (TActorIterator<AKitchenFixture> It(W); It; ++It)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTFIX fixture %s: state %d, own work %d (need %.0f), at %s"), *It->FixId(), (int32)It->State, It->CanInteract(nullptr) ? 1 : 0, It->Need, *It->GetActorLocation().ToCompactString());
		}
		return true;
	}
	if (Op == TEXT("fastener") && Sub == TEXT("undo"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTFIX dev undo %s: %d fasteners"), *Arg, UndoFastener(Arg));
		return true;
	}
	if (Op == TEXT("fix") && Sub == TEXT("push"))
	{
		PushDir(Arg);   // fix push <id> [dx dy]
		UE_LOG(LogTemp, Display, TEXT("ANTFIX dev push %s: %s"), *Arg, Push(Arg) ? TEXT("ok") : TEXT("no such fixture"));
		return true;
	}
	if (Op == TEXT("fix") && Sub == TEXT("fall"))
	{
		if (AKitchenFixture* F = AKitchenFixture::Find(W, Arg)) { F->Fall(FVector(A.IsValidIndex(3) ? FCString::Atof(*A[3]) : 1.f, A.IsValidIndex(4) ? FCString::Atof(*A[4]) : 0.f, 0.f), Me, TEXT("dev")); }
		return true;
	}
	if (Op == TEXT("break"))
	{
		if (AKitchenFixture* F = AKitchenFixture::Find(W, Sub)) { F->BreakNow(Me); } else { UE_LOG(LogTemp, Warning, TEXT("ANTFIX dev break: no fixture %s"), *Sub); }
		return true;
	}
	if (Op == TEXT("chain"))
	{
		// The six chain reactions of the design, started at their first step; the rest follows by itself.
		const FString C = Sub.ToUpper();
		if (C == TEXT("C1")) { UndoFastener(TEXT("v3_shelf_sc0")); UndoFastener(TEXT("v3_shelf_sc1")); }          // shelf -> jars -> jam / brine -> the Human slips
		else if (C == TEXT("C2")) { Push(TEXT("tablecloth")); }                                                    // tablecloth -> teapot, cups -> crash -> the dog
		else if (C == TEXT("C3")) { UndoFastener(TEXT("v3_rod_sc0")); UndoFastener(TEXT("v3_rod_sc1")); }        // rod -> curtains -> plant
		else if (C == TEXT("C4")) { UndoFastener(TEXT("v3_cola_cap")); Push(TEXT("cola")); }                      // cola -> sticky floor
		else if (C == TEXT("C5")) { UndoFastener(TEXT("v3_str0")); if (GM) { GM->MakeNoise(FVector(17700.f, -650.f, 4600.f), 1.f, AntNoise::Crash, nullptr, Me); } }   // balloon -> pop -> everyone to the sofa
		else if (C == TEXT("C6")) { UndoFastener(TEXT("v3_needle0")); }                                           // needle -> yarn -> grandma
		UE_LOG(LogTemp, Display, TEXT("ANTFIX dev chain %s started"), *C);
		return true;
	}
	if (Op == TEXT("state") && Sub == TEXT("chaos"))
	{
		// "after the battle": every chain gone off at once
		for (const TCHAR* C : { TEXT("C1"), TEXT("C2"), TEXT("C3"), TEXT("C4"), TEXT("C5"), TEXT("C6") }) { DevFixture({ TEXT("chain"), C }); }
		Push(TEXT("cereal")); Push(TEXT("glasses"));
		UndoFastener(TEXT("photo_l")); UndoFastener(TEXT("fridge_art"));
		return true;
	}
	return false;
}

