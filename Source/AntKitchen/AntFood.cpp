#include "AntFood.h"
#include "KitchenLoot.h"
#include "AntState.h"
#include "AntReact.h"
#include "AntMess.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "HumanCharacter.h"
#include "CreatureCharacters.h"
#include "KitchenInteractables.h"
#include "KitchenSlick.h"
#include "AntCarry.h"
#include "AntTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"
#include "AntMatter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "CollisionQueryParams.h"

// =============================================================================================
// The table of foods: one line per food the Blender set (Tools/food/food_v2.py) makes.
// Value / weight of the old foods are the game's own (AntTypes.cpp); the new ones are new.
// =============================================================================================

namespace AntFood
{
using K = EAntFoodKind;
static const FFood GTable[] =
{
	// Id           Kind        keys (parts of loot names)                  own name                 val  w   cm    spoil  colour                               pieces new
	{ TEXT("Cake"),       K::Crumbly, TEXT("торт"),                                  TEXT("Кусочек торта"),        140, 36, 9.0f, 14.f, FLinearColor(1.f, 0.55f, 0.70f), 4, false },
	{ TEXT("Sponge"),     K::Crumbly, TEXT("Бисквит|бисквит"),                       TEXT("Кусочек бисквита"),     60,  14, 5.5f, 14.f, FLinearColor(0.98f, 0.80f, 0.45f), 3, true },
	{ TEXT("Cookie"),     K::Crumbly, TEXT("печень|Печень"),                         TEXT("Обломок печенья"),      8,   4,  3.2f, 45.f, FLinearColor(0.70f, 0.45f, 0.22f), 3, false },
	{ TEXT("Bread"),      K::Crumbly, TEXT("Хлеб|хлеб"),                             TEXT("Хлебная крошка"),       5,   3,  2.2f, 30.f, FLinearColor(0.85f, 0.65f, 0.35f), 2, false },
	{ TEXT("GummyBear"),  K::Elastic, TEXT("мишка|Мишка"),                           TEXT("Золотой мармеладный мишка"), 260, 48, 5.0f, 0.f, FLinearColor(1.f, 0.75f, 0.10f), 2, false },
	{ TEXT("Gummy"),      K::Elastic, TEXT("Мармелад|мармелад"),                     TEXT("Мармеладка"),           60,  12, 5.0f, 0.f,  FLinearColor(0.90f, 0.20f, 0.30f), 2, false },   // a gummy worm (the owner's model is one)
	{ TEXT("Marshmallow"),K::Elastic, TEXT("Зефир|зефир"),                           TEXT("Зефирка"),              45,  7,  3.5f, 60.f, FLinearColor(1.f, 0.90f, 0.93f), 2, false },
	{ TEXT("Gum"),        K::Elastic, TEXT("Жвачк|жвачк"),                           TEXT("Жвачка"),               30,  6,  2.0f, 0.f,  FLinearColor(1.f, 0.55f, 0.75f), 0, true },
	{ TEXT("Cheese"),     K::Soft,    TEXT("сыр|Сыр"),                               TEXT("Ломтик сыра"),          45,  22, 7.0f, 25.f, FLinearColor(1.f, 0.80f, 0.25f), 2, false },
	{ TEXT("Sausage"),    K::Soft,    TEXT("Сосиск|сосиск|Колбас|колбас"),           TEXT("Кусочек сосиски"),      50,  18, 6.0f, 15.f, FLinearColor(0.85f, 0.45f, 0.38f), 2, true },
	{ TEXT("Noodle"),     K::Soft,    TEXT("Лапш|лапш"),                             TEXT("Лапшинка"),             12,  4,  8.0f, 20.f, FLinearColor(0.98f, 0.88f, 0.55f), 2, true },
	{ TEXT("Pancake"),    K::Soft,    TEXT("Блин|блин"),                             TEXT("Кусочек блина"),        50,  16, 7.0f, 20.f, FLinearColor(0.95f, 0.75f, 0.40f), 2, true },
	{ TEXT("Sugar"),      K::Hard,    TEXT("сахар|Сахар"),                           TEXT("Кубик сахара"),         25,  9,  1.6f, 0.f,  FLinearColor(1.f, 1.f, 1.f), 3, false },
	{ TEXT("Nut"),        K::Hard,    TEXT("Орех|орех"),                             TEXT("Орех в шоколаде"),      70,  15, 2.2f, 0.f,  FLinearColor(0.30f, 0.15f, 0.08f), 2, false },
	{ TEXT("Candy"),      K::Hard,    TEXT("Карамел|карамел|Леденец|леденец"),       TEXT("Карамелька"),           120, 18, 2.5f, 0.f,  FLinearColor(0.90f, 0.10f, 0.10f), 3, false },
	{ TEXT("Bone"),       K::Hard,    TEXT("Кост|кост"),                             TEXT("Куриная косточка"),     40,  20, 7.0f, 30.f, FLinearColor(0.92f, 0.86f, 0.74f), 2, true },
	{ TEXT("Grape"),      K::Rolling, TEXT("Виноград|виноград"),                     TEXT("Виноградина"),          40,  10, 2.2f, 18.f, FLinearColor(0.45f, 0.65f, 0.20f), 2, true },
	{ TEXT("Pea"),        K::Rolling, TEXT("Горошин|горошин"),                       TEXT("Горошина"),             6,   3,  0.9f, 30.f, FLinearColor(0.35f, 0.70f, 0.25f), 2, false },
	{ TEXT("Olive"),      K::Rolling, TEXT("Маслин|маслин|Оливк|оливк"),             TEXT("Маслина"),              30,  7,  2.2f, 40.f, FLinearColor(0.12f, 0.10f, 0.10f), 2, true },
	{ TEXT("Honey"),      K::Sticky,  TEXT("мёд|Мёд"),                               TEXT("Капля мёда"),           50,  10, 2.4f, 0.f,  FLinearColor(1.f, 0.55f, 0.05f), 0, false },
	{ TEXT("Jam"),        K::Sticky,  TEXT("Варень|варень|Джем|джем"),               TEXT("Капля варенья"),        45,  9,  2.4f, 60.f, FLinearColor(0.70f, 0.05f, 0.12f), 0, true },
	{ TEXT("Condensed"),  K::Sticky,  TEXT("сгущ|Сгущ"),                             TEXT("Капля сгущёнки"),       55,  10, 2.4f, 50.f, FLinearColor(0.98f, 0.94f, 0.82f), 0, false },
	{ TEXT("IceCream"),   K::Melting, TEXT("морожен|пломбир|Морожен"),               TEXT("Кусок мороженого"),     200, 40, 7.0f, 0.f,  FLinearColor(1.f, 0.85f, 0.90f), 3, false },
	{ TEXT("Butter"),     K::Melting, TEXT("Масл|масл"),                             TEXT("Кусочек масла"),        40,  12, 3.0f, 40.f, FLinearColor(1.f, 0.90f, 0.55f), 2, true },
	{ TEXT("Chocolate"),  K::Melting, TEXT("шоколад|Шоколад"),                       TEXT("Долька шоколада"),      180, 30, 5.0f, 0.f,  FLinearColor(0.25f, 0.12f, 0.05f), 4, false },
	{ TEXT("Ice"),        K::Melting, TEXT("Лёд|лёд|льда|Льд"),                      TEXT("Кубик льда"),           10,  12, 2.6f, 0.f,  FLinearColor(0.80f, 0.90f, 1.f), 3, true },
	{ TEXT("Crisp"),      K::Brittle, TEXT("Чипс|чипс"),                             TEXT("Чипсина"),              40,  6,  4.5f, 0.f,  FLinearColor(1.f, 0.85f, 0.30f), 2, false },
	{ TEXT("Cracker"),    K::Brittle, TEXT("Крекер|крекер"),                         TEXT("Крекер"),               35,  6,  4.0f, 0.f,  FLinearColor(0.90f, 0.70f, 0.40f), 2, true },
	{ TEXT("Wafer"),      K::Brittle, TEXT("Вафл|вафл"),                             TEXT("Кусочек вафли"),        45,  7,  4.5f, 0.f,  FLinearColor(0.95f, 0.80f, 0.50f), 2, true },
	{ TEXT("Eggshell"),   K::Brittle, TEXT("скорлуп|Скорлуп"),                       TEXT("Скорлупка"),            15,  3,  3.0f, 30.f, FLinearColor(0.95f, 0.92f, 0.85f), 2, true },
	{ TEXT("Apple"),      K::Juicy,   TEXT("яблок|Яблок"),                           TEXT("Кусочек яблока"),       55,  16, 6.0f, 12.f, FLinearColor(0.85f, 0.95f, 0.60f), 2, false },
	{ TEXT("Watermelon"), K::Juicy,   TEXT("Арбуз|арбуз"),                           TEXT("Кусочек арбуза"),       90,  26, 8.0f, 10.f, FLinearColor(0.95f, 0.25f, 0.30f), 2, true },
	{ TEXT("Tomato"),     K::Juicy,   TEXT("Помидор|помидор|Томат|томат"),           TEXT("Долька помидора"),      40,  12, 4.0f, 10.f, FLinearColor(0.90f, 0.15f, 0.10f), 2, true },
};

/** Foods without a model of their own: just the kind. */
static const TCHAR* const GKindKeys[][2] =
{
	{ TEXT("корм"), TEXT("Hard") }, { TEXT("емечк"), TEXT("Hard") }, { TEXT("исин"), TEXT("Hard") },
	{ TEXT("опкорн"), TEXT("Brittle") }, { TEXT("лопь"), TEXT("Brittle") }, { TEXT("акарон"), TEXT("Brittle") },
	{ TEXT("зюм"), TEXT("Rolling") }, { TEXT("отлет"), TEXT("Soft") }, { TEXT("бутерброд"), TEXT("Crumbly") }, { TEXT("орков"), TEXT("Juicy") },
};

static const TCHAR* const GKindNames[] = { TEXT("None"), TEXT("Crumbly"), TEXT("Elastic"), TEXT("Soft"), TEXT("Hard"), TEXT("Rolling"), TEXT("Sticky"), TEXT("Melting"), TEXT("Brittle"), TEXT("Juicy") };
static const TCHAR* const GKindRu[] = { TEXT("-"), TEXT("крошится"), TEXT("тянется"), TEXT("мягкое"), TEXT("твёрдое"), TEXT("катится"), TEXT("липкое"), TEXT("тает"), TEXT("хрупкое"), TEXT("сочное") };

TConstArrayView<FFood> Table() { return MakeArrayView(GTable, UE_ARRAY_COUNT(GTable)); }

static bool KeysMatch(const FString& Name, const TCHAR* Keys)
{
	TArray<FString> Parts;
	FString(Keys).ParseIntoArray(Parts, TEXT("|"));
	for (const FString& P : Parts)
	{
		if (Name.Contains(P, ESearchCase::CaseSensitive)) { return true; }
	}
	return false;
}

const FFood* Find(const FString& LootName)
{
	if (LootName.Contains(TEXT("Пачка")) || LootName.Contains(TEXT("Коробоч")) || LootName.Contains(TEXT("Банка")))
	{
		return nullptr;   // a whole packet is a box, not the food inside
	}
	for (const FFood& F : GTable)
	{
		if (KeysMatch(LootName, F.Keys)) { return &F; }
	}
	return nullptr;
}

const FFood* ById(const FString& Id)
{
	for (const FFood& F : GTable)
	{
		if (Id.Equals(F.Id, ESearchCase::IgnoreCase)) { return &F; }
	}
	return nullptr;
}

const TCHAR* KindName(EAntFoodKind Kd) { return GKindNames[FMath::Clamp((int32)Kd, 0, (int32)UE_ARRAY_COUNT(GKindNames) - 1)]; }

static EAntFoodKind KindByName(const FString& S)
{
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(GKindNames); ++i)
	{
		if (S.Equals(GKindNames[i], ESearchCase::IgnoreCase)) { return (EAntFoodKind)i; }
	}
	return EAntFoodKind::None;
}

