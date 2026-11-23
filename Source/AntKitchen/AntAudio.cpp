#include "AntAudio.h"
#include "AntTypes.h"
#include "AntGameState.h"
#include "AntRaid.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "FolkCharacters.h"
#include "Critters.h"
#include "KitchenFixture.h"
#include "KitchenInteractables.h"
#include "KitchenLoot.h"
#include "LevelV2.h"
#include "LevelV3.h"
#include "AntWreck.h"
#include "AntWeapon.h"
#include "AntCarry.h"
#include "AntTrail.h"
#include "AntSwarm.h"
#include "AntSnowball.h"
#include "AntVersus.h"
#include "HumanBody.h"
#include "AntPlayerController.h"
#include "AntFilm.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "InputCoreTypes.h"
#include "AntReact.h"

// ============================================================================ settings (console variables, saved in GameUserSettings.ini [AntAudio])
static TAutoConsoleVariable<float> CVarMaster(TEXT("ant.Audio.Master"), 1.f, TEXT("Master volume 0..1"));
static TAutoConsoleVariable<float> CVarMusic(TEXT("ant.Audio.Music"), 0.8f, TEXT("Music volume 0..1 (M mutes / unmutes)"));
static TAutoConsoleVariable<float> CVarSfx(TEXT("ant.Audio.Sfx"), 1.f, TEXT("Sound effects volume 0..1"));
static TAutoConsoleVariable<float> CVarVoice(TEXT("ant.Audio.Voice"), 1.f, TEXT("Voices (residents, shouts) volume 0..1"));
static TAutoConsoleVariable<float> CVarUi(TEXT("ant.Audio.Ui"), 0.9f, TEXT("Interface sounds volume 0..1"));
static TAutoConsoleVariable<float> CVarAmb(TEXT("ant.Audio.Ambience"), 0.9f, TEXT("Room tone and household sounds volume 0..1"));
static TAutoConsoleVariable<int32> CVarMute(TEXT("ant.Audio.MusicMute"), 0, TEXT("1: music off (the M key toggles it)"));
static TAutoConsoleVariable<int32> CVarLog(TEXT("ant.Audio.Log"), 0, TEXT("1: log every sound played (ANTSFX) and the music state (ANTMUSIC)"));
static TAutoConsoleVariable<float> CVarForce(TEXT("ant.Audio.ForceIntensity"), -1.f, TEXT("Dev: >= 0 pins the music intensity (0 calm .. 1 chaos)"));
static TAutoConsoleVariable<int32> CVarForceUrgent(TEXT("ant.Audio.ForceUrgent"), 0, TEXT("Dev: 1 keeps the last-light layer on"));

namespace
{
	enum EAtt : uint8 { Att2D, AttTiny, AttSmall, AttMedium, AttLarge, AttHuge, AttNum };
	enum EKls : uint8 { KSfx = AntMixClass::Sfx, KVoice = AntMixClass::Voice, KUi = AntMixClass::Ui, KAmb = AntMixClass::Ambience };
	constexpr uint8 NoAlso = (uint8)EAntSfx::Count;

