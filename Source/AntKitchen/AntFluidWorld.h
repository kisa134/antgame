#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AntFluidSim.h"
#include "AntFluidWorld.generated.h"

class UProceduralMeshComponent;
class UTexture2D;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class AAntCharacter;
class ACameraActor;
struct FAntEnv;

/** What one layer of one basin looks like on this machine: a displaced grid and the two textures the simulation is written into. */
USTRUCT()
struct FAntFluidLayerView
{
	GENERATED_BODY()
	UPROPERTY() UProceduralMeshComponent* Mesh = nullptr;
	UPROPERTY() UTexture2D* TexA = nullptr;
	UPROPERTY() UTexture2D* TexB = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* MID = nullptr;
	int32 TilesX = 0, TilesY = 0;
	TArray<uint8> TileShown;
	FIntRect Uploaded = FIntRect(0, 0, 0, 0);   // the cells the textures hold liquid in right now
	bool bShown = false;
};

/** A bounded 3D simulation (Niagara Fluids FLIP, on the GPU) where the action is. Cosmetic and local. */
USTRUCT()
struct FAntFluidVolume
{
	GENERATED_BODY()
	UPROPERTY() UNiagaraComponent* Comp = nullptr;
	FName Id;
	FBox Box = FBox(ForceInit);
	int32 Kind = 0;        // 0 hose, 1 pool, 2 splash
	float DieAt = -1.f;    // world time; < 0 = until removed
};

/** Layer C: a drop. At the scale of an ant water is ruled by surface tension: a drop is a cohesive ball. */
struct FAntDrop
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	float Radius = 10.f;           // of the sphere with the same volume
	uint8 Liquid = AntFluid::Water;
	uint8 State = 0;               // 0 falling, 1 resting / rolling on a surface, 2 hanging (growing at the tap)
	float Flat = 0.f;              // 0 a ball .. 1 a sessile drop pressed on the surface
	float Wobble = 0.f, WobbleVel = 0.f;
	float Age = 0.f;
	float RestZ = 0.f;             // the surface it lies on
	TWeakObjectPtr<AAntCharacter> Trapped;
	float TrapTime = 0.f;
	int32 Id = 0;
};

/**
 * The liquid of the kitchen (docs/FEATURES.md, section 29). On by default since v12; -AntNoFluid turns it off.
 *
 * Layer A (AntFluidSim): per basin a stack of shallow layers over a bed rasterised from the level's collision. The server steps it
 *   with a fixed step; gameplay reads depth and velocity from it (buoyancy and drag on physics props, the current on ants, wet / sticky).
 * Layer B: what a player sees. (1) a displaced, refracting surface per basin and layer, fed by the simulation every frame;
 *   (2) bounded 3D FLIP simulations (Niagara Fluids) placed where layer A says liquid falls or is hit.
 * Layer C: drops (cohesive balls): hang, fall, bead, roll, merge, split, trap an ant.
 */
UCLASS()
class ANTKITCHEN_API UAntFluidWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool Wanted();
	static UAntFluidWorld* Get(const UObject* Ctx);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAntFluidWorld, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate() && Wanted(); }

	/** "fluid ..." from the -AntDo script (AAntKitchenGameMode::DevCommand). */
	bool DevCommand(const TArray<FString>& A);
	/** Server, 4x per second per ant (AKitchenBuilder::EnvironmentFor): wet, sticky, drowning. */
	void AffectAnt(const AAntCharacter* Ant, FAntEnv& Env) const;

	/** All liquid at a point: the highest basin whose surface is above Z - Tolerance... or just under the point. */
	bool SampleAt(const FVector& P, AntFluid::FSample& Out, int32* OutBasin = nullptr) const;
	/** Server: pour Volume (uu^3) of a liquid at a point; it lands in the basin under it. */
	void Spill(AntFluid::ELiquid Liquid, const FVector& At, float Volume, float Radius = 40.f, const FVector2f& Vel = FVector2f::ZeroVector);
	/** Server: a drop appears. */
	int32 AddDrop(const FVector& At, float Radius, AntFluid::ELiquid Liquid = AntFluid::Water, const FVector& Vel = FVector::ZeroVector, uint8 State = 0);

	bool IsReady() const { return bReady; }
	int32 Quality = 2;   // 0 off (nothing is drawn or simulated in 3D; layer A still runs), 1 low, 2 medium, 3 high

	// ---- the game's water (v12: on by default, -AntNoFluid turns it off)
	/** The water of this world, if it is on and built. */
	static UAntFluidWorld* Active(const UObject* Ctx);
	/** Server: a pour that keeps coming (a bottle, a leak): Litres per second for Seconds. */
	void AddSource(AntFluid::ELiquid Liquid, const FVector& At, float LitresPerSecond, float Seconds, float Radius = 40.f);
	/** Server: cola + mentos: a fountain of drops out of a bottle at At for Seconds, the pool under it. */
	void Geyser(const FVector& At, float Seconds, AntFluid::ELiquid Liquid = AntFluid::Cola);
	/** Server: the tap gushes (Litres per second) for Seconds, then runs as usual. */
	void Flood(float Seconds, float LitresPerSecond = 30.f);
	/** Wet enough under P for a shoe to lose its grip (water film or an oil film). */
	bool IsSlippery(const FVector& P) const;
	/** Depth of all liquid at P (0 = dry). */
	float DepthAt(const FVector& P) const;
	/** Server: a towel or a mop takes the liquid in a disc away (about 6 uu of depth a second). */
	void Mop(const FVector& At, float Radius, float Dt);
	/** Server: dry everything, the tap off (a new raid: AKitchenFixture::ResetAll). */
	static void ResetAll(UWorld* World);
	void Dry();
	/** Litres on the floor / on the worktop right now. */
	float LitresIn(int32 Basin) const;
	FString Describe() const;

