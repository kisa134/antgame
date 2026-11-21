#include "AntRun.h"
#include "AntWreck.h"
#include "AntWeapon.h"
#include "AntVersus.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "AntBotController.h"
#include "HumanCharacter.h"
#include "KitchenLoot.h"
#include "KitchenBuilder.h"
#include "KitchenInteractables.h"
#include "KitchenMover.h"
#include "KitchenArt.h"
#include "KitchenEvents.h"
#include "Critters.h"
#include "NpcBody.h"
#include "AntSwarm.h"
#include "AntArmy.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/HUD.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "AntBody.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "AntMovementComponent.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "KitchenFixture.h"
#include "KitchenBuilder.h"
#include "HumanCharacter.h"
#include "AntRaid.h"
#include "KitchenSlick.h"
#include "AntFilm.h"
#include "AntMess.h"
#include "Engine/OverlapResult.h"
#include "NpcRoutine.h"
#include "AntCarry.h"
#include "AntSnowball.h"

namespace
{
	const FLinearColor RunGood(0.4f, 1.f, 0.4f);
	const FLinearColor RunBad(1.f, 0.35f, 0.3f);
	const FLinearColor RunInfo(0.9f, 0.9f, 0.9f);
	const FLinearColor RunGold(1.f, 0.8f, 0.2f);
}

// =============================================================================================
// Names
// =============================================================================================

namespace AntCard
{
	struct FDef { const TCHAR* Name; const TCHAR* Text; };
	static const FDef Defs[Count] =
	{
		{ TEXT("Крепкие жвала"),      TEXT("Все несут и тянут на четверть сильнее") },
		{ TEXT("Лёгкие лапы"),        TEXT("Без груза бегаете на 15% быстрее") },
		{ TEXT("Запас личинок"),      TEXT("+2 личинки в каждой смене") },
		{ TEXT("Тихий шаг"),          TEXT("Человек замечает вас позже") },
		{ TEXT("Полные железы"),      TEXT("Плевок тратит на треть меньше кислоты") },
		{ TEXT("Рычаг"),              TEXT("Двери считаются на класс легче") },
		{ TEXT("Натоптанная тропа"),  TEXT("Рой на тропах приносит в полтора раза больше") },
		{ TEXT("Хитин"),              TEXT("Первый укус за жизнь не отрывает лапу") },
		{ TEXT("Долгий сериал"),      TEXT("Человек залипает в телефон на 30 с дольше") },
		{ TEXT("Мягкая упаковка"),    TEXT("Трофеи выдерживают падение с высоты") },
		{ TEXT("Пружина"),            TEXT("Рывок перезаряжается быстрее") },
		{ TEXT("Заботливая бочка"),   TEXT("Бочка лечит дальше и сильнее") },
		{ TEXT("Азарт"),              TEXT("Множитель жадности растёт быстрее") },
		{ TEXT("Сверхурочные"),       TEXT("Смена длится на 45 с дольше") },
	};
	const TCHAR* Name(uint8 Id) { return Id < Count ? Defs[Id].Name : TEXT("?"); }
	const TCHAR* Text(uint8 Id) { return Id < Count ? Defs[Id].Text : TEXT(""); }
	bool Has(const UObject* Ctx, uint8 Id)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		const AAntGameState* GS = W ? W->GetGameState<AAntGameState>() : nullptr;
		return GS && GS->Cards.Contains(Id);
	}
}

namespace AntOrder
{
	static const TCHAR* Names[Count] =
	{
		TEXT("Донести трофей целым"),
		TEXT("Никто не погибнет за смену"),
		TEXT("Ограбить морозилку"),
		TEXT("Принести три вещи из гостиной"),
		TEXT("Вырубить или оглушить Человека"),
		TEXT("Закончить за две минуты до конца"),
		TEXT("Разорвать врага пополам"),
		TEXT("Не разбить ни одного трофея"),
		TEXT("Открыть пять дверей"),
		TEXT("Ограбить холодильник"),
		TEXT("Принести домой тело товарища"),
		TEXT("Уронить что-нибудь на Человека"),
		TEXT("Дважды отвлечь Человека шумом"),
		TEXT("Перевыполнить квоту в полтора раза"),
		TEXT("Человек слишком спокоен. Сделайте, чтобы он упал"),
	};
	const TCHAR* Name(uint8 Id) { return Id < Count ? Names[Id] : TEXT("?"); }
	bool AtEnd(uint8 Id) { return Id == NoDeaths || Id == Early || Id == NoBreak; }
}

namespace AntNoise
{
	const TCHAR* Name(uint8 Kind)
	{
		static const TCHAR* Names[Count] = { TEXT("шум"), TEXT("кран"), TEXT("тостер"), TEXT("микроволновка"), TEXT("телевизор"), TEXT("грохот"), TEXT("хлопнула дверь"), TEXT("мышеловка"), TEXT("лай Бадрика"), TEXT("визг малыша") };
		return Kind < Count ? Names[Kind] : Names[0];
	}
}

namespace AntEvent
{
	struct FDef { const TCHAR* Name; const TCHAR* Text; float Length; };
	static const FDef Defs[Count] =
	{
		{ TEXT(""), TEXT(""), 0.f },
		{ TEXT("ВЫБИЛО ПРОБКИ"),        TEXT("Света нет: Человек видит только вблизи"), 40.f },
		{ TEXT("КОШКА ПРОСНУЛАСЬ"),     TEXT("Минуту она охотится на всё, что ползает"), 60.f },
		{ TEXT("РОБОТ-ПЫЛЕСОС"),        TEXT("Засасывает крошки и давит муравьёв. Сверху на нём можно ехать"), 45.f },
		{ TEXT("РАКОВИНА ПЕРЕЛИЛАСЬ"),  TEXT("Лужа у мойки: в воде медленно, еда плавает"), 40.f },
		{ TEXT("ПРИВЕЗЛИ ПИЦЦУ"),       TEXT("Коробка у двери в гостиную. Минута, пока не унесли"), 60.f },
		{ TEXT("ХОЛОДИЛЬНИК НАРАСПАШКУ"), TEXT("Человек забыл закрыть дверь: 20 секунд"), 20.f },
		{ TEXT("БАДРИК НОСИТСЯ"),       TEXT("На него нашло: полминуты галопом по всему дому. С открытого пола — прочь!"), 28.f },
	};
	const TCHAR* Name(uint8 Id) { return Id < Count ? Defs[Id].Name : TEXT(""); }
	const TCHAR* Text(uint8 Id) { return Id < Count ? Defs[Id].Text : TEXT(""); }
	float Length(uint8 Id) { return Id < Count ? Defs[Id].Length : 0.f; }
}

namespace AntPing
{
	const TCHAR* Name(uint8 Kind)
	{
		static const TCHAR* Names[Count] = { TEXT("Сюда"), TEXT("Трофей"), TEXT("Человек!"), TEXT("Дверь") };
		return Kind < Count ? Names[Kind] : Names[0];
	}
}

// =============================================================================================
// Containers: what lies behind which door
// =============================================================================================

namespace KitchenContainers
{
	static const FDef Defs[Count] =
	{
		{ TEXT("в холодильнике"),   FVector(2780.f, 3100.f, 200.f),   FVector(4620.f, 4670.f, 2500.f),  (int32)EKitchenDoorKind::Fridge,     { -1, -1 }, true },
		{ TEXT("в морозилке"),      FVector(2780.f, 3100.f, 2560.f),  FVector(4620.f, 4670.f, 3620.f),  -1,                                  { 20, -1 }, true },
		{ TEXT("в духовке"),        FVector(-1450.f, 3000.f, 300.f),  FVector(-50.f, 4700.f, 1500.f),   (int32)EKitchenDoorKind::Oven,       { -1, -1 }, true },
		{ TEXT("в микроволновке"),  FVector(1300.f, 4000.f, 1740.f),  FVector(2200.f, 4880.f, 2330.f),  (int32)EKitchenDoorKind::Microwave,  { -1, -1 }, true },
		{ TEXT("в посудомойке"),    FVector(60.f, 3040.f, 160.f),     FVector(1240.f, 4700.f, 1600.f),  (int32)EKitchenDoorKind::Dishwasher, { -1, -1 }, false },
		{ TEXT("в верхнем ящике"),  FVector(-5550.f, 3040.f, 1330.f), FVector(-4250.f, 4640.f, 1640.f), (int32)EKitchenDoorKind::Drawer,     { -1, -1 }, false },
		{ TEXT("в ящике"),          FVector(-5550.f, 3040.f, 760.f),  FVector(-4250.f, 4640.f, 1300.f), -1,                                  { 2, -1 },  false },
		{ TEXT("в шкафу"),          FVector(-7000.f, 3040.f, 140.f),  FVector(-5600.f, 4740.f, 1650.f), -1,                                  { 0, 1 },   false },
		{ TEXT("в шкафу"),          FVector(-2800.f, 3040.f, 140.f),  FVector(-1500.f, 4740.f, 1650.f), -1,                                  { 4, 5 },   false },
		{ TEXT("в шкафу"),          FVector(1300.f, 3040.f, 140.f),   FVector(2500.f, 4740.f, 1650.f),  -1,                                  { 6, 7 },   false },
		{ TEXT("в верхнем шкафу"),  FVector(-7000.f, 4336.f, 3010.f), FVector(-5750.f, 4950.f, 4380.f), -1,                                  { 14, 10 }, false },
		{ TEXT("в верхнем шкафу"),  FVector(-5750.f, 4336.f, 3010.f), FVector(-4500.f, 4950.f, 4380.f), -1,                                  { 15, 11 }, false },
		{ TEXT("в верхнем шкафу"),  FVector(-4500.f, 4336.f, 3010.f), FVector(-3250.f, 4950.f, 4380.f), -1,                                  { 16, 12 }, false },
		{ TEXT("в верхнем шкафу"),  FVector(-3250.f, 4336.f, 3010.f), FVector(-2000.f, 4950.f, 4380.f), -1,                                  { 17, 13 }, false },
		{ TEXT("в верхнем шкафу"),  FVector(1000.f, 4336.f, 3010.f),  FVector(2500.f, 4950.f, 4380.f),  -1,                                  { 18, 19 }, false },
		{ TEXT("в тумбе под ТВ"),   FVector(11140.f, -8950.f, 100.f), FVector(13745.f, -7750.f, 1000.f), -1,                                 { 25, 26 }, false },   // level v2: the stand stands 600 further west
		{ TEXT("в хлебнице"),       FVector(-6840.f, 3040.f, 1740.f), FVector(-5700.f, 3660.f, 2280.f), -1,                                  { 30, -1 }, false },   // level v2: the bread box on the west worktop (its lid is mover 30)
	};

	const FDef& Get(int32 Index) { return Defs[FMath::Clamp(Index, 0, Count - 1)]; }

	int32 At(const FVector& P)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const FDef& D = Defs[i];
			if (P.X > D.Min.X && P.X < D.Max.X && P.Y > D.Min.Y && P.Y < D.Max.Y && P.Z > D.Min.Z && P.Z < D.Max.Z)
			{
				return i;
			}
		}
		return -1;
	}

	namespace
	{
		// The doors of the current world, found again every couple of seconds (weak: nothing is kept alive, a map reload just refills it).
		TWeakObjectPtr<const UWorld> CacheWorld;
		double CacheTime = -100.0;
		TArray<TWeakObjectPtr<AKitchenInteractable>> CacheDoors[Count];

		void Refresh(const UWorld* World)
		{
			const double Now = FPlatformTime::Seconds();
			if (CacheWorld.Get() == World && Now - CacheTime < 2.0)
			{
				return;
			}
			CacheWorld = World;
			CacheTime = Now;
			for (int32 i = 0; i < Count; ++i)
			{
				CacheDoors[i].Reset();
			}
			for (TActorIterator<AKitchenInteractable> It(World); It; ++It)
			{
				if (const AKitchenDoor* D = Cast<AKitchenDoor>(*It))
				{
					for (int32 i = 0; i < Count; ++i)
					{
						if (Defs[i].DoorKind == (int32)D->Kind) { CacheDoors[i].Add(*It); }
					}
				}
				else if (const AKitchenMover* M = Cast<AKitchenMover>(*It))
				{
					for (int32 i = 0; i < Count; ++i)
					{
						if (M->PartIndex >= 0 && (Defs[i].Movers[0] == M->PartIndex || Defs[i].Movers[1] == M->PartIndex)) { CacheDoors[i].Add(*It); }
					}
				}
			}
		}
	}

	bool IsOpen(const UWorld* World, int32 Index)
	{
		if (!World || Index < 0 || Index >= Count)
		{
			return true;
		}
		Refresh(World);
		bool bAny = false;
		for (const TWeakObjectPtr<AKitchenInteractable>& D : CacheDoors[Index])
		{
			if (D.IsValid())
			{
				bAny = true;
				if (D->IsDoorOpen()) { return true; }
			}
		}
		return !bAny;   // no door here (the box-built kitchen): nothing is shut
	}

	int32 SealedAt(const UWorld* World, const FVector& P)
	{
		const int32 i = At(P);
		return (i >= 0 && !IsOpen(World, i)) ? i : -1;
	}

	void SetOpen(UWorld* World, int32 Index, bool bOpen)
	{
		if (!World || Index < 0 || Index >= Count)
		{
			return;
		}
		Refresh(World);
		for (const TWeakObjectPtr<AKitchenInteractable>& D : CacheDoors[Index])
		{
			if (D.IsValid()) { D->SetDoorOpen(bOpen); }
		}
	}
}
// =============================================================================================
// Noise: the Human goes to look
// =============================================================================================

void AAntKitchenGameMode::MakeNoise(const FVector& Loc, float Loud, uint8 Kind, AActor* Source, AAntCharacter* By)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!bHeist || !GS || GS->Phase != 0)
	{
		return;
	}
	FAntNoise N;
	N.Loc = Loc;
	N.Loud = Loud;
	N.Kind = Kind;
	N.Time = GetWorld()->GetTimeSeconds();
	N.Source = Source;
	// One entry per source: a running tap keeps refreshing its own.
	for (int32 i = Run.Noises.Num() - 1; i >= 0; --i)
	{
		if (Run.Noises[i].Kind == Kind && ((Source && Run.Noises[i].Source == Source) || FVector::DistSquared(Run.Noises[i].Loc, Loc) < 300.f * 300.f))
		{
			Run.Noises.RemoveAt(i);
		}
	}
	Run.Noises.Add(N);
	if (Run.Noises.Num() > 16)
	{
		Run.Noises.RemoveAt(0);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTNOISE %s loud=%.2f at %s by %s"), AntNoise::Name(Kind), Loud, *Loc.ToCompactString(), By ? *By->GetDisplayName() : TEXT("-"));
}

