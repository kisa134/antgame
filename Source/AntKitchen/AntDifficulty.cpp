#include "AntDifficulty.h"
#include "AntRaid.h"
#include "AntGameState.h"
#include "AntPlayerController.h"
#include "AntCharacter.h"
#include "HumanCharacter.h"
#include "Critters.h"
#include "AntKitchenGameMode.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace AntDiff
{
	// The table of docs/DIFFICULTY.md. Normal is the game as it was: every multiplier 1.
	//                     name                short           en          text                                                                                      notice dmg  npc larv time  quiet clan  hp    resp  window loot  ask   snow  respawn xp
	static const FDef Defs[Count] =
	{
		{ TEXT("Казуальный"), TEXT("КАЗУАЛ"), TEXT("Casual"), TEXT("Прогулка по кухне: жильцы рассеяннее и бьют слабее, +2 личинки на рейд, больше времени и добычи, красных меньше"), 0.75f, 0.6f, -1, 2, 1.25f, 1.5f, 0.7f, 0.8f, 1.4f, 1.3f, 1.15f, 0.8f, 0.8f, 0.6f, 1.f },
		{ TEXT("Обычный"),    TEXT("ОБЫЧНЫЙ"), TEXT("Normal"), TEXT("Игра как задумана"),                                                                                                 1.f,   1.f,   0, 0, 1.f,   1.f,  1.f,  1.f,  1.f,  1.f,  1.f,   1.f,  1.f,  1.f,  1.f },
		{ TEXT("Хард"),       TEXT("ХАРД"),   TEXT("Hard"),   TEXT("Для ветеранов: жильцы замечают дальше и бьют больнее, личинок на одну меньше, времени меньше, красных больше. +50% опыта и медаль за торт"), 1.25f, 1.4f, 1, -1, 0.85f, 0.6f, 1.4f, 1.25f, 0.7f, 0.75f, 0.9f, 1.2f, 1.25f, 1.6f, 1.5f },
	};
	const FDef& Def(uint8 Level) { return Defs[FMath::Min<int32>(Level, Count - 1)]; }
}

AAntDifficultyInfo::AAntDifficultyInfo()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(2.f);
}

void AAntDifficultyInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntDifficultyInfo, Level);
}

namespace AntDifficulty
{
	namespace
	{
		TWeakObjectPtr<AAntDifficultyInfo> GInfo;
		double GNextFind = 0.0;
		const TCHAR* Section = TEXT("AntDifficulty");
	}

	AAntDifficultyInfo* Info(const UObject* Ctx)
	{
		UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		if (!W)
		{
			return nullptr;
		}
		if (AAntDifficultyInfo* I = GInfo.Get(); I && I->GetWorld() == W)
		{
			return I;
		}
		const double Now = FPlatformTime::Seconds();
		const bool bServer = W->GetNetMode() != NM_Client;
		if (!bServer && Now < GNextFind)
		{
			return nullptr;   // a client waiting for the actor: look twice a second, not every call
		}
		GNextFind = Now + 0.5;
		for (TActorIterator<AAntDifficultyInfo> It(W); It; ++It)
		{
			GInfo = *It;
			return *It;
		}
		if (bServer && W->IsGameWorld() && !W->bIsTearingDown)
		{
			AAntDifficultyInfo* I = W->SpawnActor<AAntDifficultyInfo>();
			if (I)
			{
				int32 L = AntDiff::Normal;
				if (GConfig) { GConfig->GetInt(Section, TEXT("Level"), L, GGameUserSettingsIni); }
				FParse::Value(FCommandLine::Get(), TEXT("AntDifficulty="), L);   // dev / tests
				I->Level = (uint8)FMath::Clamp(L, 0, AntDiff::Count - 1);
				UE_LOG(LogTemp, Display, TEXT("ANTDIFF start: %d %s"), (int32)I->Level, AntDiff::Def(I->Level).NameEn);
			}
			GInfo = I;
			return I;
		}
		return nullptr;
	}

	uint8 Level(const UObject* Ctx)
	{
		const AAntDifficultyInfo* I = Info(Ctx);
		return I ? FMath::Min<uint8>(I->Level, AntDiff::Count - 1) : (uint8)AntDiff::Normal;
	}