EAntFoodKind KindOf(const FString& LootName)
{
	if (const FFood* F = Find(LootName)) { return F->Kind; }
	for (const auto& KK : GKindKeys)
	{
		if (LootName.Contains(KK[0], ESearchCase::CaseSensitive)) { return KindByName(KK[1]); }
	}
	return EAntFoodKind::None;
}

bool V2()
{
	static int32 S = -1;
	if (S < 0)
	{
		// (FParse::Param matches whole words: -AntFoodV2 is the older spelling of the same flag)
		S = FParse::Param(FCommandLine::Get(), TEXT("AntFoodV2Models")) || FParse::Param(FCommandLine::Get(), TEXT("AntFoodV2")) ? 1 : 0;
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD models: %s"), S ? TEXT("old + the new foods (-AntFoodV2Models)") : TEXT("old only (default: no new food)"));
	}
	return S == 1;
}

bool Kinds()
{
	static int32 S = -1;
	if (S < 0) { int32 V = 1; FParse::Value(FCommandLine::Get(), TEXT("AntFoodKinds="), V); S = V != 0 ? 1 : 0; }
	return S == 1;
}

// =============================================================================================
// Models (-AntFoodV2): /Game/FoodV2/SM_FoodV2_<Id>/SM_FoodV2_<Id>[_P<k>], MI_FoodV2_<Id>, MI_FoodV2_<Id>_Inner
// =============================================================================================

struct FV2Asset
{
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Mat = nullptr;
	UMaterialInterface* Inner = nullptr;
};

