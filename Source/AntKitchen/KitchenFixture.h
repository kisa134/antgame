#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "KitchenFixture.generated.h"

class AAntCharacter;
class UStaticMeshComponent;

/**
 * Level v3 (docs/level/LEVEL_V3.md, section 5): the interactive objects of the flat and what holds them.
 *
 * AKitchenHeld is the shared rule: a group of ants holds [E] at it (the heavy-door rule: their strength sums, bots and the squad
 * help, the HUD says «Нужно ещё N» while the group is too weak). Once strong enough the work advances: Seconds at exactly Need,
 * faster with more (a screw: strength 14 - 8 s, 28 - 4 s), twice as fast on a fastener the seed left loose. Progress is replicated.
 *
 * AKitchenFastener is one screw, hinge, nail string, strip of tape, magnet, curtain ring, ribbon knot, bottle cap or knitting needle
 * (a row of the base table: every machine builds the same mesh). Undone, it drops to the floor and tells its fixture.
 *
 * AKitchenFixture is the object they hold: the shelf over the nest, the jars, the curtains and the rod, the plant, the cola, the cereal,
 * the drawings on the fridge, the photo over the sofa, the balloons, the toy box, the gift, the knitting, the glasses, the outlet cover,
 * and (when their district variant is in play) the tablecloth and the teapot. Objects with nothing to undo are pushed off their edge
 * the same way (friction). When the last fastener of its rule goes, it hangs (the shelf on one bracket, the rod on one end) or falls.
 * The fall is replicated as its start (time, place, push) and the place it lands; every machine animates the same parabola.
 * What it is made of decides the landing: glass, china and terracotta break from their height (v3.break_classes), the shards cut,
 * the contents spill (jam: sticky; brine and tea: the slippery puddle the Human falls in; cola: a sticky pool that grows for 20 s;
 * cereal: a fan of crumbs to carry), the crash is a noise, and every break makes the Human angrier.
 */
UCLASS(Abstract)
class ANTKITCHEN_API AKitchenHeld : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenHeld();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual int32 DoorClass() const override;
	virtual bool IsDoorOpen() const override { return bDone; }
	virtual void SetDoorOpen(bool bInOpen) override {}
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return !bDone && bHeldWork; }
	virtual void Interact(AAntCharacter* Ant) override {}

	/** 0..1 of the work; replicated for the prompt. */
	UPROPERTY(Replicated) float Progress = 0.f;
	UPROPERTY(Replicated) bool bDone = false;
	/** Loose (the seed): the work goes twice as fast. */
	UPROPERTY(Replicated) bool bLoose = false;
	/** Strength it takes to make any progress, and the seconds it takes at exactly that strength. */
	float Need = 14.f;
	float Seconds = 8.f;
	/** False: nothing to do with [E] right now (a fixture that hangs, or waits for its cap). */
	bool bHeldWork = true;
	/** Server: who finished it. */
	TWeakObjectPtr<AAntCharacter> DoneBy;

	/** Server (dev and tests): finish it at once, as if By had done it. */
	void ForceDone(AAntCharacter* By);

protected:
	virtual bool TryOpenByPull() override;
	/** Server: the work is done. */
	virtual void OnDone() {}
	float LastStep = -1.f;
};

UCLASS()
class ANTKITCHEN_API AKitchenFastener : public AKitchenHeld
{
	GENERATED_BODY()

public:
	AKitchenFastener();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual float DistanceTo(const FVector& P) const override;
	virtual FString DoorNoun() const override;

	/** Index into LevelV3::Fasteners(). */
	UPROPERTY(Replicated) int32 Def = -1;
	/** Server world time it came off (< 0: holding). Every machine drops it from there. */
	UPROPERTY(Replicated) float OffAt = -1.f;

	static FString KindName(int32 Kind);

protected:
	virtual void OnDone() override;

private:
	void Build();
	UPROPERTY() UStaticMeshComponent* Mesh = nullptr;
	FTransform Home;
	float FloorZ = 0.f;
	bool bOffLogged = false;
};

UCLASS()
class ANTKITCHEN_API AKitchenFixture : public AKitchenHeld
{
	GENERATED_BODY()

public:
	AKitchenFixture();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual float DistanceTo(const FVector& P) const override;
	virtual FString DoorNoun() const override;

