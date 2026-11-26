#include "AntFighter.h"
#include "AntFighterUi.h"
#include "AntCosmetics.h"
#include "AntMetaBus.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "AntSnowball.h"
#include "AntRts.h"
#include "KitchenLoot.h"
#include "KitchenFixture.h"
#include "HumanCharacter.h"
#include "HumanAIController.h"
#include "Critters.h"
#include "CreatureCharacters.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "JsonObjectConverter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/HUD.h"
#include "Camera/CameraComponent.h"
#include "AntDifficulty.h"

// The fighter: ranks, perks, medals, cosmetics, the profile and the server's side of the raid (docs/FIGHTER.md).
// The veterans' own rules are in AntVeterans.cpp, the screens in AntFighterUi.cpp, the dressing in AntCosmetics.cpp.

// =============================================================================================
// Tables
// =============================================================================================

namespace AntRank
{
	struct FDef { const TCHAR* Name; const TCHAR* Low; int32 Xp; const TCHAR* Ins; };
	static const FDef Defs[Count] =
	{
		{ TEXT("Рядовой"),          TEXT("рядовой"),          0,     TEXT("без знаков: чистый хитин") },
		{ TEXT("Ефрейтор"),         TEXT("ефрейтор"),         300,   TEXT("одна сахарная крупинка") },
		{ TEXT("Младший сержант"),  TEXT("младший сержант"),  800,   TEXT("две сахарные крупинки") },
		{ TEXT("Сержант"),          TEXT("сержант"),          1600,  TEXT("обломок спички поперёк спины") },
		{ TEXT("Старшина"),         TEXT("старшина"),         2800,  TEXT("две спички и крупинка") },
		{ TEXT("Прапор Щели"),      TEXT("прапор Щели"),      4500,  TEXT("крышечка от бутылки с крупинкой") },
		{ TEXT("Лейтенант Крошки"), TEXT("лейтенант Крошки"), 7000,  TEXT("крышечка и две спички") },
		{ TEXT("Герой Щели"),       TEXT("Герой Щели"),       10000, TEXT("золотая крышечка с тремя крупинками") },
	};
	const TCHAR* Name(int32 R) { return Defs[FMath::Clamp(R, 0, Count - 1)].Name; }
	const TCHAR* Lower(int32 R) { return Defs[FMath::Clamp(R, 0, Count - 1)].Low; }
	int32 XpFor(int32 R) { return Defs[FMath::Clamp(R, 0, Count - 1)].Xp; }
	const TCHAR* Insignia(int32 R) { return Defs[FMath::Clamp(R, 0, Count - 1)].Ins; }
	int32 Of(int32 Xp)
	{
		int32 R = 0;
		for (int32 i = 0; i < Count; ++i) { if (Xp >= Defs[i].Xp) { R = i; } }
		return R;
	}
}

namespace AntPerk
{
	static const FDef Defs[Castes][Tiers][2] =
	{
		{   // Фуражир
			{ { TEXT("Ломовой"), TEXT("Сила +35 %, скорость −10 %. Тащит то, что другим не сдвинуть") },
			  { TEXT("Челнок"), TEXT("С лёгким грузом (не тяжелее своей силы) +25 % скорости; сила −10 %") } },
			{ { TEXT("Выносливый"), TEXT("Здоровье +20 %, скорость −5 %") },
			  { TEXT("Шустрые лапы"), TEXT("Рывок восстанавливается на 30 % быстрее; здоровье −10 %") } },
			{ { TEXT("Артельщик"), TEXT("Когда груз несут двое и больше — его сила +25 %") },
			  { TEXT("Одиночка"), TEXT("Когда несёт один — его сила +25 %") } },
		},
		{   // Солдат
			{ { TEXT("Щитоносец"), TEXT("Получает на 25 % меньше урона; скорость −10 %") },
			  { TEXT("Кислотник"), TEXT("Плевок тратит на 30 % меньше кислоты; здоровье −10 %") } },
			{ { TEXT("Таран"), TEXT("Рывок на 40 % чаще; прыжок −10 %") },
			  { TEXT("Часовой"), TEXT("Стоя на месте получает на 30 % меньше урона; скорость −5 %") } },
			{ { TEXT("Ополченец"), TEXT("Рядом двое своих — урон по нему −15 %") },
			  { TEXT("Берсерк"), TEXT("Меньше 30 % здоровья: скорость +25 %, но урон по нему +15 %") } },
		},
		{   // Разведчик
			{ { TEXT("Ушной агент"), TEXT("Внутри людей держится вдвое дольше; здоровье −10 %") },
			  { TEXT("Попрыгун"), TEXT("Прыжок +25 %; сила −15 %") } },
			{ { TEXT("Бегун"), TEXT("Без груза +10 % скорости; здоровье −10 %") },
			  { TEXT("Цепкий"), TEXT("По стенам и потолку +25 % скорости; по полу −5 %") } },
			{ { TEXT("Курьер"), TEXT("С грузом +15 % скорости; сила −10 %") },
			  { TEXT("Ныряльщик"), TEXT("Мокрый — +40 % скорости (вода меньше мешает); сухой −5 %") } },
		},
		{   // Бочка
			{ { TEXT("Полевая кухня"), TEXT("Лечит в полтора раза шире, но на 25 % слабее") },
			  { TEXT("Санитар"), TEXT("Лечит в полтора раза сильнее, но вплотную (радиус −35 %)") } },
			{ { TEXT("Носилки"), TEXT("С товарищем в жвалах +25 % скорости; сила −10 %") },
			  { TEXT("Бочка с мёдом"), TEXT("Здоровье +25 %, скорость −10 %") } },
			{ { TEXT("Похоронщик"), TEXT("С телом товарища +30 % скорости (донести, пока не поздно)") },
			  { TEXT("Запасливый"), TEXT("Плевок дешевле на 20 %; лечение −10 %") } },
		},
		{   // Сапёр
			{ { TEXT("Отвёртка"), TEXT("Крепёж откручивает в 1.6 раза быстрее; ткань — медленнее (×0.8)") },
			  { TEXT("Портной"), TEXT("Скатерть, ленту, молнию — вдвое быстрее; крепёж медленнее (×0.8)") } },
			{ { TEXT("Слесарь"), TEXT("Тянет дверь с силой +30 %; скорость −5 %") },
			  { TEXT("Ухогрыз"), TEXT("Внутри людей держится в 1.6 раза дольше; здоровье −10 %") } },
			{ { TEXT("Сантехник"), TEXT("Мокрый — +40 % скорости; по стенам −10 %") },
			  { TEXT("Подрывник"), TEXT("Рывок на 30 % чаще; урон по нему +10 %") } },
		},
	};
	const FDef& Def(int32 Caste, int32 Tier, int32 Opt) { return Defs[FMath::Clamp(Caste, 0, Castes - 1)][FMath::Clamp(Tier, 0, Tiers - 1)][FMath::Clamp(Opt, 0, 1)]; }
	int32 NeedRank(int32 Tier) { static const int32 R[Tiers] = { 1, 3, 5 }; return R[FMath::Clamp(Tier, 0, Tiers - 1)]; }
	int32 Get(uint32 Bits, int32 Caste, int32 Tier) { const int32 Sh = (Caste * Tiers + Tier) * 2; return (int32)((Bits >> Sh) & 3u); }
	uint32 Set(uint32 Bits, int32 Caste, int32 Tier, int32 Choice)
	{
		const int32 Sh = (FMath::Clamp(Caste, 0, Castes - 1) * Tiers + FMath::Clamp(Tier, 0, Tiers - 1)) * 2;
		return (Bits & ~(3u << Sh)) | ((uint32)FMath::Clamp(Choice, 0, 2) << Sh);
	}
}

namespace AntFeat
{
	// Key, name, what for, icon (Icons/T_UI_Icon_*), cap colour (sRGB), XP.
	static const FDef Defs[Count] =
	{
		{ TEXT("slipper"),      TEXT("Пережил Розовую Плиту"), TEXT("Тапок Гиганта ударил рядом — и не попал"), TEXT("Slipper"), 0xE58FB0, 40 },
		{ TEXT("sneezed"),      TEXT("Был чихнут"), TEXT("Вылетел из носа Гиганта с чихом"), TEXT("Human"), 0x9FD3E6, 50 },
		{ TEXT("pickpocket"),   TEXT("Украл из кармана Гиганта"), TEXT("Вынес добычу из кармана человека"), TEXT("KidHand"), 0xC8731E, 60 },
		{ TEXT("drowned"),      TEXT("Утонул и выплыл"), TEXT("Начал тонуть — и выбрался живым"), TEXT("Jelly"), 0x5FB3B0, 40 },
		{ TEXT("runover"),      TEXT("Переехан своими"), TEXT("Свой же груз прокатился по тебе"), TEXT("Carriers"), 0xB3A285, 30 },
		{ TEXT("tablecloth"),   TEXT("Сорвал скатерть"), TEXT("Тянул скатерть, пока всё не поехало"), TEXT("Pin"), 0xF2A93B, 60 },
		{ TEXT("ear"),          TEXT("Побывал в ухе"), TEXT("Залез в ухо человеку"), TEXT("Lens"), 0xFFD27A, 40 },
		{ TEXT("knitting"),     TEXT("Довёл бабушку до вязания"), TEXT("Укусил Зелёную Тапочку в пучок — вязание на полу"), TEXT("GranSlipper"), 0x86C95A, 60 },
		{ TEXT("cake_flawless"),TEXT("Торт без потерь"), TEXT("Торт дома, и никто не погиб"), TEXT("Cake"), 0xF5B735, 120 },
		{ TEXT("plumber"),      TEXT("Водопроводчик"), TEXT("Был на кухне, когда её затопило"), TEXT("Wrench"), 0x58A9E0, 50 },
		{ TEXT("cake_home"),    TEXT("За сладкое"), TEXT("Донёс торт до гнезда"), TEXT("Cake"), 0xF2A93B, 80 },
		{ TEXT("last_out"),     TEXT("Последним из щели"), TEXT("Последним вернулся домой"), TEXT("NestEntrance"), 0xB3A285, 30 },
		{ TEXT("trophy_home"),  TEXT("Трофейщик"), TEXT("Донёс целый трофей"), TEXT("Medal"), 0xCDA8FF, 50 },
		{ TEXT("special_home"), TEXT("Находчивый"), TEXT("Донёс особую находку"), TEXT("Eye"), 0xB383EC, 50 },
		{ TEXT("contract"),     TEXT("Исполнитель"), TEXT("Выполнил приказ Королевы"), TEXT("Decree"), 0xFFD27A, 40 },
		{ TEXT("rescuer"),      TEXT("Не бросил своего"), TEXT("Донёс тело товарища до гнезда"), TEXT("Medic"), 0x86C95A, 60 },
		{ TEXT("undertaker"),   TEXT("Похоронщик"), TEXT("Пять тел донесено за службу"), TEXT("Medic"), 0x3F7D2A, 150 },
		{ TEXT("acid_rain"),    TEXT("Кислотный дождь"), TEXT("Десять красных за рейд"), TEXT("Acid"), 0xD9473B, 80 },
		{ TEXT("spider"),       TEXT("Паукобой"), TEXT("Одолел паука"), TEXT("Spider"), 0x6B3FA8, 70 },
		{ TEXT("ram"),          TEXT("Таран"), TEXT("Сбил рывком врага с грузом"), TEXT("Strength"), 0xC8731E, 30 },
		{ TEXT("legless"),      TEXT("Без ног, но в строю"), TEXT("Потерял четыре лапы и вернулся домой"), TEXT("Worker"), 0x9A2620, 80 },
		{ TEXT("rider"),        TEXT("Наездник"), TEXT("Полминуты верхом на Гиганте"), TEXT("Human"), 0x5FB3B0, 60 },
		{ TEXT("pancake"),      TEXT("Стал блином"), TEXT("Раздавлен тапком. Бывает"), TEXT("Slipper"), 0xD9473B, 20 },
		{ TEXT("ripper"),       TEXT("Рвач"), TEXT("Разорвал врага пополам вдвоём"), TEXT("FireAnt"), 0x9A2620, 50 },
		{ TEXT("doorman"),      TEXT("Швейцар"), TEXT("Открыл три двери за рейд"), TEXT("Gear"), 0xB3A285, 40 },
		{ TEXT("screwdriver"),  TEXT("Отвёрточник"), TEXT("Открутил крепёж"), TEXT("Wrench"), 0xCFC3AA, 40 },
		{ TEXT("first_raid"),   TEXT("Первый рейд"), TEXT("Вышел из щели в первый раз"), TEXT("Emblem"), 0xF2A93B, 20 },
		{ TEXT("ten_raids"),    TEXT("Десять рейдов"), TEXT("Десять рейдов за плечами"), TEXT("Emblem"), 0xF5B735, 150 },
		{ TEXT("sweet_tooth"),  TEXT("Сладкоежка"), TEXT("500 еды донесено за службу"), TEXT("Sugar"), 0xFBF5E6, 150 },
		{ TEXT("licked"),       TEXT("Облизан Слюнявым Танком"), TEXT("Бадрик лизнул — и ты жив"), TEXT("Dog"), 0xE58FB0, 40 },
		{ TEXT("cat_paw"),      TEXT("В лапе Тихой Лапы"), TEXT("Побывал в кошачьей лапе"), TEXT("Cat"), 0x33271F, 40 },
		{ TEXT("distraction"),  TEXT("Отвлекающий манёвр"), TEXT("Гигант гонялся за тобой, пока другие тащили"), TEXT("Phone"), 0x5FB3B0, 50 },
		{ TEXT("resurrected"),  TEXT("Вернулся с того света"), TEXT("Твоё тело донесли до гнезда"), TEXT("Larva"), 0xFBF5E6, 30 },
		{ TEXT("slipper_vet"),  TEXT("Ветеран тапка"), TEXT("Три удара тапком рядом за рейд — и ни одного попадания"), TEXT("GranSlipper"), 0xE58FB0, 100 },
		{ TEXT("survived"),     TEXT("Вернулся"), TEXT("Дошёл до гнезда живым"), TEXT("Ready"), 0x86C95A, 10 },
		{ TEXT("medic"),        TEXT("Полевой лекарь"), TEXT("Вылечил своим 200 здоровья за рейд"), TEXT("Medic"), 0x86C95A, 60 },
	};
	const FDef& Def(int32 Id) { return Defs[FMath::Clamp(Id, 0, Count - 1)]; }
	int32 FromKey(const FString& Key)
	{
		for (int32 i = 0; i < Count; ++i) { if (Key.Equals(Defs[i].Key, ESearchCase::IgnoreCase)) { return i; } }
		if (FCString::IsNumeric(*Key)) { const int32 N = FCString::Atoi(*Key); return N >= 0 && N < Count ? N : -1; }
		return -1;
	}
}

