#include "AntFighter.h"
#include "AntCharacter.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntGameState.h"
#include "AntKitchenGameMode.h"
#include "AntSnowball.h"
#include "AntRts.h"
#include "KitchenLoot.h"
#include "Critters.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "HumanCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"

// Squad veterans (docs/FIGHTER.md, "Ветераны"): the host's roster, the draft, quirks, doctrines, permadeath and the wall.
// Squad ants of the snowball raid are real ants (AntRts.h): a drafted veteran is one of them with a name, a rank and quirks.

namespace
{
	FString RosterPath() { return FPaths::ProjectSavedDir() / TEXT("Nest") / TEXT("veterans.json"); }

	// Nicknames (no real people, no army words) and epithets.
	const TCHAR* Nicks[] =
	{
		TEXT("Капелька"), TEXT("Изюм"), TEXT("Сухарик"), TEXT("Пряник"), TEXT("Колосок"), TEXT("Мак"), TEXT("Кунжут"), TEXT("Шуруп"), TEXT("Гвоздик"), TEXT("Скрепка"),
		TEXT("Пуговка"), TEXT("Нитка"), TEXT("Батон"), TEXT("Бублик"), TEXT("Сушка"), TEXT("Пельмень"), TEXT("Тмин"), TEXT("Перчик"), TEXT("Рисинка"), TEXT("Гречка"),
		TEXT("Пшено"), TEXT("Ириска"), TEXT("Карамелька"), TEXT("Леденец"), TEXT("Зефир"), TEXT("Мармелад"), TEXT("Фундук"), TEXT("Арахис"), TEXT("Семечка"), TEXT("Горошек"),
		TEXT("Фасолька"), TEXT("Укроп"), TEXT("Корица"), TEXT("Ваниль"), TEXT("Какао"), TEXT("Сахарок"), TEXT("Лаврушка"), TEXT("Шпрот"), TEXT("Винтик"), TEXT("Шпунтик"),
		TEXT("Крошка"), TEXT("Груздь"), TEXT("Опёнок"), TEXT("Желудь"), TEXT("Хвоинка"), TEXT("Соринка"), TEXT("Пылинка"), TEXT("Бусинка"), TEXT("Кнопка"), TEXT("Спичка"),
	};
	const TCHAR* Epithets[] =
	{
		TEXT("Батя"), TEXT("Дед"), TEXT("Малой"), TEXT("Тихий"), TEXT("Длинный"), TEXT("Хромой"), TEXT("Усатый"), TEXT("Рыжий"), TEXT("Шустрый"), TEXT("Сонный"),
		TEXT("Лысый"), TEXT("Кривой"), TEXT("Весёлый"), TEXT("Угрюмый"),
	};
	/** Veteran ranks stop at «Старшина»: XP of each. */
	const int32 VetXp[] = { 0, 100, 250, 500, 900 };

	FString DisplayName(const FAntVeteran& V) { return FString::Printf(TEXT("%s %s"), AntRank::Name(V.Rank), *V.Name); }

	int32 CountTraits(uint8 T) { return FMath::CountBits((uint64)T); }

	uint8 NewTrait(uint8 Have, FRandomStream& Rng)
	{
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const int32 T = Rng.RandRange(0, AntTrait::Count - 1);
			if (!(Have & (1 << T))) { return (uint8)(1 << T); }
		}
		return 0;
	}

	AAntRaid* RaidOf(const UWorld* W) { return W ? AAntRaid::Get(W) : nullptr; }

	APlayerState* FindPlayer(UWorld* W, const FString& Who)
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->PlayerState) { continue; }
			if (Who.IsEmpty() ? PC->IsLocalController() : PC->PlayerState->GetPlayerName().Contains(Who)) { return PC->PlayerState; }
		}
		return nullptr;
	}

	void Say(APlayerState* PS, const FString& Text)
	{
		if (AAntPlayerController* PC = PS ? Cast<AAntPlayerController>(PS->GetOwner()) : nullptr) { PC->ClientToast(Text); }
	}

	/** Coward quirk: running from the cat; sniffing out food (server, per ant). */
	struct FVetMoment { float NextLook = 0.f; float FleeUntil = 0.f; FVector Flee = FVector::ZeroVector; float NextSniff = 0.f; };
	TMap<TWeakObjectPtr<AAntCharacter>, FVetMoment> GMoments;
}