static const FV2Asset& LoadV2(const FString& Id, int32 Piece)
{
	static TMap<FString, FV2Asset> Cache;
	const FString Key = FString::Printf(TEXT("%s#%d"), *Id, Piece);
	if (const FV2Asset* F = Cache.Find(Key)) { return *F; }
	FV2Asset A;
	const FString Dir = FString::Printf(TEXT("/Game/FoodV2/SM_FoodV2_%s"), *Id);
	const FString MeshName = Piece >= 0 ? FString::Printf(TEXT("SM_FoodV2_%s_P%d"), *Id, Piece) : FString::Printf(TEXT("SM_FoodV2_%s"), *Id);
	A.Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Dir, *MeshName, *MeshName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	A.Mat = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/MI_FoodV2_%s.MI_FoodV2_%s"), *Dir, *Id, *Id), nullptr, LOAD_NoWarn | LOAD_Quiet);
	// the cut faces read the inner strip by height in the piece's own space: every piece has its own instance
	if (Piece >= 0)
	{
		A.Inner = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/MI_FoodV2_%s_Inner_P%d.MI_FoodV2_%s_Inner_P%d"), *Dir, *Id, Piece, *Id, Piece), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!A.Inner)
	{
		A.Inner = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/MI_FoodV2_%s_Inner.MI_FoodV2_%s_Inner"), *Dir, *Id, *Id), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	// (the cache outlives the map: root them, like AntArt does)
	if (A.Mesh) { A.Mesh->AddToRoot(); }
	if (A.Mat) { A.Mat->AddToRoot(); }
	if (A.Inner) { A.Inner->AddToRoot(); }
	if (!A.Mesh && Piece < 0) { UE_LOG(LogTemp, Warning, TEXT("ANTFOOD V2 model missing: %s"), *MeshName); }
	return Cache.Add(Key, A);
}

static const FFood* FoodOf(const AKitchenLoot* L)
{
	if (!L) { return nullptr; }
	if (L->FoodNet.Food < UE_ARRAY_COUNT(GTable)) { return &GTable[L->FoodNet.Food]; }
	return Find(L->LootName);
}

UStaticMesh* ModelV2(const AKitchenLoot* L, int32 Piece, UMaterialInterface** OutMat, UMaterialInterface** OutInner)
{
	if (!V2() || !L || L->bIsGear || L->bBody || L->bVault) { return nullptr; }
	const FFood* F = FoodOf(L);
	// an old food keeps its old model (the owner's decision 26.11): the new models only for the foods new to the game
	if (!F || !F->bNew || (Piece >= 0 && Piece >= F->Pieces)) { return nullptr; }
	// a pre-cut piece only for a break into exactly the model's pieces (other breaks: the cut of AntShards)
	if (Piece >= 0 && !(L->ShardPath.Num() == 2 && L->ShardPath[1] == F->Pieces)) { return nullptr; }
	const FV2Asset& A = LoadV2(F->Id, Piece);
	if (!A.Mesh) { return nullptr; }
	if (OutMat) { *OutMat = A.Mat; }
	if (OutInner) { *OutInner = A.Inner ? A.Inner : A.Mat; }
	return A.Mesh;
}

int32 BreakPieces(const AKitchenLoot* L)
{
	const FFood* F = FoodOf(L);
	if (Kinds() && L && (EAntFoodKind)L->FoodNet.Kind == EAntFoodKind::Brittle) { return 2; }   // snaps in two
	if (V2() && F && F->Pieces > 0 && L->ShardPath.Num() == 0 && ModelV2(L, -1) && LoadV2(F->Id, 0).Mesh) { return F->Pieces; }
	return 0;
}

float BreakValueMul(const AKitchenLoot* L)
{
	// brittle halves add up to less than the whole (Shatter gives ordinary pieces 1.1 of the value in total)
	return (Kinds() && L && (EAntFoodKind)L->FoodNet.Kind == EAntFoodKind::Brittle) ? 0.8f / 1.1f : 1.f;
}

static void DefOf(const FFood& F, FAntFoodDef& Out)
{
	Out.Name = F.Ru;
	Out.Value = F.Value;
	Out.Weight = F.Weight;
	Out.Color = F.Color;
	Out.Shape = F.Kind == EAntFoodKind::Rolling || F.Kind == EAntFoodKind::Sticky ? EAntShape::Sphere : EAntShape::Cube;
	Out.Scale = FVector(0.3f);
}

bool RollNew(int32 Tier, FRandomStream& R, FAntFoodDef& Out)
{
	if (!V2() || R.FRand() > 0.4f) { return false; }
	TArray<const FFood*, TInlineAllocator<16>> Pick;
	for (const FFood& F : GTable)
	{
		const int32 T = F.Value < 15 ? 0 : F.Value < 50 ? 1 : F.Value < 150 ? 2 : 3;
		if (F.bNew && T == FMath::Clamp(Tier, 0, 3)) { Pick.Add(&F); }
	}
	if (Pick.Num() == 0) { return false; }
	DefOf(*Pick[R.RandRange(0, Pick.Num() - 1)], Out);
	return true;
}

bool TestDef(const FString& What, FAntFoodDef& Out)
{
	if (const FFood* F = ById(What)) { DefOf(*F, Out); return true; }
	const EAntFoodKind Kd = KindByName(What);
	if (Kd != EAntFoodKind::None)
	{
		for (const FFood& F : GTable) { if (F.Kind == Kd) { DefOf(F, Out); return true; } }
	}
	return false;
}
} // namespace AntFood

// =============================================================================================
// Server state of one piece of food (not replicated: what clients need is in FAntFoodNet)
// =============================================================================================

struct FAntFoodServer
{
	float Mass = 1.f;            // what is left of it (crumbs, juice, melt), 0..1
	float Soak = 0.f, Toast = 0.f, Burn = 0.f, Frozen = 0.f, Dirt = 0.f, Spoil = 0.f, Melt = 0.f, Bitten = 0.f, Squeeze = 0.f;
	float ToastWarmUntil = -1.f; // toasted is worth more while it is still warm
	float Temp = 22.f;
	float SlowT = 0.f;           // states are checked a few times a second
	FVector LastVel = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	FVector FloorN = FVector::UpVector;
	bool bOnFloor = false;
	float CrumbDebt = 0.f;       // mass knocked off since the last crumb that fell as a real piece
	float NextCrumb = 0.f;
	float NextFx = 0.f;
	float NextSquirt = 0.f;
	float NextSnag = 0.f, SnagUntil = -1.f;
	float NextGlue = 0.f;
	float NextTrail = 0.f;
	float TrailDist = 0.f;
	float NextFly = 0.f;
	int32 Flies = 0;
	float BlockedT = 0.f;
	float Tension = 0.f;         // elastic: how hard the carriers pull it apart (0..1)
	FVector2D SlingPull = FVector2D::ZeroVector;
	float PuddleDebt = 0.f;
	float NextSlipCheck = 0.f;
	FVector BaseExtent = FVector::ZeroVector;
	float AppliedSqueeze = 0.f;
	float AccS = 0.f;            // smoothed acceleration (shaking)
	float PendingLoss = 0.f;     // crumbs not yet knocked off
	bool bSoakBroke = false, bRoachSaid = false, bSqueezeSaid = false;
	float LastDamp = -1.f;
	// carry log: one carry from the first grab to the last release
	bool bCarry = false;
	float CarryStart = 0.f, CarryDist = 0.f, CarryMass0 = 1.f, CarryMaxTension = 0.f, CarryMaxSpeed = 0.f;
	int32 CarryCrumbs = 0, CarrySquirts = 0, CarrySnags = 0;
	float NextLog = 0.f;
	bool bLog = false;
};

/** AKitchenLoot's private parts that the food needs (friend of AKitchenLoot). */
struct FAntFoodAccess
{
	static UBoxComponent* Body(const AKitchenLoot* L) { return L->Body; }
	static UStaticMeshComponent* Mesh(const AKitchenLoot* L) { return L->Mesh; }
	static void ShatterSoon(AKitchenLoot* L, const FVector& At) { if (!L->bShatterPending) { L->bShatterPending = true; L->ShatterAt = At; } }
	static TSharedPtr<FAntFoodServer>& Srv(AKitchenLoot* L) { return L->FoodSrv; }
	static float BreakableAfter(const AKitchenLoot* L) { return L->BreakableAfter; }
	static float LastKick(const AKitchenLoot* L) { return L->LastKickTime; }
	static void ResizeBody(AKitchenLoot* L, const FVector& E) { L->Body->SetBoxExtent(E, true); }
	static AKitchenLoot* SpawnCrumb(AKitchenLoot* L, const FString& Name, int32 V, const FVector& Where, const FVector& Fling)
	{
		AKitchenLoot* C = L->SpawnPiece(Name, V, 1, Where, Fling);
		if (C) { C->FoodNet.Soak = L->FoodNet.Soak; C->FoodNet.Spoil = L->FoodNet.Spoil; C->FoodNet.Toast = L->FoodNet.Toast; C->FoodNet.Burn = L->FoodNet.Burn; }
		return C;
	}
};

namespace AntFood
{
static inline uint8 B(float V) { return (uint8)FMath::Clamp(FMath::RoundToInt(V * 255.f), 0, 255); }
static inline float F(uint8 V) { return V / 255.f; }
static inline EAntFoodKind KindNow(const AKitchenLoot* L) { return L ? (EAntFoodKind)L->FoodNet.Kind : EAntFoodKind::None; }

static FAntFoodServer& Srv(AKitchenLoot* L)
{
	TSharedPtr<FAntFoodServer>& P = FAntFoodAccess::Srv(L);
	if (!P.IsValid())
	{
		P = MakeShared<FAntFoodServer>();
		P->LastPos = L->GetActorLocation();
		const FAntFoodNet& N = L->FoodNet;   // a piece of something carries on its states
		P->Soak = F(N.Soak); P->Toast = F(N.Toast); P->Burn = F(N.Burn); P->Frozen = F(N.Frozen); P->Dirt = F(N.Dirt); P->Spoil = F(N.Spoil);
		P->NextFly = L->GetWorld() ? L->GetWorld()->GetTimeSeconds() + 20.f : 0.f;
	}
	return *P;
}

/** A sticky patch on the floor that holds the ants of the other side (honey trail, jam, juice). Server. */
struct FGluePatch { TWeakObjectPtr<UWorld> W; FVector P; float R; float Until; uint8 Team; TWeakObjectPtr<AKitchenLoot> Src; };
static TArray<FGluePatch> GPatches;
static TArray<TWeakObjectPtr<AActor>> GFlies;   // flies the food drew (at most 3 at a time)
static bool GLogAll = false;

static bool Soakable(EAntFoodKind Kd) { return Kd == EAntFoodKind::Crumbly || Kd == EAntFoodKind::Soft || Kd == EAntFoodKind::Brittle || Kd == EAntFoodKind::Elastic; }
static float MeltSeconds(const FFood* Fd)
{
	if (!Fd) { return 300.f; }
	const FString Id(Fd->Id);
	return Id == TEXT("IceCream") ? 180.f : Id == TEXT("Ice") ? 150.f : Id == TEXT("Butter") ? 420.f : 600.f;
}
static uint8 PuddleKind(const FFood* Fd)
{
	const FString Id(Fd ? Fd->Id : TEXT(""));
	if (Id == TEXT("IceCream") || Id == TEXT("Condensed")) { return MessKind::Cream; }
	if (Id == TEXT("Ice")) { return MessKind::Water; }
	if (Id == TEXT("Butter")) { return MessKind::Oil; }
	if (Id == TEXT("Chocolate")) { return MessKind::Chocolate; }
	if (Id == TEXT("Honey")) { return MessKind::Honey; }
	if (Id == TEXT("Jam")) { return MessKind::Jam; }
	return MessKind::Juice;
}

static float TotalMass(const FAntFoodServer& S) { return FMath::Clamp(S.Mass * (1.f - 0.85f * S.Melt), 0.02f, 1.f); }

int32 ValueNow(const AKitchenLoot* L)
{
	if (!L) { return 0; }
	const FAntFoodNet& N = L->FoodNet;
	const float Base = N.BaseValue > 0 ? (float)N.BaseValue : (float)L->Value;
	const float Toast = F(N.Toast), Burn = F(N.Burn), Spoil = F(N.Spoil), Dirt = F(N.Dirt), Soak = F(N.Soak);
	const TSharedPtr<FAntFoodServer>& P = const_cast<AKitchenLoot*>(L)->FoodSrv;
	const float Mass = P.IsValid() ? TotalMass(*P) : 1.f;
	const bool bWarm = P.IsValid() && L->GetWorld() && P->ToastWarmUntil > L->GetWorld()->GetTimeSeconds();
	float V = Base * Mass;
	V *= 1.f + (bWarm ? 0.35f * Toast * (1.f - Burn) : 0.f);   // fresh from the toaster: worth more, briefly
	V *= 1.f - 0.65f * Burn;
	V *= 1.f - 0.6f * Spoil;
	V *= 1.f - 0.25f * Dirt;
	V *= Soak > 0.5f ? 0.85f : 1.f;
	return FMath::Max(1, FMath::RoundToInt(V));
}

float ScaleV2(const AKitchenLoot* L)
{
	const FFood* Fd = FoodOf(L);
	if (!Fd) { return 1.f; }
	// the model is at its real size (imported at 1 m = 1900 uu) for the table's weight; lighter (a crumb of it, bitten,
	// melted, crumbled) is smaller by the cube root; a pre-cut piece is its own size for weight / pieces
	float RefW = (float)Fd->Weight;
	if (L->ShardPath.Num() == 2 && Fd->Pieces > 0) { RefW = (float)Fd->Weight / Fd->Pieces; }
	const float Soak = Soakable((EAntFoodKind)L->FoodNet.Kind) ? F(L->FoodNet.Soak) : 0.f;
	const float W = FMath::Max(1.f, (float)L->Weight / (1.f + 0.4f * Soak));
	return FMath::Clamp(FMath::Pow(W / FMath::Max(1.f, RefW), 1.f / 3.f), 0.15f, 2.5f);
}

void Init(AKitchenLoot* L)
{
	if (!L || L->bIsGear) { return; }
	const FFood* Fd = Find(L->LootName);
	L->FoodNet.Kind = (uint8)(L->bBody || L->bVault ? EAntFoodKind::None : KindOf(L->LootName));
	L->FoodNet.Food = Fd ? (uint8)(Fd - GTable) : 255;
	if (L->FoodNet.BaseWeight == 0) { L->FoodNet.BaseWeight = (uint16)FMath::Clamp(L->Weight, 1, 60000); }
	if (L->FoodNet.BaseValue == 0) { L->FoodNet.BaseValue = (uint16)FMath::Clamp(L->Value, 1, 60000); }
	RegisterEvents();
}

static UPhysicalMaterial* MakePM(const TCHAR* Name, float Fr, float St, float Rest)
{
	UPhysicalMaterial* M = NewObject<UPhysicalMaterial>(GetTransientPackage(), Name);
	M->Friction = Fr;
	M->StaticFriction = St;
	M->Restitution = Rest;
	M->RestitutionCombineMode = EFrictionCombineMode::Max;
	M->bOverrideRestitutionCombineMode = Rest > 0.3f;
	M->AddToRoot();
	return M;
}

UPhysicalMaterial* PhysMat(const AKitchenLoot* L, float& LinDamp, float& AngDamp)
{
	if (!Kinds() || !L) { return nullptr; }
	static UPhysicalMaterial* PM[(int32)EAntFoodKind::Count] = {};
	if (!PM[1])
	{
		PM[(int32)EAntFoodKind::Crumbly] = MakePM(TEXT("PM_Food_Crumbly"), 0.55f, 0.6f, 0.08f);
		PM[(int32)EAntFoodKind::Elastic] = MakePM(TEXT("PM_Food_Elastic"), 0.7f, 0.8f, 0.82f);   // bounces
		PM[(int32)EAntFoodKind::Soft] = MakePM(TEXT("PM_Food_Soft"), 0.85f, 0.95f, 0.02f);       // drags
		PM[(int32)EAntFoodKind::Hard] = MakePM(TEXT("PM_Food_Hard"), 0.32f, 0.38f, 0.12f);
		PM[(int32)EAntFoodKind::Rolling] = MakePM(TEXT("PM_Food_Rolling"), 0.06f, 0.08f, 0.35f); // a ball bearing
		PM[(int32)EAntFoodKind::Sticky] = MakePM(TEXT("PM_Food_Sticky"), 1.2f, 1.4f, 0.f);
		PM[(int32)EAntFoodKind::Melting] = MakePM(TEXT("PM_Food_Melting"), 0.25f, 0.3f, 0.05f);  // slippery as it melts
		PM[(int32)EAntFoodKind::Brittle] = MakePM(TEXT("PM_Food_Brittle"), 0.45f, 0.5f, 0.2f);
		PM[(int32)EAntFoodKind::Juicy] = MakePM(TEXT("PM_Food_Juicy"), 0.6f, 0.65f, 0.1f);
	}
	const EAntFoodKind Kd = KindNow(L);
	switch (Kd)
	{
	case EAntFoodKind::Hard: LinDamp = 0.05f; AngDamp = 0.6f; break;        // inertia: once it goes, it keeps going
	case EAntFoodKind::Rolling: LinDamp = 0.02f; AngDamp = 0.05f; break;
	case EAntFoodKind::Elastic: LinDamp = 0.15f; AngDamp = 0.8f; break;
	case EAntFoodKind::Sticky: LinDamp = 1.2f; AngDamp = 3.f; break;
	case EAntFoodKind::Soft: LinDamp = 0.6f; AngDamp = 2.5f; break;
	default: break;
	}
	return Kd == EAntFoodKind::None ? nullptr : PM[(int32)Kd];
}

float LeashMul(const AKitchenLoot* L)
{
	return (Kinds() && KindNow(L) == EAntFoodKind::Sticky) ? 3.5f : 1.f;   // it glues the jaws: a yank does not tear it out
}

/** The weight and the value follow what is left of it (and the water in it). Server. */
static void ApplyMass(AKitchenLoot* L, FAntFoodServer& S)
{
	const FAntFoodNet& N = L->FoodNet;
	const float Soak = Soakable(KindNow(L)) ? S.Soak : 0.f;
	const int32 W = FMath::Max(1, FMath::RoundToInt(N.BaseWeight * TotalMass(S) * (1.f + 0.4f * Soak)));   // soaked: heavier
	const int32 V = ValueNow(L);
	if (W != L->Weight)
	{
		L->Weight = W;
		L->Value = V;
		L->OnRep_Visual();   // smaller model, smaller body, new mass (RefreshVisual -> SetupPhysics)
		if (UBoxComponent* Bd = FAntFoodAccess::Body(L)) { S.BaseExtent = Bd->GetUnscaledBoxExtent(); S.AppliedSqueeze = 0.f; }
	}
	else if (V != L->Value)
	{
		L->Value = V;
	}
}

static FVector FloorBelow(AKitchenLoot* L, float Radius)
{
	const UBoxComponent* Bd = FAntFoodAccess::Body(L);
	return L->GetActorLocation() - FVector(0.f, 0.f, Bd ? (float)Bd->GetScaledBoxExtent().Z : Radius * 0.5f);
}

static void Glue(UWorld* W, const FVector& P, float R, float Life, uint8 Team, AKitchenLoot* Src = nullptr)
{
	if (GPatches.Num() > 160) { GPatches.RemoveAt(0, 40); }
	GPatches.Add({ W, P, R, (float)(W->GetTimeSeconds() + Life), Team, Src });
}

static void SpawnFoodFly(AKitchenLoot* L, const TCHAR* Why)
{
	UWorld* W = L->GetWorld();
	GFlies.RemoveAll([](const TWeakObjectPtr<AActor>& F) { return !F.IsValid(); });
	if (GFlies.Num() >= 3) { return; }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector At = L->GetActorLocation() + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 600.f);
	AFlyCharacter* Fly = W->SpawnActor<AFlyCharacter>(AFlyCharacter::StaticClass(), At, FRotator::ZeroRotator, P);
	if (!Fly) { return; }
	Fly->Team = 2;
	if (AFlyAIController* C = W->SpawnActor<AFlyAIController>(AFlyAIController::StaticClass(), FTransform::Identity, P)) { C->Possess(Fly); }
	GFlies.Add(Fly);
	UE_LOG(LogTemp, Display, TEXT("ANTFOOD fly drawn to «%s» (%s)"), *L->LootName, Why);
}

/** Crumbs knocked off a crumbly piece: mass and value go, crumbs fall (some as real crumbs: a trail). Server. */
static void LoseCrumbs(AKitchenLoot* L, FAntFoodServer& S, float Amount, const TCHAR* Why)
{
	if (Amount <= 0.f || S.Mass <= 0.25f) { return; }
	Amount = FMath::Min(Amount, S.Mass - 0.25f);   // the core of it holds together
	S.Mass -= Amount;
	S.CrumbDebt += Amount;
	UWorld* W = L->GetWorld();
	const float Now = W->GetTimeSeconds();
	const float Unit = 1.f / FMath::Max(1, (int32)L->FoodNet.BaseWeight);   // one weight unit of the whole
	if (S.CrumbDebt >= Unit && Now >= S.NextCrumb && S.CarryCrumbs < 14)
	{
		// a crumb you can pick up: rivals follow the trail
		S.CrumbDebt -= Unit;
		S.NextCrumb = Now + 0.6f;
		++S.CarryCrumbs;
		const FVector Back = -L->GetVelocity().GetSafeNormal2D();
		const FVector Where = L->GetActorLocation() + Back * L->GetRadius() * 0.8f + FVector(0.f, 0.f, 6.f);
		const int32 V = FMath::Max(1, FMath::RoundToInt(L->FoodNet.BaseValue * Unit * 0.5f));
		const FString Name = FString::Printf(TEXT("Крошка: %s"), *L->LootName.Replace(TEXT("Крошка: "), TEXT("")));
		FAntFoodAccess::SpawnCrumb(L, Name, V, Where, Back * 60.f + FVector(0.f, 0.f, 60.f));
	}
	if (Now >= S.NextFx)
	{
		S.NextFx = Now + 0.25f;
		if (AAntGameState* GS = W->GetGameState<AAntGameState>())
		{
			GS->MulticastCrumbs(L->GetActorLocation(), L->Color, (uint8)FMath::Clamp(FMath::RoundToInt(Amount * 400.f), 2, 10), FMath::Clamp(L->GetRadius() * 0.08f, 1.5f, 6.f));
		}
	}
	if (S.bLog || GLogAll) { UE_LOG(LogTemp, Display, TEXT("ANTFOOD crumbs «%s» -%.1f%% (%s): mass %.1f%%"), *L->LootName, Amount * 100.f, Why, TotalMass(S) * 100.f); }
	ApplyMass(L, S);
}

static void Squirt(AKitchenLoot* L, FAntFoodServer& S, const TCHAR* Why)
{
	UWorld* W = L->GetWorld();
	const float Now = W->GetTimeSeconds();
	if (Now < S.NextSquirt || S.Mass <= 0.3f) { return; }
	S.NextSquirt = Now + 1.6f;
	S.Mass -= 0.035f;
	++S.CarrySquirts;
	const FVector Side = FMath::VRand().GetSafeNormal2D();
	const FVector At = L->GetActorLocation() + Side * L->GetRadius() * 1.2f;
	AntMess::StainAt(L, MessKind::Juice, FloorBelow(L, L->GetRadius()) + Side * L->GetRadius(), FVector::UpVector, FMath::Max(30.f, L->GetRadius() * 1.4f), FMath::RandRange(1, 9999));
	Glue(W, FloorBelow(L, L->GetRadius()) + Side * L->GetRadius(), FMath::Max(22.f, L->GetRadius() * 0.7f), 35.f, L->CarrierTeam);
	if (AAntGameState* GS = W->GetGameState<AAntGameState>()) { GS->MulticastCrumbs(At, L->Color, 6, 2.5f); }
	if (FMath::FRand() < 0.3f) { SpawnFoodFly(L, TEXT("juice")); }   // the juice is bait
	UE_LOG(LogTemp, Display, TEXT("ANTFOOD squirt «%s» (%s): mass %.0f%%"), *L->LootName, Why, TotalMass(S) * 100.f);
	ApplyMass(L, S);
}

static void Snap(AKitchenLoot* L, const TCHAR* Why)
{
	if (L->Weight < 4) { return; }
	UE_LOG(LogTemp, Display, TEXT("ANTFOOD snap «%s» in two (%s)"), *L->LootName, Why);
	FAntFoodAccess::ShatterSoon(L, L->GetActorLocation());
}

static void BreakFragileAround(AKitchenLoot* L, float Speed)
{
	// a heavy hard thing dropped: what is fragile where it lands breaks, and the floor shows the blow
	UWorld* W = L->GetWorld();
	const float R = L->GetRadius() * 2.2f + 20.f;
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* O = *It;
		if (O == L || O->bIsGear || O->IsActorBeingDestroyed()) { continue; }
		if (FVector::Dist(O->GetActorLocation(), L->GetActorLocation()) > R + O->GetRadius()) { continue; }
		const EAntFoodKind Ok = KindNow(O);
		if (Ok == EAntFoodKind::Brittle || Ok == EAntFoodKind::Crumbly || O->FoodNet.Frozen > 128)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» dropped at %.0f breaks «%s»"), *L->LootName, Speed, *O->LootName);
			FAntFoodAccess::ShatterSoon(O, O->GetActorLocation());
		}
		else if (Ok == EAntFoodKind::Juicy && O->FoodSrv.IsValid())
		{
			Squirt(O, *O->FoodSrv, TEXT("crushed"));
		}
	}
	AntState::SetAround(W, L->GetActorLocation() - FVector(0.f, 0.f, L->GetRadius()), L->GetRadius() * 1.5f, AntState::Damage, FMath::Clamp(Speed / 2500.f, 0.2f, 0.7f));
}

