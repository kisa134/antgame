#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "AntCloth.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AKitchenFixture;
class AAntCharacter;
class AAntKitchenGameMode;

/**
 * Real cloth (docs/FEATURES.md, section 33; LEVEL_V3.md section 5 «Ткань»): the tablecloth, the curtains, the plaid, the kid's
 * blanket fort, the sheets on the line. Our own position-based (Verlet) grid, not Chaos Cloth: every machine runs the same small
 * simulation from replicated inputs (who pulls where, which rings are off, which cells are bitten through, the wind, the water),
 * the server sends a coarse lattice of its particles 5 times a second while the cloth moves and clients pull theirs towards it.
 *
 * A cloth adopts a piece of the level (a fixture's body or a builder mesh, found by its mesh name and place), hides it and draws
 * itself as a two-sided procedural mesh with the piece's own material. Collision is a lazily traced two-level height field
 * (a slab on top - the table top, the seat - and the ground under it), the window's wall plane for curtains, and a little
 * layering in heaps. Asleep (nothing moves for a second) it costs nothing and cooks a collision copy the ants climb.
 *
 * Gameplay (server): ants hold [E] at an edge (the heavy-door rule: their strength sums) and pull hand over hand; things on it
 * ride and fall off its edge (fixtures by their own rules, food and plates as bodies); bites cut cells and a pulled cut tears on;
 * a cloth on the floor is a heap (cover) and the Human trips over it; curtains hang from their rings (undone rings let go), sway
 * in the draught when the kitchen window is open, and get wet and heavy in water.
 */
namespace AntClothKind
{
	enum EId : uint8 { Tablecloth, LongCloth, Curtain, Plaid, Fort, Sheet, Count };
}

USTRUCT()
struct FAntClothGrab
{
	GENERATED_BODY()
	UPROPERTY() uint16 Idx = 0;
	UPROPERTY() FVector_NetQuantize10 Target;
	bool operator==(const FAntClothGrab& O) const { return Idx == O.Idx && Target == O.Target; }
};

/** The coarse lattice the server sends while the cloth moves (every KStep-th particle in both directions). */
USTRUCT()
struct FAntClothKey
{
	GENERATED_BODY()
	UPROPERTY() uint16 Seq = 0;
	UPROPERTY() TArray<FVector_NetQuantize10> P;
};

UCLASS()
class ANTKITCHEN_API AAntCloth : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AAntCloth();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- the [E] pull (the heavy-door rule)
	virtual int32 DoorClass() const override { return 3; }
	virtual bool IsDoorOpen() const override { return false; }
	virtual bool CanInteract(const AAntCharacter* Ant) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual float DistanceTo(const FVector& P) const override;
	virtual FString DoorNoun() const override;

	// ---- replicated setup: what it is and where its piece of the level is
	UPROPERTY(Replicated) uint8 Kind = AntClothKind::Tablecloth;
	UPROPERTY(Replicated) FName SourceMesh;
	UPROPERTY(Replicated) FVector_NetQuantize10 SourceAt;
	UPROPERTY(Replicated) AKitchenFixture* SourceFix = nullptr;
	/** Builder row of the source (curtain climb proxies), -1 none. */
	UPROPERTY(Replicated) int32 SourceRow = -1;

	// ---- replicated inputs
	UPROPERTY(Replicated) TArray<FAntClothGrab> Grabs;
	/** Top-row pins let go (curtains: their rings), a bit per pin column. */
	UPROPERTY(Replicated) uint64 PinOff = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) TArray<uint16> DeadQuads;
	UPROPERTY(Replicated) float Wet = 0.f;
	UPROPERTY(Replicated) FVector_NetQuantize10 Wind;
	/** 0 asleep at rest, 1 awake, 2 a heap on the floor (cover). */
	UPROPERTY(ReplicatedUsing = OnRep_Mode) uint8 Mode = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Key) FAntClothKey Key;
	UPROPERTY(ReplicatedUsing = OnRep_Mode) FVector_NetQuantize10 HeapAt;
	UPROPERTY(Replicated) float HeapRadius = 0.f;

	UFUNCTION() void OnRep_Dead();
	UFUNCTION() void OnRep_Mode();
	UFUNCTION() void OnRep_Key();

	// ---- the server's API
	/** Wake it (it simulates until it rests again). */
	void Wake(const TCHAR* Why);
	/** A virtual crew pulls the nearest edge towards Dir with this strength for Seconds (dev, the chain C2, the table falling). */
	void Yank(const FVector& Dir, float Strength, float Seconds, AAntCharacter* By);
	/** An ant bites at P: the cell there is cut (false: nothing of this cloth in reach). */
	bool BiteAt(const FVector& P, AAntCharacter* By);
	/** Cut the cell nearest to P (dev "tear"). */
	bool CutAt(const FVector& P, float Reach);
	/** Every pin goes (curtains fall). */
	void DropAll(const TCHAR* Why);
	/** Back to its piece of the level as it was. */
	void ResetCloth();
	/** Dev: N worker bots stand at the hem on the Dir side and hold [E] for Seconds. */
	void DevCrew(int32 N, const FVector& Dir, float Seconds);
	TArray<TWeakObjectPtr<AAntCharacter>> Crew;
	float CrewUntil = 0.f;
	/** Dev: cut a line of cells through P along the cloth (a row of bites). */
	int32 CutLine(const FVector& P, int32 Cells);

	int32 NumParticles() const { return X.Num(); }
	bool IsAwake() const { return Mode == 1; }
	FString Describe() const;
	FString KindName() const;
	FBox Bounds() const;
	/** Milliseconds the last frame took (this machine), and its running mean. */
	double LastMs = 0.0;
	double MeanMs() const { return AvgMs; }
	/** Mean distance of the particles from where they started (how far it has been pulled). */
	float Displaced() const;

	static constexpr int32 KStep = 3;

