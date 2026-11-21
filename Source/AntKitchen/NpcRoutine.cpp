#include "NpcRoutine.h"
#include "LevelV3.h"
#include "AntRaid.h"
#include "AntKitchenGameMode.h"
#include "KitchenFixture.h"
#include "KitchenSlick.h"
#include "KitchenArt.h"
#include "KitchenBuilder.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace NpcRoutine
{
namespace
{
	// docs/level/LEVEL_V3.md, section 7 (Tools/level/level_v3_layout.py ROUTINES): the spots are the designer's, checked by
	// check_level_v3 for the capsules of the Dad, grandma and the kid. Seats: where the pelvis goes (the feet stay at X, Y).
	const FStep GSteps[] =
	{
		// key           who  ru                                              X        Y       Z      yaw    secs  when          pose     arg              seat X, Y
		{ TEXT("h_phone"),  Dad, TEXT("сидит на диване в телефоне"),          11300.f,  -900.f,    0.f,  -90.f, 150.f, WAll,          PSit,  4,                11300.f,     0.f },
		{ TEXT("h_fridge"), Dad, TEXT("открыл холодильник и смотрит"),         3300.f,  2000.f,    0.f,   90.f,   8.f, WAll,          PBusy, HumanBusy::Fridge,    0.f,     0.f },
		{ TEXT("h_cook"),   Dad, TEXT("греет суп у плиты, спиной к кухне"),    -750.f,  2200.f,    0.f,   90.f,  40.f, WEve,          PBusy, HumanBusy::Stir,      0.f,     0.f },
		{ TEXT("h_eat"),    Dad, TEXT("ужинает за столом"),                       0.f, -3600.f,    0.f,   90.f,  60.f, WEve,          PSit,  3,                    0.f, -2750.f },
		{ TEXT("h_wash"),   Dad, TEXT("моет посуду, спиной к кухне"),         -3500.f,  2250.f,    0.f,   90.f,  45.f, WDay | WEve,   PBusy, HumanBusy::Wash,      0.f,     0.f },
		{ TEXT("h_bin"),    Dad, TEXT("достаёт мешок из ведра"),               6000.f, -3000.f,    0.f,   90.f,  10.f, WEve | WNight, PBusy, HumanBusy::Rummage,   0.f,     0.f },
		{ TEXT("h_trash"),  Dad, TEXT("выносит мусор (его нет дома)"),        19000.f, -1000.f,    0.f,    0.f,  45.f, WEve,          PGone, 0,                    0.f,     0.f },
		{ TEXT("h_tv"),     Dad, TEXT("смотрит телевизор с дивана"),          13500.f, -1100.f,    0.f,  -90.f, 120.f, WEve | WNight, PSit,  5,                13500.f,     0.f },
		{ TEXT("h_nap"),    Dad, TEXT("дремлет на диване"),                   12300.f, -1100.f,    0.f,  -90.f,  90.f, WNight,        PSit,  1,                12300.f,     0.f },
		{ TEXT("h_window"), Dad, TEXT("проветривает: стоит у окна"),              0.f, -4100.f,    0.f,  -90.f,  15.f, WDay,          PBusy, HumanBusy::Window,    0.f,     0.f },
		{ TEXT("g_knit"),   Gran, TEXT("вяжет в кресле"),                    10250.f, -5800.f,    0.f,    0.f, 120.f, WDay | WEve,   PKnit, 0,                    0.f,     0.f },
		{ TEXT("g_water_l"), Gran, TEXT("поливает цветы в гостиной"),         17000.f, -8000.f,    0.f,  -90.f,  20.f, WDay,          PBusy, HumanBusy::Water,     0.f,     0.f },
		{ TEXT("g_water_k"), Gran, TEXT("поливает цветок на кухне"),          -1000.f, -4100.f,    0.f,  -90.f,  15.f, WDay | WEve,   PBusy, HumanBusy::Water,     0.f,     0.f },
		{ TEXT("g_kettle"), Gran, TEXT("ставит чайник и ждёт"),                2200.f,  2300.f,    0.f,   90.f,  40.f, WEve,          PBusy, HumanBusy::Kettle,    0.f,     0.f },
		{ TEXT("g_doze"),   Gran, TEXT("дремлет в кресле"),                   10250.f, -5800.f,    0.f,    0.f,  60.f, WEve | WNight, PKnit, 1,                    0.f,     0.f },
		{ TEXT("k_train"),  Kid, TEXT("играет на ковре у железной дороги"),   14800.f, -5600.f,    0.f,  -90.f,  60.f, WDay | WEve,   PSit,  2,                    0.f,     0.f },
		{ TEXT("k_toybox"), Kid, TEXT("роется в ящике с игрушками"),           9100.f,  -200.f,    0.f,   90.f,  25.f, WDay | WEve,   PBusy, HumanBusy::Rummage,   0.f,     0.f },
		{ TEXT("k_snack"),  Kid, TEXT("перекус на детском стульчике"),         3300.f, -1800.f,    0.f,   90.f,  40.f, WDay | WEve,   PSit,  3,                 3400.f,  -500.f },
		{ TEXT("k_tv"),     Kid, TEXT("смотрит мультики"),                    13000.f, -6100.f,    0.f,  -90.f,  60.f, WEve | WNight, PSit,  5,                    0.f,     0.f },
		{ TEXT("k_draw"),   Kid, TEXT("рисует у журнального стола"),          13750.f, -4750.f,    0.f,   90.f,  40.f, WDay,          PSit,  6,                    0.f,     0.f },
		{ TEXT("k_balloon"), Kid, TEXT("дёргает шарики за ленточки"),         17000.f, -1300.f,    0.f,   50.f,  15.f, WDay | WEve,   PBusy, HumanBusy::Tug,       0.f,     0.f },
		{ TEXT("c_fridge"), Cat, TEXT("спит на холодильнике"),                 3700.f,  3900.f, 3700.f,    0.f,  60.f, WDay | WNight, PRest, 0,                    0.f,     0.f },
		{ TEXT("c_shelf"),  Cat, TEXT("спит на книжном шкафу"),               19700.f, -5000.f, 4660.f,    0.f,  60.f, WEve | WNight, PRest, 0,                    0.f,     0.f },
		{ TEXT("c_sill"),   Cat, TEXT("на подоконнике кухни"),                    0.f, -4900.f, 2200.f,    0.f,  25.f, WDay | WEve,   PRest, 2,                    0.f,     0.f },
		{ TEXT("c_sofa"),   Cat, TEXT("спит на диване"),                      15500.f,   400.f,  950.f,  -90.f,  60.f, WAll,          PRest, 0,                    0.f,     0.f },
		{ TEXT("c_post"),   Cat, TEXT("умывается на когтеточке"),              8000.f, -8300.f, 1311.f,    0.f,  45.f, WDay | WEve,   PRest, 2,                    0.f,     0.f },
		{ TEXT("c_flap"),   Cat, TEXT("у кошачьей дверцы во двор"),            7400.f, -7350.f,    0.f,    0.f,  10.f, WEve | WNight, PRest, 2,                    0.f,     0.f },
		{ TEXT("c_beg"),    Cat, TEXT("выпрашивает у Человека"),                  0.f,     0.f,    0.f,    0.f,  30.f, WEve,          PBeg,  2,                    0.f,     0.f },
		{ TEXT("d_bowl"),   Dog, TEXT("пьёт и ест из миски"),                 -3000.f, -3800.f,    0.f,    0.f,  14.f, WAll,          PRest, 1,                    0.f,     0.f },
		{ TEXT("d_beg"),    Dog, TEXT("клянчит под столом"),                    600.f, -2300.f,    0.f,    0.f,  40.f, WEve,          PBeg,  0,                    0.f,     0.f },
		{ TEXT("d_bed"),    Dog, TEXT("спит на лежанке"),                     18150.f, -7900.f,    0.f,    0.f,  45.f, WAll,          PRest, 0,                    0.f,     0.f },
		{ TEXT("d_door"),   Dog, TEXT("ждёт у двери в коридор"),              19000.f, -1800.f,    0.f,    0.f,  45.f, WEve,          PDoor, 2,                    0.f,     0.f },
		{ TEXT("d_rad_k"),  Dog, TEXT("греется у батареи кухни"),                 0.f, -4300.f,    0.f,    0.f,  30.f, WDay | WNight, PRest, 0,                    0.f,     0.f },
		{ TEXT("d_follow"), Dog, TEXT("ходит за малышом"),                        0.f,     0.f,    0.f,    0.f,  40.f, WDay | WEve,   PFollow, 2,                  0.f,     0.f },
	};

	// The order of the evening, by the time of day (keys; a step its time of day does not allow is left out).
	const TCHAR* const GOrder[NumWho][3] =
	{
		{ TEXT("h_phone h_fridge h_wash h_window h_fridge h_phone"), TEXT("h_phone h_fridge h_cook h_eat h_wash h_bin h_trash h_tv h_fridge"), TEXT("h_phone h_fridge h_bin h_tv h_nap h_fridge") },
		{ TEXT("g_knit g_water_l g_knit g_water_k"), TEXT("g_knit g_water_k g_kettle g_doze"), TEXT("g_doze g_knit") },
		{ TEXT("k_train k_toybox k_snack k_draw k_balloon"), TEXT("k_train k_toybox k_snack k_tv k_balloon"), TEXT("k_tv") },
		{ TEXT("c_fridge c_sill c_sofa c_post"), TEXT("c_shelf c_sill c_sofa c_post c_flap"), TEXT("c_fridge c_shelf c_sofa c_flap") },
		{ TEXT("d_bowl d_bed d_follow d_rad_k"), TEXT("d_bowl d_bed d_follow d_bed"), TEXT("d_bowl d_bed d_rad_k") },
	};
	const float GSpeed[NumWho] = { 430.f, 300.f, 420.f, 480.f, 400.f };   // walking between spots, for the plan's walking allowance

	TArray<FSlot> GPlan[NumWho];
	float GWarp = 0.f;
	bool GPaused = false;
	uint32 GNudge[NumWho] = { 0, 0, 0, 0, 0 };
	int32 GSkipSlot[NumWho] = { -1, -1, -1, -1, -1 };
	TArray<FMess> GMess;
	int32 GNextMess = 1;

	// The hall door stand-in (every machine).
	TWeakObjectPtr<UStaticMeshComponent> GLeaf;
	TWeakObjectPtr<UPrimitiveComponent> GLeafOrig;
	TWeakObjectPtr<UWorld> GLeafWorld;
	float GLeafOpen = 0.f;
	FTransform GLeafHome;

	uint8 WhenBit(int32 Tod) { return Tod == LevelV3::Day ? WDay : Tod == LevelV3::Night ? WNight : WEve; }

	bool WhoHome(uint8 Who)
	{
		switch (Who)
		{
		case Gran: return LevelV3::IsHome(LevelV3::Grandma);
		case Kid: return LevelV3::IsHome(LevelV3::Kid);
		case Cat: return LevelV3::IsHome(LevelV3::Cat);
		case Dog: return LevelV3::IsHome(LevelV3::Dog);
		default: return true;
		}
	}

	FVector StepPoint(const FStep& S) { return FVector(S.X, S.Y, S.Z); }

	AHumanCharacter* FindDad(const UWorld* W)
	{
		for (TActorIterator<AHumanCharacter> It(W); It; ++It)
		{
			if (It->Fig == HumanFig::Dad && !It->IsA<AHouseFolk>()) { return *It; }
		}
		return nullptr;
	}
}

bool Enabled()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("AntNoRoutines"));
	return LevelV3::Enabled() && !bOff;
}

