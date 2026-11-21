#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AntAudio.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class USoundClass;
class USoundMix;
class UAudioComponent;
class AAntCharacter;

/**
 * Every sound of the game (docs/AUDIO.md). Ids 0..38 are the old AntSfx::EId with the same numbers, so AntSfx::Everyone /
 * AAntGameState::MulticastSfx carry the new ones too. The table in AntAudio.cpp gives each id its files (3-5 variations of the
 * frequent ones), volume, pitch spread, distance falloff (scaled for an 8 mm ant: 1 m = 1900 uu), mix class, voice limit and cooldown.
 */
enum class EAntSfx : uint8
{
	// ---- the old AntSfx ids (same order, same numbers)
	Spit, Splat, Hit, Crit, Kill, Grab, Drop, Deliver, Crunch, Stomp, Spray, Squish, Jump, Zap, Sizzle, Splash, Rage, Bite, Buzz, Web, Hiss, Step, Ambience,
	Swat, HumanStep, Faucet, FridgeDoor, Toaster, ArmyMarch, ArmyCharge, PickupGear, Legendary, SpiderStep, HumanPanic,
	StepWood, StepMetal, StepFabric, StepSoil, StepGrass,
	// ---- ants
	StepSticky, AcidHit, AntHurt, Release, CarryStrain, CarryThump, RipOff, DeliverCheer, DeliverChime, TrailLay, TrailCut, WorkerPatter,
	// ---- UI
	CardPick, CardFlip,
	// ---- the flat: fasteners, falls, breaks, food
	ScrewTurn, ScrewDrop, HingeCreak, Wobble, CrashWood, CrashGlass, CrashCeramic, CrashMetal, CrashPlastic, SoftThud,
	SplatJam, SplatCream, ColaFizz, Crumbs, CurtainRustle,
	// ---- the residents
	SlipperSlap, HumanGrumble, HumanRoar, HumanSlip, BatWhoosh, BatImpact, HumanPickup, HumanSniff,
	CatPurr, CatHiss, CatPounce, DogSnort, DogBark, DogSniff, DogEat, Broom, KidGiggle, KidShout, SpiderSkitter,
	// ---- ambience beds and point sources
	AmbFridge, AmbClock, AmbDrip, AmbDay, AmbEvening, AmbNight,
	Count
};

/** Mix groups (each a SoundClass under Master; docs/AUDIO.md "Mix"). */
namespace AntMixClass { enum : uint8 { Sfx, Voice, Ui, Ambience, Music, Num }; }

/** Music one-shots the director plays over the loop. */
enum class EAntSting : uint8 { RaidStart, Win, Fail, Card, LastLight, Count };

namespace AntAudio
{
	/** Local: plays at a world location. Intensity 0..2 scales volume (and a little pitch: harder = lower). */
	ANTKITCHEN_API void Play(const UObject* WorldContext, EAntSfx Id, const FVector& Location, float Intensity = 1.f);
	/** Local: no position (UI, stings). */
	ANTKITCHEN_API void Play2D(const UObject* WorldContext, EAntSfx Id, float Intensity = 1.f);
	/** Server: every machine plays it (AAntGameState multicast). */
	ANTKITCHEN_API void Everyone(const UObject* WorldContext, EAntSfx Id, const FVector& Location, float Intensity = 1.f);
	/** The old AntSfx entry points (volume and pitch as the callers give them). */
	ANTKITCHEN_API void PlayRaw(const UObject* WorldContext, uint8 Id, const FVector& Location, float Volume, float Pitch, bool b2D);
	/** A random variation of the id (for audio components: loops). */
	ANTKITCHEN_API USoundBase* Get(uint8 Id);
	/** Falloff of the id (loops started by other code use it). */
	ANTKITCHEN_API USoundAttenuation* AttenuationFor(uint8 Id);
	/** The crash of a thing made of LevelV3::EMat landing (Big: from high up / heavy). */
	ANTKITCHEN_API EAntSfx CrashFor(int32 Mat);
	/** The splat of a mess kind (AntMess MessKind order: Jam, Honey, Cola, Oil, Cream, Crumbs, Mud, Water, Chocolate, Scratch, Print,
	 *  Drag, Juice, Sugar). Count: that kind makes no sound (prints, scratches, drag marks). */
	ANTKITCHEN_API EAntSfx ForMess(uint8 MessKind);

	// ---- hooks for the systems in other branches (docs/AUDIO.md "Hooks"): all local and cheap, call them on every machine
	/** Team carrying: one beat of the «Раз-два, взяли!» rhythm (a thump, a creak of the load; Carriers scales it). */
	ANTKITCHEN_API void CarryBeat(const UObject* WorldContext, const FVector& At, int32 Carriers);
	/** Workers of the swarm near here (call ~2x a second per visible group): one cheap patter loop, its level from the count. */
	ANTKITCHEN_API void Swarm(const UObject* WorldContext, const FVector& Center, int32 Count);
	/** Something exciting happened near the listener (a crash, a chase, a weapon): pushes the music towards action for a while. */
	ANTKITCHEN_API void Excite(const UObject* WorldContext, float Amount, const FVector& Where);
	/** A voice line / shout starts: music and SFX dip under it for Seconds. */
	ANTKITCHEN_API void Duck(const UObject* WorldContext, float Seconds);
	/** A music sting (the director also fires its own from the game state). */
	ANTKITCHEN_API void Sting(const UObject* WorldContext, EAntSting Sting);

