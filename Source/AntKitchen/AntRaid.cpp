#include "AntRaid.h"
#include "AntRun.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "AntBotController.h"
#include "AntArmy.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenArt.h"
#include "LevelV2.h"
#include "NpcBody.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor RaidGood(0.4f, 1.f, 0.4f);
	const FLinearColor RaidBad(1.f, 0.35f, 0.3f);
	const FLinearColor RaidInfo(0.9f, 0.9f, 0.9f);
	const FLinearColor RaidGold(1.f, 0.8f, 0.2f);

	bool FastScreens()
	{
		static const bool b = FParse::Param(FCommandLine::Get(), TEXT("AntFastVote"));
		return b;
	}
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
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		It->bNestReady = false;
		It->bHome = true;
	}
	if (!bFirst && GM)
	{
		for (TActorIterator<AAntArmy> It(GetWorld()); It; ++It) { It->Destroy(); }
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
	Tier = (uint8)FMath::Clamp(Nest.Tier, 1, Nest.MaxTier);
	RaidNo = Nest.Raids + 1;
	StartEnds = 0.f;
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
	const AntTier::FDef& T = TierDef();
	const int32 Players = FMath::Max(1, GM->GetNumPlayers());
	S->Shift = RaidNo;
	S->Larvae = Nest.Larvae;
	S->ShiftStage = 0;
	S->bQuotaMet = false;
	S->TargetScore = T.CakeValue + FMath::RoundToInt(T.Ask * (1.f + 0.4f * (Players - 1)) / 10.f) * 10;   // the cake, and side loot for every mouth
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
	bCakeKnown = Nest.Tier_(AntUp::Scouts) >= 1;
	CakeHint = FString();
	CakeWindowEnds = 0.f;
	CakeLeft = 100;
	if (AKitchenLoot* C = GM->MainObject.Get())
	{
		CakeLoc = C->GetActorLocation();
		float Best = 1.e12f;
		for (const FLevelV2Main& M : LevelV2::MainSpots())
		{
			const float D = (float)FVector::DistSquared(FVector(M.X, M.Y, M.Z), C->GetActorLocation());
			if (D < Best) { Best = D; CakeHint = M.Where; }
		}
	}
	RaidJelly = 0;
	ExitEnds = 0.f;
	ExitBy = FString();
	ExitOk.Reset();
	ExitStarter = nullptr;
	Story.Reset();
	Variety.Reset();
	Deaths = 0;
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
	GM->SetHumanAwake(false);
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		It->bNestReady = false;
		It->bHome = true;
	}
	Say(FString::Printf(TEXT("РЕЙД %d · угроза %d «%s». Цель — кусок именинного торта. Личинок: %d. Уйти: удерживай [L] у входа в гнездо"), RaidNo, (int32)Tier, T.Name, S->Larvae), RaidGold);
	UE_LOG(LogTemp, Display, TEXT("ANTRAID start %d: tier %d, length %.0f, quiet %.0f, final %.0f, ask %d, larvae %d, cake «%s» w %d v %d at %s, known %d, cards %d, contracts %d, players %d"), RaidNo, (int32)Tier,
		S->TimeRemaining, T.Quiet, T.Final, S->TargetScore, S->Larvae, *CakeHint, GM->MainObject.IsValid() ? GM->MainObject->Weight : 0, GM->MainObject.IsValid() ? GM->MainObject->Value * 2 : 0,
		*FVector(CakeLoc).ToCompactString(), bCakeKnown ? 1 : 0, S->Cards.Num(), S->Orders.Num(), Players);
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
	const AntTier::FDef& T = TierDef();
	const float Left = S->TimeRemaining;
	const float Length = GM->MatchLength + (GM->HasCard(AntCard::Overtime) ? 45.f : 0.f);
	const float Gone = Length - Left;
	float Quiet = T.Quiet + (GM->HasCard(AntCard::Phone) ? 30.f : 0.f);
	FParse::Value(FCommandLine::Get(), TEXT("AntQuiet="), Quiet);   // dev
	const uint8 Stage = Left <= 30.f ? 3 : Left <= T.Final ? 2 : Gone >= Quiet ? 1 : 0;
	GM->RunTick();   // greed, the event, who is shut in the fridge, what the Human is up to
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
	if (Cake == AntCake::Home || Cake == AntCake::Eaten || Cake == AntCake::Broken || Cake == AntCake::InHill)
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
	const uint8 New = C->IsCarried() ? (C->CarryTeam() == 0 ? AntCake::Ours : AntCake::Clan) : AntCake::Lying;
	if (New == AntCake::Ours && !bCakeLifted)
	{
		bCakeLifted = true;
		FString Names;
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
		return false;
	}
	Variety.Add(Food->LootName);
	FString Names;
	for (const AAntCharacter* A : Carriers)
	{
		if (A) { Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + A->GetDisplayName(); }
	}
	if (Food->bMain)
	{
		Cake = AntCake::Home;
		bCakeKnown = true;
		RaidJelly += TierDef().CakeJelly;
		if (AAntGameState* S = GS()) { S->bQuotaMet = true; }   // from now on greed grows while somebody stays outside
		const bool bSouth = KitchenArt::Enabled() && FVector::Dist2D(Food->GetActorLocation(), LevelV2::SouthEntrance()) < LevelV2::SouthEntranceRadius() + 100.f;
		Note(FString::Printf(TEXT("%s — торт дома: %s%s"), *Clock(), *Names, bSouth ? TEXT(" (через южный вход)") : TEXT("")));
		Say(FString::Printf(TEXT("ТОРТ ДОМА: +%d молочка. Можно уходить — или остаться за добавкой: жадность растёт"), TierDef().CakeJelly), RaidGold);
		UE_LOG(LogTemp, Display, TEXT("ANTRAID cake home at %s by %s, jelly %d"), *Clock(), *Names, RaidJelly);
		return false;   // its value is set by the tier already
	}
	if (Food->bTrophy)
	{
		++TrophiesHome;
		RaidJelly += 1;
		Note(FString::Printf(TEXT("%s — целый трофей «%s»: %s"), *Clock(), *Food->LootName, *Names));
	}
	Value = FMath::RoundToInt(Value * TierDef().LootMul);
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
}