bool Paused() { return GPaused; }
uint32 Nudge(uint8 Who) { return Who < NumWho ? GNudge[Who] : 0; }
TConstArrayView<FStep> Steps() { return MakeArrayView(GSteps); }

int32 FindStep(const TCHAR* Key)
{
	for (int32 i = 0; i < UE_ARRAY_COUNT(GSteps); ++i)
	{
		if (FCString::Stricmp(GSteps[i].Key, Key) == 0) { return i; }
	}
	return -1;
}

const TCHAR* WhoName(uint8 Who)
{
	static const TCHAR* const N[] = { TEXT("dad"), TEXT("gran"), TEXT("kid"), TEXT("cat"), TEXT("dog") };
	return Who < NumWho ? N[Who] : TEXT("?");
}

float Clock(const UWorld* W)
{
	if (!W) { return 0.f; }
	const AAntRaid* R = AAntRaid::Get(W);
	return (R && R->InRaid() ? R->RaidTime() : W->GetTimeSeconds()) + GWarp;
}

TConstArrayView<FSlot> Plan(uint8 Who) { return Who < NumWho ? TConstArrayView<FSlot>(GPlan[Who]) : TConstArrayView<FSlot>(); }

void Rebuild(const UWorld* W)
{
	const FLevelV3Seed* S = LevelV3::Current();
	const AAntRaid* Raid = W ? AAntRaid::Get(W) : nullptr;
	const int32 Tier = FMath::Clamp(Raid ? (int32)Raid->Tier : 1, 1, 4);
	const int32 Tod = LevelV3::TimeOfDay();
	const uint8 Bit = WhenBit(Tod);
	const int32 Seed = S ? S->Seed : 0;
	FRandomStream R(Seed * 7919 + Tier * 31 + Tod * 101 + 17);
	// The higher the threat, the shorter the chores and the longer the rounds between them.
	const float K[4] = { 1.f, 0.9f, 0.8f, 0.7f };
	const float Gap[4] = { 10.f, 20.f, 32.f, 45.f };
	GWarp = 0.f;
	for (uint8 Who = 0; Who < NumWho; ++Who)
	{
		GPlan[Who].Reset();
		GSkipSlot[Who] = -1;
		if (!WhoHome(Who))
		{
			UE_LOG(LogTemp, Display, TEXT("ANTRT plan %s: not home"), WhoName(Who));
			continue;
		}
		TArray<FString> Keys;
		FString(GOrder[Who][FMath::Clamp(Tod, 0, 2)]).ParseIntoArrayWS(Keys);
		TArray<int32> Order;
		for (const FString& Key : Keys)
		{
			const int32 i = FindStep(*Key);
			if (i >= 0 && (GSteps[i].When & Bit) && !(GSteps[i].Pose == PFollow && !WhoHome(Kid))) { Order.Add(i); }
		}
		if (Order.Num() == 0) { continue; }
		const int32 Start = Who == Dad ? 0 : R.RandRange(0, Order.Num() - 1);
		float T = 0.f;
		FVector Prev = StepPoint(GSteps[Order[Start]]);
		FString Line;
		for (int32 n = 0; T < 2400.f && n < 400; ++n)
		{
			const int32 i = Order[(Start + n) % Order.Num()];
			const FStep& St = GSteps[i];
			const float Walk = (St.Pose == PFollow) ? 4.f : (float)FVector::Dist2D(Prev, StepPoint(St)) / GSpeed[Who] + 4.f;
			const float Secs = St.Secs * K[Tier - 1] * ((Who == Dad && n == 0) ? 1.f : R.FRandRange(0.85f, 1.15f)) + (St.Pose == PGone ? 15.f : 0.f);
			GPlan[Who].Add({ i, T, T + Walk + Secs });
			if (Line.Len() < 700) { Line += FString::Printf(TEXT(" %.0f-%.0f %s;"), T, T + Walk + Secs, St.Key); }
			T += Walk + Secs + Gap[Tier - 1] * (Who >= Cat ? 0.3f : 1.f) * R.FRandRange(0.7f, 1.3f);
			if (St.Pose != PFollow) { Prev = StepPoint(St); }
		}
		UE_LOG(LogTemp, Display, TEXT("ANTRT plan %s (seed %d tier %d %s):%s"), WhoName(Who), Seed, Tier, LevelV3::TimeName(Tod), *Line);
	}
}

