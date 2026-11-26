#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "AntNest.h"
#include "AntDifficulty.generated.h"

class AAntRaid;
class AAntPlayerController;
class AAntCharacter;
class AAntKitchenGameMode;
class APlayerState;
class UCanvas;

/**
 * Difficulty presets (KAN-30; the table is Defs in AntDifficulty.cpp): Казуальный / Обычный / Хард. A separate axis from the threat tiers 1-4 (AntTier): the
 * tier says what the flat is like, the difficulty says how forgiving the rules are. The host picks it in the lobby or in the nest
 * (solo: the same buttons), it is replicated by one small always-relevant actor and saved on the host's machine
 * (GameUserSettings.ini [AntDifficulty] Level). Everything reads it through the functions below: one-line hooks in the old code.
 */
namespace AntDiff
{
	enum ELevel : uint8 { Casual, Normal, Hard, Count };

	struct FDef
	{
		const TCHAR* Name;       // "Казуальный"
		const TCHAR* Short;      // "КАЗУАЛ"
		const TCHAR* NameEn;
		const TCHAR* Text;
		float Notice;            // how far the Human notices ants, how loud noises are to the house
		float Damage;            // damage the Human, the family, the cat and the dog do to colony ants
		int32 NpcAdd;            // boldness of the pets and the family (0..2), added to the tier's
		int32 LarvaeAdd;         // shared lives lent for the raid (casual) or withheld (hard)
		float Time;              // raid length
		float Quiet;             // how long the Human stays on his phone at the start
		float Clan;              // red ants: head count
		float ClanHealth;
		float ClanRespawn;       // seconds before their dead come back
		float Window;            // seconds to take the cake back from their hill
		float Loot;              // value of everything brought home
		float Ask;               // what the Queen expects
		float Carry;             // carrying strength of the colony's ants and squads: food weighs 1 / Carry of its weight
		float Respawn;           // a player's respawn delay
		float Xp;                // XP of the raid
	};
	ANTKITCHEN_API const FDef& Def(uint8 Level);
}

/** The replicated carrier. Spawned by the server the first time anybody asks (AntDifficulty::Info). */
UCLASS()
class ANTKITCHEN_API AAntDifficultyInfo : public AInfo
{
	GENERATED_BODY()

public:
	AAntDifficultyInfo();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated) uint8 Level = AntDiff::Normal;
};

/** Commands of AAntPlayerController::ServerRaid the new systems add (numbers far from AntRaidCmd's own, so branches do not collide). */
namespace AntRaidCmdX { enum : uint8 { Difficulty = 200, VoteMode = 201, Vote = 202 }; }

namespace AntDifficulty
{
	ANTKITCHEN_API AAntDifficultyInfo* Info(const UObject* Ctx);
	/** AntDiff::ELevel on this machine (Normal until the actor has replicated). */
	ANTKITCHEN_API uint8 Level(const UObject* Ctx);
	ANTKITCHEN_API const AntDiff::FDef& Cur(const UObject* Ctx);
	/** Server. True if it changed. Saved; said to everybody. */
	ANTKITCHEN_API bool Set(const UObject* Ctx, uint8 Level, bool bAnnounce = true);
	/** AAntRaid::PlayerCommand's hook: AntRaidCmdX::Difficulty (host, in the nest). True: handled. */
	ANTKITCHEN_API bool Command(AAntRaid* Raid, AAntPlayerController* PC, uint8 Cmd, int32 A);

	// ---- the hooks
	/** The tier's row with the difficulty applied (AAntRaid::TierDef). The tier table itself stays as it is. */
	ANTKITCHEN_API const AntTier::FDef& Scaled(const UObject* Ctx, int32 Tier);
	ANTKITCHEN_API float NoticeMul(const UObject* Ctx);
	/** Multiplier of damage Causer does to Victim (1 unless a resident or a pet hits a colony ant). */
	ANTKITCHEN_API float DamageMul(const AAntCharacter* Victim, const AActor* Causer);
	ANTKITCHEN_API float RespawnMul(const UObject* Ctx);
	/** Carrying strength multiplier of a colony ant (1 for anybody else): "the food is lighter / heavier". */
	ANTKITCHEN_API float CarryMul(const AAntCharacter* Ant);
	ANTKITCHEN_API float SquadCarryMul(const UObject* Ctx);
	/** Larvae the team starts a raid with, from what the nest has. */
	ANTKITCHEN_API int32 LarvaeOut(const UObject* Ctx, int32 NestLarvae);
	/** Larvae that go back into the nest: lent ones are not kept, withheld ones come back. */
	ANTKITCHEN_API int32 LarvaeHome(const UObject* Ctx, int32 Left, int32 NestBefore);
	/** Extra XP for a raid's sum (Hard: +50 %), and the line that goes with it. */
	ANTKITCHEN_API int32 BonusXp(const UObject* Ctx, int32 Sum);
	ANTKITCHEN_API FString XpWhy(const UObject* Ctx);
	ANTKITCHEN_API bool IsHard(const UObject* Ctx);

	// ---- the screen
	ANTKITCHEN_API FLinearColor Color(uint8 Level);
	/** What the level changes, in short lines for the lobby ("жильцы замечают на 25% ближе"...). Empty for Normal. */
	ANTKITCHEN_API TArray<FString> Lines(uint8 Level);
	/** A small stamp-like badge ("ХАРД"). Align 0 left, 1 centre, 2 right. Returns its width. */
	ANTKITCHEN_API float DrawBadge(UCanvas* Cv, const UObject* Ctx, float X, float Y, float Fs, int32 Align = 0);
	/** Dev (-AntDo): difficulty [0|1|2|info]. */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
