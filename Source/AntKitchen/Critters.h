#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntTypes.h"
#include "NpcBody.h"
#include "QuadBody.h"
#include "HumanNav.h"
#include "Critters.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class AAntCharacter;

/**
 * The household pets: Бадрик the French bulldog and the cat.
 * Jointed bodies (QuadBody.h): rigid segments ants climb and ride, posed on every machine from the replicated state
 * (location, rotation, State, Mood, Act, LookAt). The brains run on the server (CritterBrain.cpp).
 * The cat owns the heights: it watches from furniture, stalks low, wiggles (the warning), pounces, and plays with what it catches.
 * The dog owns the floor: he follows the SCENT ants leave, licks them up after a nose-twitch, eats food left lying, and barks.
 * -AntOldNpc: the old primitive-built pets with their old habits (the code below the line in Critters.cpp).
 */
UCLASS()
class ANTKITCHEN_API ACritter : public AActor
{
	GENERATED_BODY()

public:
	ACritter();
	/** A raid tool: this piece smells like food the ants have just dropped (the dog goes for it when he is near enough to smell it). */
	void BaitWith(class AKitchenLoot* Food);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0 dog, 1 cat. */
	UPROPERTY(Replicated) uint8 Kind = 0;
	/** The posture: QuadMode (0 sleep, 1 walk, 2 nose down, 3 sit, 4 in the air, 5 stalking, 6 frightened). */
	UPROPERTY(Replicated) uint8 State = 0;
	/** QuadMood: tail and ears. */
	UPROPERTY(Replicated) uint8 Mood = 0;
	/** What the body is doing (QuadAct), since when, at what. */
	UPROPERTY(Replicated) FNpcActState Act;
	/** What it is looking at (zero: nothing in particular). The head and the ears turn to it before the body does. */
	UPROPERTY(Replicated) FVector_NetQuantize LookAt;
	/** Level v3: its routine step (NpcRoutine.h), replicated. */
	UPROPERTY(Replicated) FNpcTask Task;
	FString DisplayName() const { return Kind == 0 ? TEXT("Бадрик") : TEXT("Кошка"); }

	float BodyLength() const { return Kind == 0 ? 950.f : 800.f; }
	float BodyHeight() const { return Kind == 0 ? 520.f : 440.f; }
	/** Server: an ant bit it. The dog yelps, shakes the riders off and after a few bites runs away to sulk;
	 *  the cat hisses and pounces straight back (or leaps away). */
	void Bitten(AAntCharacter* By);
	/** Server (the shift's event): the cat wakes up and stalks ants on the floor for that long. */
	void Hunt(float Seconds);

	// ---- the jointed body and its brain (CritterBrain.cpp) ----
	bool HasJointedBody() const { return bNewBody; }
	UStaticMeshComponent* GetSegment(int32 Seg) const { return Segs.IsValidIndex(Seg) ? Segs[Seg] : nullptr; }
	/** Server: an ant it holds struggles. Enough of that and it lets go. */
	void Struggle(AAntCharacter* Ant);
	/** Server: something frightening happened over there (a crash, the vacuum, water). The cat bolts for a high place, the dog barks at it. */
	void Scare(const FVector& From, const TCHAR* Why);
	/** Server: go and do the zoomies (the dog) for that long. */
	void Zoomies(float Seconds);
	/** Where the nose is right now (world). */
	FVector GetNose() const;
	/** Dev (-AntDo): a puppet for screenshots. The brain leaves it alone. */
	bool bDevPuppet = false;
	float DevFreezeAt = -1.f;
	FVector DevWalk = FVector::ZeroVector;
	float DevWalkSpeed = 500.f;
	void DevAct(const FString& What, const FVector& Target);
	void DevPlace(const FVector& P, float Yaw);
	void DevPrey(AAntCharacter* Ant);
	void DevPoke(const FString& What);
	FString DevDescribe() const;

private:
	struct FSpot { FVector P; uint8 Do; float Time; };   // Do: 0 sleep, 1 sniff/eat, 2 sit
	void BuildVisual();
	void ServerThink(float Dt);
	void ServerMove(float Dt);
	void ServerHurtAnts(float Dt);
	void PickSpot();
	FVector PawWorld(int32 i) const;
	void AnimateVisual(float Dt);

