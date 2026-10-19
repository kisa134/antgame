#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntTypes.h"
#include "AntArmy.generated.h"

class UInstancedStaticMeshComponent;
class AAntCharacter;
class AAntChain;

UENUM()
enum class EArmyMode : uint8
{
	Follow,     // walks behind its commander
	Attack,     // goes to a point and fights / raids / eats whatever is there
	Home,       // returns to the nest and disbands
};

/**
 * A colony army: hundreds or thousands of workers taken out of the nest by a commander (F),
 * sent at the crosshair (R): they swarm enemy ants and the Human's feet, raid the other
 * nest (steal its sweetness), eat big food on the spot, fight other armies.
 * Stomps, spray and the newspaper kill them by the hundreds.
 *
 * Marching, the army walks like real ants do: a narrow column (2-4 lanes) along the path its head
 * took (a replicated breadcrumb trail), climbing walls and furniture legs on the way.
 * At the target / in a fight / standing still it spreads into a swarm.
 * Only Count / position / mode / trail replicate; each client draws up to ~700 ants.
 */
UCLASS()
class ANTKITCHEN_API AAntArmy : public AActor
{
	GENERATED_BODY()

public:
	AAntArmy();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) uint8 Team = 0;
	UPROPERTY(Replicated) int32 Count = 0;
	UPROPERTY(Replicated) EArmyMode Mode = EArmyMode::Follow;
	UPROPERTY(Replicated) FVector_NetQuantize Target;
	UPROPERTY(Replicated) AAntCharacter* Commander = nullptr;

	/** Breadcrumbs of the head (ring buffer), newest at TrailHead. */
	UPROPERTY(Replicated) TArray<FVector_NetQuantize> Trail;
	UPROPERTY(Replicated) int32 TrailHead = -1;
	/** Spread out (fighting / arrived / waiting) instead of marching in a column. */
	UPROPERTY(Replicated) bool bSwarming = true;

	static constexpr int32 TrailMax = 128;
	static constexpr float TrailStep = 40.f * AntKit::AntShrink;

	float Radius() const { return (60.f + FMath::Sqrt((float)FMath::Max(Count, 1)) * 9.f) * AntKit::AntShrink; }
	/** Where ant #i of N stands (no wobble). Valid after PrepareLayout(). */
	FVector SlotPos(int32 i, int32 N, FVector* OutDir = nullptr) const;
	/** Server: about how many of this army's ants are inside the sphere. */
	int32 CountNear(const FVector& C, float R, float ZTol = 100.f);
	/** Server: pour a living bridge From -> To out of the workers. */
	AAntChain* BuildBridge(const FVector& From, const FVector& To);

	/** Server: kill army ants in an area (stomp, newspaper, explosions). */
	static int32 KillInSphere(UWorld* World, const FVector& Center, float Radius, int32 MaxKills);
	/** Server: spray cone, per frame. */
	static void KillInCone(UWorld* World, const FVector& Origin, const FVector& Dir, float Range, float CosHalfAngle, float Dt);
	/** The army of this commander, if any. */
	static AAntArmy* FindFor(const AAntCharacter* Commander);
	/** Server: lose ants (returns how many actually died). */
	int32 Lose(int32 N);
	/** Server: back to the nest; survivors return to the colony. */
	void Disband();

private:
	void ServerFight(float Dt);
	void RebuildVisual();
	FVector GroundAt(const FVector& P) const;
	void ServerMove(const FVector& Goal, float Dt);
	void PushTrail(const FVector& P);
	void PrepareLayout();

	/** Head first, then the trail backwards; cumulative distance along it. */
	TArray<FVector> Path;
	TArray<float> PathDist;
	float LayoutTime = -1.f;
	TArray<FVector> Cur;        // client: smoothed world position of each drawn ant
	float StillTime = 0.f;
	float FightHeat = 0.f;

	UPROPERTY() UInstancedStaticMeshComponent* Bodies = nullptr;
	UPROPERTY() class UAudioComponent* March = nullptr;
	TArray<FVector> Offsets;    // client: spot of each drawn ant in the blob
	TArray<float> Phases;
	int32 DrawnFor = -1;
	float FightTimer = 0.f;
	float Life = 0.f;
	float StealAccum = 0.f;
	float RaidAccum = 0.f;
	float InstScale = 0.3f;
	float EatAccum = 0.f;
	float NextRaidMsg = 0.f;
	FVector LastPos = FVector::ZeroVector;
	FVector MoveDir = FVector::ForwardVector;
};