	struct FSfxDef
	{
		EAntSfx Id;
		const TCHAR* Files;    // "A_Name*N" = /Game/Audio/Sfx/A_Name_1..N, "S_Name" = /Game/SoundsReal/S_Name, "S_Name_*N" = S_Name_1..N
		float Vol;
		float Jitter;          // random pitch +-
		uint8 Att;
		uint8 Kls;
		uint8 MaxCount;        // voices of this id at once (0: no limit)
		float Cooldown;        // seconds between two of this id on this machine
		EAntSfx Also;          // played together with it (a layer)
		float AlsoVol;
		float Excite;          // pushes the music towards action when it happens near the listener
	};

#define D(Id, Files, Vol, Jit, Att, Kls, Max, Cool, Also, AlsoVol, Exc) { EAntSfx::Id, TEXT(Files), Vol, Jit, Att, Kls, Max, Cool, EAntSfx::Also, AlsoVol, Exc }
	static const FSfxDef Table[] =
	{
		D(Spit,        "A_SpitN*4 S_Spit",                0.8f, 0.08f, AttSmall,  KSfx, 6, 0.03f, Count, 0.f, 0.f),
		D(Splat,       "S_Splat",                         0.8f, 0.08f, AttSmall,  KSfx, 6, 0.03f, AcidHit, 0.5f, 0.f),
		D(Hit,         "S_Hit",                           0.8f, 0.07f, AttSmall,  KSfx, 6, 0.02f, Count, 0.f, 0.f),
		D(Crit,        "S_Crit",                          0.9f, 0.05f, AttSmall,  KSfx, 4, 0.03f, Count, 0.f, 0.f),
		D(Kill,        "S_Kill",                          0.8f, 0.03f, Att2D,     KUi,  2, 0.05f, Count, 0.f, 0.f),
		D(Grab,        "A_GrabN*3 S_Grab",                0.8f, 0.08f, AttSmall,  KSfx, 4, 0.05f, Count, 0.f, 0.f),
		D(Drop,        "A_Release*3 S_Drop",              0.8f, 0.08f, AttSmall,  KSfx, 4, 0.05f, Count, 0.f, 0.f),
		D(Deliver,     "S_Deliver",                       0.9f, 0.03f, AttMedium, KSfx, 3, 0.10f, DeliverCheer, 0.7f, 0.f),
		D(Crunch,      "S_Crunch",                        0.9f, 0.08f, AttMedium, KSfx, 4, 0.05f, Crumbs, 0.5f, 0.1f),
		D(Stomp,       "S_Stomp",                         1.0f, 0.05f, AttLarge,  KSfx, 3, 0.10f, SlipperSlap, 0.8f, 0.35f),
		D(Spray,       "S_Spray",                         0.8f, 0.00f, AttMedium, KSfx, 2, 0.00f, Count, 0.f, 0.f),
		D(Squish,      "S_Squish",                        0.9f, 0.08f, AttSmall,  KSfx, 4, 0.05f, AntHurt, 0.6f, 0.1f),
		D(Jump,        "S_Jump",                          0.5f, 0.10f, AttTiny,   KSfx, 3, 0.05f, Count, 0.f, 0.f),
		D(Zap,         "S_Zap",                           0.9f, 0.05f, AttMedium, KSfx, 3, 0.05f, Count, 0.f, 0.2f),
		D(Sizzle,      "S_Sizzle A_AcidHit*3",            0.8f, 0.06f, AttSmall,  KSfx, 4, 0.05f, Count, 0.f, 0.f),
		D(Splash,      "S_Splash",                        0.9f, 0.06f, AttMedium, KSfx, 4, 0.05f, Count, 0.f, 0.1f),
		D(Rage,        "S_Rage",                          1.0f, 0.03f, AttHuge,   KVoice, 2, 0.50f, HumanRoar, 0.9f, 0.6f),
		D(Bite,        "A_BiteN*5 S_Bite",                0.9f, 0.08f, AttSmall,  KSfx, 6, 0.03f, Count, 0.f, 0.f),
		D(Buzz,        "S_Buzz",                          0.7f, 0.00f, AttMedium, KSfx, 4, 0.00f, Count, 0.f, 0.f),
		D(Web,         "S_Web",                           0.8f, 0.06f, AttSmall,  KSfx, 3, 0.05f, Count, 0.f, 0.f),
		D(Hiss,        "S_Hiss",                          0.9f, 0.05f, AttMedium, KSfx, 3, 0.10f, Count, 0.f, 0.15f),
		D(Step,        "A_StepTile*5",                    0.9f, 0.10f, AttTiny,   KSfx, 10, 0.f,  Count, 0.f, 0.f),
		D(Ambience,    "S_Ambience",                      0.5f, 0.00f, Att2D,     KAmb, 1, 0.f,   Count, 0.f, 0.f),
		D(Swat,        "S_Swat A_SwatterN*1",             1.0f, 0.06f, AttLarge,  KSfx, 3, 0.08f, Count, 0.f, 0.3f),
		D(HumanStep,   "A_HumanStepN*4",                  1.0f, 0.06f, AttHuge,   KSfx, 4, 0.08f, Count, 0.f, 0.05f),
		D(Faucet,      "S_Faucet",                        0.8f, 0.00f, AttMedium, KAmb, 2, 0.f,   Count, 0.f, 0.f),
		D(FridgeDoor,  "S_FridgeDoor",                    0.9f, 0.05f, AttLarge,  KSfx, 3, 0.10f, Count, 0.f, 0.f),
		D(Toaster,     "S_Toaster",                       0.9f, 0.03f, AttLarge,  KSfx, 2, 0.10f, Count, 0.f, 0.1f),
		D(ArmyMarch,   "S_ArmyMarch",                     0.8f, 0.00f, AttMedium, KSfx, 2, 0.f,   Count, 0.f, 0.f),
		D(ArmyCharge,  "S_ArmyCharge",                    0.9f, 0.05f, AttLarge,  KVoice, 3, 0.20f, Count, 0.f, 0.3f),
		D(PickupGear,  "S_Pickup_Gear",                   0.8f, 0.04f, AttSmall,  KUi,  3, 0.05f, Count, 0.f, 0.f),
		D(Legendary,   "S_Legendary",                     0.9f, 0.02f, AttMedium, KUi,  2, 0.20f, Count, 0.f, 0.f),
		D(SpiderStep,  "S_SpiderStep",                    0.6f, 0.12f, AttTiny,   KSfx, 6, 0.f,   SpiderSkitter, 0.35f, 0.f),
		D(HumanPanic,  "S_HumanPanic",                    1.0f, 0.04f, AttHuge,   KVoice, 2, 0.40f, Count, 0.f, 0.4f),
		D(StepWood,    "A_StepWoodN*5",                   0.9f, 0.10f, AttTiny,   KSfx, 10, 0.f,  Count, 0.f, 0.f),
		D(StepMetal,   "A_StepMetalN*5",                  0.8f, 0.10f, AttTiny,   KSfx, 10, 0.f,  Count, 0.f, 0.f),
		D(StepFabric,  "A_StepRug*5",                     1.0f, 0.10f, AttTiny,   KSfx, 10, 0.f,  Count, 0.f, 0.f),
		D(StepSoil,    "A_StepRug*5",                     1.0f, 0.10f, AttTiny,   KSfx, 10, 0.f,  Count, 0.f, 0.f),
		D(StepGrass,   "A_StepRug*5",                     1.0f, 0.12f, AttTiny,   KSfx, 10, 0.f,  Count, 0.f, 0.f),
		// ---- ants
		D(StepSticky,  "A_StepSticky*5",                  0.9f, 0.10f, AttTiny,   KSfx, 8, 0.f,   Count, 0.f, 0.f),
		D(AcidHit,     "A_AcidHit*3",                     0.7f, 0.08f, AttSmall,  KSfx, 3, 0.30f, Count, 0.f, 0.f),
		D(AntHurt,     "A_AntHurt*3",                     0.7f, 0.10f, AttSmall,  KVoice, 3, 0.10f, Count, 0.f, 0.f),
		D(Release,     "A_Release*3",                     0.8f, 0.08f, AttSmall,  KSfx, 4, 0.05f, Count, 0.f, 0.f),
		D(CarryStrain, "A_CarryStrain*3",                 0.7f, 0.06f, AttSmall,  KSfx, 2, 1.20f, Count, 0.f, 0.f),
		D(CarryThump,  "A_CarryThump*4",                  0.9f, 0.05f, AttMedium, KSfx, 3, 0.15f, Count, 0.f, 0.f),
		D(RipOff,      "A_RipOff*1",                      0.9f, 0.08f, AttSmall,  KSfx, 3, 0.20f, Count, 0.f, 0.05f),
		D(DeliverCheer,"A_DeliverCheer*1",                0.8f, 0.05f, AttMedium, KVoice, 2, 0.60f, DeliverChime, 0.8f, 0.f),
		D(DeliverChime,"A_DeliverChime*1",                0.7f, 0.02f, AttMedium, KUi,  2, 0.20f, Count, 0.f, 0.f),
		D(TrailLay,    "A_TrailLay*3",                    0.6f, 0.08f, AttSmall,  KSfx, 3, 1.00f, Count, 0.f, 0.f),
		D(TrailCut,    "A_TrailCut*1",                    0.8f, 0.05f, AttSmall,  KSfx, 3, 0.10f, AntHurt, 0.5f, 0.1f),
		D(WorkerPatter,"A_WorkerPatter*1",                0.8f, 0.00f, AttSmall,  KSfx, 1, 0.f,   Count, 0.f, 0.f),
		// ---- UI
		D(CardPick,    "A_CardPick*1",                    0.8f, 0.00f, Att2D,     KUi,  2, 0.05f, Count, 0.f, 0.f),
		D(CardFlip,    "A_CardFlip*2",                    0.7f, 0.05f, Att2D,     KUi,  3, 0.05f, Count, 0.f, 0.f),
		// ---- the flat
		D(ScrewTurn,   "A_ScrewTurn*3",                   0.7f, 0.08f, AttSmall,  KSfx, 3, 0.25f, Count, 0.f, 0.f),
		D(ScrewDrop,   "A_ScrewDrop*3",                   0.9f, 0.08f, AttMedium, KSfx, 3, 0.10f, Count, 0.f, 0.05f),
		D(HingeCreak,  "A_HingeCreak*3",                  0.8f, 0.10f, AttMedium, KSfx, 3, 0.60f, Count, 0.f, 0.f),
		D(Wobble,      "A_Wobble*1",                      0.9f, 0.06f, AttMedium, KSfx, 2, 0.50f, Count, 0.f, 0.1f),
		D(CrashWood,   "A_CrashWood*1",                   1.0f, 0.08f, AttHuge,   KSfx, 3, 0.15f, Count, 0.f, 0.6f),
		D(CrashGlass,  "A_CrashGlass*1",                  1.0f, 0.08f, AttHuge,   KSfx, 3, 0.15f, Count, 0.f, 0.6f),
		D(CrashCeramic,"A_CrashCeramic*1",                1.0f, 0.08f, AttHuge,   KSfx, 3, 0.15f, Count, 0.f, 0.6f),
		D(CrashMetal,  "A_CrashMetal*1",                  1.0f, 0.08f, AttHuge,   KSfx, 3, 0.15f, Count, 0.f, 0.5f),
		D(CrashPlastic,"A_CrashPlastic*1",                1.0f, 0.08f, AttLarge,  KSfx, 3, 0.15f, Count, 0.f, 0.3f),
		D(SoftThud,    "A_CarryThump*4",                  0.9f, 0.08f, AttLarge,  KSfx, 3, 0.10f, Count, 0.f, 0.15f),
		D(SplatJam,    "A_SplatJam*3",                    0.9f, 0.08f, AttMedium, KSfx, 4, 0.05f, Count, 0.f, 0.1f),
		D(SplatCream,  "A_SplatCream*3",                  0.9f, 0.08f, AttMedium, KSfx, 4, 0.05f, Count, 0.f, 0.1f),
		D(ColaFizz,    "A_ColaFizz*1",                    0.8f, 0.05f, AttMedium, KSfx, 2, 0.50f, Count, 0.f, 0.f),
		D(Crumbs,      "A_Crumbs*1",                      0.8f, 0.10f, AttSmall,  KSfx, 3, 0.15f, Count, 0.f, 0.f),
		D(CurtainRustle,"A_CurtainRustle*1",              0.8f, 0.10f, AttMedium, KSfx, 2, 0.40f, Count, 0.f, 0.f),
		// ---- the residents
		D(SlipperSlap, "A_SlipperSlap*2",                 1.0f, 0.06f, AttHuge,   KSfx, 3, 0.10f, Count, 0.f, 0.3f),
		D(HumanGrumble,"A_HumanGrumble*2",                0.9f, 0.04f, AttHuge,   KVoice, 1, 4.00f, Count, 0.f, 0.f),
		D(HumanRoar,   "A_HumanRoar*2",                   1.0f, 0.04f, AttHuge,   KVoice, 1, 2.00f, Count, 0.f, 0.5f),
		D(HumanSlip,   "A_HumanSlip*1",                   1.0f, 0.04f, AttHuge,   KSfx, 1, 1.00f, Count, 0.f, 0.6f),
		D(BatWhoosh,   "A_BatWhoosh*2",                   1.0f, 0.08f, AttLarge,  KSfx, 2, 0.20f, Count, 0.f, 0.3f),
		D(BatImpact,   "A_BatImpact*2",                   1.0f, 0.06f, AttHuge,   KSfx, 2, 0.15f, Count, 0.f, 0.6f),
		D(HumanPickup, "A_HumanPickup*2",                 0.9f, 0.06f, AttLarge,  KSfx, 2, 0.50f, Count, 0.f, 0.1f),
		D(HumanSniff,  "A_HumanSniff*1",                  0.9f, 0.05f, AttLarge,  KVoice, 1, 3.00f, Count, 0.f, 0.1f),
		D(CatPurr,     "A_CatPurr*1",                     0.8f, 0.00f, AttMedium, KVoice, 1, 0.f,   Count, 0.f, 0.f),
		D(CatHiss,     "A_CatHiss*2",                     1.0f, 0.06f, AttLarge,  KVoice, 1, 1.00f, Count, 0.f, 0.3f),
		D(CatPounce,   "A_CatPounce*1",                   1.0f, 0.06f, AttLarge,  KSfx, 1, 0.80f, Count, 0.f, 0.4f),
		D(DogSnort,    "A_DogSnort*2",                    0.9f, 0.06f, AttLarge,  KVoice, 1, 2.00f, Count, 0.f, 0.f),
		D(DogBark,     "A_DogBark*3",                     1.0f, 0.06f, AttHuge,   KVoice, 2, 0.50f, Count, 0.f, 0.3f),
		D(DogSniff,    "A_DogSniff*2",                    0.8f, 0.06f, AttMedium, KVoice, 1, 1.50f, Count, 0.f, 0.f),
		D(DogEat,      "A_DogEat*2",                      0.9f, 0.06f, AttMedium, KSfx, 1, 1.50f, Count, 0.f, 0.f),
		D(Broom,       "A_Broom*2",                       0.9f, 0.08f, AttLarge,  KSfx, 2, 0.40f, Count, 0.f, 0.2f),
		D(KidGiggle,   "A_KidGiggle*2",                   0.9f, 0.06f, AttLarge,  KVoice, 1, 3.00f, Count, 0.f, 0.f),
		D(KidShout,    "A_KidShout*1",                    1.0f, 0.06f, AttHuge,   KVoice, 1, 3.00f, Count, 0.f, 0.2f),
		D(SpiderSkitter,"A_SpiderSkitter*1",              0.8f, 0.08f, AttSmall,  KSfx, 2, 1.00f, Count, 0.f, 0.1f),
		// ---- ambience
		D(AmbFridge,   "A_AmbFridge*1",                   0.7f, 0.00f, AttLarge,  KAmb, 1, 0.f,   Count, 0.f, 0.f),
		D(AmbClock,    "A_AmbClock*1",                    0.35f, 0.00f, Att2D,    KAmb, 1, 0.f,   Count, 0.f, 0.f),
		D(AmbDrip,     "A_AmbDrip*1",                     0.9f, 0.00f, AttMedium, KAmb, 1, 0.f,   Count, 0.f, 0.f),
		D(AmbDay,      "A_AmbDay*1",                      0.55f, 0.00f, Att2D,    KAmb, 1, 0.f,   Count, 0.f, 0.f),
		D(AmbEvening,  "A_AmbEvening*1",                  0.55f, 0.00f, Att2D,    KAmb, 1, 0.f,   Count, 0.f, 0.f),
		D(AmbNight,    "A_AmbNight*1",                    0.6f, 0.00f, Att2D,     KAmb, 1, 0.f,   Count, 0.f, 0.f),
		// ---- inside people
		D(Sneeze,      "A_Sneeze*2",                      1.0f, 0.05f, AttHuge,   KVoice, 1, 1.00f, Count, 0.f, 0.4f),
		D(EarSlap,     "A_EarSlap*2",                     1.0f, 0.06f, AttHuge,   KSfx, 2, 0.30f, Count, 0.f, 0.3f),
		D(PocketRustle,"A_PocketRustle*2",                0.9f, 0.08f, AttMedium, KSfx, 2, 0.40f, Count, 0.f, 0.f),
		D(Heartbeat,   "A_Heartbeat*1",                   0.8f, 0.02f, Att2D,     KSfx, 1, 0.30f, Count, 0.f, 0.f),
		// ---- materials and chain reactions (AntReact.h; made by Tools/chains/make_chain_sfx.py)
		D(PowerDown,   "A_PowerDown*1",                   1.0f, 0.02f, AttHuge,   KSfx, 1, 1.00f, Count, 0.f, 0.5f),
		D(PowerUp,     "A_PowerUp*1",                     0.8f, 0.02f, AttHuge,   KSfx, 1, 1.00f, Count, 0.f, 0.f),
		D(BreakerClick,"A_BreakerClick*2",                1.0f, 0.05f, AttLarge,  KSfx, 2, 0.20f, Count, 0.f, 0.f),
		D(MicroBeep,   "A_MicroBeep*1",                   0.9f, 0.00f, AttHuge,   KSfx, 1, 0.25f, Count, 0.f, 0.f),
		D(FlourWhoosh, "A_FlourWhoosh*2",                 1.0f, 0.06f, AttLarge,  KSfx, 2, 0.30f, Count, 0.f, 0.3f),
		D(SlingTwang,  "A_SlingTwang*2",                  1.0f, 0.08f, AttLarge,  KSfx, 2, 0.20f, Count, 0.f, 0.2f),
		D(StaticCrackle,"A_StaticCrackle*2",              0.8f, 0.10f, AttMedium, KSfx, 3, 0.10f, Count, 0.f, 0.f),
		D(IceClink,    "A_IceClink*2",                    0.9f, 0.08f, AttMedium, KSfx, 2, 0.10f, Count, 0.f, 0.f),
	};
#undef D
	static_assert(UE_ARRAY_COUNT(Table) == (int32)EAntSfx::Count, "one AntAudio table row per EAntSfx, in order");

