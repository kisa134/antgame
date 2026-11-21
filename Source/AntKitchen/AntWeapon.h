#pragma once

#include "CoreMinimal.h"
#include "KitchenFixture.h"
#include "AntWeapon.generated.h"

class AHumanCharacter;
class AAntKitchenGameMode;
class UStaticMeshComponent;

/**
 * The Human's weapons (docs/FEATURES.md, section 23.3): anything he picks up and swings - the newspaper he always has, a slipper,
 * a ladle, a spatula, a book, a rolling pin, a pan, a broom, the kid's baseball bat. Each has its wind-up (never under 0.4 s:
 * the tell), reach, radius, cooldown, damage to ants and a strike on the world (impulse and damage that go to AntWreck::Hit):
 * a heavy one smashes the furniture and the food, and the ants scavenge the pieces.
 *
 * The AI trades the newspaper for heavier things as his rage grows (the bat at rage 3) and, at rage 3, takes it out on the
 * furniture when no ant is in reach. A player Human picks one up with [E] (aim at it) and swings it with [Q].
 *
 * Ants disarm him: a weapon lying about is held [E] together (strength 28, two ants) - a heavy one is swapped for a sausage
 * (a silly thing that hurts nobody), a light one is dragged off and hidden. Bitten on the right hand, stunned or down in a
 * puddle, he drops what he holds.
 */
namespace AntWeapons
{
	enum EKind : uint8 { None = 0, Slipper, Ladle, Spatula, Book, RollingPin, Pan, Broom, Bat, Sausage, Count };

	struct FDef
	{
		const TCHAR* Name;      // Russian, for the HUD
		const TCHAR* Mesh;      // a kitchen mesh, or null: built from shapes
		float Length;           // uu, grip to tip
		float Kg;
		float Windup;           // s, never under 0.4 (the tell)
		float Every;            // s between swings
		float Radius;           // uu, where it lands
		float Reach;            // extra reach over the newspaper, uu
		float AntDamage;
		float Impulse;          // kg m/s to the world
		float WorldDamage;      // hit points to the furniture
		uint8 Tier;             // 0 silly, 1 light, 2 medium, 3 heavy
	};
	ANTKITCHEN_API const FDef& Def(uint8 Kind);
	/** The newspaper (no weapon in hand). */
	ANTKITCHEN_API const FDef& Paper();

	/** Server: the swing of H landed at Point (the newspaper or the weapon in his hand): the world part of it. */
	ANTKITCHEN_API void OnSwingLanded(AHumanCharacter* H, const FVector& Point);
	/** Server, from the Human's grab key: picks up the weapon aimed at, or drops the one in hand. True if it did. */
	ANTKITCHEN_API bool TryPickOrDrop(AHumanCharacter* H, const FVector& Point);
	/** Server: drops whatever he holds (bitten hand, stun, a fall). */
	ANTKITCHEN_API void Drop(AHumanCharacter* H, const TCHAR* Why);
	/** Server, AI: going for a weapon or taking it out on the furniture. True: the AI does this now (Goal: where to walk, zero: stand). */
	ANTKITCHEN_API bool AiTick(AHumanCharacter* H, class AAntCharacter* Target, float Dt, FVector& Goal, FVector& Face);
	/** Server: the weapons of a raid in their places (the bat in the living room by the toy box). */
	ANTKITCHEN_API void SpawnAll(UWorld* World);
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	ANTKITCHEN_API float SwingWindup(const AHumanCharacter* H, float PaperWindup);
	/** What he swings right now (null: the newspaper). */
	ANTKITCHEN_API const FDef* Held(const AHumanCharacter* H);
}

UCLASS()
class ANTKITCHEN_API AHumanWeapon : public AKitchenHeld
{
	GENERATED_BODY()

public:
	AHumanWeapon();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FString GetPrompt(const AAntCharacter* Ant) const override;
	virtual float DistanceTo(const FVector& P) const override;
	virtual FString DoorNoun() const override;
	virtual bool CanInteract(const AAntCharacter* Ant) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Kind) uint8 Kind = AntWeapons::Bat;
	UPROPERTY(ReplicatedUsing = OnRep_Holder) AHumanCharacter* Holder = nullptr;
	/** Lying: where (replicated as a pose; it does not move on its own). */
	UPROPERTY(ReplicatedUsing = OnRep_Holder) FTransform LiePose;
	/** Hidden away by the ants (under the sofa...). */
	UPROPERTY(Replicated) bool bStashed = false;

	UFUNCTION() void OnRep_Kind();
	UFUNCTION() void OnRep_Holder();

	const AntWeapons::FDef& D() const { return AntWeapons::Def(Kind); }
	/** Server. */
	void PickUp(AHumanCharacter* H);
	void LayDown(const FVector& At, float Yaw, bool bLean = false);
	FVector Tip() const;

protected:
	virtual void OnDone() override;

private:
	void Build();
	void Attach();
	UPROPERTY() TArray<UStaticMeshComponent*> Parts;
	uint8 BuiltKind = 255;
};
