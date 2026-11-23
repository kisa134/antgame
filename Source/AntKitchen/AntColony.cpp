#include "AntColony.h"
#include "AntNest.h"
#include "AntRaid.h"
#include "Math/RandomStream.h"

namespace AntColony
{

namespace
{
	//  name                  id           about                                                                                         branch            own          dig     x      y     w     h   shell flip  icon
	const FChamberDef Defs[Count] =
	{
		{ TEXT("Тронный зал"),   TEXT("throne"),   TEXT("Её Сладчайшество Королева-Мать. Зал растёт вместе с колонией: уровень 8 и 20"),            -1,               -1,          -1,    960.f, 388.f, 440.f, 296.f, 0, false, TEXT("Cake") },
		{ TEXT("Кладовые"),      TEXT("stores"),   TEXT("Еда по сортам: сахар, белок, жир, крошки. При кладовых живут носильщики"),                AntUp::Carriers,  -1,          -1,    960.f, 712.f, 420.f, 282.f, 1, false, TEXT("Carriers") },
		{ TEXT("Ясли"),          TEXT("nursery"),  TEXT("Личинки и коконы: жизни всей колонии на рейд. Здесь растят отряды"),                     AntUp::Brood,     -1,          -1,    600.f, 392.f, 340.f, 228.f, 0, true,  TEXT("Brood") },
		{ TEXT("Казарма"),       TEXT("barracks"), TEXT("Койки из спичечных коробков. Сержант Жвалов точит жвала солдатам"),                     AntUp::Soldiers,  -1,          -1,   1320.f, 392.f, 340.f, 228.f, 1, true,  TEXT("Soldier") },
		{ TEXT("Арсенал"),       TEXT("arsenal"),  TEXT("Склад инструментов: приманки, тревожный запах, обманки, шумелки"),                      AntUp::Tools,     -1,          -1,    600.f, 712.f, 340.f, 228.f, 1, false, TEXT("Arsenal") },
		{ TEXT("Грибная ферма"), TEXT("fungus"),   TEXT("Грибница у протекающей трубы: еда растёт сама, пока вы в рейде"),                        -1,               OwnFungus,   Pipe,  262.f, 392.f, 310.f, 208.f, 1, true,  TEXT("Seed") },
		{ TEXT("Трофейная"),     TEXT("trophies"), TEXT("Трофеи рейдов на стенах. Каждый висящий трофей: +2% к еде с рейда"),                    -1,               OwnTrophies, Pipe,  262.f, 716.f, 310.f, 208.f, 1, false, TEXT("Medal") },
		{ TEXT("Мастерская сапёров"), TEXT("workshop"), TEXT("Инженеры: рычаги для дверей, длинные мосты, живые пандусы"),                    AntUp::Engineers, -1,          Wiring, 1660.f, 392.f, 310.f, 208.f, 0, true,  TEXT("Engineers") },
		{ TEXT("Радиоточка"),    TEXT("radio"),    TEXT("«Гнездовое радио Щели»: сводки с фронта, приказы Королевы, летопись"),                  -1,               OwnRadio,    Wiring, 1660.f, 716.f, 310.f, 208.f, 1, false, TEXT("Radio") },
		{ TEXT("Плац"),          TEXT("parade"),   TEXT("Разведчики учатся бегать, нюхать сладкое и ходить тихо. Рядом кошачий лаз"),             AntUp::Scouts,    -1,          Gap,   1460.f, 904.f, 300.f, 190.f, 1, true,  TEXT("Scouts") },
		{ TEXT("Стена памяти"),  TEXT("memorial"), TEXT("Имена павших на коре. Королева помнит каждого"),                                         -1,               OwnMemorial, Gap,   1090.f, 908.f, 286.f, 186.f, 0, true,  TEXT("Stamp") },
	};

	const FDigDef Digs[DigCount] =
	{
		{ TEXT("Трубный ход"),    TEXT("к трубе"),        TEXT("Сырость от трубы: Грибная ферма и Трофейная") },
		{ TEXT("Проводной ход"),  TEXT("к проводке"),     TEXT("Свет и ток от проводки: Мастерская сапёров и Радиоточка") },
		{ TEXT("Подпольный ход"), TEXT("к щели в полу"),  TEXT("Кошачий лаз и разведка: Плац и Стена памяти") },
	};

