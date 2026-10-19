#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntTypes.h"
#include "AntSwarm.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class AKitchenLoot;
class AAntCharacter;

/** A pheromone trail with a stream of worker ants walking it (nest <-> food). */
USTRUCT()
struct FSwarmStream
{
	GENERATED_BODY()

	UPROPERTY() TArray<FVector_NetQuantize> Points;          // nest first, food last
	UPROPERTY() TArray<FVector_NetQuantizeNormal> Normals;   // surface under each point
	UPROPERTY() int32 Count = 0;                             // ants on the trail
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() bool bHasFood = false;
	UPROPERTY() FString OwnerName;

	// Server only
	TWeakObjectPtr<AKitchenLoot> Food;
	float IdleTime = 0.f;
	float Length = 0.f;
};

/**
 * The colony's worker swarm: tens of thousands of tiny ants.
 *
 * Nothing per-ant is simulated or replicated. A stream is "N ants spread along a polyline";
 * each client turns it into instances and a material moves them on the GPU (M_SwarmAnt):
 * position = segment(time * speed + phase). The server only tracks counts:
 * income from food, losses from stomps / spray / the hot burner.
 */
UCLASS()
class ANTKITCHEN_API AAntSwarm : public AActor
{
	GENERATED_BODY()

public:
	AAntSwarm();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AAntSwarm* Get(const UWorld* World);

	static constexpr int32 MaxStreams = 6;
	static constexpr int32 MaxStreamsPerPlayer = 2;
	static constexpr float AntsPerUnit = 0.45f;       // density along a trail
	static constexpr float WorkerSpeed = 260.f * AntKit::SpeedK;

	/** Server: lay a new trail. Returns ants sent (0 = failed, Why explains). */
	int32 CreateStream(const TArray<FVector>& Points, const TArray<FVector>& Normals, AKitchenLoot* Food, AAntCharacter* By, FString& Why);
	/** Server: kill workers near a point (stomp). Returns how many died. */
	int32 KillInSphere(const FVector& Center, float Radius);
	/** Server: bug spray cone, call every frame while spraying. */
	int32 KillInCone(const FVector& Origin, const FVector& Dir, float Range, float CosHalfAngle, float Dt);
	/** Server: more workers hatch (food delivered to the nest). */
	void AddPopulation(int32 Amount);
	/** Server: a good stomp target for the AI Human (densest trail spot on the floor). */
	bool FindStompTarget(const FVector& From, float MaxDist, FVector& OutPoint) const;
	/** Server: cancel this player's trails, workers go home. */
	int32 RecallStreams(const FString& OwnerName);

	/** Dev: lay test trails with this many ants in total (floor, wall, table leg). */
	void DebugSpawnTestStreams(int32 TotalAnts);

	int32 CountStreams(const FString& OwnerName) const;
	int32 AntsOnTrails() const;

	UPROPERTY(ReplicatedUsing = OnRep_Streams) TArray<FSwarmStream> Streams;
	UPROPERTY(Replicated) int32 Population = 3000;   // all workers of the colony
	UPROPERTY(Replicated) int32 Idle = 3000;         // workers waiting in the nest

private:
	UFUNCTION() void OnRep_Streams();
	UFUNCTION(NetMulticast, Unreliable) void MulticastSplat(FVector_NetQuantize Location, int32 Killed);

	void Rebuild();
	void ServerTick();
	void RemoveStream(int32 Index, bool bReturnToNest);
	void Kill(FSwarmStream& S, int32 N);
	UInstancedStaticMeshComponent* MakePart(const TCHAR* Name, const FVector& Offset, const FVector& Size, float LegMode, const FLinearColor& Color, const FLinearColor& Glow, bool bShadow,
		class UStaticMesh* Mesh = nullptr, const TCHAR* MaterialPath = nullptr);

	UPROPERTY() TArray<UInstancedStaticMeshComponent*> BodyParts;   // abdomen, thorax, head, legs A, legs B
	UPROPERTY() UInstancedStaticMeshComponent* Crumbs = nullptr;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> MIDs;
	UPROPERTY() TArray<UStaticMeshComponent*> Splats;

	bool bHasMaterial = false;
	float ServerTimer = 0.f;
	float TimeSyncTimer = 0.f;
	TArray<float> SplatTimes;
	int32 NextSplat = 0;
};