namespace AntCosm
{
	static const TCHAR* SlotNames[SlotCount] = { TEXT("Каска"), TEXT("Очки"), TEXT("Шея"), TEXT("Оружие"), TEXT("Спина"), TEXT("Раскраска"), TEXT("Хитин"), TEXT("Узор"), TEXT("Тропа"), TEXT("Жест") };
	// Item 0 of every slot is "nothing" / the natural look. NeedRank / NeedFeat (-1: none).
	static const FItem IHead[] = {
		{ TEXT("Без каски"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Жёлудь-каска"), TEXT("acorn"), 0, -1, 0x3A2412 },
		{ TEXT("Напёрсток"), TEXT("thimble"), 2, -1, 0xA7A9AD },
		{ TEXT("Крышечка от бутылки"), TEXT("bottlecap"), -1, AntFeat::CakeHome, 0xB0141A },
		{ TEXT("Колпачок маркера"), TEXT("marker"), 3, -1, 0x1F3FA6 },
		{ TEXT("Полжёлудя «для обтекаемости»"), TEXT("halfacorn"), -1, AntFeat::Ear, 0x3A2412 },
	};
	static const FItem IFace[] = {
		{ TEXT("Без очков"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Очки из бисера"), TEXT("beads"), 1, -1, 0x1B7FC2 },
		{ TEXT("Монокль из чешуйки"), TEXT("monocle"), 5, -1, 0xD8E6EE },
		{ TEXT("Очки-капли росы"), TEXT("dew"), -1, AntFeat::Drowned, 0x9FD3E6 },
	};
	static const FItem INeck[] = {
		{ TEXT("Ничего"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Шарф из нитки"), TEXT("scarf"), 1, -1, 0xB3221B },
		{ TEXT("Шайба-воротник"), TEXT("washer"), -1, AntFeat::Screwdriver, 0x8C8E93 },
		{ TEXT("Бантик из пряжи"), TEXT("yarn"), -1, AntFeat::Knitting, 0x86C95A },
	};
	static const FItem IWeapon[] = {
		{ TEXT("Только жвалы"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Спичка-копьё"), TEXT("match"), 2, -1, 0xC79A5C },
		{ TEXT("Зубочистка"), TEXT("toothpick"), -1, AntFeat::AcidRain, 0xE6D3A8 },
		{ TEXT("Горелая спичка"), TEXT("burnt"), 4, -1, 0x2A2420 },
	};
	static const FItem IBack[] = {
		{ TEXT("Ничего"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Рюкзак из шелухи"), TEXT("husk"), -1, AntFeat::SweetTooth, 0x151518 },
		{ TEXT("Сахарная ташка"), TEXT("sugarbag"), 4, -1, 0xF4EFE2 },
		{ TEXT("Лист-плащ"), TEXT("leaf"), -1, AntFeat::Rescuer, 0x3F7D2A },
	};
	static const FItem IPaint[] = {
		{ TEXT("Чистый"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Мука на щеках"), TEXT("flour"), 1, -1, 0xF7F3EA },
		{ TEXT("Полосы вареньем"), TEXT("jam"), -1, AntFeat::Slipper, 0xC0261E },
		{ TEXT("Шоколадные полосы"), TEXT("choco"), 3, -1, 0x4A2A14 },
	};
	static const FItem IShell[] = {
		{ TEXT("Природный"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Янтарный"), TEXT("amber"), 2, -1, 0x6A3A10 },
		{ TEXT("Карамельный"), TEXT("caramel"), 4, -1, 0x7A4A1A },
		{ TEXT("Угольный"), TEXT("coal"), -1, AntFeat::Pancake, 0x0E0E10 },
		{ TEXT("Медовый"), TEXT("honey"), 6, -1, 0x8A5A12 },
		{ TEXT("Золото Щели"), TEXT("gold"), 7, -1, 0xA67C1E },
	};
	static const FItem IPattern[] = {
		{ TEXT("Без узора"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Сахарные крапинки"), TEXT("dots"), 1, -1, 0xFBF5E6 },
		{ TEXT("Маковые точки"), TEXT("poppy"), 3, -1, 0x1A1A22 },
		{ TEXT("Полосы сахарной пудрой"), TEXT("stripes"), -1, AntFeat::CakeFlawless, 0xFBF5E6 },
	};
	static const FItem ITrail[] = {
		{ TEXT("Обычная тропа"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Янтарная"), TEXT("amber"), 1, -1, 0xF2A93B },
		{ TEXT("Мятная"), TEXT("mint"), 3, -1, 0x7FE0B0 },
		{ TEXT("Вишнёвая"), TEXT("cherry"), -1, AntFeat::Plumber, 0xD9473B },
		{ TEXT("Сахарная"), TEXT("sugar"), 5, -1, 0xFBF5E6 },
	};
	static const FItem IEmote[] = {
		{ TEXT("Без жеста"), TEXT("none"), 0, -1, 0 },
		{ TEXT("Победный топот"), TEXT("stomp"), 0, -1, 0 },
		{ TEXT("Усы-пропеллер"), TEXT("spin"), 1, -1, 0 },
		{ TEXT("Поклон Королеве"), TEXT("bow"), 3, -1, 0 },
		{ TEXT("Танец с крошкой"), TEXT("dance"), -1, AntFeat::CakeFlawless, 0 },
	};
	struct FSlotTable { const FItem* Items; int32 Num; };
	static const FSlotTable Tables[SlotCount] =
	{
		{ IHead, UE_ARRAY_COUNT(IHead) }, { IFace, UE_ARRAY_COUNT(IFace) }, { INeck, UE_ARRAY_COUNT(INeck) }, { IWeapon, UE_ARRAY_COUNT(IWeapon) },
		{ IBack, UE_ARRAY_COUNT(IBack) }, { IPaint, UE_ARRAY_COUNT(IPaint) }, { IShell, UE_ARRAY_COUNT(IShell) }, { IPattern, UE_ARRAY_COUNT(IPattern) },
		{ ITrail, UE_ARRAY_COUNT(ITrail) }, { IEmote, UE_ARRAY_COUNT(IEmote) },
	};
	const TCHAR* SlotName(int32 Slot) { return SlotNames[FMath::Clamp(Slot, 0, SlotCount - 1)]; }
	int32 NumItems(int32 Slot) { return Tables[FMath::Clamp(Slot, 0, SlotCount - 1)].Num; }
	const FItem& Item(int32 Slot, int32 Index)
	{
		const FSlotTable& T = Tables[FMath::Clamp(Slot, 0, SlotCount - 1)];
		return T.Items[FMath::Clamp(Index, 0, T.Num - 1)];
	}
	bool Unlocked(int32 Slot, int32 Index, int32 Rank, uint64 Medals)
	{
		const FItem& I = Item(Slot, Index);
		if (I.NeedFeat >= 0) { return (Medals & (1ull << I.NeedFeat)) != 0; }
		return Rank >= I.NeedRank;
	}
	FString UnlockText(int32 Slot, int32 Index)
	{
		const FItem& I = Item(Slot, Index);
		if (I.NeedFeat >= 0) { return FString::Printf(TEXT("медаль «%s»"), AntFeat::Def(I.NeedFeat).Name); }
		return I.NeedRank <= 0 ? FString(TEXT("с самого начала")) : FString::Printf(TEXT("звание «%s»"), AntRank::Name(I.NeedRank));
	}
}

namespace AntTrait
{
	static const TCHAR* Names[Count] = { TEXT("боится воды"), TEXT("тащит за двоих"), TEXT("трус"), TEXT("ветеран тапка"), TEXT("железные жвала"), TEXT("счастливчик") };
	static const TCHAR* Texts[Count] =
	{
		TEXT("Мокрый — на четверть медленнее, сухой — на 10 % быстрее"),
		TEXT("Сила вдвое больше обычной"),
		TEXT("Бежит от кошки, зато вынюхивает еду и показывает её командиру; здоровье −15 %"),
		TEXT("Тапок, упавший прямо на него, промахивается в половине случаев"),
		TEXT("Здоровье +25 %, скорость −10 %"),
		TEXT("Один смертельный удар за рейд оставляет его с крупицей здоровья"),
	};
	const TCHAR* Name(int32 Id) { return Names[FMath::Clamp(Id, 0, Count - 1)]; }
	const TCHAR* Text(int32 Id) { return Texts[FMath::Clamp(Id, 0, Count - 1)]; }
}

namespace AntDoctrine
{
	static const TCHAR* Names[Count] = { TEXT("Без доктрины"), TEXT("Клин"), TEXT("Рассыпной"), TEXT("Черепаха") };
	static const TCHAR* Texts[Count] =
	{
		TEXT("Отряд ходит как привык"),
		TEXT("Строй для переноски: с грузом весь отряд +15 % скорости, держится плотно за командиром"),
		TEXT("Разведка: отряд растягивается вдвое шире, +10 % скорости, здоровье −10 %"),
		TEXT("Оборона: урон по отряду −30 %, скорость −10 %, стоят кучно"),
	};
	const TCHAR* Name(int32 Id) { return Names[FMath::Clamp(Id, 0, Count - 1)]; }
	const TCHAR* Text(int32 Id) { return Texts[FMath::Clamp(Id, 0, Count - 1)]; }
	int32 NeedRank(int32 Id) { static const int32 R[Count] = { 0, 1, 2, 3 }; return R[FMath::Clamp(Id, 0, Count - 1)]; }
}

uint64 FAntProfile::MedalBits() const
{
	uint64 B = 0;
	for (int32 i = 0; i < FeatCount.Num() && i < 64; ++i) { if (FeatCount[i] > 0) { B |= 1ull << i; } }
	return B;
}

// =============================================================================================
// The replicated state
// =============================================================================================

AAntFighterState::AAntFighterState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
}

void AAntFighterState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntFighterState, Roster);
	DOREPLIFETIME(AAntFighterState, Fallen);
	DOREPLIFETIME(AAntFighterState, Highlights);
	DOREPLIFETIME(AAntFighterState, Doctrines);
	DOREPLIFETIME(AAntFighterState, HighlightsRaid);
	DOREPLIFETIME(AAntFighterState, DraftCap);
}

AAntFighterState* AAntFighterState::Get(const UWorld* World, bool bCreate)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AAntFighterState> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	if (!bCreate || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	AAntFighterState* FS = const_cast<UWorld*>(World)->SpawnActor<AAntFighterState>();
	if (FS)
	{
		AntVets::Load(FS);
		FS->DraftCap = (uint8)AntVets::DraftCapFor(World);
	}
	return FS;
}

FAntVeteran* AAntFighterState::FindVet(int32 Id)
{
	return Roster.FindByPredicate([Id](const FAntVeteran& V) { return V.Id == Id; });
}

uint8 AAntFighterState::DoctrineOf(const FString& PlayerId) const
{
	const FAntDoctrinePick* P = Doctrines.FindByPredicate([&PlayerId](const FAntDoctrinePick& D) { return D.PlayerId == PlayerId; });
	return P ? P->Doctrine : 0;
}

// =============================================================================================
// Multipliers (every machine)
// =============================================================================================

namespace
{
	TWeakObjectPtr<UWorld> GServerWorld;
	bool GFlooded = false;

	float PerkMul(const AAntCharacter* A, int32 Caste, int32 Tier, int32 Opt, uint8 Stat)
	{
		const bool bCarry = A->Carrying != nullptr;
		const int32 Key = Caste * 100 + Tier * 10 + Opt;
		switch (Key)
		{
		// Фуражир
		case 0:   return Stat == AntStat::Strength ? 1.35f : Stat == AntStat::Speed ? 0.9f : 1.f;
		case 1:   return Stat == AntStat::CarrySpeed ? ((bCarry && A->Carrying->Weight <= AntKit::ClassDef(A->AntClass).Strength) ? 1.25f : 1.f) : Stat == AntStat::Strength ? 0.9f : 1.f;
		case 10:  return Stat == AntStat::Health ? 1.2f : Stat == AntStat::Speed ? 0.95f : 1.f;
		case 11:  return Stat == AntStat::DashCd ? 0.7f : Stat == AntStat::Health ? 0.9f : 1.f;
		case 20:  return (Stat == AntStat::Strength && bCarry && A->Carrying->NumCarriers() >= 2) ? 1.25f : 1.f;
		case 21:  return (Stat == AntStat::Strength && (!bCarry || A->Carrying->NumCarriers() <= 1)) ? 1.25f : 1.f;
		// Солдат
		case 100: return Stat == AntStat::DamageTaken ? 0.75f : Stat == AntStat::Speed ? 0.9f : 1.f;
		case 101: return Stat == AntStat::AcidCost ? 0.7f : Stat == AntStat::Health ? 0.9f : 1.f;
		case 110: return Stat == AntStat::DashCd ? 0.6f : Stat == AntStat::Jump ? 0.9f : 1.f;
		case 111: return Stat == AntStat::DamageTaken ? (A->GetVelocity().Size() < 25.f ? 0.7f : 1.f) : Stat == AntStat::Speed ? 0.95f : 1.f;
		case 120:
			if (Stat == AntStat::DamageTaken)
			{
				int32 Near = 0;
				for (TActorIterator<AAntCharacter> It(A->GetWorld()); It && Near < 2; ++It)
				{
					if (*It != A && !It->bDead && It->Team == A->Team && It->Creature == 0 && FVector::DistSquared(It->GetActorLocation(), A->GetActorLocation()) < FMath::Square(400.f)) { ++Near; }
				}
				return Near >= 2 ? 0.85f : 1.f;
			}
			return 1.f;
		case 121:
		{
			const bool bLow = A->MaxHealth > 0.f && A->Health < A->MaxHealth * 0.3f;
			return !bLow ? 1.f : Stat == AntStat::Speed ? 1.25f : Stat == AntStat::DamageTaken ? 1.15f : 1.f;
		}
		// Разведчик
		case 200: return Stat == AntStat::InsideTime ? 2.f : Stat == AntStat::Health ? 0.9f : 1.f;
		case 201: return Stat == AntStat::Jump ? 1.25f : Stat == AntStat::Strength ? 0.85f : 1.f;
		case 210: return Stat == AntStat::Speed ? (bCarry ? 1.f : 1.1f) : Stat == AntStat::Health ? 0.9f : 1.f;
		case 211: return Stat == AntStat::ClimbSpeed ? 1.25f : (Stat == AntStat::Speed && !A->IsClimbing()) ? 0.95f : 1.f;
		case 220: return Stat == AntStat::CarrySpeed ? 1.15f : Stat == AntStat::Strength ? 0.9f : 1.f;
		case 221: return Stat == AntStat::WaterSpeed ? 1.4f : (Stat == AntStat::Speed && !A->HasStatus(AntStatus::Wet)) ? 0.95f : 1.f;
		// Бочка
		case 300: return Stat == AntStat::HealRadius ? 1.5f : Stat == AntStat::HealRate ? 0.75f : 1.f;
		case 301: return Stat == AntStat::HealRate ? 1.5f : Stat == AntStat::HealRadius ? 0.65f : 1.f;
		case 310: return (Stat == AntStat::Speed && A->HoldingAnt) ? 1.25f : Stat == AntStat::Strength ? 0.9f : 1.f;
		case 311: return Stat == AntStat::Health ? 1.25f : Stat == AntStat::Speed ? 0.9f : 1.f;
		case 320: return (Stat == AntStat::CarrySpeed && bCarry && A->Carrying->bBody) ? 1.3f : 1.f;
		case 321: return Stat == AntStat::AcidCost ? 0.8f : Stat == AntStat::HealRate ? 0.9f : 1.f;
		// Сапёр
		case 400: return Stat == AntStat::HeldWork ? 1.6f : Stat == AntStat::ClothWork ? 0.8f : 1.f;
		case 401: return Stat == AntStat::ClothWork ? 2.f : Stat == AntStat::HeldWork ? 0.8f : 1.f;
		case 410: return (Stat == AntStat::Strength && A->Pulling) ? 1.3f : Stat == AntStat::Speed ? 0.95f : 1.f;
		case 411: return Stat == AntStat::InsideTime ? 1.6f : Stat == AntStat::Health ? 0.9f : 1.f;
		case 420: return Stat == AntStat::WaterSpeed ? 1.4f : Stat == AntStat::ClimbSpeed ? 0.9f : 1.f;
		case 421: return Stat == AntStat::DashCd ? 0.7f : Stat == AntStat::DamageTaken ? 1.1f : 1.f;
		default:  return 1.f;
		}
	}

	float VetMul(const AAntCharacter* A, uint8 Stat)
	{
		float M = 1.f;
		const uint8 T = A->VetTraits;
		auto Has = [T](int32 Id) { return (T & (1 << Id)) != 0; };
		switch (Stat)
		{
		case AntStat::Strength: if (Has(AntTrait::Double)) { M *= 2.f; } break;
		case AntStat::Speed:
			if (Has(AntTrait::FearsWater)) { M *= A->HasStatus(AntStatus::Wet) ? 0.75f : 1.1f; }
			if (Has(AntTrait::IronJaws)) { M *= 0.9f; }
			if (A->VetDoctrine == AntDoctrine::Loose) { M *= 1.1f; }
			if (A->VetDoctrine == AntDoctrine::Turtle) { M *= 0.9f; }
			break;
		case AntStat::CarrySpeed: if (A->VetDoctrine == AntDoctrine::Wedge) { M *= 1.15f; } break;
		case AntStat::Health:
			M *= 1.f + 0.06f * A->VetRank;
			if (Has(AntTrait::Coward)) { M *= 0.85f; }
			if (Has(AntTrait::IronJaws)) { M *= 1.25f; }
			if (A->VetDoctrine == AntDoctrine::Loose) { M *= 0.9f; }
			break;
		case AntStat::DamageTaken: if (A->VetDoctrine == AntDoctrine::Turtle) { M *= 0.7f; } break;
		default: break;
		}
		return M;
	}
}

namespace AntFighter
{
	const FAntLoadout* LoadoutOf(const AAntCharacter* Ant)
	{
		const AAntPlayerState* PS = Ant ? Ant->GetPlayerState<AAntPlayerState>() : nullptr;
		return PS ? &PS->Loadout : nullptr;
	}

	FString IdOf(const APlayerState* PS)
	{
		if (const AAntPlayerState* A = Cast<AAntPlayerState>(PS))
		{
			if (!A->Loadout.ProfileId.IsEmpty()) { return A->Loadout.ProfileId; }
		}
		return PS ? PS->GetPlayerName() : FString();
	}

	float Mul(const AAntCharacter* Ant, uint8 Stat)
	{
		if (!Ant || Ant->Creature != 0 || Ant->Team != 0)
		{
			return 1.f;
		}
		float M = 1.f;
		if (const FAntLoadout* L = LoadoutOf(Ant))
		{
			if (L->Perks != 0)
			{
				const int32 Caste = FMath::Clamp((int32)Ant->AntClass, 0, AntPerk::Castes - 1);
				for (int32 t = 0; t < AntPerk::Tiers; ++t)
				{
					const int32 C = AntPerk::Get(L->Perks, Caste, t);
					if (C > 0 && L->Rank >= AntPerk::NeedRank(t)) { M *= PerkMul(Ant, Caste, t, C - 1, Stat); }
				}
			}
		}
		if (Ant->VetTraits != 0 || Ant->VetRank != 0 || Ant->VetDoctrine != 0)
		{
			M *= VetMul(Ant, Stat);
		}
		return M;
	}

	float SpeedMul(const AAntCharacter* Ant)
	{
		float M = Mul(Ant, AntStat::Speed);
		if (Ant && Ant->Carrying) { M *= Mul(Ant, AntStat::CarrySpeed); }
		if (Ant && Ant->IsClimbing()) { M *= Mul(Ant, AntStat::ClimbSpeed); }
		if (Ant && Ant->HasStatus(AntStatus::Wet)) { M *= Mul(Ant, AntStat::WaterSpeed); }
		return M;
	}

	float HeldMul(const AAntCharacter* By, const AActor* Held)
	{
		if (!By) { return 1.f; }
		const bool bFastener = Held && Held->IsA<AKitchenFastener>();
		const float Caste = By->AntClass == EAntClass::Sapper ? 1.5f : 1.f;
		return Caste * Mul(By, bFastener ? AntStat::HeldWork : AntStat::ClothWork);
	}

	bool HasPerk(const AAntCharacter* Ant, int32 Tier, int32 Opt)
	{
		const FAntLoadout* L = LoadoutOf(Ant);
		return L && L->Rank >= AntPerk::NeedRank(Tier) && AntPerk::Get(L->Perks, (int32)Ant->AntClass, Tier) == Opt + 1;
	}
}

// =============================================================================================
// The local profile
// =============================================================================================

namespace
{
	FAntProfile GProfile;
	bool bProfileLoaded = false;
	FString GProfileKey;
	FAntLoadout GSent;
	bool bSentOnce = false;
	double GLastSend = -100.0;
	TWeakObjectPtr<APlayerState> GSentTo;

	FString ProfilePath(const FString& Key)
	{
		FString Safe = Key;
		for (TCHAR& C : Safe.GetCharArray()) { if (C != 0 && !FChar::IsAlnum(C) && C != '_' && C != '-') { C = '_'; } }
		return FPaths::ProjectSavedDir() / TEXT("Fighter") / FString::Printf(TEXT("profile_%s.json"), *Safe);
	}

	FString LocalKey(const APlayerController* PC)
	{
		FString Over;
		if (FParse::Value(FCommandLine::Get(), TEXT("AntProfile="), Over) && !Over.IsEmpty()) { return Over; }
		const APlayerState* PS = PC ? PC->PlayerState : nullptr;
		if (PS && PS->GetUniqueId().IsValid())
		{
			const FString Id = PS->GetUniqueId().ToString();
			if (Id.Len() >= 15 && Id.IsNumeric()) { return Id; }   // a Steam id
		}
		return TEXT("local");
	}

	void FixProfile(FAntProfile& P)
	{
		P.FeatCount.SetNumZeroed(AntFeat::Count);
		P.Cosm.SetNumZeroed(AntCosm::SlotCount);
		if (P.Cosm[AntCosm::Head] == 0 && P.Raids == 0 && P.Xp == 0) { P.Cosm[AntCosm::Head] = 1; }   // the acorn helmet everybody starts with
		for (int32 s = 0; s < AntCosm::SlotCount; ++s)
		{
			if (!AntCosm::Unlocked(s, P.Cosm[s], P.Rank(), P.MedalBits())) { P.Cosm[s] = 0; }   // a reset or an edited file: back to what is earned
		}
		P.Version = 1;
	}

	void LoadProfile(const FString& Key)
	{
		GProfileKey = Key;
		GProfile = FAntProfile();
		FString Json;
		const FString Path = ProfilePath(Key);
		if (FFileHelper::LoadFileToString(Json, *Path) && FJsonObjectConverter::JsonObjectStringToUStruct(Json, &GProfile, 0, 0))
		{
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER profile %s loaded: xp %d rank %d raids %d medals %d"), *Key, GProfile.Xp, GProfile.Rank(), GProfile.Raids, FMath::CountBits(GProfile.MedalBits()));
		}
		else
		{
			GProfile = FAntProfile();
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER profile %s: new (%s)"), *Key, *Path);
		}
		GProfile.Id = Key;
		FixProfile(GProfile);
		bProfileLoaded = true;
	}

	void Toast(const FString& Title, const FString& Line, const TCHAR* Icon, uint32 Color, uint8 Kind)
	{
		FAntFighterToast T;
		T.Title = Title;
		T.Line = Line;
		T.Icon = Icon ? Icon : TEXT("Medal");
		T.Color = Color;
		T.Kind = Kind;
		T.Time = FPlatformTime::Seconds();
		AntFighterUi::Toasts().Add(T);
		while (AntFighterUi::Toasts().Num() > 6) { AntFighterUi::Toasts().RemoveAt(0); }
	}

	/** Cumulative medals the profile itself decides (service totals). */
	void CheckServiceMedals(AAntPlayerController* PC)
	{
		auto Give = [PC](int32 Id)
		{
			if (GProfile.FeatCount[Id] == 0)
			{
				GProfile.FeatCount[Id] = 1;
				const AntFeat::FDef& D = AntFeat::Def(Id);
				Toast(TEXT("МЕДАЛЬ"), D.Name, D.Icon, D.Color, 1);
				GProfile.Xp += D.Xp + 100;
				GProfile.LastRaid.Add(FString::Printf(TEXT("+%d опыта: медаль «%s»"), D.Xp + 100, D.Name));
				UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER medal (service) %s"), D.Key);
			}
		};
		if (GProfile.TotalFood >= 500) { Give(AntFeat::SweetTooth); }
		if (GProfile.TotalRescues >= 5) { Give(AntFeat::Undertaker); }
		if (GProfile.Raids >= 10) { Give(AntFeat::TenRaids); }
	}
}

namespace AntFighter
{
	void LocalScript(AAntPlayerController* PC);
	void ViewAnt(APlayerController* PC, AAntCharacter* Ant, float Dist, float Yaw, float Height);

	FAntProfile& Profile() { return GProfile; }

	void SaveProfile()
	{
		if (!bProfileLoaded) { return; }
		FString Json;
		if (FJsonObjectConverter::UStructToJsonObjectString(GProfile, Json))
		{
			const FString Path = ProfilePath(GProfileKey);
			const FString Tmp = Path + TEXT(".tmp");
			if (FFileHelper::SaveStringToFile(Json, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				IFileManager::Get().Move(*Path, *Tmp, true, true);
			}
		}
	}

	FAntLoadout MakeLoadout()
	{
		FAntLoadout L;
		L.Xp = GProfile.Xp;
		L.Rank = (uint8)GProfile.Rank();
		L.Perks = GProfile.Perks;
		L.Medals = GProfile.MedalBits();
		L.Cosm = GProfile.Cosm;
		L.bInsignia = GProfile.bInsignia;
		L.Raids = GProfile.Raids;
		L.ProfileId = GProfileKey == TEXT("local") ? FString::Printf(TEXT("local:%s"), *GProfile.Name) : GProfileKey;
		return L;
	}

	void PushLoadout(AAntPlayerController* PC)
	{
		if (!PC || !PC->PlayerState) { return; }
		FAntLoadout L = MakeLoadout();
		L.Stamp = (uint8)(GSent.Stamp + 1);
		GSent = L;
		bSentOnce = true;
		GLastSend = FPlatformTime::Seconds();
		GSentTo = PC->PlayerState;
		PC->ServerFighter(L);
	}

	void LocalTick(AAntPlayerController* PC)
	{
		Init();
		if (!PC || !PC->IsLocalController() || !PC->PlayerState)
		{
			return;
		}
		if (!bProfileLoaded)
		{
			LoadProfile(LocalKey(PC));
			GProfile.Name = PC->PlayerState->GetPlayerName();
		}
		// A new player state (a map travel, a reconnect) or nothing sent yet: send the loadout.
		if (!bSentOnce || GSentTo.Get() != PC->PlayerState)
		{
			GProfile.Name = PC->PlayerState->GetPlayerName();
			PushLoadout(PC);
		}
		LocalScript(PC);
	}

	void ViewAnt(APlayerController* PC, AAntCharacter* Ant, float Dist, float Yaw, float Height)
	{
		// A fixed camera in front of an ant, looking at it (screenshots of the look, every machine).
		static TWeakObjectPtr<ACameraActor> Cam;
		if (!PC || !Ant) { return; }
		if (!Cam.IsValid()) { Cam = PC->GetWorld()->SpawnActor<ACameraActor>(); }
		const FVector At = Ant->GetActorLocation() + FVector(0.f, 0.f, 4.f);
		const FVector Dir = FRotator(0.f, Ant->GetActorRotation().Yaw + Yaw, 0.f).Vector();
		const FVector From = At + Dir * Dist + FVector(0.f, 0.f, Height);
		Cam->SetActorLocationAndRotation(From, (At - From).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(50.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		PC->SetViewTargetWithBlend(Cam.Get(), 0.f);
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER cam on %s from %s"), *Ant->GetDisplayName(), *From.ToCompactString());
	}

	void LocalScript(AAntPlayerController* PC)
	{
		// -AntFighterDo="t:cmd;..." runs on ANY machine (a client has no game mode, -AntDo does not run there):
		// cam <name part|me|vet> [dist] [yaw] [height] | camoff | shot | screen <0|1|2> [tab] | emote. t: seconds of this world.
		static TArray<TPair<float, FString>> Script;
		static bool bParsed = false;
		if (!bParsed)
		{
			bParsed = true;
			FString S;
			if (FParse::Value(FCommandLine::Get(), TEXT("AntFighterDo="), S, false))
			{
				TArray<FString> Steps;
				S.ParseIntoArray(Steps, TEXT(";"));
				for (const FString& St : Steps)
				{
					FString At, Cmd;
					if (St.Split(TEXT(":"), &At, &Cmd)) { Script.Add(TPair<float, FString>(FCString::Atof(*At), Cmd.TrimStartAndEnd())); }
				}
			}
		}
		const float Now = PC->GetWorld()->GetTimeSeconds();
		for (int32 i = 0; i < Script.Num(); )
		{
			if (Script[i].Key > Now) { ++i; continue; }
			TArray<FString> A;
			Script[i].Value.ParseIntoArrayWS(A);
			Script.RemoveAt(i);
			if (A.Num() == 0) { continue; }
			auto Num = [&A](int32 k, float Def) { return A.IsValidIndex(k) ? FCString::Atof(*A[k]) : Def; };
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER local do %.1f (%s): %s"), Now, PC->GetNetMode() == NM_Client ? TEXT("client") : TEXT("host"), *FString::Join(A, TEXT(" ")));
			if (A[0] == TEXT("cam"))
			{
				const FString Who = A.IsValidIndex(1) ? A[1] : FString(TEXT("me"));
				AAntCharacter* Pick = nullptr;
				for (TActorIterator<AAntCharacter> It(PC->GetWorld()); It; ++It)
				{
					if (It->bDead || It->Creature != 0) { continue; }
					const bool bMe = It->GetController() == PC;
					if ((Who == TEXT("me") && bMe) || (Who == TEXT("vet") && !It->VetName.IsEmpty())
						|| (Who != TEXT("me") && Who != TEXT("vet") && It->GetPlayerState() && It->GetPlayerState()->GetPlayerName().Contains(Who))
						|| (Who == TEXT("other") && !bMe && It->GetPlayerState() && !It->GetPlayerState()->IsABot()))
					{
						Pick = *It;
						break;
					}
				}
				ViewAnt(PC, Pick, Num(2, 55.f), Num(3, 0.f), Num(4, 14.f));
			}
			else if (A[0] == TEXT("camoff")) { PC->SetViewTargetWithBlend(PC->GetPawn(), 0.f); }
			else if (A[0] == TEXT("shot")) { PC->ConsoleCommand(TEXT("Shot showui"), false); }
			else if (A[0] == TEXT("screen")) { if (Num(1, 0.f) < 0.5f) { AntFighterUi::Close(); } else { AntFighterUi::Open(PC, (uint8)Num(1, 1.f), (int32)Num(2, 0.f)); } }
			else if (A[0] == TEXT("emote")) { PC->DoEmote(); }
			else if (A[0] == TEXT("hud") && PC->GetHUD()) { PC->GetHUD()->bShowHUD = Num(1, 1.f) > 0.5f; }
			else if (A[0] == TEXT("con") && A.Num() > 1)
			{
				FString C;
				for (int32 k = 1; k < A.Num(); ++k) { C += (k > 1 ? TEXT(" ") : TEXT("")) + A[k]; }
				PC->ConsoleCommand(C, false);
			}
		}
	}

	void OnClient(AAntPlayerController* PC, uint8 Kind, int32 A, int32 B, const FString& Text)
	{
		if (!bProfileLoaded) { LoadProfile(LocalKey(PC)); }
		const int32 RankBefore = GProfile.Rank();
		switch (Kind)
		{
		case ClientKind::Xp:
			GProfile.Xp = FMath::Max(0, GProfile.Xp + A);
			GProfile.LastRaid.Add(FString::Printf(TEXT("%+d опыта: %s"), A, *Text));
			GProfile.LastRaidXp += A;
			break;
		case ClientKind::Feat:
		{
			if (A < 0 || A >= AntFeat::Count) { break; }
			const bool bNew = GProfile.FeatCount[A] == 0;
			GProfile.FeatCount[A]++;
			const AntFeat::FDef& D = AntFeat::Def(A);
			Toast(bNew ? TEXT("НОВАЯ МЕДАЛЬ") : TEXT("ПОДВИГ"), D.Name, D.Icon, D.Color, bNew ? 1 : 2);
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER client feat %s (%s), count %d"), D.Key, bNew ? TEXT("new medal") : TEXT("again"), GProfile.FeatCount[A]);
			break;
		}
		case ClientKind::RaidDone:
		{
			GProfile.Raids++;
			GProfile.TotalFood += B;
			CheckServiceMedals(PC);
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER raid %d done: +%d xp this raid, xp %d rank %d (%s), raids %d"), A, GProfile.LastRaidXp, GProfile.Xp, GProfile.Rank(), AntRank::Name(GProfile.Rank()), GProfile.Raids);
			break;
		}
		case ClientKind::SetXp:
			GProfile.Xp = FMath::Max(0, A);
			break;
		case ClientKind::UnlockAll:
			for (int32 i = 0; i < AntFeat::Count; ++i) { GProfile.FeatCount[i] = FMath::Max(1, GProfile.FeatCount[i]); }
			GProfile.Xp = FMath::Max(GProfile.Xp, AntRank::XpFor(AntRank::Count - 1));
			Toast(TEXT("АДМИН"), TEXT("Все медали и облик открыты"), TEXT("Medal"), 0xF2A93B, 2);
			break;
		case ClientKind::Reset:
		{
			const FString Name = GProfile.Name;
			GProfile = FAntProfile();
			GProfile.Name = Name;
			FixProfile(GProfile);
			Toast(TEXT("АДМИН"), TEXT("Профиль сброшен"), TEXT("Stamp"), 0xD9473B, 2);
			break;
		}
		case ClientKind::Note:
			if (A == 1) { GProfile.LastRaid.Reset(); GProfile.LastRaidXp = 0; GProfile.LastRankBefore = GProfile.Rank(); }   // a raid starts
			else if (A == 2) { GProfile.Perks = AntPerk::Set(GProfile.Perks, B / 10, B % 10, FCString::Atoi(*Text)); }   // dev: perk
			else if (A == 3) { GProfile.Cosm.SetNumZeroed(AntCosm::SlotCount); GProfile.Cosm[FMath::Clamp(B, 0, AntCosm::SlotCount - 1)] = (uint8)FCString::Atoi(*Text); }   // dev: cosmetic
			else if (A == 4) { AntFighterUi::Open(PC, (uint8)B, FCString::Atoi(*Text)); }   // dev: a screen
			else if (A == 5) { GProfile.TotalRescues += B; }
			else if (A == 6) { AntFighterUi::Close(); }
			else if (A == 7) { AntFighterUi::Open(PC, AntFighterUi::Screen::Profile, 1); AntFighterUi::SetCaste(B); }   // the perk pick (AwardRank / tutorial)
			break;
		default:
			break;
		}
		const int32 RankAfter = GProfile.Rank();
		if (RankAfter > RankBefore)
		{
			Toast(TEXT("НОВОЕ ЗВАНИЕ"), AntRank::Name(RankAfter), TEXT("Emblem"), 0xF5B735, 0);
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER rank up: %s -> %s (xp %d)"), AntRank::Name(RankBefore), AntRank::Name(RankAfter), GProfile.Xp);
		}
		FixProfile(GProfile);
		SaveProfile();
		PushLoadout(PC);
	}
}

// =============================================================================================
// The server: loadouts, feats, the raid's tally
// =============================================================================================

namespace AntFighter
{
	void ServerLoadout(AAntPlayerController* PC, const FAntLoadout& L)
	{
		AAntPlayerState* PS = PC ? PC->GetPlayerState<AAntPlayerState>() : nullptr;
		if (!PS) { return; }
		FAntLoadout N = L;
		N.Rank = (uint8)AntRank::Of(N.Xp);
		N.Cosm.SetNumZeroed(AntCosm::SlotCount);
		for (int32 s = 0; s < AntCosm::SlotCount; ++s)
		{
			if (!AntCosm::Unlocked(s, N.Cosm[s], N.Rank, N.Medals)) { N.Cosm[s] = 0; }
		}
		if (N.ProfileId.IsEmpty()) { N.ProfileId = PS->GetPlayerName(); }
		const bool bFirst = PS->Loadout.ProfileId.IsEmpty();
		PS->Loadout = N;
		PS->ForceNetUpdate();
		if (AAntCharacter* Ant = Cast<AAntCharacter>(PS->GetPawn()))
		{
			Ant->RefreshStats();
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER loadout of %s (%s): xp %d rank %d (%s), perks 0x%08x, medals %d, cosm %s%s"), *PS->GetPlayerName(), *N.ProfileId, N.Xp, (int32)N.Rank,
			AntRank::Name(N.Rank), N.Perks, FMath::CountBits(N.Medals), *FString::JoinBy(N.Cosm, TEXT(","), [](uint8 C) { return FString::FromInt(C); }), bFirst ? TEXT(" (joined)") : TEXT(""));
		Init();
		if (UWorld* W = PS->GetWorld()) { GServerWorld = W; AAntFighterState::Get(W, true); }
	}

	FAntFighterTally* TallyOfPS(APlayerState* PS)
	{
		AAntFighterState* FS = PS ? AAntFighterState::Get(PS->GetWorld(), true) : nullptr;
		return FS ? &FS->Tally.FindOrAdd(PS) : nullptr;
	}

	FAntFighterTally* TallyOf(const AAntCharacter* Ant)
	{
		if (!Ant || Ant->Creature != 0 || Ant->Team != 0 || !Ant->HasAuthority()) { return nullptr; }
		APlayerState* PS = const_cast<AAntCharacter*>(Ant)->GetPlayerState();
		return (PS && !PS->IsABot()) ? TallyOfPS(PS) : nullptr;
	}

	void FeatPS(APlayerState* PS, uint8 Id)
	{
		AAntPlayerState* APS = Cast<AAntPlayerState>(PS);
		if (!APS || Id >= AntFeat::Count || APS->IsABot()) { return; }
		FAntFighterTally* T = TallyOfPS(PS);
		if (!T || T->Feats.Contains(Id)) { return; }   // once a raid
		T->Feats.Add(Id);
		const bool bNew = (APS->Loadout.Medals & (1ull << Id)) == 0;
		if (bNew) { T->NewMedals.Add(Id); APS->Loadout.Medals |= 1ull << Id; }
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER feat %s «%s» for %s%s"), AntFeat::Def(Id).Key, AntFeat::Def(Id).Name, *PS->GetPlayerName(), bNew ? TEXT(" (new medal)") : TEXT(""));
		if (AAntPlayerController* PC = Cast<AAntPlayerController>(PS->GetOwner()))
		{
			PC->ClientFighter(ClientKind::Feat, Id, 0, FString());
		}
	}

	void AwardRank(APlayerState* PS, int32 Rank)
	{
		AAntPlayerController* PC = PS ? Cast<AAntPlayerController>(PS->GetOwner()) : nullptr;
		const AAntPlayerState* A = Cast<AAntPlayerState>(PS);
		if (!PC || !A) { return; }
		const int32 Xp = AntRank::XpFor(FMath::Clamp(Rank, 0, AntRank::Count - 1));
		if (A->Loadout.Xp >= Xp) { return; }
		PC->ClientFighter(ClientKind::SetXp, Xp, 0, FString());
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER award rank %s to %s"), AntRank::Name(Rank), *PS->GetPlayerName());
	}

	void AwardMedal(APlayerState* PS, uint8 FeatId) { FeatPS(PS, FeatId); }

	bool AwardMedalKey(APlayerState* PS, const FString& Key)
	{
		const int32 Id = AntFeat::FromKey(Key);
		if (Id < 0) { return false; }
		FeatPS(PS, (uint8)Id);
		return true;
	}

	void OpenPerkPick(APlayerState* PS, int32 Caste)
	{
		if (AAntPlayerController* PC = PS ? Cast<AAntPlayerController>(PS->GetOwner()) : nullptr)
		{
			PC->ClientFighter(ClientKind::Note, 7, Caste, FString());
		}
	}

	void Feat(AAntCharacter* Ant, uint8 Id)
	{
		if (Ant && Ant->HasAuthority() && Ant->Creature == 0 && Ant->Team == 0)
		{
			if (APlayerState* PS = Ant->GetPlayerState()) { FeatPS(PS, Id); }
		}
	}

	void OnDelivered(AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers, int32 Value)
	{
		if (!Food) { return; }
		TArray<AAntCharacter*> Players;
		for (AAntCharacter* A : Carriers) { if (A && A->Team == 0 && A->Creature == 0 && A->GetPlayerState() && !A->GetPlayerState()->IsABot()) { Players.Add(A); } }
		AAntFighterState* FS = Carriers.Num() > 0 && Carriers[0] ? AAntFighterState::Get(Carriers[0]->GetWorld(), true) : nullptr;
		for (AAntCharacter* A : Carriers)
		{
			if (A && A->VetId >= 0 && FS)
			{
				if (FAntVeteran* V = FS->FindVet(A->VetId)) { V->Carried += FMath::Max(1, Food->Weight); V->Xp += 10 + Food->Weight; }
			}
		}
		if (Players.Num() == 0) { return; }
		const float Share = (float)FMath::Max(0, Value) / Players.Num();
		for (AAntCharacter* A : Players)
		{
			if (FAntFighterTally* T = TallyOf(A))
			{
				T->Food += Share;
				if (Food->bMain) { T->CakeHome = true; Feat(A, AntFeat::CakeHome); }
				if (Food->bTrophy) { Feat(A, AntFeat::Trophy); }
				if (Food->Special != 0) { Feat(A, AntFeat::Special); }
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER delivered «%s» value %d: %d players share %.1f each"), *Food->LootName, Value, Players.Num(), Share);
	}

	void OnBodyHome(AController* BodyOwner, const TArray<AAntCharacter*>& Carriers)
	{
		for (AAntCharacter* A : Carriers)
		{
			if (FAntFighterTally* T = TallyOf(A))
			{
				T->Rescues++;
				Feat(A, AntFeat::Rescuer);
				if (AAntPlayerController* PC = Cast<AAntPlayerController>(A->GetController())) { PC->ClientFighter(ClientKind::Note, 5, 1, FString()); }
			}
		}
		if (BodyOwner && BodyOwner->PlayerState)
		{
			if (FAntFighterTally* T = TallyOfPS(BodyOwner->PlayerState)) { T->bRescued = true; }
			FeatPS(BodyOwner->PlayerState, AntFeat::Resurrected);
		}
	}

	void OnHealed(AAntCharacter* Nurse, AAntCharacter* Who, float Amount)
	{
		if (Nurse == Who || Amount <= 0.f) { return; }
		if (FAntFighterTally* T = TallyOf(Nurse))
		{
			T->Healed += Amount;
			if (T->Healed >= 200.f) { Feat(Nurse, AntFeat::Medic); }
		}
	}

	void OnFastener(AAntCharacter* By)
	{
		if (FAntFighterTally* T = TallyOf(By)) { T->Fasteners++; Feat(By, AntFeat::Screwdriver); }
	}

	void OnFixtureWork(const FString& Id, AAntCharacter* By)
	{
		if (Id == TEXT("tablecloth")) { Feat(By, AntFeat::Tablecloth); }
		if (FAntFighterTally* T = TallyOf(By)) { T->Fasteners++; }
	}

	void OnInside(AAntCharacter* Ant, int32 Spot, uint8 What)
	{
		// What: 0 entered, 1 stole from a pocket, 2 sneezed out, 3 bit the bun (the knitting falls)
		FAntFighterTally* T = TallyOf(Ant);
		if (!T) { return; }
		T->Inside++;
		if (What == 0 && Spot == 1) { Feat(Ant, AntFeat::Ear); }
		if (What == 1) { Feat(Ant, AntFeat::Pickpocket); }
		if (What == 2) { Feat(Ant, AntFeat::Sneezed); }
		if (What == 3) { Feat(Ant, AntFeat::Knitting); }
	}

	void OnStompNear(AAntCharacter* Ant, bool bHit)
	{
		FAntFighterTally* T = TallyOf(Ant);
		if (!T || bHit) { return; }
		T->Stomps++;
		Feat(Ant, AntFeat::Slipper);
		if (T->Stomps >= 3) { Feat(Ant, AntFeat::SlipperVet); }
	}

	void OnDistract(AAntCharacter* Ant)
	{
		if (FAntFighterTally* T = TallyOf(Ant)) { T->Distract++; Feat(Ant, AntFeat::Distraction); }
	}

	void OnRunOver(AAntCharacter* Ant) { Feat(Ant, AntFeat::RunOver); }

	void OnFlood() { GFlooded = true; }

	void OnNpcHold(AAntCharacter* Ant, AActor* Holder)
	{
		if (const ACritter* C = Cast<ACritter>(Holder))
		{
			Feat(Ant, C->Kind == 0 ? AntFeat::Licked : AntFeat::CatPaw);
		}
	}

	void OnEnv(AAntCharacter* Ant, bool bDrowning)
	{
		if (!Ant) { return; }
		if (bDrowning) { Ant->bFighterWasDrowning = true; return; }
		if (Ant->bFighterWasDrowning && !Ant->bDead)
		{
			Ant->bFighterWasDrowning = false;
			Feat(Ant, AntFeat::Drowned);
		}
	}

	FString CauseOf(const AAntCharacter* Ant, AActor* Causer)
	{
		if (const AHumanCharacter* H = Cast<AHumanCharacter>(Causer))
		{
			if (H->Fig == HumanFig::Grandma) { return TEXT("прихлопнут Зелёной Тапочкой"); }
			if (H->Fig == HumanFig::Kid) { return TEXT("растоптан малышом"); }
			return H->bSpraying ? TEXT("задохнулся Белой Погодой") : TEXT("раздавлен Розовой Плитой");
		}
		if (const ACritter* C = Cast<ACritter>(Causer))
		{
			return C->Kind == 0 ? TEXT("слизан Слюнявым Танком") : TEXT("пойман Тихой Лапой");
		}
		if (const AAntCharacter* By = Cast<AAntCharacter>(Causer))
		{
			if (By->Creature == 2) { return TEXT("съеден пауком"); }
			if (By->Team == 1) { return TEXT("пал в бою с красными"); }
		}
		if (Ant && !Ant->LastHazard.IsEmpty() && Ant->GetWorld()->GetTimeSeconds() - Ant->LastHazardTime < 4.f)
		{
			return Ant->LastHazard;
		}
		if (Ant && Ant->HasStatus(AntStatus::Drowning)) { return TEXT("утонул"); }
		return TEXT("пал в бою");
	}

	void OnAntKilled(AAntCharacter* Ant, AController* Killer, AActor* Causer)
	{
		if (!Ant) { return; }
		if (Ant->VetId >= 0) { AntVets::OnSquadAntDied(Ant, Causer); }
		if (Ant->Team == 0 && Ant->Creature == 0)
		{
			if (FAntFighterTally* T = TallyOf(Ant)) { T->Deaths++; }
		}
		AAntCharacter* By = Killer ? Cast<AAntCharacter>(Killer->GetPawn()) : Cast<AAntCharacter>(Causer);
		if (!By || By->Team != 0 || By == Ant) { return; }
		if (By->VetId >= 0)
		{
			if (AAntFighterState* FS = AAntFighterState::Get(By->GetWorld()))
			{
				if (FAntVeteran* V = FS->FindVet(By->VetId)) { V->Kills++; V->Xp += 20; }
			}
		}
		FAntFighterTally* T = TallyOf(By);
		if (!T) { return; }
		if (Ant->Creature == 2) { T->KillsSpider++; Feat(By, AntFeat::SpiderSlayer); }
		else if (Ant->Team == 1) { T->KillsRed++; if (T->KillsRed >= 10) { Feat(By, AntFeat::AcidRain); } }
	}

	bool VetDodge(AAntCharacter* Ant, AActor* Causer, float Damage)
	{
		if (!Ant || Ant->VetId < 0 || Ant->bDead) { return false; }
		const AHumanCharacter* H = Cast<AHumanCharacter>(Causer);
		if (H && !H->bSpraying && Ant->Has(AntTrait::SlipperVet) && FMath::FRand() < 0.5f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTVET %s (ветеран тапка) dodged the slipper"), *Ant->GetDisplayName());
			Ant->ApplyLaunch(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.6f).GetSafeNormal() * 500.f * AntKit::SpeedK);
			return true;
		}
		if (Ant->Has(AntTrait::Lucky) && !Ant->bFighterLuckUsed && Damage >= Ant->Health)
		{
			Ant->bFighterLuckUsed = true;
			Ant->Health = 1.f;
			UE_LOG(LogTemp, Display, TEXT("ANTVET %s (счастливчик) survived a fatal blow with 1 hp"), *Ant->GetDisplayName());
			return true;
		}
		return false;
	}

	void RaidSecond(AAntRaid* Raid)
	{
		UWorld* W = Raid ? Raid->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		if (!FS || !Raid->InRaid()) { return; }
		GServerWorld = W;
		// The cake: a second of carrying it is worth 2 XP.
		for (TActorIterator<AKitchenLoot> It(W); It; ++It)
		{
			if (!It->bMain) { continue; }
			for (AAntCharacter* A : It->Carriers)
			{
				if (FAntFighterTally* T = TallyOf(A)) { T->CakeSecs += 1.f; }
			}
		}
		// The Giant chasing somebody while others carry: a distraction (counted once a raid per player, at 6 s of chase).
		for (TActorIterator<AHumanAIController> It(W); It; ++It)
		{
			AAntCharacter* Prey = It->ChaseTarget();
			if (Prey && !Prey->bDead && Prey->Team == 0 && !Prey->Carrying)
			{
				Prey->FighterChased += 1.f;
				if (Prey->FighterChased >= 6.f) { OnDistract(Prey); }
			}
		}
		// The doctrine of every squad (also for ants recruited after the start).
		if (AAntSnowball* S = AAntSnowball::Get(W))
		{
			for (const FAntSnowPlayer& P : S->Players)
			{
				const uint8 D = FS->DoctrineOf(IdOf(P.PS));
				for (AAntCharacter* A : P.Squad)
				{
					if (IsValid(A) && A->VetDoctrine != D) { A->VetDoctrine = D; A->RefreshStats(); }
				}
			}
		}
		// The kitchen flooded while you were in it.
		if (GFlooded)
		{
			GFlooded = false;
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				AAntCharacter* A = It->IsValid() ? Cast<AAntCharacter>((*It)->GetPawn()) : nullptr;
				if (A && !A->bDead && A->Team == 0) { Feat(A, AntFeat::Plumber); }
			}
		}
	}

	void OnRaidStart(AAntRaid* Raid)
	{
		UWorld* W = Raid ? Raid->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		if (!FS) { return; }
		GServerWorld = W;
		FS->Tally.Reset();
		FS->Survivors.Reset();
		FS->FallenThisRaid.Reset();
		FS->DraftCap = (uint8)AntVets::DraftCapFor(W);
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get());
			if (PC && PC->PlayerState)
			{
				FS->Tally.FindOrAdd(PC->PlayerState);
				PC->ClientFighter(ClientKind::Note, 1, 0, FString());
				if (AAntCharacter* A = Cast<AAntCharacter>(PC->GetPawn())) { A->FighterChased = 0.f; A->bFighterWasDrowning = false; }
			}
		}
		AntVets::SpawnDrafted(Raid);
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER raid %d starts: %d players tallied, %d veterans in the roster"), Raid->RaidNo, FS->Tally.Num(), FS->Roster.Num());
	}

	void OnRaidEnding(AAntRaid* Raid, uint8 How)
	{
		UWorld* W = Raid ? Raid->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		AAntSnowball* S = AAntSnowball::Get(W);
		if (!FS) { return; }
		FS->Survivors.Reset();
		if (S)
		{
			for (const FAntSnowPlayer& P : S->Players)
			{
				for (AAntCharacter* A : P.Squad)
				{
					// a veteran's id; a recruit: -1 forager, -2 soldier
					if (IsValid(A) && !A->bDead) { FS->Survivors.Add(TPair<TWeakObjectPtr<APlayerState>, int32>(P.PS, A->VetId >= 0 ? A->VetId : (A->AntClass == EAntClass::Soldier ? -2 : -1))); }
				}
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER raid %d ending (how %d): %d squad ants alive"), Raid->RaidNo, (int32)How, FS->Survivors.Num());
	}

	void BroadcastFallbackSummary(AAntRaid* Raid, uint8 How)
	{
		// The colony branch fills and fires the summary in AAntRaid::BuildResult; until it is merged this one does, with what the
		// fighter tally knows (all food as crumbs). At the merge the colony's version wins and this call goes.
		UWorld* W = Raid ? Raid->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		AAntGameState* GS = W ? W->GetGameState<AAntGameState>() : nullptr;
		if (!FS || !GS) { return; }
		FAntRaidSummary S;
		S.RaidNo = Raid->RaidNo;
		S.Tier = Raid->Tier;
		S.End = How;
		S.bCakeHome = Raid->Result.Cake == AntCake::Home && How != AntRaidEnd::Wiped;
		S.Kept = Raid->Result.Kept;
		S.Jelly = Raid->Result.Jelly;
		S.Larvae = Raid->Result.LarvaeLeft;
		for (APlayerState* P : GS->PlayerArray)
		{
			const AAntPlayerState* A = Cast<AAntPlayerState>(P);
			if (!A || A->bIsHuman || A->IsABot()) { continue; }
			FAntRaidPlayerLine L;
			L.Name = A->GetPlayerName();
			L.Id = IdOf(A);
			const FAntFighterTally* T = FS->Tally.Find(P);
			L.Food[AntFoodKind::Crumbs] = T ? FMath::RoundToInt(T->Food) : 0;
			S.Food[AntFoodKind::Crumbs] += L.Food[AntFoodKind::Crumbs];
			L.Deaths = T ? T->Deaths : 0;
			L.Rescues = T ? T->Rescues : 0;
			const AAntCharacter* Ant = Cast<AAntCharacter>(A->GetPawn());
			L.bCameHome = How != AntRaidEnd::Wiped && Ant && !Ant->bDead && AAntRaid::IsHome(Ant);
			if (T && T->CakeHome) { L.Feats.Add(TEXT("cake_home")); }
			if (L.bCameHome) { L.Feats.Add(TEXT("survived")); }
			S.Players.Add(L);
		}
		AntMetaBus::LastSummary() = S;
		AntMetaBus::OnRaidSummary().Broadcast(S);
	}

	void OnRaidSummary(const FAntRaidSummary& S)
	{
		UWorld* W = GServerWorld.Get();
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		AAntGameState* GS = W ? W->GetGameState<AAntGameState>() : nullptr;
		if (!FS || !GS || W->GetNetMode() == NM_Client) { return; }
		const bool bWiped = S.End == AntRaidEnd::Wiped;
		int32 TeamDeaths = 0;
		for (const FAntRaidPlayerLine& L : S.Players) { TeamDeaths += L.Deaths; }
		TArray<FAntHighlight> HL;
		for (APlayerState* P : GS->PlayerArray)
		{
			AAntPlayerState* PS = Cast<AAntPlayerState>(P);
			if (!PS || PS->bIsHuman || PS->IsABot()) { continue; }
			const FString Id = IdOf(PS);
			const FAntRaidPlayerLine* Line = S.Players.FindByPredicate([&](const FAntRaidPlayerLine& L) { return L.Id == Id || L.Name == PS->GetPlayerName(); });
			FAntFighterTally& T = FS->Tally.FindOrAdd(P);
			AAntPlayerController* PC = Cast<AAntPlayerController>(PS->GetOwner());
			// Feats of the summary (the colony's ids and ours) and of the shift's counters.
			if (Line)
			{
				for (const FName& F : Line->Feats)
				{
					const int32 Fid = AntFeat::FromKey(F.ToString());
					if (Fid >= 0) { FeatPS(PS, (uint8)Fid); }
				}
				if (Line->bCameHome) { FeatPS(PS, AntFeat::Survivor); }
			}
			if (PS->Rams > 0) { FeatPS(PS, AntFeat::Ram); }
			if (PS->LegsLost >= 4 && Line && Line->bCameHome) { FeatPS(PS, AntFeat::Legless); }
			if (PS->RideTime >= 30.f) { FeatPS(PS, AntFeat::Rider); }
			if (PS->Squashed > 0) { FeatPS(PS, AntFeat::Pancake); }
			if (PS->Torn > 0) { FeatPS(PS, AntFeat::Ripper); }
			if (PS->DoorsOpened >= 3) { FeatPS(PS, AntFeat::Doorman); }
			if (S.bCakeHome && TeamDeaths == 0) { FeatPS(PS, AntFeat::CakeFlawless); }
			if (PS->Loadout.Raids == 0) { FeatPS(PS, AntFeat::FirstRaid); }
			// XP.
			struct FXp { int32 N; FString Why; };
			TArray<FXp> Xs;
			const int32 Food = FMath::Max(Line ? Line->FoodTotal() : 0, FMath::RoundToInt(T.Food));
			if (Food > 0) { Xs.Add({ Food, FString::Printf(TEXT("донёс %d еды"), Food) }); }
			if (T.CakeSecs >= 1.f) { Xs.Add({ FMath::RoundToInt(T.CakeSecs * 2.f), FString::Printf(TEXT("тащил торт %d с"), FMath::RoundToInt(T.CakeSecs)) }); }
			const int32 Rescues = FMath::Max(T.Rescues, Line ? Line->Rescues : 0);
			if (Rescues > 0) { Xs.Add({ 60 * Rescues, FString::Printf(TEXT("донёс тела товарищей: %d"), Rescues) }); }
			if (T.Healed >= 5.f) { Xs.Add({ FMath::RoundToInt(T.Healed / 5.f), FString::Printf(TEXT("вылечил %d здоровья"), FMath::RoundToInt(T.Healed)) }); }
			if (T.Distract > 0) { Xs.Add({ 25 * T.Distract, TEXT("отвлёк Гиганта") }); }
			if (T.Fasteners > 0) { Xs.Add({ 30 * T.Fasteners, FString::Printf(TEXT("крепёж и работа руками: %d"), T.Fasteners) }); }
			if (T.Inside > 0) { Xs.Add({ 20 * T.Inside, FString::Printf(TEXT("дела внутри людей: %d"), T.Inside) }); }
			if (T.KillsRed > 0) { Xs.Add({ 15 * T.KillsRed, FString::Printf(TEXT("красных: %d"), T.KillsRed) }); }
			if (T.KillsSpider > 0) { Xs.Add({ 40 * T.KillsSpider, FString::Printf(TEXT("пауков: %d"), T.KillsSpider) }); }
			if (Line && Line->bCameHome) { Xs.Add({ 50, TEXT("вернулся живым") }); }
			Xs.Add({ 20, TEXT("вышел в рейд") });
			for (uint8 F : T.Feats) { Xs.Add({ AntFeat::Def(F).Xp + (T.NewMedals.Contains(F) ? 100 : 0), FString::Printf(TEXT("%s «%s»"), T.NewMedals.Contains(F) ? TEXT("медаль") : TEXT("подвиг"), AntFeat::Def(F).Name) }); }
			// The difficulty's bonus (hard: +50 % of the sum).
			{
				int32 Sum = 0;
				for (const FXp& X : Xs) { Sum += X.N; }
				if (const int32 Bonus = AntDifficulty::BonusXp(PS, Sum)) { Xs.Add({ Bonus, AntDifficulty::XpWhy(PS) }); }
			}
			// (before the RPCs: on a listen host they run at once and the profile pushes its new loadout straight back)
			const int32 XpBefore = PS->Loadout.Xp;
			const int32 RaidsBefore = PS->Loadout.Raids;
			int32 Total = 0;
			FString Best;
			int32 BestN = 0;
			for (const FXp& X : Xs)
			{
				Total += X.N;
				if (X.N > BestN) { BestN = X.N; Best = X.Why; }
				if (PC) { PC->ClientFighter(ClientKind::Xp, X.N, 0, X.Why); }
			}
			if (PC) { PC->ClientFighter(ClientKind::RaidDone, S.RaidNo, Food, FString()); }
			const int32 Before = AntRank::Of(XpBefore);
			PS->Loadout.Xp = FMath::Max(PS->Loadout.Xp, XpBefore + Total);
			PS->Loadout.Raids = FMath::Max(PS->Loadout.Raids, RaidsBefore + 1);
			const int32 After = AntRank::Of(PS->Loadout.Xp);
			PS->Loadout.Rank = (uint8)After;
			UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER xp %s: +%d (%d lines; best: %s) -> %d, rank %s -> %s; medals new %d, feats %d"), *PS->GetPlayerName(), Total, Xs.Num(), *Best,
				PS->Loadout.Xp, AntRank::Name(Before), AntRank::Name(After), T.NewMedals.Num(), T.Feats.Num());
			for (const FXp& X : Xs) { UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER   +%d %s"), X.N, *X.Why); }
			auto Add = [&](const TCHAR* Title, const FString& Text, const FString& Icon, uint8 Kind, int32 RB, int32 RA, int32 Xp)
			{
				FAntHighlight H;
				H.Player = PS->GetPlayerName();
				H.PlayerId = Id;
				H.Title = Title;
				H.Line = Text;
				H.Icon = Icon;
				H.Kind = Kind;
				H.RankBefore = RB;
				H.RankAfter = RA;
				H.Xp = Xp;
				HL.Add(H);
			};
			if (After > Before) { Add(TEXT("НОВОЕ ЗВАНИЕ"), FString::Printf(TEXT("%s теперь %s"), *PS->GetPlayerName(), AntRank::Lower(After)), TEXT("Icons/T_UI_Icon_Emblem"), 0, Before + 1, After + 1, 0); }
			for (uint8 F : T.NewMedals) { Add(TEXT("МЕДАЛЬ"), FString::Printf(TEXT("«%s» — %s"), AntFeat::Def(F).Name, AntFeat::Def(F).Text), FString::Printf(TEXT("Icons/T_UI_Icon_%s"), AntFeat::Def(F).Icon), 1, 0, 0, 0); }
			Add(TEXT("ПОДВИГ"), FString::Printf(TEXT("+%d опыта · %s"), Total, *Best), TEXT("Icons/T_UI_Icon_Mass"), 2, 0, 0, Total);
		}
		// The veterans: promotions, new ones, the fallen.
		AntVets::AfterRaid(FS, S.RaidNo, bWiped);
		for (const FAntHighlight& H : FS->VetHighlights) { HL.Add(H); }
		FS->VetHighlights.Reset();
		FS->Highlights = HL;
		FS->HighlightsRaid = S.RaidNo;
		FS->Tally.Reset();
		FS->ForceNetUpdate();
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER raid %d summary: %d highlights, roster %d, fallen %d"), S.RaidNo, HL.Num(), FS->Roster.Num(), FS->Fallen.Num());
	}

	void Init()
	{
		static bool bDone = false;
		if (bDone) { return; }
		bDone = true;
		AntMetaBus::OnRaidSummary().AddStatic(&AntFighter::OnRaidSummary);
		auto AnyWorld = []() -> UWorld*
		{
			if (!GEngine) { return nullptr; }
			for (const FWorldContext& C : GEngine->GetWorldContexts())
			{
				if ((C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && C.World()) { return C.World(); }
			}
			return nullptr;
		};
		AntMetaBus::SetFallenProvider([AnyWorld](TArray<FAntFallenVeteran>& Out)
		{
			const AAntFighterState* FS = AAntFighterState::Get(AnyWorld());
			if (!FS) { return; }
			for (int32 i = FS->Fallen.Num() - 1; i >= 0; --i)
			{
				const FAntFallen& F = FS->Fallen[i];
				FAntFallenVeteran V;
				V.Name = F.Name;
				V.Rank = AntRank::Lower(F.Rank);
				V.Raid = F.Raid;
				V.Cause = F.Cause;
				Out.Add(V);
			}
		});
		AntMetaBus::SetHighlightsProvider([AnyWorld](TArray<FAntPlayerHighlight>& Out)
		{
			const AAntFighterState* FS = AAntFighterState::Get(AnyWorld());
			if (!FS) { return; }
			for (const FAntHighlight& H : FS->Highlights)
			{
				FAntPlayerHighlight P;
				P.Player = H.Player;
				P.Title = H.Title;
				P.Line = H.Line;
				P.Icon = H.Icon;
				P.RankBefore = H.RankBefore;
				P.RankAfter = H.RankAfter;
				Out.Add(P);
			}
		});
		UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER meta bus: listening to the raid summary, providers set"));
	}
}

// =============================================================================================
// RPCs (AAntPlayerController)
// =============================================================================================

void AAntPlayerController::ServerFighter_Implementation(const FAntLoadout& L)
{
	AntFighter::ServerLoadout(this, L);
}

void AAntPlayerController::ClientFighter_Implementation(uint8 Kind, int32 A, int32 B, const FString& Text)
{
	AntFighter::OnClient(this, Kind, A, B, Text);
}

void AAntPlayerController::ServerVeteran_Implementation(uint8 Cmd, int32 A, int32 B)
{
	AntVets::Command(this, Cmd, A, B);
}

void AAntPlayerController::ServerEmote_Implementation(uint8 Id)
{
	AAntCharacter* Ant = Cast<AAntCharacter>(GetPawn());
	AAntPlayerState* PS = GetPlayerState<AAntPlayerState>();
	if (!Ant || Ant->bDead || !PS) { return; }
	const uint8 E = Id > 0 ? Id : PS->Loadout.CosmAt(AntCosm::Emote);
	if (E == 0 || !AntCosm::Unlocked(AntCosm::Emote, E, PS->Loadout.Rank, PS->Loadout.Medals)) { ClientToast(TEXT("Жест не выбран: профиль бойца (кнопка «БОЕЦ» в гнезде) → Облик → Жест")); return; }
	Ant->Emote = E;
	Ant->EmoteAt = GetWorld()->GetGameState() ? (float)GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	Ant->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER emote %s by %s"), AntCosm::Item(AntCosm::Emote, E).Key, *PS->GetPlayerName());
}

void AAntPlayerController::DoEmote()
{
	ServerEmote(0);
}

// =============================================================================================
// Dev / admin: fighter ..., vet ...
// =============================================================================================

namespace AntFighter
{
	bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		if (A.Num() == 0 || !GM) { return false; }
		const FString Op = A[0].ToLower();
		if (Op != TEXT("fighter") && Op != TEXT("vet")) { return false; }
		UWorld* W = GM->GetWorld();
		GServerWorld = W;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("info"));
		auto Arg = [&A](int32 i) { return A.IsValidIndex(i) ? A[i] : FString(); };
		auto Int = [&A](int32 i, int32 Def = 0) { return A.IsValidIndex(i) ? FCString::Atoi(*A[i]) : Def; };
		// Whose: a player name (part of it), "all", or the host when missing.
		auto Targets = [W](const FString& Who)
		{
			TArray<AAntPlayerController*> Out;
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get());
				if (!PC || !PC->PlayerState) { continue; }
				if (Who.IsEmpty() ? PC->IsLocalController() : (Who == TEXT("all") || PC->PlayerState->GetPlayerName().Contains(Who))) { Out.Add(PC); }
			}
			return Out;
		};
		if (Op == TEXT("vet"))
		{
			return AntVets::Dev(FS, GM, A);
		}
		if (Sub == TEXT("info"))
		{
			for (AAntPlayerController* PC : Targets(TEXT("all")))
			{
				const AAntPlayerState* PS = PC->GetPlayerState<AAntPlayerState>();
				const AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
				UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER info %s at %s (%s): xp %d rank %s, raids %d, perks 0x%08x, medals %d, caste %d, hp %.0f/%.0f, str %.1f, cosm %s"), *PS->GetPlayerName(),
					Ant ? *Ant->GetActorLocation().ToCompactString() : TEXT("-"), *PS->Loadout.ProfileId,
					PS->Loadout.Xp, AntRank::Name(PS->Loadout.Rank), PS->Loadout.Raids, PS->Loadout.Perks, FMath::CountBits(PS->Loadout.Medals), (int32)PS->AntClass, Ant ? Ant->Health : 0.f, Ant ? Ant->MaxHealth : 0.f,
					Ant ? Ant->GetStrength() : 0.f, *FString::JoinBy(PS->Loadout.Cosm, TEXT(","), [](uint8 C) { return FString::FromInt(C); }));
			}
			return true;
		}
		if (Sub == TEXT("mul"))
		{
			// fighter mul [name]: every stat multiplier of that player's ant (to measure a perk)
			for (AAntPlayerController* PC : Targets(Arg(2)))
			{
				const AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn());
				if (!Ant) { continue; }
				static const TCHAR* Names[AntStat::Count] = { TEXT("str"), TEXT("speed"), TEXT("carry"), TEXT("hp"), TEXT("dmgtaken"), TEXT("acid"), TEXT("dash"), TEXT("jump"), TEXT("healrate"), TEXT("healr"), TEXT("inside"), TEXT("held"), TEXT("cloth"), TEXT("climb"), TEXT("water") };
				FString S;
				for (int32 s = 0; s < AntStat::Count; ++s) { S += FString::Printf(TEXT(" %s=%.2f"), Names[s], Mul(Ant, (uint8)s)); }
				const UCharacterMovementComponent* M = Ant->GetCharacterMovement();
				UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER mul %s caste %d:%s | strength %.1f maxhp %.0f walk %.0f jumpz %.0f acidcost %.1f dashcd %.2f"), *PC->PlayerState->GetPlayerName(), (int32)Ant->AntClass, *S,
					Ant->GetStrength(), Ant->MaxHealth, M ? M->GetMaxSpeed() : 0.f, M ? M->JumpZVelocity : 0.f, Ant->AcidCost(), Ant->DashCooldown());
			}
			return true;
		}
		if (Sub == TEXT("xp") || Sub == TEXT("rank") || Sub == TEXT("unlockall") || Sub == TEXT("reset"))
		{
			// fighter xp N [who] | rank R [who] | unlockall [who] | reset [who]
			const bool bNum = Sub == TEXT("xp") || Sub == TEXT("rank");
			for (AAntPlayerController* PC : Targets(Arg(bNum ? 3 : 2)))
			{
				if (Sub == TEXT("xp")) { PC->ClientFighter(ClientKind::Xp, Int(2), 0, TEXT("приказ сверху (админ)")); }
				else if (Sub == TEXT("rank")) { PC->ClientFighter(ClientKind::SetXp, AntRank::XpFor(Int(2)), 0, FString()); }
				else if (Sub == TEXT("unlockall")) { PC->ClientFighter(ClientKind::UnlockAll, 0, 0, FString()); }
				else { PC->ClientFighter(ClientKind::Reset, 0, 0, FString()); }
				UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER admin %s %d -> %s"), *Sub, Int(2), *PC->PlayerState->GetPlayerName());
			}
			return true;
		}
		if (Sub == TEXT("feat"))
		{
			// fighter feat <key|id> [who]: as if it happened now
			const int32 Id = AntFeat::FromKey(Arg(2));
			if (Id < 0) { UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER feat: unknown %s"), *Arg(2)); return true; }
			for (AAntPlayerController* PC : Targets(Arg(3))) { FeatPS(PC->PlayerState, (uint8)Id); }
			return true;
		}
		if (Sub == TEXT("perk"))
		{
			// fighter perk <caste> <tier> <0|1|2> [who]
			for (AAntPlayerController* PC : Targets(Arg(5))) { PC->ClientFighter(ClientKind::Note, 2, Int(2) * 10 + Int(3), FString::FromInt(Int(4))); }
			return true;
		}
		if (Sub == TEXT("cosm"))
		{
			// fighter cosm <slot> <item> [who]
			for (AAntPlayerController* PC : Targets(Arg(4))) { PC->ClientFighter(ClientKind::Note, 3, Int(2), FString::FromInt(Int(3))); }
			return true;
		}
		if (Sub == TEXT("screen"))
		{
			// fighter screen <0 off | 1 profile | 2 roster> [tab] [who]
			for (AAntPlayerController* PC : Targets(Arg(4)))
			{
				if (Int(2) == 0) { PC->ClientFighter(ClientKind::Note, 6, 0, FString()); }
				else { PC->ClientFighter(ClientKind::Note, 4, Int(2), FString::FromInt(Int(3))); }
			}
			return true;
		}
		if (Sub == TEXT("emote"))
		{
			for (AAntPlayerController* PC : Targets(Arg(3))) { PC->ServerEmote_Implementation((uint8)Int(2)); }
			return true;
		}
		if (Sub == TEXT("caste"))
		{
			// fighter caste <0..4> [who]
			for (AAntPlayerController* PC : Targets(Arg(3)))
			{
				if (AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn())) { Ant->SetClass((EAntClass)FMath::Clamp(Int(2), 0, 4), true); }
			}
			return true;
		}
		if (Sub == TEXT("summary"))
		{
			if (AAntRaid* Raid = AAntRaid::Get(W)) { BroadcastFallbackSummary(Raid, (uint8)FMath::Max(1, Int(2, 1))); }
			return true;
		}
		if (Sub == TEXT("stomp"))
		{
			// fighter stomp [hit]: the slipper lands next to the host's ant (or on it)
			for (AAntPlayerController* PC : Targets(Arg(3)))
			{
				if (AAntCharacter* Ant = Cast<AAntCharacter>(PC->GetPawn())) { OnStompNear(Ant, Arg(2) == TEXT("hit")); }
			}
			return true;
		}
		if (Sub == TEXT("hl"))
		{
			for (const FAntHighlight& H : FS->Highlights) { UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER highlight [%s] %s: %s"), *H.Title, *H.Player, *H.Line); }
			for (const FAntFallenVeteran& F : AntMetaBus::GetFallenVeterans()) { UE_LOG(LogTemp, Display, TEXT("ANTFIGHTER wall: %s %s, рейд №%d — %s"), *F.Rank, *F.Name, F.Raid, *F.Cause); }
			return true;
		}
		return false;
	}
}
