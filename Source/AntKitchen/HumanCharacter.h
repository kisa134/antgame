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
	float GetWalkSpeed() const { return (bStunned || IsDown()) ? 1.f : bKnockedOut ? (Fig == HumanFig::Kid ? 1000.f : 260.f) : HumanBody::Style(Fig).WalkSpeed * (1.f + 0.12f * Rage) * (HasBackAche() ? 0.5f : 1.f) * (1.f + (SprintMax - 1.f) * SprintRamp); }

	// ---- The heavy body (HumanPhys.cpp): mass and inertia, momentum that shoves the flat about, the semi-ragdoll, the player's keys ----
	/** kg: what his momentum is made of (x BodyMassK, the admin's "масса тела"). */
	static constexpr float BodyKg = 90.f;
	/** Running (Shift): the top speed is this many times the walk, reached after a wind-up. */
	static constexpr float SprintMax = 1.9f;
	UPROPERTY(Replicated) bool bSprinting = false;
	/** 0..1: how far into the sprint's wind-up he is (local on every machine, eased from bSprinting and the speed). */
	float SprintRamp = 0.f;
	/** LMB held: the swing is being wound up (a charged blow on release). */
	UPROPERTY(Replicated) bool bCharging = false;
	/** Server world time until which he reels back from something he ran into (no walking). */
	UPROPERTY(Replicated) float StaggerUntil = -100.f;
	/** Admin: x the weight of his body (inertia, what he shoves) and x the force of his blows. */
	UPROPERTY(Replicated) float BodyMassK = 1.f;
	UPROPERTY(Replicated) float StrengthK = 1.f;
	UPROPERTY(Replicated) float KickReadyTime = 0.f;
	/** The swing in flight (server): x its impulse and damage (charged, the combo's last blow) and which way it sweeps (zero: down onto the point). */
	float SwingPower = 1.f;
	FVector SwingDir = FVector::ZeroVector;
	bool IsStaggered() const { return NetNow() < StaggerUntil; }
	/** Server: a blow with LMB (player): Charge 0..1 (how long it was held). A rhythm of clicks makes a combo of three. */
	void TrySwing(const FVector& Point, float Charge);
	/** Server: the stomp at a point near his feet (the player aims it: clamped to where a foot gets). */
	void TryStompAt(const FVector& Point);
	/** Server: both feet: a heavy jump and a landing with a shockwave (hold Space). */
	void TryJumpStomp();
	/** Server: a punt with the right foot along where he faces (Ctrl / C). */
	void TryKick();
	/** Server: throws what is in his hands (food, or the weapon: it flies and smashes what it hits). */
	void ThrowHeld(const FVector& Dir);
	/** Server: he goes down (ran into something at a run): the slip's fall, without a puddle. Dir: where he was going. */
	bool Topple(const FVector& Dir, const TCHAR* Why);
	/** Server: a hit on the body (every machine sees it): Dir world, Strength 0..1+, Kind 0 a bump, 1 a recoil, 2 a stagger, 3 a hand jerked (Side in Kind 3/4). */
	void Jolt(const FVector& Dir, float Strength, uint8 Kind);
	/** Where the kick lands (in front of him, shin high). */
	FVector KickPoint() const;
	/** The aimed stomp: Point clamped to where a foot gets (and which foot). The HUD marks it. */
	FVector StompAimFrom(const FVector& Point, int32& OutLeg) const;
	/** Dev (-AntDo): the player's keys and movement for a scripted Human (hin ...). */
	static bool HeavyDevCommand(class AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** Q on the keyboard: the player controller has Q for the squad; it hands it over when the pawn is the Human. */
	void OnThrowPressed();
	/** Dev: scripted input (forward / right -1..1) as if the player held the keys. */
	FVector2D DevInput = FVector2D::ZeroVector;
	float DevInputUntil = -1.f;

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

	// ---- Slipping on a puddle (KitchenSlick.h) ----
	/** Server: his feet go out from under him at At (a puddle). Chance 0..1; bForce ignores the chance and the 45 s rest. True if he falls. */
	bool TrySlip(const FVector& At, float Chance, bool bForce = false, AActor* By = nullptr);
	/** Down on the floor after a slip (from the slip until he stands again): no walking, no attacks. */
	bool IsDown() const;
	/** After a fall: one hand on his back, slow (every machine, from the replicated time). */
	bool HasBackAche() const;
	/** Server: down on his haunches wiping the floor at Point with the towel. */
	void PlayWipe(const FVector& Point, float Seconds);
	bool IsWiping() const;
	/** Server world time until which his back hurts. */
	UPROPERTY(Replicated) float BackAcheUntil = -100.f;
	/** A towel in the left hand (he is about to wipe a puddle). */
	UPROPERTY(Replicated) bool bTowel = false;
	/** Level v3: the routine step and the reaction to a mess (NpcRoutine.h), replicated. */
	UPROPERTY(Replicated) FNpcTask Task;
	/** Server: stands at a chore with the hands busy (HumanBusy) facing Target. */
	void PlayBusy(uint8 Busy, const FVector& Target);
	/** Server: whatever the body is doing for the routine stops (up from a seat, hands down). */
	void EndBusy();
	bool IsBusyPose() const { return Act.Kind == HumanAct::Busy; }
	/** Server: sit down onto Seat (world, the seat's top) facing Yaw: the feet go in front of it (teleported: the capsule may stand in the furniture while seated). */
	void SitAt(const FVector& Seat, float Yaw, uint8 How);
	/** Server, level v3: in a sticky pool (cola, jam): slow and loud steps. */
	float StickyK = 1.f;
	/** Server: out of the flat (the rubbish), hidden with no collision; false puts him back at At. */
	void SetAway(bool bAway, const FVector& At);
	bool IsAway() const { return bAway; }
	/** Where he stood before he sat down (the AI steps back there once he is up). */
	FVector SitFrom = FVector::ZeroVector;
	/** Local: the hall door's leaf, eased (the Dad only); the last routine state logged on this machine. */
	float HallDoorK = 0.f;
	uint8 LoggedStep = 254, LoggedPhase = 254, LoggedReact = 254;
	/** Server: the puddle he slipped in (the AI goes and wipes it). */
	TWeakObjectPtr<class AKitchenSlick> SlippedOn;
	/** Server: not before this world time again (45 s after he is up). */
	float NextFallTime = 0.f;
	/** The world time on this machine: the server's clock on clients. */
	float NetNow() const;
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime = 0.f) override;
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
	/** Server: rolled newspaper slap at a point (walls, tables, anywhere in reach). Force: -1 by height, 0 the bare palm, 1 the newspaper. */
	void TrySwat(const FVector& Point, int32 Force = -1);
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
	/** Level v3, the routine's things in the hands (local): 0 the rubbish bag, 1 a ladle, 2 a phone, 3 a watering can, 4 a dustpan, 5 a plate, 6 a crayon. */
	UPROPERTY() TArray<UStaticMeshComponent*> PropArt;
	void BuildRoutineProps();
	void ShowRoutineProps();
	bool bAway = false;
	FVector AwayFrom = FVector::ZeroVector;

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

	// ---- the player's keys (HumanPhys.cpp)
	void OnFirePressed();
	void OnFireReleased();
	void OnJumpPressed();
	void OnJumpReleased();
	void OnKickPressed();
	void OnSprintPressed();
	void OnSprintReleased();
	float FireDownAt = -1.f;
	float JumpDownAt = -1.f;
	bool bJumpFired = false;
	UFUNCTION(Server, Reliable) void ServerSwing(FVector_NetQuantize Point, float Charge);
	UFUNCTION(Server, Reliable) void ServerCharge(bool bOn);
	UFUNCTION(Server, Reliable) void ServerStompAt(FVector_NetQuantize Point);
	UFUNCTION(Server, Reliable) void ServerJumpStomp();
	UFUNCTION(Server, Reliable) void ServerKick();
	UFUNCTION(Server, Reliable) void ServerThrow(FVector_NetQuantizeNormal Dir);
	UFUNCTION(Server, Reliable) void ServerSprint(bool bOn);
	/** The owner's machine moves a player Human (client-authoritative movement): the reel back is applied there. */
	UFUNCTION(Client, Reliable) void ClientStagger(FVector_NetQuantize10 Vel, float Seconds);
	UFUNCTION(NetMulticast, Unreliable) void MulticastJolt(FVector_NetQuantizeNormal Dir, float Strength, uint8 Kind);

	// ---- the heavy body, every tick (HumanPhys.cpp)
	void TickHeavyBody(float Dt);
	/** Server: what his body runs into (furniture, chairs, food, fixtures): shoved by his momentum, or he reels back. */
	void BodyContacts(float Dt);
	/** Server: the thrown things in flight (food, the weapon): what they hit. */
	void TickThrown(float Dt);
	void DoKickImpact();
	void DoJumpImpact();
	/** Server: things on the floor/tables round P jump; light ones hop, pieces are shoved out radially (stomps). */
	void Shockwave(const FVector& P, float Radius, float Power, const TCHAR* Why);
	/** Local: the camera of the one who plays him (sway, bob, the run's FOV). */
	void TickCamera(float Dt);
	/** Local, dev: -AntHumanDo="t:op;..." presses the local player's Human keys (any machine; HumanPhys.cpp). */
	void TickKeyScript();
	float KeyFireUpAt = -1.f;
	float KeyJumpUpAt = -1.f;
	/** Local, PoseBody: the semi-ragdoll springs. */
	void TickPhysLayer(float Dt, const FTransform& Body);
	/** The stomp's landing point (server): the aimed one, or GetStompPoint. */
	FVector StompAim = FVector::ZeroVector;
	FVector PendingAim = FVector::ZeroVector;
	bool bPendingAim = false;
	/** The next TrySwat's Act.Arg: bits 0-1 the combo's blow, bit 2 charged (see SwingWindK). */
	uint8 SwingArg = 0;
