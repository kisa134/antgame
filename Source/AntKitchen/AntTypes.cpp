#include "AntTypes.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "AntGameState.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

namespace AntKit
{

UStaticMesh* ShapeMesh(EAntShape Shape)
{
	static const TCHAR* Paths[] =
	{
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		TEXT("/Engine/BasicShapes/Sphere.Sphere"),
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/BasicShapes/Cone.Cone")
	};
	const int32 Index = FMath::Clamp((int32)Shape, 0, 3);
	return LoadObject<UStaticMesh>(nullptr, Paths[Index]);
}

UMaterialInstanceDynamic* Paint(UStaticMeshComponent* Comp, const FLinearColor& Color)
{
	if (!Comp)
	{
		return nullptr;
	}
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Comp);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	Comp->SetMaterial(0, MID);
	return MID;
}

UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, EAntShape Shape, const FVector& Loc,
	const FVector& Scale, const FLinearColor& Color, const FRotator& Rot, bool bCollision, bool bStatic)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetMobility(bStatic ? EComponentMobility::Static : EComponentMobility::Movable);
	C->SetStaticMesh(ShapeMesh(Shape));
	C->SetupAttachment(Parent);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	if (bCollision)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
	}
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	Paint(C, Color);
	return C;
}

USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Loc, const FRotator& Rot)
{
	USceneComponent* P = NewObject<USceneComponent>(Owner);
	P->SetMobility(EComponentMobility::Movable);
	P->SetupAttachment(Parent);
	P->SetRelativeLocationAndRotation(Loc, Rot);
	P->RegisterComponent();
	Owner->AddInstanceComponent(P);
	return P;
}

FLinearColor RarityColor(int32 Rarity)
{
	switch (Rarity)
	{
	case 1: return FLinearColor(0.10f, 0.45f, 1.00f);
	case 2: return FLinearColor(0.65f, 0.15f, 1.00f);
	case 3: return FLinearColor(1.00f, 0.55f, 0.05f);
	default: return FLinearColor(0.75f, 0.75f, 0.75f);
	}
}

FString RarityName(int32 Rarity)
{
	switch (Rarity)
	{
	case 1: return TEXT("Редкий");
	case 2: return TEXT("Эпический");
	case 3: return TEXT("Легендарный");
	default: return TEXT("Обычный");
	}
}

FAntWeapon StarterWeapon()
{
	FAntWeapon W;
	W.Name = TEXT("Кислотный плевок [Обычный]");
	return W;
}