	enum EState : uint8 { Fixed = 0, Hanging = 1, Falling = 2, Lying = 3, Broken = 4, Open = 5 };

	/** Index into LevelV3::Fixtures(). */
	UPROPERTY(Replicated) int32 Fix = -1;
	/** >= 0: it is a variant's piece, row Var of LevelV3::VarArt() (the tablecloth, the teapot). */
	UPROPERTY(Replicated) int32 Var = -1;
	UPROPERTY(ReplicatedUsing = OnRep_State) uint8 State = Fixed;
	/** The fall: when it started (server world time), the pose it started from, the pose it lands in, how long it takes. */
	UPROPERTY(Replicated) float FallStart = -1.f;
	UPROPERTY(Replicated) FTransform FallFrom;
	UPROPERTY(Replicated) FTransform FallTo;
	UPROPERTY(Replicated) float FallTime = 0.5f;
	/** Hanging: the pose it hangs in. */
	UPROPERTY(Replicated) FTransform HangPose;
	/** Where it broke (the after-mesh lies there) and which way it was going. */
	UPROPERTY(ReplicatedUsing = OnRep_State) FVector_NetQuantize BreakAt;
	UPROPERTY(Replicated) float BreakYaw = 0.f;

	UFUNCTION() void OnRep_State();

	FString FixId() const;
	/** Level v3: switches the builder's climb stand-ins of the kitchen curtain on (hung) or off (torn down). */
	void SetCurtainClimb(bool bOn);
	int32 Mat() const;
	bool IsBreakable() const;

	/** Server: one of its fasteners came off. */
	void OnFastenerOff(int32 FastenerDef, AAntCharacter* By);
	/** Server: it goes over its edge (pushed by the ants: Dir away from them; or knocked by a falling shelf, rod, tablecloth). */
	void Fall(const FVector& Dir, AAntCharacter* By, const TCHAR* Why);
	/** Server: it hangs from what still holds it, tilted by Deg about the world axis Axis through Pivot. */
	void Hang(const FVector& Pivot, const FVector& Axis, float Deg, AAntCharacter* By);
	/** Server: breaks where it is (dev: break <id>). */
	void BreakNow(AAntCharacter* By);

	// ---- the world's fixtures (server)
	/** Respawns them all for the current seed (each raid starts with everything whole and fastened). */
	static void ResetAll(UWorld* World);
	static AKitchenFixture* Find(UWorld* World, const FString& Id);
	/** An invisible "ants only" box over a curtain panel (floor to rod) that the ants climb (the cloth sheet itself has nothing to grip). */
	static UStaticMeshComponent* AddClimbProxy(AActor* Owner, USceneComponent* Parent, const FTransform& PanelWorld);
	/** Rage the Human gets for one broken thing (in rage levels: three breaks = one level). */
	static constexpr float RagePerBreak = 0.34f;

protected:
	virtual void OnDone() override;

private:
	void Build();
	void Land();
	void Break();
	void ShowAfter();
	FTransform PoseAt(float T) const;
	float NetNow() const;
	/** Fixtures standing on this one (or on the table under the tablecloth...) go over with it. */
	void KnockRiders(const FVector& Dir, AAntCharacter* By);
	void OpenLid();
	void DropStaticRiders();
	bool bRidersDropped = false;
	FVector NearestEdgeDir(const FVector& Prefer) const;
public:
	/** Server (dev): push it this way instead of away from the ants. */
	FVector PushDir = FVector::ZeroVector;
private:
	void Spill(AAntCharacter* By);

	UPROPERTY() UStaticMeshComponent* Body = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Parts;
	UPROPERTY() TArray<UStaticMeshComponent*> After;
	UPROPERTY() TArray<UStaticMeshComponent*> Proxies;
	FTransform Home;
	TWeakObjectPtr<AAntCharacter> LastBy;
	bool bLanded = false;
	bool bAfterShown = false;
	float PoolStart = -1.f;
	UPROPERTY() UStaticMeshComponent* Pool = nullptr;
	TArray<int32> Undone;
};
