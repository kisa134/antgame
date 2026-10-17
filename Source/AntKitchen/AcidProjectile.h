#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "AcidProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;

/** A blob of acid spat by an ant. Only hurts the Human (no friendly fire). */
UCLASS()
class ANTKITCHEN_API AAcidProjectile : public AActor
{
	GENERATED_BODY()

public:
	AAcidProjectile();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: call between SpawnActorDeferred and FinishSpawning. */
	void Setup(const FAntWeapon& Weapon, const FVector& Direction);

protected:
	UPROPERTY(VisibleAnywhere) USphereComponent* Sphere;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* Movement;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Visual;

	UPROPERTY(Replicated) FVector InitialVelocity = FVector::ZeroVector;
	UPROPERTY(Replicated) FLinearColor Color = FLinearColor(0.4f, 1.f, 0.2f);
	UPROPERTY(Replicated) float VisualSize = 0.12f;
	UPROPERTY(Replicated) float GravityScale = 0.15f;
	UPROPERTY(Replicated) bool bBounce = false;

	UFUNCTION() void OnStop(const FHitResult& Hit);
	UFUNCTION() void OnBounce(const FHitResult& Hit, const FVector& ImpactVelocity);
	UFUNCTION() void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void Explode(AActor* DirectHit);

	float Damage = 10.f;
	float Splash = 0.f;
	int32 Bounces = 0;
	bool bExploded = false;
};
