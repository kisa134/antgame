#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "KitchenLoot.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class AAntCharacter;

/** Cosmetic only (spawned on clients): little crumbs fly out when food breaks or gets bitten. */
UCLASS()
class ANTKITCHEN_API AFoodCrumbs : public AActor
{
	GENERATED_BODY()
public:
	AFoodCrumbs();
	virtual void Tick(float DeltaSeconds) override;
	void Burst(const FLinearColor& Color, int32 Count, float Size);
private:
	UPROPERTY() TArray<UStaticMeshComponent*> Bits;
	float Age = 0.f;
	float BaseSize = 10.f;
};

/**
 * Something on the kitchen floor: food or gear (a new weapon).
 * Food is not pocketed: ants grab it and DRAG it home. Every piece has a weight; the strength of
 * all ants holding it decides whether it is lifted, dragged slowly or doesn't budge. Ants of the
 * other colony can grab the same piece - then it's a tug of war.
 */
UCLASS()
class ANTKITCHEN_API AKitchenLoot : public AActor
{
	GENERATED_BODY()

public:
	AKitchenLoot();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	static constexpr float KgPerWeight = 2.f;

	void InitFood(const FAntFoodDef& Def);
	void InitGear(const FAntWeapon& Weapon);

	/** Server: an ant grabs / lets go. */
	bool TryGrab(AAntCharacter* Ant, FString& Why);
	void Release(AAntCharacter* Ant);
	void ReleaseAll();

	bool IsCarried() const { return Carriers.Num() > 0; }
	int32 NumCarriers() const { return Carriers.Num(); }
	/** Summed strength of the team that is winning the tug of war, minus the losers. */
	float NetStrength() const { return CarryStrength; }
	/** Strength / weight. >= 1 lifted, 0.5..1 dragged slowly, < 0.5 stuck. */
	float CarryRatio() const { return Weight > 0 ? CarryStrength / (float)Weight : 10.f; }
	uint8 CarryTeam() const { return CarrierTeam; }
	float GetRadius() const;
	/** How many ants of strength S it takes to lift this. */
	int32 AntsNeeded(float S) const { return FMath::Max(1, FMath::CeilToInt((float)Weight / FMath::Max(1.f, S))); }

	/** Server: a kick (stomp, explosion). */
	void Kick(const FVector& Impulse, const FVector& At);
	/** Server: smash into N smaller pieces (fell from high up, exploded...). */
	void Shatter(const FVector& At);
	/** Server: an ant bites a crumb off this. Returns the crumb (or null if too small). */
	AKitchenLoot* BiteOff(AAntCharacter* By);
	bool CanBreak() const { return !bIsGear && Weight >= 6; }

	/** Server: the Human picks it up (attached to his hand) / throws it. */
	void PickUpBy(USceneComponent* Hand, const FVector& RelativeLocation);
	void Throw(const FVector& Velocity);
	UPROPERTY(ReplicatedUsing = OnRep_Held) bool bHeld = false;
	UFUNCTION() void OnRep_Held();

	UPROPERTY(Replicated) TArray<AAntCharacter*> Carriers;
	UPROPERTY(Replicated) float CarryStrength = 0.f;
	UPROPERTY(Replicated) uint8 CarrierTeam = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Visual) FString LootName;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) int32 Value = 5;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) int32 Weight = 1;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FLinearColor Color = FLinearColor::White;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) uint8 ShapeIndex = 1;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FVector VisualScale = FVector(0.2f);
	UPROPERTY(ReplicatedUsing = OnRep_Visual) bool bIsGear = false;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FAntWeapon Gear;

	UFUNCTION()
	void OnRep_Visual();

private:
	void RefreshVisual();
	UFUNCTION() void OnBodyHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	AKitchenLoot* SpawnPiece(const FString& Name, int32 PieceValue, int32 PieceWeight, const FVector& Where, const FVector& Fling);
	void TickWater(float Dt);

	UPROPERTY(VisibleAnywhere) UBoxComponent* Body;   // physics body for food
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Beam;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* Glow;
	UPROPERTY() UMaterialInstanceDynamic* MeshMID = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* BeamMID = nullptr;

	void ServerCarryTick(float Dt);
	void SetupPhysics(const FVector& HalfExtent);
	FVector HoldPoint(const AAntCharacter* Ant) const;

	/** Where each carrier bit into the food (local space of the body), same order as Carriers. Server. */
	TArray<FVector> GrabLocal;
	float Phase = 0.f;
	bool bPhysicsReady = false;
	FVector LastVelocity = FVector::ZeroVector;
	float BreakableAfter = 0.f;
	float NextThud = 0.f;
	bool bShatterPending = false;
	FVector ShatterAt = FVector::ZeroVector;
	FVector ArtOffset = FVector::ZeroVector;
	bool bHasArt = false;
};