int32 Current(const UWorld* W, uint8 Who, float* OutLeft)
{
	if (OutLeft) { *OutLeft = 0.f; }
	if (!Enabled() || GPaused || Who >= NumWho || !WhoHome(Who))
	{
		return -1;
	}
	// The pets follow the Dad's evening: Бадрик begs while he eats and waits at the door while he is out; the cat begs at the stove.
	if (Who == Dog || Who == Cat)
	{
		if (const AHumanCharacter* D = FindDad(W))
		{
			const int32 DS = D->Task.Step == 255 ? -1 : (int32)D->Task.Step;
			if (GSteps[FMath::Max(0, DS)].Key && DS >= 0 && D->Task.Phase >= 2)
			{
				const FString Key = GSteps[DS].Key;
				if (Who == Dog && Key == TEXT("h_trash") && LevelV3::TimeOfDay() != LevelV3::Day) { if (OutLeft) { *OutLeft = 20.f; } return FindStep(TEXT("d_door")); }
				if (Who == Dog && Key == TEXT("h_eat")) { if (OutLeft) { *OutLeft = 20.f; } return FindStep(TEXT("d_beg")); }
				if (Who == Cat && Key == TEXT("h_cook")) { if (OutLeft) { *OutLeft = 20.f; } return FindStep(TEXT("c_beg")); }
			}
		}
	}
	const float T = Clock(W);
	const TArray<FSlot>& P = GPlan[Who];
	for (int32 s = 0; s < P.Num(); ++s)
	{
		if (T >= P[s].From && T < P[s].To)
		{
			if (GSkipSlot[Who] == s) { return -1; }
			if (OutLeft) { *OutLeft = P[s].To - T; }
			return P[s].Step;
		}
	}
	return -1;
}

