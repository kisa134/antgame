#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "AntSwing.generated.h"

/**
 * A part that swings on its pins (the bucket's handle): a pendulum about one local axis of its parent, driven by gravity and by
 * how the parent moves (a knocked bucket throws its handle about), stopped by the limits where it meets the rim.
 * Cosmetic and local to every machine (the parent's pose is what replicates). Sleeps when it and its parent are still.
 * The parent's visibility is mirrored (a broken bucket hides its mesh: the handle goes with it).
 */
UCLASS()
class ANTKITCHEN_API UAntSwingComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UAntSwingComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pivot in the parent's mesh space (uu) and the hinge axis there (unit). */
	FVector Pivot = FVector::ZeroVector;
	FVector Axis = FVector::ForwardVector;
	/** Rest angle and the limits (deg) about Axis from the modelled (upright) pose. */
	float AngleDeg = 0.f;
	float LimitDeg = 120.f;
	/** Pendulum length (uu, pivot to the centre of mass of the part). */
	float Length = 200.f;
	/** Kick it (deg/s), e.g. when the parent is hit. */
	void Kick(float DegPerSec) { Vel += DegPerSec; bAwake = true; }

	/** Build: Name's companion mesh "<Name>_Handle" on Parent, from the hero manifest numbers baked into the table below. */
	static UAntSwingComponent* AddTo(UStaticMeshComponent* Parent, const TCHAR* MeshName);

private:
	void Apply();
	float Vel = 0.f;
	bool bAwake = true;
	float Still = 0.f;
	FVector LastParentVel = FVector::ZeroVector;
	FVector LastParentLoc = FVector::ZeroVector;
	FQuat LastQuat = FQuat::Identity;
	bool bHaveLast = false;
};
