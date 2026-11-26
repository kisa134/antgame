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

/** A pour the server runs (a bottle, a hose, a puddle running out): every client runs the same one. */
USTRUCT()
struct FAntFluidPour
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize10 Pos = FVector::ZeroVector;
	UPROPERTY() float Rate = 0.f;      // uu^3 per second
	UPROPERTY() float Radius = 30.f;
	UPROPERTY() uint8 Liquid = 0;
	bool operator==(const FAntFluidPour& O) const { return Pos == O.Pos && Rate == O.Rate && Radius == O.Radius && Liquid == O.Liquid; }
};

/**
 * The water over the network. Every machine runs the same deterministic flow from the same inputs (the tap, the pours, the big
 * spills, the geysers); the server's state, coarsened and compressed, is sent round every second (one basin at a time) and pulls
 * each client back where it drifted. A late joiner takes the first picture whole.
 */
UCLASS()
class ANTKITCHEN_API AAntFluidNet : public AActor
{
	GENERATED_BODY()

public:
	AAntFluidNet();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) float TapRate = 0.f;   // uu^3/s while the faucet runs
	UPROPERTY(Replicated) uint8 TapLiquid = 0;
	UPROPERTY(Replicated) TArray<FAntFluidPour> Pours;

	UFUNCTION(NetMulticast, Reliable) void MulticastSnap(uint8 Basin, uint16 Seq, uint8 Part, uint8 Parts, const TArray<uint8>& Data);
	UFUNCTION(NetMulticast, Reliable) void MulticastGeyser(FVector_NetQuantize At, float Seconds, uint8 Liquid);
	UFUNCTION(NetMulticast, Reliable) void MulticastSpill(uint8 Liquid, FVector_NetQuantize At, float Volume, float Radius);
	UFUNCTION(NetMulticast, Reliable) void MulticastDry();
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
	/** Server: takes Depth (uu) of one layer (AntFluid::ELayer) in a disc: the oil a fire burns up, the water that freezes (AntMatter). */
	void DrainLayer(const FVector& At, float Radius, uint8 Layer, float Depth);
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
	/** BasinUnder, or one a little to the side (P is moved there). */
	int32 BasinUnderNear(FVector& P, int32 Skip = -1) const;
	/** AddVolume; what lands on a wall cell runs to the nearest open one. Returns what still did not fit. */
	float AddNear(AntFluid::FBasin& T, AntFluid::ELiquid Liquid, float X, float Y, float Radius, float Volume, const FVector2f& Vel = FVector2f::ZeroVector);
	struct FSource { FName Id; FVector Pos; uint8 Liquid = 0; float Rate = 0.f; float Radius = 30.f; bool bOn = false; float Left = -1.f; };
	TArray<FSource> Sources;
	struct FFall { FVector From, To; float T = 0.f, Dur = 0.f; float Volume = 0.f; uint8 Liquid = 0; FVector2f Vel; int32 ToBasin = -1; };
	TArray<FFall> Falls;
	TMap<int64, FFall> Pending;   // falls being gathered over a few steps (one lump per landing spot)
	struct FLandCache { int32 To = -1; float X = 0.f, Y = 0.f, Surface = 0.f; uint32 Step = 0; };
	TMap<int64, FLandCache> LandCache;   // edge cell -> where its liquid lands
	int32 PendingSteps = 0;
	void FlushPending();
	/** Where liquid is pouring over an edge or out of a tap right now (smoothed): the 3D layer is placed by these. */
	struct FColumn { FVector Top, Bottom; float Rate = 0.f; float Seen = 0.f; uint8 Liquid = 0; FVector2f Vel; };
	TArray<FColumn> Columns;
	void NoteColumn(const FVector& Top, const FVector& Bottom, float Volume, uint8 Liquid, const FVector2f& Vel);
	double Accum = 0.;
	uint32 SimStep = 0;
	bool bAllowHalf = false;       // fluid half on|off: a basin mostly on the move steps at 30 Hz (off: it held water back on the worktop)
	double SpilledVol = 0.;        // stats: what went over edges since the last stats line
	float SpillStatAt = 0.f;
	int32 MaxSteps = 3;
	bool bReady = false, bFrozen = false, bTriedBuild = false;
	float SimMs = 0.f, ViewMs = 0.f, CoupleMs = 0.f, DropMs = 0.f, PropsMs = 0.f, AntsMs = 0.f;
	double LostVolume = 0.;
	float StatAt = 0.f;
	TWeakObjectPtr<class AKitchenFaucet> Faucet;

	// ---- the game (server): the tap fixture, drains, the stains it washes, the Human's reaction, geysers
	void TickGame(float Dt);
	void TickGeysers(float Dt);
	void WashStains(float Step);
	// ---- network (AAntFluidNet)
	TWeakObjectPtr<AAntFluidNet> Net;
	void TickNetServer(float Dt);
	void TickNetClient(float Dt);
	float SnapClock = 0.f;
	int32 SnapTurn = 0;
	uint16 SnapSeq = 0;
	struct FSnapIn { uint16 Seq = 0; int32 Parts = 0; int32 Got = 0; TArray<TArray<uint8>> Chunks; bool bHadFirst = false; };
	TMap<uint8, FSnapIn> SnapIn;
	struct FSnapOut { uint8 Basin = 0; uint16 Seq = 0; uint8 Part = 0; uint8 Parts = 0; TArray<uint8> Data; };
	TArray<FSnapOut> SendQueue;
	int32 SnapsRead = 0;
public:
	/** Client: a piece of the server's picture arrived (AAntFluidNet). */
	void ClientSnap(uint8 Basin, uint16 Seq, uint8 Part, uint8 Parts, const TArray<uint8>& Data);
	AAntFluidNet* GetNet() const { return Net.Get(); }
private:
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
	TArray<uint8> ViewDirty;   // tiles of the basin being drawn that changed since the last frame
	int32 UploadTiles = 0;     // stats: tiles written into the textures this frame
	int32 CausticTod = -1;
	bool bFlip = false;        // the 3D FLIP volumes (Niagara Fluids) at big splashes: off, they read as goo at this scale (-AntFluidFlip / fluid flip on)

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
	UPROPERTY() UInstancedStaticMeshComponent* SprayMesh = nullptr;
	TArray<float> LastStreamKind, LastSprayKind;
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
