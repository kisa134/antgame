#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "AntTypes.h"
#include "AntFighter.generated.h"

class AAntCharacter;
class AAntPlayerState;
class AAntPlayerController;
class APlayerState;
class AController;
class AKitchenLoot;
class AAntRaid;
class AAntKitchenGameMode;
class AHUD;
class UCanvas;
struct FAntRaidSummary;

/**
 * The fighter and his squad (docs/FIGHTER.md).
 *
 * PERSONAL (every player, his own machine): a profile saved locally (Saved/Fighter/profile_<id>.json, keyed by the Steam id;
 * "local" offline). XP from what he did in raids, a parody rank ladder, sidegrade perks of his caste (choose 1 of 2 per tier,
 * swappable in the nest), medals for funny feats, cosmetics unlocked by ranks and medals. Nothing here is raw power: a veteran
 * and a newcomer can carry the same cake. The profile goes to the server as FAntLoadout (replicated on the player state), so
 * every machine draws his helmet and insignia and the server applies his perks.
 *
 * SQUAD VETERANS (the host's colony, Saved/Nest/veterans.json): squad ants that survive a raid get a name, a rank and quirks;
 * players draft them before a raid; death is permanent and goes on the memorial wall. AAntFighterState replicates the roster,
 * the fallen and the post-raid highlights.
 *
 * Meta bus (AntMetaBus.h): the server listens to OnRaidSummary (XP, medals, promotions) and provides GetFallenVeterans /
 * GetPlayerHighlights on every machine.
 */

/** The rank ladder of the Great Colony of the Crack (a parody: no real army's ranks or insignia). */
namespace AntRank
{
	constexpr int32 Count = 8;
	ANTKITCHEN_API const TCHAR* Name(int32 Rank);
	/** Lower case, for a line ("ефрейтор Капелька"). */
	ANTKITCHEN_API const TCHAR* Lower(int32 Rank);
	ANTKITCHEN_API int32 XpFor(int32 Rank);
	ANTKITCHEN_API int32 Of(int32 Xp);
	/** What the insignia is made of ("две сахарные крупинки"). */
	ANTKITCHEN_API const TCHAR* Insignia(int32 Rank);
}

/** Sidegrade perks: 5 castes x 3 tiers x 2 options; 2 bits per tier (0 none, 1 A, 2 B). */
namespace AntPerk
{
	constexpr int32 Castes = 5;
	constexpr int32 Tiers = 3;
	struct FDef { const TCHAR* Name; const TCHAR* Text; };
	ANTKITCHEN_API const FDef& Def(int32 Caste, int32 Tier, int32 Opt);
	/** The rank a tier opens at. */
	ANTKITCHEN_API int32 NeedRank(int32 Tier);
	ANTKITCHEN_API int32 Get(uint32 Bits, int32 Caste, int32 Tier);
	ANTKITCHEN_API uint32 Set(uint32 Bits, int32 Caste, int32 Tier, int32 Choice);
}

/** What a perk, a veteran's quirk or a doctrine multiplies (AntFighter::Mul). */
namespace AntStat
{
	enum EId : uint8 { Strength, Speed, CarrySpeed, Health, DamageTaken, AcidCost, DashCd, Jump, HealRate, HealRadius, InsideTime, HeldWork, ClothWork, ClimbSpeed, WaterSpeed, Count };
}

/** Medals / feats. Tracked by the server from real events; the profile counts them. */
namespace AntFeat
{
	enum EId : uint8
	{
		Slipper, Sneezed, Pickpocket, Drowned, RunOver, Tablecloth, Ear, Knitting, CakeFlawless, Plumber,
		CakeHome, LastOut, Trophy, Special, Contract, Rescuer, Undertaker, AcidRain, SpiderSlayer, Ram,
		Legless, Rider, Pancake, Ripper, Doorman, Screwdriver, FirstRaid, TenRaids, SweetTooth, Licked,
		CatPaw, Distraction, Resurrected, SlipperVet, Survivor, Medic,
		Count
	};
	struct FDef { const TCHAR* Key; const TCHAR* Name; const TCHAR* Text; const TCHAR* Icon; uint32 Color; int32 Xp; };
	ANTKITCHEN_API const FDef& Def(int32 Id);
	/** A feat id from its key (also the colony's feat names: "cake_home" ...), or -1. */
	ANTKITCHEN_API int32 FromKey(const FString& Key);
}