void Log(const UWorld* W, uint8 Who, const TCHAR* What, int32 Step, const FVector& At)
{
	UE_LOG(LogTemp, Display, TEXT("ANTRT t=%.1f %s %s: %s at (%.0f, %.0f, %.0f)"), Clock(W), WhoName(Who), GSteps[FMath::Clamp(Step, 0, (int32)UE_ARRAY_COUNT(GSteps) - 1)].Key,
		What, At.X, At.Y, At.Z);
}

void Say(UWorld* W, uint8 Who, const FString& Line)
{
	static const TCHAR* const Names[] = { TEXT("Человек"), TEXT("Бабушка"), TEXT("Малыш"), TEXT("Кошка"), TEXT("Бадрик") };
	if (AAntKitchenGameMode* GM = W ? W->GetAuthGameMode<AAntKitchenGameMode>() : nullptr)
	{
		GM->Broadcast(FString::Printf(TEXT("%s: «%s»"), Names[FMath::Min<int32>(Who, NumWho - 1)], *Line), FLinearColor(0.95f, 0.92f, 0.8f));
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRT t=%.1f %s says «%s»"), Clock(W), WhoName(Who), *Line);
}

bool DadAway(const UWorld* W)
{
	const AHumanCharacter* D = FindDad(W);
	return D && D->Task.Phase == 3;
}

void SetHallDoor(UWorld* W, float Open)
{
	if (!W || !LevelV3::Enabled())
	{
		return;
	}
	if (GLeafWorld.Get() != W)
	{
		GLeaf = nullptr;
		GLeafOrig = nullptr;
		GLeafWorld = W;
		GLeafOpen = 0.f;
	}
	if (!GLeaf.IsValid())
	{
		if (FMath::IsNearlyZero(Open)) { return; }
		// The builder's leaf of the hall door is a static piece: a movable stand-in takes its place the first time it opens.
		UPrimitiveComponent* Orig = nullptr;
		for (TActorIterator<AKitchenBuilder> It(W); It && !Orig; ++It)
		{
			TArray<UStaticMeshComponent*> Cs;
			It->GetComponents<UStaticMeshComponent>(Cs);
			for (UStaticMeshComponent* C : Cs)
			{
				if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName() == TEXT("SM_L_Door_Leaf") && FVector::Dist2D(C->GetComponentLocation(), FVector(20148.f, -1792.f, 0.f)) < 600.f) { Orig = C; break; }
			}
		}
		UStaticMeshComponent* Src = Cast<UStaticMeshComponent>(Orig);
		if (!Src) { UE_LOG(LogTemp, Warning, TEXT("ANTRT hall door leaf not found")); return; }
		AActor* Owner = Src->GetOwner();
		UStaticMeshComponent* L = NewObject<UStaticMeshComponent>(Owner, TEXT("HallDoorLeafRt"));
		L->SetMobility(EComponentMobility::Movable);
		L->SetStaticMesh(Src->GetStaticMesh());
		for (int32 m = 0; m < Src->GetNumMaterials(); ++m) { L->SetMaterial(m, Src->GetMaterial(m)); }
		L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		L->SetupAttachment(Owner->GetRootComponent());
		L->RegisterComponent();
		L->SetWorldTransform(Src->GetComponentTransform());
		GLeafHome = Src->GetComponentTransform();
		Src->SetVisibility(false);
		GLeaf = L;
		GLeafOrig = Src;
		UE_LOG(LogTemp, Display, TEXT("ANTRT hall door: a moving leaf stands in for the static one at %s"), *GLeafHome.GetLocation().ToCompactString());
	}
	if (FMath::IsNearlyEqual(Open, GLeafOpen, 0.001f)) { return; }
	GLeafOpen = Open;
	// The leaf turns about its own origin (its hinge), into the corridor.
	const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(-95.f * FMath::InterpEaseInOut(0.f, 1.f, Open, 2.f)));
	GLeaf->SetWorldLocationAndRotation(GLeafHome.GetLocation(), Turn * GLeafHome.GetRotation());
}

