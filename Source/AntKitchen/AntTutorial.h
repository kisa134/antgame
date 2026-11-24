#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Engine/NetSerialization.h"
#include "AntTutorial.generated.h"

class AHUD;
class UCanvas;
class APlayerController;
class APlayerState;
class AAntCharacter;
class AAntRaid;
class AAntKitchenGameMode;
class AKitchenLoot;

/**
 * «Учебка Сержанта Жвалова» (docs/ONBOARDING.md) and the Steam demo flavour (docs/DEMO.md).
 *
 * A training run is an ordinary raid world (map v4, the whole flat) in which a replicated director (AAntTutorial) owns the
 * raid clock: no red clan, no events, no timeout, the family asleep at night. The director walks the team through a table of
 * beats (teach -> test -> payoff), staging each one with the same text commands the admin panel and -AntDo use
 * (AAntKitchenGameMode::StageCommand), and reads completion off the state the game already keeps (movement, carrying, the
 * snowball, trails, the squad, fixtures, cloth, water, inside the residents). Every player has his own checklist; team steps
 * count for all. Beats of branches that are not merged yet (chains, life, colony, fighter) are switched by __has_include and
 * fall back to what this branch has.
 *
 * Process-wide: what the next map load should be (the menu sets it; -AntTutorial[=N], -AntSandbox, -AntDemo), the local
 * progress save (Saved/Tutorial or Saved/Demo), the telemetry log, the HUD and the drill-sergeant radio bark.
 */
namespace AntTut
{
	enum EMode : uint8 { Off, Training, Sandbox };
	/** What the next map load should be. */
	ANTKITCHEN_API uint8 Wanted();
	ANTKITCHEN_API void SetWanted(uint8 Mode, int32 StartBeat = 0);
	ANTKITCHEN_API int32 WantedBeat();
	/** A training run in this world (every machine: the director has replicated). */
	ANTKITCHEN_API bool Active(const UObject* Ctx);
	ANTKITCHEN_API bool SandboxActive(const UObject* Ctx);
	/** The Steam demo build: -AntDemo, or compiled with ANT_DEMO=1. */
	ANTKITCHEN_API bool IsDemo();
	/** Folder under Saved/ for the colony save of this kind of run (Nest, NestDemo, NestTutorial). */
	ANTKITCHEN_API const TCHAR* NestFolder();

	/** This machine's progress (Saved/Tutorial/progress.ini, Saved/Demo/progress.ini in the demo). */
	struct FProgress
	{
		bool bDone = false;          // the whole Учебка finished once
		uint32 BeatsDone = 0;        // a bit per beat finished (chapter select stamps)
		int32 Xp = 0;
		int32 Rank = 0;              // 0 Новобранец, 1 Рядовой
		TArray<FString> Medals;
		bool bGiftPending = false;   // the Queen's starter gift goes into the real colony at the next raid
		int32 DemoRaids = 0;         // demo: real raids played
		bool bDemoVersus = false;    // demo: the versus round played
		bool bDemoEnded = false;
		float BestSeconds = 0.f;
	};
	ANTKITCHEN_API FProgress& Progress();
	ANTKITCHEN_API void SaveProgress();
	/** The starter gift is waiting: take it (once). */
	ANTKITCHEN_API bool ConsumeGift();

	// ---- the beat table (data: AntTutorial.cpp; the lines to voice later: docs/ONBOARDING.md)
	enum EBranch : uint8 { Core, Chains, Life, Colony, Fighter };
	struct FStep { const TCHAR* Ru; const TCHAR* En; const TCHAR* Keys; int32 Need; bool bTeam; };
	struct FBeat
	{
		const TCHAR* Id;
		uint8 Chapter;               // 1 Подъём, 2 Полигон «Кухня», 3 Гигант, 4 Выпуск
		uint8 Branch;                // works fully only with that branch merged (fallback otherwise)
		const TCHAR* TitleRu; const TCHAR* TitleEn;
		float Par;                   // seconds a new player needs (tuning; the telemetry compares)
		int32 Xp;
		TArray<FStep> Steps;
	};
	ANTKITCHEN_API int32 NumBeats();
	ANTKITCHEN_API const FBeat& Beat(int32 i);
	ANTKITCHEN_API FString BeatTitle(int32 i);
	ANTKITCHEN_API FString ChapterName(int32 Chapter);
	ANTKITCHEN_API bool BranchMerged(uint8 Branch);
	/** Russian or English by the UI language setting. */
	ANTKITCHEN_API const TCHAR* Tx(const TCHAR* Ru, const TCHAR* En);
	ANTKITCHEN_API FString RankName(int32 Rank);