/** Cosmetics: slots and items, each unlocked by a rank or a medal. */
namespace AntCosm
{
	enum ESlot : uint8 { Head, Face, Neck, Weapon, Back, Paint, Shell, Pattern, Trail, Emote, SlotCount };
	struct FItem { const TCHAR* Name; const TCHAR* Key; int32 NeedRank; int32 NeedFeat; uint32 Color; };
	ANTKITCHEN_API const TCHAR* SlotName(int32 Slot);
	ANTKITCHEN_API int32 NumItems(int32 Slot);
	ANTKITCHEN_API const FItem& Item(int32 Slot, int32 Index);
	/** Is it unlocked for this rank and these medals? */
	ANTKITCHEN_API bool Unlocked(int32 Slot, int32 Index, int32 Rank, uint64 Medals);
	ANTKITCHEN_API FString UnlockText(int32 Slot, int32 Index);
}

/** Squad veterans' quirks (bits of FAntVeteran::Traits) and doctrines. */
namespace AntTrait
{
	enum EId : uint8 { FearsWater, Double, Coward, SlipperVet, IronJaws, Lucky, Count };
	ANTKITCHEN_API const TCHAR* Name(int32 Id);
	ANTKITCHEN_API const TCHAR* Text(int32 Id);
}
namespace AntDoctrine
{
	enum EId : uint8 { None, Wedge, Loose, Turtle, Count };
	ANTKITCHEN_API const TCHAR* Name(int32 Id);
	ANTKITCHEN_API const TCHAR* Text(int32 Id);
	/** The veteran rank a doctrine needs among the drafted. */
	ANTKITCHEN_API int32 NeedRank(int32 Id);
}

/** What a player shows and plays with: replicated on his player state, sent by his machine from the profile. */
USTRUCT()
struct FAntLoadout
{
	GENERATED_BODY()
	UPROPERTY() int32 Xp = 0;
	UPROPERTY() uint8 Rank = 0;
	UPROPERTY() uint32 Perks = 0;
	UPROPERTY() uint64 Medals = 0;
	UPROPERTY() TArray<uint8> Cosm;
	UPROPERTY() bool bInsignia = true;
	UPROPERTY() int32 Raids = 0;
	UPROPERTY() FString ProfileId;
	/** Bumped on every change (the dress component re-dresses the ant). */
	UPROPERTY() uint8 Stamp = 0;
	uint8 CosmAt(int32 Slot) const { return Cosm.IsValidIndex(Slot) ? Cosm[Slot] : 0; }
};

/** The personal profile on disk (JSON, versioned). */
USTRUCT()
struct FAntProfile
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FString Id;
	UPROPERTY() FString Name;
	UPROPERTY() int32 Xp = 0;
	UPROPERTY() int32 Raids = 0;
	UPROPERTY() uint32 Perks = 0;
	UPROPERTY() TArray<int32> FeatCount;
	UPROPERTY() TArray<uint8> Cosm;
	UPROPERTY() bool bInsignia = true;
	UPROPERTY() int32 TotalFood = 0;
	UPROPERTY() int32 TotalRescues = 0;
	UPROPERTY() int32 TotalKills = 0;
	UPROPERTY() int32 CakesHome = 0;
	/** The last raid, as the post-raid lines ("+120 опыта: донёс 60 еды"). */
	UPROPERTY() TArray<FString> LastRaid;
	UPROPERTY() int32 LastRaidXp = 0;
	UPROPERTY() int32 LastRankBefore = 0;
	uint64 MedalBits() const;
	int32 Rank() const { return AntRank::Of(Xp); }
};

