#include "LevelV3.h"
#include "KitchenArt.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/StaticMesh.h"

#include "LevelV3Art.inl"
#include "LevelV3Data.inl"

namespace LevelV3
{

bool Enabled()
{
	static int32 State = -1;
	if (State < 0)
	{
		const TCHAR* Cmd = FCommandLine::Get();
		// v3 is the default level (21.11.2026; -AntLevelV3 still accepted); -AntLevelV2 always wins (the way back is one switch).
		const bool bAsk = !FParse::Param(Cmd, TEXT("AntLevelV2")) && !FParse::Param(Cmd, TEXT("AntOldKitchen"));
		State = (bAsk && KitchenArt::Mesh(TEXT("SM_V3_WallShelf")) != nullptr) ? 1 : 0;
		if (bAsk && !State)
		{
			UE_LOG(LogTemp, Warning, TEXT("ANTSEED level v3 asked for but its meshes are missing from the build: level v2"));
		}
		UE_LOG(LogTemp, Display, TEXT("ANTSEED level %s"), State ? TEXT("v3") : TEXT("v2"));
	}
	return State == 1;
}

bool FixturesOn()
{
	// The interactive objects are actors (KitchenFixture.h). -AntNoFixtures keeps them plain pieces of the base (to compare).
	static const bool bOn = !FParse::Param(FCommandLine::Get(), TEXT("AntNoFixtures"));
	return bOn;
}

TConstArrayView<FKitchenArtInst> BaseArt() { return TConstArrayView<FKitchenArtInst>(GKitchenArtV3, UE_ARRAY_COUNT(GKitchenArtV3)); }
TConstArrayView<FKitchenArtMover> BaseMovers() { return TConstArrayView<FKitchenArtMover>(GKitchenMoversV3, UE_ARRAY_COUNT(GKitchenMoversV3)); }
const TCHAR* BaseId(int32 Inst) { return Inst >= 0 && Inst < (int32)UE_ARRAY_COUNT(GKitchenArtV3Ids) ? GKitchenArtV3Ids[Inst] : TEXT(""); }
uint8 BaseFlags(int32 Inst) { return Inst >= 0 && Inst < (int32)UE_ARRAY_COUNT(GKitchenArtV3Flags) ? GKitchenArtV3Flags[Inst] : 0; }
const TCHAR* BaseTint(int32 Inst) { return Inst >= 0 && Inst < (int32)UE_ARRAY_COUNT(GKitchenArtV3Tint) ? GKitchenArtV3Tint[Inst] : TEXT(""); }
int32 FindBase(const TCHAR* Id)
{
	if (!Id || !*Id) { return INDEX_NONE; }
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(GKitchenArtV3Ids); ++i)
	{
		if (FCString::Strcmp(GKitchenArtV3Ids[i], Id) == 0) { return i; }
	}
	return INDEX_NONE;
}
TConstArrayView<FKitchenArtInst> VarArt() { return TConstArrayView<FKitchenArtInst>(GV3VarArt, UE_ARRAY_COUNT(GV3VarArt)); }
uint8 VarFlags(int32 i) { return i >= 0 && i < (int32)UE_ARRAY_COUNT(GV3VarArtFlags) ? GV3VarArtFlags[i] : 0; }
const TCHAR* VarTint(int32 i) { return i >= 0 && i < (int32)UE_ARRAY_COUNT(GV3VarArtTint) ? GV3VarArtTint[i] : TEXT(""); }
TConstArrayView<FLevelV3Op> Ops() { return TConstArrayView<FLevelV3Op>(GV3Ops, UE_ARRAY_COUNT(GV3Ops)); }
TConstArrayView<FLevelV3Variant> Variants() { return TConstArrayView<FLevelV3Variant>(GV3Variants, UE_ARRAY_COUNT(GV3Variants)); }
int32 NumDistricts() { return UE_ARRAY_COUNT(GV3DistrictIds); }
const TCHAR* DistrictId(int32 D) { return D >= 0 && D < NumDistricts() ? GV3DistrictIds[D] : TEXT(""); }
const TCHAR* DistrictName(int32 D) { return D >= 0 && D < NumDistricts() ? GV3DistrictRu[D] : TEXT(""); }
namespace
{
	// The design left two fixtures without fastener rows: the upper cabinet doors ("hinge x2 per door") and the toaster ("in the code").
	// The code adds them: two doors of the cabinet over the toaster on two hinges each, and the toaster's crumb tray (its own work).
	// Inst -2 - k: row k of GExtraRows (the hinge pieces); the tray is the toaster fixture's own work (Inst -1).
	const FKitchenArtInst GExtraRows[] =
	{
		{ TEXT("SM_V3_Hinge"), 0, 0, -1, FVector3f(-6935.f, 4318.f, 4180.f), FVector3f(-1.f, 0.f, 0.f), FVector3f(0.f, -1.f, 0.f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 1.f, 1.f) },
		{ TEXT("SM_V3_Hinge"), 0, 0, -1, FVector3f(-6935.f, 4318.f, 3240.f), FVector3f(-1.f, 0.f, 0.f), FVector3f(0.f, -1.f, 0.f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 1.f, 1.f) },
		{ TEXT("SM_V3_Hinge"), 0, 0, -1, FVector3f(-5815.f, 4318.f, 4180.f), FVector3f(-1.f, 0.f, 0.f), FVector3f(0.f, -1.f, 0.f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 1.f, 1.f) },
		{ TEXT("SM_V3_Hinge"), 0, 0, -1, FVector3f(-5815.f, 4318.f, 3240.f), FVector3f(-1.f, 0.f, 0.f), FVector3f(0.f, -1.f, 0.f), FVector3f(0.f, 0.f, 1.f), FVector3f(1.f, 1.f, 1.f) },
	};
	const TCHAR* const GExtraIds[] = { TEXT("v3_upd_r_top"), TEXT("v3_upd_r_bot"), TEXT("v3_upd_l_top"), TEXT("v3_upd_l_bot") };
	const TCHAR* const GExtraDoor[] = { TEXT("SM_K_WallCab1250_DoorR"), TEXT("SM_K_WallCab1250_DoorR"), TEXT("SM_K_WallCab1250_DoorL"), TEXT("SM_K_WallCab1250_DoorL") };
	TArray<FLevelV3Fixture> GFixAll;
	TArray<FLevelV3FastenerDef> GFastAll;
	int32 GFirstExtra = -1;

