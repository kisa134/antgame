#include "AntSnowball.h"
#include "AntTrail.h"
#include "AntRts.h"
#include "AntHaulAdapter.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntRun.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "AntRoutes.h"
#include "AntRun.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const FLinearColor SnowGold(1.f, 0.82f, 0.3f);
	const FLinearColor SnowInfo(0.75f, 0.95f, 0.8f);
	APlayerState* GCredit = nullptr;
}

// =============================================================================================
// The picks
// =============================================================================================

namespace AntPick
{
	struct FDef { const TCHAR* Name; const TCHAR* Text; uint8 Pool; uint8 Card; };
	static const FDef Defs[Count] =
	{
		{ TEXT("Новобранцы"),         TEXT("+2 умных муравья в твой отряд"), PSquad, 255 },
		{ TEXT("Подкрепление"),       TEXT("+3 умных муравья в твой отряд"), PSquad, 255 },
		{ TEXT("Ветеран"),            TEXT("+1 солдат и +1 фуражир в отряд"), PSquad, 255 },
		{ TEXT("Рабочие"),            TEXT("+12 рабочих на твои тропы"), PWorkers, 255 },
		{ TEXT("Выводок рабочих"),    TEXT("+25 рабочих на твои тропы"), PWorkers, 255 },
		{ TEXT("Длинная тропа"),      TEXT("Тропа может быть на 50 % длиннее"), PTrail, 255 },
		{ TEXT("Вторая тропа"),       TEXT("Ещё одна тропа одновременно"), PTrail, 255 },
		{ TEXT("Быстрые рабочие"),    TEXT("Рабочие на тропах на 35 % быстрее"), PTrail, 255 },
		{ TEXT("Крепкие рабочие"),    TEXT("Рабочий уносит еду тяжелее (+4 к весу)"), PTrail, 255 },
		{ TEXT("Сбор у тропы"),       TEXT("Рабочие сами сходят за едой в стороне от тропы (+500)"), PTrail, 255 },
		{ TEXT("Острые жвала"),       TEXT("Твой укус на 25 % сильнее"), PPersonal, 255 },
		{ TEXT("Быстрые лапы"),       TEXT("Ты на 10 % быстрее"), PPersonal, 255 },
		{ TEXT("Сильная спина"),      TEXT("Ты тянешь на 30 % больше"), PPersonal, 255 },
		{ TEXT("Едкая кислота"),      TEXT("Плевок на 25 % дешевле"), PPersonal, 255 },
		{ TEXT("Мёртвая хватка"),     TEXT("Укусы и тараны не выбивают груз из жвал"), PPersonal, 255 },
		{ TEXT("Сильный отряд"),      TEXT("Каждый муравей отряда тянет на 4 больше"), PSquadUp, 255 },
		{ TEXT("Броня отряда"),       TEXT("Отряд на 40 % живучее"), PSquadUp, 255 },
		{ TEXT("Резвый отряд"),       TEXT("Отряд ходит на 12 % быстрее"), PSquadUp, 255 },
		{ TEXT("Обманка"),            TEXT("+2 заряда: засада красных уходит сторожить точку"), PTools, 255 },
		{ TEXT("Тревожный запах"),    TEXT("+2 заряда: весь отряд бросает всё и бежит к тебе"), PTools, 255 },
		{ TEXT("Приманка"),           TEXT("+2 заряда: Бадрик идёт жрать крошку"), PTools, 255 },
		{ TEXT("Крепкие жвала"),      TEXT("Карта колонии: все несут на 25 % сильнее"), PCards, AntCard::Jaws },
		{ TEXT("Лёгкие лапы"),        TEXT("Карта колонии: без груза на 15 % быстрее"), PCards, AntCard::Legs },
		{ TEXT("Тихий шаг"),          TEXT("Карта колонии: Человек замечает позже"), PCards, AntCard::Quiet },
		{ TEXT("Рычаг"),              TEXT("Карта колонии: двери на класс легче"), PCards, AntCard::Lever },
		{ TEXT("Хитин"),              TEXT("Карта колонии: первый укус не отрывает лапу"), PCards, AntCard::Chitin },
		{ TEXT("Пружина"),            TEXT("Карта колонии: рывок перезаряжается быстрее"), PCards, AntCard::Spring },
		{ TEXT("Заботливая бочка"),   TEXT("Карта колонии: Бочка лечит сильнее"), PCards, AntCard::Nurse },
		{ TEXT("Мягкая упаковка"),    TEXT("Карта колонии: торт и трофеи не бьются"), PCards, AntCard::Padding },
		{ TEXT("Азарт"),              TEXT("Карта колонии: жадность растёт быстрее"), PCards, AntCard::Greedy },
		{ TEXT("Сверхурочные"),       TEXT("Карта колонии: рейд на 45 с дольше"), PCards, AntCard::Overtime },
		{ TEXT("Запас личинок"),      TEXT("Карта колонии: +2 личинки прямо сейчас"), PCards, AntCard::Brood },
	};
	const TCHAR* Name(uint8 Id) { return Id < Count ? Defs[Id].Name : TEXT("?"); }
	const TCHAR* Text(uint8 Id) { return Id < Count ? Defs[Id].Text : TEXT(""); }
	uint8 Pool(uint8 Id) { return Id < Count ? Defs[Id].Pool : 0; }
	uint8 Card(uint8 Id) { return Id < Count ? Defs[Id].Card : 255; }
	const TCHAR* PoolName(uint8 P)
	{
		static const TCHAR* N[PoolCount] = { TEXT("ОТРЯД"), TEXT("РАБОЧИЕ"), TEXT("ТРОПА"), TEXT("ТЫ"), TEXT("ОТРЯД+"), TEXT("ИНСТРУМЕНТ"), TEXT("КОЛОНИЯ") };
		return P < PoolCount ? N[P] : TEXT("");
	}
}