FAntWeapon RollWeapon(int32 Rarity, FRandomStream& R)
{
	Rarity = FMath::Clamp(Rarity, 0, 3);
	FAntWeapon W;
	W.Rarity = (uint8)Rarity;
	W.Archetype = (uint8)R.RandRange(0, 4);

	FString Base;
	switch (W.Archetype)
	{
	case 0:
		Base = TEXT("Кислотный плевок");
		W.Damage = 12.f; W.FireRate = 4.f; W.ProjectileSpeed = 2600.f; W.Spread = 1.5f; W.Pellets = 1; W.Gravity = 0.15f; W.Size = 0.12f;
		break;
	case 1:
		Base = TEXT("Муравьиный дробовик");
		W.Damage = 7.f; W.FireRate = 1.3f; W.ProjectileSpeed = 2400.f; W.Spread = 7.f; W.Pellets = 6; W.Gravity = 0.2f; W.Size = 0.08f;
		break;
	case 2:
		Base = TEXT("Жвало-пулемёт");
		W.Damage = 5.f; W.FireRate = 11.f; W.ProjectileSpeed = 3200.f; W.Spread = 3.f; W.Pellets = 1; W.Gravity = 0.05f; W.Size = 0.07f;
		break;
	case 3:
		Base = TEXT("Ядовитая мортира");
		W.Damage = 45.f; W.FireRate = 0.9f; W.ProjectileSpeed = 1500.f; W.Spread = 1.f; W.Pellets = 1; W.Gravity = 1.f; W.Splash = 350.f; W.Size = 0.25f;
		break;
	default:
		Base = TEXT("Прыгучая капля");
		W.Damage = 16.f; W.FireRate = 2.5f; W.ProjectileSpeed = 2000.f; W.Spread = 2.f; W.Pellets = 1; W.Gravity = 0.8f; W.bBounce = true; W.Size = 0.15f;
		break;
	}

	const float Mult = 1.f + 0.3f * Rarity + R.FRandRange(-0.08f, 0.12f);
	W.Damage *= Mult;
	W.FireRate *= 1.f + 0.1f * Rarity + R.FRandRange(-0.05f, 0.05f);
	W.ProjectileSpeed *= 1.f + 0.05f * Rarity;

	if (Rarity >= 2 && R.FRand() < 0.35f)
	{
		W.Pellets += (W.Archetype == 1) ? 2 : 1;
	}
	if (Rarity >= 3)
	{
		if (R.FRand() < 0.5f) { W.bBounce = true; }
		else { W.Splash += 150.f; }
	}

	if (Rarity == 3)
	{
		static const TCHAR* Titles[] = { TEXT("Сахарный"), TEXT("Медовый"), TEXT("Королевский"), TEXT("Святой Крошки") };
		W.Name = FString::Printf(TEXT("%s «%s»"), *Base, Titles[R.RandRange(0, 3)]);
	}
	else
	{
		W.Name = FString::Printf(TEXT("%s [%s]"), *Base, *RarityName(Rarity));
	}
	return W;
}

static FAntFoodDef MakeFood(const TCHAR* Name, int32 Value, int32 Weight, const FLinearColor& Color, EAntShape Shape, const FVector& Scale)
{
	FAntFoodDef D;
	D.Name = Name;
	D.Value = Value;
	D.Weight = Weight;
	D.Color = Color;
	D.Shape = Shape;
	D.Scale = Scale;
	return D;
}

FAntFoodDef RollFood(int32 Tier, FRandomStream& R)
{
	switch (FMath::Clamp(Tier, 0, 3))
	{
	case 0:
		switch (R.RandRange(0, 2))
		{
		case 0: return MakeFood(TEXT("Хлебная крошка"), 5, 3, FLinearColor(0.75f, 0.55f, 0.30f), EAntShape::Sphere, FVector(0.18f));
		case 1: return MakeFood(TEXT("Крошка печенья"), 8, 4, FLinearColor(0.45f, 0.25f, 0.10f), EAntShape::Cube, FVector(0.15f));
		default: return MakeFood(TEXT("Рисинка"), 6, 2, FLinearColor(0.95f, 0.95f, 0.90f), EAntShape::Sphere, FVector(0.22f, 0.10f, 0.10f));
		}
	case 1:
		switch (R.RandRange(0, 3))
		{
		case 0: return MakeFood(TEXT("Кубик сахара"), 25, 9, FLinearColor(1.f, 1.f, 1.f), EAntShape::Cube, FVector(0.3f));
		case 1: return MakeFood(TEXT("Кусочек сыра"), 45, 22, FLinearColor(1.f, 0.80f, 0.15f), EAntShape::Cube, FVector(0.4f, 0.3f, 0.2f));
		case 2: return MakeFood(TEXT("Кошачий корм"), 20, 8, FLinearColor(0.40f, 0.20f, 0.08f), EAntShape::Sphere, FVector(0.3f));
		default: return MakeFood(TEXT("Хлопья"), 15, 5, FLinearColor(1.f, 0.55f, 0.10f), EAntShape::Sphere, FVector(0.3f, 0.3f, 0.12f));
		}
	case 2:
		switch (R.RandRange(0, 3))
		{
		case 0: return MakeFood(TEXT("Мармеладка"), 60, 12, FLinearColor(R.FRand(), 0.2f, R.FRand()), EAntShape::Sphere, FVector(0.35f));
		case 1: return MakeFood(TEXT("Капля мёда"), 50, 10, FLinearColor(1.f, 0.55f, 0.05f), EAntShape::Sphere, FVector(0.32f));
		case 2: return MakeFood(TEXT("Кусочек торта"), 140, 36, FLinearColor(1.f, 0.55f, 0.70f), EAntShape::Cube, FVector(0.5f));
		default: return MakeFood(TEXT("Чипсина"), 40, 6, FLinearColor(1.f, 0.85f, 0.30f), EAntShape::Cube, FVector(0.5f, 0.4f, 0.05f));
		}
	default:
		switch (R.RandRange(0, 2))
		{
		case 0: return MakeFood(TEXT("Золотой мармеладный мишка"), 260, 48, FLinearColor(1.f, 0.75f, 0.10f), EAntShape::Sphere, FVector(0.55f));
		case 1: return MakeFood(TEXT("Карамелька"), 120, 18, FLinearColor(0.9f, 0.10f, 0.10f), EAntShape::Sphere, FVector(0.45f));
		default: return MakeFood(TEXT("Долька шоколада"), 180, 30, FLinearColor(0.25f, 0.12f, 0.05f), EAntShape::Cube, FVector(0.6f, 0.4f, 0.15f));
		}
	}
}