	struct FOwnTier { const TCHAR* Name; const TCHAR* Text; };
	const FOwnTier OwnTiers[OwnCount][MaxTier] =
	{
		{
			{ TEXT("Грибница"),          TEXT("+20 еды после каждого рейда (грибы растут сами)") },
			{ TEXT("Капельный полив"),   TEXT("+40 еды после каждого рейда") },
			{ TEXT("Плантация"),         TEXT("+60 еды и +1 молочко после каждого рейда") },
		},
		{
			{ TEXT("Гвоздики-булавки"),  TEXT("На стенах висят 4 трофея; каждый +2% к еде с рейда") },
			{ TEXT("Стеллаж из линейки"),TEXT("Висят 8 трофеев") },
			{ TEXT("Музей колонии"),     TEXT("Висят все трофеи; Королева любуется: +2 настроения после рейда") },
		},
		{
			{ TEXT("Рупор из напёрстка"),TEXT("Приказы Королевы платят вдвое больше еды") },
			{ TEXT("Сводки с фронта"),   TEXT("+1 молочко за рейд, в котором выполнен хоть один приказ") },
			{ TEXT("Вечерний эфир"),     TEXT("Королева слушает радио: +3 настроения после каждого рейда") },
		},
		{
			{ TEXT("Имена на коре"),     TEXT("Смерти огорчают Королеву вдвое меньше") },
			{ TEXT("Вечный огонёк"),     TEXT("Если отряд погиб, красные уносят треть добычи, а не половину") },
			{ TEXT("Почётный караул"),   TEXT("После рейда с потерями Королева кладёт +1 личинку") },
		},
	};
	const TCHAR* ThroneNames[MaxTier] = { TEXT("Земляной трон"), TEXT("Трон из спичечного коробка"), TEXT("Трон под крышечкой") };
	const TCHAR* ThroneTexts[MaxTier] =
	{
		TEXT("Королева на куче земли. Зал растёт сам, когда колония растёт"),
		TEXT("Уровень колонии 8: трон из коробка, корона из крышечки"),
		TEXT("Уровень колонии 20: балдахин из крышечки и фольги, корона с бусинами"),
	};
	constexpr int32 ThroneAt2 = 8;
	constexpr int32 ThroneAt3 = 20;
	// New chambers cost a little more than the old branches (120 / 300+2 / 650+5 and a pleased Queen): they come on top of the old economy.
	const int32 OwnCost[MaxTier][3] = { { 150, 0, 0 }, { 400, 3, 0 }, { 800, 6, 60 } };
	const int32 DigCosts[DigCount][2] = { { 120, 0 }, { 300, 1 }, { 500, 3 } };

	const FTrophyDef Trophies_[] =
	{
		{ TEXT("button"),  TEXT("Пуговица"),                    TEXT("первый рейд, из которого вернулись с добычей"), TEXT("Button") },
		{ TEXT("rose"),    TEXT("Кремовая розочка"),            TEXT("первый торт дома"),                             TEXT("Rose") },
		{ TEXT("coin"),    TEXT("Монетка"),                     TEXT("добыча в полтора раза больше запроса Королевы"), TEXT("Coin") },
		{ TEXT("sausage"), TEXT("Колечко колбасы"),             TEXT("150 белка за один рейд"),                       TEXT("Sausage") },
		{ TEXT("slipper"), TEXT("Клок тапка Великана"),         TEXT("торт дома, хотя кто-то погиб"),                 TEXT("Slipper") },
		{ TEXT("plate"),   TEXT("Осколок тарелки"),             TEXT("рейд с добычей на угрозе «Большая уборка» и выше"), TEXT("Plate") },
		{ TEXT("bear"),    TEXT("Золотой мармеладный мишка"),   TEXT("донести мишку целым"),                          TEXT("Bear") },
		{ TEXT("foil"),    TEXT("Фольга от шоколадки"),         TEXT("донести шоколад целым"),                        TEXT("Foil") },
		{ TEXT("cheese"),  TEXT("Корочка сыра"),                TEXT("донести сыр целым"),                            TEXT("Cheese") },
		{ TEXT("thimble"), TEXT("Напёрсток молочка"),           TEXT("особая находка дома"),                          TEXT("Thimble") },
		{ TEXT("earring"), TEXT("Серёжка Бабушки"),             TEXT("два приказа Королевы за один рейд"),            TEXT("Earring") },
		{ TEXT("jaw"),     TEXT("Жвало красного"),              TEXT("отбить торт из красного холма"),                TEXT("Jaw") },
		{ TEXT("candle"),  TEXT("Свечка именинника"),           TEXT("торт дома в «День рождения»"),                  TEXT("Candle") },
	};
	constexpr int32 NumTrophies = UE_ARRAY_COUNT(Trophies_);