	const FSfxDef& Def(uint8 Id) { return Table[Id]; }

	const TCHAR* Names[] = {
		TEXT("Spit"), TEXT("Splat"), TEXT("Hit"), TEXT("Crit"), TEXT("Kill"), TEXT("Grab"), TEXT("Drop"), TEXT("Deliver"), TEXT("Crunch"), TEXT("Stomp"), TEXT("Spray"),
		TEXT("Squish"), TEXT("Jump"), TEXT("Zap"), TEXT("Sizzle"), TEXT("Splash"), TEXT("Rage"), TEXT("Bite"), TEXT("Buzz"), TEXT("Web"), TEXT("Hiss"), TEXT("Step"), TEXT("Ambience"),
		TEXT("Swat"), TEXT("HumanStep"), TEXT("Faucet"), TEXT("FridgeDoor"), TEXT("Toaster"), TEXT("ArmyMarch"), TEXT("ArmyCharge"), TEXT("PickupGear"), TEXT("Legendary"),
		TEXT("SpiderStep"), TEXT("HumanPanic"), TEXT("StepWood"), TEXT("StepMetal"), TEXT("StepFabric"), TEXT("StepSoil"), TEXT("StepGrass"),
		TEXT("StepSticky"), TEXT("AcidHit"), TEXT("AntHurt"), TEXT("Release"), TEXT("CarryStrain"), TEXT("CarryThump"), TEXT("RipOff"), TEXT("DeliverCheer"), TEXT("DeliverChime"),
		TEXT("TrailLay"), TEXT("TrailCut"), TEXT("WorkerPatter"), TEXT("CardPick"), TEXT("CardFlip"),
		TEXT("ScrewTurn"), TEXT("ScrewDrop"), TEXT("HingeCreak"), TEXT("Wobble"), TEXT("CrashWood"), TEXT("CrashGlass"), TEXT("CrashCeramic"), TEXT("CrashMetal"), TEXT("CrashPlastic"),
		TEXT("SoftThud"), TEXT("SplatJam"), TEXT("SplatCream"), TEXT("ColaFizz"), TEXT("Crumbs"), TEXT("CurtainRustle"),
		TEXT("SlipperSlap"), TEXT("HumanGrumble"), TEXT("HumanRoar"), TEXT("HumanSlip"), TEXT("BatWhoosh"), TEXT("BatImpact"), TEXT("HumanPickup"), TEXT("HumanSniff"),
		TEXT("CatPurr"), TEXT("CatHiss"), TEXT("CatPounce"), TEXT("DogSnort"), TEXT("DogBark"), TEXT("DogSniff"), TEXT("DogEat"), TEXT("Broom"), TEXT("KidGiggle"), TEXT("KidShout"),
		TEXT("SpiderSkitter"), TEXT("AmbFridge"), TEXT("AmbClock"), TEXT("AmbDrip"), TEXT("AmbDay"), TEXT("AmbEvening"), TEXT("AmbNight"),
		TEXT("Sneeze"), TEXT("EarSlap"), TEXT("PocketRustle"), TEXT("Heartbeat"),
		TEXT("PowerDown"), TEXT("PowerUp"), TEXT("BreakerClick"), TEXT("MicroBeep"), TEXT("FlourWhoosh"), TEXT("SlingTwang"), TEXT("StaticCrackle"), TEXT("IceClink") };
	static_assert(UE_ARRAY_COUNT(Names) == (int32)EAntSfx::Count, "one name per EAntSfx");

	/** "A_Name*N S_Old" -> package paths (no probing for files that may not exist: a packaged build logs every miss). */
	void ExpandFiles(const TCHAR* Files, TArray<FString>& Out)
	{
		TArray<FString> Tokens;
		FString(Files).ParseIntoArrayWS(Tokens);
		for (const FString& T : Tokens)
		{
			FString Base = T;
			int32 Num = 0;
			int32 Star = INDEX_NONE;
			if (T.FindChar(TEXT('*'), Star))
			{
				Base = T.Left(Star);
				Num = FCString::Atoi(*T.Mid(Star + 1));
			}
			const TCHAR* Dir = Base.StartsWith(TEXT("A_")) ? TEXT("/Game/Audio/Sfx/") : Base.StartsWith(TEXT("M_")) ? TEXT("/Game/Audio/Music/") : TEXT("/Game/SoundsReal/");
			if (Num <= 0)
			{
				Out.Add(FString(Dir) + Base + TEXT(".") + Base);
				continue;
			}
			const FString Stem = Base.EndsWith(TEXT("_")) ? Base.LeftChop(1) : Base;
			for (int32 i = 1; i <= Num; ++i)
			{
				const FString N = FString::Printf(TEXT("%s_%d"), *Stem, i);
				Out.Add(FString(Dir) + N + TEXT(".") + N);
			}
		}
	}

	struct FRuntime
	{
		TArray<TArray<TObjectPtr<USoundBase>>> Sounds;
		TArray<bool> Loaded;
		TArray<double> LastPlayed;
		TArray<uint8> LastVariant;
		TArray<TObjectPtr<USoundConcurrency>> Conc;
		TObjectPtr<USoundAttenuation> Att[AttNum] = {};
		bool bInit = false;
	};
	FRuntime& RT()
	{
		static FRuntime R;
		if (!R.bInit)
		{
			R.bInit = true;
			const int32 N = (int32)EAntSfx::Count;
			R.Sounds.SetNum(N);
			R.Loaded.SetNumZeroed(N);
			R.LastPlayed.Init(-100.0, N);
			R.LastVariant.Init(255, N);
			R.Conc.SetNum(N);
		}
		return R;
	}

	/** Falloffs for an 8 mm hero (1 m = 1900 uu): what an ant's feet do is heard within a hand's width, the Human's steps across the flat. */
	USoundAttenuation* MakeAtt(uint8 Kind)
	{
		FRuntime& R = RT();
		if (Kind == Att2D || Kind >= AttNum)
		{
			return nullptr;
		}
		if (!R.Att[Kind])
		{
			static const float Inner[AttNum] = { 0.f, 120.f, 300.f, 900.f, 2500.f, 5000.f };
			static const float Fall[AttNum] = { 0.f, 1600.f, 3800.f, 9000.f, 22000.f, 42000.f };
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), *FString::Printf(TEXT("AntAtt%d"), Kind));
			A->AddToRoot();
			FSoundAttenuationSettings& S = A->Attenuation;
			S.bAttenuate = true;
			S.bSpatialize = true;
			S.AttenuationShape = EAttenuationShape::Sphere;
			S.AttenuationShapeExtents = FVector(Inner[Kind], 0.f, 0.f);
			S.FalloffDistance = Fall[Kind];
			S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
			S.dBAttenuationAtMax = -48.f;
			S.bAttenuateWithLPF = true;
			S.LPFRadiusMin = Inner[Kind];
			S.LPFRadiusMax = Inner[Kind] + Fall[Kind];
			S.LPFFrequencyAtMin = 20000.f;
			S.LPFFrequencyAtMax = Kind >= AttLarge ? 1800.f : 3500.f;
			S.bEnableOcclusion = false;
			S.NonSpatializedRadiusStart = Inner[Kind] * 0.25f;
			S.NonSpatializedRadiusEnd = Inner[Kind] * 0.05f;
			S.NonSpatializedRadiusMode = ENonSpatializedRadiusSpeakerMapMode::OmniDirectional;
			R.Att[Kind] = A;
		}
		return R.Att[Kind];
	}

	USoundConcurrency* ConcFor(uint8 Id)
	{
		FRuntime& R = RT();
		const FSfxDef& D = Def(Id);
		if (D.MaxCount == 0)
		{
			return nullptr;
		}
		if (!R.Conc[Id])
		{
			USoundConcurrency* C = NewObject<USoundConcurrency>(GetTransientPackage(), *FString::Printf(TEXT("AntConc_%s"), Names[Id]));
			C->AddToRoot();
			C->Concurrency.MaxCount = D.MaxCount;
			C->Concurrency.bLimitToOwner = false;
			C->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopQuietest;
			R.Conc[Id] = C;
		}
		return R.Conc[Id];
	}

	float ClassVolume(uint8 Kls)
	{
		const float M = FMath::Clamp(CVarMaster.GetValueOnGameThread(), 0.f, 1.f);
		switch (Kls)
		{
		case AntMixClass::Voice: return M * FMath::Clamp(CVarVoice.GetValueOnGameThread(), 0.f, 1.f);
		case AntMixClass::Ui: return M * FMath::Clamp(CVarUi.GetValueOnGameThread(), 0.f, 1.f);
		case AntMixClass::Ambience: return M * FMath::Clamp(CVarAmb.GetValueOnGameThread(), 0.f, 1.f);
		case AntMixClass::Music: return CVarMute.GetValueOnGameThread() ? 0.f : M * FMath::Clamp(CVarMusic.GetValueOnGameThread(), 0.f, 1.f);
		default: return M * FMath::Clamp(CVarSfx.GetValueOnGameThread(), 0.f, 1.f);
		}
	}

	bool ListenerAt(UWorld* W, FVector& Out)
	{
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->IsLocalController())
		{
			return false;
		}
		FVector Front, Right;
		PC->GetAudioListenerPosition(Out, Front, Right);
		return true;
	}

	bool IsStep(uint8 Id)
	{
		return Id == (uint8)EAntSfx::Step || (Id >= (uint8)EAntSfx::StepWood && Id <= (uint8)EAntSfx::StepGrass);
	}

	bool StickyAt(const FVector& P)
	{
		if (LevelV2::ZoneAt(P) == 0)
		{
			return true;
		}
		return LevelV3::Enabled() && LevelV3::PlayZoneAt(0, P);
	}
}

