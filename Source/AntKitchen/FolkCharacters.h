#pragma once

#include "CoreMinimal.h"
#include "HumanCharacter.h"
#include "HumanNav.h"
#include "FolkCharacters.generated.h"

class AAntCharacter;
class AKitchenLoot;
class AKitchenProp;
class AKitchenInteractable;

/**
 * The rest of the family: grandma and the kid. The same jointed figure as the Dad (HumanBody.cpp) with their own
 * segment tables (Tools/blender/npc_folk.py -> /Game/Npc), size, posture and tempo, and their own heads (server).
 * Grandma owns the living-room floor in wide strokes: a slow flat swatter, a broom that shoves everything along, and
 * she puts food away behind doors. The kid owns the low places and is curious rather than murderous: he pokes, he
 * picks an ant up and carries it off, he stomps for fun, he drops crumbs, and he screams when bitten.
 * -AntOldNpc: the old seated primitives (AHouseholdNpc) instead.
 */
UCLASS()
class ANTKITCHEN_API AHouseFolk : public AHumanCharacter
{
	GENERATED_BODY()

public:
	AHouseFolk();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DevAct(const FString& What, const FVector& Target) override;
	/** Dev (-AntDo): "gran ..." / "kid ...". */
	void DevCommand(const TArray<FString>& A);
	/** Server: an ant held in the fingers struggles. */
	virtual void Struggle(AAntCharacter* Ant) {}
	FString FolkDescribe() const;

protected:
	virtual void Think(float Dt, float Now) {}
	virtual void Settle() {}
	/** Walks towards a point (through the doorway when it is in the other room). True when within Arrive. */
	bool WalkTowards(const FVector& Goal, float Scale, float Arrive);
	void TurnTo(const FVector& Point, float Dt, float Rate = 3.f);
	void SitDown(const FVector& Seat, uint8 How);
	void SetSitHow(uint8 How);
	void GetUp();
	bool IsSeated() const { return Act.Kind == HumanAct::Sit; }
	bool IsGettingUp() const;
	/** The nearest ant on the floor of this room it can see. */
	AAntCharacter* SeeAnt(float Range, float* OutDist = nullptr, float MaxUp = 260.f) const;
	FVector FloorPoint(const FVector& P) const;
	void SetMode(uint8 NewMode, float Seconds = 0.f);
	uint8 Mode = 0;
	float ModeSince = 0.f, ModeUntil = 0.f;
	bool bSettled = false;
	FHumanNavFollower NavFollow;                 // level v2: their way round the furniture
	FVector SeatPoint = FVector::ZeroVector;     // world
	FVector StandPoint = FVector::ZeroVector;    // where the feet are while seated
	float StandYaw = 0.f;
	TWeakObjectPtr<AAntCharacter> Quarry;
	float SeenSince = 0.f, LastSeen = -100.f;
	FVector GoalPoint = FVector::ZeroVector;
	float LastNoise = 0.f;
	float NextOuch = 0.f;
	float StuckFor = 0.f;
};

UCLASS()
class ANTKITCHEN_API AGrandmaCharacter : public AHouseFolk
{
	GENERATED_BODY()

public:
	AGrandmaCharacter();
	/** Server: one long stroke of the broom across the floor through Point. */
	void TrySweep(const FVector& Point);
	virtual void DevAct(const FString& What, const FVector& Target) override;

protected:
	virtual void Think(float Dt, float Now) override;
	virtual void Settle() override;
	virtual void OnFolkHurt(float Damage, AController* By) override;

private:
	enum EMode : uint8 { Seated, Rising, Hunting, Tidying, Stowing, Shutting, Returning };
	void DoSweep(const FVector& Point);
	/** Food lying about within Range that she would put away: what the ants dropped and the kid's crumbs. */
	AKitchenLoot* FindMess(float Range) const;
	int32 TidyLeft = 0;
	bool bHome = false;
	TWeakObjectPtr<UPrimitiveComponent> ChairPart;   // her armchair (level v2: see Settle)
	bool bChairThrough = false;
	float NextSweep = 0.f, NextKnit = 0.f, NextScold = 0.f, NextTidy = 0.f;
	bool bSwept = false;
	TWeakObjectPtr<AKitchenInteractable> Door;
	float ReachAt = 0.f;
	float TidyClock = -100.f;
};

UCLASS()
class ANTKITCHEN_API AKidCharacter : public AHouseFolk
{
	GENERATED_BODY()

public:
	AKidCharacter();
	virtual void Struggle(AAntCharacter* Ant) override;
	/** One of the crumbs a kid dropped (grandma tidies those up). */
	static bool IsCrumb(const UWorld* W, const AKitchenLoot* L);
	virtual void DevAct(const FString& What, const FVector& Target) override;

protected:
	virtual void Think(float Dt, float Now) override;
	virtual void Settle() override;
	virtual void OnFolkHurt(float Damage, AController* By) override;

private:
	enum EMode : uint8 { FloorPlay, Toddle, Chase, Pinching, Carry, Poking, Stomping, Scream, Cupping };
	void DropAnt(bool bCup);
	void DropCrumb();
	void WatchCups(float Now);
	TWeakObjectPtr<AAntCharacter> HeldAnt;
	int32 Grip = 0;
	int32 StompsLeft = 0;
	bool bActed = false;
	float NextCrumb = 20.f, NextUp = 12.f, NextGrab = 0.f, NextSwipe = 10.f;
	TWeakObjectPtr<AKitchenLoot> Thing;
	struct FCup { TWeakObjectPtr<AKitchenProp> Cup; TWeakObjectPtr<AAntCharacter> Ant; FVector At; float Until; };
	TArray<FCup> Cups;
	TArray<TWeakObjectPtr<AKitchenLoot>> Crumbs;
	/** Dev: what he does with the next ant he reaches (0 stomps, 1 pokes, 2 picks it up; -1 as he pleases). */
	int32 DevWant = -1;
};

/** The family's side of the shared hooks (NpcBody.h): the test driver and an ant wriggling in somebody's grip. */
namespace FolkWorld
{
	ANTKITCHEN_API bool DevCommand(class AAntKitchenGameMode* GM, const TArray<FString>& A);
	ANTKITCHEN_API bool Struggle(AAntCharacter* Ant);
	/** Server: an ant bit whoever holds it. True if somebody of the family did. */
	ANTKITCHEN_API bool BiteHolder(AAntCharacter* Ant);
}