FAntFoodDef HeavyFood(int32 Index)
{
	switch (((Index % 4) + 4) % 4)
	{
	case 0: return MakeFood(TEXT("Кусочек торта"), 140, 36, FLinearColor(1.f, 0.55f, 0.70f), EAntShape::Cube, FVector(0.5f));
	case 1: return MakeFood(TEXT("Золотой мармеладный мишка"), 260, 48, FLinearColor(1.f, 0.75f, 0.10f), EAntShape::Sphere, FVector(0.55f));
	case 2: return MakeFood(TEXT("Долька шоколада"), 180, 30, FLinearColor(0.25f, 0.12f, 0.05f), EAntShape::Cube, FVector(0.6f, 0.4f, 0.15f));
	default: return MakeFood(TEXT("Кусочек сыра"), 45, 22, FLinearColor(1.f, 0.80f, 0.15f), EAntShape::Cube, FVector(0.4f, 0.3f, 0.2f));
	}
}

float WeaponDps(const FAntWeapon& W)
{
	return W.Damage * (float)FMath::Max<int32>(1, W.Pellets) * W.FireRate;
}

const FAntClassDef& ClassDef(EAntClass C)
{
	static FAntClassDef Defs[4];
	static bool bInit = false;
	if (!bInit)
	{
		bInit = true;
		Defs[0] = { TEXT("Фуражир"), 14.f, 100.f, 1.0f, 620.f, 1.0f, 1.0f, 0.f, 0.f };
		Defs[1] = { TEXT("Солдат"), 20.f, 170.f, 0.85f, 560.f, 1.5f, 1.2f, 0.f, 0.f };
		Defs[2] = { TEXT("Разведчик"), 7.f, 70.f, 1.35f, 950.f, 0.85f, 0.85f, 0.f, 0.f };
		Defs[3] = { TEXT("Бочка"), 10.f, 110.f, 0.95f, 600.f, 0.8f, 1.05f, 300.f, 7.f };
	}
	return Defs[FMath::Clamp((int32)C, 0, 3)];
}

FLinearColor TeamColor(uint8 Team, int32 Index)
{
	static const FLinearColor Colony[] =
	{
		FLinearColor(0.12f, 0.04f, 0.03f), FLinearColor(0.10f, 0.20f, 0.45f), FLinearColor(0.15f, 0.40f, 0.10f), FLinearColor(0.40f, 0.10f, 0.45f),
	};
	static const FLinearColor Fire[] =
	{
		FLinearColor(0.75f, 0.12f, 0.04f), FLinearColor(0.85f, 0.25f, 0.05f), FLinearColor(0.65f, 0.08f, 0.06f), FLinearColor(0.90f, 0.35f, 0.08f),
	};
	return Team == 0 ? Colony[Index % 4] : Fire[Index % 4];
}

} // namespace AntKit

