#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/NetSerialization.h"
#include "AntTypes.h"
#include "AntCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AKitchenLoot;

/**
 * The playable ant. Shoots acid, loots food and gear, climbs walls,
 * carries food back to the nest for team score.
 */
UCLASS()
class ANTKITCHEN_API AAntCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAntCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	static constexpr int32 MaxCarry = 10;
	static constexpr float InteractRadius = 190.f;

	UPROPERTY(Replicated, BlueprintReadOnly) float Health = 100.f;
	UPROPERTY(BlueprintReadOnly) float MaxHealth = 100.f;
	UPROPERTY(ReplicatedUsing = OnRep_Carry) int32 CarriedValue = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Carry) int32 CarriedWeight = 0;
	UPROPERTY(Replicated) int32 CarriedItems = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(ReplicatedUsing = OnRep_Weapon) FAntWeapon Weapon;
	UPROPERTY(ReplicatedUsing = OnRep_BodyColor) FLinearColor BodyColor = FLinearColor(0.12f, 0.04f, 0.03f);
	UPROPERTY(Replicated) bool bSprinting = false;

	bool IsClimbing() const { return bClimbing; }
	bool IsInNest() const;
	AKitchenLoot* FindNearestLoot(float Radius) const;
	FVector GetMuzzleLocation() const;
	FVector GetGroundLocation() const;
	FString GetDisplayName() const;

	/** Server: drop carried food on the ground (death, becoming the Human...). */
	void DropEverything();

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void StartFire();
	void StopFire();
	void Interact();
	void StartSprint();
	void StopSprint();
	void DoJump();
	void DoStopJump();

	UFUNCTION(Server, Unreliable) void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void ServerInteract();
	UFUNCTION(Server, Reliable) void ServerSetSprint(bool bNewSprint);

	UFUNCTION() void OnRep_Carry();
	UFUNCTION() void OnRep_Dead();
	UFUNCTION() void OnRep_Weapon();
	UFUNCTION() void OnRep_BodyColor();

private:
	void BuildBody();
	void UpdateClimb();
	void TickFire();
	void UpdateSpeed();
	void UpdateVisuals(float DeltaSeconds);
	void Die(AController* Killer, AActor* Causer);
	void Toast(const FString& Msg) const;

	UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;
	UPROPERTY(VisibleAnywhere) USceneComponent* VisualRoot;

	UPROPERTY() TArray<UMaterialInstanceDynamic*> BodyMIDs;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> LegMIDs;
	UPROPERTY() TArray<USceneComponent*> LegPivots;
	UPROPERTY() UStaticMeshComponent* CarryChunk = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* GlandMID = nullptr;

	float ForwardInput = 0.f;
	bool bClimbing = false;
	bool bWantsFire = false;
	float NextFireTime = 0.f;
	float ServerNextFireTime = 0.f;
	float LastDamageTime = -100.f;
	float WalkPhase = 0.f;
};
