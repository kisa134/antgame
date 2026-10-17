#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "KitchenLoot.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/** Something an ant can pick up: food (carried to the nest) or gear (a new weapon). */
UCLASS()
class ANTKITCHEN_API AKitchenLoot : public AActor
{
	GENERATED_BODY()

public:
	AKitchenLoot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void InitFood(const FAntFoodDef& Def);
	void InitGear(const FAntWeapon& Weapon);

	UPROPERTY(ReplicatedUsing = OnRep_Visual) FString LootName;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) int32 Value = 5;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) int32 Weight = 1;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FLinearColor Color = FLinearColor::White;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) uint8 ShapeIndex = 1;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FVector VisualScale = FVector(0.2f);
	UPROPERTY(ReplicatedUsing = OnRep_Visual) bool bIsGear = false;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FAntWeapon Gear;

	UFUNCTION()
	void OnRep_Visual();

private:
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Beam;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* Glow;
	UPROPERTY() UMaterialInstanceDynamic* MeshMID = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* BeamMID = nullptr;

	float Phase = 0.f;
};
