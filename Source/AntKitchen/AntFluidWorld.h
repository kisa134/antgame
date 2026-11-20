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
 * The liquid of the kitchen (technology prototype, branch code/ANT-fluid). Off unless the game runs with -AntFluid or -AntFluidDemo=N.
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

	// ---- coupling to the game
	void CoupleProps(float Dt);
	void CoupleAnts(float Dt);
	void CoupleFeet(float Dt);
	TMap<TWeakObjectPtr<AActor>, FVector> LastFoot[2];
	TMap<TWeakObjectPtr<AActor>, float> WasIn;   // props: were they in liquid last tick (for the splash on entry)

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
	float FrameMs = 0.f;
	int32 Demo = 0;
	bool bDemoStarted = false;
	void LogStats();
	void AsciiMap(const AntFluid::FBasin& B, int32 Layer) const;
};