	int32 Sum(const TArray<int32>& A) { int32 T = 0; for (int32 V : A) { T += V; } return T; }
}

const FChamberDef& Def(uint8 Chamber) { return Defs[FMath::Min<int32>(Chamber, Count - 1)]; }
const FDigDef& Dig(uint8 D) { return Digs[FMath::Min<int32>(D, DigCount - 1)]; }

int32 Level(const FAntNestState& S)
{
	int32 L = FMath::CountBits((uint64)(S.Dug & 7));
	for (uint8 c = 1; c < Count; ++c) { L += TierOf(S, c); }
	return L;
}

int32 MaxLevel() { return DigCount + (Count - 1) * MaxTier; }

int32 ThroneTier(const FAntNestState& S)
{
	const int32 L = Level(S);
	return L >= ThroneAt3 ? 3 : L >= ThroneAt2 ? 2 : 1;
}

int32 TierOf(const FAntNestState& S, uint8 Chamber)
{
	if (Chamber == Throne) { return ThroneTier(S); }
	const FChamberDef& D = Def(Chamber);
	if (D.Branch >= 0) { return S.Tier_((uint8)D.Branch); }
	if (D.Own >= 0) { return S.Own_((uint8)D.Own); }
	return 0;
}

FString TierName(uint8 Chamber, int32 Tier)
{
	Tier = FMath::Clamp(Tier, 1, MaxTier);
	const FChamberDef& D = Def(Chamber);
	if (Chamber == Throne) { return ThroneNames[Tier - 1]; }
	if (D.Branch >= 0) { return AntUp::Def((uint8)D.Branch, Tier).Name; }
	return OwnTiers[FMath::Clamp<int32>(D.Own, 0, OwnCount - 1)][Tier - 1].Name;
}

FString TierText(uint8 Chamber, int32 Tier)
{
	Tier = FMath::Clamp(Tier, 1, MaxTier);
	const FChamberDef& D = Def(Chamber);
	if (Chamber == Throne) { return ThroneTexts[Tier - 1]; }
	if (D.Branch >= 0) { return AntUp::Def((uint8)D.Branch, Tier).Text; }
	return OwnTiers[FMath::Clamp<int32>(D.Own, 0, OwnCount - 1)][Tier - 1].Text;
}

void Cost(const FAntNestState& S, uint8 Chamber, int32& Food, int32& Jelly, int32& Mood)
{
	Food = Jelly = Mood = 0;
	const FChamberDef& D = Def(Chamber);
	const int32 Next = FMath::Clamp(TierOf(S, Chamber) + 1, 1, MaxTier);
	if (D.Branch >= 0)
	{
		const AntUp::FDef& U = AntUp::Def((uint8)D.Branch, Next);
		Food = U.Food; Jelly = U.Jelly; Mood = U.Mood;
	}
	else if (D.Own >= 0)
	{
		Food = OwnCost[Next - 1][0]; Jelly = OwnCost[Next - 1][1]; Mood = OwnCost[Next - 1][2];
	}
}

bool IsDug(const FAntNestState& S, uint8 D) { return D < DigCount && (S.Dug & (1 << D)) != 0; }

bool IsOpen(const FAntNestState& S, uint8 Chamber)
{
	const FChamberDef& D = Def(Chamber);
	return D.Dig < 0 || IsDug(S, (uint8)D.Dig);
}

uint8 ChamberOfBranch(uint8 Branch)
{
	for (uint8 c = 0; c < Count; ++c) { if (Defs[c].Branch == (int8)Branch) { return c; } }
	return Throne;
}

bool BranchOpen(const FAntNestState& S, uint8 Branch) { return IsOpen(S, ChamberOfBranch(Branch)); }

bool CanUpgrade(const FAntNestState& S, uint8 Chamber, FString* Why)
{
	if (Chamber >= Count) { return false; }
	const FChamberDef& D = Def(Chamber);
	if (Chamber == Throne)
	{
		if (Why) { *Why = TEXT("тронный зал растёт сам вместе с колонией"); }
		return false;
	}
	if (!IsOpen(S, Chamber))
	{
		if (Why) { *Why = FString::Printf(TEXT("сначала прорыть %s (%s)"), Dig((uint8)D.Dig).Name, Dig((uint8)D.Dig).Toward); }
		return false;
	}
	if (D.Branch >= 0) { return AntNest::CanBuy(S, (uint8)D.Branch, Why); }
	const int32 Have = TierOf(S, Chamber);
	if (Have >= MaxTier)
	{
		if (Why) { *Why = TEXT("построено всё"); }
		return false;
	}
	int32 F, J, M;
	Cost(S, Chamber, F, J, M);
	if (S.Mood < M) { if (Why) { *Why = TEXT("нужна довольная Королева"); } return false; }
	if (S.Food < F) { if (Why) { *Why = FString::Printf(TEXT("не хватает еды: %d"), F - S.Food); } return false; }
	if (S.Jelly < J) { if (Why) { *Why = FString::Printf(TEXT("не хватает молочка: %d"), J - S.Jelly); } return false; }
	return true;
}

bool Upgrade(FAntNestState& S, uint8 Chamber)
{
	if (!CanUpgrade(S, Chamber)) { return false; }
	const FChamberDef& D = Def(Chamber);
	if (D.Branch >= 0)
	{
		const bool bOk = AntNest::Buy(S, (uint8)D.Branch);
		Sync(S);
		return bOk;
	}
	int32 F, J, M;
	Cost(S, Chamber, F, J, M);
	if (S.Own.Num() < OwnCount) { S.Own.SetNumZeroed(OwnCount); }
	S.Food -= F;
	S.Jelly -= J;
	S.Own[D.Own]++;
	Sync(S);
	return true;
}

void DigCost(const FAntNestState& S, int32& Food, int32& Jelly)
{
	const int32 N = FMath::Clamp(FMath::CountBits((uint64)(S.Dug & 7)), 0, DigCount - 1);
	Food = DigCosts[N][0];
	Jelly = DigCosts[N][1];
}

bool CanDig(const FAntNestState& S, uint8 D, FString* Why)
{
	if (D >= DigCount) { return false; }
	if (IsDug(S, D)) { if (Why) { *Why = TEXT("уже прорыт"); } return false; }
	int32 F, J;
	DigCost(S, F, J);
	if (S.Food < F) { if (Why) { *Why = FString::Printf(TEXT("не хватает еды: %d"), F - S.Food); } return false; }
	if (S.Jelly < J) { if (Why) { *Why = FString::Printf(TEXT("не хватает молочка: %d"), J - S.Jelly); } return false; }
	return true;
}

bool DoDig(FAntNestState& S, uint8 D)
{
	if (!CanDig(S, D)) { return false; }
	int32 F, J;
	DigCost(S, F, J);
	S.Food -= F;
	S.Jelly -= J;
	S.Dug |= (uint8)(1 << D);
	Sync(S);
	return true;
}

// ---------------------------------------------------------------------------------------------
// The stores by kind
// ---------------------------------------------------------------------------------------------

const TCHAR* KindName(uint8 Kind)
{
	static const TCHAR* N[AntFoodKind::Count] = { TEXT("Сахар"), TEXT("Белок"), TEXT("Жир"), TEXT("Крошки") };
	return N[FMath::Min<int32>(Kind, AntFoodKind::Count - 1)];
}

uint8 KindOf(const FString& LootName)
{
	const FString N = LootName.ToLower();
	auto Has = [&N](std::initializer_list<const TCHAR*> L) { for (const TCHAR* S : L) { if (N.Contains(S)) { return true; } } return false; };
	if (Has({ TEXT("шоколад"), TEXT("сахар"), TEXT("торт"), TEXT("мармелад"), TEXT("мишк"), TEXT("мёд"), TEXT("мед"), TEXT("яблок"), TEXT("зефир"), TEXT("карамел"),
		TEXT("леденец"), TEXT("морожен"), TEXT("пломбир"), TEXT("изюм"), TEXT("конфет"), TEXT("варень"), TEXT("джем"), TEXT("сгущ"), TEXT("батончик"), TEXT("кола"), TEXT("сок"), TEXT("молочк") }))
	{
		return AntFoodKind::Sugar;
	}
	if (Has({ TEXT("сыр"), TEXT("чипс"), TEXT("попкорн"), TEXT("масл"), TEXT("сало"), TEXT("сливк"), TEXT("крем"), TEXT("майонез") }))
	{
		return AntFoodKind::Fat;
	}
	if (Has({ TEXT("корм"), TEXT("семеч"), TEXT("орех"), TEXT("горош"), TEXT("котлет"), TEXT("колбас"), TEXT("сосис"), TEXT("мяс"), TEXT("яйц"), TEXT("ветчин"),
		TEXT("рыб"), TEXT("бутерброд"), TEXT("фасол"), TEXT("муха"), TEXT("личинк") }))
	{
		return AntFoodKind::Protein;
	}
	return AntFoodKind::Crumbs;   // bread, biscuits, pasta, rice, cereal and whatever nobody recognises
}

void Sync(FAntNestState& S)
{
	if (S.Stock.Num() != AntFoodKind::Count) { S.Stock.SetNumZeroed(AntFoodKind::Count); }
	for (int32& V : S.Stock) { V = FMath::Max(0, V); }
	const int32 Have = Sum(S.Stock);
	const int32 Food = FMath::Max(0, S.Food);
	if (Have == Food) { return; }
	if (Food == 0) { for (int32& V : S.Stock) { V = 0; } return; }
	if (Food > Have) { S.Stock[AntFoodKind::Crumbs] += Food - Have; return; }
	// Spent: every kind gives its share.
	int32 Left = Food;
	int32 Big = 0;
	for (int32 k = 0; k < AntFoodKind::Count; ++k)
	{
		S.Stock[k] = (int32)((int64)S.Stock[k] * Food / FMath::Max(1, Have));
		Left -= S.Stock[k];
		if (S.Stock[k] > S.Stock[Big]) { Big = k; }
	}
	S.Stock[Big] += Left;
}

void AddFood(FAntNestState& S, uint8 Kind, int32 Amount)
{
	if (Amount == 0 || Kind >= AntFoodKind::Count) { return; }
	Sync(S);
	S.Food = FMath::Max(0, S.Food + Amount);
	S.Stock[Kind] = FMath::Max(0, S.Stock[Kind] + Amount);
	Sync(S);
}

int32 Held(const FAntNestState& S, uint8 Kind)
{
	if (!S.Stock.IsValidIndex(Kind)) { return Kind == AntFoodKind::Crumbs ? S.Food : 0; }
	const int32 Have = Sum(S.Stock);
	if (Have <= 0) { return Kind == AntFoodKind::Crumbs ? S.Food : 0; }
	return (int32)((int64)S.Stock[Kind] * FMath::Max(0, S.Food) / Have);
}

// ---------------------------------------------------------------------------------------------
// Trophies
// ---------------------------------------------------------------------------------------------

int32 TrophyCount() { return NumTrophies; }
const FTrophyDef& Trophy(int32 Index) { return Trophies_[FMath::Clamp(Index, 0, NumTrophies - 1)]; }
bool HasTrophy(const FAntNestState& S, int32 Index) { return S.Trophies.Contains((uint8)Index); }

bool GiveTrophy(FAntNestState& S, int32 Index)
{
	if (Index < 0 || Index >= NumTrophies || HasTrophy(S, Index)) { return false; }
	S.Trophies.Add((uint8)Index);
	return true;
}

int32 TrophySlots(const FAntNestState& S)
{
	static const int32 Slots[MaxTier + 1] = { 0, 4, 8, NumTrophies };
	return Slots[FMath::Clamp(S.Own_(OwnTrophies), 0, MaxTier)];
}

float LootBonus(const FAntNestState& S) { return 1.f + 0.02f * FMath::Min(S.Trophies.Num(), TrophySlots(S)); }

// ---------------------------------------------------------------------------------------------
// What the chambers do
// ---------------------------------------------------------------------------------------------

int32 FungusFood(const FAntNestState& S) { return 20 * FMath::Clamp(S.Own_(OwnFungus), 0, MaxTier); }
int32 FungusJelly(const FAntNestState& S) { return S.Own_(OwnFungus) >= 3 ? 1 : 0; }
int32 MoodPerRaid(const FAntNestState& S) { return (S.Own_(OwnTrophies) >= 3 ? 2 : 0) + (S.Own_(OwnRadio) >= 3 ? 3 : 0); }
int32 DeathMood(const FAntNestState& S, int32 Deaths) { return S.Own_(OwnMemorial) >= 1 ? FMath::Min(4, Deaths) : FMath::Min(8, Deaths * 2); }
float WipeKeep(const FAntNestState& S) { return S.Own_(OwnMemorial) >= 2 ? 2.f / 3.f : 0.5f; }
int32 OrderFood(const FAntNestState& S, int32 Base) { return S.Own_(OwnRadio) >= 1 ? Base * 2 : Base; }

void Chronicle(FAntNestState& S, const FString& Line)
{
	FString L = Line.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" "));
	S.Chronicle.Add(L);
	while (S.Chronicle.Num() > MaxChronicle) { S.Chronicle.RemoveAt(0); }
}

