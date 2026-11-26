#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntTrail.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AKitchenLoot;
class AAntCharacter;
class APlayerState;

/** One pheromone trail of the snowball raid: nest first, its far end last. */
USTRUCT()
struct FAntTrailLine
{
	GENERATED_BODY()
	UPROPERTY() TArray<FVector_NetQuantize> Points;
	UPROPERTY() TArray<FVector_NetQuantizeNormal> Normals;
	UPROPERTY() APlayerState* Owner = nullptr;
	UPROPERTY() int32 Workers = 0;
	UPROPERTY() int32 Seed = 0;
	/** Still being laid by its owner (drawn, no workers yet). */
	UPROPERTY() bool bLaying = false;
	UPROPERTY() uint16 Id = 0;
	/** Delivered home along this trail so far: pieces and their food value (the trail's counter, KAN-32). */
	UPROPERTY() int32 Delivered = 0;
	UPROPERTY() int32 DeliveredValue = 0;
	// server
	TArray<float> Dist;      // distance from the nest at each point
	float Length = 0.f;
};

/** A crumb on its way home in a worker's jaws: every machine moves it along the trail from the replicated start. */
USTRUCT()
struct FAntFerry
{
	GENERATED_BODY()
	UPROPERTY() uint16 Trail = 0;      // FAntTrailLine::Id
	UPROPERTY() float Start = 0.f;     // server time it set off
	UPROPERTY() float From = 0.f;      // distance along the trail it set off from (it walks to 0)
	UPROPERTY() float Speed = 150.f;
	UPROPERTY() FColor Color = FColor::White;
	UPROPERTY() uint8 Size = 10;       // radius of the crumb drawn
	/** Brought to the trail by a squad (drawn with a ring: the hand-off), not picked up by the trail's own workers. */
	UPROPERTY() bool bFromSquad = false;
	// server
	TWeakObjectPtr<AKitchenLoot> Load;
};

/**
 * Every pheromone trail of the snowball raid and the workers on it (one replicated actor). Workers are not actors: a trail is
 * "N workers spread along a polyline" drawn by the swarm's GPU material (M_SwarmAntArt) as instances; the server only counts
 * them. Small food (light enough for one worker) lying on or near a trail is taken by a worker and walked home: the real loot
 * actor is hidden and held while a replicated ferry entry moves its stand-in along the trail; at the nest it is delivered
 * through the game mode like any food. Blows (the Human's slipper and step, the cat, the dog, grandma's broom) cut a trail where
 * they land; red ants on a trail kill workers and drop what they carry; the dog smells trails.
 */
UCLASS()
class ANTKITCHEN_API AAntTrailNet : public AActor
{
	GENERATED_BODY()

public:
	AAntTrailNet();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AAntTrailNet* Get(const UWorld* World, bool bCreate = false);

	UPROPERTY(ReplicatedUsing = OnRep_Lines) TArray<FAntTrailLine> Lines;
	UPROPERTY(ReplicatedUsing = OnRep_Ferries) TArray<FAntFerry> Ferries;

	static constexpr float PointStep = 45.f;
	static constexpr float BaseSpeed = 156.f;      // = AAntSwarm::WorkerSpeed

	// ---- server
	/** [T]: start laying a trail where the ant stands / finish it. True: handled (the snowball raid). */
	bool ToggleLay(AAntCharacter* Ant);
	/** [Y]: remove this player's trails (workers go home). */
	int32 Recall(APlayerState* PS);
	/** Dev: a finished trail along these points (the first one is moved to the nest). */
	int32 DevLay(APlayerState* PS, const TArray<FVector>& Pts);
	/** A blow landed here: the trails under it are cut. Cause: for the message. Returns workers killed. */
	int32 Cut(const FVector& Center, float Radius, const TCHAR* Cause);
	/** Workers of this player went up / down: share them out over his trails again. */
	void Rebalance(APlayerState* PS);
	void Clear();
	int32 WorkersOnTrails(const APlayerState* PS) const;
	int32 TrailsOf(const APlayerState* PS) const;
	float MaxLength(const APlayerState* PS) const;
	/** Ferried home so far this raid, for the timeline. */
	int32 Ferried = 0;
	int32 CutsDone = 0;