bool AAntKitchenGameMode::BestNoise(const FVector& From, FAntNoise& Out) const
{
	const float Now = GetWorld()->GetTimeSeconds();
	float Best = 0.3f;   // quieter than this (or too old) is not worth the walk
	bool bFound = false;
	for (const FAntNoise& N : Run.Noises)
	{
		const float Age = Now - N.Time;
		if (Age > 14.f || N.Kind >= AntNoise::Count)
		{
			continue;
		}
		if (N.Source.IsValid())
		{
			if (const float* Until = Run.CheckedSource.Find(N.Source); Until && *Until > Now)
			{
				continue;   // he has just been there
			}
		}
		else if (Run.CheckedKind[N.Kind] > Now)
		{
			continue;
		}
		const float Score = N.Loud * (1.f - Age / 14.f);
		if (Score > Best)
		{
			Best = Score;
			Out = N;
			bFound = true;
		}
	}
	return bFound;
}

void AAntKitchenGameMode::NoiseChecked(const FAntNoise& N, bool bDistracted)
{
	const float Until = GetWorld()->GetTimeSeconds() + 40.f;
	if (N.Source.IsValid())
	{
		Run.CheckedSource.Add(N.Source, Until);
		// He switches it off.
		if (AKitchenFaucet* F = Cast<AKitchenFaucet>(N.Source.Get()))
		{
			F->SetWater(false, false);
		}
		else if (AKitchenNoiseBox* Box = Cast<AKitchenNoiseBox>(N.Source.Get()))
		{
			Box->SwitchOff();
		}
	}
	else if (N.Kind < AntNoise::Count)
	{
		Run.CheckedKind[N.Kind] = Until;
	}
	for (int32 i = Run.Noises.Num() - 1; i >= 0; --i)
	{
		if (Run.Noises[i].Kind == N.Kind && (Run.Noises[i].Source == N.Source || FVector::DistSquared(Run.Noises[i].Loc, N.Loc) < 600.f * 600.f))
		{
			Run.Noises.RemoveAt(i);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTNOISE the Human checked: %s (distracted=%d)"), AntNoise::Name(N.Kind), bDistracted ? 1 : 0);
	if (bDistracted)
	{
		Broadcast(FString::Printf(TEXT("Человек проверил шум (%s) и ничего не нашёл"), AntNoise::Name(N.Kind)), RunInfo);
		if (++Run.Distractions >= 2)
		{
			CompleteOrder(AntOrder::Distract);
		}
	}
}

void AAntKitchenGameMode::SetHumanGoal(uint8 Kind, const FString& Text, const FVector& Loc)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	if (GS->HumanGoal != Kind || GS->HumanGoalText != Text)
	{
		GS->HumanGoal = Kind;
		GS->HumanGoalText = Text;
		UE_LOG(LogTemp, Display, TEXT("ANTHUMAN goal %d: %s"), (int32)Kind, *Text);
	}
	if (FVector::DistSquared(FVector(GS->HumanGoalLoc), Loc) > 200.f * 200.f)
	{
		GS->HumanGoalLoc = Loc;
	}
}

// =============================================================================================
// Hooks from the rest of the game
// =============================================================================================

bool AAntKitchenGameMode::HasCard(uint8 Id) const
{
	const AAntGameState* GS = GetGameState<AAntGameState>();
	return GS && GS->Cards.Contains(Id);
}

void AAntKitchenGameMode::OnHumanFell(AAntCharacter* By)
{
	Broadcast(By ? FString::Printf(TEXT("Человек поскользнулся на луже и грохнулся! Разлил: %s"), *By->GetDisplayName()) : FString(TEXT("Человек поскользнулся и грохнулся!")), RunGood);
	CompleteOrder(AntOrder::HumanFell);
	MakeNoise(Human ? Human->GetActorLocation() : FVector::ZeroVector, 1.f, AntNoise::Crash);
}

void AAntKitchenGameMode::OnDoorOpened(AKitchenInteractable* Door, AAntCharacter* By, int32 Class)
{
	if (!bHeist || Cast<AKitchenSpillBottle>(Door))
	{
		return;   // (a bottle tipped over is not a door)
	}
	if (By && By->Team == 0)
	{
		if (AAntPlayerState* PS = By->GetPlayerState<AAntPlayerState>())
		{
			PS->DoorsOpened++;
		}
		if (++Run.DoorsOpened >= 5)
		{
			CompleteOrder(AntOrder::Doors5);
		}
		if (Class >= AntDoor::Heavy)
		{
			Broadcast(FString::Printf(TEXT("%s с командой открыли тяжёлую дверь"), *By->GetDisplayName()), RunGood);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Level v2: the main object of the shift. One slice of birthday cake, heavier than any trophy, at one of three designed
// places deep in the flat. Everybody is told where. It is a trophy: whole it pays double (the design's 600), smashed its
// pieces add up to the plain 300. The raid mode the design wants later is not here: this lives inside the shift / quota loop.
// ---------------------------------------------------------------------------------------------

const FHumanNav* AAntKitchenGameMode::HouseNav()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoHumanNav"));
	if (bOff || !KitchenArt::Enabled())
	{
		return nullptr;
	}
	if (!HouseNavGrid.IsBuilt())
	{
		// The Human's own size (grandma and the kid are smaller: the grid is a little wider than they need).
		float Radius = 340.f, Half = 1700.f, Step = 250.f;
		if (Human)
		{
			Radius = Human->GetCapsuleComponent()->GetScaledCapsuleRadius();
			Half = Human->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Step = Human->GetCharacterMovement()->MaxStepHeight;
		}
		const double T0 = FPlatformTime::Seconds();
		HouseNavGrid.Build(GetWorld(), Human, Radius, Half, Step);
		int32 BadStops = 0;
		for (const FVector2f& S : LevelV2::PatrolStops())
		{
			if (!HouseNavGrid.IsFree(FVector(S.X, S.Y, 0.f)))
			{
				++BadStops;
				UE_LOG(LogTemp, Warning, TEXT("ANTV2 nav: patrol stop (%.0f, %.0f) is not free for his capsule"), S.X, S.Y);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 nav: %d cells, %d free, capsule %.0f x %.0f, step %.0f, patrol stops %d (%d not free), built in %.0f ms"), HouseNavGrid.NumCells(), HouseNavGrid.NumFree(),
			Radius, Half, Step, LevelV2::PatrolStops().Num(), BadStops, (FPlatformTime::Seconds() - T0) * 1000.0);
	}
	return &HouseNavGrid;
}

int32 AAntKitchenGameMode::MainWeight()
{
	int32 W = FMath::RoundToInt(LevelV2::MainWeightDesign());
	FParse::Value(FCommandLine::Get(), TEXT("AntCakeWeight="), W);   // tuning
	return FMath::Clamp(W, 6, 200);
}

void AAntKitchenGameMode::SpawnMainObject(int32 ForceSpot)
{
	if (!bHeist || !KitchenArt::Enabled())
	{
		return;
	}
	if (AKitchenLoot* Old = MainObject.Get())
	{
		Old->ReleaseAll();
		Old->Destroy();
	}
	const TConstArrayView<FLevelV2Main> Spots = LevelV2::MainSpots();
	int32 Pick = ForceSpot;
	if (!Spots.IsValidIndex(Pick))
	{
		int32 DevSpot = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntCakeSpot="), DevSpot)) { Pick = DevSpot - 1; }   // dev: -AntCakeSpot=1..3
	}
	if (!Spots.IsValidIndex(Pick) && LevelV3::Enabled() && LevelV3::Current())
	{
		Pick = LevelV3::Current()->Cake;   // level v3: the seed put it there (M1..M4)
	}
	if (!Spots.IsValidIndex(Pick))
	{
		int32 Roll = Rng.RandRange(0, 99);
		Pick = Spots.Num() - 1;
		for (int32 i = 0; i < Spots.Num(); ++i)
		{
			if (Roll < Spots[i].Chance) { Pick = i; break; }
			Roll -= Spots[i].Chance;
		}
	}
	const FLevelV2Main& S = Spots[Pick];
	FAntFoodDef D;
	D.Name = TEXT("Кусок именинного торта");
	D.Value = LevelV2::MainValue() / 2;   // a trophy: delivered whole it pays double
	D.Weight = MainWeight();
	if (bRaid && Raid)
	{
		// A raid: the threat tier decides how big the slice is.
		D.Value = Raid->TierDef().CakeValue / 2;
		// Heavier for a bigger team (+20% a player after the first up to four, +12% each from five to eight - AntCarry::CakePlayerMul):
		// the team must not walk off with it alone.
		int32 W = FMath::RoundToInt(Raid->TierDef().CakeWeight * AntCarry::CakePlayerMul(GetNumPlayers()));
		FParse::Value(FCommandLine::Get(), TEXT("AntCakeWeight="), W);
		D.Weight = FMath::Clamp(W, 6, 200);
	}
	D.Color = FLinearColor(1.f, 0.62f, 0.8f);
	D.Shape = EAntShape::Cube;
	D.Scale = FVector(0.8f, 0.55f, 0.45f);
	const FVector Where(S.X, S.Y, S.Z + 40.f);
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!L)
	{
		return;
	}
	L->InitFood(D);
	L->bTrophy = true;
	L->bMain = true;
	L->FinishSpawning(FTransform(Where));
	MainObject = L;
	if (!bRaid)   // (a raid: where it lies is for the scouts to know or for the team to find out)
	{
		Broadcast(FString::Printf(TEXT("ИМЕНИННЫЙ ТОРТ! Кусок лежит %s. Целым он стоит %d, но он тяжёлый: тащите всей колонной (F — армия)"), S.Where, LevelV2::MainValue()), RunGold);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTV2 cake: spot %d of %d (%d%%) «%s» at %s, weight %d, value %d whole"), Pick + 1, Spots.Num(), S.Chance, S.Where, *Where.ToCompactString(), D.Weight, D.Value * 2);
}

void AAntKitchenGameMode::OnTrophyBroken(AKitchenLoot* Trophy)
{
	if (Trophy && Trophy->bMain)
	{
		if (bRaid && Raid) { Raid->OnCakeBroken(Trophy->LastCarrier.Get()); }
		Broadcast(TEXT("Именинный торт разбился! Куски стоят вдвое меньше целого"), RunBad);
		UE_LOG(LogTemp, Display, TEXT("ANTV2 cake broken at %s"), *Trophy->GetActorLocation().ToCompactString());
	}
	UE_LOG(LogTemp, Display, TEXT("ANTLOOT trophy broken: «%s» at %s"), Trophy ? *Trophy->LootName : TEXT("?"), Trophy ? *Trophy->GetActorLocation().ToCompactString() : TEXT("?"));
	FailOrder(AntOrder::NoBreak);
	if (AAntCharacter* By = Trophy ? Trophy->LastCarrier.Get() : nullptr)
	{
		if (AAntPlayerState* PS = By->GetPlayerState<AAntPlayerState>())
		{
			PS->TrophyBreaks++;
		}
	}
}

void AAntKitchenGameMode::OnEnemyTorn(AAntCharacter* Victim, AAntCharacter* A, AAntCharacter* B)
{
	if (!Victim || Victim->Team == 0)
	{
		return;
	}
	for (AAntCharacter* Who : { A, B })
	{
		if (AAntPlayerState* PS = (Who && Who->Team == 0) ? Who->GetPlayerState<AAntPlayerState>() : nullptr)
		{
			PS->Torn++;
		}
	}
	CompleteOrder(AntOrder::Tear);
}

void AAntKitchenGameMode::OnHumanStunned(AAntCharacter* By, const FString& What)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	Broadcast(Human && Human->StunKind == 1 ? FString::Printf(TEXT("БАМ! «%s» — Человеку прямо по ноге. Он скачет на одной!"), *What)
		: FString::Printf(TEXT("БАМ! «%s» — Человеку прямо по голове. Он оглушён!"), *What), RunGold);
	if (Human && GS && Human->Rage < 3)
	{
		Human->SetRage(Human->Rage + 1);
		GS->HumanRage = Human->Rage;
	}
	SetHumanGoal(AntHumanGoal::Stunned, TEXT("оглушён"), Human ? Human->GetActorLocation() : FVector::ZeroVector);
	CompleteOrder(AntOrder::DropOnHuman);
	CompleteOrder(AntOrder::KnockOut);
}

void AAntKitchenGameMode::SpawnBody(AAntCharacter* Ant)
{
	if (!bHeist || !IsValid(Ant) || !Ant->bDead || Ant->Team != 0 || Ant->Creature != 0 || Ant->bCorpseGone || Ant->DeathKind == 2)
	{
		return;   // torn in two: nothing left to carry
	}
	AController* C = Ant->GetController();
	if (!C || !C->IsPlayerController())
	{
		return;   // bots hatch for free: only a player's body is worth a larva
	}
	FVector At = Ant->GetActorLocation();
	if (FVector::Dist2D(At, AKitchenBuilder::NestLocation(0)) < AKitchenBuilder::NestRadius(0) + 100.f)
	{
		return;   // died on the doorstep
	}
	FAntFoodDef D;
	D.Name = FString::Printf(TEXT("Тело: %s"), *Ant->GetDisplayName());
	D.Value = 0;
	D.Weight = 5;
	D.Color = Ant->BodyColor;
	D.Shape = EAntShape::Sphere;
	D.Scale = FVector(0.17f, 0.08f, 0.06f);
	At.Z += 6.f;
	AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (L)
	{
		L->InitFood(D);
		L->bBody = true;
		L->BodyOf = C;
		L->ExpireAt = GetWorld()->GetTimeSeconds() + (bRaid ? 90.f : 60.f);
		L->FinishSpawning(FTransform(At));
		Ant->StuckTo = nullptr;
		Ant->bCorpseGone = true;
		Ant->OnRep_Corpse();
		UE_LOG(LogTemp, Display, TEXT("ANTBODY body of %s lies at %s for %d s"), *Ant->GetDisplayName(), *At.ToCompactString(), bRaid ? 90 : 60);
	}
}

void AAntKitchenGameMode::CompleteOrder(uint8 OrderId)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!bHeist || !GS)
	{
		return;
	}
	const int32 i = GS->Orders.Find(OrderId);
	if (i == INDEX_NONE || (GS->OrdersDone & (1 << i)) || (GS->OrdersFailed & (1 << i)))
	{
		return;
	}
	GS->OrdersDone |= (uint8)(1 << i);
	const int32 Bonus = (bRaid && Raid) ? Raid->ContractBonus(OrderId) : 40 + 10 * GS->Shift;
	UE_LOG(LogTemp, Display, TEXT("ANTORDER done: %s (+%d)"), AntOrder::Name(OrderId), Bonus);
	Broadcast(FString::Printf(TEXT("ПРИКАЗ КОРОЛЕВЫ ВЫПОЛНЕН: %s (+%d)"), AntOrder::Name(OrderId), Bonus), RunGold);
	AddScore(0, Bonus);
}

void AAntKitchenGameMode::FailOrder(uint8 OrderId)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!bHeist || !GS)
	{
		return;
	}
	const int32 i = GS->Orders.Find(OrderId);
	if (i != INDEX_NONE && !(GS->OrdersDone & (1 << i)) && !(GS->OrdersFailed & (1 << i)))
	{
		GS->OrdersFailed |= (uint8)(1 << i);
		UE_LOG(LogTemp, Display, TEXT("ANTORDER failed: %s"), AntOrder::Name(OrderId));
	}
}