	void BuildFixtureTables()
	{
		if (GFixAll.Num() > 0) { return; }
		GFixAll.Append(GV3Fixtures, UE_ARRAY_COUNT(GV3Fixtures));
		GFastAll.Append(GV3Fasteners, UE_ARRAY_COUNT(GV3Fasteners));
		for (int32 f = 0; f < GFixAll.Num(); ++f)
		{
			if (FCString::Strcmp(GFixAll[f].Id, TEXT("upper_doors")) == 0)
			{
				GFixAll[f].FirstFastener = GFastAll.Num();
				GFixAll[f].NumFasteners = UE_ARRAY_COUNT(GExtraRows);
				GFirstExtra = GFastAll.Num();
				for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(GExtraRows); ++k) { GFastAll.Add({ GExtraIds[k], -2 - k, Hinge, f }); }
			}
			else if (FCString::Strcmp(GFixAll[f].Id, TEXT("toaster")) == 0)
			{
				GFixAll[f].FirstFastener = GFastAll.Num();
				GFixAll[f].NumFasteners = 1;
				GFastAll.Add({ TEXT("v3_toaster_tray"), -1, Code, f });
			}
		}
	}
}

TConstArrayView<FLevelV3Fixture> Fixtures() { BuildFixtureTables(); return GFixAll; }
TConstArrayView<FLevelV3FastenerDef> Fasteners() { BuildFixtureTables(); return GFastAll; }
const FKitchenArtInst* ExtraFastenerRow(int32 Inst)
{
	const int32 k = -2 - Inst;
	return k >= 0 && k < (int32)UE_ARRAY_COUNT(GExtraRows) ? &GExtraRows[k] : nullptr;
}
int32 UpperDoorMover(int32 FastenerDef, bool* bTop)
{
	BuildFixtureTables();
	const int32 k = FastenerDef - GFirstExtra;
	if (GFirstExtra < 0 || k < 0 || k >= (int32)UE_ARRAY_COUNT(GExtraRows)) { return INDEX_NONE; }
	if (bTop) { *bTop = (k % 2) == 0; }
	// the door of the cabinet over the toaster whose hinge side this is: the mover of that mesh nearest to the hinge
	const TConstArrayView<FKitchenArtMover> M = BaseMovers();
	const TConstArrayView<FKitchenArtInst> Rows = BaseArt();
	int32 Best = INDEX_NONE;
	float BestD = 1.e9f;
	for (int32 i = 0; i < M.Num(); ++i)
	{
		if (!Rows.IsValidIndex(M[i].Inst) || FCString::Strcmp(Rows[M[i].Inst].Name, GExtraDoor[k]) != 0) { continue; }
		const float D = FVector2f::Distance(FVector2f(Rows[M[i].Inst].Loc.X, Rows[M[i].Inst].Loc.Y), FVector2f(GExtraRows[k].Loc.X, GExtraRows[k].Loc.Y));
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}
void FixtureParts(int32 F, TArray<int32>& Out)
{
	Out.Reset();
	for (int32 i = 0; GV3FixtureParts[i][0] >= 0; ++i)
	{
		if (GV3FixtureParts[i][0] == F) { Out.Add(GV3FixtureParts[i][1]); }
	}
}
void FixtureRows(TSet<int32>& Out)
{
	Out.Reset();
	if (!FixturesOn()) { return; }
	for (const FLevelV3Fixture& F : GV3Fixtures)
	{
		if (F.Inst >= 0) { Out.Add(F.Inst); }
	}
	for (const FLevelV3FastenerDef& F : GV3Fasteners)
	{
		if (F.Inst >= 0) { Out.Add(F.Inst); }
	}
	for (int32 i = 0; GV3FixtureParts[i][0] >= 0; ++i) { Out.Add(GV3FixtureParts[i][1]); }
	for (const FLevelV3Fixture& F : GV3Fixtures)
	{
		if (FCString::Strcmp(F.Id, TEXT("toaster")) == 0 || FCString::Strcmp(F.Id, TEXT("upper_doors")) == 0)
		{
			Out.Remove(F.Inst);   // (the toaster's body and the cabinet stay the builder's: their fixtures are the crumb tray and the doors' hinges)
			continue;
		}
		for (const TCHAR* E : ExtraParts(F.Id)) { const int32 R = FindBase(E); if (R >= 0) { Out.Add(R); } }
	}
}

TArray<const TCHAR*> ExtraParts(const TCHAR* Id)
{
	const FString S(Id);
	if (S == TEXT("balloons")) { return { TEXT("v3_bal1"), TEXT("v3_bal2"), TEXT("v3_str1"), TEXT("v3_str2") }; }
	if (S == TEXT("curtain_k")) { return { TEXT("v3_curt_e") }; }
	if (S == TEXT("fridge_art")) { return { TEXT("v3_draw1") }; }
	return {};
}

float PushNeed(const TCHAR* Id)
{
	const FString S(Id);
	// one ant: 14; "вдвоём": 24; "втроём": 40 (AntDoor's classes)
	if (S == TEXT("pickles") || S == TEXT("cereal") || S == TEXT("cola")) { return 24.f; }
	if (S == TEXT("glasses")) { return 40.f; }
	if (S == TEXT("trolley")) { return 24.f; }
	return 14.f;
}

const TCHAR* VarId(int32 V) { return V >= 0 && V < (int32)UE_ARRAY_COUNT(GV3VarArtIds) ? GV3VarArtIds[V] : TEXT(""); }

bool IsVarActive(int32 V)
{
	for (int32 D = 0; D < NumDistricts(); ++D)
	{
		const int32 Vi = CurrentVariant(D);
		if (Vi == INDEX_NONE) { continue; }
		const FLevelV3Variant& Vr = GV3Variants[Vi];
		for (int32 k = 0; k < Vr.NumOps; ++k)
		{
			if (GV3Ops[Vr.FirstOp + k].Kind == Add && GV3Ops[Vr.FirstOp + k].A == V) { return true; }
		}
	}
	return false;
}

int32 ActiveVarRow(const TCHAR* Obj)
{
	for (int32 v = 0; v < (int32)UE_ARRAY_COUNT(GV3VarArtIds); ++v)
	{
		if (FCString::Strcmp(GV3VarArtIds[v], Obj) == 0 && IsVarActive(v)) { return v; }
	}
	return INDEX_NONE;
}

bool IsFixtureObject(const TCHAR* Obj)
{
	if (!FixturesOn() || !Obj || !*Obj) { return false; }
	const FString S(Obj);
	for (const FLevelV3Fixture& F : GV3Fixtures)
	{
		if (F.Inst < 0 && (S == F.Obj || S.StartsWith(FString(F.Obj) + TEXT("_")))) { return true; }
	}
	return S.StartsWith(TEXT("v3_cup")) && FixturesOn();   // the cups ride the tablecloth
}

bool IsVariantRow(int32 Inst)
{
	for (const FLevelV3Op& O : GV3Ops)
	{
		if ((O.Kind == Remove || O.Kind == Move) && O.A == Inst) { return true; }
	}
	return false;
}

TConstArrayView<FLevelV3Seed> Seeds() { return TConstArrayView<FLevelV3Seed>(GV3Seeds, UE_ARRAY_COUNT(GV3Seeds)); }
TConstArrayView<FLevelV3Scatter> Scatter() { return TConstArrayView<FLevelV3Scatter>(GV3Scatter, UE_ARRAY_COUNT(GV3Scatter) - 1); }
const TCHAR* ScatterName(int32 i) { return i >= 0 && i < (int32)UE_ARRAY_COUNT(GV3ScatterNames) ? GV3ScatterNames[i] : TEXT(""); }

// ---------------------------------------------------------------------------------------------
// The current layout and what it derives
// ---------------------------------------------------------------------------------------------
namespace
{
	const FLevelV3Seed* GCur = nullptr;
	TCHAR GLetters[32] = { 0 };
	TArray<FLevelV2Spot> GLoot;
	TArray<FVector3f> GHeavy;
	TArray<FLevelV2Box> GCover;
	TArray<FLevelV2Zone> GZones;
	TArray<FLevelV2Zone> GPlayZones;
	TArray<FLevelV2Zone> GAllZones;
	TArray<FString> GConnOff, GHazOff, GHazOn;
	FVector2D GDogBowl;
	bool bDogBowl = false;
	bool bInit = false;

	void RebuildZones()
	{
		GAllZones = GZones;
		GAllZones.Append(GPlayZones);
	}

	int32 VariantFor(int32 D, TCHAR Letter)
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(GV3Variants); ++i)
		{
			if (GV3Variants[i].District == D && GV3Variants[i].Letter == Letter) { return i; }
		}
		return INDEX_NONE;
	}

	void Recompute()
	{
		bInit = true;
		GLoot.Reset();
		GLoot.Append(GV3Loot, UE_ARRAY_COUNT(GV3Loot));
		GHeavy.Reset();
		GHeavy.Append(GV3Heavy, UE_ARRAY_COUNT(GV3Heavy));
		GCover.Reset();
		GCover.Append(GV3Cover, UE_ARRAY_COUNT(GV3Cover));
		TArray<FString> CoverIds;
		for (const TCHAR* Id : GV3CoverIds) { CoverIds.Add(Id); }
		GZones.Reset();
		GZones.Append(GV3Zones, UE_ARRAY_COUNT(GV3Zones));
		GConnOff.Reset();
		GHazOff.Reset();
		GHazOn.Reset();
		bDogBowl = false;
		for (int32 D = 0; D < NumDistricts(); ++D)
		{
			const int32 V = VariantFor(D, GLetters[D] ? GLetters[D] : TEXT('A'));
			if (V == INDEX_NONE) { continue; }
			const FLevelV3Variant& Var = GV3Variants[V];
			for (int32 k = 0; k < Var.NumOps; ++k)
			{
				const FLevelV3Op& O = GV3Ops[Var.FirstOp + k];
				switch (O.Kind)
				{
				case Hide:
					GCover.Add({ O.F[0], O.F[1], 0.f, O.F[2], O.F[3], O.F[5], O.Ru });
					CoverIds.Add(O.Text);
					break;
				case HideOff:
					for (int32 c = CoverIds.Num() - 1; c >= 0; --c)
					{
						if (CoverIds[c] == O.Text) { CoverIds.RemoveAt(c); GCover.RemoveAt(c); }
					}
					break;
				case Hazard:
					if (O.B == 0 || O.B == 1)
					{
						GZones.Add({ O.B, O.F[0], O.F[1], -50.f, O.F[2], O.F[3], FMath::Max(300.f, O.F[5]), O.F[4] });
					}
					GHazOn.Add(O.Text);
					break;
				case HazardOff: GHazOff.Add(O.Text); break;
				case ConnOff: GConnOff.Add(O.Text); break;
				case DogSpot: GDogBowl = FVector2D(O.F[0], O.F[1]); bDogBowl = true; break;
				case LootZ:
				{
					TArray<FString> Ids;
					FString(O.Text).ParseIntoArray(Ids, TEXT(","));
					for (const FString& Id : Ids)
					{
						const int32 N = FCString::Atoi(*Id.Mid(1));
						if (Id.StartsWith(TEXT("L")))
						{
							for (int32 s = 0; s < GLoot.Num(); ++s) { if (GV3LootIds[s] == N) { GLoot[s].Z = O.F[0]; } }
						}
						else if (Id.StartsWith(TEXT("H")))
						{
							for (int32 s = 0; s < GHeavy.Num(); ++s) { if (GV3HeavyIds[s] == N) { GHeavy[s].Z = O.F[0]; } }
						}
					}
					break;
				}
				default: break;
				}
			}
		}
		RebuildZones();
	}

	void EnsureInit()
	{
		if (!bInit)
		{
			for (int32 D = 0; D < NumDistricts(); ++D) { GLetters[D] = TEXT('A'); }
			Recompute();
		}
	}

	const TCHAR* const HomeNames[4] = { TEXT("бабушка"), TEXT("малыш"), TEXT("Бадрик"), TEXT("кошка") };
	const TCHAR* const HomeIds[4] = { TEXT("gran"), TEXT("kid"), TEXT("dog"), TEXT("cat") };
}