namespace AntTask
{
	const TCHAR* Name(uint8 Id)
	{
		static const TCHAR* N[Count] = { TEXT("за мной"), TEXT("тащат"), TEXT("держат"), TEXT("атакуют"), TEXT("сами") };
		return Id < Count ? N[Id] : TEXT("?");
	}
}

// =============================================================================================
// The actor
// =============================================================================================

AAntSnowball::AAntSnowball()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(5.f);
}

void AAntSnowball::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntSnowball, Mass);
	DOREPLIFETIME(AAntSnowball, Level);
	DOREPLIFETIME(AAntSnowball, PrevAt);
	DOREPLIFETIME(AAntSnowball, NextAt);
	DOREPLIFETIME(AAntSnowball, Players);
	DOREPLIFETIME(AAntSnowball, bActive);
}

void AAntSnowball::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

AAntSnowball* AAntSnowball::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntSnowball> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FAntSnowPlayer* AAntSnowball::Find(const APlayerState* PS)
{
	for (FAntSnowPlayer& P : Players) { if (PS && P.PS == PS) { return &P; } }
	return nullptr;
}

const FAntSnowPlayer* AAntSnowball::Find(const APlayerState* PS) const
{
	for (const FAntSnowPlayer& P : Players) { if (PS && P.PS == PS) { return &P; } }
	return nullptr;
}

FAntSnowPlayer* AAntSnowball::For(APlayerState* PS)
{
	if (!PS)
	{
		return nullptr;
	}
	if (FAntSnowPlayer* P = Find(PS))
	{
		return P;
	}
	FAntSnowPlayer& N = Players.AddDefaulted_GetRef();
	N.PS = PS;
	N.Lv.Init(0, AntBoost::Count);
	N.PicksDone = (uint8)FMath::Max(0, Level - 1);   // (a late joiner gets the next pick, not all the old ones)
	return &N;
}

int32 AAntSnowball::LvOf(const APlayerState* PS, uint8 Boost) const
{
	const FAntSnowPlayer* P = Find(PS);
	return (P && P->Lv.IsValidIndex(Boost)) ? P->Lv[Boost] : 0;
}

float AAntSnowball::RaidClock() const
{
	return GetWorld()->GetTimeSeconds() - StartedAt;
}

int32 AAntSnowball::Threshold(int32 Index) const
{
	// Cumulative mass for pick #Index+1: 20, 57, 110, 177, 260, 357, 470, 597, 740, 897, 1070 ... (one player; +60 % per extra player).
	int32 Humans = 0;
	for (const FAntSnowPlayer& P : Players) { Humans += P.PS ? 1 : 0; }
	const float K = 1.f + 0.6f * FMath::Max(0, Humans - 1);
	const float I = (float)Index;
	return FMath::RoundToInt((20.f + 30.f * I + 7.5f * I * I) * K);
}

int32 AAntSnowball::SquadCap() const
{
	const AAntRaid* Raid = AAntRaid::Get(GetWorld());
	const int32 Brood = Raid ? Raid->Nest.Tier_(AntUp::Brood) : 0;
	int32 Cap = 24 + 12 * Brood;
	FParse::Value(FCommandLine::Get(), TEXT("AntSquadCap="), Cap);   // dev
	return Cap;
}

int32 AAntSnowball::WorkerCap() const
{
	const AAntRaid* Raid = AAntRaid::Get(GetWorld());
	const int32 Brood = Raid ? Raid->Nest.Tier_(AntUp::Brood) : 0;
	const int32 Carriers = Raid ? Raid->Nest.Tier_(AntUp::Carriers) : 0;
	int32 Cap = 80 + 40 * Brood + 20 * Carriers;
	FParse::Value(FCommandLine::Get(), TEXT("AntWorkerCap="), Cap);   // dev
	return Cap;
}

void AAntSnowball::Begin(AAntRaid* Raid)
{
	bActive = true;
	Mass = 0;
	Level = 0;
	PrevAt = 0;
	Players.Reset();
	CardsAdded.Reset();
	Rng.Initialize(FMath::Rand());
	StartedAt = GetWorld()->GetTimeSeconds();
	NextTimeline = 0.f;
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		if (It->Team == 0 && !It->bIsHuman && !It->IsABot())
		{
			For(*It);
		}
	}
	for (FAntSnowPlayer& P : Players) { P.PicksDone = 0; }
	NextAt = Threshold(0);
	int32 Auto = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntAutoPick="), Auto)) { AutoPick = Auto == 9 ? 8 : FMath::Clamp(Auto - 1, -1, 2); }
	if (AAntTrailNet* T = AAntTrailNet::Get(GetWorld(), true)) { T->Clear(); }
	if (Raid)
	{
		Raid->Note(FString::Printf(TEXT("%s — каждый вышел один. Масса до первого выбора: %d"), *Raid->Clock(), NextAt));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSNOW begin: players %d, first pick at %d, squad cap %d, worker cap %d"), Players.Num(), NextAt, SquadCap(), WorkerCap());
	ForceNetUpdate();
}

