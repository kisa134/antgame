#include "AntTypes.h"
#include "Engine/StaticMesh.h"
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
		case 0: return MakeFood(TEXT("Хлебная крошка"), 5, 1, FLinearColor(0.75f, 0.55f, 0.30f), EAntShape::Sphere, FVector(0.18f));
		case 1: return MakeFood(TEXT("Крошка печенья"), 8, 1, FLinearColor(0.45f, 0.25f, 0.10f), EAntShape::Cube, FVector(0.15f));
		default: return MakeFood(TEXT("Рисинка"), 6, 1, FLinearColor(0.95f, 0.95f, 0.90f), EAntShape::Sphere, FVector(0.22f, 0.10f, 0.10f));
		}
	case 1:
		switch (R.RandRange(0, 3))
		{
		case 0: return MakeFood(TEXT("Кубик сахара"), 25, 2, FLinearColor(1.f, 1.f, 1.f), EAntShape::Cube, FVector(0.3f));
		case 1: return MakeFood(TEXT("Кусочек сыра"), 35, 3, FLinearColor(1.f, 0.80f, 0.15f), EAntShape::Cube, FVector(0.4f, 0.3f, 0.2f));
		case 2: return MakeFood(TEXT("Кошачий корм"), 20, 2, FLinearColor(0.40f, 0.20f, 0.08f), EAntShape::Sphere, FVector(0.3f));
		default: return MakeFood(TEXT("Хлопья"), 15, 1, FLinearColor(1.f, 0.55f, 0.10f), EAntShape::Sphere, FVector(0.3f, 0.3f, 0.12f));
		}
	case 2:
		switch (R.RandRange(0, 3))
		{
		case 0: return MakeFood(TEXT("Мармеладка"), 60, 2, FLinearColor(R.FRand(), 0.2f, R.FRand()), EAntShape::Sphere, FVector(0.35f));
		case 1: return MakeFood(TEXT("Капля мёда"), 50, 2, FLinearColor(1.f, 0.55f, 0.05f), EAntShape::Sphere, FVector(0.32f));
		case 2: return MakeFood(TEXT("Кусочек торта"), 90, 5, FLinearColor(1.f, 0.55f, 0.70f), EAntShape::Cube, FVector(0.5f));
		default: return MakeFood(TEXT("Чипсина"), 40, 1, FLinearColor(1.f, 0.85f, 0.30f), EAntShape::Cube, FVector(0.5f, 0.4f, 0.05f));
		}
	default:
		switch (R.RandRange(0, 2))
		{
		case 0: return MakeFood(TEXT("Золотой мармеладный мишка"), 200, 5, FLinearColor(1.f, 0.75f, 0.10f), EAntShape::Sphere, FVector(0.55f));
		case 1: return MakeFood(TEXT("Карамелька"), 140, 3, FLinearColor(0.9f, 0.10f, 0.10f), EAntShape::Sphere, FVector(0.45f));
		default: return MakeFood(TEXT("Долька шоколада"), 160, 4, FLinearColor(0.25f, 0.12f, 0.05f), EAntShape::Cube, FVector(0.6f, 0.4f, 0.15f));
		}
	}
}

float WeaponDps(const FAntWeapon& W)
{
	return W.Damage * (float)FMath::Max<int32>(1, W.Pellets) * W.FireRate;
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
	return nullptr;
}

} // namespace AntArt