/** Carriers pulling it apart: 0..1, and their mean pull (world X, Y; -1..1 of their strength). Every machine (replicated grips). */
static float GripTension(const AKitchenLoot* L, FVector2D& Mean)
{
	const TArray<FAntCarryGrip>& G = L->CarryNet.Grips;
	Mean = FVector2D::ZeroVector;
	if (G.Num() == 0) { return 0.f; }
	const FTransform TM = L->GetActorTransform();
	const FVector C = TM.GetLocation();
	float T = 0.f;
	for (const FAntCarryGrip& Gp : G)
	{
		const FVector2D Pull(Gp.PullX / 127.f, Gp.PullY / 127.f);
		Mean += Pull;
		const FVector R = TM.TransformPosition((FVector)Gp.Local) - C;
		const FVector2D Rd = FVector2D(R.X, R.Y).GetSafeNormal();
		T += FMath::Max(0.f, (float)FVector2D::DotProduct(Pull, Rd));
	}
	Mean /= (float)G.Num();
	return G.Num() < 2 ? 0.f : FMath::Clamp(T / 1.2f, 0.f, 1.f);
}

static bool InSinkWater(AKitchenLoot* L)
{
	static TWeakObjectPtr<AKitchenFaucet> Faucet;
	if (!Faucet.IsValid() || Faucet->GetWorld() != L->GetWorld())
	{
		Faucet = nullptr;
		for (TActorIterator<AKitchenFaucet> It(L->GetWorld()); It; ++It) { Faucet = *It; break; }
	}
	if (!Faucet.IsValid()) { return false; }
	const FVector P = L->GetActorLocation();
	const FBox Bx = AKitchenFaucet::Basin();
	return P.X > Bx.Min.X && P.X < Bx.Max.X && P.Y > Bx.Min.Y && P.Y < Bx.Max.Y && P.Z < Faucet->WaterLevel + 2.f && P.Z > Bx.Min.Z - 50.f;
}

static void CarryEnd(AKitchenLoot* L, FAntFoodServer& S, const TCHAR* How)
{
	if (!S.bCarry) { return; }
	S.bCarry = false;
	const float Now = L->GetWorld()->GetTimeSeconds();
	const float M1 = TotalMass(S);
	UE_LOG(LogTemp, Display, TEXT("ANTFOOD carry %s «%s» [%s]: %.1f s, %.0f uu (%.2f m real), mass %.1f%% -> %.1f%% (lost %.1f%%), weight %d, value %d, crumbs dropped %d, max speed %.0f, max stretch %.2f, squirts %d, snags %d, %s"),
		How, *L->LootName, KindName(KindNow(L)), Now - S.CarryStart, S.CarryDist, S.CarryDist / 1900.f, S.CarryMass0 * 100.f, M1 * 100.f, (S.CarryMass0 - M1) * 100.f,
		L->Weight, L->Value, S.CarryCrumbs, S.CarryMaxSpeed, S.CarryMaxTension, S.CarrySquirts, S.CarrySnags, *Describe(L));
}

static void SlowTick(AKitchenLoot* L, FAntFoodServer& S, float T, EAntFoodKind Kd0, bool bCarried, bool bDragged, const FVector& V)
{
	UWorld* W = L->GetWorld();
	const float Now = W->GetTimeSeconds();
	const FFood* Fd = FoodOf(L);
	const FVector P = L->GetActorLocation();
	S.Temp = AntReact::HeatAt(W, P);
	const bool bFrozen = S.Frozen > 0.5f;
	UStaticMeshComponent* Mesh = FAntFoodAccess::Mesh(L);
	// the matter system's own temperature of this piece (a flame, a hot pan, a heat gun next to it: KAN-21)
	if (AntMatter::Enabled() && Mesh)
	{
		if (const AAntMatterNet* Mn = AAntMatterNet::Get(W)) { S.Temp = FMath::Max(S.Temp, Mn->BodyTemp(Mesh)); }
	}

	// ---- water: soaked through (heavier; crumbly and brittle things fall apart)
	if (Soakable(Kd0))
	{
		// (wet from outside: the matter system or a dev command - not the food's own channel, which only shows this soak)
		const float WetOut = Mesh ? FMath::Max(AntState::GetSource(Mesh, AntState::Matter, AntState::Wet), AntState::GetSource(Mesh, AntState::Dev, AntState::Wet)) : 0.f;
		const bool bWater = InSinkWater(L) || (WetOut > 0.6f && S.Soak < 0.6f);
		S.Soak = FMath::Clamp(S.Soak + (bWater ? T / 12.f : -T / 300.f), 0.f, 1.f);
		if (S.Soak >= 0.85f && !S.bSoakBroke && !bFrozen && (Kd0 == EAntFoodKind::Crumbly || Kd0 == EAntFoodKind::Brittle) && L->Weight >= 6)
		{
			S.bSoakBroke = true;
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» soaked through: falls apart"), *L->LootName);
			FAntFoodAccess::ShatterSoon(L, P);
		}
	}
	// ---- heat: toasted (worth more while warm), then burnt
	if (S.Temp >= 60.f && !bFrozen && Kd0 != EAntFoodKind::Melting && Kd0 != EAntFoodKind::Sticky)
	{
		if (S.Temp >= 130.f) { S.Burn += T / 8.f; S.Toast += T / 6.f; }
		else { S.Toast += T / 12.f; if (S.Toast >= 1.f) { S.Burn += T / 30.f; } }
		S.ToastWarmUntil = Now + 60.f;
	}
	S.Toast = FMath::Clamp(S.Toast, 0.f, 1.f);
	S.Burn = FMath::Clamp(S.Burn, 0.f, 1.f);
	// ---- cold: frozen hard (no crumbling, no spoiling, no melting); thaws slowly in the room
	S.Frozen = FMath::Clamp(S.Frozen + (S.Temp <= -5.f ? T / 25.f : S.Temp > 2.f ? -T / 120.f : 0.f), 0.f, 1.f);
	// ---- melting: the heat eats it; it leaves a puddle; in the cold it holds
	if (Kd0 == EAntFoodKind::Melting && !bFrozen && S.Temp > 8.f)
	{
		const float Rate = T / MeltSeconds(Fd) * FMath::Max(0.25f, 1.f + (S.Temp - 22.f) / 12.f);
		S.Melt = FMath::Min(1.f, S.Melt + Rate);
		S.PuddleDebt += Rate;
		if (S.PuddleDebt >= 0.07f)
		{
			S.PuddleDebt = 0.f;
			const uint8 PK = PuddleKind(Fd);
			AntMess::StainAt(L, PK, FloorBelow(L, L->GetRadius()), FVector::UpVector, FMath::Max(30.f, L->GetRadius() * 1.6f), FMath::RandRange(1, 9999));
			if (PK == MessKind::Water || PK == MessKind::Oil)
			{
				AKitchenSlick::Spill(W, FloorBelow(L, L->GetRadius()), PK == MessKind::Oil ? SlickKind::Oil : SlickKind::Water, L->LastCarrier.Get(), FMath::Max(60.f, L->GetRadius() * 2.f), 40.f);
			}
			else
			{
				Glue(W, FloorBelow(L, L->GetRadius()), FMath::Max(20.f, L->GetRadius() * 0.8f), 30.f, 255);
			}
		}
		if (S.Melt >= 0.98f)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» melted away (%.0f °C)"), *L->LootName, S.Temp);
			AntMess::StainAt(L, PuddleKind(Fd), FloorBelow(L, L->GetRadius()), FVector::UpVector, FMath::Max(40.f, L->GetRadius() * 2.4f), FMath::RandRange(1, 9999));
			CarryEnd(L, S, TEXT("melted"));
			L->ReleaseAll();
			L->Destroy();
			return;
		}
	}
	// ---- spoiling: slow in the fridge, fast in the heat, never frozen; flies come, then (would) cockroaches
	if (Fd && Fd->SpoilMin > 0.f && !bFrozen)
	{
		const float Tf = S.Temp <= -5.f ? 0.f : S.Temp <= 8.f ? 0.15f : S.Temp >= 30.f ? 2.f : 1.f;
		S.Spoil = FMath::Clamp(S.Spoil + T / (Fd->SpoilMin * 60.f) * Tf, 0.f, 1.f);
		if (S.Spoil > 0.5f && Now >= S.NextFly && S.Flies < 2)
		{
			S.NextFly = Now + FMath::FRandRange(25.f, 40.f);
			++S.Flies;
			SpawnFoodFly(L, TEXT("spoiled"));
		}
		if (S.Spoil > 0.85f && !S.bRoachSaid)
		{
			S.bRoachSaid = true;
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» rotten: would draw a cockroach (no cockroach in the game yet)"), *L->LootName);
		}
	}
	// ---- dirt: dragged over the floor, rolled through the dust
	if (bDragged) { S.Dirt += T / 90.f; }
	if (Kd0 == EAntFoodKind::Rolling && !bCarried) { S.Dirt += T * (float)V.Size2D() / 40000.f; }
	S.Dirt = FMath::Clamp(S.Dirt, 0.f, 1.f);

	// ---- the floor under it (hard things slide down slopes, round ones roll)
	if (Kd0 == EAntFoodKind::Hard || Kd0 == EAntFoodKind::Rolling || bFrozen)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AntFoodFloor), false, L);
		const float Down = L->GetRadius() + 40.f;
		S.bOnFloor = W->LineTraceSingleByChannel(Hit, P, P - FVector(0.f, 0.f, Down), ECC_WorldStatic, Q);
		S.FloorN = S.bOnFloor ? (FVector)Hit.ImpactNormal : FVector::UpVector;
	}
	// ---- a grape under the Human's foot
	if (Kd0 == EAntFoodKind::Rolling && !bCarried && !L->bHeld)
	{
		for (TActorIterator<AHumanCharacter> It(W); It; ++It)
		{
			AHumanCharacter* H = *It;
			if (!H || H->GetVelocity().Size2D() < 80.f) { continue; }
			for (int32 Leg = 0; Leg < 2; ++Leg)
			{
				const FVector Sole = H->GetSoleLocation(Leg);
				if (FVector::Dist2D(Sole, P) < 60.f + L->GetRadius() && FMath::Abs(Sole.Z - P.Z) < 120.f)
				{
					const bool bFell = H->TrySlip(P, 0.55f, false, L->LastCarrier.Get());
					L->Kick(H->GetVelocity().GetSafeNormal2D() * 900.f + FVector(0.f, 0.f, 150.f), P);
					UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» under the Human's foot: %s"), *L->LootName, bFell ? TEXT("he slips and falls") : TEXT("it shoots away"));
					break;
				}
			}
		}
	}
	// ---- sticky: whatever touches it gets stuck (the patch moves with it)
	if (Kd0 == EAntFoodKind::Sticky && !bFrozen)
	{
		Glue(W, P, L->GetRadius() + 10.f, T + 0.1f, bCarried ? L->CarrierTeam : (uint8)255, L);
	}

	// ---- to the clients
	FAntFoodNet& N = L->FoodNet;
	N.Soak = B(S.Soak); N.Toast = B(S.Toast); N.Burn = B(S.Burn); N.Frozen = B(S.Frozen); N.Dirt = B(S.Dirt); N.Spoil = B(S.Spoil); N.Melt = B(S.Melt);
	N.Bitten = B(S.Bitten); N.Squeeze = B(S.Squeeze);
	ApplyMass(L, S);
	if ((S.bLog || (GLogAll && bCarried)) && Now >= S.NextLog)   // (all of them: only the ones being carried, or the log drowns)
	{
		S.NextLog = Now + 0.5f;
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD state t=%.1f «%s» [%s] carriers %d speed %.0f mass %.1f%% w %d v %d stretch %.2f squeeze %.2f temp %.0f %s"),
			Now, *L->LootName, KindName(Kd0), L->CarryNet.Grips.Num(), V.Size2D(), TotalMass(S) * 100.f, L->Weight, L->Value, S.Tension, S.Squeeze, S.Temp, *Describe(L));
	}
}