void AAntSnowball::End()
{
	if (!bActive)
	{
		return;
	}
	AAntRaid* Raid = AAntRaid::Get(GetWorld());
	for (const FAntSnowPlayer& P : Players)
	{
		const FString Who = P.PS ? P.PS->GetPlayerName() : FString(TEXT("?"));
		UE_LOG(LogTemp, Display, TEXT("ANTSNOW end %s: mass %d, picks %d, squad %d (ever %d, lost %d), workers %d"), *Who, P.Mass, P.Taken.Num(), P.Squad.Num(), P.SquadEver, P.SquadLost, P.Workers);
		if (Raid)
		{
			Raid->Note(FString::Printf(TEXT("%s: масса %d, выборов %d, отряд %d (погибло %d), рабочих %d"), *Who, P.Mass, P.Taken.Num(), P.Squad.Num(), P.SquadLost, P.Workers));
		}
	}
	bActive = false;
	AntRts::DespawnAll(GetWorld());
	if (AAntTrailNet* T = AAntTrailNet::Get(GetWorld())) { T->Clear(); }
	for (FAntSnowPlayer& P : Players)
	{
		P.Offer.Reset();
		P.Squad.Reset();
		P.SquadTask.Reset();
		P.Tasks.Reset();
	}
	ForceNetUpdate();
}