void AddFallen(FAntNestState& S, const FString& Name, int32 Raid, const FString& Cause)
{
	auto Clean = [](const FString& In) { return In.Replace(TEXT("|"), TEXT("/")).Replace(TEXT("\n"), TEXT(" ")); };
	S.Fallen.Add(FString::Printf(TEXT("%s|%d|%s"), *Clean(Name), Raid, *Clean(Cause)));
	while (S.Fallen.Num() > MaxFallen) { S.Fallen.RemoveAt(0); }
}

TArray<FAntFallenVeteran> Fallen(const FAntNestState& S)
{
	TArray<FAntFallenVeteran> Out = AntMetaBus::GetFallenVeterans();
	if (Out.Num() > 0) { return Out; }
	for (int32 i = S.Fallen.Num() - 1; i >= 0; --i)
	{
		TArray<FString> P;
		S.Fallen[i].ParseIntoArray(P, TEXT("|"), false);
		FAntFallenVeteran V;
		V.Name = P.IsValidIndex(0) ? P[0] : FString();
		V.Raid = P.IsValidIndex(1) ? FCString::Atoi(*P[1]) : 0;
		V.Cause = P.IsValidIndex(2) ? P[2] : FString();
		Out.Add(V);
	}
	return Out;
}

int32 QueenFace(int32 Mood) { return Mood >= 85 ? 3 : Mood >= 50 ? 2 : Mood > 25 ? 1 : 0; }

