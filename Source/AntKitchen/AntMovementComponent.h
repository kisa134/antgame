#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AntMovementComponent.generated.h"

/** Custom movement modes (MOVE_Custom + this value). */
enum EAntMoveMode : uint8
{
	ANTMOVE_None = 0,
	ANTMOVE_Crawl = 1,   // glued to any surface: floor, wall, ceiling
};

/**
 * Ant locomotion: instead of "walking" on floors only, the ant crawls on any static surface.
 * - The collision is a sphere, so the capsule never needs to rotate.
 * - SurfaceNormal is the plane the ant is glued to; input is projected onto it.
 * - Concave corners (walking into a wall) and convex edges (walking off a table edge) are wrapped.
 * - Jumping pushes away from the surface; touching any surface while falling sticks again.
 */
UCLASS()
class ANTKITCHEN_API UAntMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UAntMovementComponent();

	bool IsCrawling() const { return MovementMode == MOVE_Custom && CustomMovementMode == ANTMOVE_Crawl; }
	/** Plane the ant stands on (world up while falling). */
	FVector GetSurfaceNormal() const { return IsCrawling() ? SurfaceNormal : FVector::UpVector; }
	void StartCrawl(const FVector& Normal);
	/** Don't glue to surfaces for a moment (after a launch). */
	void BlockSticking(float Seconds);

	/** Set every frame by the ant from its status (cold, honey, water, cargo...). */
	float SpeedMultiplier = 1.f;
	bool bNoJump = false;
	bool bNoClimb = false;       // can still walk on floors, but won't go up walls
	bool bHeavyLoad = false;     // falls off ceilings

	virtual float GetMaxSpeed() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	void PhysCrawl(float DeltaTime);
	bool TraceSurface(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	bool IsStickable(const FHitResult& Hit) const;
	void Detach(const FVector& Push);
	float Now() const;

	FVector SurfaceNormal = FVector::UpVector;
	FVector PendingStickNormal = FVector::UpVector;
	bool bPendingStick = false;
	float NoStickUntil = -10.f;
};
