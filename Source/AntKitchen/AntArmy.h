#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntArmy.generated.h"

class UInstancedStaticMeshComponent;
class AAntCharacter;

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
 * Only Count / position / mode replicate; each client draws up to ~700 ants in a moving blob.
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

	float Radius() const { return 60.f + FMath::Sqrt((float)FMath::Max(Count, 1)) * 9.f; }

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