// ============================================================================ playing
namespace AntAudio
{
bool LogOn()
{
	static const bool bCmd = FParse::Param(FCommandLine::Get(), TEXT("AntSfxLog"));
	return bCmd || CVarLog.GetValueOnGameThread() != 0;
}

const TCHAR* Name(uint8 Id)
{
	return Id < (uint8)EAntSfx::Count ? Names[Id] : TEXT("?");
}

static void LoadId(uint8 Id)
{
	FRuntime& R = RT();
	if (R.Loaded[Id])
	{
		return;
	}
	R.Loaded[Id] = true;
	if (Table[Id].Id != (EAntSfx)Id)
	{
		UE_LOG(LogTemp, Error, TEXT("ANTSFX table row %d is %d: the table is out of order"), Id, (int32)Table[Id].Id);
	}
	TArray<FString> Paths;
	ExpandFiles(Def(Id).Files, Paths);
	for (const FString& P : Paths)
	{
		USoundBase* S = LoadObject<USoundBase>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (S)
		{
			S->AddToRoot();   // the cache survives map travel, the sound must too
			R.Sounds[Id].Add(S);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ANTSFX missing %s (%s)"), *P, Names[Id]);
		}
	}
}

USoundBase* Get(uint8 Id)
{
	if (Id >= (uint8)EAntSfx::Count)
	{
		return nullptr;
	}
	LoadId(Id);
	FRuntime& R = RT();
	const TArray<TObjectPtr<USoundBase>>& L = R.Sounds[Id];
	if (L.Num() == 0)
	{
		return (Id >= (uint8)EAntSfx::StepWood && Id <= (uint8)EAntSfx::StepGrass) ? Get((uint8)EAntSfx::Step) : nullptr;
	}
	// a random variation, never the same one twice in a row
	int32 Pick = FMath::RandRange(0, L.Num() - 1);
	if (L.Num() > 1 && Pick == R.LastVariant[Id])
	{
		Pick = (Pick + 1 + FMath::RandRange(0, L.Num() - 2)) % L.Num();
	}
	R.LastVariant[Id] = (uint8)Pick;
	return L[Pick].Get();
}

USoundAttenuation* AttenuationFor(uint8 Id)
{
	return Id < (uint8)EAntSfx::Count ? MakeAtt(FMath::Max<uint8>(Def(Id).Att, AttSmall)) : MakeAtt(AttMedium);
}

EAntSfx CrashFor(int32 Mat)
{
	switch (Mat)
	{
	case 0: return EAntSfx::CrashGlass;                                   // glass
	case 1: case 2: return EAntSfx::CrashCeramic;                         // china, terracotta
	case 3: case 4: case 8: return EAntSfx::CrashPlastic;                 // bottle, plastic, latex
	case 9: return EAntSfx::CrashWood;
	case 10: case 13: return EAntSfx::CrashMetal;                         // metal, tin
	case 12: return EAntSfx::SplatJam;                                    // food
	default: return EAntSfx::SoftThud;                                    // cardboard, paper, cloth, yarn
	}
}

EAntSfx ForMess(uint8 MessKind)
{
	switch (MessKind)
	{
	case 0: case 1: case 6: case 8: return EAntSfx::SplatJam;      // jam, honey, mud, chocolate
	case 2: case 12: return EAntSfx::ColaFizz;                     // cola, juice
	case 3: case 7: return EAntSfx::Splash;                        // oil, water
	case 4: return EAntSfx::SplatCream;
	case 5: case 13: return EAntSfx::Crumbs;                       // crumbs, sugar
	default: return EAntSfx::Count;                                // scratches, prints, drag marks: silent
	}
}

EAntSfx ForWreck(uint8 WreckMat, bool bBreak)
{
	switch (WreckMat)
	{
	case AntWreck::Wood: case AntWreck::Board: return bBreak ? EAntSfx::CrashWood : EAntSfx::BatImpact;
	case AntWreck::Metal: case AntWreck::Appliance: return EAntSfx::CrashMetal;
	case AntWreck::Glass: return bBreak ? EAntSfx::CrashGlass : EAntSfx::Crunch;
	case AntWreck::China: return bBreak ? EAntSfx::CrashCeramic : EAntSfx::Crunch;
	case AntWreck::Plastic: return EAntSfx::CrashPlastic;
	case AntWreck::Food: return bBreak ? EAntSfx::SplatJam : EAntSfx::SplatCream;
	default: return EAntSfx::SoftThud;   // cloth, paper
	}
}

EAntSfx ForWeapon(uint8 WeaponKind)
{
	switch (WeaponKind)
	{
	case AntWeapons::Slipper: return EAntSfx::SlipperSlap;
	case AntWeapons::Ladle: case AntWeapons::Pan: return EAntSfx::CrashMetal;
	case AntWeapons::Book: return EAntSfx::SoftThud;
	case AntWeapons::RollingPin: case AntWeapons::Bat: return EAntSfx::BatImpact;
	case AntWeapons::Broom: return EAntSfx::Broom;
	case AntWeapons::Sausage: return EAntSfx::SplatCream;
	default: return EAntSfx::Swat;   // the newspaper, the spatula
	}
}

static IConsoleVariable* VolumeVar(uint8 MixClass)
{
	switch (MixClass)
	{
	case AntMixClass::Sfx: return CVarSfx.AsVariable();
	case AntMixClass::Voice: return CVarVoice.AsVariable();
	case AntMixClass::Ui: return CVarUi.AsVariable();
	case AntMixClass::Ambience: return CVarAmb.AsVariable();
	case AntMixClass::Music: return CVarMusic.AsVariable();
	default: return CVarMaster.AsVariable();
	}
}

float GetVolume(uint8 MixClass)
{
	return FMath::Clamp(VolumeVar(MixClass)->GetFloat(), 0.f, 1.f);
}

void SetVolume(uint8 MixClass, float Value)
{
	VolumeVar(MixClass)->Set(FMath::Clamp(Value, 0.f, 1.f), ECVF_SetByConsole);   // the subsystem notices and saves it
}

bool MusicMuted()
{
	return CVarMute.GetValueOnGameThread() != 0;
}

void SetMusicMuted(bool bMuted)
{
	CVarMute.AsVariable()->Set(bMuted ? 1 : 0, ECVF_SetByConsole);
}

static void PlayImpl(const UObject* Ctx, uint8 Id, const FVector* Loc, float Volume, float Pitch, int32 Depth)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W || W->GetNetMode() == NM_DedicatedServer || Id >= (uint8)EAntSfx::Count || Volume <= 0.001f)
	{
		return;
	}
	// a foot in jam or cola: the sticky step, whatever the surface under it
	if (Loc && IsStep(Id) && StickyAt(*Loc))
	{
		Id = (uint8)EAntSfx::StepSticky;
	}
	const FSfxDef& D = Def(Id);
	FRuntime& R = RT();
	const double Now = W->GetRealTimeSeconds();
	if (D.Cooldown > 0.f && Now - R.LastPlayed[Id] < D.Cooldown && Now >= R.LastPlayed[Id])
	{
		return;
	}
	FVector Ear = FVector::ZeroVector;
	const bool bEar = ListenerAt(W, Ear);
	float Dist = 0.f;
	if (Loc && D.Att != Att2D && bEar)
	{
		static const float Reach[AttNum] = { 0.f, 1900.f, 4400.f, 10500.f, 26000.f, 50000.f };
		Dist = (float)FVector::Dist(Ear, *Loc);
		if (Dist > Reach[D.Att])
		{
			return;   // too far to hear: not even a voice slot
		}
	}
	USoundBase* S = Get(Id);
	if (!S)
	{
		return;
	}
	R.LastPlayed[Id] = Now;
	const float V = Volume * D.Vol * ClassVolume(D.Kls);
	const float P = Pitch * FMath::FRandRange(1.f - D.Jitter, 1.f + D.Jitter);
	if (V > 0.001f)
	{
		if (Loc && D.Att != Att2D)
		{
			UGameplayStatics::PlaySoundAtLocation(W, S, *Loc, FRotator::ZeroRotator, V, P, 0.f, MakeAtt(D.Att), ConcFor(Id));
		}
		else
		{
			UGameplayStatics::PlaySound2D(W, S, V, P, 0.f, ConcFor(Id));
		}
		if (LogOn())
		{
			UE_LOG(LogTemp, Display, TEXT("ANTSFX %s %s v=%.2f p=%.2f d=%.0f"), Names[Id], *GetNameSafe(S), V, P, Dist);
		}
	}
	if (D.Excite > 0.f)
	{
		if (UAntAudioSubsystem* Sub = UAntAudioSubsystem::Get(W))
		{
			const float Near = Loc && bEar ? FMath::Clamp(1.f - Dist / 9000.f, 0.f, 1.f) : 1.f;
			Sub->AddExcite(D.Excite * Near * FMath::Min(Volume, 1.5f) * 0.5f);
		}
	}
	if (D.Also != EAntSfx::Count && Depth < 2)
	{
		PlayImpl(Ctx, (uint8)D.Also, Loc, Volume * D.AlsoVol, 1.f, Depth + 1);
	}
}

void Play(const UObject* Ctx, EAntSfx Id, const FVector& Location, float Intensity)
{
	const float I = FMath::Clamp(Intensity, 0.f, 2.f);
	PlayImpl(Ctx, (uint8)Id, &Location, FMath::Lerp(0.55f, 1.f, FMath::Min(I, 1.f)) * (I > 1.f ? 1.f + 0.25f * (I - 1.f) : 1.f), FMath::Clamp(1.08f - 0.08f * I, 0.9f, 1.1f), 0);
}

void Play2D(const UObject* Ctx, EAntSfx Id, float Intensity)
{
	PlayImpl(Ctx, (uint8)Id, nullptr, FMath::Clamp(Intensity, 0.f, 2.f), 1.f, 0);
}

void Everyone(const UObject* Ctx, EAntSfx Id, const FVector& Location, float Intensity)
{
	// The multicast carries a pitch; harder hits are a little lower.
	AntSfx::Everyone(Ctx, (uint8)Id, Location, FMath::Clamp(1.08f - 0.08f * Intensity, 0.9f, 1.1f));
}

void PlayRaw(const UObject* Ctx, uint8 Id, const FVector& Location, float Volume, float Pitch, bool b2D)
{
	PlayImpl(Ctx, Id, b2D ? nullptr : &Location, Volume, Pitch, 0);
}

void CarryBeat(const UObject* Ctx, const FVector& At, int32 Carriers)
{
	const float I = FMath::Clamp(0.5f + 0.15f * Carriers, 0.5f, 1.4f);
	Play(Ctx, EAntSfx::CarryThump, At, I);
	if (FMath::FRand() < 0.5f)
	{
		Play(Ctx, EAntSfx::CarryStrain, At, I * 0.8f);
	}
}

void Swarm(const UObject* Ctx, const FVector& Center, int32 Count)
{
	if (UAntAudioSubsystem* Sub = UAntAudioSubsystem::Get(Ctx))
	{
		Sub->SwarmAt(Center, Count);
	}
}

void Excite(const UObject* Ctx, float Amount, const FVector& Where)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	UAntAudioSubsystem* Sub = UAntAudioSubsystem::Get(W);
	FVector Ear;
	if (Sub && ListenerAt(W, Ear))
	{
		Sub->AddExcite(Amount * FMath::Clamp(1.f - (float)FVector::Dist(Ear, Where) / 12000.f, 0.15f, 1.f));
	}
}

