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
class AKitchenInteractable;
class UAntMovementComponent;

/** Status bits replicated to everyone (HUD, visuals, client-side movement speed). */
namespace AntStatus
{
	constexpr uint8 Wet = 1 << 0;
	constexpr uint8 Sticky = 1 << 1;
	constexpr uint8 Drowning = 1 << 2;
	constexpr uint8 Shocked = 1 << 3;
	constexpr uint8 Stream = 1 << 4;
}

/** What the kitchen does to an ant right now (filled on the server by the zones and hazards). */
struct FAntEnv
{
	float TargetTemp = 22.f;
	float Rate = 4.f;          // degrees per second towards TargetTemp
	uint8 Flags = 0;
	float Dps = 0.f;
	FString Cause;

	/** A stronger temperature source wins. */
	void Temp(float Target, float InRate)
	{
		if (InRate > Rate)
		{
			TargetTemp = Target;
			Rate = InRate;
		}
	}
	void Hurt(float InDps, const TCHAR* InCause)
	{
		Dps += InDps;
		Cause = InCause;
	}
};

/** One procedural leg: the foot is planted in the world and steps when it lags behind the body. */
struct FAntLeg
{
	FVector HipLocal = FVector::ZeroVector;
	FVector RestLocal = FVector::ZeroVector;
	int32 Side = 1;
	int32 Group = 0;            // tripod gait: legs of one group step together
	FVector Foot = FVector::ZeroVector;
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float T = 1.f;              // < 1 while stepping
	bool bInit = false;
};

/**
 * The playable ant. Crawls on any surface, shoots acid, loots food and gear,
 * carries food back to the nest for team score, uses kitchen gadgets.
 */
UCLASS()
class ANTKITCHEN_API AAntCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAntCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	static constexpr int32 MaxCarry = 10;
	static constexpr float InteractRadius = 190.f;
	static constexpr float BodyRadius = 26.f;

	UPROPERTY(Replicated, BlueprintReadOnly) float Health = 100.f;
	UPROPERTY(BlueprintReadOnly) float MaxHealth = 100.f;
	UPROPERTY(ReplicatedUsing = OnRep_Carry) int32 CarriedValue = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Carry) int32 CarriedWeight = 0;
	UPROPERTY(Replicated) int32 CarriedItems = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(ReplicatedUsing = OnRep_Weapon) FAntWeapon Weapon;
	UPROPERTY(ReplicatedUsing = OnRep_BodyColor) FLinearColor BodyColor = FLinearColor(0.12f, 0.04f, 0.03f);
	UPROPERTY(Replicated) bool bSprinting = false;

	/** Body temperature, °C. Cold slows you down, heat burns. */
	UPROPERTY(Replicated) float Temperature = 22.f;
	UPROPERTY(Replicated) uint8 StatusFlags = 0;

	UAntMovementComponent* GetAntMovement() const;
	bool IsCrawling() const;
	bool IsClimbing() const;     // on a wall or ceiling
	bool IsFrozen() const { return Temperature < -5.f; }
	bool HasStatus(uint8 Bit) const { return (StatusFlags & Bit) != 0; }
	bool IsInNest() const;
	AKitchenLoot* FindNearestLoot(float Radius) const;
	AKitchenInteractable* FindNearestInteractable(float* OutDistance = nullptr) const;
	FVector GetMuzzleLocation() const;
	FVector GetGroundLocation() const;
	FVector GetVisualUp() const { return VisNormal; }
	FString GetDisplayName() const;

	/** Server: drop carried food on the ground (death, becoming the Human...). */
	void DropEverything();
	/** Server: knock the ant around. Works with client-side movement. */
	void ApplyLaunch(const FVector& Velocity);
	/** Server: move the ant somewhere else (vents, drains). */
	void ApplyTeleport(const FVector& Location);
	/** Server, called by the kitchen 4x per second. */
	void ApplyEnvironment(const FAntEnv& Env, float Dt);
	/** Server: show a message to this ant's player. */
	void Toast(const FString& Msg) const;
	/** Server: damage from a kitchen hazard; Hazard is used in the kill feed ("попал в мышеловку"). */
	void HurtBy(float Damage, const FString& Hazard);

	/** Last kitchen hazard that hurt us (server), for the kill feed. */
	FString LastHazard;
	float LastHazardTime = -100.f;

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
	UFUNCTION(Client, Reliable) void ClientLaunch(FVector_NetQuantize10 Velocity);
	UFUNCTION(Client, Reliable) void ClientTeleport(FVector_NetQuantize Location);

	UFUNCTION() void OnRep_Carry();
	UFUNCTION() void OnRep_Dead();
	UFUNCTION() void OnRep_Weapon();
	UFUNCTION() void OnRep_BodyColor();

private:
	void BuildBody();
	void SurfaceAxes(FVector& OutForward, FVector& OutRight) const;
	void TickFire();
	void UpdateSpeed();
	void UpdateVisuals(float DeltaSeconds);
	void UpdateLegs(float DeltaSeconds, const FVector& N);
	FVector EstimateSurfaceNormal() const;
	void Die(AController* Killer, AActor* Causer);

	UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;
	UPROPERTY(VisibleAnywhere) USceneComponent* VisualRoot;

	UPROPERTY() TArray<UMaterialInstanceDynamic*> BodyMIDs;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> LegMIDs;
	UPROPERTY() TArray<UStaticMeshComponent*> LegFemurs;
	UPROPERTY() TArray<UStaticMeshComponent*> LegTibias;
	UPROPERTY() TArray<UStaticMeshComponent*> LegKnees;
	UPROPERTY() TArray<USceneComponent*> AntennaPivots;
	UPROPERTY() UStaticMeshComponent* CarryChunk = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* GlandMID = nullptr;
	UPROPERTY() UStaticMeshComponent* FrostShell = nullptr;

	TArray<FAntLeg> Legs;
	FVector VisNormal = FVector::UpVector;
	FVector VisHeading = FVector::ForwardVector;
	float AnimTime = 0.f;

	bool bWantsFire = false;
	float NextFireTime = 0.f;
	float ServerNextFireTime = 0.f;
	float LastDamageTime = -100.f;
	float LastEnvToast = -100.f;
	float DamageAccum = 0.f;
};