/** A squad veteran of the host's colony. */
USTRUCT()
struct FAntVeteran
{
	GENERATED_BODY()
	UPROPERTY() int32 Id = 0;
	UPROPERTY() FString Name;
	UPROPERTY() uint8 Rank = 0;
	UPROPERTY() int32 Xp = 0;
	UPROPERTY() int32 Raids = 0;
	UPROPERTY() uint8 Traits = 0;
	/** 0 forager, 1 soldier. */
	UPROPERTY() uint8 Caste = 0;
	UPROPERTY() int32 Kills = 0;
	UPROPERTY() int32 Carried = 0;
	UPROPERTY() int32 Joined = 0;
	/** The profile id of the player who drafted him for the next raid (empty: in the barracks). */
	UPROPERTY() FString DraftedBy;
	/** Out in the raid right now (server). */
	UPROPERTY() bool bOut = false;
	bool Has(int32 Trait) const { return (Traits & (1 << Trait)) != 0; }
};

USTRUCT()
struct FAntFallen
{
	GENERATED_BODY()
	UPROPERTY() FString Name;
	UPROPERTY() uint8 Rank = 0;
	UPROPERTY() int32 Raid = 0;
	UPROPERTY() int32 Raids = 0;
	UPROPERTY() FString Cause;
};

/** The roster save (host). */
USTRUCT()
struct FAntRosterSave
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 NextId = 1;
	UPROPERTY() TArray<FAntVeteran> Roster;
	UPROPERTY() TArray<FAntFallen> Fallen;
	UPROPERTY() TArray<FString> UsedNames;
};

USTRUCT()
struct FAntHighlight
{
	GENERATED_BODY()
	UPROPERTY() FString Player;
	UPROPERTY() FString PlayerId;
	UPROPERTY() FString Title;
	UPROPERTY() FString Line;
	UPROPERTY() FString Icon;
	UPROPERTY() int32 RankBefore = 0;
	UPROPERTY() int32 RankAfter = 0;
	/** 0 rank, 1 medal, 2 feat / xp, 3 veteran promoted, 4 veteran fallen, 5 new veteran. */
	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() int32 Xp = 0;
};

USTRUCT()
struct FAntDoctrinePick
{
	GENERATED_BODY()
	UPROPERTY() FString PlayerId;
	UPROPERTY() uint8 Doctrine = 0;
};

/** What a player did this raid (server only). */
struct FAntFighterTally
{
	float Food = 0.f;
	float CakeSecs = 0.f;
	int32 Rescues = 0;
	float Healed = 0.f;
	int32 Distract = 0;
	int32 Fasteners = 0;
	int32 Inside = 0;
	int32 KillsRed = 0;
	int32 KillsSpider = 0;
	int32 Stomps = 0;
	int32 Deaths = 0;
	TArray<uint8> Feats;
	/** Feats that were not yet medals of his (first time). */
	TArray<uint8> NewMedals;
	bool bRescued = false;
	bool CakeHome = false;
};

/**
 * The replicated half (one per world, made by the server on first use): the veterans' roster with who drafted whom,
 * the fallen, the doctrines picked, the highlights of the last raid.
 */
UCLASS()
class ANTKITCHEN_API AAntFighterState : public AInfo
{
	GENERATED_BODY()

public:
	AAntFighterState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AAntFighterState* Get(const UWorld* World, bool bCreate = false);

	UPROPERTY(Replicated) TArray<FAntVeteran> Roster;
	UPROPERTY(Replicated) TArray<FAntFallen> Fallen;
	UPROPERTY(Replicated) TArray<FAntHighlight> Highlights;
	UPROPERTY(Replicated) TArray<FAntDoctrinePick> Doctrines;
	UPROPERTY(Replicated) int32 HighlightsRaid = 0;
	/** How many veterans one player may draft (from the colony's barracks / brood). */
	UPROPERTY(Replicated) uint8 DraftCap = 2;

	// server
	int32 NextId = 1;
	TArray<FString> UsedNames;
	bool bLoaded = false;
	TMap<TWeakObjectPtr<APlayerState>, FAntFighterTally> Tally;
	/** Squad ants alive when the raid ended: veteran ids (-1: a recruit), by owner. */
	TArray<TPair<TWeakObjectPtr<APlayerState>, int32>> Survivors;
	/** Veterans who fell this raid (for the highlights), with the profile id of their commander. */
	TArray<TPair<FString, FAntFallen>> FallenThisRaid;
	/** Lines about veterans for the highlights (AntVets::AfterRaid fills them). */
	TArray<FAntHighlight> VetHighlights;

	FAntVeteran* FindVet(int32 Id);
	uint8 DoctrineOf(const FString& PlayerId) const;
};