// ---------------------------------------------------------------------------------------------
// The save
// ---------------------------------------------------------------------------------------------

FString EncodeKeys(const FAntNestState& S)
{
	auto Join = [](auto&& Arr) { FString T; for (int32 i = 0; i < Arr.Num(); ++i) { T += FString::Printf(TEXT("%s%d"), i ? TEXT(",") : TEXT(""), (int32)Arr[i]); } return T; };
	TArray<int32> Stock = S.Stock;
	Stock.SetNumZeroed(AntFoodKind::Count);
	TArray<uint8> Own = S.Own;
	Own.SetNumZeroed(OwnCount);
	FString T = FString::Printf(TEXT("stock=%s\ndug=%d\nown=%s\ntrophies=%s\ngrown=%d\n"), *Join(Stock), (int32)S.Dug, *Join(Own), *Join(S.Trophies), S.Grown);
	for (int32 i = 0; i < S.Chronicle.Num(); ++i) { T += FString::Printf(TEXT("chron%d=%s\n"), i, *S.Chronicle[i]); }
	for (int32 i = 0; i < S.Fallen.Num(); ++i) { T += FString::Printf(TEXT("fallen%d=%s\n"), i, *S.Fallen[i]); }
	return T;
}

void DecodeKeys(const TMap<FString, FString>& KV, int32 Version, FAntNestState& S)
{
	S.Stock.Init(0, AntFoodKind::Count);
	S.Own.Init(0, OwnCount);
	if (Version >= 2)
	{
		auto Ints = [&KV](const TCHAR* K) { TArray<int32> Out; if (const FString* V = KV.Find(K)) { TArray<FString> P; V->ParseIntoArray(P, TEXT(",")); for (const FString& X : P) { Out.Add(FCString::Atoi(*X)); } } return Out; };
		const TArray<int32> Stock = Ints(TEXT("stock"));
		for (int32 k = 0; k < AntFoodKind::Count && k < Stock.Num(); ++k) { S.Stock[k] = FMath::Clamp(Stock[k], 0, 9999999); }
		const TArray<int32> Own = Ints(TEXT("own"));
		for (int32 k = 0; k < OwnCount && k < Own.Num(); ++k) { S.Own[k] = (uint8)FMath::Clamp(Own[k], 0, MaxTier); }
		for (int32 Id : Ints(TEXT("trophies"))) { if (Id >= 0 && Id < NumTrophies) { S.Trophies.AddUnique((uint8)Id); } }
		if (const FString* V = KV.Find(TEXT("dug"))) { S.Dug = (uint8)(FCString::Atoi(**V) & 7); }
		if (const FString* V = KV.Find(TEXT("grown"))) { S.Grown = FMath::Max(0, FCString::Atoi(**V)); }
		for (int32 i = 0; i < MaxChronicle; ++i) { if (const FString* V = KV.Find(FString::Printf(TEXT("chron%d"), i))) { S.Chronicle.Add(*V); } }
		for (int32 i = 0; i < MaxFallen; ++i) { if (const FString* V = KV.Find(FString::Printf(TEXT("fallen%d"), i))) { S.Fallen.Add(*V); } }
	}
	else
	{
		Migrate(S, Version);
	}
	Sync(S);
	S.Version = SaveVersion;
}

