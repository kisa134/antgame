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
class AAntChain;

/** Status bits replicated to everyone (HUD, visuals, client-side movement speed). */
namespace AntStatus
{
	constexpr uint8 Wet = 1 << 0;
	constexpr uint8 Sticky = 1 << 1;
	constexpr uint8 Drowning = 1 << 2;
	constexpr uint8 Shocked = 1 << 3;
	constexpr uint8 Stream = 1 << 4;
	constexpr uint8 Burning = 1 << 5;
	constexpr uint8 Webbed = 1 << 6;
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
	virtual void PawnClientRestart() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Ants are small: everything about the body is scaled by this (the kitchen stays the same). */
	/** Second shrink (a real ~8 mm ant): every body-sized distance tuned for the 0.48 ant is multiplied by this. */
	static constexpr float AntShrink = AntKit::AntShrink;
	static constexpr float AntScale = 0.48f * AntShrink;
	static constexpr float SpeedK = AntKit::SpeedK;
	static constexpr float JumpK = AntKit::JumpK;
	static constexpr float InteractRadius = 140.f * AntShrink;
	static constexpr float BodyRadius = 26.f * AntScale;
	// Orientation of the generated (Meshy) parts: flip if a part shows up backwards.
	static constexpr bool ArtFlipBody = false;
	static constexpr bool ArtFlipFemur = false;
	static constexpr bool ArtFlipTibia = false;
	static constexpr bool ArtFlipFlagellum = false;

	UPROPERTY(Replicated, BlueprintReadOnly) float Health = 100.f;
	UPROPERTY(BlueprintReadOnly) float MaxHealth = 100.f;
	/** Food this ant is holding in its mandibles (dragged together with other ants). */
	UPROPERTY(ReplicatedUsing = OnRep_Carry) AKitchenLoot* Carrying = nullptr;
	/** 0 = our colony, 1 = rival fire ants. */
	UPROPERTY(Replicated) uint8 Team = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Class) EAntClass AntClass = EAntClass::Worker;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(ReplicatedUsing = OnRep_Weapon) FAntWeapon Weapon;
	UPROPERTY(ReplicatedUsing = OnRep_BodyColor) FLinearColor BodyColor = FLinearColor(0.12f, 0.04f, 0.03f);
	UPROPERTY(Replicated) bool bSprinting = false;

	/** Body temperature, °C. Cold slows you down, heat burns. */
	UPROPERTY(Replicated) float Temperature = 22.f;
	UPROPERTY(Replicated) uint8 StatusFlags = 0;

	/** Pheromone trail being laid right now (food -> nest). Everyone sees the glowing dots. */
	UPROPERTY(Replicated) TArray<FVector_NetQuantize> TrailPoints;
	UPROPERTY(Replicated) bool bLayingTrail = false;
	static constexpr float TrailStep = 140.f;
	static constexpr int32 TrailMaxPoints = 160;

	UAntMovementComponent* GetAntMovement() const;
	bool IsCrawling() const;
	/** The Human this ant is crawling on right now (climbed up his leg), or null. */
	class AHumanCharacter* GetRiddenHuman() const;
	bool IsClimbing() const;     // on a wall or ceiling
	bool IsFrozen() const { return Temperature < -5.f; }
	bool HasStatus(uint8 Bit) const { return (StatusFlags & Bit) != 0; }
	bool IsInNest() const;
	bool IsCarrying() const { return Carrying != nullptr; }
	/** 0 when not carrying; the load's weight otherwise. */
	int32 CarriedWeight() const;
	bool IsEnemy(const AAntCharacter* Other) const { return Other && Other->Team != Team; }
	const FAntClassDef& GetClassDef() const { return AntKit::ClassDef(AntClass); }
	float GetStrength() const { return StrengthOverride >= 0.f ? StrengthOverride : GetClassDef().Strength; }

	// ---- Creatures (fly, spider) reuse the ant: same health, teams, legs, crawling, AI ----
	/** 0 ant, 1 fly, 2 spider. Set by the subclass. */
	uint8 Creature = 0;
	bool IsMonster() const { return Team >= 2; }
	float ExtraScale = 1.f;        // body size on top of the caste
	float SpeedScale = 1.f;
	float DamageScale = 1.f;
	float HealthOverride = 0.f;    // > 0: ignores the caste
	float StrengthOverride = -1.f; // >= 0: ignores the caste (0 = can't carry)
	float LegLength = 1.f;
	float LegThickness = 1.f;
	const TCHAR* LegUpperArt = TEXT("SM_Ant_Femur");
	const TCHAR* LegLowerArt = TEXT("SM_Ant_Tibia");
	float DamageMult() const { return GetClassDef().Damage * DamageScale; }
	/** Where carried food is held (mandibles; under the body for a fly). */
	virtual FVector GetHoldPoint() const;
	/** Server: stuck in spider silk for a while. */
	void Webbed(float Seconds);
	/** Server: bite (players: right mouse; AI calls this directly). */
	void BotBite();
	/** Server: AI shoots in this direction (same rate limits as players). */
	void BotFire(const FVector& Direction);
	/** Server: change caste (only inside the own nest unless bForce). */
	void SetClass(EAntClass NewClass, bool bForce);
	/** Server, called by the loot. */
	void SetCarrying(AKitchenLoot* Loot);
	/** Server: grab / let go of the nearest food (bots use this too). */
	bool GrabLoot(AKitchenLoot* Loot);
	void LetGo();
	void DevInteract();
	/** Local camera shake (0..1, adds up, decays). */
	void AddTrauma(float Amount) { Trauma = FMath::Min(1.f, Trauma + Amount); }
	/** Server: set on fire for a few seconds (spreads to ants nearby, water puts it out). */
	void Ignite(float Seconds);
	bool IsBurning() const { return HasStatus(AntStatus::Burning); }
	float VisScale() const { return AntScale * GetClassDef().BodyScale * ExtraScale; }
	bool bBodycam = false;
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

	// ---- Holding each other: carry a friend, fly with an ant, living chains and bridges ----
	/** The chain/bridge this ant is a link of. */
	UPROPERTY(ReplicatedUsing = OnRep_Link) AAntChain* Chain = nullptr;
	/** Who carries this ant (an ant in its mandibles, a fly in its legs). */
	UPROPERTY(ReplicatedUsing = OnRep_Link) AAntCharacter* HeldBy = nullptr;
	/** The ant this one is carrying. */
	UPROPERTY(Replicated) AAntCharacter* HoldingAnt = nullptr;
	bool IsAttached() const { return Chain != nullptr || HeldBy != nullptr; }
	/** How heavy this ant is to lift. */
	float AntWeight() const;
	/** Server. */
	bool GrabAnt(AAntCharacter* Other);
	void ReleaseAnt(bool bThrow);
	void SetChain(AAntChain* NewChain);
	/** Server: the [G] action (also used by bots). */
	void DoLink(const FVector& Aim);
	/** Ally ant close enough to grab, or null. */
	AAntCharacter* FindGrabbableAlly(float Radius) const;
	/** Server: switch colony (only in the nest or while dead). */
	bool CanSwitchTeam() const;

	/** Last kitchen hazard that hurt us (server), for the kill feed. */
	FString LastHazard;
	float LastHazardTime = -100.f;