void Duck(const UObject* Ctx, float Seconds)
{
	if (UAntAudioSubsystem* Sub = UAntAudioSubsystem::Get(Ctx))
	{
		Sub->DuckFor(Seconds);
	}
}

void Sting(const UObject* Ctx, EAntSting S)
{
	if (UAntAudioSubsystem* Sub = UAntAudioSubsystem::Get(Ctx))
	{
		Sub->PlaySting(S);
	}
}
} // namespace AntAudio

// ============================================================================ the subsystem: music director, ambience, state-driven sounds, mix
namespace
{
	const TCHAR* SetNames[3] = { TEXT("Funk"), TEXT("Lounge"), TEXT("Nest") };
	const TCHAR* LayerNames[4] = { TEXT("Base"), TEXT("Tension"), TEXT("Action"), TEXT("Urgency") };
	const TCHAR* StingFiles[(int32)EAntSting::Count] = { TEXT("M_Sting_RaidStart"), TEXT("M_Sting_Win"), TEXT("M_Sting_Fail"), TEXT("M_Sting_Card"), TEXT("M_Sting_LastLight") };

	USoundBase* MusicAsset(const FString& N)
	{
		const FString P = FString::Printf(TEXT("/Game/Audio/Music/%s.%s"), *N, *N);
		USoundBase* S = LoadObject<USoundBase>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!S)
		{
			UE_LOG(LogTemp, Warning, TEXT("ANTMUSIC missing %s"), *P);
		}
		else
		{
			S->AddToRoot();
			S->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;   // a quiet stem keeps its place in the bar
		}
		return S;
	}

	float Smooth01(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / FMath::Max(B - A, 1e-3f), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float Approach(float Cur, float Target, float Up, float Down, float Dt)
	{
		return Target > Cur ? FMath::Min(Target, Cur + Up * Dt) : FMath::Max(Target, Cur - Down * Dt);
	}

	void LoadSettingsOnce()
	{
		static bool bDone = false;
		if (bDone || !GConfig)
		{
			return;
		}
		bDone = true;
		auto Read = [](const TCHAR* Key, IConsoleVariable* V)
		{
			float F = 0.f;
			if (GConfig->GetFloat(TEXT("AntAudio"), Key, F, GGameUserSettingsIni))
			{
				V->Set(F, ECVF_SetByGameSetting);
			}
		};
		Read(TEXT("Master"), CVarMaster.AsVariable());
		Read(TEXT("Music"), CVarMusic.AsVariable());
		Read(TEXT("Sfx"), CVarSfx.AsVariable());
		Read(TEXT("Voice"), CVarVoice.AsVariable());
		Read(TEXT("Ui"), CVarUi.AsVariable());
		Read(TEXT("Ambience"), CVarAmb.AsVariable());
		Read(TEXT("MusicMute"), CVarMute.AsVariable());
	}
}

bool UAntAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer() || FParse::Param(FCommandLine::Get(), TEXT("nosound")))
	{
		return false;
	}
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

UAntAudioSubsystem* UAntAudioSubsystem::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UAntAudioSubsystem>() : nullptr;
}

TStatId UAntAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAntAudioSubsystem, STATGROUP_Tickables);
}

void UAntAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadSettingsOnce();
	SetupMix();
	// warm the cache: no hitch the first time a crash or a bark plays
	for (uint8 Id = 0; Id < (uint8)EAntSfx::Count; ++Id)
	{
		AntAudio::Get(Id);
	}
	// the server hears about carried loads dropped, broken, snatched (AntCarry events) and multicasts their sounds
	if (InWorld.GetNetMode() != NM_Client)
	{
		CarryEventsHandle = AntCarry::Events().AddUObject(this, &UAntAudioSubsystem::OnCarryEvent);
	}
	UE_LOG(LogTemp, Display, TEXT("ANTMUSIC audio subsystem up (%s)"), *InWorld.GetMapName());
}

void UAntAudioSubsystem::Deinitialize()
{
	if (CarryEventsHandle.IsValid())
	{
		AntCarry::Events().Remove(CarryEventsHandle);
		CarryEventsHandle.Reset();
	}
	for (UAudioComponent* C : Owned)
	{
		if (C)
		{
			C->Stop();
		}
	}
	Owned.Reset();
	Super::Deinitialize();
}

UAudioComponent* UAntAudioSubsystem::Loop(EAntSfx Id, const FVector* At, float Volume)
{
	UWorld* W = GetWorld();
	USoundBase* S = AntAudio::Get((uint8)Id);
	if (!W || !S)
	{
		return nullptr;
	}
	UAudioComponent* C = At
		? UGameplayStatics::SpawnSoundAtLocation(W, S, *At, FRotator::ZeroRotator, Volume, 1.f, 0.f, AntAudio::AttenuationFor((uint8)Id), nullptr, false)
		: UGameplayStatics::CreateSound2D(W, S, Volume, 1.f, 0.f, nullptr, false, false);
	if (C)
	{
		if (!At)
		{
			C->Play();
		}
		Owned.Add(C);
	}
	return C;
}

void UAntAudioSubsystem::SetupMix()
{
	// The mix assets (Tools/audio/import_audio.py): SoundClasses Master > Music / SFX / Voice / UI / Ambience on every wave,
	// SM_AntDuck dips Music and Ambience under shouts and stings. Volumes are applied per play from the ant.Audio.* settings.
}

void UAntAudioSubsystem::DuckFor(float Seconds)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	static USoundMix* DuckMix = nullptr;
	if (!DuckMix)
	{
		DuckMix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/Mix/SM_AntDuck.SM_AntDuck"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (DuckMix)
		{
			DuckMix->AddToRoot();
		}
	}
	if (DuckMix)
	{
		UGameplayStatics::PushSoundMixModifier(W, DuckMix);
		FTimerHandle H;
		TWeakObjectPtr<UWorld> WW = W;
		W->GetTimerManager().SetTimer(H, [WW]()
		{
			if (WW.IsValid() && DuckMix)
			{
				UGameplayStatics::PopSoundMixModifier(WW.Get(), DuckMix);
			}
		}, FMath::Max(0.1f, Seconds), false);
	}
	else
	{
		DuckUntil = FMath::Max(DuckUntil, (float)W->GetRealTimeSeconds() + Seconds);   // no mix asset: dip the music by hand
	}
}

void UAntAudioSubsystem::PlaySting(EAntSting S)
{
	UWorld* W = GetWorld();
	static USoundBase* Stings[(int32)EAntSting::Count] = {};
	const int32 I = (int32)S;
	if (!W || I < 0 || I >= (int32)EAntSting::Count)
	{
		return;
	}
	if (!Stings[I])
	{
		Stings[I] = MusicAsset(StingFiles[I]);
	}
	const float V = ClassVolume(AntMixClass::Music);
	if (Stings[I] && V > 0.001f)
	{
		UGameplayStatics::PlaySound2D(W, Stings[I], V * 0.95f);
		DuckUntil = FMath::Max(DuckUntil, (float)W->GetRealTimeSeconds() + (S == EAntSting::Card ? 0.8f : 2.2f));
		if (AntAudio::LogOn())
		{
			UE_LOG(LogTemp, Display, TEXT("ANTMUSIC sting %s"), StingFiles[I]);
		}
	}
}

void UAntAudioSubsystem::SwarmAt(const FVector& Center, int32 Count)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	PatterAt = PatterSeen > W->GetRealTimeSeconds() - 1.0 ? FMath::Lerp(PatterAt, Center, 0.5f) : Center;
	PatterSeen = W->GetRealTimeSeconds();
	PatterLevel = FMath::Max(PatterLevel * 0.5f, FMath::Clamp(FMath::Log2(1.f + Count) / 7.f, 0.f, 1.f));
}

FString UAntAudioSubsystem::Describe() const
{
	return FString::Printf(TEXT("set=%s I=%.2f base=%.2f ten=%.2f act=%.2f urg=%.2f excite=%.2f"), ActiveSet >= 0 ? SetNames[ActiveSet] : TEXT("-"),
		Intensity, LayerGain[0], LayerGain[1], LayerGain[2], LayerGain[3], Excitement);
}

// ---------------------------------------------------------------------------- tick
void UAntAudioSubsystem::Tick(float Dt)
{
	UWorld* W = GetWorld();
	if (!W || !W->HasBegunPlay())
	{
		return;
	}
	Dt = FMath::Min(Dt, 0.1f);
	// the mute key and saving the settings when they change
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		// (while the filming mode is on, M is its own key: AntFilm.cpp)
		const AAntPlayerController* APC = Cast<AAntPlayerController>(PC);
		const bool bFilming = APC && APC->Film && APC->Film->IsOn();
		if (PC->IsLocalController() && !bFilming && PC->WasInputKeyJustPressed(EKeys::M) && !PC->IsInputKeyDown(EKeys::LeftControl))
		{
			CVarMute.AsVariable()->Set(CVarMute.GetValueOnGameThread() ? 0 : 1, ECVF_SetByConsole);
			UE_LOG(LogTemp, Display, TEXT("ANTMUSIC music %s (M)"), CVarMute.GetValueOnGameThread() ? TEXT("muted") : TEXT("on"));
		}
	}
	ApplyVolumes();
	if (!bMusicStarted)
	{
		StartMusic();
	}
	StateClock += Dt;
	if (StateClock >= 0.2f)
	{
		UpdateState();
		StateClock = 0.f;
	}
	TickMusic(Dt);
	if (!bAmbienceStarted && W->GetGameState())
	{
		StartAmbience();
	}
	TickAmbience(Dt);
	WorldScanClock += Dt;
	if (WorldScanClock >= 0.12f)
	{
		TickWorldState(WorldScanClock);
		WorldScanClock = 0.f;
	}
	PetClock += Dt;
	if (PetClock >= 0.25f)
	{
		TickPets(PetClock);
		PetClock = 0.f;
	}
	if (AntAudio::LogOn())
	{
		LogClock += Dt;
		if (LogClock >= 2.f)
		{
			LogClock = 0.f;
			UE_LOG(LogTemp, Display, TEXT("ANTMUSIC %s"), *Describe());
		}
	}
}

