#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/NetSerialization.h"
#include "HumanBody.h"
#include "HumanCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

/** What the Human's body is doing: HumanAct::EId, since when (server world time), at what. */
USTRUCT()
struct FHumanActState
{
	GENERATED_BODY()

	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() uint8 Arg = 0;
	UPROPERTY() float Start = -100.f;
	UPROPERTY() FVector_NetQuantize Target;
};

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
	/** Ants ride him. (A character is not a base for other characters by default: the engine made every ant that got a grip on him jump straight off.) */
	virtual bool CanBeBaseForCharacter(APawn* Pawn) const override { return true; }

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
	float GetWalkSpeed() const { return bStunned ? 1.f : bKnockedOut ? (Fig == HumanFig::Kid ? 1000.f : 260.f) : HumanBody::Style(Fig).WalkSpeed * (1.f + 0.12f * Rage); }

	// ---- The family: grandma and the kid are this same jointed figure with their own tables, size and habits (FolkCharacters.h) ----
	/** HumanFig: who wears the figure. Set in the constructor of the subclass (InitFigure). */
	uint8 Fig = HumanFig::Dad;
	/** The whole figure's size on top of its model (the Dad 1). */
	float FigScale = 1.f;
	/** The family: what they are looking at (zero: nothing in particular). The head turns to it before the body does. */
	UPROPERTY(Replicated) FVector_NetQuantize FolkLook;
	/** The left hand is held up in front with something small in its fingers (the kid carrying an ant). */
	UPROPERTY(Replicated) bool bCarryPose = false;
	float GetStompRadius() const { return StompRadius * FigScale; }
	float FeetZ() const;
	/** Server: runs about with the hands in the air for that long (the kid, bitten). */
	void Panic(float Seconds);

	/** Server: something heavy landed on him. He reels for a moment (there is a cooldown) and gets angrier. False = no effect. */
	bool Stun(float Seconds, class AAntCharacter* By, const FString& What, uint8 Kind = 0);
	bool IsStunned() const { return bStunned; }
	UPROPERTY(ReplicatedUsing = OnRep_KnockedOut) bool bStunned = false;
	/** Where the sole of that foot is right now: a squashed ant rides on it. */
	FVector GetSoleLocation(int32 Leg) const;
	/** What squashed the ant just now: 0 / 1 a slipper, 2 the bare left palm, 3 the rolled newspaper. The pancake sticks to it (GetSoleLocation takes the same number). */
	uint8 GetLastStompLeg() const;

	// ---- The jointed body (HumanBody.h / HumanBody.cpp) ----
	/** What his body is doing, since when and at what. Replicated (only when it changes): late joiners see the right pose. */
	UPROPERTY(Replicated) FHumanActState Act;
	/** How he takes a stun: 0 it landed on his head (he reels, holding it), 1 on his foot (he hops, holding it). */
	UPROPERTY(Replicated) uint8 StunKind = 0;
	bool HasJointedBody() const { return bNewBody; }
	/** Seconds since the current action started (the server's clock on every machine). */
	float ActTime() const;
	/** Server: the left hand goes out to a handle or a switch at Point (the caller does the thing HumanAct::ReachTouch later). */
	void PlayReach(const FVector& Point);
	/** Server: crouch and peer under the furniture at Point for Seconds. */
	void PlayPeer(const FVector& Point, float Seconds);
	bool IsPeering() const;
	void StopPeer();
	/** One thing at a time: a stomp, a swat, a grab or a throw that has not landed yet keeps the body busy. */
	bool IsBodyBusy() const;
	/** Bent down to the floor with a hand out (slap, grab, reach): the AI stands still until he is up again. */
	bool IsRooted() const;
	/** The middle of a palm right now. 0 left, 1 right. */
	FVector GetPalmLocation(int32 Side) const;
	class UStaticMeshComponent* GetSegment(int32 Seg) const { return Segs.IsValidIndex(Seg) ? Segs[Seg] : nullptr; }
	/** Dev (-AntDo): hold the current action at this many seconds from its start (< 0: let go). */
	float DevFreezeAt = -1.f;
	/** Dev: a puppet for screenshots. The AI leaves him alone; DevWalk is where he walks to (zero: stands). */
	bool bDevPuppet = false;

	/** The cover rule (level v2). True where an ant is out of his reach whoever plays him: inside the fridge, the oven, under
	 *  the sink, in the corridor behind the cabinets, behind a shut door, and in every designed hiding place of the level
	 *  (LevelV2::CoverAt). His AI does not pick such an ant; his stomp, floor slap and newspaper do not hurt it. */
	static bool InCover(const UWorld* World, const FVector& P);
	bool bLetThrough = false;
	FVector DevWalk = FVector::ZeroVector;
	float DevWalkScale = 1.f;
	virtual void DevAct(const FString& What, const FVector& Target);
	FString DevDescribe() const;

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

	/** Server: shake off every ant/spider crawling on the body (AI does it on its own, a player presses F). */
	void TryShakeOff();
	UPROPERTY(Replicated) float ShakeReadyTime = 0.f;
	/** Ants (and the spider) crawling on the Human right now. */
	int32 CountClimbers() const;

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
	/** For the constructors of the family: the figure's own capsule, size and speed. */
	void InitFigure(uint8 InFig);
	/** Server: the family are hurt (an ant bit an ankle). They do not get knocked out by it: they react. */
	virtual void OnFolkHurt(float Damage, AController* By) {}
	void StartAct(uint8 Kind, uint8 Arg, const FVector& Target);
	/** One component per HumanSeg, made in the constructor: a base ants ride needs the same name on every machine. */
	UPROPERTY(VisibleAnywhere) TArray<UStaticMeshComponent*> Segs;
	bool bNewBody = false;
	FHumanRig Rig;
	UPROPERTY() USceneComponent* SwatterPivot = nullptr;
	UPROPERTY() USceneComponent* BroomPivot = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> NeedleArt;

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
	void OnShakePressed();
	UFUNCTION(Server, Reliable) void ServerShake();
	UFUNCTION(NetMulticast, Unreliable) void MulticastShake();
	bool AimPoint(FVector& OutPoint, FVector& OutDir) const;
	UFUNCTION(NetMulticast, Unreliable) void MulticastStomp(uint8 Leg);
	UFUNCTION() void OnRep_KnockedOut();
	UFUNCTION() void OnRep_Rage();

