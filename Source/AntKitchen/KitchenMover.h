#pragma once

#include "CoreMinimal.h"
#include "KitchenInteractables.h"
#include "KitchenMover.generated.h"

class UStaticMeshComponent;

/**
 * One moving part of the modelled kitchen that no special gadget drives: a cabinet door, a drawer, the freezer door,
 * the crisper, a dishwasher rack, the matchbox tray. [E] opens and closes it. The part, its hinge or slide and its
 * limits come from the art manifest (KitchenArt::Movers()); only the index and the open flag are replicated, every
 * machine builds the mesh itself. Its collision moves with it: ants stand on an open door and ride a drawer.
 */
UCLASS()
class ANTKITCHEN_API AKitchenMover : public AKitchenInteractable
{
	GENERATED_BODY()

public:
	AKitchenMover();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual void Interact(AAntCharacter* Ant) override;

	/** Server: open/close; AutoClose > 0 shuts it again after that many seconds. */
	void SetOpen(bool bInOpen, float AutoClose = -1.f);
	bool IsOpen() const { return bOpen; }
	virtual int32 DoorClass() const override;
	virtual bool IsDoorOpen() const override { return bOpen; }
	virtual void SetDoorOpen(bool bInOpen) override;
	virtual bool CanPull(AAntCharacter* Ant, bool bTell) const override;
	virtual bool CanPullFrom(const FVector& P) const override;
	/** Level v2: the cat flap in the living room's wall. Latched: only ants on the living-room side can open it. */
	bool IsCatFlap() const;
	virtual bool HumanShuts() const override;
	virtual FString DoorNoun() const override;

	/** Server, the Human's "kitchen life": he opens one cabinet door or drawer for a while. False if none was free. */
	static bool HumanOpensOne(UWorld* World, FRandomStream& Rng, float Seconds);

	/** Which part: index into KitchenArt::Movers(). */
	UPROPERTY(Replicated) int32 PartIndex = -1;
	UPROPERTY(Replicated) bool bOpen = false;

private:
	void Build();
	void ApplyAlpha();
	/** The door or drawer this part hides behind is shut (the crisper behind the fridge door, the racks in the dishwasher). */
	bool IsBlocked() const;

	UPROPERTY() USceneComponent* Hinge = nullptr;
	UPROPERTY() UStaticMeshComponent* Body = nullptr;

	float Alpha = 0.f;
	float AutoCloseAt = -1.f;
	float ServerCheck = 0.f;
};