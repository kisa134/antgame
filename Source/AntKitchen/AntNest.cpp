#include "AntNest.h"
#include "AntRun.h"
#include "AntColony.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"

namespace AntUp
{
	static const TCHAR* Branches[Count] = { TEXT("Выводок"), TEXT("Солдаты"), TEXT("Носильщики"), TEXT("Инженеры"), TEXT("Разведчики"), TEXT("Арсенал") };
	// Costs: tier I 120 food, II 300 food + 2 jelly, III 650 food + 5 jelly and a pleased Queen (mood 60).
	static const FDef Defs[Count][MaxTier] =
	{
		{
			{ TEXT("Тёплая камера"),       TEXT("Отряд до 14 рабочих (было 10)"), 120, 0, 0 },
			{ TEXT("Заботливые няньки"),   TEXT("Отряд до 18. Бочка лечит дальше и сильнее"), 300, 2, 0 },
			{ TEXT("Королевский выводок"), TEXT("Отряд до 24. Королева кладёт +1 личинку за рейд"), 650, 5, 60 },
		},
		{
			{ TEXT("Острые жвала"),        TEXT("Отряд кусает вдвое сильнее. Плевок тратит на треть меньше кислоты"), 120, 0, 0 },
			{ TEXT("Стража"),              TEXT("Стража у обоих входов грызёт красных. Первый укус не отрывает лапу"), 300, 2, 0 },
			{ TEXT("Панцири"),             TEXT("Топот и дихлофос убивают вдвое меньше рабочих отряда"), 650, 5, 60 },
		},
		{
			{ TEXT("Сами донесут"),        TEXT("[Q] дважды: отряд сам несёт груз домой безопасной дорогой"), 120, 0, 0 },
			{ TEXT("Крепкие жвала"),       TEXT("Все несут на четверть сильнее, рабочий отряда тянет 5 вместо 4"), 300, 2, 0 },
			{ TEXT("Натоптанная тропа"),   TEXT("Тропа [T] для роя работает в рейде. Отряд с грузом идёт на треть быстрее"), 650, 5, 60 },
		},
		{
			{ TEXT("Рычаг"),               TEXT("Двери на класс легче. Отряд на [B] у двери держит её открытой"), 120, 0, 0 },
			{ TEXT("Длинные мосты"),       TEXT("Мост на [G] втрое длиннее на тех же рабочих"), 300, 2, 0 },
			{ TEXT("Живой пандус"),        TEXT("Мост на [G] можно вести вверх и вниз: по нему несут груз"), 650, 5, 60 },
		},
		{
			{ TEXT("Нюх на сладкое"),      TEXT("Место торта известно на любой угрозе (без этого — только на первой). Рывок перезаряжается быстрее"), 120, 0, 0 },
			{ TEXT("Дозорные"),            TEXT("Засады красных видны заранее. Без груза бегаете на 15% быстрее"), 300, 2, 0 },
			{ TEXT("Тихий шаг"),           TEXT("Человек замечает вас позже. Красные с тортом видны всегда"), 650, 5, 60 },
		},
		{
			{ TEXT("Приманка и тревога"),  TEXT("Инструменты: крошка-приманка для Бадрика, тревожный запах зовёт отряд"), 120, 0, 0 },
			{ TEXT("Обманка"),             TEXT("Инструмент: феромон-обманка уводит красных"), 300, 2, 0 },
			{ TEXT("Шумелка"),             TEXT("Инструмент: шумелка уводит Человека"), 650, 5, 60 },
		},
	};
	const TCHAR* BranchName(uint8 Branch) { return Branch < Count ? Branches[Branch] : TEXT("?"); }
	const FDef& Def(uint8 Branch, int32 Tier) { return Defs[FMath::Min<int32>(Branch, Count - 1)][FMath::Clamp(Tier, 1, MaxTier) - 1]; }
}