private:
	// ---- grid
	int32 NU = 0, NV = 0;
	TArray<FVector> X, Prev, Rest, FlatPos;
	float SourceTop = 0.f;
	double AvgMs = 0.0;
	TArray<float> InvM;
	TArray<uint8> Ghost;         // clients: steps left without collision after a snap to the server's lattice
	TArray<uint8> Pinned;        // 1: a top-row pin (curtains), 2: held by a grab this step
	TArray<FVector> PinAt;
	struct FEdge { int32 A, B; float L; float K; int32 Q0, Q1; };
	TArray<FEdge> Edges;
	TArray<uint8> QuadAlive;
	int32 Idx(int32 u, int32 v) const { return v * NU + u; }
	bool bBuilt = false;
	bool bTopoDirty = true;
	float Thickness = 6.f;
	float CellUU = 100.f;
	FVector WallN = FVector::ZeroVector;   // curtains: the window's side (nothing goes behind WallD)
	float WallD = 0.f;
	float FloorZ = 0.f;
	float Accum = 0.f;
	float StillFor = 0.f;
	float AwakeFor = 0.f;
	float KeyClock = 0.f;
	float PullClockC = 0.f;
	float ServerClock = 0.f;
	float NextTrip = 0.f;
	float TripLogAt = 0.f;
	float WetSeen = -1.f;
	uint16 KeySeen = 0;
	TArray<FVector> KeyErr;      // clients: the server's lattice minus ours
	float KeyAge = 99.f;
	bool bKeyFresh = false;
	bool bHeapCover = false;
	bool bCollisionCooked = false;
	bool bRestMesh = true;
	bool bPins = false;
	uint8 ShownMode = 255;
	int32 ShownDead = -1;

	// ---- the source piece of the level
	TWeakObjectPtr<UStaticMeshComponent> Source;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> CupsSrc;
	UPROPERTY() TArray<UStaticMeshComponent*> Cups;
	TArray<int32> CupIdx;
	TArray<FVector> CupOff;
	TArray<float> CupFall;       // < 0 riding; >= 0 seconds since it went over the edge
	TArray<FVector> CupVel;
	TArray<uint8> CupDone;
	float FindTries = 0.f;
	bool FindSource();
	void BuildFromSource();
	void HideSource(bool bHide);

	// ---- the height field (lazily traced)
	FVector2D HfMin = FVector2D::ZeroVector;
	int32 HfW = 0, HfH = 0;
	float HfCell = 25.f;
	float HfStartZ = 0.f;
	TArray<float> HfTop, HfUnder, HfGround;
	TArray<uint8> HfKnown;
	int32 HfBudget = 0;
	bool bHfStarved = false;     // the last sample wanted a cell the frame had no traces left for
	TArray<uint16> LayerCount;
	FCollisionQueryParams HfParams;
	FCollisionObjectQueryParams HfObjects;
	float HfIgnoreClock = 0.f;
	void HfInit();
	void HfRefreshIgnores();
	bool HfSample(float x, float y, float& Top, float& Under, float& Ground);
	void HfInvalidate(const FBox& Box);

	// ---- simulation
	void BuildEdges();
	void Step(float Dt);
	void Collide(int32 i);
	void SolveEdges(int32 Iter);
	void ApplyKey(float Dt);
	void PreRelax(int32 Steps);

	// ---- render
	UPROPERTY() UProceduralMeshComponent* Mesh = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* Mid = nullptr;
	FLinearColor BaseTint = FLinearColor::White;
	bool bHaveTint = false;
	void UpdateMesh(bool bRebuild, bool bCollision);

	// ---- server gameplay
	struct FPuller { TWeakObjectPtr<AAntCharacter> Ant; int32 P = -1; FVector Dir = FVector::ZeroVector; };
	TArray<FPuller> Pullers;
	struct FRider { TWeakObjectPtr<AActor> Actor; int32 Piece = -1; int32 P = -1; FVector Off = FVector::ZeroVector; bool bOff = false; bool bWoken = false; };
	TArray<FRider> Riders;
	float YankLeft = 0.f;
	FVector YankDir = FVector::ZeroVector;
	float YankStrength = 0.f;
	TWeakObjectPtr<AAntCharacter> YankBy;
	TArray<int32> YankP;
	TArray<TWeakObjectPtr<class AKitchenFastener>> RingOf;
	bool bRingsBound = false;
	float TearClock = 0.f;
	float RipClock = 0.f;
	TWeakObjectPtr<AAntCharacter> LastBy;
	void ServerTick(float Dt);
	void ServerPull(float Dt);
	void ServerRiders(float Dt);
	void ServerRings();
	void ServerTear();
	void ServerTrip();
	void ServerWetWind(float Dt);
	void CheckHeap();
	void FindRiders();
	void TickCups(float Dt);
	int32 NearestBorder(const FVector& P, float* OutDist = nullptr) const;
	int32 NearestParticle(const FVector& P, float* OutDist = nullptr) const;
	float NeedStrength() const;
};