void Migrate(FAntNestState& S, int32 FromVersion)
{
	if (FromVersion >= 2) { return; }
	// The stores of a v1 colony were one heap: split it the way a usual haul splits.
	S.Stock.Init(0, AntFoodKind::Count);
	const int32 F = FMath::Max(0, S.Food);
	S.Stock[AntFoodKind::Sugar] = F * 30 / 100;
	S.Stock[AntFoodKind::Protein] = F * 20 / 100;
	S.Stock[AntFoodKind::Fat] = F * 15 / 100;
	S.Stock[AntFoodKind::Crumbs] = F - S.Stock[0] - S.Stock[1] - S.Stock[2];
	S.Own.Init(0, OwnCount);
	// Branches already built keep standing: their tunnels count as dug (free).
	for (uint8 b = 0; b < AntUp::Count; ++b)
	{
		const FChamberDef& D = Def(ChamberOfBranch(b));
		if (S.Tier_(b) > 0 && D.Dig >= 0) { S.Dug |= (uint8)(1 << D.Dig); }
	}
	if (S.Raids > 0)
	{
		Chronicle(S, FString::Printf(TEXT("Рейд %d: колония переехала в новый муравейник в щели. Все постройки на месте"), S.Raids));
	}
	static int32 Said = 0;   // (the main menu's summary reads the save too)
	if (++Said <= 2) UE_LOG(LogTemp, Display, TEXT("ANTCOLONY migrated a v%d colony: food %d split %d/%d/%d/%d, dug %d"), FromVersion, F, S.Stock[0], S.Stock[1], S.Stock[2], S.Stock[3], (int32)S.Dug);
}

FString Info(const FAntNestState& S)
{
	FString T = FString::Printf(TEXT("level %d/%d throne %d, food %d (sugar %d protein %d fat %d crumbs %d), jelly %d, larvae %d, mood %d, dug %d, tiers"),
		Level(S), MaxLevel(), ThroneTier(S), S.Food, Held(S, 0), Held(S, 1), Held(S, 2), Held(S, 3), S.Jelly, S.Larvae, S.Mood, (int32)S.Dug);
	for (uint8 c = 1; c < Count; ++c) { T += FString::Printf(TEXT(" %s=%d"), Def(c).Id, TierOf(S, c)); }
	T += FString::Printf(TEXT(", trophies %d (slots %d, bonus x%.2f), chronicle %d, fallen %d"), S.Trophies.Num(), TrophySlots(S), LootBonus(S), S.Chronicle.Num(), S.Fallen.Num());
	return T;
}