TConstArrayView<FLevelV2Spot> LootSpots() { EnsureInit(); return GLoot; }
TConstArrayView<FVector3f> HeavySpots() { EnsureInit(); return GHeavy; }
TConstArrayView<FLevelV2Main> MainSpots() { return TConstArrayView<FLevelV2Main>(GV3Main, UE_ARRAY_COUNT(GV3Main)); }
TConstArrayView<FVector2f> PatrolStops() { return TConstArrayView<FVector2f>(GV3Patrol, UE_ARRAY_COUNT(GV3Patrol)); }
TConstArrayView<FLevelV2Box> CoverBoxes() { EnsureInit(); return GCover; }
TConstArrayView<FLevelV2Box> TargetBounds() { return TConstArrayView<FLevelV2Box>(GV3TargetBounds, UE_ARRAY_COUNT(GV3TargetBounds)); }
TConstArrayView<FLevelV2Zone> Zones() { EnsureInit(); return GAllZones; }
TConstArrayView<FLevelV2District> Districts() { return TConstArrayView<FLevelV2District>(GV3Districts, UE_ARRAY_COUNT(GV3Districts)); }
TConstArrayView<FLevelV2Callout> Callouts() { return TConstArrayView<FLevelV2Callout>(GV3Callouts, UE_ARRAY_COUNT(GV3Callouts)); }
FBox2D WanderBox(int32 Room)
{
	const float* B = Room == 0 ? GV3WanderKitchen : GV3WanderLiving;
	return FBox2D(FVector2D(B[0], B[1]), FVector2D(B[2], B[3]));
}

