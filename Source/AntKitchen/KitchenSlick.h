#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "KitchenSlick.generated.h"

class AAntCharacter;

/** What spilled. */
namespace SlickKind
{
	enum EId : uint8 { Water, Oil, Count };
}

/**
 * A slippery puddle on the floor (docs/FEATURES.md, «Человек на мокром полу»): water or cooking oil the ants spilled by tipping
 * a bottle over. It spreads for about a second, lies 60-90 s, and dries up. The Human who steps into it while walking may
 * slip (always, when he runs) and fall flat on his back (HumanCharacter.cpp, TrySlip); the cat and the dog slide on oil.
 * Replicated: kind, size, when it was spilled and when it is being wiped; every machine works out the radius itself.
 */
UCLASS()
class ANTKITCHEN_API AKitchenSlick : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenSlick();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return false; }
	virtual void AffectAnt(AAntCharacter* Ant, FAntEnv& Env) const override;

	UPROPERTY(Replicated) uint8 Kind = SlickKind::Water;
	UPROPERTY(Replicated) float MaxRadius = 560.f;
	/** Server world time it was spilled, how long it lies, when the Human started wiping it (< 0: not yet) and for how long. */
	UPROPERTY(Replicated) float Born = 0.f;
	UPROPERTY(Replicated) float Life = 75.f;
	UPROPERTY(Replicated) float WipeStart = -1.f;
	UPROPERTY(Replicated) float WipeLen = 4.f;

	/** Server: the ant that spilled it (the Queen's order counts it). */
	TWeakObjectPtr<AActor> By;
	/** Server: the Human knows about it (he fell in one): he walks round it. */
	bool bKnown = false;

	/** Radius right now (grows over a second, shrinks while wiped and in the last seconds), on every machine. */
	float Radius() const;
	bool IsSlippery() const { return Radius() > 110.f; }
	/** Server: the Human wipes it up over this many seconds. */
	void StartWipe(float Seconds);
	bool IsBeingWiped() const { return WipeStart >= 0.f; }

	/** A slippery puddle under P (within its radius plus Margin, near its height), or null. */
	static AKitchenSlick* At(const UWorld* World, const FVector& P, float Margin = 0.f);
	/** Server: spill one onto the floor under P. Radius / life < 0: the kind's own. */
	static AKitchenSlick* Spill(UWorld* World, const FVector& P, uint8 Kind, AActor* By, float InRadius = -1.f, float InLife = -1.f);

private:
	float NetNow() const;
	UPROPERTY() UStaticMeshComponent* Pool = nullptr;
	UPROPERTY() UStaticMeshComponent* Sheen = nullptr;
	float MeshR = 50.f;
	float FxClock = 0.f;
};

/**
 * A bottle standing on the floor: cooking oil by the stove, water further into the kitchen. Heavy: about three ants holding [E]
 * together (the heavy-door rule, AKitchenInteractable::Pull, the HUD's «Нужно ещё N») tip it over, away from the side they push
 * from. It falls in half a second and the liquid runs out into an AKitchenSlick. When that has dried it stands up again.
 */
UCLASS()
class ANTKITCHEN_API AKitchenSpillBottle : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenSpillBottle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override { return !IsDoorOpen(); }
	virtual void Interact(AAntCharacter* Ant) override {}
	virtual float DistanceTo(const FVector& P) const override;
	virtual int32 DoorClass() const override;
	virtual bool IsDoorOpen() const override { return TipStart >= 0.f; }
	/** Server: true tips it over (away from whoever pushes), false stands it up again. */
	virtual void SetDoorOpen(bool bInOpen) override;
	virtual FString DoorNoun() const override { return TEXT("бутылку"); }

	UPROPERTY(Replicated) uint8 Kind = SlickKind::Oil;
	/** Server world time it started to fall (< 0: standing) and the world yaw it falls towards. */
	UPROPERTY(Replicated) float TipStart = -1.f;
	UPROPERTY(Replicated) float TipYaw = 0.f;

	FString BottleName() const { return Kind == SlickKind::Oil ? TEXT("Бутылка масла") : TEXT("Бутылка воды"); }
	static constexpr float Height = 620.f;
	static constexpr float Radius = 95.f;
	static constexpr float FallTime = 0.55f;

	/** Server, level v2: the oil by the stove and the water by the fridge. */
	static void SpawnAll(UWorld* World);
	static AKitchenSpillBottle* Nearest(const UWorld* World, const FVector& P, int32 OnlyKind = -1);

private:
	void BuildLook();
	float NetNow() const;
	/** Every machine: the bottle's lean (0 standing, 90 lying) from the replicated start time. */
	float Lean() const;
	UPROPERTY() USceneComponent* YawPivot = nullptr;
	UPROPERTY() USceneComponent* Edge = nullptr;
	UPROPERTY() USceneComponent* BodyRoot = nullptr;
	TWeakObjectPtr<AKitchenSlick> Puddle;
	TWeakObjectPtr<AActor> TippedBy;
	bool bSpilled = false;
	float StandAt = -1.f;
	bool bLandFx = false;
	float LastTip = -100.f;
};
