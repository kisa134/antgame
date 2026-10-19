#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "KitchenBuilder.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class AAntCharacter;
struct FAntEnv;

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

	/** Team 0: our colony (west baseboard). Team 1: rival fire ants (east, by the trash). */
	static FVector NestLocation(uint8 Team = 0);
	static float NestRadius(uint8 Team = 0) { return Team == 0 ? 650.f : 560.f; }
	/** Where the big prizes lie: they need several ants. */
	static void HeavySpots(TArray<FVector>& Out);
	static FVector HumanSpawn();
	/** The spider's web: high on the east wall, above the fire ants' nest. */
	static FVector SpiderLair() { return FVector(6980.f, 2600.f, 4300.f); }
	static FVector HumanFleePoint();
	static FBox HotBurner();
	static FBox PlayArea();
	/** A random walkable point on the floor of the kitchen or the living room. */
	static FVector RandomFloorPoint(FRandomStream* Rng = nullptr);
	/** The doorway between the kitchen and the living room. */
	static FVector Doorway() { return FVector(7100.f, -4000.f, 0.f); }
	/** Next point to walk to on the way From -> To (goes through the doorway between the rooms). */
	static FVector Route(const FVector& From, const FVector& To);
	static void LootSpots(TArray<FVector>& OutLocations, TArray<int32>& OutTiers);

	/** Static kitchen zones (burner, honey, jam, water bowls...) acting on an ant. */
	static void EnvironmentFor(const AAntCharacter* Ant, FAntEnv& Env);

	/** Gives a shape the real textured surface for its palette color (wood, tiles, steel...). */
	static void Surface(UStaticMeshComponent* C, const FLinearColor& Color);

	/** Ceiling lamps on/off (light switch). */
	void SetMainLights(bool bOn);

private:
	void Build();
	UStaticMeshComponent* Part(EAntShape Shape, const FVector& Center, const FVector& Size, const FRotator& Rot, const FLinearColor& Color, bool bCollide);
	UStaticMeshComponent* Box(const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bCollide = true);
	UStaticMeshComponent* Cyl(const FVector& Base, float Radius, float Height, const FLinearColor& Color, bool bCollide = true);
	UStaticMeshComponent* Ball(const FVector& Center, const FVector& Radii, const FLinearColor& Color);
	void Doors(float X0, float X1, int32 Count, const FLinearColor& DoorColor, const FLinearColor& HandleColor);
	void Chair(const FVector& Center, bool bBackSouth, const FLinearColor& Color);
	UPointLightComponent* Lamp(const FVector& Loc, float Intensity, const FLinearColor& Color, float Radius, bool bShadows);
	void BuildNorthRun();
	void BuildFridge();
	void BuildSecrets();
	void BuildLivingRoom();

	UPROPERTY()
	USceneComponent* Root = nullptr;

	UPROPERTY() TArray<UPointLightComponent*> MainLamps;
	UPROPERTY() TArray<UStaticMeshComponent*> LampShades;
	UPROPERTY() UDirectionalLightComponent* Fill = nullptr;
	UPROPERTY() UPointLightComponent* WindowLight = nullptr;

	float HazardTimer = 0.f;
	/** Nest mounds grow as each colony fills its stores (index = team). */
	UPROPERTY() TArray<UStaticMeshComponent*> Mounds0;
	UPROPERTY() TArray<UStaticMeshComponent*> Mounds1;
	TArray<UStaticMeshComponent*>& Mounds(int32 Team) { return Team == 0 ? Mounds0 : Mounds1; }
	TArray<FVector> MoundBaseScale[2];
	TArray<FVector> MoundBaseLoc[2];
	float MoundGrowth[2] = { 0.f, 0.f };
	void AddMound(int32 Team, UStaticMeshComponent* C);
	void UpdateMounds(float Dt);
};
