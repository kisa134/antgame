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
class UProceduralMeshComponent;
namespace AntArt { struct FPart; }
namespace AntShards { struct FChunk; }

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
	bool CanBreak() const { return !bIsGear && !bVault && !bBody && Weight >= 6; }
	/** A dead teammate's body: hauled home like food, it gives the colony a larva back. Gone after about a minute. */
	UPROPERTY(Replicated) bool bBody = false;
	/** Server. The player (or bot) the body belonged to. */
	TWeakObjectPtr<AController> BodyOf;
	/** Server. World time this piece disappears (bodies, the pizza slices); < 0 = never. */
	float ExpireAt = -1.f;
	/** Server. Where it was lying when it appeared: KitchenContainers index + 1 (0 = in the open), and was it in the living room. */
	uint8 Origin = 0;
	bool bFromLiving = false;
	bool bOriginSet = false;
	/** Server. The last ant that held it (who dropped the trophy, who dropped this on the Human). */
	TWeakObjectPtr<AAntCharacter> LastCarrier;
	/** War mode: the sugar-bowl safe. Not food: it goes to a cashout, never into a nest. */
	UPROPERTY(Replicated) bool bVault = false;
	/** Heist: a big prize placed up high. Delivered whole it is worth double; its pieces are worth the plain value. */
	UPROPERTY(ReplicatedUsing = OnRep_Visual) bool bTrophy = false;
	/** Server. Level v2: the main object of the shift (the slice of birthday cake). A trophy that is hauled by the whole column:
	 *  the army of a carrier leans on it the way it leans on a heavy door (up to 30 strength). */
	bool bMain = false;

	/** Server: a raid squad lifts it (it rides on the squad) / puts it down. Any ant that bites into it takes it away from the squad. */
	void SquadPick(AActor* Squad, USceneComponent* At, const FVector& RelativeLocation);
	void SquadDrop();
	TWeakObjectPtr<AActor> SquadHolder;
	/** Server. A special find (AntFind id, 0 = plain food): its own small effect when it gets home. */
	UPROPERTY(Replicated) uint8 Special = 0;
	/** Server: where the main object last lay inside the world. */
	FVector LastSafe = FVector::ZeroVector;
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

	/**
	 * Real fracture: this piece is a chunk cut out of the model. (index, count) pairs, see AntShards::Get.
	 * Empty for whole food and for crumbs bitten off.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Visual) TArray<uint8> ShardPath;
	UPROPERTY(ReplicatedUsing = OnRep_Visual) int32 ShardSeed = 0;
	/** Scale of the whole model the chunk was cut from. */
	UPROPERTY(ReplicatedUsing = OnRep_Visual) FVector ShardScale = FVector::OneVector;

	UFUNCTION()
	void OnRep_Visual();

private:
	void RefreshVisual();
	UFUNCTION() void OnBodyHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	AKitchenLoot* SpawnPiece(const FString& Name, int32 PieceValue, int32 PieceWeight, const FVector& Where, const FVector& Fling, int32 ShardIndex = -1, int32 ShardCount = 0, int32 Seed = 0);
	/** Shows this piece as its cut chunk. False = not a chunk (or the mesh can't be cut): the plain model is used. */
	bool BuildShard(const AntArt::FPart* Art);
	UPROPERTY() UProceduralMeshComponent* Shard = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* CapMID = nullptr;
	TSharedPtr<const AntShards::FChunk> ShardBuilt;
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
public:
	/** Where this carrier holds the food, in the world (server). False if it is not one of the carriers. */
	bool GrabPointOf(const AAntCharacter* Ant, FVector& Out) const;
private:

	/** Where each carrier bit into the food (local space of the body), same order as Carriers. Server. */
	TArray<FVector> GrabLocal;
	float Phase = 0.f;
	bool bPhysicsReady = false;
	FVector LastVelocity = FVector::ZeroVector;
	float BreakableAfter = 0.f;
	float NextThud = 0.f;
	float NextCrash = 0.f;
	float LastKickTime = -100.f;   // kicked by a stomp / thrown by the Human: what comes down then is his own doing
	bool bShatterPending = false;
	FVector ShatterAt = FVector::ZeroVector;
	FVector ArtOffset = FVector::ZeroVector;
	bool bHasArt = false;
	/** Cosmetic: how far it has been dragged since the last crumbs. */
	FVector DragFxLast = FVector::ZeroVector;
	float DragFxDist = 0.f;
};