	const AntDiff::FDef& Cur(const UObject* Ctx) { return AntDiff::Def(Level(Ctx)); }
	bool IsHard(const UObject* Ctx) { return Level(Ctx) == AntDiff::Hard; }

	bool Set(const UObject* Ctx, uint8 NewLevel, bool bAnnounce)
	{
		AAntDifficultyInfo* I = Info(Ctx);
		if (!I || !I->HasAuthority() || NewLevel >= AntDiff::Count || I->Level == NewLevel)
		{
			return false;
		}
		I->Level = NewLevel;
		I->ForceNetUpdate();
		if (GConfig)
		{
			GConfig->SetInt(Section, TEXT("Level"), NewLevel, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		const AntDiff::FDef& D = AntDiff::Def(NewLevel);
		UE_LOG(LogTemp, Display, TEXT("ANTDIFF set: %d %s (notice %.2f dmg %.2f larvae %+d time %.2f clan %.2f loot %.2f snow %.2f respawn %.2f xp %.2f)"),
			(int32)NewLevel, D.NameEn, D.Notice, D.Damage, D.LarvaeAdd, D.Time, D.Clan, D.Loot, D.Snow, D.Respawn, D.Xp);
		if (bAnnounce)
		{
			if (AAntGameState* GS = I->GetWorld()->GetGameState<AAntGameState>())
			{
				GS->MulticastMessage(FString::Printf(TEXT("СЛОЖНОСТЬ: %s. %s"), D.Name, D.Text), Color(NewLevel));
			}
		}
		return true;
	}

	bool Command(AAntRaid* Raid, AAntPlayerController* PC, uint8 Cmd, int32 A)
	{
		if (Cmd != AntRaidCmdX::Difficulty)
		{
			return false;
		}
		if (!Raid || !PC)
		{
			return true;
		}
		if (!PC->IsLocalController())
		{
			PC->ClientToast(TEXT("Сложность выбирает хост"));
		}
		else if (Raid->Phase != AntRaidPhase::Nest)
		{
			PC->ClientToast(TEXT("Сложность меняют в гнезде, между рейдами"));
		}
		else
		{
			Set(Raid, (uint8)FMath::Clamp(A, 0, AntDiff::Count - 1));
		}
		return true;
	}

	const AntTier::FDef& Scaled(const UObject* Ctx, int32 Tier)
	{
		const uint8 L = Level(Ctx);
		const AntTier::FDef& Base = AntTier::Def(Tier);
		if (L == AntDiff::Normal)
		{
			return Base;
		}
		// One scaled copy per (tier, level), made the first time it is asked for. (The tier table is static data.)
		static AntTier::FDef Cache[AntTier::Max + 1][AntDiff::Count];
		static bool bMade[AntTier::Max + 1][AntDiff::Count] = {};
		const int32 T = FMath::Clamp(Tier, 0, AntTier::Max);
		if (!bMade[T][L])
		{
			const AntDiff::FDef& D = AntDiff::Def(L);
			AntTier::FDef S = Base;
			S.Length *= D.Time;
			S.Quiet *= D.Quiet;
			S.LootMul *= D.Loot;
			S.Ask = FMath::RoundToInt(S.Ask * D.Ask / 10.f) * 10;
			for (int32& N : S.Npc) { N = FMath::Clamp(N + D.NpcAdd, 0, 2); }
			auto Heads = [&D](int32 N) { return N <= 0 ? 0 : FMath::Max(1, FMath::RoundToInt(N * D.Clan)); };
			S.Scouts = Heads(S.Scouts);
			S.Haulers = Heads(S.Haulers);
			S.Escorts = Heads(S.Escorts);
			S.Ambushers = Heads(S.Ambushers);
			S.ClanFirst *= D.ClanRespawn;      // (casual: their scouts leave later; hard: sooner)
			S.ClanRespawn *= D.ClanRespawn;
			S.ClanHealth *= D.ClanHealth;
			S.Window *= D.Window;
			Cache[T][L] = S;
			bMade[T][L] = true;
		}
		return Cache[T][L];
	}

	float NoticeMul(const UObject* Ctx) { return Cur(Ctx).Notice; }
	float RespawnMul(const UObject* Ctx) { return Cur(Ctx).Respawn; }
	float SnowMul(const UObject* Ctx) { return Cur(Ctx).Snow; }

	float DamageMul(const AAntCharacter* Victim, const AActor* Causer)
	{
		if (!Victim || !Causer || Victim->Team != 0)
		{
			return 1.f;
		}
		if (Causer->IsA<AHumanCharacter>() || Causer->IsA<ACritter>() || Causer->IsA<AHouseholdNpc>())
		{
			return Cur(Victim).Damage;
		}
		return 1.f;
	}

	int32 LarvaeOut(const UObject* Ctx, int32 NestLarvae)
	{
		return FMath::Max(1, NestLarvae + Cur(Ctx).LarvaeAdd);
	}

	int32 LarvaeHome(const UObject* Ctx, int32 Left, int32 NestBefore)
	{
		const int32 Out = LarvaeOut(Ctx, NestBefore);
		if (Out > NestBefore)
		{
			// Lent larvae are spent first and never kept; a larva found in the raid (Left above what went out) is.
			return FMath::Min(Left, FMath::Max(NestBefore, Left - (Out - NestBefore)));
		}
		return Left + (NestBefore - Out);   // the withheld ones were safe at home
	}

	int32 BonusXp(const UObject* Ctx, int32 Sum)
	{
		return FMath::Max(0, FMath::RoundToInt(Sum * (Cur(Ctx).Xp - 1.f)));
	}

	FString XpWhy(const UObject* Ctx)
	{
		return FString::Printf(TEXT("сложность «%s»: +%d%%"), Cur(Ctx).Name, FMath::RoundToInt((Cur(Ctx).Xp - 1.f) * 100.f));
	}

	FLinearColor Color(uint8 L)
	{
		return L == AntDiff::Casual ? AntUi::C::Leaf : L == AntDiff::Hard ? AntUi::C::Jam : AntUi::C::Amber;
	}

	float DrawBadge(UCanvas* Cv, const UObject* Ctx, float X, float Y, float Fs, int32 Align)
	{
		if (!Cv)
		{
			return 0.f;
		}
		const uint8 L = Level(Ctx);
		const FString T = AntDiff::Def(L).Short;
		const float Pt = 13.f * Fs;
		const FVector2D M = AntUiCanvas::TextSize(Cv, T, AntUi::EFont::Display, Pt);
		const float W = M.X + 16.f * Fs, H = M.Y + 6.f * Fs;
		const float X0 = Align == 1 ? X - W * 0.5f : Align == 2 ? X - W : X;
		AntUiCanvas::Rect(Cv, X0, Y, W, H, AntUi::WithA(AntUi::C::Ink, 0.85f));
		AntUiCanvas::Frame(Cv, X0, Y, W, H, FMath::Max(1.f, 2.f * Fs), Color(L));
		AntUiCanvas::Text(Cv, T, X0 + W * 0.5f, Y + 3.f * Fs, AntUi::EFont::Display, Pt, Color(L), 1);
		return W;
	}

	bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		if (A.Num() == 0 || A[0] != TEXT("difficulty"))
		{
			return false;
		}
		if (A.Num() >= 2 && FCString::IsNumeric(*A[1]))
		{
			Set(GM, (uint8)FMath::Clamp(FCString::Atoi(*A[1]), 0, AntDiff::Count - 1));
		}
		const AntDiff::FDef& D = Cur(GM);
		const AAntRaid* Raid = GM ? AAntRaid::Get(GM->GetWorld()) : nullptr;
		const AAntGameState* GS = GM ? GM->GetGameState<AAntGameState>() : nullptr;
		if (Raid)
		{
			const AntTier::FDef& T = Raid->TierDef();
			UE_LOG(LogTemp, Display, TEXT("ANTDIFF info: %s | tier %d: length %.0f quiet %.0f loot x%.2f ask %d npc %d/%d/%d/%d clan %d+%d+%d+%d hp x%.2f respawn %.0f window %.0f | larvae nest %d raid %d | match %.0f"),
				D.NameEn, (int32)Raid->Tier, T.Length, T.Quiet, T.LootMul, T.Ask, T.Npc[0], T.Npc[1], T.Npc[2], T.Npc[3], T.Scouts, T.Haulers, T.Escorts, T.Ambushers, T.ClanHealth, T.ClanRespawn, T.Window,
				Raid->Nest.Larvae, GS ? GS->Larvae : -1, GS ? GS->TimeRemaining : -1.f);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("ANTDIFF info: %s (no raid)"), D.NameEn);
		}
		return true;
	}
}