	// ---- hooks of the existing code (server)
	enum EEvent : uint8 { EvDash, EvSpit, EvBite, EvDeliver, EvPick, EvKill, EvCount };
	ANTKITCHEN_API void Note(const UObject* Ctx, uint8 Event, AActor* By, AActor* What = nullptr);
	/** AAntRaid::RaidSecond: true when the tutorial owns the raid clock (nothing else of the raid's second runs). */
	ANTKITCHEN_API bool OwnsRaid(const AAntRaid* Raid);
	/** AntSnowball: deliveries feed the mass only in the snowball's own beat (no card picks over the other lessons). */
	ANTKITCHEN_API bool MassHeld(const UObject* Ctx);
	/** AAntRaid::ServerInit of a real raid world: the Queen's starter gift after the training (once). */
	ANTKITCHEN_API void ApplyGift(AAntRaid* Raid);
	/** The Steam demo (every machine, every frame): counts the real raids, opens the end screen after the last one. */
	ANTKITCHEN_API void DemoHud(AHUD* Hud, UCanvas* Canvas, APlayerController* PC);
	/** Real raids the demo has (then «Спасибо, что играли»). */
	constexpr int32 DemoRaids = 3;

	// ---- every machine
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Canvas, APlayerController* PC);
	/** [Enter]: next line / take the offer; held 1.2 s by the host: skip the beat. Returns true if the tutorial used the key. */
	ANTKITCHEN_API bool OnEnter(APlayerController* PC, bool bPressed);
	/** Dev (-AntDo): tut info | tut skip | tut beat N | tut assist | tut end | tut shot <name> | tut auto 1|0 | sandbox ... */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
	/** The radio bark under a sergeant line (synthesised, Animal-Crossing-like gibberish; local). */
	ANTKITCHEN_API void Bark(const UObject* Ctx, int32 Seed, int32 Syllables, uint8 Voice);
}

/** One player's checklist of the current beat. */
USTRUCT()
struct FAntTutPlayer
{
	GENERATED_BODY()
	UPROPERTY() APlayerState* PS = nullptr;
	UPROPERTY() TArray<uint8> Count;   // per step of the beat
	UPROPERTY() int32 Xp = 0;
	// server
	float SprintFor = 0.f, CeilingFor = 0.f, SyncFor = 0.f, WaterFor = 0.f, AirFor = 0.f, Walked = 0.f;
	FVector LastPos = FVector::ZeroVector;
	bool bWasAir = false;
	float DryFor = 0.f;
	float AcidSeen = -1.f;
	bool bWasInNose = false;
	int32 DeathsSeen = 0;
	float DeadFor = 0.f;
	float Aux = 0.f;
	int32 AuxN = 0;
	int32 LastStepSaid = -1;
};

/** A marker in the world for everyone (objective, cover, where to stand). */
USTRUCT()
struct FAntTutMark
{
	GENERATED_BODY()
	UPROPERTY() FVector_NetQuantize At;
	UPROPERTY() FString Label;
	UPROPERTY() uint8 Kind = 0;   // 0 objective, 1 cover, 2 danger, 3 food
};

UCLASS()
class ANTKITCHEN_API AAntTutorial : public AInfo
{
	GENERATED_BODY()

public:
	AAntTutorial();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	static AAntTutorial* Get(const UWorld* World);

	enum EPhase : uint8 { Waiting, Intro, Play, Poligon, Payoff, Fade, Debrief, Done };