namespace AntVets
{
	int32 RankOfXp(int32 Xp)
	{
		int32 R = 0;
		for (int32 i = 0; i < UE_ARRAY_COUNT(VetXp); ++i) { if (Xp >= VetXp[i]) { R = i; } }
		return R;
	}

	int32 DraftCapFor(const UWorld* World)
	{
		// The colony's barracks (Soldiers branch, «Казарма») and brood decide it; the colony agent may replace this with its own building.
		const AAntRaid* R = RaidOf(World);
		if (!R) { return 2; }
		return FMath::Clamp(2 + R->Nest.Tier_(AntUp::Soldiers) + R->Nest.Tier_(AntUp::Brood) / 2, 2, 6);
	}

	void Load(AAntFighterState* FS)
	{
		if (!FS || FS->bLoaded) { return; }
		FS->bLoaded = true;
		FString Json;
		FAntRosterSave S;
		if (FFileHelper::LoadFileToString(Json, *RosterPath()) && FJsonObjectConverter::JsonObjectStringToUStruct(Json, &S, 0, 0))
		{
			FS->Roster = S.Roster;
			FS->Fallen = S.Fallen;
			FS->NextId = FMath::Max(1, S.NextId);
			FS->UsedNames = S.UsedNames;
			for (FAntVeteran& V : FS->Roster) { V.bOut = false; }
			UE_LOG(LogTemp, Display, TEXT("ANTVET roster loaded: %d veterans, %d fallen (%s)"), FS->Roster.Num(), FS->Fallen.Num(), *RosterPath());
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("ANTVET roster: none yet (%s)"), *RosterPath());
		}
	}

	void Save(AAntFighterState* FS)
	{
		if (!FS || FS->GetNetMode() == NM_Client) { return; }
		FAntRosterSave S;
		S.Roster = FS->Roster;
		S.Fallen = FS->Fallen;
		S.NextId = FS->NextId;
		S.UsedNames = FS->UsedNames;
		FString Json;
		if (FJsonObjectConverter::UStructToJsonObjectString(S, Json))
		{
			const FString Tmp = RosterPath() + TEXT(".tmp");
			if (FFileHelper::SaveStringToFile(Json, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				IFileManager::Get().Move(*RosterPath(), *Tmp, true, true);
			}
		}
		FS->ForceNetUpdate();
	}

	FString NewName(AAntFighterState* FS, FRandomStream& Rng)
	{
		for (int32 Try = 0; Try < 60; ++Try)
		{
			FString N = Nicks[Rng.RandRange(0, UE_ARRAY_COUNT(Nicks) - 1)];
			if (Rng.FRand() < 0.35f) { N = FString::Printf(TEXT("%s %s"), Epithets[Rng.RandRange(0, UE_ARRAY_COUNT(Epithets) - 1)], *N); }
			const bool bTaken = FS->UsedNames.Contains(N) || FS->Roster.ContainsByPredicate([&N](const FAntVeteran& V) { return V.Name == N; });
			if (!bTaken) { FS->UsedNames.Add(N); return N; }
		}
		return FString::Printf(TEXT("Номер %d"), FS->NextId);
	}

	int32 NumDrafted(const AAntFighterState* FS, const FString& PlayerId)
	{
		int32 N = 0;
		if (FS) { for (const FAntVeteran& V : FS->Roster) { N += V.DraftedBy == PlayerId ? 1 : 0; } }
		return N;
	}

	int32 BestDraftedRank(const AAntFighterState* FS, const FString& PlayerId)
	{
		int32 R = -1;
		if (FS) { for (const FAntVeteran& V : FS->Roster) { if (V.DraftedBy == PlayerId) { R = FMath::Max(R, (int32)V.Rank); } } }
		return R;
	}

	static FAntVeteran& AddVet(AAntFighterState* FS, FRandomStream& Rng, int32 RaidNo, uint8 Caste)
	{
		FAntVeteran V;
		V.Id = FS->NextId++;
		V.Name = NewName(FS, Rng);
		V.Traits = NewTrait(0, Rng);
		V.Caste = Caste;
		V.Joined = RaidNo;
		return FS->Roster.Add_GetRef(V);
	}

	void Command(AAntPlayerController* PC, uint8 C, int32 A, int32 B)
	{
		UWorld* W = PC ? PC->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		if (!FS || !PC->PlayerState) { return; }
		const AAntRaid* Raid = RaidOf(W);
		if (Raid && Raid->InRaid() && C != Cmd::Doctrine)
		{
			PC->ClientToast(TEXT("Ветеранов набирают в гнезде, до рейда"));
			return;
		}
		const FString Me = AntFighter::IdOf(PC->PlayerState);
		const int32 Cap = AntVets::DraftCapFor(W);
		FS->DraftCap = (uint8)Cap;
		if (C == Cmd::Draft || C == Cmd::Undraft)
		{
			FAntVeteran* V = FS->FindVet(A);
			if (!V) { return; }
			if (C == Cmd::Undraft) { if (V->DraftedBy == Me) { V->DraftedBy.Reset(); } }
			else if (!V->DraftedBy.IsEmpty() && V->DraftedBy != Me) { PC->ClientToast(FString::Printf(TEXT("%s уже в чужом отряде"), *DisplayName(*V))); return; }
			else if (V->DraftedBy != Me)
			{
				if (NumDrafted(FS, Me) >= Cap) { PC->ClientToast(FString::Printf(TEXT("В отряд можно взять ветеранов: %d (Казарма и Выводок дают больше)"), Cap)); return; }
				V->DraftedBy = Me;
			}
			UE_LOG(LogTemp, Display, TEXT("ANTVET %s %s %s (now %d of %d)"), *PC->PlayerState->GetPlayerName(), C == Cmd::Draft ? TEXT("drafts") : TEXT("lets go"), *DisplayName(*V), NumDrafted(FS, Me), Cap);
		}
		else if (C == Cmd::AutoDraft)
		{
			TArray<FAntVeteran*> Free;
			for (FAntVeteran& V : FS->Roster) { if (V.DraftedBy.IsEmpty()) { Free.Add(&V); } }
			Free.Sort([](const FAntVeteran& X, const FAntVeteran& Y) { return X.Xp > Y.Xp; });
			for (FAntVeteran* V : Free) { if (NumDrafted(FS, Me) < Cap) { V->DraftedBy = Me; } }
			UE_LOG(LogTemp, Display, TEXT("ANTVET %s auto-drafts: %d of %d"), *PC->PlayerState->GetPlayerName(), NumDrafted(FS, Me), Cap);
		}
		else if (C == Cmd::Doctrine)
		{
			const int32 D = FMath::Clamp(A, 0, AntDoctrine::Count - 1);
			if (D != 0 && BestDraftedRank(FS, Me) < AntDoctrine::NeedRank(D))
			{
				PC->ClientToast(FString::Printf(TEXT("«%s» нужен ветеран в звании «%s» в отряде"), AntDoctrine::Name(D), AntRank::Name(AntDoctrine::NeedRank(D))));
				return;
			}
			FAntDoctrinePick* P = FS->Doctrines.FindByPredicate([&Me](const FAntDoctrinePick& X) { return X.PlayerId == Me; });
			if (!P) { P = &FS->Doctrines.AddDefaulted_GetRef(); P->PlayerId = Me; }
			P->Doctrine = (uint8)D;
			UE_LOG(LogTemp, Display, TEXT("ANTVET %s picks the doctrine «%s»"), *PC->PlayerState->GetPlayerName(), AntDoctrine::Name(D));
		}
		Save(FS);
	}

	void SpawnDrafted(AAntRaid* Raid)
	{
		UWorld* W = Raid ? Raid->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		AAntSnowball* S = AAntSnowball::Get(W);
		if (!FS) { return; }
		if (!S || !AntSnowball::Enabled(Raid))
		{
			UE_LOG(LogTemp, Display, TEXT("ANTVET no snowball raid: veterans stay in the barracks"));
			return;
		}
		const int32 Cap = DraftCapFor(W);
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			APlayerState* PS = PC ? PC->PlayerState : nullptr;
			if (!PS) { continue; }
			const FString Id = AntFighter::IdOf(PS);
			int32 Went = 0;
			for (FAntVeteran& V : FS->Roster)
			{
				if (V.DraftedBy != Id || Went >= Cap) { continue; }
				if (AntRts::Spawn(PS, 1, V.Caste) != 1) { continue; }
				FAntSnowPlayer* P = S->Find(PS);
				AAntCharacter* A = (P && P->Squad.Num() > 0) ? P->Squad.Last() : nullptr;
				if (!A) { continue; }
				A->VetId = V.Id;
				A->VetName = DisplayName(V);
				A->VetRank = V.Rank;
				A->VetTraits = V.Traits;
				A->VetDoctrine = FS->DoctrineOf(Id);
				A->bFighterLuckUsed = false;
				A->RefreshStats();
				A->Health = A->MaxHealth;
				A->ForceNetUpdate();
				V.bOut = true;
				++Went;
				UE_LOG(LogTemp, Display, TEXT("ANTVET %s goes out with %s: traits 0x%02x, hp %.0f, str %.1f, doctrine %d"), *DisplayName(V), *PS->GetPlayerName(), (int32)V.Traits, A->MaxHealth, A->GetStrength(), (int32)A->VetDoctrine);
			}
			if (Went > 0) { Say(PS, FString::Printf(TEXT("С тобой ветераны: %d. Береги их — погибший не вернётся"), Went)); }
		}
		FS->ForceNetUpdate();
	}

	void OnSquadAntDied(AAntCharacter* Ant, AActor* Causer)
	{
		UWorld* W = Ant ? Ant->GetWorld() : nullptr;
		AAntFighterState* FS = AAntFighterState::Get(W, true);
		FAntVeteran* V = FS ? FS->FindVet(Ant->VetId) : nullptr;
		if (!V) { return; }
		const AAntRaid* Raid = RaidOf(W);
		FAntFallen F;
		F.Name = V->Name;
		F.Rank = V->Rank;
		F.Raid = Raid ? Raid->RaidNo : 0;
		F.Raids = V->Raids;
		F.Cause = AntFighter::CauseOf(Ant, Causer);
		FS->Fallen.Add(F);
		FS->FallenThisRaid.Add(TPair<FString, FAntFallen>(V->DraftedBy, F));
		UE_LOG(LogTemp, Display, TEXT("ANTVET FALLEN %s: %s, raid %d (served %d raids)"), *DisplayName(*V), *F.Cause, F.Raid, F.Raids);
		if (GEngine) { if (AAntGameState* GS = W->GetGameState<AAntGameState>()) { GS->MulticastMessage(FString::Printf(TEXT("Пал ветеран: %s — %s"), *DisplayName(*V), *F.Cause), FLinearColor(1.f, 0.55f, 0.4f)); } }
		FS->Roster.RemoveAll([Id = V->Id](const FAntVeteran& X) { return X.Id == Id; });
		Ant->VetId = -1;
		Save(FS);
	}

	void AfterRaid(AAntFighterState* FS, int32 RaidNo, bool bWiped)
	{
		if (!FS) { return; }
		FRandomStream Rng(RaidNo * 7919 + FS->NextId * 31 + FS->Roster.Num());
		TMap<FString, int32> NewOf;
		auto HL = [FS](const FString& Owner, const TCHAR* Title, const FString& Line, const TCHAR* Icon, uint8 Kind, int32 RB, int32 RA)
		{
			FAntHighlight H;
			H.PlayerId = Owner;
			H.Player = Owner.StartsWith(TEXT("local:")) ? Owner.Mid(6) : Owner;
			if (const AGameStateBase* G = FS->GetWorld() ? FS->GetWorld()->GetGameState() : nullptr)
			{
				for (APlayerState* P : G->PlayerArray) { if (P && AntFighter::IdOf(P) == Owner) { H.Player = P->GetPlayerName(); break; } }
			}
			H.Title = Title;
			H.Line = Line;
			H.Icon = Icon;
			H.Kind = Kind;
			H.RankBefore = RB;
			H.RankAfter = RA;
			FS->VetHighlights.Add(H);
		};
		const int32 Cap = DraftCapFor(FS->GetWorld());
		for (const TPair<TWeakObjectPtr<APlayerState>, int32>& Sv : FS->Survivors)
		{
			const FString Owner = Sv.Key.IsValid() ? AntFighter::IdOf(Sv.Key.Get()) : FString();
			if (Sv.Value >= 0)
			{
				FAntVeteran* V = FS->FindVet(Sv.Value);
				if (!V) { continue; }
				if (bWiped) { continue; }   // left with the out-list: mourned below
				V->bOut = false;
				V->Raids++;
				V->Xp += 100;
				const int32 Old = V->Rank;
				V->Rank = (uint8)RankOfXp(V->Xp);
				UE_LOG(LogTemp, Display, TEXT("ANTVET %s came home: raids %d, xp %d, rank %d -> %d"), *V->Name, V->Raids, V->Xp, Old, (int32)V->Rank);
				if (V->Rank > Old)
				{
					FString Extra;
					if ((V->Rank == 2 || V->Rank == 4) && CountTraits(V->Traits) < 2)
					{
						const uint8 T = NewTrait(V->Traits, Rng);
						V->Traits |= T;
						if (T) { Extra = FString::Printf(TEXT(", теперь ещё и «%s»"), AntTrait::Name(FMath::FloorLog2(T))); }
					}
					HL(V->DraftedBy.IsEmpty() ? Owner : V->DraftedBy, TEXT("ВЕТЕРАН"), FString::Printf(TEXT("%s — теперь %s%s"), *V->Name, AntRank::Lower(V->Rank), *Extra), TEXT("Icons/T_UI_Icon_Squad"), 3, Old + 1, V->Rank + 1);
				}
			}
			else if (!bWiped && !Owner.IsEmpty() && NewOf.FindOrAdd(Owner) < 2 && FS->Roster.Num() < 30)
			{
				// A recruit lived through his first raid: he has a name now.
				FAntVeteran& V = AddVet(FS, Rng, RaidNo, Sv.Value == -2 ? 1 : 0);
				V.Raids = 1;
				V.Xp = 100;
				V.Rank = (uint8)RankOfXp(V.Xp);
				if (NumDrafted(FS, Owner) < Cap) { V.DraftedBy = Owner; }
				NewOf[Owner]++;
				UE_LOG(LogTemp, Display, TEXT("ANTVET new veteran %s (%s) for %s, quirk «%s»"), *DisplayName(V), V.Caste ? TEXT("soldier") : TEXT("forager"), *Owner, AntTrait::Name(FMath::FloorLog2(FMath::Max<uint32>(1, V.Traits))));
				HL(Owner, TEXT("НОВЫЙ ВЕТЕРАН"), FString::Printf(TEXT("%s выжил в первом рейде — «%s»"), *DisplayName(V), AntTrait::Name(FMath::FloorLog2(FMath::Max<uint32>(1, V.Traits)))), TEXT("Icons/T_UI_Icon_Recruit"), 5, 0, 0);
			}
		}
		// Out in the flat and not home: lost (a wiped raid takes everybody who was out).
		for (int32 i = FS->Roster.Num() - 1; i >= 0; --i)
		{
			FAntVeteran& V = FS->Roster[i];
			if (!V.bOut) { continue; }
			FAntFallen F;
			F.Name = V.Name;
			F.Rank = V.Rank;
			F.Raid = RaidNo;
			F.Raids = V.Raids;
			F.Cause = bWiped ? TEXT("остался в квартире, когда отряд погиб") : TEXT("пропал без вести в квартире");
			FS->Fallen.Add(F);
			FS->FallenThisRaid.Add(TPair<FString, FAntFallen>(V.DraftedBy, F));
			UE_LOG(LogTemp, Display, TEXT("ANTVET LOST %s: %s"), *DisplayName(V), *F.Cause);
			FS->Roster.RemoveAt(i);
		}
		for (const TPair<FString, FAntFallen>& F : FS->FallenThisRaid)
		{
			HL(F.Key, TEXT("ПАЛ"), FString::Printf(TEXT("%s %s — %s, рейд №%d"), AntRank::Name(F.Value.Rank), *F.Value.Name, *F.Value.Cause, F.Value.Raid), TEXT("Icons/T_UI_Icon_Larva"), 4, 0, 0);
		}
		FS->FallenThisRaid.Reset();
		FS->Survivors.Reset();
		Save(FS);
	}

	void Tick(AAntCharacter* Ant, float Dt)
	{
		if (!Ant || Ant->VetId < 0 || !Ant->Has(AntTrait::Coward) || Ant->bDead)
		{
			return;
		}
		UWorld* W = Ant->GetWorld();
		const float Now = W->GetTimeSeconds();
		FVetMoment& M = GMoments.FindOrAdd(Ant);
		if (Now >= M.NextLook)
		{
			M.NextLook = Now + 0.5f;
			for (TActorIterator<ACritter> It(W); It; ++It)
			{
				if (It->Kind == 1 && FVector::Dist2D(It->GetActorLocation(), Ant->GetActorLocation()) < 700.f)
				{
					if (M.FleeUntil < Now) { UE_LOG(LogTemp, Display, TEXT("ANTVET %s (трус) runs from the cat"), *Ant->GetDisplayName()); }
					M.FleeUntil = Now + 1.5f;
					M.Flee = (Ant->GetActorLocation() - It->GetActorLocation()).GetSafeNormal2D();
				}
			}
			if (Now >= M.NextSniff && !Ant->Carrying)
			{
				M.NextSniff = Now + 12.f;
				if (FMath::FRand() < 0.35f)
				{
					AKitchenLoot* Best = nullptr;
					float BestD = 2200.f;
					for (TActorIterator<AKitchenLoot> It(W); It; ++It)
					{
						if (It->bMain || It->bIsGear || It->bBody || It->IsCarried() || It->Weight > 6) { continue; }
						const float D = (float)FVector::Dist(It->GetActorLocation(), Ant->GetActorLocation());
						if (D < BestD) { BestD = D; Best = *It; }
					}
					if (Best)
					{
						UE_LOG(LogTemp, Display, TEXT("ANTVET %s (трус) sniffs out «%s» %.0f away"), *Ant->GetDisplayName(), *Best->LootName, BestD);
						if (APlayerState* Owner = AntSnowball::SquadOwnerOf(Ant)) { Say(Owner, FString::Printf(TEXT("%s вынюхал: «%s», %.1f м"), *Ant->GetDisplayName(), *Best->LootName, BestD / 190.f)); }
					}
				}
			}
		}
		if (Now < M.FleeUntil)
		{
			Ant->AddMovementInput(M.Flee, 1.f, true);
		}
	}

	bool Dev(AAntFighterState* FS, AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		UWorld* W = GM->GetWorld();
		const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString(TEXT("list"));
		auto Arg = [&A](int32 i) { return A.IsValidIndex(i) ? A[i] : FString(); };
		auto Int = [&A](int32 i, int32 Def = 0) { return A.IsValidIndex(i) ? FCString::Atoi(*A[i]) : Def; };
		auto Rest = [&A](int32 From) { FString S; for (int32 i = From; i < A.Num(); ++i) { S += (S.IsEmpty() ? TEXT("") : TEXT(" ")) + A[i]; } return S; };
		auto Find = [FS](const FString& Key) -> FAntVeteran*
		{
			if (Key == TEXT("0") || Key == TEXT("first"))
			{
				// the first one out with a squad, else the first in the barracks
				if (FAntVeteran* Out = FS->Roster.FindByPredicate([](const FAntVeteran& V) { return V.bOut; })) { return Out; }
				return FS->Roster.Num() > 0 ? &FS->Roster[0] : nullptr;
			}
			if (Key.IsNumeric()) { return FS->FindVet(FCString::Atoi(*Key)); }
			return FS->Roster.FindByPredicate([&Key](const FAntVeteran& V) { return V.Name.Contains(Key); });
		};
		FRandomStream Rng(FPlatformTime::Cycles());
		const AAntRaid* Raid = RaidOf(W);
		if (Sub == TEXT("list"))
		{
			UE_LOG(LogTemp, Display, TEXT("ANTVET list: %d veterans, %d fallen, draft cap %d"), FS->Roster.Num(), FS->Fallen.Num(), DraftCapFor(W));
			for (const FAntVeteran& V : FS->Roster)
			{
				FString Tr;
				for (int32 t = 0; t < AntTrait::Count; ++t) { if (V.Has(t)) { Tr += FString::Printf(TEXT("%s«%s»"), Tr.IsEmpty() ? TEXT("") : TEXT(" "), AntTrait::Name(t)); } }
				UE_LOG(LogTemp, Display, TEXT("ANTVET  #%d %s: xp %d raids %d kills %d carried %d, %s, drafted by «%s»%s"), V.Id, *DisplayName(V), V.Xp, V.Raids, V.Kills, V.Carried, *Tr, *V.DraftedBy, V.bOut ? TEXT(", OUT") : TEXT(""));
			}
			for (const FAntFallen& F : FS->Fallen) { UE_LOG(LogTemp, Display, TEXT("ANTVET  wall: %s %s — %s, рейд №%d"), AntRank::Name(F.Rank), *F.Name, *F.Cause, F.Raid); }
			return true;
		}
		if (Sub == TEXT("add"))
		{
			// vet add [n] [xp] [traits bits]
			const int32 N = FMath::Clamp(Int(2, 1), 1, 20);
			for (int32 i = 0; i < N; ++i)
			{
				FAntVeteran& V = AddVet(FS, Rng, Raid ? Raid->RaidNo : 0, (uint8)(Rng.FRand() < 0.3f ? 1 : 0));
				V.Xp = Int(3, Rng.RandRange(0, 600));
				V.Rank = (uint8)RankOfXp(V.Xp);
				V.Raids = 1 + V.Xp / 100;
				if (A.IsValidIndex(4)) { V.Traits = (uint8)Int(4); }
				UE_LOG(LogTemp, Display, TEXT("ANTVET admin: added #%d %s"), V.Id, *DisplayName(V));
			}
			Save(FS);
			return true;
		}
		if (Sub == TEXT("kill"))
		{
			// vet kill <id|name> [cause]: out in the raid - the ant dies there; in the barracks - straight to the wall
			FAntVeteran* V = Find(Arg(2));
			if (!V) { UE_LOG(LogTemp, Display, TEXT("ANTVET kill: no %s"), *Arg(2)); return true; }
			const FString Cause = A.Num() > 3 ? Rest(3) : FString(TEXT("приказ сверху (админ)"));
			for (TActorIterator<AAntCharacter> It(W); It; ++It)
			{
				if (It->VetId == V->Id && !It->bDead) { It->HurtBy(100000.f, Cause); return true; }
			}
			FAntFallen F;
			F.Name = V->Name; F.Rank = V->Rank; F.Raid = Raid ? Raid->RaidNo : 0; F.Raids = V->Raids; F.Cause = Cause;
			FS->Fallen.Add(F);
			UE_LOG(LogTemp, Display, TEXT("ANTVET FALLEN %s: %s (admin)"), *DisplayName(*V), *Cause);
			FS->Roster.RemoveAll([Id = V->Id](const FAntVeteran& X) { return X.Id == Id; });
			Save(FS);
			return true;
		}
		if (Sub == TEXT("revive"))
		{
			// vet revive [name]: back from the wall (admin only: in the game death is for good)
			int32 Idx = FS->Fallen.Num() - 1;
			if (A.IsValidIndex(2)) { Idx = FS->Fallen.IndexOfByPredicate([&A](const FAntFallen& F) { return F.Name.Contains(A[2]); }); }
			if (!FS->Fallen.IsValidIndex(Idx)) { return true; }
			const FAntFallen F = FS->Fallen[Idx];
			FS->Fallen.RemoveAt(Idx);
			FAntVeteran& V = AddVet(FS, Rng, Raid ? Raid->RaidNo : 0, 0);
			FS->UsedNames.Remove(V.Name);
			V.Name = F.Name; V.Rank = F.Rank; V.Xp = VetXp[FMath::Clamp((int32)F.Rank, 0, 4)]; V.Raids = F.Raids;
			UE_LOG(LogTemp, Display, TEXT("ANTVET admin: %s is back from the wall"), *DisplayName(V));
			Save(FS);
			return true;
		}
		if (Sub == TEXT("draft") || Sub == TEXT("undraft"))
		{
			FAntVeteran* V = Find(Arg(2));
			APlayerState* PS = FindPlayer(W, Arg(3));
			if (!V || !PS) { return true; }
			if (Sub == TEXT("draft")) { V->DraftedBy = AntFighter::IdOf(PS); } else { V->DraftedBy.Reset(); }
			UE_LOG(LogTemp, Display, TEXT("ANTVET admin: %s %s %s"), *DisplayName(*V), Sub == TEXT("draft") ? TEXT("drafted by") : TEXT("let go by"), *PS->GetPlayerName());
			Save(FS);
			return true;
		}
		if (Sub == TEXT("autodraft") || Sub == TEXT("doctrine"))
		{
			APlayerState* PS = FindPlayer(W, Arg(Sub == TEXT("doctrine") ? 3 : 2));
			if (AAntPlayerController* PC = PS ? Cast<AAntPlayerController>(PS->GetOwner()) : nullptr)
			{
				Command(PC, Sub == TEXT("doctrine") ? Cmd::Doctrine : Cmd::AutoDraft, Int(2), 0);
			}
			return true;
		}
		if (Sub == TEXT("xp"))
		{
			if (FAntVeteran* V = Find(Arg(2))) { V->Xp = FMath::Max(0, V->Xp + Int(3)); V->Rank = (uint8)RankOfXp(V->Xp); Save(FS); UE_LOG(LogTemp, Display, TEXT("ANTVET admin: %s xp %d"), *DisplayName(*V), V->Xp); }
			return true;
		}
		if (Sub == TEXT("clear"))
		{
			FS->Roster.Reset();
			FS->Fallen.Reset();
			FS->Doctrines.Reset();
			Save(FS);
			UE_LOG(LogTemp, Display, TEXT("ANTVET admin: roster and wall cleared"));
			return true;
		}
		if (Sub == TEXT("spawn"))
		{
			if (AAntRaid* R = AAntRaid::Get(W)) { SpawnDrafted(R); }
			return true;
		}
		if (Sub == TEXT("stomp"))
		{
			// vet stomp <id|name|first>: that veteran (out in the raid) is put under the Giant's slipper, and the slipper comes down (a real death)
			FAntVeteran* V = Find(Arg(2));
			AHumanCharacter* H = nullptr;
			for (TActorIterator<AHumanCharacter> Hi(W); Hi; ++Hi) { if (Hi->Fig == HumanFig::Dad) { H = *Hi; break; } }
			for (TActorIterator<AAntCharacter> It(W); It && V && H; ++It)
			{
				if (It->VetId == V->Id && !It->bDead)
				{
					It->ApplyTeleport(H->GetStompPoint(0) + FVector(0.f, 0.f, 12.f));
					It->Webbed(4.f);   // held there while the slipper comes down (as the dev "crush" does)
					H->StompReadyTime = 0.f;
					H->TryStomp(0);   // the slipper's swing for the eye (refused while he is busy or stunned)...
					// ...and its blow, as DoStompImpact deals it to what is right under the sole
					UGameplayStatics::ApplyDamage(*It, 200.f, H->GetController(), H, UDamageType::StaticClass());
					UE_LOG(LogTemp, Display, TEXT("ANTVET dev: the slipper comes down on %s"), *It->GetDisplayName());
					return true;
				}
			}
			UE_LOG(LogTemp, Display, TEXT("ANTVET dev stomp: no such veteran out in the raid (%s)"), *Arg(2));
			return true;
		}
		if (Sub == TEXT("near"))
		{
			// vet near: every squad veteran of the raid comes to the host's ant (for screenshots and tests)
			APlayerController* PC = W->GetFirstPlayerController();
			const APawn* Me = PC ? PC->GetPawn() : nullptr;
			int32 i = 0;
			for (TActorIterator<AAntCharacter> It(W); It && Me; ++It)
			{
				if (It->VetId >= 0 && !It->bDead) { It->ApplyTeleport(Me->GetActorLocation() + FVector(FMath::Cos(i * 1.3f), FMath::Sin(i * 1.3f), 0.f) * (60.f + 12.f * i) + FVector(0.f, 0.f, 10.f)); ++i; }
			}
			return true;
		}
		return false;
	}
}
