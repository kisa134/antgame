#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "KitchenBuilder.generated.h"

class UStaticMeshComponent;

/**
 * Builds the whole kitchen out of engine basic shapes.
 * Spawned locally on every machine (server and clients) by the GameState,
 * so the geometry is identical everywhere without replicating hundreds of meshes.
 *
 * Scale: the Human is ~3400 units tall, so 1 m of real kitchen ~ 1900 units.
 * X: west(-) -> east(+), Y: south(-) -> north(+), floor at Z = 0.
 */
UCLASS()
class ANTKITCHEN_API AKitchenBuilder : public AActor
{
	GENERATED_BODY()

public:
	AKitchenBuilder();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static FVector NestLocation();
	static float NestRadius() { return 650.f; }
	static FVector HumanSpawn();
	static FVector HumanFleePoint();
	static FBox HotBurner();
	static FBox PlayArea();
	static void LootSpots(TArray<FVector>& OutLocations, TArray<int32>& OutTiers);

private:
	void Build();
	UStaticMeshComponent* Part(EAntShape Shape, const FVector& Center, const FVector& Size, const FRotator& Rot, const FLinearColor& Color, bool bCollide);
	UStaticMeshComponent* Box(const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bCollide = true);
	UStaticMeshComponent* Cyl(const FVector& Base, float Radius, float Height, const FLinearColor& Color, bool bCollide = true);
	UStaticMeshComponent* Ball(const FVector& Center, const FVector& Radii, const FLinearColor& Color);
	void Doors(float X0, float X1, int32 Count, const FLinearColor& DoorColor, const FLinearColor& HandleColor);
	void Chair(const FVector& Center, bool bBackSouth, const FLinearColor& Color);
	void Lamp(const FVector& Loc, float Intensity, const FLinearColor& Color, float Radius, bool bShadows);

	UPROPERTY()
	USceneComponent* Root = nullptr;

	float HazardTimer = 0.f;
};