const FLevelV3Seed* FindSeed(int32 Seed, int32 Tier)
{
	const FLevelV3Seed* Best = nullptr;
	const FLevelV3Seed* First = nullptr;
	for (const FLevelV3Seed& S : GV3Seeds)
	{
		if (S.Tier != Tier) { continue; }
		if (!First || S.Seed < First->Seed) { First = &S; }
		if (S.Seed >= Seed && (!Best || S.Seed < Best->Seed)) { Best = &S; }
	}
	return Best ? Best : First;
}

const FLevelV3Seed* PickSeed(int32 Tier, FRandomStream& Rng)
{
	TArray<const FLevelV3Seed*> Of;
	for (const FLevelV3Seed& S : GV3Seeds)
	{
		if (S.Tier == Tier) { Of.Add(&S); }
	}
	return Of.Num() ? Of[Rng.RandRange(0, Of.Num() - 1)] : nullptr;
}

void SetCurrent(const FLevelV3Seed* S)
{
	GCur = S;
	for (int32 D = 0; D < NumDistricts(); ++D)
	{
		GLetters[D] = (S && D < FCString::Strlen(S->Variants)) ? S->Variants[D] : TEXT('A');
	}
	GPlayZones.Reset();
	Recompute();
}

const FLevelV3Seed* Current() { return GCur; }