int32 AAntKitchenGameMode::GreedValue(int32 Value) const
{
	const AAntGameState* GS = GetGameState<AAntGameState>();
	return (bHeist && GS && GS->bQuotaMet && GS->Greed > 1.f) ? FMath::RoundToInt(Value * GS->Greed) : Value;
}

void AAntKitchenGameMode::Ping(AAntPlayerController* By, const FVector& Loc)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || !By)
	{
		return;
	}
	uint8 Kind = AntPing::Here;
	if (Human && FVector::Dist2D(Human->GetActorLocation(), Loc) < 900.f && Loc.Z < Human->GetActorLocation().Z + 2000.f)
	{
		Kind = AntPing::Human;
	}
	if (Kind == AntPing::Here)
	{
		for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
		{
			if (It->bTrophy && FVector::Dist(It->GetActorLocation(), Loc) < 350.f)
			{
				Kind = AntPing::Trophy;
				break;
			}
		}
	}
	if (Kind == AntPing::Here)
	{
		for (TActorIterator<AKitchenInteractable> It(GetWorld()); It; ++It)
		{
			if (It->DoorClass() > 0 && It->DistanceTo(Loc) < 260.f)
			{
				Kind = AntPing::Door;
				break;
			}
		}
	}
	uint8 Team = 0;
	FString Who = TEXT("?");
	if (const AAntPlayerState* PS = By->GetPlayerState<AAntPlayerState>())
	{
		Team = PS->Team;
		Who = PS->GetPlayerName();
	}
	UE_LOG(LogTemp, Display, TEXT("ANTPING %s: %s at %s"), *Who, AntPing::Name(Kind), *Loc.ToCompactString());
	GS->MulticastPing(Loc, Kind, Team, Who);
}

void AAntKitchenGameMode::CheckTrapped()
{
	for (TActorIterator<AAntCharacter> It(GetWorld()); It; ++It)
	{
		AAntCharacter* A = *It;
		if (!A || A->Team != 0 || A->Creature != 0)
		{
			continue;
		}
		int32 In = A->bDead ? -1 : KitchenContainers::SealedAt(GetWorld(), A->GetActorLocation());
		if (In >= 0 && !KitchenContainers::Get(In).bTrap)
		{
			In = -1;
		}
		const int32* Was = Run.Trapped.Find(A);
		if (In >= 0 && (!Was || *Was != In))
		{
			Run.Trapped.Add(A, In);
			UE_LOG(LogTemp, Display, TEXT("ANTTRAP %s is shut %s"), *A->GetDisplayName(), KitchenContainers::Get(In).Where);
			Broadcast(FString::Printf(TEXT("%s заперт %s! Откройте дверь"), *A->GetDisplayName(), KitchenContainers::Get(In).Where), RunBad);
			A->Toast(TEXT("Ты заперт! Изнутри дверь такая же тяжёлая: [E] и зови своих"));
		}
		else if (In < 0 && Was)
		{
			Run.Trapped.Remove(A);
		}
	}
	for (auto It = Run.Trapped.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
}
// =============================================================================================
// The shift inside the run
// =============================================================================================

void AAntKitchenGameMode::RunBeginPlay()
{
	UWorld* W = GetWorld();
	auto SpawnBox = [W](uint8 Kind, const FVector& Loc)
	{
		const FTransform TM(Loc);
		if (AKitchenNoiseBox* B = W->SpawnActorDeferred<AKitchenNoiseBox>(AKitchenNoiseBox::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			B->Kind = Kind;
			B->FinishSpawning(TM);
		}
	};
	// Things ants can switch on to make noise: the microwave (start button on its front) and the TV (the console on the stand).
	SpawnBox(0, FVector(2290.f, 3985.f, 1960.f));
	FVector Console(12500.f, -8250.f, 1160.f);
	if (const FKitchenArtInst* I = KitchenArt::Enabled() ? KitchenArt::Find(TEXT("SM_L_GameConsole")) : nullptr)
	{
		Console = FVector(I->Loc) + FVector(0.f, 0.f, 60.f);
	}
	SpawnBox(1, Console);
	SpawnHighProps();

	// Dev: -AntDo="12:tp 4500 2900 60;14:pull;20:bots 2;30:win" runs commands at those seconds (see DevCommand).
	FString Script;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntDo="), Script, false))
	{
		TArray<FString> Steps;
		Script.ParseIntoArray(Steps, TEXT(";"));
		for (const FString& Step : Steps)
		{
			FString At, Cmd;
			if (Step.Split(TEXT(":"), &At, &Cmd))
			{
				DevScript.Add(TPair<float, FString>(FCString::Atof(*At), Cmd.TrimStartAndEnd()));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTDEV script with %d steps"), DevScript.Num());
	}
	float WinAt = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntWinAt="), WinAt) && WinAt > 0.f)
	{
		DevScript.Add(TPair<float, FString>(WinAt, TEXT("win")));
	}
	GetWorldTimerManager().SetTimer(DevTimer, this, &AAntKitchenGameMode::DevScriptTick, 0.25f, true);
}

void AAntKitchenGameMode::SpawnHighProps()
{
	// Heavy things near an edge, up high: lean on one and it goes over. On the Human's head it stuns him; on a fire ant it is the end.
	struct FDef { const TCHAR* Name; EAntShape Shape; FVector Loc; FVector Size; FLinearColor Color; float Mass; };
	const FDef Defs[] =
	{
		{ TEXT("Крышка от кастрюли"), EAntShape::Cylinder, FVector(300.f, 3170.f, 1760.f),    FVector(230.f, 230.f, 26.f),  FLinearColor(0.72f, 0.74f, 0.78f), 4.f },
		{ TEXT("Кружка"),             EAntShape::Cylinder, FVector(-4700.f, 3170.f, 1820.f),  FVector(150.f, 150.f, 170.f), FLinearColor(0.2f, 0.45f, 0.8f),   5.f },   // level v2: the bread box stands where it was
		{ TEXT("Банка специй"),       EAntShape::Cylinder, FVector(2250.f, 3160.f, 1800.f),   FVector(110.f, 110.f, 180.f), FLinearColor(0.75f, 0.3f, 0.15f),  4.f },
		{ TEXT("Банка огурцов"),      EAntShape::Cylinder, FVector(3700.f, 3230.f, 3880.f),   FVector(190.f, 190.f, 270.f), FLinearColor(0.35f, 0.6f, 0.25f),  7.f },
		{ TEXT("Банка варенья"),      EAntShape::Cylinder, FVector(-6700.f, 300.f, 2780.f),   FVector(160.f, 160.f, 220.f), FLinearColor(0.6f, 0.1f, 0.2f),    6.f },
		{ TEXT("Тарелка"),            EAntShape::Cylinder, FVector(-1250.f, -1700.f, 1440.f), FVector(320.f, 320.f, 22.f),  FLinearColor(0.95f, 0.95f, 0.92f), 5.f },
		{ TEXT("Кружка"),             EAntShape::Cylinder, FVector(14450.f, -3150.f, 1090.f), FVector(150.f, 150.f, 170.f), FLinearColor(0.85f, 0.3f, 0.25f),  5.f },
	};
	for (const FDef& D : Defs)
	{
		const FTransform TM(D.Loc);
		if (AKitchenProp* P = GetWorld()->SpawnActorDeferred<AKitchenProp>(AKitchenProp::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			P->ShapeIndex = (uint8)D.Shape;
			P->Size = D.Size;
			P->Color = D.Color;
			P->Mass = D.Mass;
			P->PropName = D.Name;
			P->FinishSpawning(TM);
		}
	}
}

void AAntKitchenGameMode::SpawnContainerLoot()
{
	if (!bHeist || !KitchenArt::Enabled())
	{
		return;
	}
	// The good stuff is behind doors: cabinets, drawers, the fridge and the freezer, the TV stand. Floor crumbs stay the safe option.
	struct FSpot { FVector P; int32 Tier; };
	static const FSpot Spots[] =
	{
		// Level v2, the gradient rule: nothing of tier 3 closer than 25 s from the nest (three spots went down a tier).
		{ FVector(-6300.f, 3900.f, 210.f), 2 },  { FVector(-6650.f, 4350.f, 210.f), 2 },    // base cabinet by the toaster
		{ FVector(-2150.f, 3900.f, 210.f), 2 },  { FVector(-2450.f, 4350.f, 210.f), 2 },    // base cabinet left of the oven
		{ FVector(1900.f, 3900.f, 210.f), 2 },   { FVector(2250.f, 4350.f, 210.f), 3 },     // base cabinet under the microwave
		{ FVector(-6375.f, 4640.f, 3060.f), 3 }, { FVector(-5125.f, 4640.f, 3060.f), 2 },   // wall cabinets
		{ FVector(-3875.f, 4640.f, 3060.f), 3 }, { FVector(-2625.f, 4640.f, 3060.f), 2 },
		{ FVector(1500.f, 4640.f, 3060.f), 3 },  { FVector(2150.f, 4640.f, 3060.f), 2 },
		{ FVector(-4900.f, 3650.f, 840.f), 2 },  { FVector(-5200.f, 4050.f, 840.f), 2 },    // the second drawer
		{ FVector(3600.f, 4250.f, 2600.f), 3 },                                             // freezer
		{ FVector(4300.f, 3550.f, 970.f), 3 },   { FVector(3250.f, 4400.f, 1770.f), 2 },    // fridge shelves
		{ FVector(11800.f, -8400.f, 160.f), 3 }, { FVector(12700.f, -8450.f, 160.f), 2 },   // TV stand, behind its sliding doors
	};
	int32 N = 0;
	TArray<TWeakObjectPtr<AKitchenLoot>> Placed;
	TArray<FVector> Wanted;
	for (const FSpot& S : Spots)
	{
		if (!IsSpotFree(S.P))
		{
			continue;
		}
		if (AKitchenLoot* L = SpawnFood(S.P, S.Tier))
		{
			++N;
			Placed.Add(L);
			Wanted.Add(S.P);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTLOOT %d pieces placed behind doors"), N);
	if (FParse::Param(FCommandLine::Get(), TEXT("AntLootProbe")))
	{
		// Dev: where did each piece (and each trophy) come to rest? A spot that is not on a shelf shows up as a long fall.
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Placed, Wanted]()
		{
			for (int32 i = 0; i < Placed.Num(); ++i)
			{
				const AKitchenLoot* L = Placed[i].Get();
				UE_LOG(LogTemp, Display, TEXT("ANTLOOTPROBE spot %s -> %s  in=%d"), *Wanted[i].ToCompactString(), L ? *L->GetActorLocation().ToCompactString() : TEXT("GONE (broke or fell out)"),
					L ? KitchenContainers::At(L->GetActorLocation()) : -2);
			}
			for (TActorIterator<AKitchenLoot> It(GetWorld()); It; ++It)
			{
				if (It->bTrophy)
				{
					UE_LOG(LogTemp, Display, TEXT("ANTLOOTPROBE trophy «%s» at %s  in=%d"), *It->LootName, *It->GetActorLocation().ToCompactString(), KitchenContainers::At(It->GetActorLocation()));
				}
			}
			for (TActorIterator<AKitchenProp> It(GetWorld()); It; ++It)
			{
				if (!It->PropName.IsEmpty())
				{
					UE_LOG(LogTemp, Display, TEXT("ANTLOOTPROBE prop «%s» at %s"), *It->PropName, *It->GetActorLocation().ToCompactString());
				}
			}
		}), 7.f, false);
	}
}

void AAntKitchenGameMode::RunStartShift()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	if (GetWorldTimerManager().IsTimerActive(EventTimer) || GS->EventId != 0)
	{
		EndEvent();
	}
	// A new run starts clean: the cards belong to the run that was lost. (A raid: the nest has set them already.)
	if (ShiftNumber == 1 && !bRaid)
	{
		GS->Cards.Reset();
		GS->RunShifts = 0;
		GS->RunHaul = 0;
		FString Grant;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntCards="), Grant, false))   // dev: start the run with these cards
		{
			TArray<FString> Ids;
			Grant.ParseIntoArray(Ids, TEXT(","));
			for (const FString& Id : Ids) { GS->Cards.AddUnique((uint8)FMath::Clamp(FCString::Atoi(*Id), 0, AntCard::Count - 1)); }
		}
	}
	const float Length = MatchLength + (HasCard(AntCard::Overtime) ? 45.f : 0.f);
	GS->TimeRemaining = Length;
	if (HasCard(AntCard::Brood))
	{
		GS->Larvae += 2;
	}
	GS->LarvaeStart = GS->Larvae;
	GS->Greed = 1.f;
	GS->Interlude = 0;
	GS->CardOffer.Reset();
	GS->CardVotes.Reset();
	GS->CardPicked = 255;
	GS->Awards.Reset();
	GS->HumanGoal = AntHumanGoal::Idle;
	GS->HumanGoalText = FString();

	Run.Noises.Reset();
	Run.CheckedSource.Reset();
	for (float& T : Run.CheckedKind) { T = 0.f; }
	Run.Votes.Reset();
	Run.Trapped.Reset();
	Run.OutPlayers.Reset();
	Run.DoorsOpened = 0;
	Run.LivingDelivered = 0;
	Run.Distractions = 0;
	Run.DeathsThisShift = 0;
	Run.GreedClock = 0.f;
	Run.bEventDone = false;

	// Three orders from the Queen.
	TArray<uint8> Pool;
	for (uint8 i = 0; i < AntOrder::Count; ++i) { Pool.Add(i); }
	GS->Orders.Reset();
	GS->OrdersDone = 0;
	GS->OrdersFailed = 0;
	FString Forced;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntOrders="), Forced, false))   // dev: -AntOrders=0,8,11
	{
		TArray<FString> Ids;
		Forced.ParseIntoArray(Ids, TEXT(","));
		for (const FString& Id : Ids)
		{
			const uint8 O = (uint8)FMath::Clamp(FCString::Atoi(*Id), 0, AntOrder::Count - 1);
			if (GS->Orders.Num() < 3 && Pool.Remove(O) > 0) { GS->Orders.Add(O); }
		}
	}
	while (GS->Orders.Num() < 3 && Pool.Num() > 0)
	{
		const int32 k = Rng.RandRange(0, Pool.Num() - 1);
		GS->Orders.Add(Pool[k]);
		Pool.RemoveAtSwap(k);
	}

	// One event, some time after the Human has walked in.
	const float Quiet = 75.f + (HasCard(AntCard::Phone) ? 30.f : 0.f);
	Run.EventPlanned = (uint8)Rng.RandRange(1, AntEvent::Count - 1);
	Run.EventAt = Quiet + Rng.FRandRange(15.f, FMath::Max(20.f, Length - Quiet - 170.f));
	int32 ForcedEvent = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AntEvent="), ForcedEvent) && ForcedEvent > 0)   // dev: -AntEvent=3 [-AntEventAt=20]
	{
		Run.EventPlanned = (uint8)FMath::Clamp(ForcedEvent, 1, AntEvent::Count - 1);
		float At = 20.f;
		FParse::Value(FCommandLine::Get(), TEXT("AntEventAt="), At);
		Run.EventAt = At;
	}

	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		It->ResetShiftStats();
		It->Delivered = 0;
		It->Deaths = 0;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRUN shift %d: orders [%s | %s | %s], event «%s» at %.0f s, cards owned %d, larvae %d"), ShiftNumber,
		GS->Orders.Num() > 0 ? AntOrder::Name(GS->Orders[0]) : TEXT("-"), GS->Orders.Num() > 1 ? AntOrder::Name(GS->Orders[1]) : TEXT("-"),
		GS->Orders.Num() > 2 ? AntOrder::Name(GS->Orders[2]) : TEXT("-"), AntEvent::Name(Run.EventPlanned), Run.EventAt, GS->Cards.Num(), GS->Larvae);
}

void AAntKitchenGameMode::RunTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Phase != 0)
	{
		return;
	}
	const float Length = MatchLength + (HasCard(AntCard::Overtime) ? 45.f : 0.f);
	const float Gone = Length - GS->TimeRemaining;

	// Greed: the quota is in, somebody is still out there: everything brought from now on is worth more and more.
	if (GS->bQuotaMet && GS->Greed < 2.f && AnyAllyPlayerAlive(true))
	{
		Run.GreedClock += 1.f;
		const float Step = HasCard(AntCard::Greedy) ? 10.f : 15.f;
		if (Run.GreedClock >= Step)
		{
			Run.GreedClock -= Step;
			GS->Greed = FMath::Min(2.f, GS->Greed + 0.1f);
			UE_LOG(LogTemp, Display, TEXT("ANTGREED x%.1f"), GS->Greed);
		}
	}
	if (GS->bQuotaMet && GS->TeamScore * 2 >= GS->TargetScore * 3)
	{
		CompleteOrder(AntOrder::Overfill);
	}
	CheckTrapped();
	if (!Run.bEventDone && Run.EventPlanned != 0 && Gone >= Run.EventAt && GS->ShiftStage < 3)
	{
		StartEvent(Run.EventPlanned);
	}
	if (GS->ShiftStage == 0 || HumanPlayer || (Human && Human->IsKnockedOut()))
	{
		SetHumanGoal(AntHumanGoal::Idle, FString(), FVector::ZeroVector);   // asleep, played by somebody, or running away: no plan to show
	}
}

