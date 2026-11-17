#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntGibs.generated.h"

class UStaticMeshComponent;

/**
 * Cosmetic only, spawned on every client: torn-off legs, body halves and drops of hemolymph. Each piece flies,
 * bounces off the kitchen and comes to rest; the whole lot is cleaned up after a while. No physics bodies:
 * dozens of these at once must cost nothing.
 */
UCLASS()
class ANTKITCHEN_API AAntGibs : public AActor
{
	GENERATED_BODY()

public:
	AAntGibs();
	virtual void Tick(float DeltaSeconds) override;

	/** A copy of Src (mesh, materials, place) that flies off. ScaleMul squeezes it along its own axes. */
	void AddCopy(const UStaticMeshComponent* Src, const FVector& Velocity, float Spin, const FVector& ScaleMul = FVector::OneVector, const FVector& Offset = FVector::ZeroVector);
	/** A drop that flies and sticks where it lands as a flat splat. Size = diameter in world units. */
	void AddGoo(const FVector& From, const FVector& Velocity, float Size, const FLinearColor& Color);

	float Life = 24.f;

private:
	struct FPiece
	{
		FVector Velocity = FVector::ZeroVector;
		FVector Axis = FVector::UpVector;
		float Spin = 0.f;
		float Radius = 1.f;
		float GooSize = 0.f;   // > 0: a drop
		bool bRest = false;
	};
	UPROPERTY() TArray<UStaticMeshComponent*> Comps;
	TArray<FPiece> Pieces;
	float Age = 0.f;
};