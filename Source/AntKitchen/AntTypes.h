#pragma once

#include "CoreMinimal.h"
#include "AntTypes.generated.h"

class AActor;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;

/** Engine basic shapes we build everything from (no custom assets needed). */
UENUM()
enum class EAntShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone
};

/** A rolled ant weapon. Looter-shooter style: rarity scales the stats. */
USTRUCT(BlueprintType)
struct FAntWeapon
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) uint8 Rarity = 0;        // 0 common .. 3 legendary
	UPROPERTY(BlueprintReadOnly) uint8 Archetype = 0;
	UPROPERTY(BlueprintReadOnly) float Damage = 12.f;
	UPROPERTY(BlueprintReadOnly) float FireRate = 4.f;     // shots per second
	UPROPERTY(BlueprintReadOnly) float ProjectileSpeed = 2600.f;
	UPROPERTY(BlueprintReadOnly) float Spread = 1.5f;      // degrees
	UPROPERTY(BlueprintReadOnly) uint8 Pellets = 1;
	UPROPERTY(BlueprintReadOnly) float Gravity = 0.15f;
	UPROPERTY(BlueprintReadOnly) float Splash = 0.f;       // radius, 0 = none
	UPROPERTY(BlueprintReadOnly) float Size = 0.12f;       // visual scale of the blob
	UPROPERTY(BlueprintReadOnly) bool bBounce = false;
};

/** A piece of food lying in the kitchen. */
struct FAntFoodDef
{
	FString Name;
	int32 Value = 5;
	int32 Weight = 1;
	FLinearColor Color = FLinearColor::White;
	EAntShape Shape = EAntShape::Sphere;
	FVector Scale = FVector(0.2f);
};

namespace AntKit
{
	ANTKITCHEN_API UStaticMesh* ShapeMesh(EAntShape Shape);
	ANTKITCHEN_API UMaterialInstanceDynamic* Paint(UStaticMeshComponent* Comp, const FLinearColor& Color);

	/** Creates, attaches and registers a colored shape. Scale is relative to a 100-unit shape. */
	ANTKITCHEN_API UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, EAntShape Shape,
		const FVector& Loc, const FVector& Scale, const FLinearColor& Color,
		const FRotator& Rot = FRotator::ZeroRotator, bool bCollision = false, bool bStatic = false);

	ANTKITCHEN_API USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Loc,
		const FRotator& Rot = FRotator::ZeroRotator);

	ANTKITCHEN_API FLinearColor RarityColor(int32 Rarity);
	ANTKITCHEN_API FString RarityName(int32 Rarity);
	ANTKITCHEN_API FAntWeapon StarterWeapon();
	ANTKITCHEN_API FAntWeapon RollWeapon(int32 Rarity, FRandomStream& Rng);
	ANTKITCHEN_API FAntFoodDef RollFood(int32 Tier, FRandomStream& Rng);
	ANTKITCHEN_API float WeaponDps(const FAntWeapon& W);
}
