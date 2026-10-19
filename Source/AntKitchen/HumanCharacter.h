#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/NetSerialization.h"
#include "HumanCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

/**
 * The Human: a giant who stomps ants with slippers and sprays bug spray.
 * Controlled by AHumanAIController, or by a player who pressed H.
 */
UCLASS()
class ANTKITCHEN_API AHumanCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHumanCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	static constexpr float StompRadius = 420.f;
	static constexpr float SprayRange = 2300.f;

	UPROPERTY(Replicated) float Health = 2500.f;
	float MaxHealth = 2500.f;
	UPROPERTY(Replicated) float SprayFuel = 100.f;
	UPROPERTY(Replicated) bool bSpraying = false;
	UPROPERTY(ReplicatedUsing = OnRep_KnockedOut) bool bKnockedOut = false;
	UPROPERTY(Replicated) float StompReadyTime = 0.f;  // server world time
	/** 0..3: the more food the ants steal, the angrier (faster, longer spray, quicker stomps). */
	UPROPERTY(ReplicatedUsing = OnRep_Rage) uint8 Rage = 0;

	void SetRage(uint8 NewRage);
	float GetSprayRange() const { return SprayRange * (1.f + 0.25f * Rage); }
	float GetWalkSpeed() const { return bKnockedOut ? 260.f : 720.f * (1.f + 0.12f * Rage); }

	/** Server: start a stomp with leg 0 (left) / 1 (right), -1 = alternate. */
	void TryStomp(int32 Leg = -1);
	/** Server: rolled newspaper slap at a point (walls, tables, anywhere in reach). */
	void TrySwat(const FVector& Point);
	/** Server: grab the food nearest to Point, or throw what's in the hand along Dir. */
	void GrabOrThrow(const FVector& Point, const FVector& Dir);
	UPROPERTY(Replicated) class AKitchenLoot* Held = nullptr;
	UPROPERTY(Replicated) float SwatReadyTime = 0.f;
	static constexpr float SwatRadius = 300.f;
	static constexpr float SwatRange = 3200.f;

	/** Server: turn the bug spray on/off. */
	void SetSpraying(bool bOn);
	/** Server: full heal + teleport (new round). */
	void ResetHuman(const FVector& Location);

	FVector GetSprayOrigin() const;
	FVector GetSprayDirection() const;
	FVector GetStompPoint(int32 Leg) const;
	bool IsKnockedOut() const { return bKnockedOut; }
	FString GetDisplayName() const;

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void OnStompPressed();
	void OnSprayPressed();
	void OnSprayReleased();

	UFUNCTION(Server, Reliable) void ServerStomp();
	UFUNCTION(Server, Reliable) void ServerSetSpray(bool bOn);
	UFUNCTION(Server, Reliable) void ServerSwat(FVector_NetQuantize Point);
	UFUNCTION(Server, Reliable) void ServerGrabOrThrow(FVector_NetQuantize Point, FVector_NetQuantizeNormal Dir);
	UFUNCTION(NetMulticast, Unreliable) void MulticastSwat(FVector_NetQuantize Point);
	void OnSwatPressed();
	void OnGrabPressed();
	bool AimPoint(FVector& OutPoint, FVector& OutDir) const;
	UFUNCTION(NetMulticast, Unreliable) void MulticastStomp(uint8 Leg);
	UFUNCTION() void OnRep_KnockedOut();
	UFUNCTION() void OnRep_Rage();

private:
	void BuildBody();
	void DoStompImpact();
	void DoSprayDamage(float Dt);
	void KnockOut(AController* By);
	void UpdateVisuals(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;
	UPROPERTY(VisibleAnywhere) USceneComponent* VisualRoot;

	UPROPERTY() TArray<USceneComponent*> LegPivots;
	UPROPERTY() TArray<USceneComponent*> ArmPivots;
	UPROPERTY() TArray<UStaticMeshComponent*> Puffs;
	// Generated art: the whole body is one static model (no skeleton yet), a giant slipper slams for stomps.
	UPROPERTY() USceneComponent* ModelPivot = nullptr;
	UPROPERTY() UStaticMeshComponent* SlipperArt = nullptr;
	UPROPERTY() UStaticMeshComponent* SprayArt = nullptr;
	FTransform SlipperFit;
	UPROPERTY() class UAudioComponent* SprayAudio = nullptr;
	UPROPERTY() UStaticMeshComponent* PaperArt = nullptr;
	FVector SwatPoint = FVector::ZeroVector;
	float SwatLocalStart = -100.f;
	/** Turn the model if it faces the wrong way (multiples of 90). */
	static constexpr float ModelYaw = 0.f;

	bool bStompPending = false;
	float StompImpactTime = 0.f;
	uint8 PendingLeg = 0;
	uint8 NextLeg = 0;
	float KnockoutEnd = 0.f;
	float LastHurtTime = -100.f;
	float SprayAccum = 0.f;
	float LocalStompStart = -100.f;
	uint8 LocalStompLeg = 0;
	float WalkPhase = 0.f;
};