	// ---- replicated
	UPROPERTY(Replicated) uint8 Mode = AntTut::Training;
	UPROPERTY(Replicated) uint8 BeatNo = 0;
	UPROPERTY(Replicated) uint8 Phase = Waiting;
	UPROPERTY(Replicated) float PhaseStart = 0.f;
	UPROPERTY(Replicated) TArray<FAntTutPlayer> Players;
	UPROPERTY(Replicated) TArray<FAntTutMark> Marks;
	/** The line on screen (index into the line table, -1 none) and when it started (server time). */
	UPROPERTY(Replicated) int16 LineNo = -1;
	UPROPERTY(Replicated) float LineStart = 0.f;
	UPROPERTY(Replicated) int32 TeamXp = 0;
	/** A timer on screen (the finale's «последний свет»): server time it runs out, 0 none. */
	UPROPERTY(Replicated) float TimerEnds = 0.f;
	/** Help is offered ([Enter] — помочь) after the par time. */
	UPROPERTY(Replicated) bool bOffer = false;
	UPROPERTY(Replicated) uint8 Retries = 0;
	UPROPERTY(Replicated) float PoligonEnds = 0.f;
	/** Debrief numbers (filled at the end). */
	UPROPERTY(Replicated) int32 TotalSeconds = 0;
	UPROPERTY(Replicated) int32 BeatsDoneMask = 0;
	UPROPERTY(Replicated) uint8 Assists = 0;

	float ServerNow() const;
	const FAntTutPlayer* Find(const APlayerState* PS) const;
	/** Server. */
	void Command(const FString& Cmd);
	void SkipBeat(const TCHAR* Why);
	void Assist();
	void GoToBeat(int32 N);
	void Say(int32 Line);
	void Enter(APlayerController* PC);
	/** Server: a hook of the game (AntTut::Note). */
	void OnNote(uint8 Event, AActor* By, AActor* What);
	/** Server: a sandbox tool key (AAntPlayerController::ServerTut). */
	void SandboxTool(APlayerController* PC, int32 Tool);
	bool bAuto = false;          // dev: the scripted playthrough plays the beats itself
	int32 StartBeat = 0;

private:
	void ServerTick(float Dt);
	void Start();
	void Stage(int32 N);
	void Check(float Dt);
	void Complete();
	void Next();
	void Cleanup(int32 N);
	void AutoPlay(float Dt);
	/** Flies, red ants and stand-in bots of the raid: the training is the players' own. */
	void PurgeStrays();
	TWeakObjectPtr<AAntCharacter> Fly;
	float FlyHealth = 0.f;
	float AutoLogAt = 0.f;
	int32 ShotMask = 0;
	FVector ClothEdge = FVector::ZeroVector;
	void Telemetry(const TCHAR* What);
	void Finish();
	void PlayerStep(FAntTutPlayer& P, int32 Step, int32 Value, bool bSet = false);
	void TeamStep(int32 Step, int32 Value = 1, bool bSet = true);
	AAntCharacter* AntOf(const FAntTutPlayer& P) const;
	AKitchenLoot* SpawnFood(const FString& Name, const FVector& At, const FString& Rename = FString());
	FVector Checkpoint() const;
	void TeleportAll(const FVector& At, float Yaw);
	void SetGod(bool bOn);

	AAntKitchenGameMode* GM = nullptr;
	bool bStarted = false;
	bool bStaged = false;
	TArray<int32> LineQueue;
	float LineEnds = 0.f;
	float FadeAt = -1.f;
	float StartedAt = 0.f;
	float PayoffAt = 0.f;
	float NextHintAt = 0.f;
	int32 BaseFerried = 0;
	int32 Fired = 0;            // staged events of the beat already fired (bit mask)
	float BeatClock = 0.f;
	float AutoClock = 0.f;
	int32 AutoStep = 0;
	TArray<TWeakObjectPtr<AActor>> Spawned;
	TWeakObjectPtr<AKitchenLoot> Target;
	TArray<FString> TelemetryRows;
	FString Session;
	int32 BeatRetries = 0;
	int32 BeatAssists = 0;
	bool bSkipped = false;
};