private:
	// ---- the jointed body ----
	void SetupJointedBody();
	void BuildHeldThings();
	/** Places every segment for this frame. Runs on every machine, the server too: ants ride the same pose. */
	void PoseBody(float DeltaSeconds);
	/** Cosmetic, local: the spray cloud and hiss, what is shown in the hands. */
	void BodyEffects(float DeltaSeconds);
	void OnFootfall(int32 Leg, float Strength);
	FTransform BodyFrame() const;
	/** The nozzle of the spray can when the arm is out, body space: where the cloud and the damage cone start. */
	FVector SprayNozzleBody() const;
	FVector SwatHitPoint = FVector::ZeroVector;
	FTimerHandle GrabTimer;
	FTimerHandle ThrowTimer;

	void BuildBody();
	void DoStompImpact();
	void DoSprayDamage(float Dt);
	void KnockOut(AController* By);
	void UpdateVisuals(float DeltaSeconds);
	/** Sets up the body model + its climbable collision (on every machine, the server too). */
	void SetupBody();
	/** Waddle / stomp lean / knockout sway / shake of the body. Runs everywhere so ants ride the same pose. */
	void AnimateBody(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;
	UPROPERTY(VisibleAnywhere) USceneComponent* VisualRoot;

	UPROPERTY() TArray<USceneComponent*> LegPivots;
	UPROPERTY() TArray<USceneComponent*> ArmPivots;

	// Generated art: the whole body is one static model (no skeleton yet), a giant slipper slams for stomps.
	// Both are created in the constructor: the body is a moving surface ants ride on, and a base
	// must have a stable network name so the ant's position can replicate relative to it.
	UPROPERTY(VisibleAnywhere) USceneComponent* BodyPivot = nullptr;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* BodyMesh = nullptr;
	USceneComponent* ModelPivot = nullptr;   // = BodyPivot once the model is loaded
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
	float StunEnd = 0.f;
	float NextStunTime = 0.f;
	float LastHurtTime = -100.f;
	float SprayAccum = 0.f;
	float SprayFxAccum = 0.f;   // cosmetic: the spray cloud
	float LocalStompStart = -100.f;
	uint8 LocalStompLeg = 0;
	float WalkPhase = 0.f;
	float PrevWalkPhase = 0.f;
	float ClimberTime = 0.f;          // server, AI: how long (weighted by count) ants have been on him
	float ShakeLocalStart = -100.f;
};