static void SetDamp(UBoxComponent* Bd, FAntFoodServer& S, float D)
{
	if (Bd && !FMath::IsNearlyEqual(S.LastDamp, D, 0.01f)) { Bd->SetLinearDamping(D); S.LastDamp = D; }
}

void ServerTick(AKitchenLoot* L, float Dt)
{
	if (!Kinds() || !L || L->bIsGear || L->bBody || L->bVault || Dt <= 0.f || L->IsActorBeingDestroyed()) { return; }
	const EAntFoodKind Kd0 = KindNow(L);
	if (Kd0 == EAntFoodKind::None) { return; }
	FAntFoodServer& S = Srv(L);
	UWorld* W = L->GetWorld();
	const float Now = W->GetTimeSeconds();
	UBoxComponent* Bd = FAntFoodAccess::Body(L);
	const bool bSim = Bd && Bd->IsSimulatingPhysics();
	const bool bFrozen = S.Frozen > 0.5f;
	const bool bGivesWay = Kd0 == EAntFoodKind::Crumbly || Kd0 == EAntFoodKind::Soft || Kd0 == EAntFoodKind::Elastic || Kd0 == EAntFoodKind::Juicy || Kd0 == EAntFoodKind::Melting;
	const EAntFoodKind Kd = (bFrozen && bGivesWay) ? EAntFoodKind::Hard : Kd0;   // frozen: hard as a stone
	const FVector P = L->GetActorLocation();
	const FVector V = bSim ? Bd->GetPhysicsLinearVelocity() : (P - S.LastPos) / Dt;
	const float Acc = (float)(V - S.LastVel).Size() / Dt;
	S.AccS = FMath::Lerp(S.AccS, FMath::Min(Acc, 20000.f), 1.f - FMath::Exp(-Dt * 8.f));
	const float Moved = FMath::Min(100.f, (float)FVector::Dist(P, S.LastPos));
	S.LastVel = V;
	S.LastPos = P;
	const int32 NGrip = L->CarryNet.Grips.Num();
	const bool bCarried = NGrip > 0 || L->IsCarried();
	const bool bDragged = bCarried && L->CarryRatio() < 1.f && !L->bHeld;
	const float Speed = (float)V.Size2D();

	// one carry, for the log: from the first grab to the last release
	if (bCarried)
	{
		if (!S.bCarry)
		{
			S.bCarry = true; S.CarryStart = Now; S.CarryDist = 0.f; S.CarryMass0 = TotalMass(S); S.CarryMaxTension = 0.f; S.CarryMaxSpeed = 0.f;
			S.CarryCrumbs = 0; S.CarrySquirts = 0; S.CarrySnags = 0;
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD carry start «%s» [%s] weight %d value %d mass %.1f%%"), *L->LootName, KindName(Kd0), L->Weight, L->Value, S.CarryMass0 * 100.f);
		}
		S.CarryDist += Moved;
		S.CarryMaxSpeed = FMath::Max(S.CarryMaxSpeed, Speed);
	}

	S.SlowT += Dt;
	if (S.SlowT >= 0.25f)
	{
		const float T = S.SlowT;
		S.SlowT = 0.f;
		SlowTick(L, S, T, Kd0, bCarried, bDragged, V);
		if (L->IsActorBeingDestroyed()) { return; }
	}

	switch (Kd)
	{
	case EAntFoodKind::Crumbly:
		if (bCarried || Speed > 40.f)
		{
			// carry it carefully: running and shaking knock crumbs off it, scraping along the floor too
			const float Run = FMath::Max(0.f, Speed - 85.f) / 150.f;
			const float Shake = FMath::Max(0.f, S.AccS - 900.f) / 900.f;
			S.PendingLoss += Dt * (0.0055f * Run * Run + 0.012f * Shake + (bDragged ? 0.0025f : 0.f)) * (1.f + 0.8f * S.Soak);
			if (S.PendingLoss >= 0.004f) { const float A = S.PendingLoss; S.PendingLoss = 0.f; LoseCrumbs(L, S, A, Run > Shake ? TEXT("running") : TEXT("shaking")); }
		}
		break;
	case EAntFoodKind::Elastic:
	{
		FVector2D Mean;
		const float Tn = GripTension(L, Mean);
		S.Tension = FMath::Lerp(S.Tension, Tn, 1.f - FMath::Exp(-Dt * 10.f));
		S.SlingPull = Mean;
		S.CarryMaxTension = FMath::Max(S.CarryMaxTension, S.Tension);
		// squeezing through: they pull, it does not move -> it goes thin and long (a smaller body slips through the gap)
		const bool bStuck = bCarried && Mean.Size() > 0.35f && Speed < 30.f;
		S.BlockedT = bStuck ? S.BlockedT + Dt : FMath::Max(0.f, S.BlockedT - Dt * 2.f);
		const float Want = S.BlockedT > 0.35f ? 1.f : 0.f;
		S.Squeeze = FMath::FInterpConstantTo(S.Squeeze, Want, Dt, 1.6f);
		if (Bd && FMath::Abs(S.Squeeze - S.AppliedSqueeze) > 0.05f)
		{
			if (S.BaseExtent.IsNearlyZero()) { S.BaseExtent = Bd->GetUnscaledBoxExtent(); }
			FVector Dl = L->GetActorTransform().InverseTransformVectorNoScale(FVector(Mean.X, Mean.Y, 0.f)).GetSafeNormal();
			FVector E = S.BaseExtent;
			for (int32 i = 0; i < 3; ++i) { E[i] *= 1.f - 0.45f * S.Squeeze * (1.f - FMath::Abs((float)Dl[i])); }
			FAntFoodAccess::ResizeBody(L, E.ComponentMax(FVector(3.f)));
			S.AppliedSqueeze = S.Squeeze;
			if (S.Squeeze > 0.5f && !S.bSqueezeSaid) { S.bSqueezeSaid = true; UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» squeezes through (body %.0f%% thinner)"), *L->LootName, 45.f * S.Squeeze); }
			if (S.Squeeze < 0.1f) { S.bSqueezeSaid = false; }
		}
		break;
	}
	case EAntFoodKind::Soft:
		// it drags and snags: now and then it catches on an edge and holds the carriers back for a moment
		if (bCarried && Speed > 40.f && Now >= S.NextSnag)
		{
			S.NextSnag = Now + FMath::FRandRange(2.5f, 6.f);
			if (bDragged || FMath::FRand() < 0.35f)
			{
				S.SnagUntil = Now + 0.35f;
				++S.CarrySnags;
				if (S.bLog || GLogAll) { UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» snags"), *L->LootName); }
			}
		}
		SetDamp(Bd, S, Now < S.SnagUntil ? 12.f : (bCarried ? 0.9f : 0.6f));
		break;
	case EAntFoodKind::Hard:
	case EAntFoodKind::Rolling:
		if (bSim && !bCarried && !L->bHeld && S.bOnFloor)
		{
			// down the slope: round things roll on the slightest tilt, hard ones slide once it is steep enough
			const FVector G(0.f, 0.f, W->GetGravityZ());
			const FVector Gt = G - FVector::DotProduct(G, S.FloorN) * S.FloorN;
			const float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)S.FloorN.Z, -1.f, 1.f)));
			const float K = Kd == EAntFoodKind::Rolling ? 0.9f : (SlopeDeg > 6.f ? 0.55f : 0.f);
			if (K > 0.f && SlopeDeg > 1.5f) { Bd->AddForce(Gt * Bd->GetMass() * K); }
		}
		SetDamp(Bd, S, Kd == EAntFoodKind::Rolling ? 0.02f : 0.05f);
		break;
	case EAntFoodKind::Sticky:
		SetDamp(Bd, S, bCarried ? 2.2f : 1.2f);   // slow
		if (bCarried)
		{
			S.TrailDist += Moved;
			if (S.TrailDist >= FMath::Max(40.f, L->GetRadius() * 1.2f))
			{
				S.TrailDist = 0.f;
				const FVector Fl = FloorBelow(L, L->GetRadius());
				AntMess::StainAt(L, PuddleKind(FoodOf(L)), Fl, FVector::UpVector, FMath::Max(24.f, L->GetRadius() * 0.9f), FMath::RandRange(1, 9999));
				Glue(W, Fl, FMath::Max(16.f, L->GetRadius() * 0.55f), 45.f, L->CarrierTeam);
			}
		}
		break;
	case EAntFoodKind::Juicy:
		if (bCarried)
		{
			bool bHard = NGrip >= 3;
			for (const FAntCarryGrip& G : L->CarryNet.Grips) { bHard |= G.Strain > 140; }
			if (bHard) { Squirt(L, S, NGrip >= 3 ? TEXT("squeezed by many") : TEXT("squeezed hard")); }
		}
		break;
	default:
		break;
	}
	if (S.bCarry && !bCarried) { CarryEnd(L, S, TEXT("end")); }
}