	/** Where a ferry is now (every machine). */
	bool FerryPos(const FAntFerry& F, float Now, FVector& Out) const;
	const FAntTrailLine* LineById(uint16 Id) const;
	/** The point of this player's working trails (finished, with workers) nearest to P, within MaxDist (2D). Its distance from
	 *  the nest along the trail goes to OutAlong. False: none. */
	bool NearestTrailPoint(const APlayerState* PS, const FVector& P, float MaxDist, FVector& OutPoint, uint16& OutTrail, float* OutAlong = nullptr) const;
	/** Server: a squad brought this load to the trail: its workers take it from here and walk it home (a heavy one slowly).
	 *  False: refused (no such trail, the cake, already held). */
	bool HandOff(AKitchenLoot* Load, uint16 Trail);
	/** Server time on every machine (the ferries move by it). */
	float NetNow() const;

private:
	UFUNCTION() void OnRep_Lines();
	UFUNCTION() void OnRep_Ferries();
	void ServerTick(float Dt);
	void LayTick();
	void Finish(int32 Index);
	void Measure(FAntTrailLine& L) const;
	void Forage(FAntTrailLine& L, float Dt);
	void ArriveFerries();
	void Dangers();
	void DropFerry(int32 FerryIndex, const FVector& At);
	void RemoveLine(int32 Index, bool bWorkersHome);
	bool PosAt(const FAntTrailLine& L, float D, FVector& Out, FVector* OutN = nullptr) const;
	float NearestOnLine(const FAntTrailLine& L, const FVector& P, float& OutDistSq) const;
	void Rebuild();
	void DrawFerries();
	UInstancedStaticMeshComponent* MakeWorkerPart(const TCHAR* Name, const FVector& Offset, const FVector& Size, float LegMode, const FLinearColor& Color, class UStaticMesh* Mesh, const TCHAR* Mat);

	struct FLaying { TWeakObjectPtr<AAntCharacter> Ant; int32 Line = -1; bool bFromNest = true; };
	TArray<FLaying> Laying;
	TSet<TWeakObjectPtr<AKitchenLoot>> Claimed;
	struct FPickup { TWeakObjectPtr<AKitchenLoot> Load; uint16 Trail; float At; float From; };
	TArray<FPickup> Pickups;
	uint16 NextId = 1;
	float SlowClock = 0.f;
	float AmbushAt = 0.f;
	float LastCutMsg = 0.f;
	TMap<TWeakObjectPtr<AActor>, FVector> HumanWas;

	UPROPERTY() TArray<UInstancedStaticMeshComponent*> WorkerParts;
	UPROPERTY() UInstancedStaticMeshComponent* Dots = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* LayDots = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> FerryCrumbs;
	UPROPERTY() TArray<UStaticMeshComponent*> FerryAnts;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> FerryMIDs;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> MIDs;
	/** The direction of the flow: glowing dashes along each working trail, gliding towards the nest like a conveyor's belt
	 *  (walked by the swarm's GPU material, as the workers are: no cost per frame). */
	UPROPERTY() UInstancedStaticMeshComponent* Flow = nullptr;
	/** A gold disc under every load on its way home: food being hauled reads from far away. */
	UPROPERTY() TArray<UStaticMeshComponent*> FerryRings;
	bool bHasMaterial = false;
	float TimeSync = 0.f;
};

/** A read-only view of one pheromone trail for the HUD and the minimap (every machine). */
struct FAntTrailView
{
	uint16 Id = 0;
	const APlayerState* Owner = nullptr;
	/** Nest first, the far end last. */
	TArray<FVector> Points;
	float Length = 0.f;
	int32 Workers = 0;
	/** Still being laid (no workers yet). */
	bool bLaying = false;
	/** Pieces delivered home along it so far, and their food value. */
	int32 Delivered = 0;
	int32 DeliveredValue = 0;
	/** The loads on their way home along it right now: where each is. */
	TArray<FVector> Loads;
};

namespace AntTrail
{
	/** Hooks of the existing code (server). */
	ANTKITCHEN_API bool OnToggleKey(AAntCharacter* Ant);
	ANTKITCHEN_API bool OnRecallKey(AAntCharacter* Ant);
	ANTKITCHEN_API void OnBlow(UWorld* World, const FVector& Center, float Radius);
	/** Every trail of the raid (OnlyOwner: only that player's), with its counter and the loads moving on it. Every machine. */
	ANTKITCHEN_API void GetTrails(const UWorld* World, TArray<FAntTrailView>& Out, const APlayerState* OnlyOwner = nullptr);
	/** The local player's HUD: a label over every trail (workers, on the way, delivered). Called from AntRts::DrawHud. */
	ANTKITCHEN_API void DrawHud(class AHUD* Hud, class UCanvas* Canvas, class APlayerController* PC);
}