namespace AntCloth
{
	/** -AntNoCloth: the old static meshes. */
	ANTKITCHEN_API bool Enabled();
	/** Server, AKitchenFixture::ResetAll: every cloth back to its piece of the level (adopted again next frame). */
	ANTKITCHEN_API void ResetAll(UWorld* World);
	/** The cloth that took over this fixture (the tablecloth, the kitchen curtain), or null. */
	ANTKITCHEN_API AAntCloth* ForFixture(const AKitchenFixture* Fx);
	/** Server, AKitchenFixture::OnDone of the tablecloth (the chain C2, the dev push): the real cloth is yanked. True = handled. */
	ANTKITCHEN_API bool Yank(AKitchenFixture* Fx, const FVector& Dir, AAntCharacter* By);
	/** Server, from the ant's bite: cut the cloth in front of its jaws. True = it bit cloth. */
	ANTKITCHEN_API bool OnBite(AAntCharacter* Ant);
	/** Dev and admin: cloth list|info|pull|crew|tear|wind|wet|drop|reset|cost. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** The draught (admin): -1 automatic (the open kitchen window), 0 still, 1 windy. */
	ANTKITCHEN_API int32& WindMode();
	/** Total ms of all cloth on this machine in the last frame, and how many are awake. */
	ANTKITCHEN_API double FrameMs(const UWorld* World, int32* OutAwake = nullptr, int32* OutParticles = nullptr);
	/** Register a piece of the level as cloth (the map's own meshes): mesh name prefix -> kind. */
	ANTKITCHEN_API void AddMeshRule(const TCHAR* MeshPrefix, uint8 Kind);
	/** Dev helpers shared with AntInside: a fixed local camera (From at At; Fov <= 0: back to the pawn) and a screenshot to Saved/Screenshots/<Folder>/<Name>.png. */
	ANTKITCHEN_API void DevCam(UWorld* World, const FVector& From, const FVector& At, float Fov);
	ANTKITCHEN_API void DevShot(UWorld* World, const FString& Folder, const FString& Name);
	/** Dev: a cloth by word (tablecloth, long, curtain[N], plaid, fort, sheet, or its number). */
	ANTKITCHEN_API AAntCloth* Find(UWorld* World, const FString& Word);
}