void AAntKitchenGameMode::BuildAwards()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	GS->Awards.Reset();
	TArray<AAntPlayerState*> All;
	for (TActorIterator<AAntPlayerState> It(GetWorld()); It; ++It)
	{
		if (It->Team == 0 && !It->bIsHuman) { All.Add(*It); }
	}
	auto Best = [&All](TFunctionRef<float(const AAntPlayerState*)> Get, float Min) -> AAntPlayerState*
	{
		AAntPlayerState* Top = nullptr;
		float TopV = Min;
		for (AAntPlayerState* PS : All)
		{
			const float V = Get(PS);
			if (V >= TopV && (V > TopV || !Top)) { Top = PS; TopV = V; }
		}
		return Top;
	};
	auto Add = [GS](const FString& Line) { if (GS->Awards.Num() < 5) { GS->Awards.Add(Line); } };
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->Delivered; }, 1.f))
	{
		Add(FString::Printf(TEXT("Грузчик смены — %s (принёс %d)"), *P->GetPlayerName(), P->Delivered));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->Squashed; }, 1.f))
	{
		Add(FString::Printf(TEXT("Лепёшка дня — %s (расплющен: %d)"), *P->GetPlayerName(), P->Squashed));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->TrophyBreaks; }, 1.f))
	{
		Add(FString::Printf(TEXT("Руки-крюки — %s (разбил трофей)"), *P->GetPlayerName()));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return S->RideTime; }, 3.f))
	{
		Add(FString::Printf(TEXT("Наездник — %s (%d с верхом на Человеке)"), *P->GetPlayerName(), FMath::RoundToInt(P->RideTime)));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->Torn; }, 1.f))
	{
		Add(FString::Printf(TEXT("Мясник — %s (порвал врагов: %d)"), *P->GetPlayerName(), P->Torn));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->DoorsOpened; }, 1.f))
	{
		Add(FString::Printf(TEXT("Швейцар — %s (открыл дверей: %d)"), *P->GetPlayerName(), P->DoorsOpened));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->LegsLost; }, 1.f))
	{
		Add(FString::Printf(TEXT("Безногий — %s (потерял лап: %d)"), *P->GetPlayerName(), P->LegsLost));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->BodiesHome; }, 1.f))
	{
		Add(FString::Printf(TEXT("Санитар — %s (принёс тел: %d)"), *P->GetPlayerName(), P->BodiesHome));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->Rams; }, 1.f))
	{
		Add(FString::Printf(TEXT("Таран — %s (сбил с ног: %d)"), *P->GetPlayerName(), P->Rams));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return (float)S->Deaths; }, 1.f))
	{
		Add(FString::Printf(TEXT("Девять жизней — %s (погиб: %d)"), *P->GetPlayerName(), P->Deaths));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return S->Deaths == 0 ? 1.f : 0.f; }, 1.f))
	{
		Add(FString::Printf(TEXT("Заговорённый — %s (ни царапины)"), *P->GetPlayerName()));
	}
	if (AAntPlayerState* P = Best([](const AAntPlayerState* S) { return -(float)S->Delivered; }, -1.e9f))
	{
		Add(FString::Printf(TEXT("Отдыхающий — %s (принёс %d)"), *P->GetPlayerName(), P->Delivered));
	}
	for (const FString& A : GS->Awards)
	{
		UE_LOG(LogTemp, Display, TEXT("ANTAWARD %s"), *A);
	}
}
void AAntKitchenGameMode::RunEndShift(bool bWon)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	EndEvent();
	if (bWon)
	{
		if (Run.DeathsThisShift == 0) { CompleteOrder(AntOrder::NoDeaths); }
		if (GS->TimeRemaining >= 120.f) { CompleteOrder(AntOrder::Early); }
		if (GS->TrophiesBroken == 0) { CompleteOrder(AntOrder::NoBreak); }
	}
	for (int32 i = 0; i < GS->Orders.Num(); ++i)
	{
		if (!(GS->OrdersDone & (1 << i))) { GS->OrdersFailed |= (uint8)(1 << i); }
	}
	for (TActorIterator<AKitchenNoiseBox> It(GetWorld()); It; ++It)
	{
		It->SwitchOff();
	}
	BuildAwards();
	GS->RunHaul += GS->TeamScore;
	if (bWon)
	{
		GS->RunShifts = ShiftNumber;
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRUN shift %d %s: haul %d, run so far: %d shifts, %d haul, orders done mask %d"), ShiftNumber, bWon ? TEXT("WON") : TEXT("LOST"),
		GS->TeamScore, GS->RunShifts, GS->RunHaul, (int32)GS->OrdersDone);
	if (!bWon && !bRaid)
	{
		GS->MulticastRunOver(GS->RunShifts, GS->RunHaul);
	}
}

void AAntKitchenGameMode::InterludeTick()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS)
	{
		return;
	}
	const bool bFast = FParse::Param(FCommandLine::Get(), TEXT("AntFastVote"));
	if (GS->Interlude == 1)
	{
		// The vote: three cards the colony does not own yet.
		TArray<uint8> Pool;
		for (uint8 i = 0; i < AntCard::Count; ++i)
		{
			if (!GS->Cards.Contains(i)) { Pool.Add(i); }
		}
		GS->CardOffer.Reset();
		while (GS->CardOffer.Num() < 3 && Pool.Num() > 0)
		{
			const int32 k = Rng.RandRange(0, Pool.Num() - 1);
			GS->CardOffer.Add(Pool[k]);
			Pool.RemoveAtSwap(k);
		}
		if (GS->CardOffer.Num() == 0)
		{
			GS->Interlude = 0;
			ResetMatch();
			return;
		}
		GS->CardVotes.Init(0, GS->CardOffer.Num());
		GS->CardPicked = 255;
		Run.Votes.Reset();
		GS->Interlude = 2;
		const float T = bFast ? 8.f : 20.f;
		GS->InterludeEnds = GS->GetServerWorldTimeSeconds() + T;
		GetWorldTimerManager().SetTimer(RunTimer, this, &AAntKitchenGameMode::ResolveCards, T, false);
		UE_LOG(LogTemp, Display, TEXT("ANTCARD offer: [1] %s  [2] %s  [3] %s"), AntCard::Name(GS->CardOffer[0]),
			GS->CardOffer.Num() > 1 ? AntCard::Name(GS->CardOffer[1]) : TEXT("-"), GS->CardOffer.Num() > 2 ? AntCard::Name(GS->CardOffer[2]) : TEXT("-"));
		Broadcast(TEXT("Выбор карты: голосуйте клавишами 1, 2, 3"), RunGold);
		int32 Auto = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntAutoVote="), Auto) && Auto >= 1)   // dev: the host votes by itself after 3 s
		{
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Auto]()
			{
				VoteCard(GetWorld()->GetFirstPlayerController(), Auto - 1);
			}), 3.f, false);
		}
	}
	else if (GS->Interlude == 3)
	{
		GS->Interlude = 0;
		ResetMatch();
	}
}

void AAntKitchenGameMode::VoteCard(AController* By, int32 Option)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Interlude != 2 || !By || !GS->CardOffer.IsValidIndex(Option))
	{
		return;
	}
	Run.Votes.Add(By, Option);
	GS->CardVotes.Init(0, GS->CardOffer.Num());
	for (const auto& V : Run.Votes)
	{
		if (V.Key.IsValid() && GS->CardVotes.IsValidIndex(V.Value)) { GS->CardVotes[V.Value]++; }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCARD vote: %s -> [%d] %s"), By->PlayerState ? *By->PlayerState->GetPlayerName() : TEXT("?"), Option + 1, AntCard::Name(GS->CardOffer[Option]));
	// Everybody has voted: no need to wait for the clock.
	int32 Players = 0, Voted = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->IsValid())
		{
			++Players;
			if (Run.Votes.Contains(It->Get())) { ++Voted; }
		}
	}
	if (Voted >= Players)
	{
		GetWorldTimerManager().ClearTimer(RunTimer);
		ResolveCards();
	}
}

void AAntKitchenGameMode::ResolveCards()
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->Interlude != 2 || GS->CardOffer.Num() == 0)
	{
		return;
	}
	int32 Top = 0;
	for (uint8 V : GS->CardVotes) { Top = FMath::Max(Top, (int32)V); }
	TArray<int32> Winners;
	for (int32 i = 0; i < GS->CardOffer.Num(); ++i)
	{
		if (!GS->CardVotes.IsValidIndex(i) || GS->CardVotes[i] == Top) { Winners.Add(i); }
	}
	const int32 Pick = Winners[Rng.RandRange(0, Winners.Num() - 1)];   // a tie (or nobody voted): the Queen decides
	const uint8 Card = GS->CardOffer[Pick];
	GS->Cards.AddUnique(Card);
	GS->CardPicked = Card;
	GS->Interlude = 3;
	GS->InterludeEnds = GS->GetServerWorldTimeSeconds() + 4.f;
	UE_LOG(LogTemp, Display, TEXT("ANTCARD picked: %s (votes %d, tie between %d)"), AntCard::Name(Card), Top, Winners.Num());
	Broadcast(FString::Printf(TEXT("Карта колонии: %s — %s"), AntCard::Name(Card), AntCard::Text(Card)), RunGold);
	GetWorldTimerManager().SetTimer(RunTimer, this, &AAntKitchenGameMode::InterludeTick, 4.f, false);
}

// =============================================================================================
// Events: one per shift
// =============================================================================================

void AAntKitchenGameMode::StartEvent(uint8 Id)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!bHeist || !GS || Id == 0 || Id >= AntEvent::Count || GS->Phase != 0)
	{
		return;
	}
	if (GS->EventId != 0)
	{
		EndEvent();
	}
	Run.bEventDone = true;
	UWorld* W = GetWorld();
	const float Len = AntEvent::Length(Id);
	const float Now = W->GetTimeSeconds();
	FVector Where = FVector::ZeroVector;
	switch (Id)
	{
	case AntEvent::Blackout:
		for (TActorIterator<AKitchenSwitch> It(W); It; ++It)
		{
			Run.bLightsBefore = It->bLightsOn;
			It->SetLights(false);
			Where = It->GetActorLocation();
		}
		break;
	case AntEvent::Cat:
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			if (It->Kind == 1)
			{
				It->Hunt(Len);
				Where = It->GetActorLocation();
			}
		}
		break;
	case AntEvent::Zoomies:
		for (TActorIterator<ACritter> It(W); It; ++It)
		{
			if (It->Kind == 0)
			{
				It->Zoomies(Len);
				Where = It->GetActorLocation();
			}
		}
		break;
	case AntEvent::Vacuum:
	{
		Where = FVector(5300.f, -3600.f, ARobotVacuum::Height * 0.5f + 2.f);
		const FTransform TM(FRotator(0.f, 160.f, 0.f), Where);
		if (ARobotVacuum* V = W->SpawnActorDeferred<ARobotVacuum>(ARobotVacuum::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			V->Life = Len;
			V->FinishSpawning(TM);
			Run.EventActors.Add(V);
		}
		break;
	}
	case AntEvent::Flood:
	{
		Where = FVector(-3500.f, 2200.f, 1.f);
		const FTransform TM(Where);
		if (AKitchenFlood* F = W->SpawnActorDeferred<AKitchenFlood>(AKitchenFlood::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			F->Life = Len;
			F->FinishSpawning(TM);
			Run.EventActors.Add(F);
		}
		break;
	}
	case AntEvent::Pizza:
	{
		Where = FVector(5100.f, -3500.f, 0.f);
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APizzaBox* Box = W->SpawnActor<APizzaBox>(APizzaBox::StaticClass(), FTransform(Where), P))
		{
			Run.EventActors.Add(Box);
		}
		FAntFoodDef D;
		D.Name = TEXT("Кусок пиццы");
		D.Value = 45;
		D.Weight = 9;
		D.Color = FLinearColor(0.95f, 0.66f, 0.25f);
		D.Shape = EAntShape::Cube;
		D.Scale = FVector(0.42f, 0.32f, 0.07f);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = i * PI / 3.f + 0.3f;
			const FVector At = Where + FVector(FMath::Cos(A) * 380.f, FMath::Sin(A) * 380.f, APizzaBox::Thick + 30.f);
			if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(At), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				L->InitFood(D);
				L->ExpireAt = Now + Len;
				L->FinishSpawning(FTransform(At));
			}
		}
		break;
	}
	case AntEvent::FridgeOpen:
		if (FridgeDoor)
		{
			FridgeDoor->SetOpen(true, Len);
			Where = FridgeDoor->GetActorLocation();
		}
		break;
	default:
		break;
	}
	GS->EventId = Id;
	GS->EventEnds = GS->GetServerWorldTimeSeconds() + Len;
	GS->EventLoc = Where;
	UE_LOG(LogTemp, Display, TEXT("ANTEVENT start %d «%s» for %.0f s at %s"), (int32)Id, AntEvent::Name(Id), Len, *Where.ToCompactString());
	Broadcast(FString::Printf(TEXT("%s. %s"), AntEvent::Name(Id), AntEvent::Text(Id)), RunGold);
	GetWorldTimerManager().SetTimer(EventTimer, this, &AAntKitchenGameMode::EndEvent, Len, false);
}

