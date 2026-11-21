#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include <initializer_list>
#include "KitchenBuilder.generated.h"

class UStaticMeshComponent;
struct FKitchenArtInst;
class UPointLightComponent;
class UDirectionalLightComponent;
class USpotLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UMaterialInstanceDynamic;
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
	static float NestRadius(uint8 Team = 0) { return Team == 0 ? 950.f : 560.f; }
	/** Where a carrier of this colony standing at From takes its load. Level v2: from the yard our ants use the south entrance
	 *  of the nest at the house corner (LevelV2::SouthEntrance), from everywhere else the nest itself. */
	static FVector HomeFor(uint8 Team, const FVector& From);
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
	/** Next point to walk to on the way From -> To (through the doorway / out the window into the yard). */
	static FVector Route(const FVector& From, const FVector& To, bool bFlying = false);
	/** 0 kitchen, 1 living room, 2 back yard (south), 3 the west lawn / sidewalk / street (the colony's nest is there). */
	static int32 Region(const FVector& P);
	/** A random point on the lawn outside the window. */
	static FVector RandomYardPoint(FRandomStream* Rng = nullptr);
	/** Dog poop on the lawn / the outdoor trash bin (fly food). */
	static FVector YardPoop();
	static FVector YardBin();
	static void LootSpots(TArray<FVector>& OutLocations, TArray<int32>& OutTiers);

	/** Static kitchen zones (burner, honey, jam, water bowls...) acting on an ant. */
	static void EnvironmentFor(const AAntCharacter* Ant, FAntEnv& Env);

	/** Gives a shape the real textured surface for its palette color (wood, tiles, steel...). */
	static void Surface(UStaticMeshComponent* C, const FLinearColor& Color);

	/** Level v2: makes a big walker's capsule pass through the "ants only" pieces (the yarn bridge, the garland, the chili
	 *  string, the fly ribbon). False while the kitchen is not built yet on this machine: call again. */
	static bool LetThrough(class ACharacter* Big);
	/** The first placed piece of the modelled flat with this mesh (its component), or null. */
	static UPrimitiveComponent* FindArt(const UWorld* World, const TCHAR* MeshName);
	/** Level v3: puts the districts of the current seed (LevelV3::Current) into the world, on this machine. */
	static void ApplyLevelV3(UWorld* World);
	void ApplyV3Layout();
	/** Level v3: the component of base row Inst (null if a fixture builds it, or it is not a static piece). */
	UStaticMeshComponent* ArtComponent(int32 Inst) const { return ArtByInst.IsValidIndex(Inst) ? ArtByInst[Inst] : nullptr; }
	/** Level v3: the climb stand-in of curtain row Inst (null if none). */
	UStaticMeshComponent* CurtainProxy(int32 Inst) const { UStaticMeshComponent* const* P = CurtainProxies.Find(Inst); return P ? *P : nullptr; }
	TMap<int32, UStaticMeshComponent*> CurtainProxies;

	/** Ceiling lamps on/off (light switch). */
	void SetMainLights(bool bOn);

	/** Time of day for a shift: 0 morning, 1 golden evening, 2 night with the lamps on. Cycles. */
	static int32 TimeOfDayFor(int32 Shift) { return ((FMath::Max(1, Shift) - 1) % 3); }

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
	void BuildYard();
	void BuildShell();
	/** A solid box with holes cut out of it (walls with cracks, windows, cat flaps). */
	void SolidBox(const FBox& B, std::initializer_list<FBox> Cavities, const FLinearColor& Color);
	/** Level v2: invisible walls round the yards and the street. */
	void BuildEdges();
	void BuildDucts();
	void BuildWestStreet();
	void BuildClutter();
	void BuildAppliances();
	/** The modelled kitchen (KitchenArt): every fixed piece at its manifest placement. The box-built versions of the same
	 *  things are skipped (bArt); -AntOldKitchen keeps them instead. */
	void BuildArt();
	/** Modelled grass: tufts instead of the given single blades (every third one). False if the models are missing. */
	bool ScatterTufts(const TArray<FTransform>& Blades, int32 Seed);
	bool bArt = false;
	bool bBuilt = false;
	UPROPERTY() TArray<UPrimitiveComponent*> AntsOnlyParts;
	// level v3
	void MakeAntsOnly(UStaticMeshComponent* C);
	UStaticMeshComponent* AddV3Piece(const FKitchenArtInst& I, uint8 Flags, const TCHAR* Tint);
	void SetArtShown(int32 Inst, bool bShown);
	UPROPERTY() TArray<UStaticMeshComponent*> ArtByInst;
	UPROPERTY() TArray<UStaticMeshComponent*> V3Extra;
	TArray<int32> V3Hidden;
	TMap<int32, FTransform> V3Moved;

	/** Air moving through a duct: ants inside are carried along. */
	struct FDuctWind { FBox Box; FVector Dir; };
	TArray<FDuctWind> DuctWind;

	UPROPERTY()
	USceneComponent* Root = nullptr;

	UPROPERTY() TArray<UPointLightComponent*> MainLamps;
	TArray<float> MainLampBase;                              // their full intensity (same order)
	UPROPERTY() TArray<UStaticMeshComponent*> LampShades;
	UPROPERTY() UDirectionalLightComponent* Fill = nullptr;   // the sun / the moon
	UPROPERTY() UPointLightComponent* WindowLight = nullptr;
	UPROPERTY() USpotLightComponent* Pendant = nullptr;       // the lamp over the dinner table
	UPROPERTY() UMaterialInstanceDynamic* PendantBulb = nullptr;
	UPROPERTY() UPointLightComponent* HoodLight = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* HoodBulb = nullptr;
	/** Shadowless fills standing in for the light the room bounces around: from above the ceiling (floors under furniture never go black) and from under the floor (undersides of tables, lower walls). */
	UPROPERTY() TArray<UPointLightComponent*> AmbFills;
	UPROPERTY() TArray<UPointLightComponent*> BounceFills;
	UPROPERTY() USkyLightComponent* SkyFill = nullptr;
	UPROPERTY() UExponentialHeightFogComponent* Haze = nullptr;
	UPROPERTY() UPostProcessComponent* TodGrade = nullptr;

	/** Everything the time of day changes. Blended, so a new shift fades into its light. */
	struct FTimeOfDay
	{
		FRotator SunRot = FRotator(-32.f, 82.f, 0.f);
		FLinearColor SunColor = FLinearColor::White;
		float SunLux = 8.f;
		float SunShafts = 2.f;        // how strongly the sun lights the dust in the air
		float Sky = 1.f;
		float Lamps = 0.3f;           // ceiling lamps, 0..1 of their full intensity
		float Practicals = 0.5f;      // the pendant over the table and the hood light
		float Amb = 0.35f;            // fill from above
		FLinearColor AmbColor = FLinearColor::White;
		float Bounce = 0.25f;         // fill from below (what the floor throws back up)
		FLinearColor BounceColor = FLinearColor::White;
		FLinearColor WindowColor = FLinearColor::White;
		float Window = 2.f;
		float FogDensity = 0.004f;
		float ExposureBias = 0.f;
		float WhiteTemp = 6700.f;
		float FxAmbient = 1.f;        // brightness of unlit dust / steam
		FLinearColor FxTint = FLinearColor::White;
	};
	static FTimeOfDay TimeOfDay(int32 Index);
	static FTimeOfDay Blend(const FTimeOfDay& A, const FTimeOfDay& B, float T);
	void UpdateTimeOfDay(float Dt);
	void ApplyLights();
	FTimeOfDay Tod;
	int32 TodIndex = -1;         // what Tod is heading for
	int32 ForcedTod = -1;        // -AntTime=0|1|2
	float TodBlend = 1.f;
	FTimeOfDay TodFrom;
	bool bMainOn = true;
	float DripTimer = 1.f;       // cosmetic: the drip into the puddle behind the fridge
	float DripRipple = -1.f;
	float SprinkleTimer = 0.f;   // cosmetic: the yard sprinkler's jet (level v2)

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