void OnImpact(AKitchenLoot* L, float Speed, const FVector& At, AActor* Other)
{
	if (!Kinds() || !L || L->bIsGear || L->bBody || Speed < 150.f) { return; }
	FAntFoodServer& S = Srv(L);
	const EAntFoodKind Kd0 = KindNow(L);
	const bool bFrozen = S.Frozen > 0.5f;
	const float Now = L->GetWorld()->GetTimeSeconds();
	if (Now < FAntFoodAccess::BreakableAfter(L) || Now < 12.f) { return; }   // just spawned / just broke / the level settling at the start
	// the fragile reactions (snap, squirt, smash) only when somebody had a hand in it: carried, dropped, kicked, thrown
	const bool bHandled = L->IsCarried() || L->CarryNet.Grips.Num() > 0 || L->LastCarrier.IsValid() || Now - FAntFoodAccess::LastKick(L) < 4.f;
	switch (Kd0)
	{
	case EAntFoodKind::Crumbly:
		if (!bFrozen) { LoseCrumbs(L, S, FMath::Clamp((Speed - 150.f) / 1500.f * 0.06f, 0.f, 0.06f), TEXT("bump")); }
		break;
	case EAntFoodKind::Brittle:
		if (Speed > 380.f && bHandled) { Snap(L, TEXT("hit")); }
		break;
	case EAntFoodKind::Juicy:
		if (Speed > 300.f && !bFrozen && bHandled) { Squirt(L, S, TEXT("hit")); }
		break;
	case EAntFoodKind::Elastic:
		if (Speed > 400.f && !bFrozen) { AntSfx::Everyone(L, AntSfx::Squish, At, 1.4f); }   // boing
		break;
	case EAntFoodKind::Hard:
		if (Speed > 600.f && !L->IsCarried()) { BreakFragileAround(L, Speed); }
		// a ram: carried fast into another piece of food, it shoves it (and breaks the fragile one)
		if (AKitchenLoot* O = Cast<AKitchenLoot>(Other))
		{
			if (L->IsCarried() && Speed > 200.f && !O->bIsGear)
			{
				O->Kick(L->GetVelocity() * 0.8f + FVector(0.f, 0.f, 120.f), At);
				if (KindNow(O) == EAntFoodKind::Brittle) { Snap(O, TEXT("rammed")); }
			}
		}
		break;
	case EAntFoodKind::Melting:
		if (Speed > 500.f && !bFrozen)
		{
			AntMess::StainAt(L, PuddleKind(FoodOf(L)), FloorBelow(L, L->GetRadius()), FVector::UpVector, FMath::Max(30.f, L->GetRadius() * 1.3f), FMath::RandRange(1, 9999));
		}
		break;
	default:
		break;
	}
	if (bFrozen && Kd0 == EAntFoodKind::Melting && Speed > 900.f) { Snap(L, TEXT("frozen, shattered")); }
}

void OnRelease(AKitchenLoot* L, const FVector& GripWorld)
{
	if (!Kinds() || !L || KindNow(L) != EAntFoodKind::Elastic) { return; }
	FAntFoodServer& S = Srv(L);
	if (S.Tension < 0.3f || S.Frozen > 0.5f) { return; }
	// it was stretched: let go of one end and it springs back towards the others - and on, a catapult
	FVector Dir = L->GetActorLocation() - GripWorld;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero()) { Dir = FVector(-S.SlingPull.X, -S.SlingPull.Y, 0.f).GetSafeNormal(); }
	const float T = S.Tension;
	const FVector Imp = Dir * (350.f + 900.f * T) + FVector(0.f, 0.f, 150.f + 400.f * T);
	L->Kick(Imp, L->GetActorLocation());
	AntSfx::Everyone(L, AntSfx::Squish, L->GetActorLocation(), 1.6f);
	UE_LOG(LogTemp, Display, TEXT("ANTFOOD «%s» springs: stretch %.2f, released -> %.0f uu/s"), *L->LootName, T, Imp.Size());
	S.Tension = 0.f;
}

void OnGrab(AKitchenLoot* L)
{
	if (!Kinds() || !L || KindNow(L) != EAntFoodKind::Brittle) { return; }
	const int32 N = FMath::Max(L->NumCarriers(), L->CarryNet.Grips.Num());
	const int32 Allowed = FMath::Max(2, L->AntsNeeded(10.f) + 1);
	if (N > Allowed) { Snap(L, TEXT("one carrier too many")); }
}

void OnBitten(AKitchenLoot* L)
{
	if (!L || L->FoodNet.BaseWeight == 0) { return; }
	FAntFoodServer& S = Srv(L);
	// BiteOff took weight and value away already: the mass follows (the bite shows on the model)
	const float Soak = Soakable(KindNow(L)) ? S.Soak : 0.f;
	const float NewMass = (float)L->Weight / (L->FoodNet.BaseWeight * (1.f + 0.4f * Soak) * FMath::Max(0.05f, 1.f - 0.85f * S.Melt));
	S.Bitten = FMath::Clamp(S.Bitten + FMath::Max(0.f, S.Mass - NewMass) * 2.f, 0.f, 1.f);
	S.Mass = FMath::Clamp(NewMass, 0.02f, 1.f);
	L->FoodNet.Bitten = B(S.Bitten);
}

FString Describe(const AKitchenLoot* L)
{
	if (!L) { return FString(); }
	const FAntFoodNet& N = L->FoodNet;
	FString S = GKindRu[FMath::Clamp((int32)N.Kind, 0, (int32)UE_ARRAY_COUNT(GKindRu) - 1)];
	auto Add = [&S](uint8 V, const TCHAR* Name) { if (V > 25) { S += FString::Printf(TEXT(", %s %d%%"), Name, FMath::RoundToInt(V / 2.55f)); } };
	Add(N.Soak, TEXT("размокло")); Add(N.Toast, TEXT("подрумянено")); Add(N.Burn, TEXT("подгорело")); Add(N.Frozen, TEXT("замёрзло"));
	Add(N.Dirt, TEXT("грязное")); Add(N.Spoil, TEXT("испортилось")); Add(N.Melt, TEXT("растаяло")); Add(N.Bitten, TEXT("надкусано"));
	return S;
}

void AffectAnt(AAntCharacter* Ant, FAntEnv& Env)
{
	if (!Kinds() || !Ant || GPatches.Num() == 0) { return; }
	UWorld* W = Ant->GetWorld();
	const float Now = W->GetTimeSeconds();
	const FVector P = Ant->GetActorLocation();
	for (int32 i = GPatches.Num() - 1; i >= 0; --i)
	{
		const FGluePatch& G = GPatches[i];
		if (G.Until < Now || !G.W.IsValid()) { GPatches.RemoveAtSwap(i); continue; }
		if (G.W.Get() != W || (G.Team != 255 && G.Team == Ant->Team)) { continue; }
		if (G.Src.IsValid() && Ant->Carrying == G.Src.Get()) { continue; }   // the carriers hold it, it does not hold them
		if (FVector::Dist2D(P, G.P) < G.R + 4.f && FMath::Abs(P.Z - G.P.Z) < G.R + 30.f)
		{
			Env.Flags |= AntStatus::Sticky;
			break;
		}
	}
}

// =============================================================================================
// What it looks like (every machine): the deformation numbers for M_FoodV2 and the shared state layer
// =============================================================================================

struct FFoodVis
{
	FVector LastPos = FVector::ZeroVector;
	FVector LastVel = FVector::ZeroVector;
	float Wobble = 0.f, Stretch = 0.f, Sag = 0.f, Tip = 0.f, TipSign = 1.f;
	int32 Axis = 0;
	FQuat Roll = FQuat::Identity;
	bool bInit = false;
	float Seen = 0.f;
	float NextWatch = 0.f;
};
static TMap<TWeakObjectPtr<const AKitchenLoot>, FFoodVis> GVis;

void PackDeform(float Sag, float Tip, float TipSign, float Stretch, float Squash, int32 Axis, float Wobble, float Melt, float Bite, float Out[3])
{
	auto Q = [](float V, int32 Max) { return FMath::Clamp(FMath::RoundToInt(V * Max), 0, Max); };
	Out[0] = (float)(Q(Sag, 1023) * 1024 + Q(Tip, 1023));
	Out[1] = (float)(Q(Stretch, 1023) * 1024 + Q(Squash, 1023));
	Out[2] = (float)(Q(Wobble, 255) * 65536 + Q(Melt, 255) * 256 + Q(Bite, 63) * 4 + (Axis != 0 ? 2 : 0) + (TipSign < 0.f ? 1 : 0));
}

/**
 * The old models keep their look: the master of an old food's material (M_M4, M_M4_UV, M_MeshyPBR) has a twin with the deformation
 * layer (M_M4_Food, M_M4_UV_Food, M_MeshyPBR_Food: Tools/food/make_food_materials.py, the same graph). The old instance is swapped
 * for a dynamic instance of the twin with all its parameters copied - one per old instance, shared by every piece that uses it.
 */
static TMap<TWeakObjectPtr<UMaterialInterface>, TWeakObjectPtr<UMaterialInstanceDynamic>> GTwinOf;
static TSet<TWeakObjectPtr<UMaterialInterface>> GTwins;

static UMaterialInterface* TwinMaster(const UMaterial* Base)
{
	static const TCHAR* const Pairs[][2] =
	{
		{ TEXT("/Game/Map4/Materials/M_M4.M_M4"), TEXT("/Game/Map4/Materials/M_M4_Food.M_M4_Food") },
		{ TEXT("/Game/Map4/Materials/M_M4_UV.M_M4_UV"), TEXT("/Game/Map4/Materials/M_M4_UV_Food.M_M4_UV_Food") },
		{ TEXT("/Game/Meshy/M_MeshyPBR.M_MeshyPBR"), TEXT("/Game/Meshy/M_MeshyPBR_Food.M_MeshyPBR_Food") },
	};
	static UMaterialInterface* Loaded[UE_ARRAY_COUNT(Pairs)] = {};
	static bool bTried[UE_ARRAY_COUNT(Pairs)] = {};
	if (!Base) { return nullptr; }
	const FString P = Base->GetPathName();
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Pairs); ++i)
	{
		if (P != Pairs[i][0]) { continue; }
		if (!bTried[i])
		{
			bTried[i] = true;
			Loaded[i] = LoadObject<UMaterialInterface>(nullptr, Pairs[i][1], nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Loaded[i]) { Loaded[i]->AddToRoot(); }
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD deform material %s -> %s"), Pairs[i][0], Loaded[i] ? Pairs[i][1] : TEXT("MISSING (old food will not deform)"));
		}
		return Loaded[i];
	}
	return nullptr;
}