void AAntKitchenGameMode::EndEvent()
{
	GetWorldTimerManager().ClearTimer(EventTimer);
	AAntGameState* GS = GetGameState<AAntGameState>();
	if (!GS || GS->EventId == 0)
	{
		return;
	}
	if (GS->EventId == AntEvent::Blackout && Run.bLightsBefore)
	{
		for (TActorIterator<AKitchenSwitch> It(GetWorld()); It; ++It)
		{
			It->SetLights(true);
		}
		Broadcast(TEXT("Свет дали"), RunInfo);
	}
	for (const TWeakObjectPtr<AActor>& A : Run.EventActors)
	{
		if (A.IsValid()) { A->Destroy(); }
	}
	Run.EventActors.Reset();
	UE_LOG(LogTemp, Display, TEXT("ANTEVENT end %d «%s»"), (int32)GS->EventId, AntEvent::Name(GS->EventId));
	GS->EventId = 0;
}
// =============================================================================================
// Dev: a scripted test run. -AntDo="12:tp 4500 2900 60;14:pull;20:bots 2;30:win" (seconds since the map started).
// Nobody can press keys in an automated run, so every feature has a command here that exercises it.
// =============================================================================================

void AAntKitchenGameMode::DevScriptTick()
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < DevScript.Num(); )
	{
		if (DevScript[i].Key <= Now)
		{
			const FString Cmd = DevScript[i].Value;
			DevScript.RemoveAt(i);
			UE_LOG(LogTemp, Display, TEXT("ANTDEV %.1f: %s"), Now, *Cmd);
			DevCommand(Cmd);
		}
		else
		{
			++i;
		}
	}
	// "q <command>": a queue. The next one runs a second after the one before it, and never while the walker ("go") is still
	// on its way: a script that does not depend on how long a walk takes. "q wait N" waits N seconds.
	if (DevQueue.Num() > 0 && DevGoPts.Num() == 0 && Now >= DevQueueAt)
	{
		const FString Cmd = DevQueue[0];
		DevQueue.RemoveAt(0);
		DevQueueAt = Now + 1.f;
		UE_LOG(LogTemp, Display, TEXT("ANTDEV %.1f (queue, %d left): %s"), Now, DevQueue.Num(), *Cmd);
		if (Cmd.StartsWith(TEXT("wait "))) { DevQueueAt = Now + FCString::Atof(*Cmd.Mid(5)); }
		else { DevCommand(Cmd); }
	}
	// "go x y z ...": the player's ant walks the points one after another (AAntCharacter::bDevGo steers it every frame).
	if (DevGoPts.Num() > 0)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
		if (!Me || Me->bDead)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTV2 go: FAILED, the ant is dead or gone (%d points left, %d queued commands dropped)"), DevGoPts.Num(), DevQueue.Num());
			DevQueue.Reset();
			DevGoPts.Reset();
			if (Me) { Me->bDevGo = false; }
			return;
		}
		const FVector P = Me->GetActorLocation();
		const FVector T = DevGoPts[0];
		const float D = (float)FVector::Dist(P, T);
		const AKitchenLoot* C = Me->Carrying;
		if (FVector::Dist2D(P, T) < 70.f && FMath::Abs(P.Z - T.Z) < 140.f)
		{
			DevGoPts.RemoveAt(0);
			if (DevGoDetourPts > 0) { DevGoDetourPts = 0; }
			else { DevGoDetours = 0; UE_LOG(LogTemp, Display, TEXT("ANTV2 go: reached (%.0f, %.0f, %.0f) at %.1f s, %d left, load %s"), T.X, T.Y, T.Z, Now - DevGoStart, DevGoPts.Num(), C ? *C->LootName : TEXT("-")); }
			DevGoBest = 1.e9f;
			DevGoBestAt = Now;
			if (DevGoPts.Num() == 0)
			{
				Me->bDevGo = false;
				UE_LOG(LogTemp, Display, TEXT("ANTV2 go: DONE in %.1f s at %s, load %s (ratio %.2f, carriers %d)"), Now - DevGoStart, *P.ToCompactString(), C ? *C->LootName : TEXT("-"), C ? C->CarryRatio() : 0.f, C ? C->NumCarriers() : 0);
			}
			else
			{
				Me->DevGoTo = DevGoPts[0];
			}
			return;
		}
		Me->DevGoTo = T;
		Me->bDevGo = true;
		if (D < DevGoBest - 12.f)
		{
			DevGoBest = D;
			DevGoBestAt = Now;
		}
		else if (Now - DevGoBestAt > 3.5f && DevGoDetours < 12)
		{
			// Something in the way (a table leg, a slipper, the rug's lip): a step aside - left, then right, wider each time -
			// and on. A crude way round for a walker that only knows straight lines.
			if (DevGoDetourPts > 0) { DevGoPts.RemoveAt(0); DevGoDetourPts = 0; }
			const FVector Real = DevGoPts[0];
			const FVector Dir = (Real - P).GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir) * ((DevGoDetours % 2) ? -1.f : 1.f);
			const float Step = 240.f + 110.f * (DevGoDetours / 2);
			const FVector Aside(P.X + Side.X * Step - Dir.X * 80.f, P.Y + Side.Y * Step - Dir.Y * 80.f, P.Z);
			DevGoPts.Insert(Aside, 0);
			DevGoDetourPts = 1;
			++DevGoDetours;
			DevGoBest = 1.e9f;
			DevGoBestAt = Now;
			UE_LOG(LogTemp, Display, TEXT("ANTV2 go: no way on at %s, detour %d to (%.0f, %.0f)"), *P.ToCompactString(), DevGoDetours, Aside.X, Aside.Y);
			return;
		}
		else if (Now - DevGoBestAt > 12.f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTV2 go: STUCK at %s, %.0f from (%.0f, %.0f, %.0f), %d points left, load %s (ratio %.2f, carriers %d), falling %d"), *P.ToCompactString(), D, T.X, T.Y, T.Z,
				DevGoPts.Num(), C ? *C->LootName : TEXT("-"), C ? C->CarryRatio() : 0.f, C ? C->NumCarriers() : 0, Me->GetCharacterMovement()->IsFalling() ? 1 : 0);
			if (DevQueue.Num() > 0) { UE_LOG(LogTemp, Display, TEXT("ANTDEV queue: %d commands dropped (the walker is stuck)"), DevQueue.Num()); }
			DevQueue.Reset();
			DevGoPts.Reset();
			Me->bDevGo = false;
			return;
		}
		if (Now >= DevGoLogAt)
		{
			DevGoLogAt = Now + 2.f;
			UE_LOG(LogTemp, Display, TEXT("ANTV2 go: at (%.0f, %.0f, %.0f) speed %.0f, %.0f to go, load %s (ratio %.2f, carriers %d)"), P.X, P.Y, P.Z, Me->GetVelocity().Size(), D,
				C ? *C->LootName : TEXT("-"), C ? C->CarryRatio() : 0.f, C ? C->NumCarriers() : 0);
		}
	}
}

