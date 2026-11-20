#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntTypes.h"
#include "AntArmy.generated.h"

class UInstancedStaticMeshComponent;
class AAntCharacter;
class AAntChain;
class AKitchenLoot;
class AKitchenInteractable;
class APlayerState;
class AController;

UENUM()
enum class EArmyMode : uint8
{
	Follow,     // walks behind its commander
	Attack,     // goes to a point and fights / raids / eats whatever is there
	Home,       // returns to the nest and disbands
	Haul,       // a squad: goes to a piece of loot, lifts it, carries it (beside its commander or home on its own)
	Hold,       // a squad: stands guard at a spot; holds a door open
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

	// ---- A player's squad in a raid (AntSquad.h): a small army that is his stake ----
	UPROPERTY(Replicated) bool bSquad = false;
	/** What the squad is going for or carrying (attached to it while it carries). */
	UPROPERTY(Replicated) AKitchenLoot* Cargo = nullptr;
	UPROPERTY(Replicated) bool bCargoHeld = false;
	/** It is taking the cargo home by itself. */
	UPROPERTY(Replicated) bool bCargoHome = false;
	/** "Free": it forages by itself around its commander. */
	UPROPERTY(Replicated) bool bForaging = false;
	/** Its commander is dead: it waits by the body (and is lost if the body is not brought home). */
	UPROPERTY(Replicated) bool bOrphan = false;
	/** Whose squad it is (the commander pointer is gone while he is dead). */
	UPROPERTY(Replicated) APlayerState* OwnerPS = nullptr;
	// Server only.
	TWeakObjectPtr<AController> SquadOwner;
	float WorkerStr = 4.f;
	float BiteMul = 1.f;
	bool bArmor = false;
	bool bHaulHome = false;
	float HaulSpeedMul = 1.f;
	bool bHoldsDoors = false;
	int32 BridgeMul = 4;          // chain links one worker stands for
	bool bRamps = false;
	int32 Taken = 0;
	float OrphanUntil = 0.f;
	/** Nest guards (Soldiers II): nobody's squad, they stand at an entrance and go for any red ant that comes near. */
	bool bGuard = false;
	/** Server: no blow kills squad workers before this time (they have just scattered from one). */
	float BlowSafeUntil = 0.f;
	FVector GuardPost = FVector::ZeroVector;
	TWeakObjectPtr<AAntCharacter> AttackWho;
	TWeakObjectPtr<AKitchenInteractable> HeldDoor;
	TArray<FVector> Lead;         // where the commander walked: the squad walks the same way
	FVector LastCmdAt = FVector::ZeroVector;
	bool bCatchUp = false;
	TArray<FVector> Way;          // the way home with a load (route graph)
	int32 WayAt = 0;
	float WaitFor = 0.f;
	float IdleFor = 0.f;
	float NextForage = 0.f;
	float NextDoorCheck = 0.f;
	/** Pull / carry strength of the whole squad. */
	float Strength() const { return Count * WorkerStr; }
	/** Following its commander with nothing in its jaws, close to him: it helps him carry and pull. */
	bool HelpsCommander(const AAntCharacter* Cmd) const;
	void DropCargo();
	void BuildWayHome();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	void ServerMove(const FVector& Goal, float Dt, float SpeedMul = 1.f);
	/** Server: a squad's own moving and carrying (AntSquad.cpp). False: the squad is gone. */
	bool SquadTick(float Dt, FVector& OutGoal);
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