// ---------------------------------------------------------------------------------------------
// Mess
// ---------------------------------------------------------------------------------------------

const TCHAR* MessName(uint8 Kind)
{
	static const TCHAR* const N[] = { TEXT("spill"), TEXT("shards"), TEXT("fallen"), TEXT("yarn"), TEXT("drawing") };
	return Kind < NumMess ? N[Kind] : TEXT("?");
}

void AddMess(UWorld* W, uint8 Kind, const FVector& Loc, AActor* Src, const FString& What)
{
	if (!Enabled() || !W || W->GetNetMode() == NM_Client)
	{
		return;
	}
	for (const FMess& M : GMess)
	{
		if (!M.bDone && M.Kind == Kind && M.Src.Get() == Src && FVector::Dist2D(M.Loc, Loc) < 400.f) { return; }
	}
	FMess M;
	M.Id = GNextMess++;
	M.Kind = Kind;
	M.Loc = Loc;
	M.Src = Src;
	M.Time = W->GetTimeSeconds();
	M.What = What;
	GMess.Add(M);
	UE_LOG(LogTemp, Display, TEXT("ANTRT t=%.1f mess %d %s: %s at %s"), Clock(W), M.Id, MessName(Kind), *What, *Loc.ToCompactString());
}

FMess* GetMess(int32 Id)
{
	for (FMess& M : GMess) { if (M.Id == Id) { return &M; } }
	return nullptr;
}