namespace AntTool
{
	struct FDef { const TCHAR* Name; const TCHAR* Text; int32 Cost; int32 Tier; };
	static const FDef Defs[Count] =
	{
		{ TEXT("пусто"),            TEXT(""), 0, 0 },
		{ TEXT("Крошка-приманка"),  TEXT("Бросить в прицел: Бадрик идёт её жрать"), 10, 1 },
		{ TEXT("Тревожный запах"),  TEXT("Отряд бросает всё и бежит к тебе"), 10, 1 },
		{ TEXT("Феромон-обманка"),  TEXT("В прицел: красные 20 с сторожат это место"), 20, 2 },
		{ TEXT("Шумелка"),          TEXT("В прицел: Человек идёт на треск"), 25, 3 },
	};
	const TCHAR* Name(uint8 Id) { return Defs[Id < Count ? Id : 0].Name; }
	const TCHAR* Text(uint8 Id) { return Defs[Id < Count ? Id : 0].Text; }
	int32 Cost(uint8 Id) { return Defs[Id < Count ? Id : 0].Cost; }
	int32 NeedTier(uint8 Id) { return Defs[Id < Count ? Id : 0].Tier; }
}

namespace AntMod
{
	struct FDef { const TCHAR* Name; const TCHAR* Text; int32 Cost; uint8 Card; };
	static const FDef Defs[Count] =
	{
		{ TEXT("Долгий сериал"),   TEXT("Человек выходит на 30 с позже"), 30, AntCard::Phone },
		{ TEXT("Мягкая упаковка"), TEXT("Торт и трофеи не бьются при падении"), 40, AntCard::Padding },
		{ TEXT("Азарт"),           TEXT("Жадность растёт быстрее"), 25, AntCard::Greedy },
		{ TEXT("Сверхурочные"),    TEXT("Рейд на 45 с дольше"), 35, AntCard::Overtime },
	};
	const TCHAR* Name(uint8 Id) { return Defs[Id < Count ? Id : 0].Name; }
	const TCHAR* Text(uint8 Id) { return Defs[Id < Count ? Id : 0].Text; }
	int32 Cost(uint8 Id) { return Defs[Id < Count ? Id : 0].Cost; }
	uint8 Card(uint8 Id) { return Defs[Id < Count ? Id : 0].Card; }
}

namespace AntTier
{
	static const FDef Defs[Max] =
	{
		//  name                 what changes                                                       len   quiet final loot  cakeV cakeW jelly ask   dog cat kid gran  rage door spec ev   S  H  E  A  first resp  hp    window
		{ TEXT("Будний день"),   TEXT("Дом сонный, красных мало, место торта известно. Для первых рейдов"),               660.f, 150.f, 120.f, 1.0f,  600, 48, 2, 200, { 1, 0, 0, 0 }, 0, 0, 1, 1,   2, 4, 1, 2, 210.f, 30.f, 1.0f,  120.f },
		{ TEXT("Выходной"),      TEXT("Все дома и раньше просыпаются. Еда в 1,3 раза дороже"),      660.f, 55.f, 150.f, 1.3f,  800, 56, 3, 280, { 2, 1, 1, 0 }, 0, 0, 2, 1,   2, 4, 2, 3, 110.f, 24.f, 1.0f,  105.f },
		{ TEXT("Большая уборка"), TEXT("Двери тяжелее, Человек зол с порога, красных отряд. Еда ×1,6"), 690.f, 40.f, 180.f, 1.6f, 1000, 64, 4, 360, { 2, 1, 1, 1 }, 1, 1, 3, 2,   3, 5, 3, 4,  60.f, 18.f, 1.15f, 90.f },
		{ TEXT("День рождения"), TEXT("Полный дом, все на ногах, красные идут войной. Еда вдвое дороже"), 720.f, 25.f, 210.f, 2.0f, 1300, 72, 6, 480, { 2, 2, 2, 1 }, 2, 1, 4, 2,   3, 5, 4, 5,  30.f, 14.f, 1.3f,  75.f },
	};
	const FDef& Def(int32 Tier) { return Defs[FMath::Clamp(Tier, 1, Max) - 1]; }
	int32 CakesToUnlock(int32 Tier) { return Tier <= 1 ? 1 : 2; }
}