	/** Dev: log every played sound (ANTSFX lines). -AntSfxLog or ant.Audio.Log 1. */
	ANTKITCHEN_API bool LogOn();
	ANTKITCHEN_API const TCHAR* Name(uint8 Id);
}

/**
 * Per world, every machine but a dedicated server: the adaptive music director, the ambience, the state-driven sounds
 * (fasteners, falling and breaking fixtures, pets' and residents' idle voices, carrying), the mix (classes, ducking, volumes).
 * Reads only replicated state, so each machine hears its own mix of the same game.
 */
UCLASS()
class ANTKITCHEN_API UAntAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	static UAntAudioSubsystem* Get(const UObject* WorldContext);

	void AddExcite(float Amount) { Excitement = FMath::Min(1.f, Excitement + Amount); }
	void DuckFor(float Seconds);
	void PlaySting(EAntSting S);
	void SwarmAt(const FVector& Center, int32 Count);
	/** 0..1 the director's intensity right now, and the layer gains (for the dev overlay / logs). */
	float Intensity = 0.f;
	FString Describe() const;

private:
	// ---- music
	struct FMusicSet
	{
		FName Name;
		TArray<TObjectPtr<UAudioComponent>> Layers;   // Base, Tension, Action, Urgency (synchronized)
		float Gain = 0.f;                             // crossfade between sets
		float LoopSeconds = 0.f;
		double StartedAt = -1.0;
	};
	void StartMusic();
	void TickMusic(float Dt);
	float ComputeThreat(float& OutChase) const;
	void StartSet(int32 Index);
	void UpdateState();
	FMusicSet Sets[3];               // 0 Funk, 1 Lounge, 2 Nest
	int32 ActiveSet = -1;
	float LayerGain[4] = { 0.f, 0.f, 0.f, 0.f };
	float LayerTarget[4] = { 1.f, 0.f, 0.f, 0.f };
	float Excitement = 0.f;
	float DuckUntil = 0.f;
	float DuckLevel = 1.f;
	bool bMusicStarted = false;
	uint8 LastRaidPhase = 255;
	uint8 LastCardPicked = 255;
	uint8 LastRage = 0;
	bool bLastLightSaid = false;
	int32 LoopsInSet = 0;
	float SetTime = 0.f;
	float ThreatSmoothed = 0.f;

	// ---- ambience and the world around the listener
	void StartAmbience();
	void TickAmbience(float Dt);
	void TickWorldState(float Dt);
	void TickPets(float Dt);
	UAudioComponent* Loop(EAntSfx Id, const FVector* At, float Volume);
	UPROPERTY() TObjectPtr<UAudioComponent> RoomTone;
	int32 RoomToneTod = -1;
	UPROPERTY() TObjectPtr<UAudioComponent> FridgeHum;
	UPROPERTY() TObjectPtr<UAudioComponent> ClockTick;
	UPROPERTY() TObjectPtr<UAudioComponent> TapDrip;
	UPROPERTY() TObjectPtr<UAudioComponent> Patter;
	UPROPERTY() TObjectPtr<UAudioComponent> Purr;
	/** Keeps every component this subsystem made alive (the music layers live in plain structs). */
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Owned;
	float StrainClock = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> BackAche;
	TMap<TWeakObjectPtr<AActor>, bool> HeldSomething;
	TMap<TWeakObjectPtr<AActor>, uint8> PetState;
	TMap<TWeakObjectPtr<AActor>, bool> TrailOn;
	int32 PruneCount = 0;
	float PatterLevel = 0.f;
	FVector PatterAt = FVector::ZeroVector;
	float PatterSeen = -100.f;
	TMap<TWeakObjectPtr<AActor>, float> FastenerProgress;
	TMap<TWeakObjectPtr<AActor>, float> FastenerTick;
	TMap<TWeakObjectPtr<AActor>, float> FastenerOff;
	TMap<TWeakObjectPtr<AActor>, uint8> FixtureState;
	TMap<TWeakObjectPtr<AActor>, float> NextVoice;
	float WorldScanClock = 0.f;
	float PetClock = 0.f;
	float StateClock = 0.f;
	bool bAmbienceStarted = false;

	// ---- mix
	void SetupMix();
	void ApplyVolumes();
	float LastMaster = -1.f, LastMusic = -1.f, LastSfx = -1.f, LastAmb = -1.f, LastVoice = -1.f, LastUi = -1.f;
	bool bMuteKeyDown = false;
	float LogClock = 0.f;
};