	// ---- the jointed body ----
	/** One component per QuadSeg, made in the constructor: a base ants ride needs the same name on every machine. */
	UPROPERTY(VisibleAnywhere) TArray<UStaticMeshComponent*> Segs;
	UPROPERTY(VisibleAnywhere) USceneComponent* BodyPivot = nullptr;
	bool bNewBody = false;
	FQuadRig Rig;
	FVector PoseVel = FVector::ZeroVector;
	FVector PoseLast = FVector::ZeroVector;
	FVector VisLoc = FVector::ZeroVector;     // clients: the body follows the replicated actor smoothly
	float VisYaw = 0.f;
	bool bVisInit = false;
	void SetupJointedBody();
	void PoseBody(float Dt);
	void OnFootfall(int32 Leg, float Strength, const FVector& At);
	void StartAct(uint8 Kind, uint8 Arg, const FVector& Target);
	float ActT() const;
	bool ActBusy() const;
	FVector PawPoint(int32 Paw) const;        // under the toes of a front paw, world

	// ---- the brain (server) ----
	enum class EBrain : uint8
	{
		Rest, Idle, Go, Notice, Stalk, Wiggle, Pounce, Pin, Watch, Swipe, Guard, Leap, Flee, Cower,
		Track, Tell, Lick, Eat, Bark, Ball, Beg, Zoom, Sulk
	};
	struct FRestSpot { FVector P; uint8 Do; float Time; bool bHigh; float Yaw = 1000.f; int32 Rt = -1; };   // Do: 0 sleep, 1 sniff / eat, 2 sit. Yaw: which way it settles (1000: any). Rt: level v3 routine step
	/** Level v3: the spot of its routine step now (NpcRoutine.h); false: none (or not reachable): the old random choice. */
	bool RoutineSpot();
	/** Level v3: its way round the furniture on the house grid (the routine sends it across both rooms). */
	FHumanNavFollower NavFollow;
	int32 RtStep = -1;
	uint32 RtNudge = 0;
	uint8 LoggedStep = 254;
	void BrainBegin();
	void BrainTick(float Dt);
	void CatThink(float Dt, float Now);
	void DogThink(float Dt, float Now);
	void SetBrain(EBrain B, float Seconds = 0.f);
	void ResolveSpots();
	void GoToSpot(int32 Index);
	void PickNextSpot();
	bool WalkTo(const FVector& Dest, float Pace, float Dt, float Arrive = 70.f);
	void Face(const FVector& Point, float Dt, float Rate = 6.f);
	void BeginLeap(const FVector& To, bool bPounce);
	bool TickAir(float Now);
	void Land(float Now);
	AAntCharacter* FindPrey(float Range, bool bAnyRoom, float* OutDist = nullptr) const;
	bool PreyValid(const AAntCharacter* A) const;
	bool CheckFrights(float Now);
	void ReleaseHeld(bool bFling);
	void DoSwipeHit();
	int32 CountRiders() const;
	void ThrowRiders(float Force);
	void BittenNew(AAntCharacter* By);
	TArray<FRestSpot> Rests;
	bool bSpotsReady = false;
	EBrain Brain = EBrain::Rest;
	float BrainSince = 0.f;
	float BrainUntil = 0.f;
	int32 RestIndex = 0;
	FVector WalkGoal = FVector::ZeroVector;
	FVector LeapAfter = FVector::ZeroVector;  // where to walk once it is down
	int32 LeapSpot = -1;                      // the rest spot the leap lands on
	bool bAirPounce = false;
	float AirFly = 0.4f;
	FVector AirPaws = FVector::ZeroVector;    // where the front paws come down
	TWeakObjectPtr<AAntCharacter> PreyAnt;
	TWeakObjectPtr<AAntCharacter> HeldAnt;
	FVector PreySeen = FVector::ZeroVector;
	float PreySeenTime = -100.f;
	float NoticeLevel = 0.f;
	float SenseClock = 0.f;
	float NextPounce = 0.f, NextSwipe = 0.f, NextWash = 0.f, NextNotice = 0.f, NextBark = 0.f, NextLick = 0.f, NextShake = 0.f;
	int32 Pins = 0;
	int32 Grip = 0;
	int32 Bites = 0;
	float LastBite = -100.f;
	float RiderTime = 0.f;
	float SteerSide = 0.f, SteerUntil = 0.f;
	FVector SlideVel = FVector::ZeroVector;   // on spilled oil: the paws go out sideways (KitchenSlick.h)
	float StuckTime = 0.f;
	float LastNoiseTime = 0.f;
	FVector ScareFrom = FVector::ZeroVector;
	FString ScareWhy;
	TWeakObjectPtr<AActor> Interest;          // the food he is going to eat, the ball he is after
	float ZoomUntil = 0.f;
	int32 BarksLeft = 0;
	bool bActHit = false;                     // the action's hit has landed
	float NextDogFright = 0.f;
	float NextFallCheck = 0.f;
	bool bSettling = false;
	// the dog
	void BeginBark(const FVector& At);
	AAntCharacter* FindLick(const FVector& Nose, float Range, bool bBlocked) const;
	void DoLick();
	class AKitchenLoot* FindLeftFood(const FVector& Nose, float Range, float Now) const;
	TMap<TWeakObjectPtr<class AKitchenLoot>, float> LeftFood;   // food seen in an ant's jaws -> when
	TWeakObjectPtr<class AKitchenProp> Ball;
	bool bBallMade = false;
	bool bEating = false;
	bool bShook = true;
	int32 BallNudges = 0;
	float FoodClock = 0.f, NoScentSince = 0.f, CantReachSince = 0.f, NextNudge = 0.f;
	void ArriveAtSpot(int32 Index);
	void BeginPin(AAntCharacter* Ant);
	void BeginWiggle(AAntCharacter* Prey);
	void BeginSwipe(AAntCharacter* Prey);