namespace AntNest
{

void Fresh(FAntNestState& S)
{
	S = FAntNestState();
	S.Up.Init(0, AntUp::Count);
	S.Stock.Init(0, AntFoodKind::Count);
	S.Own.Init(0, AntColony::OwnCount);
	S.Version = AntColony::SaveVersion;
}

int32 SquadCap(const FAntNestState& S)
{
	static const int32 Cap[AntUp::MaxTier + 1] = { 10, 14, 18, 24 };
	return Cap[FMath::Clamp(S.Tier_(AntUp::Brood), 0, AntUp::MaxTier)];
}

float WorkerStrength(const FAntNestState& S)
{
	return S.Tier_(AntUp::Carriers) >= 2 ? 5.f : 4.f;
}

int32 MoodBand(int32 Mood)
{
	return Mood >= 85 ? 3 : Mood >= 60 ? 2 : Mood > 25 ? 1 : 0;
}

const TCHAR* MoodName(int32 Mood)
{
	static const TCHAR* Names[] = { TEXT("в ярости"), TEXT("спокойна"), TEXT("довольна"), TEXT("в восторге") };
	return Names[MoodBand(Mood)];
}

bool CanBuy(const FAntNestState& S, uint8 Branch, FString* Why)
{
	if (Branch >= AntUp::Count)
	{
		return false;
	}
	if (!AntColony::BranchOpen(S, Branch))
	{
		const AntColony::FChamberDef& C = AntColony::Def(AntColony::ChamberOfBranch(Branch));
		if (Why) { *Why = FString::Printf(TEXT("сначала прорыть %s (%s)"), AntColony::Dig((uint8)C.Dig).Name, AntColony::Dig((uint8)C.Dig).Toward); }
		return false;
	}
	const int32 Have = S.Tier_(Branch);
	if (Have >= AntUp::MaxTier)
	{
		if (Why) { *Why = TEXT("построено всё"); }
		return false;
	}
	const AntUp::FDef& D = AntUp::Def(Branch, Have + 1);
	if (S.Mood < D.Mood)
	{
		if (Why) { *Why = TEXT("нужна довольная Королева"); }
		return false;
	}
	if (S.Food < D.Food)
	{
		if (Why) { *Why = FString::Printf(TEXT("не хватает еды: %d"), D.Food - S.Food); }
		return false;
	}
	if (S.Jelly < D.Jelly)
	{
		if (Why) { *Why = FString::Printf(TEXT("не хватает молочка: %d"), D.Jelly - S.Jelly); }
		return false;
	}
	return true;
}

bool Buy(FAntNestState& S, uint8 Branch)
{
	if (!CanBuy(S, Branch))
	{
		return false;
	}
	if (S.Up.Num() < AntUp::Count) { S.Up.SetNumZeroed(AntUp::Count); }
	const AntUp::FDef& D = AntUp::Def(Branch, S.Up[Branch] + 1);
	S.Food -= D.Food;
	S.Jelly -= D.Jelly;
	S.Up[Branch]++;
	AntColony::Sync(S);
	return true;
}

void Cards(const FAntNestState& S, TArray<uint8>& Out)
{
	if (S.Tier_(AntUp::Brood) >= 2) { Out.AddUnique(AntCard::Nurse); }
	if (S.Tier_(AntUp::Soldiers) >= 1) { Out.AddUnique(AntCard::Acid); }
	if (S.Tier_(AntUp::Soldiers) >= 2) { Out.AddUnique(AntCard::Chitin); }
	if (S.Tier_(AntUp::Carriers) >= 2) { Out.AddUnique(AntCard::Jaws); }
	if (S.Tier_(AntUp::Carriers) >= 3) { Out.AddUnique(AntCard::Trail); }
	if (S.Tier_(AntUp::Engineers) >= 1) { Out.AddUnique(AntCard::Lever); }
	if (S.Tier_(AntUp::Scouts) >= 1) { Out.AddUnique(AntCard::Spring); }
	if (S.Tier_(AntUp::Scouts) >= 2) { Out.AddUnique(AntCard::Legs); }
	if (S.Tier_(AntUp::Scouts) >= 3) { Out.AddUnique(AntCard::Quiet); }
}

bool ToolUnlocked(const FAntNestState& S, uint8 Tool)
{
	return Tool > AntTool::None && Tool < AntTool::Count && S.Tier_(AntUp::Tools) >= AntTool::NeedTier(Tool);
}

// ---------------------------------------------------------------------------------------------
// The save: a small text file, key=value per line, the last line is a checksum of everything before it.
// Written to a temp file first and moved into place; the save that was there becomes the backup.
// ---------------------------------------------------------------------------------------------

FString SavePath() { return FPaths::ProjectSavedDir() / TEXT("Nest") / TEXT("colony.sav"); }
FString BackupPath() { return FPaths::ProjectSavedDir() / TEXT("Nest") / TEXT("colony.bak"); }

namespace
{
	constexpr int32 SaveVersion = AntColony::SaveVersion;   // 2: the colony as a place (AntColony keys); 1 is read and migrated