private:
	// ---- layer A
	TArray<TUniquePtr<AntFluid::FBasin>> Basins;
	void BuildBasins();
	void Rasterise(AntFluid::FBasin& B, float ProbeUp, float ProbeDown);
	void StepSim(float Dt);
	void HandleSpills(int32 BasinIndex);
	int32 BasinUnder(const FVector& P, int32 Skip = -1) const;
	struct FSource { FName Id; FVector Pos; uint8 Liquid = 0; float Rate = 0.f; float Radius = 30.f; bool bOn = false; float Left = -1.f; };
	TArray<FSource> Sources;
	struct FFall { FVector From, To; float T = 0.f, Dur = 0.f; float Volume = 0.f; uint8 Liquid = 0; FVector2f Vel; int32 ToBasin = -1; };
	TArray<FFall> Falls;
	/** Where liquid is pouring over an edge or out of a tap right now (smoothed): the 3D layer is placed by these. */
	struct FColumn { FVector Top, Bottom; float Rate = 0.f; float Seen = 0.f; uint8 Liquid = 0; FVector2f Vel; };
	TArray<FColumn> Columns;
	void NoteColumn(const FVector& Top, const FVector& Bottom, float Volume, uint8 Liquid, const FVector2f& Vel);
	double Accum = 0.;
	int32 MaxSteps = 3;
	bool bReady = false, bFrozen = false, bTriedBuild = false;
	float SimMs = 0.f, ViewMs = 0.f, CoupleMs = 0.f, DropMs = 0.f;
	double LostVolume = 0.;
	float StatAt = 0.f;
	TWeakObjectPtr<class AKitchenFaucet> Faucet;

	// ---- the game (server): the tap fixture, drains, the stains it washes, the Human's reaction, geysers
	void TickGame(float Dt);
	struct FGeyserJet { FVector At; float Left = 0.f; float Acc = 0.f; uint8 Liquid = 1; int32 Seed = 0; };
	TArray<FGeyserJet> Geysers;
	float TapRate = 5.f;           // litres per second of the open tap
	uint8 TapLiquid = 0;
	float SinkDrain = 1.2f;        // litres per second the sink's plughole takes (a plate half over it)
	float FloodLeft = 0.f, FloodRate = 30.f;
	float GameClock = 0.f, WashClock = 0.f, NextYell = 0.f;
	TMap<uint16, float> StainWet;  // mess stain id -> seconds under water
	bool bDebug = false;
	float DebugAt = 0.f;

	// ---- coupling to the game
	void CoupleProps(float Dt);
	void CoupleAnts(float Dt);
	void CoupleFeet(float Dt);
	TMap<TWeakObjectPtr<AActor>, FVector> LastFoot[2];
	TMap<TWeakObjectPtr<AActor>, float> WasIn;
	TWeakObjectPtr<AActor> LastProp;
	UPROPERTY() class UPhysicalMaterial* OilMat = nullptr;      // what a thing lying on an oil film slides on
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<class UPhysicalMaterial>> Oiled;
	TMap<TWeakObjectPtr<AAntCharacter>, FVector2f> AntGrip;    // an ant's own ground friction and braking, while it is on oil   // props: were they in liquid last tick (for the splash on entry)

	// ---- layer B: the surface
	UPROPERTY() AActor* Holder = nullptr;
	UPROPERTY() TArray<FAntFluidLayerView> Views;   // basin * LCount + layer
	UPROPERTY() UMaterialInterface* LiquidMat = nullptr;
	UPROPERTY() UMaterialInterface* DropMat = nullptr;
	void BuildViews();
	void UpdateViews();
	void UpdateLayerView(int32 BasinIndex, int32 L);
	TArray<float> FieldA, FieldB;

	// ---- layer B: 3D volumes
	UPROPERTY() TArray<FAntFluidVolume> Volumes;
	UPROPERTY() UNiagaraSystem* FlipHose = nullptr;
	UPROPERTY() UNiagaraSystem* FlipPool = nullptr;
	UPROPERTY() UNiagaraSystem* FlipSplash = nullptr;
	UNiagaraComponent* AddVolume(FName Id, int32 Kind, const FBox& Box, int32 Cells, float Life, float WaterHeight = 0.f);
	void RemoveVolume(FName Id);
	void TickVolumes(float Dt);
	void DumpNiagara(UNiagaraSystem* Sys) const;

	// ---- layer C
	TArray<FAntDrop> Drops;
	int32 NextDropId = 1;
	UPROPERTY() UInstancedStaticMeshComponent* DropMesh = nullptr;
	void TickDrops(float Dt);
	void DrawDrops();
	UPROPERTY() UInstancedStaticMeshComponent* StreamMesh = nullptr;
	void DrawStreams();
	float TapDripAt = -1.f;
	bool bDrip = false;

	// ---- dev
	UPROPERTY() ACameraActor* DevCam = nullptr;
	void RunDemo(int32 N);
	TArray<TPair<float, FString>> DemoScript;
	float DemoT0 = 0.f;
	float FrameMs = 0.f, GpuMs = 0.f;
	int32 Demo = 0;
	bool bDemoStarted = false;
	void LogStats();
	void AsciiMap(const AntFluid::FBasin& B, int32 Layer) const;
};
