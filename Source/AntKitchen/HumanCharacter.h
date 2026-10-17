#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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

	/** Server: start a stomp with leg 0 (left) / 1 (right), -1 = alternate. */
	void TryStomp(int32 Leg = -1);
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
	UFUNCTION(NetMulticast, Unreliable) void MulticastStomp(uint8 Leg);
	UFUNCTION() void OnRep_KnockedOut();

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