void AAntSnowball::AddMass(APlayerState* By, int32 Amount, const FString& What)
{
	if (!bActive || Amount <= 0)
	{
		return;
	}
	Mass += Amount;
	if (FAntSnowPlayer* P = By ? For(By) : nullptr)
	{
		P->Mass += Amount;
	}
	AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>();
	while (Mass >= NextAt)
	{
		++Level;
		PrevAt = NextAt;
		NextAt = Threshold(Level);
		UE_LOG(LogTemp, Display, TEXT("ANTSNOW threshold %d at %.0f s: mass %d (next %d)"), Level, RaidClock(), Mass, NextAt);
		if (GM)
		{
			GM->Broadcast(FString::Printf(TEXT("МАССА %d — выбор! [1] [2] [3]"), Mass), SnowGold);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSNOW +%d «%s» by %s: mass %d / %d"), Amount, *What, By ? *By->GetPlayerName() : TEXT("-"), Mass, NextAt);
	ForceNetUpdate();
}

void AAntSnowball::MakeOffer(FAntSnowPlayer& P)
{
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	auto Allowed = [&](uint8 Id)
	{
		const uint8 Pool = AntPick::Pool(Id);
		if (Pool == AntPick::PSquad && P.Squad.Num() >= SquadCap()) { return false; }
		if (Pool == AntPick::PSquadUp && P.Squad.Num() == 0) { return false; }
		if (Pool == AntPick::PWorkers && P.Workers >= WorkerCap()) { return false; }
		if (Pool == AntPick::PCards && GS && GS->Cards.Contains(AntPick::Card(Id))) { return false; }
		if (Id == AntPick::Grip && P.Lv[AntBoost::Grip] > 0) { return false; }
		if (Id == AntPick::TrailSecond && P.Lv[AntBoost::TrailCount] >= 2) { return false; }
		if ((Id == AntPick::TrailFast || Id == AntPick::TrailHeavy || Id == AntPick::TrailForage || Id == AntPick::TrailLong) && P.Workers == 0 && P.Taken.Num() < 3) { return false; }
		return true;
	};
	P.Offer.Reset();
	if (P.Taken.Num() == 0)
	{
		// The first pick of a raid always shows the loop: a squad, workers for a trail, or yourself.
		P.Offer = { (uint8)AntPick::Recruit2, (uint8)AntPick::Workers12, (uint8)(Rng.RandRange(0, 1) ? AntPick::Carry : AntPick::Speed) };
	}
	else
	{
		// Three cards from three different pools; the squad and the workers come up most (they are the snowball).
		static const float W[AntPick::PoolCount] = { 26.f, 20.f, 18.f, 14.f, 10.f, 6.f, 6.f };
		TArray<uint8> UsedPools;
		for (int32 k = 0; k < 3; ++k)
		{
			TArray<uint8> Cand;
			TArray<float> Wt;
			float Sum = 0.f;
			for (uint8 Id = 0; Id < AntPick::Count; ++Id)
			{
				const uint8 Pool = AntPick::Pool(Id);
				if (UsedPools.Contains(Pool) || P.Offer.Contains(Id) || !Allowed(Id)) { continue; }
				int32 InPool = 0;
				for (uint8 J = 0; J < AntPick::Count; ++J) { InPool += AntPick::Pool(J) == Pool ? 1 : 0; }
				Cand.Add(Id);
				Wt.Add(W[Pool] / InPool);
				Sum += W[Pool] / InPool;
			}
			if (Cand.Num() == 0) { break; }
			float R = Rng.FRand() * Sum;
			int32 Pick = Cand.Num() - 1;
			for (int32 i = 0; i < Cand.Num(); ++i) { R -= Wt[i]; if (R <= 0.f) { Pick = i; break; } }
			P.Offer.Add(Cand[Pick]);
			UsedPools.Add(AntPick::Pool(Cand[Pick]));
		}
	}
	P.OfferEnds = GetWorld()->GetTimeSeconds() + 10.f;
	UE_LOG(LogTemp, Display, TEXT("ANTSNOW offer %s #%d: [1] %s  [2] %s  [3] %s"), P.PS ? *P.PS->GetPlayerName() : TEXT("?"), P.PicksDone + 1,
		P.Offer.IsValidIndex(0) ? AntPick::Name(P.Offer[0]) : TEXT("-"), P.Offer.IsValidIndex(1) ? AntPick::Name(P.Offer[1]) : TEXT("-"), P.Offer.IsValidIndex(2) ? AntPick::Name(P.Offer[2]) : TEXT("-"));
	ForceNetUpdate();
}

void AAntSnowball::Pick(APlayerState* PS, int32 Option)
{
	FAntSnowPlayer* P = Find(PS);
	if (!P || !P->Offer.IsValidIndex(Option))
	{
		return;
	}
	const uint8 Id = P->Offer[Option];
	P->Offer.Reset();
	P->PicksDone++;
	P->Taken.Add(Id);
	Apply(*P, Id);
	UE_LOG(LogTemp, Display, TEXT("ANTSNOW pick %s #%d at %.0f s: [%d] %s — squad %d, workers %d"), PS ? *PS->GetPlayerName() : TEXT("?"), P->PicksDone, RaidClock(), Option + 1, AntPick::Name(Id), P->Squad.Num(), P->Workers);
	if (AAntPlayerController* PC = PS ? Cast<AAntPlayerController>(PS->GetOwner()) : nullptr)
	{
		PC->ClientToast(FString::Printf(TEXT("Выбрано: %s — %s"), AntPick::Name(Id), AntPick::Text(Id)));
	}
	ForceNetUpdate();
}

void AAntSnowball::Apply(FAntSnowPlayer& P, uint8 Id)
{
	UWorld* W = GetWorld();
	AAntGameState* GS = W->GetGameState<AAntGameState>();
	AAntKitchenGameMode* GM = W->GetAuthGameMode<AAntKitchenGameMode>();
	AAntTrailNet* Trails = AAntTrailNet::Get(W, true);
	auto Up = [&P](uint8 B) { if (P.Lv.IsValidIndex(B)) { P.Lv[B] = (uint8)FMath::Min(9, P.Lv[B] + 1); } };
	auto Room = [&](int32 N) { return FMath::Clamp(SquadCap() - P.Squad.Num(), 0, N); };
	switch (Id)
	{
	case AntPick::Recruit2: AntRts::Spawn(P.PS, Room(2), 0); break;
	case AntPick::Recruit3: AntRts::Spawn(P.PS, Room(3), 0); break;
	case AntPick::Veteran: AntRts::Spawn(P.PS, Room(1), 1); AntRts::Spawn(P.PS, Room(1), 0); break;
	case AntPick::Workers12: P.Workers = FMath::Min(WorkerCap(), P.Workers + 12); break;
	case AntPick::Workers25: P.Workers = FMath::Min(WorkerCap(), P.Workers + 25); break;
	case AntPick::TrailLong: Up(AntBoost::TrailLen); break;
	case AntPick::TrailSecond: Up(AntBoost::TrailCount); break;
	case AntPick::TrailFast: Up(AntBoost::WorkerSpeed); break;
	case AntPick::TrailHeavy: Up(AntBoost::WorkerCarry); break;
	case AntPick::TrailForage: Up(AntBoost::Forage); break;
	case AntPick::Bite: Up(AntBoost::Bite); break;
	case AntPick::Speed: Up(AntBoost::Speed); break;
	case AntPick::Carry: Up(AntBoost::Carry); break;
	case AntPick::Acid: Up(AntBoost::Acid); break;
	case AntPick::Grip: Up(AntBoost::Grip); break;
	case AntPick::SquadStrong: Up(AntBoost::SquadStr); break;
	case AntPick::SquadTough: Up(AntBoost::SquadHp); break;
	case AntPick::SquadFast: Up(AntBoost::SquadSpeed); break;
	case AntPick::ToolDecoy:
	case AntPick::ToolAlarm:
	case AntPick::ToolBait:
		if (AAntPlayerState* PS = Cast<AAntPlayerState>(P.PS))
		{
			const uint8 Tool = Id == AntPick::ToolDecoy ? AntTool::Decoy : Id == AntPick::ToolAlarm ? AntTool::Alarm : AntTool::Bait;
			// Same tool in a pocket: two more charges; else an empty pocket; else the second pocket is swapped.
			const int32 Pocket = PS->Tool0 == Tool ? 0 : PS->Tool1 == Tool ? 1 : (PS->Tool0 == AntTool::None || PS->Charges0 == 0) ? 0 : 1;
			uint8& T = Pocket == 0 ? PS->Tool0 : PS->Tool1;
			uint8& C = Pocket == 0 ? PS->Charges0 : PS->Charges1;
			C = (uint8)FMath::Min(9, (T == Tool ? C : 0) + 2);
			T = Tool;
			PS->ForceNetUpdate();
		}
		break;
	default:
		if (AntPick::Pool(Id) == AntPick::PCards && GS)
		{
			const uint8 Card = AntPick::Card(Id);
			GS->Cards.AddUnique(Card);
			CardsAdded.AddUnique(Card);
			if (Card == AntCard::Brood)
			{
				GS->Larvae = FMath::Min(AntNest::MaxLarvae, GS->Larvae + 2);
			}
			if (GM)
			{
				GM->Broadcast(FString::Printf(TEXT("%s взял карту колонии «%s»: %s"), P.PS ? *P.PS->GetPlayerName() : TEXT("?"), AntPick::Name(Id), AntPick::Text(Id)), SnowGold);
			}
		}
		break;
	}
	// Squad upgrades act on the ants already in it.
	if (AntPick::Pool(Id) == AntPick::PSquadUp)
	{
		for (AAntCharacter* A : P.Squad)
		{
			if (!IsValid(A) || A->bDead) { continue; }
			A->StrengthOverride = A->GetClassDef().Strength + 4.f * P.Lv[AntBoost::SquadStr];
			A->HealthOverride = A->GetClassDef().Health * (1.f + 0.4f * P.Lv[AntBoost::SquadHp]);
			A->SpeedScale = 1.f + 0.12f * P.Lv[AntBoost::SquadSpeed];
			A->SetClass(A->AntClass, true);
		}
	}
	if (Trails && P.PS) { Trails->Rebalance(P.PS); }
}

void AAntSnowball::Second()
{
	if (!bActive)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	// Players who joined after the start.
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		if (It->Team == 0 && !It->bIsHuman && !It->IsABot() && !Find(*It)) { For(*It); }
	}
	for (FAntSnowPlayer& P : Players)
	{
		if (!P.PS)
		{
			continue;
		}
		if (P.Offer.Num() > 0 && (Now >= P.OfferEnds || AutoPick >= 0))
		{
			int32 Opt = AutoPick >= 0 ? FMath::Min(AutoPick, P.Offer.Num() - 1) : Rng.RandRange(0, P.Offer.Num() - 1);
			if (AutoPick == 8)
			{
				// Dev (-AntAutoPick=9): what a sensible player would take - workers when the trail is empty, a squad, then the trail.
				auto Score = [&P](uint8 Id)
				{
					switch (Id)
					{
					case AntPick::Workers12: case AntPick::Workers25: return P.Workers < 12 ? 100 : 45;
					case AntPick::Recruit2: case AntPick::Recruit3: case AntPick::Veteran: return P.Squad.Num() < 8 ? 90 : 60;
					case AntPick::TrailForage: return P.Workers > 0 && P.Lv[AntBoost::Forage] == 0 ? 80 : 35;
					case AntPick::TrailFast: case AntPick::TrailHeavy: return 50;
					case AntPick::SquadStrong: case AntPick::SquadFast: return 55;
					case AntPick::Carry: case AntPick::Speed: return 40;
					default: return 20;
					}
				};
				Opt = 0;
				for (int32 i = 1; i < P.Offer.Num(); ++i) { if (Score(P.Offer[i]) > Score(P.Offer[Opt])) { Opt = i; } }
			}
			if (AutoPick < 0) { UE_LOG(LogTemp, Display, TEXT("ANTSNOW auto pick for %s (10 s passed)"), *P.PS->GetPlayerName()); }
			Pick(P.PS, Opt);
		}
		if (P.Offer.Num() == 0 && P.PicksDone < Level)
		{
			MakeOffer(P);
		}
		AntRts::TasksSecond(P);
	}
	// The family drops crumbs all evening: a light crumb on the floor every few seconds, so that trails have something to
	// fetch (the flat holds only ~30 light pieces on its floors at the start). -AntCrumbEvery=N seconds, 0 = off.
	static float CrumbEvery = -1.f;
	if (CrumbEvery < 0.f) { CrumbEvery = 6.f; FParse::Value(FCommandLine::Get(), TEXT("AntCrumbEvery="), CrumbEvery); }
	if (CrumbEvery > 0.f && Now >= NextCrumb)
	{
		NextCrumb = Now + CrumbEvery;
		if (AAntKitchenGameMode* GM = GetWorld()->GetAuthGameMode<AAntKitchenGameMode>())
		{
			const FVector P = AKitchenBuilder::RandomFloorPoint() + FVector(0.f, 0.f, 30.f);
			if (FVector::Dist2D(P, AKitchenBuilder::NestLocation(0)) > 1500.f && KitchenContainers::SealedAt(GetWorld(), P) < 0) { GM->SpawnFood(P, 0); }
		}
	}
	if (Now >= NextTimeline)
	{
		NextTimeline = Now + 10.f;
		const AAntTrailNet* T = AAntTrailNet::Get(GetWorld());
		FString Line = FString::Printf(TEXT("ANTSNOW timeline t=%.0f mass=%d level=%d next=%d ferried=%d cuts=%d"), RaidClock(), Mass, Level, NextAt, T ? T->Ferried : 0, T ? T->CutsDone : 0);
		for (const FAntSnowPlayer& P : Players)
		{
			int32 Alive = 0;
			for (const AAntCharacter* A : P.Squad) { Alive += (IsValid(A) && !A->bDead) ? 1 : 0; }
			Line += FString::Printf(TEXT(" | %s squad=%d workers=%d onTrails=%d trails=%d picks=%d tasks=%d"), P.PS ? *P.PS->GetPlayerName() : TEXT("?"), Alive, P.Workers,
				T ? T->WorkersOnTrails(P.PS) : 0, T ? T->TrailsOf(P.PS) : 0, P.Taken.Num(), P.Tasks.Num());
		}
		UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
	}
}

// =============================================================================================
// Hooks
// =============================================================================================

namespace AntSnowball
{
	bool Enabled(const UObject* Ctx)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoSnowball"));
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		return !bOff && W && AAntRaid::Get(W) != nullptr;
	}

	static AAntSnowball* Server(UWorld* W, bool bCreate)
	{
		AAntSnowball* S = AAntSnowball::Get(W);
		if (!S && bCreate && W && W->GetNetMode() != NM_Client)
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			S = W->SpawnActor<AAntSnowball>(AAntSnowball::StaticClass(), FTransform::Identity, P);
		}
		return S;
	}

	void OnWorldReset(AAntRaid* Raid)
	{
		if (!Raid || !Enabled(Raid))
		{
			return;
		}
		// The squad ants of the last raid go before the game mode respawns every bot.
		AntRts::DespawnAll(Raid->GetWorld());
		if (AAntTrailNet* T = AAntTrailNet::Get(Raid->GetWorld())) { T->Clear(); }
	}

	void OnRaidStart(AAntRaid* Raid)
	{
		if (Raid && Enabled(Raid))
		{
			if (AAntSnowball* S = Server(Raid->GetWorld(), true)) { S->Begin(Raid); }
		}
	}

	void OnRaidEnd(AAntRaid* Raid)
	{
		if (AAntSnowball* S = Raid ? AAntSnowball::Get(Raid->GetWorld()) : nullptr)
		{
			S->End();
		}
	}

	void RaidSecond(AAntRaid* Raid)
	{
		if (AAntSnowball* S = Raid ? AAntSnowball::Get(Raid->GetWorld()) : nullptr)
		{
			S->Second();
		}
	}

	void SetCredit(APlayerState* PS)
	{
		GCredit = PS;
	}

	void OnDelivered(AAntKitchenGameMode* GM, AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers, int32 Value)
	{
		AntHaul::NotifyDelivered(Food, 0);
		AAntSnowball* S = GM ? AAntSnowball::Get(GM->GetWorld()) : nullptr;
		if (!S || !S->bActive || !Food || Food->bBody)
		{
			return;
		}
		APlayerState* By = GCredit;
		for (int32 i = 0; !By && i < Carriers.Num(); ++i)
		{
			const AAntCharacter* A = Carriers[i];
			if (!A) { continue; }
			By = A->IsPlayerControlled() ? A->GetPlayerState() : SquadOwnerOf(A);
		}
		// The cake is the raid's own prize (it pays in jelly and the tier): a quarter of its price feeds the snowball.
		const int32 Amount = Food->bMain ? Value / 4 : Value;
		S->AddMass(By, Amount, Food->LootName);
	}

	bool TryPick(AAntPlayerController* PC, int32 Option)
	{
		const AAntSnowball* S = PC ? AAntSnowball::Get(PC->GetWorld()) : nullptr;
		const FAntSnowPlayer* P = S ? S->Find(PC->PlayerState) : nullptr;
		if (!P || !P->Offer.IsValidIndex(Option))
		{
			return false;
		}
		PC->ServerSwarm(AntRts::CmdPick, Option, TArray<AAntCharacter*>(), FVector::ZeroVector, nullptr);
		return true;
	}

	void ServerPick(AAntPlayerController* PC, int32 Option)
	{
		if (AAntSnowball* S = PC ? AAntSnowball::Get(PC->GetWorld()) : nullptr)
		{
			S->Pick(PC->PlayerState, Option);
		}
	}

	static int32 Lv(const AAntCharacter* Ant, uint8 B)
	{
		if (!Ant || Ant->Team != 0 || Ant->Creature != 0)
		{
			return 0;
		}
		const AAntSnowball* S = AAntSnowball::Get(Ant->GetWorld());
		const APlayerState* PS = Ant->GetPlayerState();
		return (S && S->bActive && PS) ? S->LvOf(PS, B) : 0;
	}

	float BiteMul(const AAntCharacter* Ant) { return 1.f + 0.25f * Lv(Ant, AntBoost::Bite); }
	float SpeedMul(const AAntCharacter* Ant) { return 1.f + 0.10f * Lv(Ant, AntBoost::Speed); }
	float CarryMul(const AAntCharacter* Ant) { return 1.f + 0.30f * Lv(Ant, AntBoost::Carry); }
	float AcidMul(const AAntCharacter* Ant) { return FMath::Pow(0.75f, (float)Lv(Ant, AntBoost::Acid)); }
	bool Grip(const AAntCharacter* Ant) { return Lv(Ant, AntBoost::Grip) > 0; }

	APlayerState* SquadOwnerOf(const AAntCharacter* Ant)
	{
		const AAntSnowball* S = Ant ? AAntSnowball::Get(Ant->GetWorld()) : nullptr;
		if (!S)
		{
			return nullptr;
		}
		for (const FAntSnowPlayer& P : S->Players)
		{
			if (P.Squad.Contains(Ant)) { return P.PS; }
		}
		return nullptr;
	}

	bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		if (!GM || A.Num() == 0)
		{
			return false;
		}
		const FString Op = A[0].ToLower();
		if (Op != TEXT("snow") && Op != TEXT("trail") && Op != TEXT("rts") && Op != TEXT("perf") && Op != TEXT("squad+"))
		{
			return false;
		}
		UWorld* W = GM->GetWorld();
		APlayerController* Host = W->GetFirstPlayerController();
		AAntSnowball* S = Server(W, true);
		// "p2" anywhere picks the second player (co-op tests); the host otherwise.
		APlayerState* PS = Host ? Host->PlayerState : nullptr;
		TArray<FString> Args;
		for (const FString& X : A)
		{
			if (X.StartsWith(TEXT("p")) && X.Len() == 2 && FChar::IsDigit(X[1]) && S)
			{
				const int32 Idx = X[1] - '1';
				if (S->Players.IsValidIndex(Idx)) { PS = S->Players[Idx].PS; }
				continue;
			}
			Args.Add(X);
		}
		auto ANum = [&Args](int32 i, float Def = 0.f) { return Args.IsValidIndex(i) ? FCString::Atof(*Args[i]) : Def; };
		const FString Sub = Args.IsValidIndex(1) ? Args[1].ToLower() : FString();
		AAntTrailNet* T = AAntTrailNet::Get(W, true);

		if (Op == TEXT("rts"))
		{
			UAntRtsInput* In = UAntRtsInput::Of(Host);
			if (In) { In->Dev(Args); }
			return true;
		}
		if (Op == TEXT("squad+"))
		{
			AntRts::Spawn(PS, (int32)ANum(1, 1), (uint8)ANum(2, 0));
			return true;
		}
		if (Op == TEXT("perf"))
		{
			// perf <squad> <workers>: a big squad and a busy trail for the frame-rate check.
			const int32 Squad = (int32)ANum(1, 30);
			const int32 Workers = (int32)ANum(2, 70);
			if (FAntSnowPlayer* P = S ? S->For(PS) : nullptr)
			{
				AntRts::Spawn(PS, Squad, 0);
				P->Workers = FMath::Max(P->Workers, Workers);
				if (T && T->TrailsOf(PS) == 0)
				{
					const FVector N = AKitchenBuilder::NestLocation(0);
					TArray<FVector> Pts = { N, N + FVector(2500.f, 600.f, 0.f), N + FVector(5000.f, 1800.f, 0.f) };
					T->DevLay(PS, Pts);
				}
				if (T) { T->Rebalance(PS); }
				UE_LOG(LogTemp, Display, TEXT("ANTSNOW perf: squad %d, workers %d (on trails %d)"), P->Squad.Num(), P->Workers, T ? T->WorkersOnTrails(PS) : 0);
			}
			return true;
		}
		if (Op == TEXT("trail") && T)
		{
			if (Sub == TEXT("lay"))
			{
				TArray<FVector> Pts;
				for (int32 i = 2; i + 2 < Args.Num(); i += 3) { Pts.Add(FVector(ANum(i), ANum(i + 1), ANum(i + 2))); }
				const int32 Id = T->DevLay(PS, Pts);
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL dev lay: %d points -> trail %d"), Pts.Num(), Id);
			}
			else if (Sub == TEXT("crumbs"))
			{
				// trail crumbs N R: N tier-0 crumbs within R of the player's trails (or around him).
				const int32 N = (int32)ANum(2, 10);
				const float R = ANum(3, 300.f);
				int32 Made = 0;
				for (int32 k = 0; k < N; ++k)
				{
					FVector At = FVector::ZeroVector;
					bool bHave = false;
					for (const FAntTrailLine& L : T->Lines)
					{
						if (L.Owner == PS && L.Points.Num() > 2 && !L.bLaying)
						{
							const int32 i = FMath::RandRange(L.Points.Num() / 4, L.Points.Num() - 1);
							At = FVector(L.Points[i]);
							bHave = true;
							break;
						}
					}
					if (!bHave)
					{
						const APawn* Pw = PS ? PS->GetPawn() : nullptr;
						if (!Pw) { break; }
						At = Pw->GetActorLocation();
					}
					const FVector2D Off = FMath::RandPointInCircle(R);
					if (AKitchenLoot* L = GM->SpawnFood(At + FVector(Off.X, Off.Y, 40.f), 0)) { ++Made; }
				}
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL dev crumbs: %d"), Made);
			}
			else if (Sub == TEXT("cut"))
			{
				const int32 K = T->Cut(FVector(ANum(2), ANum(3), ANum(4)), ANum(5, 200.f), TEXT("отладка"));
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL dev cut: %d workers"), K);
			}
			else if (Sub == TEXT("key"))
			{
				// trail key: [T] for the player's ant (start / finish laying)
				if (AAntCharacter* Ant = PS ? Cast<AAntCharacter>(PS->GetPawn()) : nullptr) { AntTrail::OnToggleKey(Ant); }
			}
			else if (Sub == TEXT("goto"))
			{
				// trail goto <route node>: the host's ant walks there along the route graph, on the floor (the dev walker of "go")
				AAntCharacter* Ant = Host ? Cast<AAntCharacter>(Host->GetPawn()) : nullptr;
				const int32 Node = FMath::Clamp((int32)ANum(2), 0, AntRoutes::NumNodes() - 1);
				TArray<FVector> Way;
				if (Ant && AntRoutes::PathPoints(Ant->GetActorLocation(), AntRoutes::Point(Node), false, Way, nullptr, AntRoutes::FloorOnly))
				{
					GM->DevGoPts = Way;
					GM->DevGoStart = GM->DevGoBestAt = W->GetTimeSeconds();
					GM->DevGoBest = 1.e9f;
					GM->DevGoLogAt = 0.f;
					UE_LOG(LogTemp, Display, TEXT("ANTTRAIL dev goto %d: %d points"), Node, Way.Num());
				}
			}
			else if (Sub == TEXT("rich"))
			{
				// trail rich: the host's ant walks (route graph, floor) to the floor spot with the most light food within 700 - where a player would lay a trail to
				AAntCharacter* Ant = Host ? Cast<AAntCharacter>(Host->GetPawn()) : nullptr;
				TArray<FVector> Foods;
				for (TActorIterator<AKitchenLoot> It(W); It; ++It)
				{
					const FVector P = It->GetActorLocation();
					if (!It->bIsGear && !It->bMain && !It->IsCarried() && !It->bHeld && It->Weight <= 8 && P.Z < 150.f && KitchenContainers::SealedAt(W, P) < 0
						&& FVector::Dist2D(P, AKitchenBuilder::NestLocation(0)) > 1500.f && FVector::Dist2D(P, AKitchenBuilder::NestLocation(0)) < ANum(2, 9000.f)) { Foods.Add(P); }
				}
				int32 Best = -1, BestN = 0;
				for (int32 i = 0; i < Foods.Num(); ++i)
				{
					int32 Nn = 0;
					for (const FVector& Q : Foods) { Nn += FVector::Dist2D(Q, Foods[i]) < 700.f ? 1 : 0; }
					if (Nn > BestN) { BestN = Nn; Best = i; }
				}
				TArray<FVector> Way;
				if (Ant && Best >= 0 && AntRoutes::PathPoints(Ant->GetActorLocation(), Foods[Best], false, Way, nullptr, AntRoutes::FloorOnly))
				{
					Way.Add(Foods[Best]);
					GM->DevGoPts = Way;
					GM->DevGoStart = GM->DevGoBestAt = W->GetTimeSeconds();
					GM->DevGoBest = 1.e9f;
					GM->DevGoLogAt = 0.f;
				}
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL dev rich: %d pieces of food within 700 of %s (%d light pieces on the floor in all), %d points"), BestN, Best >= 0 ? *Foods[Best].ToCompactString() : TEXT("-"), Foods.Num(), Way.Num());
			}
			else if (Sub == TEXT("fetch"))
			{
				// trail fetch: the host's ant walks to the nearest light food on the floor (then "grab" and "trail goto 0" bring it home)
				AAntCharacter* Ant = Host ? Cast<AAntCharacter>(Host->GetPawn()) : nullptr;
				AKitchenLoot* Best = nullptr;
				float BestD = FMath::Square(ANum(2, 6000.f));
				for (TActorIterator<AKitchenLoot> It(W); Ant && It; ++It)
				{
					const FVector P = It->GetActorLocation();
					if (It->bIsGear || It->bMain || It->bBody || It->IsCarried() || It->bHeld || It->IsHidden() || It->Weight > 12 || P.Z > 150.f
						|| FVector::Dist2D(P, AKitchenBuilder::NestLocation(0)) < 1300.f || KitchenContainers::SealedAt(W, P) >= 0) { continue; }
					const float D = (float)FVector::DistSquared(P, Ant->GetActorLocation());
					if (D < BestD) { BestD = D; Best = *It; }
				}
				TArray<FVector> Way;
				if (Best && AntRoutes::PathPoints(Ant->GetActorLocation(), Best->GetActorLocation(), false, Way, nullptr, AntRoutes::FloorOnly))
				{
					Way.Add(Best->GetActorLocation());
					GM->DevGoPts = Way;
					GM->DevGoStart = GM->DevGoBestAt = W->GetTimeSeconds();
					GM->DevGoBest = 1.e9f;
					GM->DevGoLogAt = 0.f;
				}
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL dev fetch: %s at %s, %d points"), Best ? *Best->LootName : TEXT("nothing"), Best ? *Best->GetActorLocation().ToCompactString() : TEXT("-"), Way.Num());
			}
			else if (Sub == TEXT("mid"))
			{
				// trail mid [frac]: the log line with the point at that fraction of the player's first trail (to aim tests at it).
				for (const FAntTrailLine& L : T->Lines)
				{
					if (L.Owner == PS && L.Points.Num() > 1)
					{
						const FVector P = FVector(L.Points[FMath::Clamp((int32)(ANum(2, 0.5f) * (L.Points.Num() - 1)), 0, L.Points.Num() - 1)]);
						UE_LOG(LogTemp, Display, TEXT("ANTTRAIL mid of %d: %.0f %.0f %.0f"), (int32)L.Id, P.X, P.Y, P.Z);
						break;
					}
				}
			}
			else
			{
				for (const FAntTrailLine& L : T->Lines)
				{
					UE_LOG(LogTemp, Display, TEXT("ANTTRAIL info %d: owner %s, points %d, length %.0f, workers %d, laying %d"), (int32)L.Id, L.Owner ? *L.Owner->GetPlayerName() : TEXT("-"), L.Points.Num(), L.Length, L.Workers, L.bLaying ? 1 : 0);
				}
				UE_LOG(LogTemp, Display, TEXT("ANTTRAIL info: %d trails, %d ferries, ferried %d, cuts %d"), T->Lines.Num(), T->Ferries.Num(), T->Ferried, T->CutsDone);
			}
			return true;
		}
		if (Op == TEXT("snow") && S)
		{
			if (!S->bActive && Sub != TEXT("info"))
			{
				UE_LOG(LogTemp, Display, TEXT("ANTSNOW dev: the snowball is not running (no raid?)"));
				return true;
			}
			if (Sub == TEXT("mass")) { S->AddMass(PS, (int32)ANum(2, 20), TEXT("отладка")); }
			else if (Sub == TEXT("food"))
			{
				// snow food K x y z: a big prize (AntKit::HeavyFood K: 0 cake piece, 1 golden bear, 2 chocolate, 3 cheese) dropped there
				const FVector At(ANum(3), ANum(4), ANum(5) + 40.f);
				if (AKitchenLoot* F = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					F->InitFood(AntKit::HeavyFood((int32)ANum(2, 3)));
					F->FinishSpawning(FTransform(At));
					UE_LOG(LogTemp, Display, TEXT("ANTSNOW dev food «%s» weight %d at %s"), *F->LootName, F->Weight, *At.ToCompactString());
				}
			}
			else if (Sub == TEXT("pick")) { S->Pick(PS, (int32)ANum(2, 1) - 1); }
			else if (Sub == TEXT("auto")) { S->AutoPick = Args.IsValidIndex(2) && Args[2] != TEXT("off") ? FMath::Clamp((int32)ANum(2, 1) - 1, 0, 2) : -1; }
			else if (Sub == TEXT("give")) { if (FAntSnowPlayer* P = S->For(PS)) { const uint8 Id = (uint8)FMath::Clamp((int32)ANum(2), 0, AntPick::Count - 1); P->Taken.Add(Id); S->Apply(*P, Id); } }
			else if (Sub == TEXT("offer")) { if (FAntSnowPlayer* P = S->For(PS)) { S->MakeOffer(*P); } }
			else
			{
				UE_LOG(LogTemp, Display, TEXT("ANTSNOW info: active %d mass %d level %d next %d players %d"), S->bActive ? 1 : 0, S->Mass, S->Level, S->NextAt, S->Players.Num());
				for (const FAntSnowPlayer& P : S->Players)
				{
					FString Picks;
					for (uint8 Id : P.Taken) { Picks += FString(AntPick::Name(Id)) + TEXT(", "); }
					UE_LOG(LogTemp, Display, TEXT("ANTSNOW info %s: mass %d squad %d workers %d tasks %d picks: %s"), P.PS ? *P.PS->GetPlayerName() : TEXT("?"), P.Mass, P.Squad.Num(), P.Workers, P.Tasks.Num(), *Picks);
				}
			}
			return true;
		}
		return true;
	}
}