namespace AntFighter
{
	// ---- every machine
	/** Multiplier of a stat for this ant: its player's perks (his caste), a veteran's quirks and his squad's doctrine. */
	ANTKITCHEN_API float Mul(const AAntCharacter* Ant, uint8 Stat);
	/** Speed x carrying / climbing / wet multipliers, for UpdateSpeed. */
	ANTKITCHEN_API float SpeedMul(const AAntCharacter* Ant);
	/** How fast this ant works a held job (a fastener: HeldWork; cloth, ribbons, zips: ClothWork). The Sapper caste is x1.5. */
	ANTKITCHEN_API float HeldMul(const AAntCharacter* By, const AActor* Held);
	/** Does this ant's player have that perk chosen (for his current caste)? */
	ANTKITCHEN_API bool HasPerk(const AAntCharacter* Ant, int32 Tier, int32 Opt);
	ANTKITCHEN_API const FAntLoadout* LoadoutOf(const AAntCharacter* Ant);
	/** The profile id of a player state (unique net id, or the name). */
	ANTKITCHEN_API FString IdOf(const APlayerState* PS);

	// ---- the local profile (this machine's player)
	ANTKITCHEN_API FAntProfile& Profile();
	ANTKITCHEN_API void SaveProfile();
	/** Profile -> loadout -> server. */
	ANTKITCHEN_API void PushLoadout(AAntPlayerController* PC);
	ANTKITCHEN_API FAntLoadout MakeLoadout();
	/** Called every frame by the local HUD: loads the profile, sends the loadout, plays the medal toasts. */
	ANTKITCHEN_API void LocalTick(AAntPlayerController* PC);
	/** The server's word to this machine (AAntPlayerController::ClientFighter). */
	ANTKITCHEN_API void OnClient(AAntPlayerController* PC, uint8 Kind, int32 A, int32 B, const FString& Text);
	namespace ClientKind { enum : uint8 { Xp, Feat, RaidDone, SetXp, UnlockAll, Reset, Note }; }

	// ---- stable API for other systems (the tutorial, quests). Server only; safe to call any time, also outside a raid.
	/** Gives the player this rank (0 Рядовой .. 7 Герой Щели): his XP is raised to the rank's threshold (never lowered). */
	ANTKITCHEN_API void AwardRank(APlayerState* PS, int32 Rank);
	/** Awards a medal (AntFeat id): toast + profile on his machine, highlight. Once per raid per medal. */
	ANTKITCHEN_API void AwardMedal(APlayerState* PS, uint8 FeatId);
	/** The same by key: "slipper" = «Пережил Розовую Плиту», "sneezed", "cake_home", ... False: unknown key. */
	ANTKITCHEN_API bool AwardMedalKey(APlayerState* PS, const FString& Key);
	/** Opens on that player's machine the profile at the «Касты» tab (perk pick); Caste -1 = his current caste. */
	ANTKITCHEN_API void OpenPerkPick(APlayerState* PS, int32 Caste = -1);

