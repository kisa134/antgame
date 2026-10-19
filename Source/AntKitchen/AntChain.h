#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "AntTypes.h"
#include "AntChain.generated.h"

class AAntCharacter;
class UBoxComponent;
class UInstancedStaticMeshComponent;

/**
 * A living chain of ants, like real ants build: each one holds the next.
 *  - Ladder: hangs straight down from a ledge or a ceiling.
 *  - Bridge: grows from one bank towards a point on the other bank; complete when long enough.
 * Players and bots join at the tail ([G]); an army can pour a whole bridge out of its workers.
 * Once complete (or as a ladder) it gets a walkable collision: other ants crawl over it.
 */
UCLASS()
class ANTKITCHEN_API AAntChain : public AActor
{
	GENERATED_BODY()

public:
	AAntChain();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** One ant ~ this long (scaled ant body). */
	static constexpr float LinkLen = 44.f * AntKit::AntShrink;
	static constexpr int32 MaxLinks = 220;

	UPROPERTY(Replicated) uint8 Team = 0;
	UPROPERTY(Replicated) FVector_NetQuantize Anchor;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal Dir = FVector(0.f, 0.f, -1.f);
	UPROPERTY(Replicated) FVector_NetQuantize EndPoint;
	UPROPERTY(Replicated) bool bBridge = false;
	/** Real ants (players and bots), in order from the anchor. */
	UPROPERTY(Replicated) TArray<AAntCharacter*> Links;
	/** Army workers after the real ants (drawn as instances). */
	UPROPERTY(Replicated) int32 ArmyLinks = 0;
	/** Commander whose army poured this bridge (gets the workers back). */
	UPROPERTY(Replicated) AAntCharacter* Builder = nullptr;

	int32 NumLinks() const { return Links.Num() + ArmyLinks; }
	float Length() const { return NumLinks() * LinkLen; }
	float Span() const { return bBridge ? (float)FVector::Dist(FVector(Anchor), FVector(EndPoint)) : 0.f; }
	bool IsComplete() const { return bBridge && Length() >= Span() - LinkLen * 0.5f; }
	/** Can someone still join at the tail? */
	bool IsGrowing() const { return NumLinks() < MaxLinks && (!bBridge || !IsComplete()); }
	FVector PointAt(float D) const;
	FVector LinkPoint(int32 Index) const { return PointAt((Index + 0.5f) * LinkLen); }
	FVector TailPoint() const { return PointAt(Length()); }
	int32 IndexOf(const AAntCharacter* A) const { return Links.IndexOfByKey(A); }

	/** Server: a new chain with its first ant (First may be null for an army bridge). */
	static AAntChain* Start(UWorld* World, uint8 InTeam, AAntCharacter* First, const FVector& InAnchor, const FVector& InDir, bool bInBridge, const FVector& InEnd);
	/** Server. */
	bool Join(AAntCharacter* A);
	/** Server: A lets go; everyone hanging below it falls too. */
	void Leave(AAntCharacter* A);
	/** Server: stomps and sprays kill the army workers in the chain. */
	int32 KillArmyInSphere(const FVector& C, float R);
	/** Server: workers go back, everyone lets go. */
	void Dissolve();
	/** Lifetime for army bridges (0 = until everyone leaves). */
	float MaxLife = 0.f;

	static AAntChain* FindNearTail(UWorld* World, uint8 Team, const FVector& P, float Radius);

private:
	void UpdateCollision();
	void UpdateArmyVisual(float Dt);

	UPROPERTY() UBoxComponent* Walk = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* ArmyBodies = nullptr;
	int32 ShapeKey = -1;
	int32 DrawnArmy = -1;
	float InstScale = 0.3f;
	float Life = 0.f;
	bool bAnnounced = false;
};