void AAntRaid::OnPlayerDied(AAntCharacter* Ant, const FString& How)
{
	if (!InRaid() || !Ant)
	{
		return;
	}
	++Deaths;
	const FString Place = PlaceName(Ant->GetActorLocation());
	Note(FString::Printf(TEXT("%s — %s %s%s"), *Clock(), *Ant->GetDisplayName(), *How, Place.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Place)));
}

int32 AAntRaid::ContractBonus(uint8 OrderId)
{
	RaidJelly += 1;
	Note(FString::Printf(TEXT("%s — контракт Королевы: %s"), *Clock(), AntOrder::Name(OrderId)));
	return FMath::RoundToInt(40.f * TierDef().LootMul);
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
	GM->RunEndShift(How != AntRaidEnd::Wiped);   // contracts decided at the end, the awards
	BuildResult(How);
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
	R.Tier = Tier;
	R.Banked = S->TeamScore;
	const bool bWiped = How == AntRaidEnd::Wiped;
	R.Kept = bWiped ? R.Banked / 2 : R.Banked;
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
	Mood += TrophiesHome * 2 + SpecialsHome * 2;
	Mood -= FMath::Min(8, Deaths * 2);
	Mood -= bWiped ? 14 : 0;
	Nest.Mood = FMath::Clamp(Nest.Mood + Mood, 0, 100);
	R.MoodAfter = Nest.Mood;

	// The stores.
	Nest.Food += R.Kept;
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
	Result = R;
	UE_LOG(LogTemp, Display, TEXT("ANTRAID end %d: how %d, banked %d kept %d ask %d, jelly %d, cake %d, deaths %d, contracts %d/%d, variety %d, mood %d -> %d, larvae left %d +%d, workers +%d, queen ate %d, new tier %d"),
		RaidNo, (int32)How, R.Banked, R.Kept, R.Ask, R.Jelly, (int32)R.Cake, R.Deaths, R.ContractsDone, R.ContractsTaken, Variety.Num(), R.MoodBefore, R.MoodAfter, R.LarvaeLeft, R.LarvaeLaid,
		R.WorkersHatched, R.QueenAte, R.NewTier);
	SaveNest();
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
	default:
		break;
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
	const FString Op = A[0].ToLower(), Sub = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	AAntGameState* S = GS();
	AAntPlayerController* PC = Cast<AAntPlayerController>(GetWorld()->GetFirstPlayerController());
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	if (Op == TEXT("raid"))
	{
		if (Sub == TEXT("start")) { StartRaid(); }
		else if (Sub == TEXT("end")) { EndRaid(A.Num() > 2 && A[2] == TEXT("wipe") ? AntRaidEnd::Wiped : A.Num() > 2 && A[2] == TEXT("time") ? AntRaidEnd::Timeout : AntRaidEnd::Left); }
		else if (Sub == TEXT("leave") && PC) { PlayerCommand(PC, AntRaidCmd::Leave, 0, 0); }
		else if (Sub == TEXT("ready") && PC) { PlayerCommand(PC, AntRaidCmd::Ready, 1, 0); }
		else if (Sub == TEXT("go") && PC) { PlayerCommand(PC, AntRaidCmd::Start, 0, 0); }
		else if (Sub == TEXT("skip") && PC) { PlayerCommand(PC, AntRaidCmd::Skip, 0, 0); }
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