// ---------------------------------------------------------------------------------------------
// A raid on paper (admin / dev / the balance check in docs/COLONY.md)
// ---------------------------------------------------------------------------------------------

void SimulateRaid(FAntNestState& S, FAntRaidResult& R, int32 Seed, bool bBuild)
{
	FRandomStream Rand(Seed);
	Sync(S);
	const int32 RaidNo = S.Raids + 1;
	const AntTier::FDef& T = AntTier::Def(S.Tier);
	R = FAntRaidResult();
	R.bSimulated = true;
	R.RaidNo = RaidNo;
	R.Tier = (uint8)S.Tier;
	R.StockBefore = S.Stock;
	R.JellyBefore = S.Jelly;
	R.MoodBefore = S.Mood;
	R.LarvaeLeft = S.Larvae;
	// How it went: mostly home with the cake; now and then without it; a wipe once the colony is not brand new.
	const float Roll = Rand.FRand();
	const bool bWiped = RaidNo > 2 && Roll < 0.08f;
	const bool bCake = !bWiped && Roll < 0.86f;
	R.End = bWiped ? AntRaidEnd::Wiped : AntRaidEnd::Left;
	R.Cake = bCake ? AntCake::Home : AntCake::Lying;
	R.Deaths = Rand.RandRange(0, bWiped ? 3 : 2);
	R.Ask = T.Ask;
	const float Mul = LootBonus(S);
	int32 Kinds[AntFoodKind::Count] = { 0, 0, 0, 0 };
	if (bCake) { Kinds[AntFoodKind::Sugar] += T.CakeValue; }
	const int32 Side = FMath::RoundToInt(T.Ask * Rand.FRandRange(0.7f, 1.7f) * T.LootMul);
	static const float Share[AntFoodKind::Count] = { 0.25f, 0.22f, 0.15f, 0.38f };
	for (int32 k = 0; k < AntFoodKind::Count; ++k) { Kinds[k] += FMath::RoundToInt(Side * Share[k] * Rand.FRandRange(0.6f, 1.4f) * Mul); }
	int32 Banked = 0;
	for (int32 k = 0; k < AntFoodKind::Count; ++k) { Banked += Kinds[k]; }
	R.Banked = Banked;
	const float Keep = bWiped ? WipeKeep(S) : 1.f;
	R.Kinds.SetNumZeroed(AntFoodKind::Count);
	for (int32 k = 0; k < AntFoodKind::Count; ++k) { R.Kinds[k] = FMath::RoundToInt(Kinds[k] * Keep); R.Kept += R.Kinds[k]; }
	R.ContractsTaken = 2;
	R.ContractsDone = bWiped ? 0 : Rand.RandRange(0, 2);
	R.Jelly = bWiped ? 0 : (bCake ? T.CakeJelly : 0) + R.ContractsDone + (Rand.FRand() < 0.35f ? 2 : 0) + (S.Own_(OwnRadio) >= 2 && R.ContractsDone > 0 ? 1 : 0);
	// The Queen, the same rules as AAntRaid::BuildResult.
	int32 Mood = bCake ? 12 : -4;
	Mood += R.Kept >= R.Ask * 3 / 2 ? 8 : R.Kept >= R.Ask ? 5 : 0;
	Mood += R.ContractsDone * 4 + MoodPerRaid(S) - DeathMood(S, R.Deaths) - (bWiped ? 14 : 0);
	S.Mood = FMath::Clamp(S.Mood + Mood, 0, 100);
	R.MoodAfter = S.Mood;
	for (int32 k = 0; k < AntFoodKind::Count; ++k) { AddFood(S, (uint8)k, R.Kinds[k]); }
	S.Jelly += R.Jelly;
	R.Grown = FungusFood(S);
	R.GrownJelly = FungusJelly(S);
	AddFood(S, AntFoodKind::Protein, R.Grown);
	S.Grown += R.Grown;
	S.Jelly += R.GrownJelly;
	S.Raids = RaidNo;
	S.Wipes += bWiped ? 1 : 0;
	S.BestHaul = FMath::Max(S.BestHaul, R.Kept);
	const int32 Band = AntNest::MoodBand(S.Mood);
	R.LarvaeLaid = FMath::Min(AntNest::MaxLarvae - S.Larvae, (Band >= 3 ? 2 : Band >= 2 ? 1 : 0) + (S.Tier_(AntUp::Brood) >= 3 ? 1 : 0) + (S.Own_(OwnMemorial) >= 3 && R.Deaths > 0 ? 1 : 0));
	S.Larvae = FMath::Clamp(S.Larvae - FMath::Min(R.Deaths, 1) + R.LarvaeLaid, 1, AntNest::MaxLarvae);
	R.LarvaeLeft = S.Larvae - R.LarvaeLaid;
	R.WorkersHatched = Band >= 3 ? 6 : Band >= 2 ? 3 : 1;
	S.Workers = FMath::Min(AntNest::MaxWorkers, S.Workers + R.WorkersHatched);
	if (bCake)
	{
		S.CakesHome++;
		if (S.Tier >= S.MaxTier && S.MaxTier < AntTier::Max && ++S.CakesAtMax >= AntTier::CakesToUnlock(S.MaxTier))
		{
			S.MaxTier++;
			S.CakesAtMax = 0;
			R.NewTier = S.MaxTier;
		}
		S.Tier = S.MaxTier;   // a paper colony always takes the hardest threat it has
	}
	// Trophies, by the same rules as a real raid where the paper can tell.
	auto Try = [&](int32 Id, bool bWhen) { if (bWhen && GiveTrophy(S, Id)) { R.NewTrophies.Add((uint8)Id); } };
	Try(0, !bWiped);
	Try(1, bCake);
	Try(2, R.Kept >= R.Ask * 3 / 2);
	Try(3, R.Kinds[AntFoodKind::Protein] >= 150);
	Try(4, bCake && R.Deaths > 0);
	Try(5, !bWiped && S.Tier >= 3);
	Try(6 + Rand.RandRange(0, 2), !bWiped && Rand.FRand() < 0.3f);
	Try(9, !bWiped && Rand.FRand() < 0.25f);
	Try(10, R.ContractsDone >= 2);
	Try(11, bCake && Rand.FRand() < 0.12f);
	Try(12, bCake && R.Tier >= 4);
	static const TCHAR* Names[] = { TEXT("Грузд"), TEXT("Жвалов"), TEXT("Усик"), TEXT("Тихон") };
	static const TCHAR* Causes[] = { TEXT("раздавлен тапком"), TEXT("утонул в варенье"), TEXT("съеден Бадриком"), TEXT("сражён дихлофосом") };
	for (int32 d = 0; d < R.Deaths; ++d) { AddFallen(S, FString::Printf(TEXT("%s-%d"), Names[Rand.RandRange(0, 3)], Rand.RandRange(2, 99)), RaidNo, Causes[Rand.RandRange(0, 3)]); }
	if (bCake) { R.Heroes.Add(FString::Printf(TEXT("ПОДВИГ|%s донёс торт до входа"), Names[Rand.RandRange(0, 3)])); }
	R.Heroes.Add(FString::Printf(TEXT("ЛУЧШИЙ НОСИЛЬЩИК|%s: +%d еды"), Names[Rand.RandRange(0, 3)], R.Kept / 3));
	R.Chronicle = FString::Printf(TEXT("Рейд %d («%s»): %s +%d еды, +%d молочка. Королева %s"), RaidNo, T.Name,
		bWiped ? TEXT("отряд погиб, красные растащили часть добычи.") : bCake ? TEXT("торт дома!") : TEXT("без торта."), R.Kept, R.Jelly, AntNest::MoodName(S.Mood));
	Chronicle(S, R.Chronicle);
	for (uint8 Id : R.NewTrophies) { Chronicle(S, FString::Printf(TEXT("Рейд %d: в трофейной новый трофей — %s"), RaidNo, Trophy(Id).Name)); }
	if (!bBuild) { return; }
	// The host on paper: digs the next tunnel when a chamber waits behind it, then builds the cheapest thing, again and again.
	const uint8 DigOrder[DigCount] = { (uint8)(Seed % 3), (uint8)((Seed + 1) % 3), (uint8)((Seed + 2) % 3) };
	for (int32 Guard = 0; Guard < 40; ++Guard)
	{
		int32 BestFood = MAX_int32, BestC = -1, BestD = -1;
		for (uint8 c = 1; c < Count; ++c)
		{
			int32 F, J, M;
			Cost(S, c, F, J, M);
			if (CanUpgrade(S, c) && F < BestFood) { BestFood = F; BestC = c; }
		}
		for (uint8 d : DigOrder)
		{
			int32 F, J;
			DigCost(S, F, J);
			if (CanDig(S, d) && F <= BestFood) { BestFood = F; BestD = d; BestC = -1; break; }
		}
		if (BestD >= 0)
		{
			DoDig(S, (uint8)BestD);
			Chronicle(S, FString::Printf(TEXT("Рейд %d: прорыт %s — %s"), RaidNo, Dig((uint8)BestD).Name, Dig((uint8)BestD).Toward));
		}
		else if (BestC >= 0)
		{
			Upgrade(S, (uint8)BestC);
			Chronicle(S, FString::Printf(TEXT("Рейд %d: построено — %s %d «%s»"), RaidNo, Def((uint8)BestC).Name, TierOf(S, (uint8)BestC), *TierName((uint8)BestC, TierOf(S, (uint8)BestC))));
		}
		else
		{
			break;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCOLONY sim raid %d: kept %d jelly %d mood %d -> %s"), RaidNo, R.Kept, R.Jelly, S.Mood, *Info(S));
}

}   // namespace AntColony