int32 CurrentVariant(int32 D)
{
	EnsureInit();
	return VariantFor(D, GLetters[D] ? GLetters[D] : TEXT('A'));
}

TCHAR CurrentLetter(int32 D)
{
	EnsureInit();
	return D >= 0 && D < NumDistricts() && GLetters[D] ? GLetters[D] : TEXT('A');
}

bool IsHome(int32 Who) { return GCur ? (GCur->Home & Who) != 0 : true; }
static int32 GTimeOverride = -1;
void SetTimeOverride(int32 T) { GTimeOverride = FMath::Clamp(T, -1, 2); }
int32 TimeOfDay() { return GTimeOverride >= 0 ? GTimeOverride : GCur ? GCur->Time : Evening; }
const TCHAR* TimeName(int32 T) { return T == Day ? TEXT("день") : T == Night ? TEXT("ночь") : TEXT("вечер"); }

bool IsLoose(int32 Inst)
{
	if (!GCur || Inst < 0) { return false; }
	return GCur->Loose[0] == Inst || GCur->Loose[1] == Inst || GCur->Loose[2] == Inst;
}

bool IsConnOff(const TCHAR* Id) { EnsureInit(); return GConnOff.Contains(FString(Id)); }
bool IsHazardOff(const TCHAR* Id) { EnsureInit(); return GHazOff.Contains(FString(Id)); }
bool IsHazardOn(const TCHAR* Id) { EnsureInit(); return GHazOn.Contains(FString(Id)); }
bool DogBowl(FVector2D& Out) { EnsureInit(); Out = GDogBowl; return bDogBowl; }