static UMaterialInterface* TwinOf(UMaterialInterface* M)
{
	if (!M || GTwins.Contains(M)) { return nullptr; }
	if (const TWeakObjectPtr<UMaterialInstanceDynamic>* Have = GTwinOf.Find(M))
	{
		return Have->IsValid() ? Have->Get() : nullptr;
	}
	UMaterialInterface* Master = TwinMaster(M->GetBaseMaterial());
	UMaterialInstanceDynamic* Mid = nullptr;
	if (Master)
	{
		Mid = UMaterialInstanceDynamic::Create(Master, GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMaterialInstanceDynamic::StaticClass(), *(M->GetName() + TEXT("_Food"))));
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		// (only what the old instance chain sets: the custom primitive data parameters stay the master's)
		int32 Copied = 0;
		M->GetAllScalarParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos) { float V = 0.f; if (M->GetScalarParameterValue(FHashedMaterialParameterInfo(I), V, true)) { Mid->SetScalarParameterValueByInfo(I, V); ++Copied; } }
		M->GetAllVectorParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos) { FLinearColor V; if (M->GetVectorParameterValue(FHashedMaterialParameterInfo(I), V, true)) { Mid->SetVectorParameterValueByInfo(I, V); ++Copied; } }
		M->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& I : Infos) { UTexture* V = nullptr; if (M->GetTextureParameterValue(FHashedMaterialParameterInfo(I), V, true) && V) { Mid->SetTextureParameterValueByInfo(I, V); ++Copied; } }
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD %s -> a deforming twin of %s (%d parameters copied)"), *M->GetPathName(), *Master->GetName(), Copied);
		Mid->AddToRoot();   // (shared by every piece made of this instance, for the whole run)
		GTwins.Add(Mid);
	}
	GTwinOf.Add(M, Mid);
	return Mid;
}

static void UseDeformMaterials(UPrimitiveComponent* Visual)
{
	if (!Visual || Visual->GetNumMaterials() > 8) { return; }
	for (int32 i = 0; i < Visual->GetNumMaterials(); ++i)
	{
		UMaterialInterface* M = Visual->GetMaterial(i);
		// (another system's own dynamic instance is left alone: it may be setting something on it)
		if (!M || GTwins.Contains(M) || M->IsA<UMaterialInstanceDynamic>()) { continue; }
		if (UMaterialInterface* T = TwinOf(M)) { Visual->SetMaterial(i, T); }
	}
}

void VisualTick(AKitchenLoot* L, UPrimitiveComponent* Visual, float Dt)
{
	if (!L || !Visual || L->bIsGear || Dt <= 0.f) { return; }
	const FAntFoodNet& N = L->FoodNet;
	const EAntFoodKind Kd0 = (EAntFoodKind)N.Kind;
	if (Kd0 == EAntFoodKind::None && N.Soak == 0 && N.Burn == 0 && N.Dirt == 0) { return; }
	const float Now = L->GetWorld()->GetTimeSeconds();
	if (GVis.Num() > 600)
	{
		for (auto It = GVis.CreateIterator(); It; ++It) { if (!It->Key.IsValid() || Now - It->Value.Seen > 5.f) { It.RemoveCurrent(); } }
	}
	FFoodVis& Vs = GVis.FindOrAdd(L);
	Vs.Seen = Now;
	const FVector P = L->GetActorLocation();
	if (!Vs.bInit) { Vs.bInit = true; Vs.LastPos = P; }
	const FVector V = (P - Vs.LastPos) / Dt;
	const float Acc = FMath::Min(20000.f, (float)(V - Vs.LastVel).Size() / Dt);
	Vs.LastPos = P;
	Vs.LastVel = V;
	const float Frozen = F(N.Frozen);
	const float Give = 1.f - FMath::Clamp((Frozen - 0.3f) / 0.4f, 0.f, 1.f);   // frozen: no give at all
	const bool bOn = Kinds();
	float Soft = 0.f, Wobbly = 0.f;
	switch (Kd0)
	{
	case EAntFoodKind::Soft: Soft = 1.f; Wobbly = 0.4f; break;
	case EAntFoodKind::Elastic: Soft = 0.35f; Wobbly = 1.f; break;
	case EAntFoodKind::Sticky: Soft = 0.25f; Wobbly = 0.6f; break;
	case EAntFoodKind::Melting: Soft = 0.1f * F(N.Melt); Wobbly = 0.2f; break;
	case EAntFoodKind::Juicy: Soft = 0.08f; Wobbly = 0.15f; break;
	case EAntFoodKind::Crumbly: Soft = 0.06f; Wobbly = 0.05f; break;
	default: break;
	}
	Soft *= Give * (bOn ? 1.f : 0.f);
	Wobbly *= Give * (bOn ? 1.f : 0.f);
	const float K = 1.f - FMath::Exp(-Dt * 9.f);

	// stretched between the carriers (elastic), squeezing through a gap (thin and long)
	float WantStretch = 0.f;
	if (Kd0 == EAntFoodKind::Elastic && bOn)
	{
		FVector2D Mean;
		const float Tn = GripTension(L, Mean);
		WantStretch = FMath::Max(Tn * 0.75f, F(N.Squeeze) * 0.6f) * Give;
		// which way: the axis (local X or Y) the grips spread along most
		float Sx = 0.f, Sy = 0.f;
		for (const FAntCarryGrip& G : L->CarryNet.Grips) { Sx += FMath::Abs((float)G.Local.X); Sy += FMath::Abs((float)G.Local.Y); }
		if (Tn > 0.05f) { Vs.Axis = Sx >= Sy ? 0 : 1; }
		else if (N.Squeeze > 20)
		{
			const FVector Dl = L->GetActorTransform().InverseTransformVectorNoScale(FVector(Mean.X, Mean.Y, 0.f));
			Vs.Axis = FMath::Abs(Dl.X) >= FMath::Abs(Dl.Y) ? 0 : 1;
		}
	}
	Vs.Stretch = FMath::Lerp(Vs.Stretch, WantStretch, K);

	// sag: held up at the grips, the rest droops (a slice of cheese over two carriers sags in the middle, held at one end
	// its far end hangs down); dragged along the floor, less
	float WantSag = 0.f, WantTip = 0.f;
	const TArray<FAntCarryGrip>& G = L->CarryNet.Grips;
	const UBoxComponent* Bd = FAntFoodAccess::Body(L);
	if (Soft > 0.f && G.Num() > 0 && Bd)
	{
		const float Hx = FMath::Max(1.f, (float)Bd->GetUnscaledBoxExtent().X);
		float Mn = 1.f, Mx = -1.f, Sum = 0.f;
		for (const FAntCarryGrip& Gp : G) { const float X = FMath::Clamp((float)Gp.Local.X / Hx, -1.f, 1.f); Mn = FMath::Min(Mn, X); Mx = FMath::Max(Mx, X); Sum += X; }
		const float Mean = Sum / G.Num();
		const float Spread = FMath::Clamp((Mx - Mn) * 0.5f, 0.f, 1.f);
		const float Lift = L->CarryRatio() >= 1.f ? 1.f : 0.4f;
		WantSag = Soft * Spread * 0.5f * Lift;
		WantTip = Soft * FMath::Clamp(1.f - Spread, 0.f, 1.f) * (0.3f + 0.7f * FMath::Abs(Mean)) * Lift;
		if (FMath::Abs(Mean) > 0.05f) { Vs.TipSign = Mean > 0.f ? -1.f : 1.f; }
	}
	Vs.Sag = FMath::Lerp(Vs.Sag, WantSag, K);
	Vs.Tip = FMath::Lerp(Vs.Tip, WantTip, K);

	// wobble: a jolt sets it shaking, it settles
	Vs.Wobble = FMath::Max(Vs.Wobble * FMath::Exp(-Dt * 3.f), FMath::Clamp(Acc / 2500.f, 0.f, 1.f) * Wobbly);
	if (Vs.Wobble < 0.04f) { Vs.Wobble = 0.f; }   // (a body at rest jitters a little: below this it is no jolt, and the model stays as made)

	// rolling: the model turns as it goes (the body itself slides on its ball-bearing friction)
	if (Kd0 == EAntFoodKind::Rolling && bOn && !L->IsCarried() && Visual == FAntFoodAccess::Mesh(L))
	{
		const float Spd = (float)V.Size2D();
		if (Spd > 5.f && Bd)
		{
			const FVector AxisW = FVector::CrossProduct(FVector::UpVector, V.GetSafeNormal2D());
			const FVector AxisL = L->GetActorTransform().InverseTransformVectorNoScale(AxisW).GetSafeNormal();
			const float R = FMath::Max(3.f, (float)Bd->GetUnscaledBoxExtent().Z);
			Vs.Roll = FQuat(AxisL, Spd * Dt / R) * Vs.Roll;
		}
		const FVector Off = Visual->GetRelativeLocation();   // (KitchenLoot set it to the art offset this frame)
		Visual->SetRelativeLocationAndRotation(Vs.Roll.RotateVector(Off), Vs.Roll);
	}

	// an old model: its own material with the deformation layer added (same look at rest)
	if (bOn) { UseDeformMaterials(Visual); }

	// the shared state layer (AntState.h, the food's own channel): toasted is char up to 0.35, burnt blackens above that
	const float Toast = F(N.Toast), Burn = F(N.Burn);
	AntState::SetSource(Visual, AntState::Food, AntState::Wet, FMath::Max(F(N.Soak), Kd0 == EAntFoodKind::Melting ? F(N.Melt) * 0.6f : 0.f));
	AntState::SetSource(Visual, AntState::Food, AntState::Char, FMath::Max(Toast * 0.35f, Burn > 0.01f ? 0.35f + 0.65f * Burn : 0.f));
	AntState::SetSource(Visual, AntState::Food, AntState::Dirt, F(N.Dirt));
	AntState::SetSource(Visual, AntState::Food, AntState::Frost, Frozen);
	AntState::SetSource(Visual, AntState::Food, AntState::Spoil, F(N.Spoil));
	// the deformation, packed into the 3 floats the layer leaves free
	float D[3];
	PackDeform(Vs.Sag, Vs.Tip, Vs.TipSign, Vs.Stretch, F(N.Squeeze) * 0.2f, Vs.Axis, Vs.Wobble, Kd0 == EAntFoodKind::Melting ? F(N.Melt) : 0.f, F(N.Bitten), D);
	for (int32 i = 0; i < 3; ++i)
	{
		const TArray<float>& Cd = Visual->GetCustomPrimitiveData().Data;
		const float Old = Cd.IsValidIndex(CpdDeform0 + i) ? Cd[CpdDeform0 + i] : 0.f;
		if (Old != D[i]) { Visual->SetCustomPrimitiveDataFloat(CpdDeform0 + i, D[i]); }
	}
	// -AntFoodWatch: what THIS machine draws (a guest in co-op: is the deformation and the state there?)
	static const bool bWatch = FParse::Param(FCommandLine::Get(), TEXT("AntFoodWatch"));
	if (bWatch && Now >= Vs.NextWatch && (D[0] + D[1] + D[2] > 0.f || Toast + Burn + Frozen + F(N.Spoil) + F(N.Soak) > 0.f))
	{
		Vs.NextWatch = Now + 1.5f;
		const UMaterialInterface* M0 = Visual->GetMaterial(0);
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD watch [%s] «%s» [%s] stretch %.2f sag %.2f tip %.2f wobble %.2f squash %.2f melt %.2f bite %.2f | toast %.2f burn %.2f frozen %.2f spoil %.2f soak %.2f | material %s, cpd %.0f %.0f %.0f"),
			L->GetNetMode() == NM_Client ? TEXT("guest") : TEXT("host"), *L->LootName, KindName(Kd0), Vs.Stretch, Vs.Sag, Vs.Tip, Vs.Wobble, F(N.Squeeze) * 0.2f,
			Kd0 == EAntFoodKind::Melting ? F(N.Melt) : 0.f, F(N.Bitten), Toast, Burn, Frozen, F(N.Spoil), F(N.Soak), *GetNameSafe(M0), D[0], D[1], D[2]);
	}
}