protected:
	virtual void MoveForward(float Value);
	virtual void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void StartFire();
	void StopFire();
	void Interact();
	void StartSprint();
	void StopSprint();
	virtual void DoJump();
	virtual void DoStopJump();
	void ToggleTrail();
	void RecallSwarm();

	UFUNCTION(Server, Unreliable) void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);
	UFUNCTION(Server, Reliable) void ServerInteract();
	UFUNCTION(Server, Reliable) void ServerSetSprint(bool bNewSprint);
	UFUNCTION(Server, Reliable) void ServerToggleTrail();
	UFUNCTION(Server, Reliable) void ServerRecallSwarm();
	UFUNCTION(Server, Reliable) void ServerSetClass(uint8 NewClass);
	UFUNCTION(Server, Reliable) void ServerBite();
	UFUNCTION(Server, Reliable) void ServerLink(FVector_NetQuantize Aim);
	void LinkPressed();
	/** Right mouse: bite for ants and flies, web-swing for the spider. */
	virtual void AltFirePressed();
	virtual void AltFireReleased() {}
	UFUNCTION() void OnRep_Link();
	void ApplyAttachedPose();
	bool StartChainHere(const FVector& Aim);
	/** Commander: 0 summon/dismiss army, 1 army to point (or follow), 2 bots follow me, 3 bots attack point, 4 bots free. */
	UFUNCTION(Server, Reliable) void ServerCommand(uint8 Cmd, FVector_NetQuantize Point);
	void CmdArmy();
	void CmdArmyTarget();
	void CmdFollow();
	void CmdAttack();
	void CmdFree();
	FVector AimPoint() const;
	void Bite();
	void ToggleBodycam();
	void ApplyCameraLook();
	void PickClass1();
	void PickClass2();
	void PickClass3();
	void PickClass4();
	UFUNCTION(Client, Reliable) void ClientLaunch(FVector_NetQuantize10 Velocity);
	UFUNCTION(Client, Reliable) void ClientTeleport(FVector_NetQuantize Location);

	UFUNCTION() void OnRep_Carry();
	UFUNCTION() void OnRep_Dead();
	UFUNCTION() void OnRep_Weapon();
	UFUNCTION() void OnRep_BodyColor();
	UFUNCTION() void OnRep_Class();

protected:
	virtual void BuildBody();
	/** Frost / fire / web shells and the old carry chunk (every creature needs them). */
	void BuildStatusFx();
	virtual void ApplyClassStats();
	void TickNurse(float Dt);
	void SurfaceAxes(FVector& OutForward, FVector& OutRight) const;
	void TickFire();
	void UpdateSpeed();
	void UpdateVisuals(float DeltaSeconds);
	void UpdateLegs(float DeltaSeconds, const FVector& N);
	FVector EstimateSurfaceNormal() const;
	void Die(AController* Killer, AActor* Causer);
	void TickTrail();
	void CancelTrail(const TCHAR* Why);
	void UpdateTrailDots();

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
	UPROPERTY() TArray<UStaticMeshComponent*> TrailDots;
	UPROPERTY() TArray<UStaticMeshComponent*> LegCoxas;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> ArtMIDs;
	UPROPERTY() UMaterialInstanceDynamic* GlandArtMID = nullptr;
	bool bArtLegs = false;

	TArray<FVector> TrailNormals;                 // server
	TWeakObjectPtr<AKitchenLoot> TrailFood;       // server

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
	float NurseTimer = 0.f;
	float Trauma = 0.f;
	float FootstepVolume = 0.f;
	float LastHealthSeen = -1.f;
	float NextBiteTime = 0.f;
	float ServerNextBiteTime = 0.f;
	float BurnTime = 0.f;           // server
	UPROPERTY() UStaticMeshComponent* FlameShell = nullptr;
	UPROPERTY() class UPointLightComponent* FlameLight = nullptr;
	UPROPERTY() UStaticMeshComponent* WebShell = nullptr;
	float WebTime = 0.f;            // server
	FVector AttachedUp = FVector::UpVector;
	FVector AttachedAlong = FVector::ForwardVector;
	float AttachedSince = 0.f;
};