bool IsJob(const UWorld* W, uint8 Who, const FMess& M)
{
	if (M.bDone || !M.Src.IsValid())
	{
		return false;
	}
	const float Now = W ? W->GetTimeSeconds() : 0.f;
	const bool bGran = WhoHome(Gran), bKid = WhoHome(Kid);
	switch (Who)
	{
	case Gran: return M.Kind == Spill || M.Kind == Shards || M.Kind == Yarn;
	case Kid: return M.Kind == Drawing;
	case Dad: return M.Kind == Fallen || ((M.Kind == Spill || M.Kind == Shards) && (!bGran || Now - M.Time > 60.f)) || (M.Kind == Drawing && !bKid);
	default: return false;
	}
}

FMess* FindJob(const UWorld* W, uint8 Who, const FVector& From, float Range)
{
	FMess* Best = nullptr;
	float BestD = Range;
	for (FMess& M : GMess)
	{
		if (M.By >= 0 || !IsJob(W, Who, M))
		{
			continue;
		}
		const float D = (float)FVector::Dist2D(From, M.Loc);
		if (D < BestD)
		{
			BestD = D;
			Best = &M;
		}
	}
	return Best;
}

FMess* FindUnseen(const UWorld* W, uint8 Who, const FVector& From, float Range)
{
	for (FMess& M : GMess)
	{
		if (!M.bDone && Who < NumWho && !M.bSeen[Who] && FVector::Dist2D(From, M.Loc) < Range && AKitchenBuilder::Region(From) == AKitchenBuilder::Region(M.Loc))
		{
			return &M;
		}
	}
	return nullptr;
}

void Claim(int32 Id, uint8 Who) { if (FMess* M = GetMess(Id)) { M->By = (int8)Who; } }
void Release(int32 Id) { if (FMess* M = GetMess(Id)) { if (!M->bDone) { M->By = -1; } } }