/** For the logs and the screenshots: the numbers the material gets right now. */
static FString VisDescribe(const AKitchenLoot* L)
{
	const FFoodVis* Vs = GVis.Find(L);
	return Vs ? FString::Printf(TEXT("stretch %.2f (axis %d) sag %.2f tip %.2f wobble %.2f"), Vs->Stretch, Vs->Axis, Vs->Sag, Vs->Tip, Vs->Wobble) : FString(TEXT("-"));
}

static void OnCarryEvent(AKitchenLoot* L, uint8 Kind, uint8 Team)
{
	if (L && L->FoodSrv.IsValid() && (Kind == AntCarryEvent::Delivered || Kind == AntCarryEvent::Broken || Kind == AntCarryEvent::Snatched))
	{
		CarryEnd(L, *L->FoodSrv, Kind == AntCarryEvent::Delivered ? TEXT("delivered") : Kind == AntCarryEvent::Broken ? TEXT("broken") : TEXT("snatched"));
	}
}

void RegisterEvents()
{
	static bool bDone = false;
	if (!bDone) { bDone = true; AntCarry::Events().AddStatic(&OnCarryEvent); }
}

static void ForFoodNear(UWorld* W, const FVector& At, float R, TFunctionRef<void(AKitchenLoot*)> Fn)
{
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		if (!It->bIsGear && !It->IsActorBeingDestroyed() && FVector::Dist(It->GetActorLocation(), At) <= R) { Fn(*It); }
	}
}

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (!GM || A.Num() < 2 || !A[0].Equals(TEXT("food"), ESearchCase::IgnoreCase)) { return false; }
	UWorld* W = GM->GetWorld();
	const FString Op = A[1].ToLower();
	auto Num = [&A](int32 i, float Def = 0.f) { return A.IsValidIndex(i) ? FCString::Atof(*A[i]) : Def; };
	APlayerController* PC = W->GetFirstPlayerController();
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	const FVector Here = Me ? Me->GetActorLocation() : FVector::ZeroVector;
	RegisterEvents();

	if (Op == TEXT("list"))
	{
		for (const FFood& Fd : GTable)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD table %-11s %-8s «%s» value %d weight %d size %.1f cm (%.0f uu) spoil %.0f min pieces %d%s, V2 model %s"),
				Fd.Id, KindName(Fd.Kind), Fd.Ru, Fd.Value, Fd.Weight, Fd.SizeCm, Fd.SizeCm * 19.f, Fd.SpoilMin, Fd.Pieces, Fd.bNew ? TEXT(" (new)") : TEXT(""),
				LoadV2(Fd.Id, -1).Mesh ? TEXT("yes") : TEXT("no"));
		}
		return true;
	}
	if (Op == TEXT("log")) { GLogAll = Num(2, 1.f) > 0.5f; return true; }
	if (Op == TEXT("row"))
	{
		// food row <kind|id|all> x y z [spacing] [yaw]: a row of foods to look at (screenshots)
		const FString What = A.IsValidIndex(2) ? A[2] : FString(TEXT("all"));
		FVector At(Num(3, Here.X), Num(4, Here.Y), Num(5, Here.Z));
		const float Step = Num(6, 90.f);
		const FVector Dir = FRotator(0.f, Num(7, 0.f), 0.f).Vector();
		int32 n = 0;
		for (const FFood& Fd : GTable)
		{
			if (!(What.Equals(TEXT("all"), ESearchCase::IgnoreCase) || What.Equals(Fd.Id, ESearchCase::IgnoreCase) || What.Equals(KindName(Fd.Kind), ESearchCase::IgnoreCase))) { continue; }
			FAntFoodDef D;
			DefOf(Fd, D);
			const FTransform TM(At + Dir * Step * n);
			AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!L) { continue; }
			L->InitFood(D);
			L->FinishSpawning(TM);
			++n;
		}
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD row %s: %d foods from %s"), *What, n, *At.ToCompactString());
		return true;
	}
	if (Op == TEXT("sheet"))
	{
		// food sheet <id> x y z [spacing] [yaw]: one food in every state in a row (fresh, toasted, burnt, soaked, frozen, dirty,
		// spoiled, bitten, melted) - the in-game state sheet for screenshots
		const FFood* Fd = A.IsValidIndex(2) ? ById(A[2]) : nullptr;
		if (!Fd) { UE_LOG(LogTemp, Warning, TEXT("ANTFOOD sheet: unknown food %s"), A.IsValidIndex(2) ? *A[2] : TEXT("-")); return true; }
		const FVector At(Num(3, Here.X), Num(4, Here.Y), Num(5, Here.Z));
		const float Step = Num(6, Fd->SizeCm * 19.f * 1.6f + 20.f);
		const FVector Dir = FRotator(0.f, Num(7, 0.f), 0.f).Vector();
		static const TCHAR* const States[] = { TEXT("fresh"), TEXT("toast"), TEXT("burn"), TEXT("soak"), TEXT("frozen"), TEXT("dirt"), TEXT("spoil"), TEXT("bite"), TEXT("melt") };
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(States); ++i)
		{
			FAntFoodDef D;
			DefOf(*Fd, D);
			const FTransform TM(At + Dir * Step * i);
			AKitchenLoot* L = W->SpawnActorDeferred<AKitchenLoot>(AKitchenLoot::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!L) { continue; }
			L->InitFood(D);
			L->FinishSpawning(TM);
			FAntFoodServer& S = Srv(L);
			const FString St = States[i];
			if (St == TEXT("toast")) { S.Toast = 1.f; S.ToastWarmUntil = W->GetTimeSeconds() + 60.f; }
			else if (St == TEXT("burn")) { S.Toast = 1.f; S.Burn = 1.f; }
			else if (St == TEXT("soak")) { S.Soak = 1.f; S.bSoakBroke = true; }
			else if (St == TEXT("frozen")) { S.Frozen = 1.f; }
			else if (St == TEXT("dirt")) { S.Dirt = 1.f; }
			else if (St == TEXT("spoil")) { S.Spoil = 1.f; S.NextFly = W->GetTimeSeconds() + 1000.f; }
			else if (St == TEXT("bite")) { S.Bitten = 1.f; }
			else if (St == TEXT("melt")) { S.Melt = Fd->Kind == EAntFoodKind::Melting ? 0.5f : 0.f; }
			FAntFoodNet& N = L->FoodNet;
			N.Soak = B(S.Soak); N.Toast = B(S.Toast); N.Burn = B(S.Burn); N.Frozen = B(S.Frozen); N.Dirt = B(S.Dirt); N.Spoil = B(S.Spoil); N.Melt = B(S.Melt); N.Bitten = B(S.Bitten);
			ApplyMass(L, S);
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD sheet %s %s at %s: weight %d value %d (%s)"), Fd->Id, *St, *L->GetActorLocation().ToCompactString(), L->Weight, L->Value, *Describe(L));
		}
		return true;
	}
	if (Op == TEXT("state"))
	{
		// food state <soak|toast|burn|frozen|dirt|spoil|melt|bite|clear> <0..1> [x y z r]: set a state on the food around
		const FString St = A.IsValidIndex(2) ? A[2].ToLower() : FString();
		const float Val = FMath::Clamp(Num(3, 1.f), 0.f, 1.f);
		const FVector At = A.Num() >= 7 ? FVector(Num(4), Num(5), Num(6)) : Here;
		const float R = Num(7, 400.f);
		int32 n = 0;
		ForFoodNear(W, At, R, [&](AKitchenLoot* L)
		{
			FAntFoodServer& S = Srv(L);
			if (St == TEXT("soak")) { S.Soak = Val; S.bSoakBroke = true; }
			else if (St == TEXT("toast")) { S.Toast = Val; S.ToastWarmUntil = W->GetTimeSeconds() + 60.f; }
			else if (St == TEXT("burn")) { S.Burn = Val; }
			else if (St == TEXT("frozen")) { S.Frozen = Val; }
			else if (St == TEXT("dirt")) { S.Dirt = Val; }
			else if (St == TEXT("spoil")) { S.Spoil = Val; }
			else if (St == TEXT("melt")) { S.Melt = FMath::Min(Val, 0.9f); }
			else if (St == TEXT("bite")) { S.Bitten = Val; }
			else if (St == TEXT("clear")) { S.Soak = S.Toast = S.Burn = S.Frozen = S.Dirt = S.Spoil = S.Melt = S.Bitten = 0.f; }
			FAntFoodNet& N = L->FoodNet;
			N.Soak = B(S.Soak); N.Toast = B(S.Toast); N.Burn = B(S.Burn); N.Frozen = B(S.Frozen); N.Dirt = B(S.Dirt); N.Spoil = B(S.Spoil); N.Melt = B(S.Melt); N.Bitten = B(S.Bitten);
			ApplyMass(L, S);
			++n;
		});
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD state %s %.2f on %d foods within %.0f of %s"), *St, Val, n, R, *At.ToCompactString());
		return true;
	}
	if (Op == TEXT("report"))
	{
		const FVector At = A.Num() >= 5 ? FVector(Num(2), Num(3), Num(4)) : Here;
		ForFoodNear(W, At, Num(5, 3000.f), [&](AKitchenLoot* L)
		{
			// what is drawn: the mesh, its material (an old model's deforming twin: *_Food) and the packed deformation it gets
			const UStaticMeshComponent* Ms = FAntFoodAccess::Mesh(L);
			const UPrimitiveComponent* Vis = Ms && Ms->IsVisible() ? (const UPrimitiveComponent*)Ms : nullptr;
			if (!Vis) { for (UActorComponent* Ac : L->GetComponents()) { if (UProceduralMeshComponent* Pm = Cast<UProceduralMeshComponent>(Ac)) { if (Pm->IsVisible()) { Vis = Pm; } } } }
			UMaterialInterface* M0 = Vis ? Vis->GetMaterial(0) : nullptr;
			const TArray<float>& Cd = Vis ? Vis->GetCustomPrimitiveData().Data : TArray<float>();
			auto CpdAt = [&Cd](int32 i) { return Cd.IsValidIndex(i) ? Cd[i] : 0.f; };
			UE_LOG(LogTemp, Display, TEXT("ANTFOOD report «%s» [%s] w %d/%d v %d/%d carriers %d at %s: %s; look: %s; model %s %s, material %s (base %s), cpd deform %.0f %.0f %.0f, state wet %.2f char %.2f frost %.2f spoil %.2f"),
				*L->LootName, KindName(KindNow(L)), L->Weight, (int32)L->FoodNet.BaseWeight, L->Value, (int32)L->FoodNet.BaseValue, L->CarryNet.Grips.Num(),
				*L->GetActorLocation().ToCompactString(), *Describe(L), *VisDescribe(L), ModelV2(L) ? TEXT("V2") : TEXT("old"),
				Vis == Ms && Ms && Ms->GetStaticMesh() ? *Ms->GetStaticMesh()->GetName() : Vis ? TEXT("(break piece)") : TEXT("-"),
				*GetNameSafe(M0), M0 && M0->GetBaseMaterial() ? *M0->GetBaseMaterial()->GetName() : TEXT("-"),
				CpdAt(13), CpdAt(14), CpdAt(15), CpdAt(0), CpdAt(1), CpdAt(6), CpdAt(12));
		});
		return true;
	}
	if (Op == TEXT("heat"))
	{
		const FVector At = A.Num() >= 5 ? FVector(Num(2), Num(3), Num(4)) : Here;
		UE_LOG(LogTemp, Display, TEXT("ANTFOOD heat at %s: %.0f °C"), *At.ToCompactString(), AntReact::HeatAt(W, At));
		return true;
	}
	if (Op == TEXT("kick"))
	{
		// food kick dx dy dz [x y z r]: shove the food around (drop tests: bounce, snap, squirt)
		const FVector Imp(Num(2), Num(3), Num(4));
		const FVector At = A.Num() >= 8 ? FVector(Num(5), Num(6), Num(7)) : Here;
		ForFoodNear(W, At, Num(8, 400.f), [&](AKitchenLoot* L) { L->Kick(Imp, L->GetActorLocation()); });
		return true;
	}
	return false;
}
} // namespace AntFood