void UAntAudioSubsystem::ApplyVolumes()
{
	const float M = CVarMaster.GetValueOnGameThread(), Mu = CVarMusic.GetValueOnGameThread(), S = CVarSfx.GetValueOnGameThread();
	const float A = CVarAmb.GetValueOnGameThread(), V = CVarVoice.GetValueOnGameThread(), U = CVarUi.GetValueOnGameThread();
	const float Mute = (float)CVarMute.GetValueOnGameThread();
	if (M != LastMaster || Mu != LastMusic || S != LastSfx || A != LastAmb || V != LastVoice || U != LastUi || Mute != (float)bMuteKeyDown)
	{
		const bool bFirst = LastMaster < 0.f;
		LastMaster = M; LastMusic = Mu; LastSfx = S; LastAmb = A; LastVoice = V; LastUi = U; bMuteKeyDown = Mute != 0.f;
		if (!bFirst && GConfig)
		{
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("Master"), M, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("Music"), Mu, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("Sfx"), S, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("Voice"), V, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("Ui"), U, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("Ambience"), A, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("AntAudio"), TEXT("MusicMute"), Mute, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
}

// ---------------------------------------------------------------------------- music
void UAntAudioSubsystem::StartMusic()
{
	UWorld* W = GetWorld();
	bMusicStarted = true;
	static const float Loops[3] = { 117.551f, 109.091f, 76.8f };
	for (int32 S = 0; S < 3; ++S)
	{
		Sets[S].Name = SetNames[S];
		Sets[S].LoopSeconds = Loops[S];
		const int32 NumLayers = S == 2 ? 1 : 4;
		for (int32 L = 0; L < NumLayers; ++L)
		{
			const FString N = S == 2 ? FString(TEXT("M_Nest")) : FString::Printf(TEXT("M_%s_%s"), SetNames[S], LayerNames[L]);
			USoundBase* Snd = MusicAsset(N);
			UAudioComponent* C = Snd ? UGameplayStatics::CreateSound2D(W, Snd, 0.f, 1.f, 0.f, nullptr, false, false) : nullptr;
			if (C)
			{
				C->bIsMusic = true;
				C->bAllowSpatialization = false;
				Owned.Add(C);
			}
			Sets[S].Layers.Add(C);
		}
	}
}

void UAntAudioSubsystem::StartSet(int32 Index)
{
	FMusicSet& S = Sets[Index];
	// every layer in the same frame: the stems are one piece of music and must stay sample-locked
	for (UAudioComponent* C : S.Layers)
	{
		if (C)
		{
			C->SetVolumeMultiplier(0.001f);
			C->Play(0.f);
		}
	}
	S.StartedAt = GetWorld()->GetRealTimeSeconds();
	if (AntAudio::LogOn())
	{
		UE_LOG(LogTemp, Display, TEXT("ANTMUSIC start set %s"), SetNames[Index]);
	}
}

float UAntAudioSubsystem::ComputeThreat(float& OutChase) const
{
	UWorld* W = GetWorld();
	OutChase = 0.f;
	APlayerController* PC = W->GetFirstPlayerController();
	APawn* Me = PC ? PC->GetPawn() : nullptr;
	FVector MyPos = FVector::ZeroVector;
	if (Me)
	{
		MyPos = Me->GetActorLocation();
	}
	else if (!ListenerAt(W, MyPos))
	{
		return 0.f;
	}
	float T = 0.f;
	if (Cast<AHumanCharacter>(Me))
	{
		T = 0.5f;   // you are the Human (versus): it is a fight all the way
	}
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		AHumanCharacter* H = *It;
		if (H == Me || H->IsHidden())
		{
			continue;
		}
		const FVector HP = H->GetActorLocation();
		const float D = (float)FVector::Dist(HP, MyPos);
		const float Prox = FMath::Clamp(1.f - (D - 2000.f) / 12000.f, 0.f, 1.f);
		const float Rage = FMath::Clamp(H->Rage / 3.f, 0.f, 1.f);
		T = FMath::Max(T, Prox * (0.3f + 0.5f * Rage));
		const float Toward = (float)FVector::DotProduct(H->GetVelocity(), (MyPos - HP).GetSafeNormal2D());
		if (D < 9000.f && Toward > 150.f)
		{
			OutChase = FMath::Max(OutChase, FMath::Clamp((Toward - 150.f) / 350.f, 0.f, 1.f) * FMath::Clamp(1.2f - D / 9000.f, 0.f, 1.f));
		}
		if (H->bSpraying && D < 6000.f)
		{
			OutChase = FMath::Max(OutChase, 0.8f);
		}
	}
	for (TActorIterator<ACritter> It(W); It; ++It)
	{
		const float D = (float)FVector::Dist(It->GetActorLocation(), MyPos);
		const float Prox = FMath::Clamp(1.f - (D - 1000.f) / 7000.f, 0.f, 1.f);
		T = FMath::Max(T, Prox * 0.35f);
		if ((It->State == 5 || It->State == 4) && D < 5000.f)
		{
			OutChase = FMath::Max(OutChase, 0.4f + 0.5f * (1.f - D / 5000.f));
		}
	}
	return T;
}

void UAntAudioSubsystem::UpdateState()
{
	UWorld* W = GetWorld();
	AAntGameState* GS = W->GetGameState<AAntGameState>();
	AAntRaid* Raid = AAntRaid::Get(W);
	// which piece: the nest march at home and between raids, a raid set in the house
	int32 Want = 2;
	bool bInRaid = false;
	if (Raid)
	{
		bInRaid = Raid->Phase == AntRaidPhase::Raid;
		if (Raid->Phase != LastRaidPhase)
		{
			if (LastRaidPhase != 255)
			{
				if (Raid->Phase == AntRaidPhase::Raid)
				{
					PlaySting(EAntSting::RaidStart);
				}
				else if (Raid->Phase == AntRaidPhase::Results)
				{
					PlaySting(Raid->Result.End == AntRaidEnd::Left ? EAntSting::Win : EAntSting::Fail);
				}
			}
			if (Raid->Phase == AntRaidPhase::Raid)
			{
				ActiveSet = -1;          // pick afresh: this raid's set
				LoopsInSet = 0;
				bLastLightSaid = false;
			}
			LastRaidPhase = Raid->Phase;
		}
	}
	else if (GS)
	{
		bInRaid = GS->Phase == 0;
	}
	if (bInRaid)
	{
		const int32 RaidNo = Raid ? Raid->RaidNo : (GS ? GS->Shift : 0);
		if (ActiveSet < 0 || ActiveSet == 2)
		{
			Want = RaidNo % 2;
			SetTime = 0.f;
		}
		else
		{
			Want = ActiveSet;
			// two loops of a set (~4 min), then the other one, at a calm moment
			if (SetTime > Sets[ActiveSet].LoopSeconds * 2.f && Intensity < 0.45f)
			{
				Want = 1 - ActiveSet;
				SetTime = 0.f;
			}
		}
	}
	ActiveSet = Want;
	// the cards
	if (GS)
	{
		if (GS->CardPicked != LastCardPicked)
		{
			if (LastCardPicked == 255 && GS->CardPicked != 255)
			{
				PlaySting(EAntSting::Card);
				AntAudio::Play2D(this, EAntSfx::CardPick, 1.f);
			}
			LastCardPicked = GS->CardPicked;
		}
	}
	// intensity
	float Chase = 0.f;
	const float Threat = bInRaid ? ComputeThreat(Chase) : 0.f;
	const float RageAll = GS ? GS->HumanRage / 3.f * 0.5f : 0.f;
	// Two questions: is somebody dangerous about (tension: a resident near, angry) and is it happening now (action: a chase,
	// a crash, a blow, a fall). Danger alone never brings the chaos layer, so the music calms down when the noise stops.
	float Tense = FMath::Max(Threat, RageAll);
	float Act = 0.7f * Chase + 0.8f * Excitement + (GS && GS->HumanRage >= 3 && Threat > 0.4f ? 0.25f : 0.f);
	if (bInRaid && ((GS && GS->bWarMode) || AntVersus::Active(W)))
	{
		Act = FMath::Max(Act, 0.55f);   // war, versus (a player is the Human): a fight all the way
	}
	float I = FMath::Max(Tense * 0.6f, FMath::Min(1.f, Tense * 0.3f + Act));
	const float Force = CVarForce.GetValueOnGameThread();
	if (Force >= 0.f)
	{
		I = Tense = Act = Force;
	}
	I = FMath::Clamp(I, 0.f, 1.f);
	ThreatSmoothed = I;
	const bool bUrgent = CVarForceUrgent.GetValueOnGameThread() != 0
		|| (bInRaid && GS && GS->TimeRemaining > 0.f && GS->TimeRemaining <= 30.f);
	if (bUrgent && bInRaid && !bLastLightSaid && GS && GS->TimeRemaining > 0.f)
	{
		bLastLightSaid = true;
		PlaySting(EAntSting::LastLight);
	}
	LayerTarget[0] = 1.f;
	LayerTarget[1] = bInRaid ? FMath::Max(Smooth01(0.2f, 0.55f, FMath::Max(Tense, Act)), bUrgent ? 0.7f : 0.f) : 0.f;
	LayerTarget[2] = bInRaid ? Smooth01(0.3f, 0.65f, Act) : 0.f;
	LayerTarget[3] = bInRaid && bUrgent ? 1.f : 0.f;
}

void UAntAudioSubsystem::TickMusic(float Dt)
{
	UWorld* W = GetWorld();
	const float Now = (float)W->GetRealTimeSeconds();
	Excitement = FMath::Max(0.f, Excitement - 0.07f * Dt);
	SetTime += Dt;
	// the intensity follows the state: up fast (a chase starts now), down slow (the dust settles)
	Intensity = Approach(Intensity, ThreatSmoothed, 1.2f, 0.12f, Dt);
	for (int32 L = 0; L < 4; ++L)
	{
		LayerGain[L] = Approach(LayerGain[L], LayerTarget[L], L == 2 ? 0.9f : 0.5f, L == 2 ? 0.12f : 0.2f, Dt);
	}
	DuckLevel = Approach(DuckLevel, Now < DuckUntil ? 0.45f : 1.f, 4.f, 0.8f, Dt);
	const float MusicVol = ClassVolume(AntMixClass::Music) * DuckLevel;
	for (int32 S = 0; S < 3; ++S)
	{
		FMusicSet& Set = Sets[S];
		const bool bOn = S == ActiveSet;
		Set.Gain = Approach(Set.Gain, bOn ? 1.f : 0.f, 0.25f, 0.2f, Dt);
		const bool bPlaying = Set.Layers.Num() > 0 && Set.Layers[0] && Set.Layers[0]->IsPlaying();
		if (bOn && !bPlaying && MusicVol > 0.f)
		{
			StartSet(S);
		}
		else if (!bOn && bPlaying && Set.Gain <= 0.001f)
		{
			for (UAudioComponent* C : Set.Layers)
			{
				if (C)
				{
					C->Stop();
				}
			}
		}
		for (int32 L = 0; L < Set.Layers.Num(); ++L)
		{
			if (UAudioComponent* C = Set.Layers[L])
			{
				const float G = S == 2 ? 0.85f : LayerGain[L];
				C->SetVolumeMultiplier(FMath::Max(0.0001f, Set.Gain * G * MusicVol));
			}
		}
	}
}

// ---------------------------------------------------------------------------- ambience: room tone by time of day, the fridge, the tap, the clock, the swarm
void UAntAudioSubsystem::StartAmbience()
{
	UWorld* W = GetWorld();
	bAmbienceStarted = true;
	for (TActorIterator<AKitchenDoor> It(W); It; ++It)
	{
		if (It->Kind == EKitchenDoorKind::Fridge)
		{
			const FVector At = It->GetActorLocation();
			FridgeHum = Loop(EAntSfx::AmbFridge, &At, 0.01f);
			break;
		}
	}
	const FVector Spout = AKitchenFaucet::Spout() - FVector(0.f, 0.f, 600.f);
	TapDrip = Loop(EAntSfx::AmbDrip, &Spout, 0.01f);
	ClockTick = Loop(EAntSfx::AmbClock, nullptr, 0.01f);
}

void UAntAudioSubsystem::TickAmbience(float Dt)
{
	if (!bAmbienceStarted)
	{
		return;
	}
	UWorld* W = GetWorld();
	const float Amb = ClassVolume(AntMixClass::Ambience);
	AAntRaid* Raid = AAntRaid::Get(W);
	const bool bNest = Raid && Raid->Phase != AntRaidPhase::Raid;
	const int32 Tod = LevelV3::Enabled() ? FMath::Clamp(LevelV3::TimeOfDay(), 0, 2) : 0;
	if (Tod != RoomToneTod)
	{
		if (RoomTone)
		{
			RoomTone->FadeOut(3.f, 0.f);
		}
		RoomTone = Loop(Tod == 0 ? EAntSfx::AmbDay : Tod == 1 ? EAntSfx::AmbEvening : EAntSfx::AmbNight, nullptr, 0.01f);
		if (RoomTone)
		{
			RoomTone->FadeIn(3.f, 1.f);
		}
		RoomToneTod = Tod;
	}
	// inside the wall the house is far away
	const float Room = (bNest ? 0.35f : 1.f) * Amb;
	if (RoomTone)
	{
		RoomTone->SetVolumeMultiplier(FMath::Max(0.0001f, Room * Table[(int32)EAntSfx::AmbDay + RoomToneTod].Vol));
	}
	if (FridgeHum)
	{
		FridgeHum->SetVolumeMultiplier(FMath::Max(0.0001f, Table[(int32)EAntSfx::AmbFridge].Vol * Amb * (bNest ? 0.3f : 1.f) * (AntReact::HasPower(this) ? 1.f : 0.f)));   // the breaker tripped: the fridge falls silent (AntReact.h)
	}
	if (ClockTick)
	{
		ClockTick->SetVolumeMultiplier(FMath::Max(0.0001f, Table[(int32)EAntSfx::AmbClock].Vol * Amb * (Tod == 0 ? 0.35f : 1.f) * (bNest ? 0.3f : 1.f)));
	}
	if (TapDrip)
	{
		bool bFlowing = false;
		for (TActorIterator<AKitchenFaucet> It(W); It; ++It)
		{
			bFlowing |= It->IsOn();
		}
		TapDrip->SetVolumeMultiplier(FMath::Max(0.0001f, bFlowing ? 0.0001f : Table[(int32)EAntSfx::AmbDrip].Vol * Amb));
	}
	// the worker swarm: one loop, wherever the workers are, as loud as there are many
	const float Now = (float)W->GetRealTimeSeconds();
	if (Now - PatterSeen > 1.5f)
	{
		PatterLevel = FMath::Max(0.f, PatterLevel - Dt * 0.8f);
	}
	if (PatterLevel > 0.01f)
	{
		if (!Patter)
		{
			Patter = Loop(EAntSfx::WorkerPatter, &PatterAt, 0.01f);
		}
		if (Patter)
		{
			Patter->SetWorldLocation(PatterAt);
			Patter->SetVolumeMultiplier(FMath::Max(0.0001f, PatterLevel * Table[(int32)EAntSfx::WorkerPatter].Vol * ClassVolume(AntMixClass::Sfx)));
			Patter->SetPitchMultiplier(1.f + 0.1f * PatterLevel);
		}
	}
	else if (Patter)
	{
		Patter->SetVolumeMultiplier(0.0001f);
	}
}

// ---------------------------------------------------------------------------- the flat as the listener hears it: fasteners, fixtures, the residents' hands and falls, carrying
void UAntAudioSubsystem::TickWorldState(float Dt)
{
	UWorld* W = GetWorld();
	FVector Ear;
	if (!ListenerAt(W, Ear))
	{
		return;
	}
	const float Now = (float)W->GetRealTimeSeconds();
	if (++PruneCount >= 100)
	{
		// actors come and go (a new raid respawns the fixtures, ants die): forget the stale ones
		PruneCount = 0;
		auto Prune = [](auto& Map) { for (auto It = Map.CreateIterator(); It; ++It) { if (!It.Key().IsValid()) { It.RemoveCurrent(); } } };
		Prune(FastenerProgress); Prune(FastenerTick); Prune(FastenerOff); Prune(FixtureState); Prune(NextVoice);
		Prune(BackAche); Prune(HeldSomething); Prune(PetState); Prune(TrailOn);
		Prune(CarryBark); Prune(CarryNextBeat); Prune(HumanActStart);
		if (TrailLaying.Num() > 200) { TrailLaying.Reset(); }
	}
	if (LevelV3::Enabled())
	{
		const TConstArrayView<FLevelV3FastenerDef> Defs = LevelV3::Fasteners();
		for (TActorIterator<AKitchenFastener> It(W); It; ++It)
		{
			AKitchenFastener* F = *It;
			const int32 Kind = Defs.IsValidIndex(F->Def) ? Defs[F->Def].Kind : 0;
			const FVector At = F->GetActorLocation();
			float& Prev = FastenerProgress.FindOrAdd(F, F->Progress);
			float& Off = FastenerOff.FindOrAdd(F, F->OffAt);
			// being worked: ticks of the screw turning, the hinge pin squeaking, the tape peeling
			if (F->Progress > Prev + 0.0005f && FVector::DistSquared(At, Ear) < FMath::Square(4500.f))
			{
				float& Next = FastenerTick.FindOrAdd(F, 0.f);
				if (Now >= Next)
				{
					const EAntSfx Id = Kind == 1 ? EAntSfx::HingeCreak : Kind == 4 ? EAntSfx::RipOff : (Kind == 5 || Kind == 6) ? EAntSfx::CurtainRustle : EAntSfx::ScrewTurn;
					AntAudio::Play(this, Id, At, 0.6f + 0.4f * F->Progress);
					Next = Now + (Id == EAntSfx::ScrewTurn ? 0.35f : 1.2f);
				}
			}
			Prev = F->Progress;
			// it came off
			if (F->OffAt >= 0.f && Off < 0.f)
			{
				const EAntSfx Id = (Kind == 0 || Kind == 2 || Kind == 7 || Kind == 8) ? EAntSfx::ScrewDrop : Kind == 1 ? EAntSfx::ScrewDrop
					: Kind == 4 ? EAntSfx::RipOff : (Kind == 5 || Kind == 6) ? EAntSfx::CurtainRustle : EAntSfx::Release;
				AntAudio::Play(this, Id, At, 1.f);
				if (Kind == 1)
				{
					AntAudio::Play(this, EAntSfx::HingeCreak, At, 1.f);
				}
			}
			Off = F->OffAt;
		}
		for (TActorIterator<AKitchenFixture> It(W); It; ++It)
		{
			AKitchenFixture* X = *It;
			uint8* Seen = FixtureState.Find(X);
			if (!Seen)
			{
				FixtureState.Add(X, X->State);   // first look: whatever it is now is no news
				continue;
			}
			if (*Seen != X->State)
			{
				const FVector At = X->State == AKitchenFixture::Broken ? FVector(X->BreakAt) : X->GetActorLocation();
				const int32 Mat = X->Mat();
				switch (X->State)
				{
				case AKitchenFixture::Hanging:
					AntAudio::Play(this, EAntSfx::Wobble, At, 1.f);
					AntAudio::Play(this, EAntSfx::HingeCreak, At, 0.8f);
					break;
				case AKitchenFixture::Falling:
					AntAudio::Play(this, EAntSfx::Wobble, At, 0.7f);
					break;
				case AKitchenFixture::Lying:
					AntAudio::Play(this, AntAudio::CrashFor(Mat), At, 1.f);
					AntAudio::Excite(this, 0.3f, At);
					break;
				case AKitchenFixture::Broken:
					AntAudio::Play(this, AntAudio::CrashFor(Mat), At, 1.4f);
					if (Mat == 12)
					{
						AntAudio::Play(this, EAntSfx::SplatCream, At, 1.f);
					}
					AntAudio::Excite(this, 0.45f, At);
					break;
				case AKitchenFixture::Open:
					AntAudio::Play(this, EAntSfx::HingeCreak, At, 1.f);
					break;
				default:
					break;
				}
				*Seen = X->State;
			}
		}
	}
	// the residents: a fall (the back ache starts), something picked up
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		AHumanCharacter* H = *It;
		float& Ache = BackAche.FindOrAdd(H, H->BackAcheUntil);
		if (H->BackAcheUntil > Ache + 1.f)
		{
			AntAudio::Play(this, EAntSfx::HumanSlip, H->GetActorLocation(), 1.f);
			AntAudio::Excite(this, 0.5f, H->GetActorLocation());
		}
		Ache = H->BackAcheUntil;
		bool& bHeld = HeldSomething.FindOrAdd(H, H->Held != nullptr);
		if (!bHeld && H->Held)
		{
			AntAudio::Play(this, EAntSfx::HumanPickup, H->GetActorLocation() + FVector(0.f, 0.f, 1500.f), 1.f);
		}
		bHeld = H->Held != nullptr;
	}
	// pheromone trails: a shimmer when an ant near here starts laying one, a snip when it stops
	for (TActorIterator<AAntCharacter> It(W); It; ++It)
	{
		AAntCharacter* A = *It;
		bool& bWas = TrailOn.FindOrAdd(A, A->bLayingTrail);
		if (A->bLayingTrail && !bWas && FVector::DistSquared(A->GetActorLocation(), Ear) < FMath::Square(4000.f))
		{
			AntAudio::Play(this, EAntSfx::TrailLay, A->GetActorLocation(), 0.8f);
		}
		bWas = A->bLayingTrail;
	}
	// carrying together, trails and workers, the snowball's cards, the Human's swings
	TickV12(Dt, Ear);
}