void Finish(UWorld* W, int32 Id, uint8 Who)
{
	FMess* M = GetMess(Id);
	if (!M || M->bDone)
	{
		return;
	}
	M->bDone = true;
	M->By = (int8)Who;
	if (AKitchenFixture* F = Cast<AKitchenFixture>(M->Src.Get()))
	{
		F->Tidy(M->Kind == Fallen || M->Kind == Drawing || M->Kind == Yarn);
	}
	else if (AKitchenSlick* S = Cast<AKitchenSlick>(M->Src.Get()))
	{
		S->StartWipe(1.f);
	}
	// A broken jar is two jobs (the shards and the jam): one clean-up does both.
	for (FMess& O : GMess)
	{
		if (!O.bDone && O.Id != M->Id && O.Src.IsValid() && O.Src == M->Src && O.Kind != Fallen) { O.bDone = true; }
	}
	UE_LOG(LogTemp, Display, TEXT("ANTRT t=%.1f %s finished mess %d %s (%s)"), Clock(W), WhoName(Who), M->Id, MessName(M->Kind), *M->What);
}

FString Mutter(uint8 Who, uint8 Kind, int32 Salt)
{
	static const TCHAR* const DadLines[NumMess][3] =
	{
		{ TEXT("Да что ж такое... Кто разлил?"), TEXT("Опять лужа!"), TEXT("Сейчас вытру, сейчас...") },
		{ TEXT("Банка вдребезги. Ну класс."), TEXT("Кто это уронил?!"), TEXT("Осколки... Где совок?") },
		{ TEXT("Это ещё как упало?"), TEXT("Шурупы сами не выкручиваются..."), TEXT("Сейчас на место повешу.") },
		{ TEXT("Мам, у тебя вязание распустилось!"), TEXT("Нитки по всему полу..."), TEXT("Кто тут играл с клубком?") },
		{ TEXT("Рисунок упал. Сейчас приклею."), TEXT("Скотч отклеился..."), TEXT("Ну и сквозняк...") },
	};
	static const TCHAR* const GranLines[NumMess][3] =
	{
		{ TEXT("Ох, батюшки, разлили-то!"), TEXT("Сейчас тряпочкой..."), TEXT("Кто ж это так?") },
		{ TEXT("Ох, батюшки... цветочек мой!"), TEXT("Осколки! Не ходите тут!"), TEXT("Сейчас подмету.") },
		{ TEXT("Упало что-то..."), TEXT("Ох, грохот какой."), TEXT("Сейчас поправлю.") },
		{ TEXT("Кто ж мне вязание распустил..."), TEXT("Петли спустились, ох..."), TEXT("Сейчас смотаю.") },
		{ TEXT("Рисуночек упал."), TEXT("Сейчас повешу, внучек."), TEXT("Ох, сквозняк.") },
	};
	static const TCHAR* const KidLines[3] = { TEXT("Мой рисунок!"), TEXT("Эй! Это мой Бадрик!"), TEXT("Папа, рисунок упал!") };
	const int32 k = FMath::Abs(Salt) % 3;
	const uint8 Kd = FMath::Min<uint8>(Kind, NumMess - 1);
	return Who == Gran ? GranLines[Kd][k] : Who == Kid ? KidLines[k] : DadLines[Kd][k];
}

FVector ToolFor(uint8 Who, uint8 Kind, FString& OutName)
{
	auto Art = [](const TCHAR* Name, const FVector& Fallback)
	{
		const FKitchenArtInst* I = KitchenArt::Enabled() ? KitchenArt::Find(Name) : nullptr;
		return I ? FVector(I->Loc) : Fallback;
	};
	if (Who == Gran)
	{
		OutName = Kind == Spill ? TEXT("тряпку") : TEXT("веник");
		return FVector::ZeroVector;   // her own broom, her own rag (in her pocket)
	}
	if (Kind == Spill) { OutName = TEXT("полотенце"); const FVector T = Art(TEXT("SM_K_Towel"), FVector(-750.f, 2853.f, 0.f)); return FVector(T.X, FMath::Min((float)T.Y, 3000.f) - 550.f, 0.f); }
	if (Kind == Shards) { OutName = TEXT("совок и веник"); const FVector T = Art(TEXT("SM_K_Dustpan"), FVector(-6250.f, -700.f, 0.f)); return FVector(T.X + 650.f, T.Y, 0.f); }
	if (Kind == Fallen) { OutName = TEXT("отвёртку"); return FVector(2200.f, 2400.f, 0.f); }   // the top drawer by the fridge
	OutName = TEXT("скотч");
	return FVector(2200.f, 2400.f, 0.f);
}