	// ---- server hooks (each one line in the game's code)
	ANTKITCHEN_API void Feat(AAntCharacter* Ant, uint8 Id);
	ANTKITCHEN_API void FeatPS(APlayerState* PS, uint8 Id);
	ANTKITCHEN_API FAntFighterTally* TallyOf(const AAntCharacter* Ant);
	ANTKITCHEN_API void OnDelivered(AKitchenLoot* Food, const TArray<AAntCharacter*>& Carriers, int32 Value);
	ANTKITCHEN_API void OnBodyHome(AController* BodyOwner, const TArray<AAntCharacter*>& Carriers);
	ANTKITCHEN_API void OnAntKilled(AAntCharacter* Ant, AController* Killer, AActor* Causer);
	ANTKITCHEN_API void OnHealed(AAntCharacter* Nurse, AAntCharacter* Who, float Amount);
	ANTKITCHEN_API void OnFastener(AAntCharacter* By);
	ANTKITCHEN_API void OnFixtureWork(const FString& Id, AAntCharacter* By);
	ANTKITCHEN_API void OnInside(AAntCharacter* Ant, int32 Spot, uint8 What);
	ANTKITCHEN_API void OnStompNear(AAntCharacter* Ant, bool bHit);
	ANTKITCHEN_API void OnDistract(AAntCharacter* Ant);
	ANTKITCHEN_API void OnRunOver(AAntCharacter* Ant);
	/** The kitchen floor floods: everybody of ours alive gets «Водопроводчик» on the next raid second. */
	ANTKITCHEN_API void OnFlood();
	ANTKITCHEN_API void OnNpcHold(AAntCharacter* Ant, AActor* Holder);
	/** Server, 4x a second per ant (ApplyEnvironment): drowning and coming up again. */
	ANTKITCHEN_API void OnEnv(AAntCharacter* Ant, bool bDrowning);
	/** Server, once a second in a raid: who carries the cake. */
	ANTKITCHEN_API void RaidSecond(AAntRaid* Raid);
	ANTKITCHEN_API void OnRaidStart(AAntRaid* Raid);
	/** Server: the raid is ending (before its squad ants go). */
	ANTKITCHEN_API void OnRaidEnding(AAntRaid* Raid, uint8 How);
	/** Server: AntMetaBus's summary (the colony fires it; this branch fires a fallback from AAntRaid::BuildResult). */
	ANTKITCHEN_API void OnRaidSummary(const FAntRaidSummary& S);
	ANTKITCHEN_API void BroadcastFallbackSummary(AAntRaid* Raid, uint8 How);
	/** Startup: subscribe to the bus, set the providers (once per process). */
	ANTKITCHEN_API void Init();
	/** Damage dodge of a veteran (ветеран тапка, счастливчик): true = the blow is ignored. */
	ANTKITCHEN_API bool VetDodge(AAntCharacter* Ant, AActor* Causer, float Damage);
	/** A death cause in the setting's words ("раздавлен Розовой Плитой"). */
	ANTKITCHEN_API FString CauseOf(const AAntCharacter* Ant, AActor* Causer);
	/** Dev / admin (-AntDo): fighter ..., vet ... True if handled. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& Args);
	/** Server: a loadout came from a player. */
	ANTKITCHEN_API void ServerLoadout(AAntPlayerController* PC, const FAntLoadout& L);
}

/** Squad veterans (AntVeterans.cpp). Server unless said otherwise. */
namespace AntVets
{
	ANTKITCHEN_API void Load(AAntFighterState* FS);
	ANTKITCHEN_API void Save(AAntFighterState* FS);
	namespace Cmd { enum : uint8 { Draft, Undraft, Doctrine, AutoDraft }; }
	ANTKITCHEN_API void Command(AAntPlayerController* PC, uint8 Cmd, int32 A, int32 B);
	/** The raid starts: every player's drafted veterans join his squad. */
	ANTKITCHEN_API void SpawnDrafted(AAntRaid* Raid);
	/** A squad ant died: a veteran goes to the wall. */
	ANTKITCHEN_API void OnSquadAntDied(AAntCharacter* Ant, AActor* Causer);
	/** The summary: survivors are promoted, recruits become veterans, the lost are mourned. */
	ANTKITCHEN_API void AfterRaid(AAntFighterState* FS, int32 RaidNo, bool bWiped);
	ANTKITCHEN_API FString NewName(AAntFighterState* FS, FRandomStream& Rng);
	ANTKITCHEN_API int32 RankOfXp(int32 Xp);
	/** Every machine: veterans drafted by this player id. */
	ANTKITCHEN_API int32 NumDrafted(const AAntFighterState* FS, const FString& PlayerId);
	/** The best rank among this player's drafted veterans (-1 none). */
	ANTKITCHEN_API int32 BestDraftedRank(const AAntFighterState* FS, const FString& PlayerId);
	/** Server, 4 Hz per squad veteran: quirks that depend on the moment (water, the cat). */
	ANTKITCHEN_API void Tick(AAntCharacter* Ant, float Dt);
	ANTKITCHEN_API int32 DraftCapFor(const UWorld* World);
	/** Dev: vet list | add [n] [name] | kill <id|name> [cause] | revive [name] | draft <id> [who] | undraft <id> | doctrine N [who] | xp <id> N | clear | spawn */
	ANTKITCHEN_API bool Dev(AAntFighterState* FS, AAntKitchenGameMode* GM, const TArray<FString>& Args);
}