void AAntKitchenGameMode::DevCommand(const FString& Cmd)
{
	if (Cmd.StartsWith(TEXT("q ")))
	{
		DevQueue.Add(Cmd.Mid(2).TrimStartAndEnd());
		return;
	}
	TArray<FString> A;
	Cmd.ParseIntoArrayWS(A);
	if (A.Num() == 0)
	{
		return;
	}
	if (NpcWorld::DevCommand(this, A))
	{
		return;   // the pets and the family: cat ..., dog ..., gran ..., kid ..., npclevel N (CritterBrain.cpp)
	}
	if (AntMess::DevCommand(this, A))
	{
		return;   // mess stain|test|break|splat|burst|jamwalk|walk|dust|stir|clean|fill|hide|cost|profilegpu|info (AntMess.cpp); before NpcRoutine: its bare "mess" would swallow these
	}
	if (NpcRoutine::DevCommand(this, A))
	{
		return;   // level v3, the residents' evening and their mess: routine plan|info|warp S|skip who|goto step|off|on, mess, react <fixture>
	}
	if (AntSnowball::DevCommand(this, A))
	{
		return;   // snow ..., trail ..., rts ..., perf ..., squad+ ... (AntSnowball.cpp)
	}
	if (Raid && Raid->DevCommand(A))
	{
		return;   // raid ..., nest ... (AntRaid.cpp)
	}
	if (AntCarry::DevCommand(this, A))
	{
		return;   // carrying together: carry spawn|crew|go|desync|yank|stop|kill|human|rivals|army|workers|ramp|corner|cam|shot|log|info (AntCarry.cpp)
	}
	if (AntWreck::DevCommand(this, A) || AntWeapons::DevCommand(this, A) || AntVersus::DevCommand(this, A))
	{
		return;   // wreck ..., weapon ..., rage N, versus ... (AntWreck.cpp, AntWeapon.cpp, AntVersus.cpp)
	}
	if (AntBodyRules::DevCommand(this, A))
	{
		return;   // the ant's own body: ant info | hp N | leg N | hit .. | knock .. | die kind | cam .. | slomo X (AntBody.cpp)
	}
	const FString Op = A[0].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	auto Vec = [&Num](int32 i) { return FVector(Num(i), Num(i + 1), Num(i + 2)); };
	UWorld* W = GetWorld();
	AAntGameState* GS = GetGameState<AAntGameState>();
	APlayerController* PC = W->GetFirstPlayerController();
	AAntCharacter* Me = PC ? Cast<AAntCharacter>(PC->GetPawn()) : nullptr;
	auto Bots = [W](uint8 Team)
	{
		TArray<AAntCharacter*> Out;
		for (TActorIterator<AAntCharacter> It(W); It; ++It)
		{
			if (!It->bDead && It->Creature == 0 && It->Team == Team && !It->IsPlayerControlled()) { Out.Add(*It); }
		}
		return Out;
	};

	if (Op == TEXT("tp") && Me) { Me->ApplyTeleport(Vec(1)); }
	else if (Op == TEXT("pull") && Me) { Me->DevInteract(); UE_LOG(LogTemp, Display, TEXT("ANTDEV pulling=%s"), Me->Pulling ? *Me->Pulling->GetName() : TEXT("-")); }
	else if (Op == TEXT("release") && Me) { Me->Pulling = nullptr; }
	else if (Op == TEXT("interact") && Me) { Me->DevInteract(); }
	else if (Op == TEXT("look") && PC) { PC->SetControlRotation(FRotator(Num(2), Num(1), 0.f)); }   // look yaw pitch
	else if (Op == TEXT("trophies")) { for (int32 i = 0; i < 16; ++i) { SpawnTrophy(i); } }      // one on every trophy spot
	else if (Op == TEXT("shot") && PC) { PC->ConsoleCommand(TEXT("Shot showui"), false); }        // a screenshot at an exact moment (Saved/Screenshots)
	else if (Op == TEXT("hits"))
	{
		// hits x y z dx dy dz: every component a ray meets on the pawn channel (what stops an ant there)
		const FVector HA = Vec(1), HB = HA + Vec(4);
		TArray<FHitResult> Hs;
		FCollisionQueryParams Q(TEXT("AntDevHits"), true);
		FCollisionObjectQueryParams OQ;
		OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic); OQ.AddObjectTypesToQuery(ECC_PhysicsBody); OQ.AddObjectTypesToQuery(ECC_AntClimbable);
		W->LineTraceMultiByObjectType(Hs, HA, HB, OQ, Q);
		TArray<FOverlapResult> Os;
		W->OverlapMultiByChannel(Os, HA, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.f), Q);
		if (Me)
		{
			// the player ant's own capsule swept along the ray
			TArray<FHitResult> Sw;
			FCollisionQueryParams SQ(TEXT("AntDevSweep"), false, Me);
			UCapsuleComponent* Cap = Me->GetCapsuleComponent();
			W->SweepMultiByChannel(Sw, HA, HB, Cap->GetComponentQuat(), ECC_Pawn, Cap->GetCollisionShape(), SQ);
			for (const FHitResult& H : Sw) { UE_LOG(LogTemp, Display, TEXT("ANTHITS sweep %s.%s at %s n %s t %.2f start-pen %d"), *GetNameSafe(H.GetActor()), *GetNameSafe(H.GetComponent()), *H.ImpactPoint.ToCompactString(), *H.ImpactNormal.ToCompactString(), H.Time, H.bStartPenetrating ? 1 : 0); }
		}
		for (const FHitResult& H : Hs) { UE_LOG(LogTemp, Display, TEXT("ANTHITS ray %s.%s type %d pawn %d at %s n %s"), *GetNameSafe(H.GetActor()), *GetNameSafe(H.GetComponent()), H.GetComponent() ? (int32)H.GetComponent()->GetCollisionObjectType() : -1, H.GetComponent() ? (int32)H.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) : -1, *H.ImpactPoint.ToCompactString(), *H.ImpactNormal.ToCompactString()); }
		for (const FOverlapResult& O : Os) { UE_LOG(LogTemp, Display, TEXT("ANTHITS overlap %s.%s"), *GetNameSafe(O.GetActor()), *GetNameSafe(O.GetComponent())); }
		UE_LOG(LogTemp, Display, TEXT("ANTHITS done %d ray, %d overlap"), Hs.Num(), Os.Num());
	}
	else if (Op == TEXT("probe"))
	{
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (It->bTrophy)
			{
				UE_LOG(LogTemp, Display, TEXT("ANTLOOTPROBE trophy «%s» at %s  in=%d"), *It->LootName, *It->GetActorLocation().ToCompactString(), KitchenContainers::At(It->GetActorLocation()));
			}
		}
		for (TActorIterator<AKitchenProp> It(W); It; ++It)
		{
			if (!It->PropName.IsEmpty())
			{
				UE_LOG(LogTemp, Display, TEXT("ANTLOOTPROBE prop «%s» at %s"), *It->PropName, *It->GetActorLocation().ToCompactString());
			}
		}
	}
	else if (Op == TEXT("caste") && Me) { Me->SetClass((EAntClass)FMath::Clamp((int32)Num(1), 0, 3), true); }
	else if (Op == TEXT("bots") && Me)
	{
		// Brings N ally bots right next to the player (they help with a door when they stand at it with empty jaws).
		int32 N = (int32)Num(1, 1.f);
		for (AAntCharacter* B : Bots(0))
		{
			if (N-- <= 0) { break; }
			B->LetGo();
			B->ApplyTeleport(Me->GetActorLocation() + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 10.f));
		}
	}
	else if (Op == TEXT("enemy") && Me)
	{
		// Puts a fire ant right in front of the player (to ram or to bite).
		TArray<AAntCharacter*> E = Bots(1);
		if (E.Num() > 0)
		{
			const FVector Fwd = Me->GetActorForwardVector();
			E[0]->ApplyTeleport(Me->GetActorLocation() + Fwd * 14.f + FVector(0.f, 0.f, 2.f));
			if (AKitchenLoot* L = E[0]->FindNearestLoot(4000.f))
			{
				if (!L->bIsGear && L->Weight <= 4)
				{
					L->SetActorLocation(E[0]->GetActorLocation() + Fwd * 8.f, false, nullptr, ETeleportType::TeleportPhysics);
					E[0]->GrabLoot(L);
				}
			}
		}
	}
	else if (Op == TEXT("dash") && Me) { Me->Dash(); }
	else if ((Op == TEXT("botto") || Op == TEXT("botgo")) && Bots(0).Num() > 0)
	{
		// The first ally bot: put it somewhere / tell it to walk to a point (it shoves whatever stands in its way).
		AAntCharacter* B = Bots(0)[0];
		if (Op == TEXT("botto")) { B->LetGo(); B->ApplyTeleport(Vec(1)); }
		else if (AAntBotController* BC = Cast<AAntBotController>(B->GetController())) { BC->SetOrder(2, Me, Vec(1)); }
	}
	else if (Op == TEXT("army") && Me) { Me->DevCommand(0, Me->GetActorLocation()); }
	else if (Op == TEXT("event")) { StartEvent((uint8)Num(1, 1.f)); }
	else if (Op == TEXT("win") && GS && GS->Phase == 0)
	{
		GS->TeamScore = FMath::Max(GS->TeamScore, GS->TargetScore);
		GS->bQuotaMet = true;
		EndMatch(1);
	}
	else if (Op == TEXT("lose")) { EndMatch(2); }
	else if (Op == TEXT("score")) { AddScore(0, (int32)Num(1, 50.f)); }
	else if (Op == TEXT("vote")) { VoteCard(PC, (int32)Num(1, 1.f) - 1); }
	else if (Op == TEXT("card") && GS) { GS->Cards.AddUnique((uint8)FMath::Clamp((int32)Num(1), 0, AntCard::Count - 1)); }
	else if (Op == TEXT("noise")) { MakeNoise(Vec(2), 0.9f, (uint8)FMath::Clamp((int32)Num(1, 5.f), 1, AntNoise::Count - 1)); }
	else if (Op == TEXT("ping") && PC) { Ping(Cast<AAntPlayerController>(PC), A.Num() >= 4 ? Vec(1) : (Me ? Me->GetActorLocation() : FVector::ZeroVector)); }
	else if (Op == TEXT("open")) { KitchenContainers::SetOpen(W, (int32)Num(1), true); }
	else if (Op == TEXT("shut")) { KitchenContainers::SetOpen(W, (int32)Num(1), false); }
	else if (Op == TEXT("tv") || Op == TEXT("micro"))
	{
		for (TActorIterator<AKitchenNoiseBox> It(W); It; ++It)
		{
			if (It->Kind == (Op == TEXT("tv") ? 1 : 0)) { It->Interact(Me); }
		}
	}
	else if (Op == TEXT("faucet") && Faucet && Me) { Faucet->Interact(Me); }
	else if (Op == TEXT("human") && Human) { Human->SetActorLocation(FVector(Num(1), Num(2), Human->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics); }
	else if (Op == TEXT("stomp") && Human && Me)
	{
		// The Human stomps right on the player (0) or just beside it (1: the shockwave).
		const FVector P = Human->GetStompPoint(0) + FVector(Num(1) > 0.5f ? AHumanCharacter::StompRadius * 1.5f : 0.f, 0.f, 12.f);
		Me->ApplyTeleport(P);
		Human->TryStomp(0);
	}
	else if (Op == TEXT("drop") && Human)
	{
		// A jar falls on the Human's head.
		const FTransform TM(Human->GetActorLocation() + FVector(Num(1), Num(2), 2600.f));
		if (AKitchenProp* P = W->SpawnActorDeferred<AKitchenProp>(AKitchenProp::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			P->ShapeIndex = (uint8)EAntShape::Cylinder;
			P->Size = FVector(190.f, 190.f, 270.f);
			P->Color = FLinearColor(0.35f, 0.6f, 0.25f);
			P->Mass = 7.f;
			P->PropName = TEXT("Банка огурцов");
			P->bDevPushed = true;
			P->FinishSpawning(TM);
		}
	}
	else if (Op == TEXT("crush"))
	{
		// A heavy piece of food falls on the first fire ant.
		TArray<AAntCharacter*> E = Bots(1);
		if (E.Num() > 0)
		{
			FAntFoodDef D = AntKit::HeavyFood(0);
			E[0]->Webbed(5.f);   // held in place, so the test does not depend on where the bot walks while the thing falls
			const FTransform TM(E[0]->GetActorLocation() + FVector(0.f, 0.f, 2600.f));
			if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				L->InitFood(D);
				L->FinishSpawning(TM);
			}
		}
	}
	// ---- level v2
	else if (Op == TEXT("go") && Me)
	{
		// go x y z [x y z ...]: walk these points in order (logs ANTV2 go: reached / DONE / STUCK)
		DevGoPts.Reset();
		for (int32 i = 1; i + 2 < A.Num(); i += 3) { DevGoPts.Add(Vec(i)); }
		DevGoStart = DevGoBestAt = W->GetTimeSeconds();
		DevGoBest = 1.e9f;
		DevGoLogAt = 0.f;
		DevGoDetours = DevGoDetourPts = 0;
	}
	else if (Op == TEXT("gostop") && Me) { DevGoPts.Reset(); Me->bDevGo = false; }
	else if (Op == TEXT("me") && Me)
	{
		const FVector P = Me->GetActorLocation();
		const FLevelV2Box* Cov = LevelV2::CoverAt(P);
		const TCHAR* Dist = LevelV2::DistrictAt(P);
		UE_LOG(LogTemp, Display, TEXT("ANTV2 me: at %s health %.0f dead %d region %d district «%s» cover %s (%s) in nest %d falling %d carrying %s"), *P.ToCompactString(), Me->Health, Me->bDead ? 1 : 0,
			AKitchenBuilder::Region(P), Dist ? Dist : TEXT("-"), AHumanCharacter::InCover(W, P) ? TEXT("YES") : TEXT("no"), Cov ? Cov->Name : TEXT("-"), Me->IsInNest() ? 1 : 0,
			Me->GetCharacterMovement()->IsFalling() ? 1 : 0, Me->Carrying ? *Me->Carrying->LootName : TEXT("-"));
	}
	else if (Op == TEXT("lootall"))
	{
		// Food on every spot of the table at once (everything else that lay about is removed first).
		for (TActorIterator<AKitchenLoot> It(W); It; ++It) { It->ReleaseAll(); It->Destroy(); }
		DevLootPlaced.Reset();
		DevLootWanted.Reset();
		for (int32 i = 0; i < SpotLocations.Num(); ++i)
		{
			DevLootPlaced.Add(SpawnFood(SpotLocations[i], SpotTiers[i]));
			DevLootWanted.Add(SpotLocations[i]);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 lootall: food on %d spots"), DevLootPlaced.Num());
	}
	else if (Op == TEXT("edges"))
	{
		// Every outer edge of the street and the yards, a step every 100 uu: is there ground right up to the edge, and does
		// something solid stop an ant there (a sphere of its size, at four heights)? The log names every place that fails.
		struct FDevEdge { float X0, Y0, X1, Y1, NX, NY; const TCHAR* Name; };
		static const FDevEdge Edges[] =
		{
			{ -15300.f, -5150.f, -15300.f, 5550.f, -1.f, 0.f, TEXT("street west") },
			{ -15250.f, 5600.f, -9500.f, 5600.f, 0.f, 1.f, TEXT("street north") },
			{ -9450.f, 5670.f, -7250.f, 5670.f, 0.f, 1.f, TEXT("front lawn north") },
			{ -15250.f, -5200.f, -9450.f, -5200.f, 0.f, -1.f, TEXT("street south") },
			{ -9470.f, -8800.f, -9470.f, -5250.f, -1.f, 0.f, TEXT("yard A west") },
			{ -9400.f, -8870.f, 5100.f, -8870.f, 0.f, -1.f, TEXT("yard A south") },
			{ 5130.f, -12200.f, 5130.f, -8950.f, -1.f, 0.f, TEXT("yard B west") },
			{ 5200.f, -12270.f, 20400.f, -12270.f, 0.f, -1.f, TEXT("yard B south") },
			{ 20470.f, -12200.f, 20470.f, -9250.f, 1.f, 0.f, TEXT("yard B east") },
			{ 20210.f, -9200.f, 20460.f, -9200.f, 0.f, 1.f, TEXT("yard B north-east notch") },
		};
		static const float Heights[4] = { 6.f, 80.f, 700.f, 2400.f };
		const FCollisionShape Ball = FCollisionShape::MakeSphere(5.f);
		FCollisionQueryParams QP(SCENE_QUERY_STAT(DevEdges), false);
		int32 Steps = 0, Open = 0, NoGround = 0;
		for (const FDevEdge& E : Edges)
		{
			const FVector A0(E.X0, E.Y0, 0.f), B0(E.X1, E.Y1, 0.f), Out(E.NX, E.NY, 0.f);
			const int32 N = FMath::Max(1, FMath::RoundToInt((float)FVector::Dist(A0, B0) / 100.f));
			int32 EdgeOpen = 0, EdgeNoGround = 0;
			float Far = 0.f;
			for (int32 i = 0; i <= N; ++i)
			{
				const FVector P = FMath::Lerp(A0, B0, (float)i / N) - Out * 200.f;   // 200 inside the edge line
				++Steps;
				float Reach = 900.f;
				for (float H : Heights)
				{
					FHitResult Hit;
					const FVector From = P + FVector(0.f, 0.f, H);
					if (!W->SweepSingleByChannel(Hit, From, From + Out * 900.f, FQuat::Identity, ECC_Pawn, Ball, QP))
					{
						++EdgeOpen;
						if (EdgeOpen <= 4) { UE_LOG(LogTemp, Display, TEXT("ANTV2 edges: OPEN %s at (%.0f, %.0f) height %.0f: nothing within 900"), E.Name, P.X, P.Y, H); }
					}
					else if (H < 10.f) { Reach = Hit.Distance; Far = FMath::Max(Far, Hit.Distance - 200.f); }
				}
				for (float D = 0.f; D <= Reach; D += 40.f)
				{
					FHitResult G;
					const FVector At = P + Out * D;
					if (!W->LineTraceSingleByChannel(G, At + FVector(0.f, 0.f, 40.f), At - FVector(0.f, 0.f, 300.f), ECC_Pawn, QP))
					{
						++EdgeNoGround;
						if (EdgeNoGround <= 4) { UE_LOG(LogTemp, Display, TEXT("ANTV2 edges: NO GROUND %s at (%.0f, %.0f)"), E.Name, At.X, At.Y); }
						break;
					}
				}
			}
			Open += EdgeOpen;
			NoGround += EdgeNoGround;
			UE_LOG(LogTemp, Display, TEXT("ANTV2 edges: %s: %d steps, %d open sweeps, %d places without ground, the stop lies at most %.0f beyond the edge line"), E.Name, N + 1, EdgeOpen, EdgeNoGround, Far);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 edges: %d steps along %d edges: %d open sweeps, %d places without ground"), Steps, (int32)UE_ARRAY_COUNT(Edges), Open, NoGround);
	}
	else if (Op == TEXT("lootclear") && Me)
	{
		// lootclear [r]: the food lying within r (500) of the player is removed (to try a passage without a crumb plugging it)
		int32 Gone = 0;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (!It->IsCarried() && *It != MainObject.Get() && FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) < Num(1, 500.f)) { It->Destroy(); ++Gone; }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 lootclear: %d pieces removed"), Gone);
	}
	else if (Op == TEXT("lootrest"))
	{
		// Where did each piece come to rest? The spot is 35 above its surface: a piece that lies lower than 70 under the spot
		// fell through or off, one more than 150 aside rolled away, one that is gone broke or left the world.
		int32 Ok = 0, Fell = 0, Rolled = 0, Gone = 0;
		for (int32 i = 0; i < DevLootPlaced.Num(); ++i)
		{
			const AKitchenLoot* L = DevLootPlaced[i].Get();
			const FVector Want = DevLootWanted[i];
			// what is straight under the designed point (the first solid thing below it)
			FHitResult Under;
			FCollisionQueryParams UQ(SCENE_QUERY_STAT(DevLootRest), true);
			for (const TWeakObjectPtr<AKitchenLoot>& Other : DevLootPlaced) { if (Other.IsValid()) { UQ.AddIgnoredActor(Other.Get()); } }
			const bool bUnder = W->LineTraceSingleByChannel(Under, Want + FVector(0.f, 0.f, 10.f), Want - FVector(0.f, 0.f, 6000.f), ECC_Pawn, UQ);
			const UStaticMeshComponent* USM = bUnder ? Cast<UStaticMeshComponent>(Under.GetComponent()) : nullptr;
			const FString Below = bUnder ? FString::Printf(TEXT("under the point: %s at z %.0f"), USM && USM->GetStaticMesh() ? *USM->GetStaticMesh()->GetName() : *GetNameSafe(Under.GetActor()), Under.ImpactPoint.Z) : FString(TEXT("under the point: nothing"));
			if (!L)
			{
				++Gone;
				UE_LOG(LogTemp, Display, TEXT("ANTV2 lootrest %d (%.0f, %.0f, %.0f): GONE; %s"), i, Want.X, Want.Y, Want.Z - 35.f, *Below);
				continue;
			}
			const FVector At = L->GetActorLocation();
			const bool bFell = At.Z < Want.Z - 70.f, bRolled = FVector::Dist2D(At, Want) > 150.f;
			if (bFell || bRolled)
			{
				bFell ? ++Fell : ++Rolled;
				UE_LOG(LogTemp, Display, TEXT("ANTV2 lootrest %d (%.0f, %.0f, %.0f): %s, lies at (%.0f, %.0f, %.0f) «%s»"), i, Want.X, Want.Y, Want.Z - 35.f, bFell ? TEXT("FELL") : TEXT("ROLLED"), At.X, At.Y, At.Z, *(L->LootName + TEXT("; ") + Below));
			}
			else
			{
				++Ok;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 lootrest: %d spots: %d lie where designed, %d fell, %d rolled away, %d gone"), DevLootPlaced.Num(), Ok, Fell, Rolled, Gone);
	}
	else if (Op == TEXT("around") && Me)
	{
		// What is round the player's ant: the first solid thing down, up and to the four sides within 400 uu (to see what blocks a way).
		const FVector P = Me->GetActorLocation();
		static const FVector Dirs[6] = { FVector(0, 0, -1), FVector(0, 0, 1), FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0) };
		static const TCHAR* Names[6] = { TEXT("down"), TEXT("up"), TEXT("+x"), TEXT("-x"), TEXT("+y"), TEXT("-y") };
		FString Line;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(DevAround), true, Me);
		for (int32 i = 0; i < 6; ++i)
		{
			FHitResult H;
			if (W->LineTraceSingleByChannel(H, P, P + Dirs[i] * 400.f, ECC_Pawn, QP))
			{
				const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(H.GetComponent());
				Line += FString::Printf(TEXT(" %s: %s %.0f |"), Names[i], SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : *GetNameSafe(H.GetActor()), H.Distance);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 around %s:%s"), *P.ToCompactString(), *Line);
	}
	else if (Op == TEXT("cake")) { SpawnMainObject((int32)Num(1, 0.f) - 1); }   // cake [1|2|3]: a new one, at that place or at random
	else if (Op == TEXT("caketo") && MainObject.IsValid())
	{
		MainObject->ReleaseAll();
		MainObject->SetActorLocation(Vec(1), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (Op == TEXT("cakeinfo"))
	{
		const AKitchenLoot* C = MainObject.Get();
		UE_LOG(LogTemp, Display, TEXT("ANTV2 cake: %s"), C ? *FString::Printf(TEXT("at %s, weight %d, carriers %d, strength %.0f, ratio %.2f, speed %.0f"), *C->GetActorLocation().ToCompactString(), C->Weight,
			C->NumCarriers(), C->NetStrength(), C->CarryRatio(), C->GetVelocity().Size()) : TEXT("none (delivered, broken or not spawned)"));
		for (int32 i = 0; C && i < C->Carriers.Num(); ++i)
		{
			FVector G;
			if (C->Carriers[i] && C->GrabPointOf(C->Carriers[i], G))
			{
				const FVector H = C->Carriers[i]->GetHoldPoint();
				UE_LOG(LogTemp, Display, TEXT("ANTV2 cake:   carrier %d at %s holds %s, bite %s, stretch %.0f (flat %.0f), rot %s"), i, *C->Carriers[i]->GetActorLocation().ToCompactString(), *H.ToCompactString(), *G.ToCompactString(),
					FVector::Dist(G, H), FVector::Dist2D(G, H), *C->GetActorRotation().ToCompactString());
			}
		}
	}
	else if (Op == TEXT("give") && Me)
	{
		// give weight [value]: a piece of food of that weight appears in the player's jaws
		FAntFoodDef D;
		D.Name = TEXT("Пробный груз");
		D.Weight = FMath::Max(1, (int32)Num(1, 10.f));
		D.Value = FMath::Max(1, (int32)Num(2, 10.f));
		D.Color = FLinearColor(0.9f, 0.7f, 0.2f);
		D.Shape = EAntShape::Cube;
		D.Scale = FVector(FMath::Clamp(0.2f + D.Weight * 0.008f, 0.2f, 0.8f));
		const FTransform TM(Me->GetActorLocation() + Me->GetActorForwardVector() * 30.f + FVector(0.f, 0.f, 25.f));
		if (AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			L->InitFood(D);
			L->FinishSpawning(TM);
			Me->GrabLoot(L);
		}
	}
	else if (Op == TEXT("grab") && Me)
	{
		// grab [cake]: the player takes the nearest food (or the cake, wherever it is: the ant is put next to it)
		AKitchenLoot* L = (A.Num() > 1 && MainObject.IsValid()) ? MainObject.Get() : Me->FindNearestLoot(400.f);
		if (L && A.Num() == 2) { Me->ApplyTeleport(L->GetActorLocation() + FVector(60.f, 0.f, 20.f)); }   // "grab cake here": from where the ant stands
		const bool bOk = L && Me->GrabLoot(L);
		UE_LOG(LogTemp, Display, TEXT("ANTV2 grab: %s %s"), L ? *L->LootName : TEXT("nothing near"), bOk ? TEXT("in the jaws") : TEXT("FAILED"));
	}
	else if (Op == TEXT("helpers") && Me && Me->Carrying)
	{
		// helpers N: N ally bots come and take hold of what the player carries
		int32 N = (int32)Num(1, 1.f), Got = 0;
		for (AAntCharacter* B : Bots(0))
		{
			if (N-- <= 0) { break; }
			B->LetGo();
			B->ApplyTeleport(Me->GetActorLocation() + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 8.f));
			Got += B->GrabLoot(Me->Carrying) ? 1 : 0;
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 helpers: %d took hold of «%s», ratio %.2f"), Got, *Me->Carrying->LootName, Me->Carrying->CarryRatio());
	}
	else if (Op == TEXT("roof"))
	{
		// roof 0|1: hide / show the ceilings (for a look from above)
		const bool bShow = Num(1, 0.f) > 0.5f;
		for (TActorIterator<AKitchenBuilder> It(W); It; ++It)
		{
			TInlineComponentArray<UStaticMeshComponent*> Parts(*It);
			for (UStaticMeshComponent* C : Parts)
			{
				if (C && C->Bounds.GetBox().Min.Z > 5300.f) { C->SetVisibility(bShow); }
			}
		}
	}
	else if (Op == TEXT("mover") && A.Num() >= 3)
	{
		// mover <part of the mesh name> 1|0 [part]: open / shut every generic moving part whose mesh name contains that (mover Drawer3 1),
		// or only the one with that number in the movers table (mover TVStand_Door 1 25)
		const TConstArrayView<FKitchenArtMover> All = KitchenArt::Movers();
		for (TActorIterator<AKitchenMover> It(W); It; ++It)
		{
			if (All.IsValidIndex(It->PartIndex) && FString(KitchenArt::Instances()[All[It->PartIndex].Inst].Name).Contains(A[1]) && (A.Num() < 4 || It->PartIndex == (int32)Num(3)))
			{
				It->SetOpen(Num(2) > 0.5f);
				UE_LOG(LogTemp, Display, TEXT("ANTV2 mover %s: %s"), KitchenArt::Instances()[All[It->PartIndex].Inst].Name, It->IsOpen() ? TEXT("open") : TEXT("shut"));
			}
		}
	}
	else if (Op == TEXT("flap"))
	{
		for (TActorIterator<AKitchenMover> It(W); It; ++It)
		{
			if (It->IsCatFlap())
			{
				if (A.Num() > 1) { It->SetDoorOpen(Num(1) > 0.5f); }
				UE_LOG(LogTemp, Display, TEXT("ANTV2 catflap: open %d, need %.0f, missing %d, me on the %s side"), It->IsOpen() ? 1 : 0, It->PullNeed(), (int32)It->PullMissing,
					Me && LevelV2::OnLatchSideOfCatFlap(Me->GetActorLocation()) ? TEXT("living-room") : TEXT("yard"));
			}
		}
	}
	else if ((Op == TEXT("hstompme") || Op == TEXT("hswatme")) && Human && Me)
	{
		// The Human is put so that his stomp lands on the player's ant / he swats it (low: the palm, high: the newspaper).
		const FVector At = Me->GetActorLocation();
		if (Op == TEXT("hstompme"))
		{
			const FVector Off = Human->GetActorLocation() - Human->GetStompPoint(0);
			Human->SetActorLocation(FVector(At.X + Off.X, At.Y + Off.Y, Human->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			Human->TryStomp(0);
		}
		else
		{
			const FVector Side = FVector(Num(1, 900.f), Num(2, 0.f), 0.f);
			Human->SetActorLocation(FVector(At.X + Side.X, At.Y + Side.Y, Human->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			Human->SetActorRotation(FRotator(0.f, (float)(-Side).Rotation().Yaw, 0.f));
			Human->SwatReadyTime = 0.f;
			Human->TrySwat(At);
		}
		UE_LOG(LogTemp, Display, TEXT("ANTV2 %s: at the ant %s (health %.0f, in cover %d), the Human at %s"), *Op, *At.ToCompactString(), Me->Health, AHumanCharacter::InCover(W, At) ? 1 : 0, *Human->GetActorLocation().ToCompactString());
	}
	else if (Op == TEXT("die") && Me) { Me->HurtBy(1000.f, TEXT("погиб на испытаниях")); }
	else if (Op == TEXT("body") && Me)
	{
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (It->bBody)
			{
				Me->ApplyTeleport(It->GetActorLocation() + FVector(10.f, 0.f, 8.f));
				const bool bOk = Me->GrabLoot(*It);
				UE_LOG(LogTemp, Display, TEXT("ANTDEV grabbed the body: %d (%s)"), bOk ? 1 : 0, *It->LootName);
				break;
			}
		}
	}
	else if (Op == TEXT("grab") && Me)
	{
		if (AKitchenLoot* L = Me->FindNearestLoot(6000.f))
		{
			Me->ApplyTeleport(L->GetActorLocation() + FVector(10.f, 0.f, 8.f));
			Me->GrabLoot(L);
		}
	}
	else if (Op == TEXT("haul") && Me && Me->Carrying)
	{
		// Carries the load (and the ant) to the doorstep of the nest.
		const FVector Home = AKitchenBuilder::NestLocation(0) + FVector(300.f, 0.f, 60.f);
		Me->Carrying->SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
		Me->ApplyTeleport(Home + FVector(12.f, 0.f, 4.f));
	}
	else if (Op == TEXT("legs") && Me) { Me->TearLegOff(Me->GetActorLocation() + FVector(10.f, 10.f, 0.f)); }
	// ---- the Human's jointed body: a puppet for looking at one pose at a time
	else if (Op == TEXT("hpuppet") && Human)
	{
		// hpuppet 1|0: the AI lets go of him (he stands where he is put and does what hact says).
		Human->bDevPuppet = Num(1, 1.f) > 0.5f;
		Human->DevWalk = FVector::ZeroVector;
		Human->SetSpraying(false);
	}
	else if (Op == TEXT("hto") && Human)
	{
		// hto x y yaw
		const FRotator Rot(0.f, Num(3), 0.f);
		Human->SetActorLocationAndRotation(FVector(Num(1), Num(2), Human->GetActorLocation().Z), Rot, false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* HC = Human->GetController()) { HC->SetControlRotation(Rot); }
		Human->DevWalk = FVector::ZeroVector;
	}
	else if (Op == TEXT("hwalk") && Human) { Human->DevWalk = FVector(Num(1), Num(2), 0.f); Human->DevWalkScale = Num(3, 1.f); }   // hwalk x y [input 0..1]
	else if (Op == TEXT("hact") && Human && A.Num() >= 2)
	{
		// hact stomp0|stomp1|swat|grab|throw|reach|peer|shake|spray1|spray0|stun0|stun1|ko|rageN [x y z]; without a point: at the player's ant.
		Human->DevAct(A[1].ToLower(), A.Num() >= 5 ? Vec(2) : (Me ? Me->GetActorLocation() : Human->GetStompPoint(0)));
	}
	else if (Op == TEXT("hfreeze") && Human) { Human->DevFreezeAt = Num(1, -1.f); }   // hold the action at this second from its start; -1 lets go
	else if (Op == TEXT("hinfo") && Human) { UE_LOG(LogTemp, Display, TEXT("ANTHUMAN %s"), *Human->DevDescribe()); }
	else if (Op == TEXT("hfood") && Human)
	{
		// hfood [dx dy]: puts the nearest loose food on the floor in front of the Human (default 850 ahead, 250 to his left), for hact grab.
		AKitchenLoot* Best = nullptr;
		float BestD = 1.e9f;
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			const float D = (float)FVector::Dist(It->GetActorLocation(), Human->GetActorLocation());
			if (!It->bIsGear && !It->bHeld && !It->IsCarried() && !It->bTrophy && It->Weight <= 6 && D < BestD) { BestD = D; Best = *It; }
		}
		if (Best)
		{
			const FVector At = Human->GetActorLocation() + Human->GetActorForwardVector() * Num(1, 850.f) + Human->GetActorRightVector() * Num(2, -250.f)
				- FVector(0.f, 0.f, Human->GetSimpleCollisionHalfHeight() - 40.f);
			Best->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogTemp, Display, TEXT("ANTHUMAN dev food «%s» put at %s"), *Best->LootName, *At.ToCompactString());
		}
	}
	else if (Op == TEXT("hplay") && PC) { ToggleHuman(Cast<AAntPlayerController>(PC)); }   // the player takes the Human (again: gives him back)
	else if (Op == TEXT("hud") && PC && PC->GetHUD()) { PC->GetHUD()->bShowHUD = Num(1, 0.f) > 0.5f; }
	else if (Op == TEXT("shotc") && PC) { PC->ConsoleCommand(TEXT("Shot"), false); }   // a screenshot without the Slate UI
	else if ((Op == TEXT("cam") || Op == TEXT("camw")) && PC)
	{
		// cam off | cam dx dy dz lookZ [fov]: from a point of the Human's frame (dx ahead of him, dy to his right, dz above the floor) at him, lookZ above the floor.
		// camw x y z tx ty tz [fov]: from a world point at a world point.
		static TWeakObjectPtr<ACameraActor> DevCam;
		if (A.Num() >= 2 && A[1] == TEXT("off"))
		{
			PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f);
		}
		else
		{
			FVector From = Vec(1), To = Vec(4);
			float Fov = Num(7, 70.f);
			if (Op == TEXT("cam") && Human)
			{
				const FTransform B(FRotator(0.f, Human->GetActorRotation().Yaw, 0.f), Human->GetActorLocation() - FVector(0.f, 0.f, Human->GetSimpleCollisionHalfHeight()));
				From = B.TransformPosition(Vec(1));
				To = B.TransformPosition(FVector(0.f, 0.f, Num(4, 1700.f)));
				Fov = Num(5, 70.f);
			}
			if (!DevCam.IsValid())
			{
				DevCam = W->SpawnActor<ACameraActor>();
			}
			if (ACameraActor* Cam = DevCam.Get())
			{
				Cam->SetActorLocationAndRotation(From, (To - From).Rotation());
				Cam->GetCameraComponent()->SetFieldOfView(Fov);
				Cam->GetCameraComponent()->bConstrainAspectRatio = false;
				PC->SetViewTargetWithBlend(Cam, 0.f);
			}
		}
	}
	else if (Op == TEXT("ride") && Human && Me)
	{
		// ride seg [height]: glues the player's ant to the front of a segment of the Human (HumanSeg index; height along the limb, uu below its joint).
		const int32 Seg = (int32)Num(1, (float)HumanSeg::ShinL);
		if (UStaticMeshComponent* C = Human->GetSegment(Seg))
		{
			// ride seg down 1: dropped onto the segment from above instead (at `down` uu ahead of its joint): lands on it like on any floor.
			const FTransform TM = C->GetComponentTransform();
			const bool bDrop = Num(3) > 0.5f;
			const float Down = Num(2, 400.f) / HumanBody::Scale;
			FHitResult Hit;
			const FVector Out = bDrop ? TM.TransformPosition(FVector(Down, 0.f, 60.f)) : TM.TransformPosition(FVector(60.f, 0.f, -Down));
			const FVector In = bDrop ? TM.TransformPosition(FVector(Down, 0.f, -30.f)) : TM.TransformPosition(FVector(0.f, 0.f, -Down));
			if (C->LineTraceComponent(Hit, Out, In, FCollisionQueryParams(SCENE_QUERY_STAT(DevRide), true)))
			{
				const float Rad = Me->GetCapsuleComponent()->GetScaledCapsuleRadius();
				Me->ApplyTeleport(Hit.ImpactPoint + Hit.ImpactNormal * (Rad + (bDrop ? 12.f : 0.6f)));
				UAntMovementComponent* Move = Me->GetAntMovement();
				if (Move && !bDrop) { Move->StartCrawl(Hit.ImpactNormal); }
				// What the ant's own "is there a surface under me" trace sees from here.
				FHitResult G;
				const FVector L = Me->GetActorLocation(), N = Hit.ImpactNormal;
				const bool bSees = W->LineTraceSingleByObjectType(G, L + N * 2.f, L - N * (Rad + 28.f * AntKit::AntShrink), UAntMovementComponent::SurfaceObjects(), FCollisionQueryParams(SCENE_QUERY_STAT(DevRide2), false, Me));
				UE_LOG(LogTemp, Display, TEXT("ANTRIDE put on %s at %s, normal %s, radius %.1f, noclimb=%d, own trace sees %s at %.1f"), *C->GetName(), *Hit.ImpactPoint.ToCompactString(), *Hit.ImpactNormal.ToCompactString(),
					Rad, Move && Move->bNoClimb ? 1 : 0, bSees && G.GetComponent() ? *G.GetComponent()->GetName() : TEXT("NOTHING"), bSees ? G.Distance : -1.f);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ANTRIDE no surface found on %s"), *C->GetName());
			}
		}
	}
	else if (Op == TEXT("rideinfo") && Me)
	{
		const UPrimitiveComponent* Base = Me->GetAntMovement() ? Me->GetAntMovement()->GetRiddenComponent() : nullptr;
		const FVector Rel = Base ? Base->GetComponentTransform().InverseTransformPosition(Me->GetActorLocation()) * HumanBody::Scale : FVector::ZeroVector;
		const float Sole = Human ? (float)FMath::Min(FVector::Dist(Me->GetActorLocation(), Human->GetSoleLocation(0)), FVector::Dist(Me->GetActorLocation(), Human->GetSoleLocation(1))) : -1.f;
		UE_LOG(LogTemp, Display, TEXT("ANTRIDE base=%s onHuman=%d rel=(%.0f,%.0f,%.0f) ant=%s crawling=%d dead=%d stuck=%d toSole=%.0f humanSpeed=%.0f human=%s"),
			Base ? *Base->GetName() : TEXT("-"), Me->GetRiddenHuman() ? 1 : 0, Rel.X, Rel.Y, Rel.Z, *Me->GetActorLocation().ToCompactString(),
			Me->GetAntMovement() && Me->GetAntMovement()->IsCrawling() ? 1 : 0, Me->bDead ? 1 : 0, Me->StuckTo ? 1 : 0, Sole,
			Human ? Human->GetVelocity().Size2D() : 0.f, Human ? *Human->GetActorLocation().ToCompactString() : TEXT("-"));
	}
	// ---- puddles, bottles, the fall (KitchenSlick.h, HumanCharacter.cpp TrySlip)
	else if (Op == TEXT("spill"))
	{
		// spill x y z [kind 0 water / 1 oil] [radius] [life]: a puddle on the floor under the point
		AKitchenSlick::Spill(W, Vec(1), (uint8)FMath::Clamp((int32)Num(4, 1.f), 0, 1), Me, Num(5, -1.f), Num(6, -1.f));
	}
	else if (Op == TEXT("slicks"))
	{
		for (TActorIterator<AKitchenSlick> It(W); It; ++It)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTSLICK at %s kind %d radius %.0f/%.0f age %.1f of %.0f wiping %d known %d"), *It->GetActorLocation().ToCompactString(), (int32)It->Kind, It->Radius(), It->MaxRadius,
				W->GetTimeSeconds() - It->Born, It->Life, It->IsBeingWiped() ? 1 : 0, It->bKnown ? 1 : 0);
		}
	}
	else if ((Op == TEXT("bottle") || Op == TEXT("bottlepush")) && A.Num() >= 2)
	{
		// bottle oil|water tip|stand|info   ·   bottlepush oil|water [bots]: the player and N ally bots at its back (the cabinet side), the player holds [E]
		AKitchenSpillBottle* B = AKitchenSpillBottle::Nearest(W, Me ? Me->GetActorLocation() : FVector::ZeroVector, A[1].ToLower() == TEXT("water") ? SlickKind::Water : SlickKind::Oil);
		if (!B)
		{
			UE_LOG(LogTemp, Warning, TEXT("ANTSPILL no bottle"));
		}
		else if (Op == TEXT("bottlepush") && Me)
		{
			const FVector Back = B->GetActorLocation() + FVector(0.f, AKitchenSpillBottle::Radius + 18.f, 8.f);
			Me->ApplyTeleport(Back);
			int32 N = (int32)Num(2, 2.f), Got = 0;
			if (Bots(0).Num() < N) { AddBots(0, N - Bots(0).Num()); }
			for (AAntCharacter* Bot : Bots(0))
			{
				if (N-- <= 0) { break; }
				Bot->LetGo();
				Bot->ApplyTeleport(Back + FVector(FMath::FRandRange(-55.f, 55.f), FMath::FRandRange(-4.f, 4.f), 2.f));
				++Got;
			}
			UE_LOG(LogTemp, Display, TEXT("ANTSPILL push: the player and %d bots at %s, need %.0f"), Got, *Back.ToCompactString(), B->PullNeed());
			Me->DevInteract();
			UE_LOG(LogTemp, Display, TEXT("ANTSPILL push: pulling %s, missing %d, tipped %d"), Me->Pulling ? *Me->Pulling->GetName() : TEXT("-"), (int32)B->PullMissing, B->IsDoorOpen() ? 1 : 0);
		}
		else if (A.Num() >= 3 && A[2].ToLower() == TEXT("tip")) { B->SetDoorOpen(true); }
		else if (A.Num() >= 3 && A[2].ToLower() == TEXT("stand")) { B->SetDoorOpen(false); }
		else
		{
			int32 Hands = 0;
			UE_LOG(LogTemp, Display, TEXT("ANTSPILL %s at %s tipped %d yaw %.0f missing %d strength %.0f of %.0f (hands %d)"), *B->BottleName(), *B->GetActorLocation().ToCompactString(), B->IsDoorOpen() ? 1 : 0,
				B->TipYaw, (int32)B->PullMissing, B->PullStrength(&Hands), B->PullNeed(), Hands);
		}
	}
	else if (Op == TEXT("noharm")) { AntFilm::SetNoHarm(Num(1, 1.f) > 0.5f); UE_LOG(LogTemp, Display, TEXT("ANTFILM noharm %d"), AntFilm::NoHarm() ? 1 : 0); }
	else if (Op == TEXT("pos"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTDEV pos me=%s human=%s goal=%d «%s» stage=%d score=%d/%d larvae=%d greed=%.1f"), Me ? *Me->GetActorLocation().ToCompactString() : TEXT("-"),
			Human ? *Human->GetActorLocation().ToCompactString() : TEXT("-"), GS ? (int32)GS->HumanGoal : -1, GS ? *GS->HumanGoalText : TEXT(""),
			GS ? (int32)GS->ShiftStage : -1, GS ? GS->TeamScore : 0, GS ? GS->TargetScore : 0, GS ? GS->Larvae : 0, GS ? GS->Greed : 0.f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ANTDEV unknown or impossible right now: %s"), *Cmd);
	}
}

// ---------------------------------------------------------------------------------------------
// Level v3: what the raid seed decides that lives on the server (doors, crumbs); the meshes are KitchenBuilder::ApplyV3Layout.
// ---------------------------------------------------------------------------------------------

void AAntKitchenGameMode::ApplyV3Doors()
{
	if (!LevelV3::Enabled())
	{
		return;
	}
	AKitchenFixture::ResetAll(GetWorld());   // everything whole and fastened, the variant's pieces (tablecloth, teapot) in place
	const TConstArrayView<FKitchenArtMover> Parts = KitchenArt::Movers();
	TMap<int32, float> Want;   // mover -> -1 shut, 1 open, 0..1 ajar
	if (const FLevelV3Seed* S = LevelV3::Current())
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (Parts.IsValidIndex(S->Doors[i])) { Want.Add(S->Doors[i], 25.f / FMath::Max(25.f, FMath::Abs(Parts[S->Doors[i]].Angle))); }   // "ajar:25"
		}
	}
	TMap<FString, bool> Gadgets;
	for (int32 D = 0; D < LevelV3::NumDistricts(); ++D)
	{
		const int32 V = LevelV3::CurrentVariant(D);
		if (V == INDEX_NONE) { continue; }
		const FLevelV3Variant& Vr = LevelV3::Variants()[V];
		for (int32 k = 0; k < Vr.NumOps; ++k)
		{
			const FLevelV3Op& O = LevelV3::Ops()[Vr.FirstOp + k];
			if (O.Kind == LevelV3::Mover && Parts.IsValidIndex(O.A))
			{
				Want.Add(O.A, O.B == 0 ? -1.f : O.B == 1 ? 1.f : O.F[0] / FMath::Max(1.f, FMath::Abs(Parts[O.A].Angle)));
			}
			else if (O.Kind == LevelV3::Gadget)
			{
				Gadgets.Add(O.Text, O.B != 0);
			}
		}
	}
	int32 Set = 0;
	for (TActorIterator<AKitchenMover> It(GetWorld()); It; ++It)
	{
		if (!Parts.IsValidIndex(It->PartIndex)) { continue; }
		const bool bStartsOpen = (Parts[It->PartIndex].Flags & KitchenArt::StartsOpen) != 0;
		const float* W = Want.Find(It->PartIndex);
		const bool bOpen = W ? *W >= 1.f : bStartsOpen;
		It->SetOpen(bOpen);
		It->Ajar = (W && *W > 0.f && *W < 1.f) ? FMath::Clamp(*W, 0.f, 1.f) : 0.f;
		Set += W ? 1 : 0;
	}
	static const TCHAR* const Names[] = { TEXT("FRIDGE"), TEXT("OVEN"), TEXT("DRAWER"), TEXT("DISHWASHER") };
	static const EKitchenDoorKind Kinds[] = { EKitchenDoorKind::Fridge, EKitchenDoorKind::Oven, EKitchenDoorKind::Drawer, EKitchenDoorKind::Dishwasher };
	for (int32 g = 0; g < 4; ++g)
	{
		const bool* Open = Gadgets.Find(Names[g]);
		if (!Open && !V3GadgetsOpened.Contains((uint8)g)) { continue; }   // (not ours to touch)
		for (TActorIterator<AKitchenDoor> It(GetWorld()); It; ++It)
		{
			if (It->Kind == Kinds[g]) { It->SetDoorOpen(Open && *Open); }
		}
		if (Open && *Open) { V3GadgetsOpened.Add((uint8)g); } else { V3GadgetsOpened.Remove((uint8)g); }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSEED doors: %d movers set by the seed, gadgets %d"), Set, Gadgets.Num());
}

void AAntKitchenGameMode::SpawnV3Scatter()
{
	const FLevelV3Seed* S = LevelV3::Enabled() ? LevelV3::Current() : nullptr;
	if (!S)
	{
		return;
	}
	const TConstArrayView<FLevelV3Scatter> Sc = LevelV3::Scatter();
	int32 N = 0;
	for (int32 i = 0; i < S->ScatterNum; ++i)
	{
		if (!Sc.IsValidIndex(S->ScatterFirst + i)) { break; }
		const FLevelV3Scatter& R = Sc[S->ScatterFirst + i];
		const FString Item = LevelV3::ScatterName(R.Item);
		if (!R.HasPos || Item.StartsWith(TEXT("SM_")) || Item == TEXT("клубочек"))
		{
			continue;   // things, not food (KitchenBuilder::ApplyV3Layout)
		}
		// Crumbs: the smallest food there is, worth a little, carried by one ant.
		FAntFoodDef D;
		D.Name = Item;
		D.Value = Item.Contains(TEXT("печень")) || Item.Contains(TEXT("попкорн")) ? 6 : 4;
		D.Weight = 1;
		D.Shape = Item.Contains(TEXT("хлопья")) || Item.Contains(TEXT("чипс")) ? EAntShape::Cube : EAntShape::Sphere;
		D.Color = Item.Contains(TEXT("корм")) ? FLinearColor(0.42f, 0.24f, 0.1f) : Item.Contains(TEXT("горош")) ? FLinearColor(0.3f, 0.65f, 0.2f)
			: Item.Contains(TEXT("изюм")) ? FLinearColor(0.25f, 0.1f, 0.12f) : FLinearColor(0.85f, 0.65f, 0.35f);
		D.Scale = FVector(D.Shape == EAntShape::Cube ? 0.16f : 0.13f, 0.13f, D.Shape == EAntShape::Cube ? 0.05f : 0.11f);
		const FVector Where(R.X, R.Y, R.Z + 25.f);
		if (AKitchenLoot* L = GetWorld()->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), FTransform(FRotator(0.f, R.Yaw, 0.f), Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			L->InitFood(D);
			L->FinishSpawning(FTransform(FRotator(0.f, R.Yaw, 0.f), Where));
			++N;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTSEED scatter: %d crumbs of seed %d"), N, S->Seed);
}

void AAntKitchenGameMode::OnV3Break(AKitchenFixture* What, AAntCharacter* By)
{
	AAntGameState* GS = GetGameState<AAntGameState>();
	++V3Breaks;
	V3BreakRage += AKitchenFixture::RagePerBreak;
	const FString Name = What ? What->DoorNoun() : FString(TEXT("что-то"));
	UE_LOG(LogTemp, Display, TEXT("ANTFIX break %d: %s by %s, rage from breaks %.2f, Human rage %d"), V3Breaks, *Name, By ? *By->GetDisplayName() : TEXT("-"), V3BreakRage, Human ? (int32)Human->Rage : -1);
	if (V3BreakRage >= 1.f && Human && Human->Rage < 3)
	{
		V3BreakRage -= 1.f;
		Human->SetRage(Human->Rage + 1);
		if (GS) { GS->HumanRage = Human->Rage; }
		Broadcast(FString::Printf(TEXT("Человек слышит, как бьётся посуда: ЯРОСТЬ %d"), (int32)Human->Rage), RunBad);
		AntSfx::Everyone(this, AntSfx::Rage, Human->GetActorLocation(), 1.f - 0.1f * Human->Rage);
	}
	else
	{
		Broadcast(FString::Printf(TEXT("Разбито: %s. Человек злится (ещё %d — и ярость растёт)"), *Name, FMath::Max(1, FMath::CeilToInt((1.f - V3BreakRage) / AKitchenFixture::RagePerBreak))), RunGold);
	}
}

void AAntKitchenGameMode::OnV3Fixture(AKitchenFixture* What, const TCHAR* Event, AAntCharacter* By)
{
	if (!What)
	{
		return;
	}
	const FString E(Event), Id = What->FixId();
	if (E == TEXT("hang")) { Broadcast(FString::Printf(TEXT("%s повисает на одной стороне!"), *What->DoorNoun()), RunGold); }
	else if (E == TEXT("fall") && (Id == TEXT("shelf") || Id == TEXT("curtain_rod") || Id == TEXT("tablecloth") || Id == TEXT("curtain_k"))) { Broadcast(FString::Printf(TEXT("%s падает!"), *What->DoorNoun()), RunGold); }
	else if (E == TEXT("knitting")) { Broadcast(TEXT("Спица вытащена: бабушкин шарф распускается"), RunGold); }
}