FString Describe()
{
	EnsureInit();
	FString Home, Var, Doors, Loose;
	for (int32 i = 0; i < 4; ++i)
	{
		if (IsHome(1 << i)) { Home += (Home.IsEmpty() ? TEXT("") : TEXT(",")) + FString(HomeIds[i]); }
	}
	for (int32 D = 0; D < NumDistricts(); ++D) { Var.AppendChar(CurrentLetter(D)); }
	for (int32 i = 0; GCur && i < 3; ++i)
	{
		if (GCur->Doors[i] >= 0) { Doors += FString::Printf(TEXT("%s%d"), Doors.IsEmpty() ? TEXT("") : TEXT(","), GCur->Doors[i]); }
		if (GCur->Loose[i] >= 0) { Loose += FString::Printf(TEXT("%s%s"), Loose.IsEmpty() ? TEXT("") : TEXT(","), BaseId(GCur->Loose[i])); }
	}
	return FString::Printf(TEXT("ANTSEED seed=%d tier=%d time=%s home=human,%s variants=%s cake=M%d doors=%s loose=%s scatter=%d"),
		GCur ? GCur->Seed : -1, GCur ? GCur->Tier : 0, GCur ? (GCur->Time == Day ? TEXT("day") : GCur->Time == Night ? TEXT("night") : TEXT("evening")) : TEXT("-"),
		*Home, *Var, GCur ? GCur->Cake + 1 : 0, *Doors, *Loose, GCur ? GCur->ScatterNum : 0);
}