void ResetRaid()
{
	GMess.Reset();
	GWarp = 0.f;
	for (int32 i = 0; i < NumWho; ++i) { GSkipSlot[i] = -1; }
}

// ---------------------------------------------------------------------------------------------
// Dev
// ---------------------------------------------------------------------------------------------

bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
{
	if (A.Num() == 0 || !GM)
	{
		return false;
	}
	const FString Op = A[0].ToLower(), Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString();
	UWorld* W = GM->GetWorld();
	auto WhoOf = [](const FString& S) -> int32
	{
		for (uint8 i = 0; i < NumWho; ++i) { if (S == WhoName(i)) { return i; } }
		return S == TEXT("human") ? 0 : S == TEXT("grandma") ? 1 : -1;
	};
	if (Op == TEXT("routine"))
	{
		if (Sub == TEXT("plan") || Sub == TEXT("rebuild")) { Rebuild(W); }
		else if (Sub == TEXT("warp") && A.IsValidIndex(2))
		{
			GWarp += FCString::Atof(*A[2]);
			for (int32 i = 0; i < NumWho; ++i) { ++GNudge[i]; }
			UE_LOG(LogTemp, Display, TEXT("ANTRT warp: the routine clock is at %.1f"), Clock(W));
		}
		else if (Sub == TEXT("skip") && A.IsValidIndex(2))
		{
			const int32 Who = WhoOf(A[2].ToLower());
			if (Who >= 0)
			{
				const float T = Clock(W);
				for (int32 s = 0; s < GPlan[Who].Num(); ++s)
				{
					if (T >= GPlan[Who][s].From && T < GPlan[Who][s].To) { GWarp += GPlan[Who][s].To - T + 0.1f; break; }
				}
				for (int32 i = 0; i < NumWho; ++i) { ++GNudge[i]; }
				UE_LOG(LogTemp, Display, TEXT("ANTRT skip %s: clock now %.1f"), WhoName(Who), Clock(W));
			}
		}
		else if (Sub == TEXT("goto") && A.IsValidIndex(2))
		{
			// Jump the clock to the next slot of that step: routine goto h_trash
			const int32 Step = FindStep(*A[2]);
			if (Step >= 0)
			{
				const uint8 Who = GSteps[Step].Who;
				const float T = Clock(W);
				const FSlot* Found = nullptr;
				for (const FSlot& S : GPlan[Who]) { if (S.Step == Step && S.From > T) { Found = &S; break; } }
				for (const FSlot& S : GPlan[Who]) { if (!Found && S.Step == Step) { Found = &S; } }   // (none ahead: back to the first one)
				if (Found) { GWarp += Found->From - T + 0.1f; }
				for (int32 i = 0; i < NumWho; ++i) { ++GNudge[i]; }
				UE_LOG(LogTemp, Display, TEXT("ANTRT goto %s: clock now %.1f"), GSteps[Step].Key, Clock(W));
			}
		}
		else if (Sub == TEXT("off")) { GPaused = true; for (int32 i = 0; i < NumWho; ++i) { ++GNudge[i]; } }
		else if (Sub == TEXT("on")) { GPaused = false; }
		else
		{
			for (uint8 Who = 0; Who < NumWho; ++Who)
			{
				float Left = 0.f;
				const int32 S = Current(W, Who, &Left);
				UE_LOG(LogTemp, Display, TEXT("ANTRT info t=%.1f %s: %s (%.0f s left)"), Clock(W), WhoName(Who), S >= 0 ? GSteps[S].Key : TEXT("free"), Left);
			}
		}
		return true;
	}
	if (Op == TEXT("mess"))
	{
		for (const FMess& M : GMess)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTRT mess %d %s %s at %s by %d done %d"), M.Id, MessName(M.Kind), *M.What, *M.Loc.ToCompactString(), (int32)M.By, M.bDone ? 1 : 0);
		}
		return true;
	}
	if (Op == TEXT("react") && A.IsValidIndex(1))
	{
		// react <fixture id> : the mess of what that fixture is now (fallen / broken / undone), for whoever's job it is
		if (AKitchenFixture* F = AKitchenFixture::Find(W, A[1]))
		{
			F->ReportMess();
		}
		return true;
	}
	return false;
}
}