public:
	/** The wind-up of a swing with Act.Arg A, times the weapon's own: charged longer, the combo's follow-ups quicker. */
	static float SwingWindK(uint8 A) { return (A & 4) ? 1.35f : (A & 3) ? 0.8f : 1.f; }
protected:
	FVector PrevBodyVel = FVector::ZeroVector;
	float PrevBodySpeed = 0.f;
	TMap<TWeakObjectPtr<UObject>, float> ContactCool;
	float NextContactLog = 0.f;
	struct FThrownThing { TWeakObjectPtr<AActor> What; float Until = 0.f; FVector LastVel = FVector::ZeroVector; float Kg = 1.f; };
	TArray<FThrownThing> ThrownThings;
	uint8 Combo = 0;
	float ComboUntil = 0.f;
	float CamBob = 0.f;
	float BaseFov = 90.f;
	FVector BaseSocket = FVector::ZeroVector;
	bool bCamInit = false;
	float LastSprintSent = -1.f;
	float NextHandJolt = 0.f;
	float NextStaggerAt = 0.f;
	float BlockArmedUntil = 0.f;
	float BlockSpeed = 0.f;
	FVector BlockDir = FVector::ZeroVector;
	FVector BlockPoint = FVector::ZeroVector;
	int32 BumpCount = 0;
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
	/** Server: a foot came down (Leg) - in a puddle? */
	void CheckSlip(int32 Leg, bool bAttack);
	/** Server: the back hits the floor: ants under him squashed, the rest thrown, things on the low surfaces nearby jump. */
	void DoFallImpact();
	float FallImpactAt = -1.f;
	float LocalFallFx = -100.f;   // cosmetic: the Act.Start whose landing has been shown here
	UPROPERTY() UStaticMeshComponent* TowelArt = nullptr;
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
