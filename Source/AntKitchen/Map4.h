#pragma once
// Map v4 (docs/level/V4_AUDIT.md, V4_ART.md, V4_COLLISION.md): the art audit, the district camera tour and the collision test.
//   -AntDo "art audit"                    lists every visible placeholder (engine basic shapes, flat default paint) -> Saved/map4/art_audit.csv
//   -AntDo "art shots <tag> [cams]"       the district cameras, one screenshot each -> Saved/map4/shots/<tag>/<cam>.png
//   -AntDo "colltest [walkers] [kinds]"   ants walk every surface (floors, walls, tops of furniture); stuck/stall/fall events -> Saved/map4/coll_<tag>.csv
//   -AntDo "humantest"                    the Human (puppet) walks his rounds; snags and walk-throughs -> log
#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Actor.h"
#include "Map4.generated.h"

class AAntKitchenGameMode;
class AAntCharacter;

/** A test walker's brain: owns a player state (the ant needs one) and does nothing; Map4 steers its ant. */
UCLASS()
class ANTKITCHEN_API AMap4Walker : public AAIController
{
	GENERATED_BODY()
public:
	AMap4Walker() { bWantsPlayerState = true; }
};

/** Runs the timed parts (camera tour, collision test) in the world. */
UCLASS()
class ANTKITCHEN_API AMap4Runner : public AActor
{
	GENERATED_BODY()
public:
	AMap4Runner();
	virtual void Tick(float Dt) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---- camera tour
	struct FCam { FString Name; FVector From; FVector To; float Fov; };
	TArray<FCam> Cams;
	FString ShotDir;
	int32 CamIndex = -1;
	float CamAt = 0.f;
	bool bShotTaken = false;
	TWeakObjectPtr<AActor> CamActor;
	bool bFps = false;           // "art fps": the same tour, 6 s per camera, frame times instead of pictures
	TArray<float> FrameMs, AllMs;
	float FpsFrom = 0.f;

	// ---- collision test
	struct FJob { FString Name; TArray<FVector> Pts; bool bDrop = true; };
	struct FWalker
	{
		TWeakObjectPtr<AAntCharacter> Ant;
		TWeakObjectPtr<AController> Ctrl;
		int32 Job = -1;
		int32 Pt = 0;
		float PtStart = 0.f, LastCheck = 0.f, BestDist = 1.e9f, FallingFor = 0.f;
		int32 StallChecks = 0;
		float JumpAt = -1.f;   // the harness let go of a surface (Jump) at this time
	};
	TArray<FJob> Jobs;
	TArray<FWalker> Walkers;
	int32 NextJob = 0;
	bool bColl = false;
	bool bTestsRan = false;      // a collision or Human test was started: -AntM4Quit ends the run once all are done
	float CollStart = 0.f;
	FString CollTag;
	TArray<FString> Events;      // csv rows
	TMap<FString, int32> Counts; // kind|district -> n

	// ---- Human route test
	bool bHuman = false;
	TArray<FVector> HPts;
	int32 HPt = 0;
	float HPtStart = 0.f, HBest = 1.e9f, HLastCheck = 0.f, HBlockedFor = 0.f;
	FVector HDetour = FVector::ZeroVector;
	TArray<bool> HMain;   // HPts[i] is one of the route's own points (not a via point the plan added)
	int32 HStalls = 0, HThrough = 0, HReached = 0;
	TSet<FString> HThroughWhat;

	void StartTour(const FString& Tag, const FString& Only);
	void StartColl(int32 NumWalkers, const FString& Kinds, const FString& Tag);
	void StartHuman();
	void Event(const FString& Kind, const FVector& At, const FString& Detail);
	void FinishColl();
	void BuildJobs(const FString& Kinds);
	void NextFor(FWalker& Wk);
};

namespace Map4
{
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** Counts visible placeholders; logs them and writes Saved/map4/art_audit.csv. Returns the number of engine basic shapes shown. */
	ANTKITCHEN_API int32 Audit(UWorld* W, bool bVerbose);
	/** District id (docs/level/level_v3.json) of a floor point, or "out". */
	ANTKITCHEN_API FString DistrictAt(const FVector& P);
	/** Marks a component as a debug-only helper: the art audit skips it. */
	ANTKITCHEN_API void MarkDebug(UActorComponent* C);
}