	FString Encode(const FAntNestState& S)
	{
		FString Up;
		for (int32 i = 0; i < AntUp::Count; ++i) { Up += FString::Printf(TEXT("%s%d"), i ? TEXT(",") : TEXT(""), (int32)S.Tier_((uint8)i)); }
		FString T = FString::Printf(TEXT("antnest=%d\nfood=%d\njelly=%d\nlarvae=%d\nworkers=%d\nmood=%d\nup=%s\nmaxtier=%d\ntier=%d\nraids=%d\ncakes=%d\ncakesatmax=%d\nwipes=%d\nbesthaul=%d\n"),
			SaveVersion, S.Food, S.Jelly, S.Larvae, S.Workers, S.Mood, *Up, S.MaxTier, S.Tier, S.Raids, S.CakesHome, S.CakesAtMax, S.Wipes, S.BestHaul) + AntColony::EncodeKeys(S);
		return T + FString::Printf(TEXT("sum=%u\n"), FCrc::StrCrc32(*T));
	}

	bool Decode(const FString& Text, FAntNestState& S)
	{
		int32 SumAt = Text.Find(TEXT("sum="), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (SumAt <= 0)
		{
			return false;
		}
		const FString Body = Text.Left(SumAt);
		const uint32 Want = (uint32)FCString::Strtoui64(*Text.Mid(SumAt + 4), nullptr, 10);
		if (FCrc::StrCrc32(*Body) != Want)
		{
			return false;
		}
		TMap<FString, FString> KV;
		TArray<FString> Lines;
		Body.ParseIntoArrayLines(Lines);
		for (const FString& L : Lines)
		{
			FString K, V;
			if (L.Split(TEXT("="), &K, &V)) { KV.Add(K, V); }
		}
		auto Int = [&KV](const TCHAR* K, int32 Def) { const FString* V = KV.Find(K); return V ? FCString::Atoi(**V) : Def; };
		const int32 Ver = Int(TEXT("antnest"), 0);
		if (Ver < 1 || Ver > SaveVersion)
		{
			return false;   // written by a newer game (or not a colony at all)
		}
		Fresh(S);
		S.Version = SaveVersion;
		S.Food = FMath::Clamp(Int(TEXT("food"), 0), 0, 9999999);
		S.Jelly = FMath::Clamp(Int(TEXT("jelly"), 0), 0, 99999);
		S.Larvae = FMath::Clamp(Int(TEXT("larvae"), 3), 0, MaxLarvae);
		S.Workers = FMath::Clamp(Int(TEXT("workers"), 12), 0, MaxWorkers);
		S.Mood = FMath::Clamp(Int(TEXT("mood"), 50), 0, 100);
		S.MaxTier = FMath::Clamp(Int(TEXT("maxtier"), 1), 1, AntTier::Max);
		S.Tier = FMath::Clamp(Int(TEXT("tier"), 1), 1, S.MaxTier);
		S.Raids = FMath::Max(0, Int(TEXT("raids"), 0));
		S.CakesHome = FMath::Max(0, Int(TEXT("cakes"), 0));
		S.CakesAtMax = FMath::Max(0, Int(TEXT("cakesatmax"), 0));
		S.Wipes = FMath::Max(0, Int(TEXT("wipes"), 0));
		S.BestHaul = FMath::Max(0, Int(TEXT("besthaul"), 0));
		if (const FString* Up = KV.Find(TEXT("up")))
		{
			TArray<FString> Parts;
			Up->ParseIntoArray(Parts, TEXT(","));
			for (int32 i = 0; i < AntUp::Count && i < Parts.Num(); ++i) { S.Up[i] = (uint8)FMath::Clamp(FCString::Atoi(*Parts[i]), 0, AntUp::MaxTier); }
		}
		AntColony::DecodeKeys(KV, Ver, S);   // v2 keys, or a v1 colony migrated
		return true;
	}

	bool ReadFile(const FString& Path, FAntNestState& S)
	{
		FString Text;
		return FFileHelper::LoadFileToString(Text, *Path) && Decode(Text, S);
	}
}

bool Load(FAntNestState& S, FString* OutNote)
{
	if (ReadFile(SavePath(), S))
	{
		if (OutNote) { *OutNote = TEXT("save read"); }
		return true;
	}
	const bool bHadMain = IFileManager::Get().FileExists(*SavePath());
	if (ReadFile(BackupPath(), S))
	{
		if (OutNote) { *OutNote = bHadMain ? TEXT("the save is damaged: the backup was read") : TEXT("no save: the backup was read"); }
		return true;
	}
	Fresh(S);
	if (OutNote) { *OutNote = bHadMain ? TEXT("the save and its backup are damaged: a fresh colony") : TEXT("no save: a fresh colony"); }
	return false;
}

bool Save(const FAntNestState& S)
{
	IFileManager& FM = IFileManager::Get();
	const FString Path = SavePath(), Tmp = Path + TEXT(".tmp");
	FM.MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Encode(S), *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}
	// Only a save that reads back keeps the previous one as the backup (a damaged file must not push a good backup out).
	FAntNestState Check;
	if (!ReadFile(Tmp, Check))
	{
		FM.Delete(*Tmp);
		return false;
	}
	FAntNestState Old;
	if (ReadFile(Path, Old))
	{
		FM.Copy(*BackupPath(), *Path, true);
	}
	return FM.Move(*Path, *Tmp, true);
}

bool SaveAsV1(const FAntNestState& S)
{
	FString Up;
	for (int32 i = 0; i < AntUp::Count; ++i) { Up += FString::Printf(TEXT("%s%d"), i ? TEXT(",") : TEXT(""), (int32)S.Tier_((uint8)i)); }
	const FString T = FString::Printf(TEXT("antnest=1\nfood=%d\njelly=%d\nlarvae=%d\nworkers=%d\nmood=%d\nup=%s\nmaxtier=%d\ntier=%d\nraids=%d\ncakes=%d\ncakesatmax=%d\nwipes=%d\nbesthaul=%d\n"),
		S.Food, S.Jelly, S.Larvae, S.Workers, S.Mood, *Up, S.MaxTier, S.Tier, S.Raids, S.CakesHome, S.CakesAtMax, S.Wipes, S.BestHaul);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath()), true);
	IFileManager::Get().Delete(*BackupPath());
	return FFileHelper::SaveStringToFile(T + FString::Printf(TEXT("sum=%u\n"), FCrc::StrCrc32(*T)), *SavePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void Reset()
{
	IFileManager& FM = IFileManager::Get();
	const FString Old = FPaths::ProjectSavedDir() / TEXT("Nest") / TEXT("colony.old");
	if (FM.FileExists(*SavePath())) { FM.Move(*Old, *SavePath(), true); }
	FM.Delete(*BackupPath());
}

FString Summary()
{
	FAntNestState S;
	if (!ReadFile(SavePath(), S) && !ReadFile(BackupPath(), S))
	{
		return FString();
	}
	return FString::Printf(TEXT("Колония: рейдов %d  ·  тортов %d  ·  еда %d  ·  молочко %d  ·  Королева %s"), S.Raids, S.CakesHome, S.Food, S.Jelly, MoodName(S.Mood));
}

}   // namespace AntNest