namespace AntArt
{

const FPart& Get(const TCHAR* Name)
{
	static TMap<FString, FPart> Cache;
	if (const FPart* Found = Cache.Find(Name))
	{
		return *Found;
	}
	// Generated furniture that didn't fit the kitchen (wrong proportions, odd collision):
	// the clean, textured procedural versions are used instead.
	static const TCHAR* Disabled[] = { TEXT("SM_Fridge_Body"), TEXT("SM_Fridge_Door"), TEXT("SM_Oven_Body"), TEXT("SM_Oven_Door"),
		TEXT("SM_Drawer"), TEXT("SM_CerealBox"), TEXT("SM_Cabinet_Door"), TEXT("SM_Cabinet_Base") };
	for (const TCHAR* D : Disabled)
	{
		if (FCString::Stricmp(D, Name) == 0)
		{
			return Cache.Add(Name, FPart());
		}
	}
	FPart P;
	const FString N(Name);
	const FString Short = N.StartsWith(TEXT("SM_")) ? N.Mid(3) : N;
	P.Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Meshy/%s/%s.%s"), *N, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (P.Mesh)
	{
		P.Material = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Meshy/%s/MI_%s.MI_%s"), *N, *Short, *Short), nullptr, LOAD_NoWarn | LOAD_Quiet);
		P.Bounds = P.Mesh->GetBoundingBox();
		const FVector E = P.Bounds.GetSize();
		P.LongAxis = (E.X >= E.Y && E.X >= E.Z) ? 0 : ((E.Y >= E.Z) ? 1 : 2);
	}
	return Cache.Add(N, P);
}

FQuat AxisFix(const FPart& P, bool bFlip)
{
	FQuat Q = FQuat::Identity;
	if (P.LongAxis == 1) { Q = FRotator(0.f, -90.f, 0.f).Quaternion(); }       // Y -> X
	else if (P.LongAxis == 2) { Q = FRotator(-90.f, 0.f, 0.f).Quaternion(); }  // Z -> X
	if (bFlip)
	{
		Q = FRotator(0.f, 180.f, 0.f).Quaternion() * Q;
	}
	return Q;
}

static FVector PartScale(const FPart& P, float Length, float Thickness)
{
	const FVector E = P.Bounds.GetSize().ComponentMax(FVector(0.01f));
	FVector S;
	for (int32 i = 0; i < 3; ++i)
	{
		S[i] = (i == P.LongAxis) ? Length / E[i] : Thickness / E[i];
	}
	return S;
}

void PlaceAlong(USceneComponent* C, const FPart& P, const FVector& A, const FVector& B, const FVector& Up, float Thickness, bool bFlip, bool bRelative)
{
	if (!C || !P.IsValid())
	{
		return;
	}
	const FVector D = B - A;
	const float Len = FMath::Max(1.f, (float)D.Size());
	const FQuat Q = FRotationMatrix::MakeFromXZ(D / Len, Up).ToQuat() * AxisFix(P, bFlip);
	const FVector S = PartScale(P, Len, Thickness);
	const FVector Loc = (A + B) * 0.5f - Q.RotateVector(S * P.Bounds.GetCenter());
	const FTransform T(Q, Loc, S);
	if (bRelative) { C->SetRelativeTransform(T); }
	else { C->SetWorldTransform(T); }
}

FTransform Fit(const FPart& P, const FVector& Center, float Length, bool bFlip, const FRotator& Extra)
{
	if (!P.IsValid())
	{
		return FTransform(Center);
	}
	const FVector E = P.Bounds.GetSize().ComponentMax(FVector(0.01f));
	const float S = Length / E.GetMax();
	const FQuat Q = Extra.Quaternion() * AxisFix(P, bFlip);
	return FTransform(Q, Center - Q.RotateVector(P.Bounds.GetCenter() * S), FVector(S));
}

FTransform FitBox(const FPart& P, const FVector& Min, const FVector& Max, float Yaw)
{
	const FVector Target = (Max - Min).ComponentMax(FVector(1.f));
	const FVector Center = (Min + Max) * 0.5f;
	if (!P.IsValid())
	{
		return FTransform(Center);
	}
	const FVector E = P.Bounds.GetSize().ComponentMax(FVector(0.01f));
	const int32 Quarter = ((FMath::RoundToInt(Yaw / 90.f) % 4) + 4) % 4;
	const bool bSwap = (Quarter % 2) == 1;
	// Size the mesh has to have along its own axes so that, once turned, it fills the box.
	const FVector WantLocal(bSwap ? Target.Y : Target.X, bSwap ? Target.X : Target.Y, Target.Z);
	const FVector S(WantLocal.X / E.X, WantLocal.Y / E.Y, WantLocal.Z / E.Z);
	const FQuat Q = FRotator(0.f, Quarter * 90.f, 0.f).Quaternion();
	return FTransform(Q, Center - Q.RotateVector(S * P.Bounds.GetCenter()), S);
}

void MakeSolid(UStaticMeshComponent* C)
{
	if (!C)
	{
		return;
	}
	C->SetCollisionObjectType(ECC_WorldStatic);
	C->SetCollisionResponseToAllChannels(ECR_Block);
	C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	C->bReturnMaterialOnMove = true;
}

UStaticMeshComponent* Dress(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FVector& Min, const FVector& Max, float Yaw, bool bSolid)
{
	const FPart& P = Get(Name);
	if (!P.IsValid())
	{
		return nullptr;
	}
	UStaticMeshComponent* C = AddArt(Owner, Parent, P);
	if (C)
	{
		C->SetRelativeTransform(FitBox(P, Min, Max, Yaw));
		if (bSolid)
		{
			MakeSolid(C);
		}
	}
	return C;
}

UStaticMeshComponent* DressAuto(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FVector& InMin, const FVector& InMax, float ExtraYaw, float Inflate, bool bSolid)
{
	const FPart& P = Get(Name);
	if (!P.IsValid())
	{
		return nullptr;
	}
	const FVector Min = InMin - FVector(Inflate), Max = InMax + FVector(Inflate);
	const FVector E = P.Bounds.GetSize();
	const FVector T = Max - Min;
	const bool bMeshThinX = E.X < E.Y;
	const bool bBoxThinX = T.X < T.Y;
	const float Yaw = (bMeshThinX == bBoxThinX ? 0.f : 90.f) + ExtraYaw;
	return Dress(Owner, Parent, Name, Min, Max, Yaw, bSolid);
}

void HideAll(std::initializer_list<UStaticMeshComponent*> Comps)
{
	for (UStaticMeshComponent* C : Comps)
	{
		if (C)
		{
			C->SetVisibility(false);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

UStaticMeshComponent* AddArt(AActor* Owner, USceneComponent* Parent, const FPart& P)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(P.Mesh);
	if (P.Material)
	{
		C->SetMaterial(0, P.Material);
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetupAttachment(Parent);
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}

const TCHAR* FoodArtName(const FString& LootName)
{
	if (LootName.Contains(TEXT("мишка"))) return TEXT("SM_Food_GummyBearGold");
	if (LootName.Contains(TEXT("Мармелад"))) return TEXT("SM_Food_Gummy");
	if (LootName.Contains(TEXT("сыр"))) return TEXT("SM_Food_CheeseChunk");
	if (LootName.Contains(TEXT("мёд"))) return TEXT("SM_Food_HoneyDrop");
	if (LootName.Contains(TEXT("торт"))) return TEXT("SM_Food_CakePiece");
	if (LootName.Contains(TEXT("шоколад"))) return TEXT("SM_Food_Chocolate");
	if (LootName.Contains(TEXT("сахар"))) return TEXT("SM_Food_SugarCube");
	if (LootName.Contains(TEXT("муха"))) return TEXT("SM_Food_DeadFly");
	if (LootName.Contains(TEXT("Паучья"))) return TEXT("SM_Food_SpiderLeg");
	return nullptr;
}

} // namespace AntArt

namespace AntSfx
{
static const TCHAR* Names[Count] = { TEXT("S_Spit"), TEXT("S_Splat"), TEXT("S_Hit"), TEXT("S_Crit"), TEXT("S_Kill"), TEXT("S_Grab"), TEXT("S_Drop"),
	TEXT("S_Deliver"), TEXT("S_Crunch"), TEXT("S_Stomp"), TEXT("S_Spray"), TEXT("S_Squish"), TEXT("S_Jump"), TEXT("S_Zap"), TEXT("S_Sizzle"),
	TEXT("S_Splash"), TEXT("S_Rage"), TEXT("S_Bite"), TEXT("S_Buzz"), TEXT("S_Web"), TEXT("S_Hiss"), TEXT("S_Step"), TEXT("S_Ambience"),
	TEXT("S_Swat"), TEXT("S_HumanStep"), TEXT("S_Faucet"), TEXT("S_FridgeDoor"), TEXT("S_Toaster"), TEXT("S_ArmyMarch"), TEXT("S_ArmyCharge"),
	TEXT("S_Pickup_Gear"), TEXT("S_Legendary"), TEXT("S_SpiderStep"), TEXT("S_HumanPanic") };

USoundBase* Get(uint8 Id)
{
	// Each id can have variants: S_Name, S_Name_1 .. S_Name_4 (a random one plays every time).
	static TArray<TArray<TWeakObjectPtr<USoundBase>>> Cache;
	static TArray<bool> Tried;
	if (Id >= Count)
	{
		return nullptr;
	}
	if (Cache.Num() == 0)
	{
		Cache.SetNum(Count);
		Tried.SetNumZeroed(Count);
	}
	if (!Tried[Id])
	{
		Tried[Id] = true;
		// Only real (generated/recorded) sounds live in /Game/SoundsReal; the old synth placeholders are ignored.
		auto Load = [Id](const FString& N)
		{
			const FString Path = FString::Printf(TEXT("/Game/SoundsReal/%s.%s"), *N, *N);
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				S->AddToRoot();
				Cache[Id].Add(S);
			}
		};
		Load(Names[Id]);
		for (int32 V = 1; V <= 4; ++V)
		{
			Load(FString::Printf(TEXT("%s_%d"), Names[Id], V));
		}
	}
	const TArray<TWeakObjectPtr<USoundBase>>& L = Cache[Id];
	return L.Num() > 0 ? L[FMath::RandRange(0, L.Num() - 1)].Get() : nullptr;
}

static USoundAttenuation* Falloff()
{
	static USoundAttenuation* A = nullptr;
	if (!A)
	{
		A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AntSfxFalloff"));
		A->AddToRoot();
		A->Attenuation.bAttenuate = true;
		A->Attenuation.bSpatialize = true;
		A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		A->Attenuation.AttenuationShapeExtents = FVector(500.f);
		A->Attenuation.FalloffDistance = 6500.f;
	}
	return A;
}

USoundAttenuation* Attenuation() { return Falloff(); }

void Play(const UObject* Ctx, uint8 Id, const FVector& Location, float Volume, float Pitch)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W || W->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USoundBase* S = Get(Id))
	{
		UGameplayStatics::PlaySoundAtLocation(W, S, Location, Volume, Pitch * FMath::FRandRange(0.93f, 1.07f), 0.f, Falloff());
	}
}

void Play2D(const UObject* Ctx, uint8 Id, float Volume, float Pitch)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W || W->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USoundBase* S = Get(Id))
	{
		UGameplayStatics::PlaySound2D(W, S, Volume, Pitch);
	}
}

void Everyone(const UObject* Ctx, uint8 Id, const FVector& Location, float Pitch)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (AAntGameState* GS = W ? W->GetGameState<AAntGameState>() : nullptr)
	{
		GS->MulticastSfx(Id, Location, Pitch);
	}
}
}