FString ShortText()
{
	if (!GCur) { return FString(); }
	FString Home;
	for (int32 i = 0; i < 4; ++i)
	{
		if (IsHome(1 << i)) { Home += (Home.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(HomeNames[i]); }
	}
	return FString::Printf(TEXT("Сид %d · %s · дома: папа%s%s"), GCur->Seed, TimeName(GCur->Time), Home.IsEmpty() ? TEXT("") : TEXT(", "), *Home);
}

void ApplyDevVariants()
{
	FString Arg;
	if (!FParse::Value(FCommandLine::Get(), TEXT("AntVariant="), Arg, false)) { return; }
	TArray<FString> Parts;
	Arg.ParseIntoArray(Parts, TEXT(","));
	for (const FString& P : Parts)
	{
		FString Dist, Letter;
		if (!P.Split(TEXT(":"), &Dist, &Letter) || Letter.IsEmpty()) { continue; }
		for (int32 D = 0; D < NumDistricts(); ++D)
		{
			if (Dist.Equals(GV3DistrictIds[D], ESearchCase::IgnoreCase) && VariantFor(D, Letter[0]) != INDEX_NONE)
			{
				GLetters[D] = Letter[0];
			}
		}
	}
	Recompute();
	UE_LOG(LogTemp, Warning, TEXT("ANTSEED dev -AntVariant=%s: this layout was NOT validated (%s)"), *Arg, *Describe());
}

void AddZone(int32 Kind, const FVector& C, float R)
{
	EnsureInit();
	GPlayZones.Add({ Kind, (float)C.X - R, (float)C.Y - R, (float)C.Z - 60.f, (float)C.X + R, (float)C.Y + R, (float)C.Z + 300.f, R });
	RebuildZones();
}

void ClearPlayZones()
{
	GPlayZones.Reset();
	RebuildZones();
	Recompute();   // (drops the play covers too)
}

void AddCover(const FVector& C, float R, float H, const TCHAR* Name)
{
	EnsureInit();
	GCover.Add({ (float)C.X - R, (float)C.Y - R, (float)C.Z - 10.f, (float)C.X + R, (float)C.Y + R, (float)C.Z + H, Name });
}

bool PlayZoneAt(int32 Kind, const FVector& P)
{
	for (const FLevelV2Zone& Z : GPlayZones)
	{
		if (Z.Kind == Kind && P.Z >= Z.Z0 && P.Z <= Z.Z1 && FVector2D::DistSquared(FVector2D(P), FVector2D((Z.X0 + Z.X1) * 0.5f, (Z.Y0 + Z.Y1) * 0.5f)) < Z.Radius * Z.Radius)
		{
			return true;
		}
	}
	return false;
}

void RemovePlayZones(const FVector& C, float Radius)
{
	const int32 Before = GPlayZones.Num();
	GPlayZones.RemoveAll([&](const FLevelV2Zone& Z) { return FVector2D::Distance(FVector2D(C), FVector2D((Z.X0 + Z.X1) * 0.5f, (Z.Y0 + Z.Y1) * 0.5f)) < Radius; });
	if (GPlayZones.Num() != Before) { RebuildZones(); }
}

bool ShardsAt(const FVector& P)
{
	for (const FLevelV2Zone& Z : GPlayZones)
	{
		if (Z.Kind == 2 && P.Z >= Z.Z0 && P.Z <= Z.Z1 && FVector2D::DistSquared(FVector2D(P), FVector2D((Z.X0 + Z.X1) * 0.5f, (Z.Y0 + Z.Y1) * 0.5f)) < Z.Radius * Z.Radius)
		{
			return true;
		}
	}
	return false;
}

} // namespace LevelV3