// ---------------------------------------------------------------------------- pets and people: idle voices from their replicated state
void UAntAudioSubsystem::TickPets(float Dt)
{
	UWorld* W = GetWorld();
	FVector Ear;
	if (!ListenerAt(W, Ear))
	{
		return;
	}
	const float Now = (float)W->GetRealTimeSeconds();
	bool bPurr = false;
	FVector PurrAt = FVector::ZeroVector;
	for (TActorIterator<ACritter> It(W); It; ++It)
	{
		ACritter* C = *It;
		const FVector At = C->GetActorLocation() + FVector(0.f, 0.f, C->BodyHeight() * 0.6f);
		const float D = (float)FVector::Dist(At, Ear);
		uint8& Prev = PetState.FindOrAdd(C, C->State);
		float& Next = NextVoice.FindOrAdd(C, Now + FMath::FRandRange(2.f, 6.f));
		const bool bCat = C->Kind == 1;
		if (C->State != Prev)
		{
			if (C->State == 4)
			{
				AntAudio::Play(this, bCat ? EAntSfx::CatPounce : EAntSfx::DogBark, At, 1.f);
			}
			else if (C->State == 6)
			{
				AntAudio::Play(this, bCat ? EAntSfx::CatHiss : EAntSfx::DogBark, At, 1.f);
			}
			else if (C->State == 5 && !bCat)
			{
				AntAudio::Play(this, EAntSfx::DogSnort, At, 0.8f);
			}
			Prev = C->State;
		}
		if (bCat && (C->State == 0 || C->State == 3) && D < 5000.f)
		{
			bPurr = true;
			PurrAt = At;
		}
		if (Now >= Next && D < 9000.f)
		{
			if (!bCat && C->State == 2)
			{
				AntAudio::Play(this, EAntSfx::DogSniff, At, 1.f);
				Next = Now + FMath::FRandRange(1.5f, 3.5f);
			}
			else if (!bCat && C->State == 1)
			{
				AntAudio::Play(this, EAntSfx::DogSnort, At, 0.8f);
				Next = Now + FMath::FRandRange(5.f, 11.f);
			}
			else
			{
				Next = Now + FMath::FRandRange(3.f, 7.f);
			}
		}
	}
	if (bPurr)
	{
		if (!Purr)
		{
			Purr = Loop(EAntSfx::CatPurr, &PurrAt, 0.01f);
		}
		if (Purr)
		{
			Purr->SetWorldLocation(PurrAt);
			Purr->SetVolumeMultiplier(Table[(int32)EAntSfx::CatPurr].Vol * ClassVolume(AntMixClass::Voice));
		}
	}
	else if (Purr)
	{
		Purr->SetVolumeMultiplier(0.0001f);
	}
	// the kid giggles about the place; the Human grumbles when angry and sniffs about when he is looking for the ants
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		AHumanCharacter* H = *It;
		if (H->IsHidden())
		{
			continue;
		}
		const FVector At = H->GetActorLocation() + FVector(0.f, 0.f, 1200.f);
		const float D = (float)FVector::Dist(At, Ear);
		float& Next = NextVoice.FindOrAdd(H, Now + FMath::FRandRange(4.f, 10.f));
		if (Now < Next || D > 14000.f)
		{
			continue;
		}
		if (H->IsA<AKidCharacter>())
		{
			AntAudio::Play(this, H->GetVelocity().Size2D() > 300.f ? EAntSfx::KidShout : EAntSfx::KidGiggle, At, 1.f);
			Next = Now + FMath::FRandRange(7.f, 16.f);
		}
		else if (H->IsA<AGrandmaCharacter>())
		{
			if (H->GetVelocity().Size2D() > 60.f && FMath::FRand() < 0.4f)
			{
				AntAudio::Play(this, EAntSfx::Broom, H->GetActorLocation() + FVector(0.f, 0.f, 200.f), 0.7f);
			}
			Next = Now + FMath::FRandRange(6.f, 14.f);
		}
		else if (!H->IsA<AHouseFolk>())
		{
			if (H->Rage >= 1)
			{
				AntAudio::Play(this, EAntSfx::HumanGrumble, At, 0.7f + 0.15f * H->Rage);
			}
			else if (H->GetVelocity().Size2D() < 120.f && FMath::FRand() < 0.5f)
			{
				AntAudio::Play(this, EAntSfx::HumanSniff, At, 0.8f);
			}
			Next = Now + FMath::FRandRange(9.f, 20.f);
		}
	}
}