	// ---- the old primitive-built pets (-AntOldNpc) ----
	UPROPERTY() USceneComponent* Vis = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Legs;
	UPROPERTY() UStaticMeshComponent* Tail = nullptr;
	UPROPERTY() UStaticMeshComponent* Head = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Ears;
	UPROPERTY() TArray<UStaticMeshComponent*> Eyes;

	TArray<FSpot> Spots;
	int32 SpotIndex = -1;
	float StateTimer = 0.f;
	float ThinkTimer = 0.f;
	float HurtTimer = 0.f;
	float Speed = 0.f;
	FVector Goal = FVector::ZeroVector;
	FVector LastPos = FVector::ZeroVector;
	float Anim = 0.f;
	float Awake = 0.f;        // cat: seconds since it woke
	FVector PounceFrom, PounceTo;
	float PounceT = 0.f;
	float EatTimer = 0.f;
	int32 Patience = 3;
	float HuntUntil = 0.f;
	float NextHuntPounce = 0.f;
	float Fleeing = 0.f;
};

/**
 * People of the house besides the Human you fight: a kid on the sofa watching TV (drops crumbs,
 * swings his legs) and grandma asleep in the armchair (shifts her feet; a ball of yarn to climb).
 * Static primitives with a couple of timed habits; replicated so everyone sees the same pose.
 */
UCLASS()
class ANTKITCHEN_API AHouseholdNpc : public AActor
{
	GENERATED_BODY()

public:
	AHouseholdNpc();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 0 kid on the sofa, 1 grandma in the armchair. */
	UPROPERTY(Replicated) uint8 Kind = 0;
	/** Kid: legs swinging (dangerous in front of the sofa). Grandma: which foot spot (0/1). */
	UPROPERTY(Replicated) uint8 Pose = 0;
	/** Set 1 s before the pose changes: the warning. */
	UPROPERTY(Replicated) bool bWarning = false;

private:
	void BuildVisual();
	UPROPERTY() USceneComponent* Vis = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Shins;
	UPROPERTY() TArray<UStaticMeshComponent*> Feet;
	UPROPERTY() UStaticMeshComponent* Chest = nullptr;
	float Timer = 0.f;
	float Habit = 20.f;
	float Anim = 0.f;
	float Swing = 0.f;
	float HurtTimer = 0.f;
};