// ---------------------------------------------------------------------------- v12: carrying together, trails and workers, the snowball's cards, the Human's swings
void UAntAudioSubsystem::TickV12(float Dt, const FVector& Ear)
{
	UWorld* W = GetWorld();
	const float Now = (float)W->GetRealTimeSeconds();
	// carrying together (AntCarry.h, replicated CarryNet): a shout from a carrier («Раз-два, взяли!» and the rest) is a small
	// chorus with the music dipping under it; in rhythm every stride is a thump of the load; out of step it creaks
	for (TActorIterator<AKitchenLoot> It(W); It; ++It)
	{
		AKitchenLoot* L = *It;
		const FAntCarryNet& Net = L->CarryNet;
		if (L->bIsGear || !Net.bActive || Net.Grips.Num() == 0)
		{
			continue;
		}
		const FVector At = L->GetActorLocation();
		const bool bNear = FVector::DistSquared(At, Ear) < FMath::Square(5000.f);
		uint8& Seq = CarryBark.FindOrAdd(L, Net.BarkSeq);
		if (Seq != Net.BarkSeq)
		{
			Seq = Net.BarkSeq;
			if (bNear)
			{
				AntAudio::Play(this, EAntSfx::ArmyCharge, At, 0.45f);
				AntAudio::Duck(this, 0.9f);
			}
		}
		float& Beat = CarryNextBeat.FindOrAdd(L, Now + 0.5f);
		if (bNear && Now >= Beat && Net.Grips.Num() >= 2)
		{
			if (Net.bRhythm)
			{
				AntAudio::CarryBeat(this, At, Net.Grips.Num());
				Beat = Now + 0.62f;
			}
			else
			{
				AntAudio::Play(this, EAntSfx::CarryStrain, At, FMath::Clamp(0.5f + 0.1f * Net.Grips.Num(), 0.5f, 1.2f) * (Net.Sync < 140 ? 1.2f : 0.8f));
				Beat = Now + FMath::FRandRange(1.3f, 2.f);
			}
		}
	}
	// the Human's body: the swing of a weapon whooshes, peering under the furniture he sniffs
	for (TActorIterator<AHumanCharacter> It(W); It; ++It)
	{
		AHumanCharacter* H = *It;
		float& Start = HumanActStart.FindOrAdd(H, H->Act.Start);
		if (H->Act.Start != Start)
		{
			Start = H->Act.Start;
			const FVector At = H->GetActorLocation() + FVector(0.f, 0.f, 1200.f);
			if (H->Act.Kind == HumanAct::Swat)
			{
				if (const AntWeapons::FDef* D = AntWeapons::Held(H))
				{
					AntAudio::Play(this, EAntSfx::BatWhoosh, At, 0.6f + 0.25f * D->Tier);
				}
			}
			else if (H->Act.Kind == HumanAct::Peer)
			{
				AntAudio::Play(this, EAntSfx::HumanSniff, At - FVector(0.f, 0.f, 700.f), 1.f);
			}
		}
	}
	// trails: a shimmer when one starts being laid near here; the workers on trails and swarm streams patter (one loop)
	SwarmClock += Dt;
	const bool bSwarmScan = SwarmClock >= 0.5f;
	if (bSwarmScan)
	{
		SwarmClock = 0.f;
	}
	float BestD2 = FMath::Square(4000.f);
	FVector BestAt = FVector::ZeroVector;
	int32 BestCount = 0;
	if (AAntTrailNet* T = AAntTrailNet::Get(W, false))
	{
		for (const FAntTrailLine& Ln : T->Lines)
		{
			bool& bWas = TrailLaying.FindOrAdd(Ln.Id, Ln.bLaying);
			if (Ln.bLaying && !bWas && Ln.Points.Num() > 0)
			{
				AntAudio::Play(this, EAntSfx::TrailLay, FVector(Ln.Points.Last()), 0.9f);
			}
			bWas = Ln.bLaying;
			if (bSwarmScan && Ln.Workers > 0)
			{
				for (int32 i = 0; i < Ln.Points.Num(); i += 4)
				{
					const float D2 = (float)FVector::DistSquared(FVector(Ln.Points[i]), Ear);
					if (D2 < BestD2) { BestD2 = D2; BestAt = Ln.Points[i]; BestCount = Ln.Workers; }
				}
			}
		}
	}
	if (bSwarmScan)
	{
		if (AAntSwarm* Sw = AAntSwarm::Get(W))
		{
			for (const FSwarmStream& St : Sw->Streams)
			{
				if (St.Count <= 0) { continue; }
				for (int32 i = 0; i < St.Points.Num(); i += 4)
				{
					const float D2 = (float)FVector::DistSquared(FVector(St.Points[i]), Ear);
					if (D2 < BestD2) { BestD2 = D2; BestAt = St.Points[i]; BestCount = St.Count; }
				}
			}
		}
		if (BestCount > 0)
		{
			SwarmAt(BestAt, BestCount);
		}
	}
	// the snowball (AntSnowball.h): my cards are dealt, I picked one, the colony grew a level
	APlayerController* PC = W->GetFirstPlayerController();
	AAntSnowball* S = AAntSnowball::Get(W);
	if (S && PC && PC->PlayerState)
	{
		for (const FAntSnowPlayer& P : S->Players)
		{
			if (P.PS != PC->PlayerState)
			{
				continue;
			}
			if (P.Offer.Num() > 0 && LastOfferNum == 0)
			{
				AntAudio::Play2D(this, EAntSfx::CardFlip, 1.f);
			}
			LastOfferNum = P.Offer.Num();
			if (LastPicks >= 0 && P.PicksDone > LastPicks)
			{
				AntAudio::Play2D(this, EAntSfx::CardPick, 1.f);
				PlaySting(EAntSting::Card);
			}
			LastPicks = P.PicksDone;
		}
		if (LastSnowLevel >= 0 && S->Level > LastSnowLevel)
		{
			AntAudio::Play2D(this, EAntSfx::Legendary, 0.7f);
		}
		LastSnowLevel = S->Level;
	}
}

void UAntAudioSubsystem::OnCarryEvent(AKitchenLoot* L, uint8 Kind, uint8 Team)
{
	if (!L)
	{
		return;
	}
	const FVector At = L->GetActorLocation();
	switch (Kind)
	{
	case AntCarryEvent::Dropped: AntAudio::Everyone(this, EAntSfx::Release, At, 1.f); break;
	case AntCarryEvent::Broken: AntAudio::Everyone(this, EAntSfx::SplatJam, At, 1.2f); break;
	case AntCarryEvent::Snatched: AntAudio::Everyone(this, EAntSfx::HumanPickup, At, 1.f); break;
	case AntCarryEvent::Joined: AntAudio::Everyone(this, EAntSfx::Grab, At, 0.6f); break;
	default: break;
	}
}
