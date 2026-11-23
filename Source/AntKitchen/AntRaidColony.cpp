#include "AntRaid.h"
#include "AntColony.h"
#include "AntColonyView.h"
#include "AntNest.h"
#include "AntCharacter.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// The colony's part of the raid (AntColony.h, docs/COLONY.md): who brought what, the trophies, the fungus farm, the chronicle,
// the summary for the personal layer (AntMetaBus), and the dev / admin commands "colony ...".

FAntRaidPlayerLine& AAntRaid::LineOf(const AAntCharacter* Ant)
{
	const FString Name = Ant ? Ant->GetDisplayName() : FString(TEXT("?"));
	FAntRaidPlayerLine& L = RaidLines.FindOrAdd(Name);
	if (L.Name.IsEmpty())
	{
		L.Name = Name;
		const APlayerState* PS = Ant ? Ant->GetPlayerState() : nullptr;
		L.Id = (PS && PS->GetUniqueId().IsValid()) ? PS->GetUniqueId().ToString() : Name;
	}
	return L;
}

void AAntRaid::ColonyAfterRaid(FAntRaidResult& R, uint8 How)
{
	const bool bWiped = How == AntRaidEnd::Wiped;
	const bool bCakeHome = R.Cake == AntCake::Home && !bWiped;

	// The fungus farm grew while everybody was out; the radio's reports bring jelly.
	R.Grown = AntColony::FungusFood(Nest);
	R.GrownJelly = AntColony::FungusJelly(Nest);
	if (R.Grown > 0) { AntColony::AddFood(Nest, AntFoodKind::Protein, R.Grown); Nest.Grown += R.Grown; }
	Nest.Jelly += R.GrownJelly;
	if (!bWiped && R.ContractsDone > 0 && Nest.Own_(AntColony::OwnRadio) >= 2) { Nest.Jelly += 1; R.Jelly += 1; }

	// Trophies.
	auto Try = [&](int32 Id, bool bWhen) { if (bWhen && Id >= 0 && Id < AntColony::TrophyCount() && AntColony::GiveTrophy(Nest, Id)) { R.NewTrophies.Add((uint8)Id); } };
	Try(0, !bWiped && R.Kept > 0);
	Try(1, bCakeHome);
	Try(2, !bWiped && R.Ask > 0 && R.Kept >= R.Ask * 3 / 2);
	Try(3, R.Kinds.IsValidIndex(AntFoodKind::Protein) && R.Kinds[AntFoodKind::Protein] >= 150);
	Try(4, bCakeHome && R.Deaths > 0);
	Try(5, !bWiped && R.Tier >= 3 && R.Kept > 0);
	if (!bWiped) { for (uint8 Id : RaidTrophies) { Try(Id, Id != 255); } }
	Try(10, R.ContractsDone >= 2);
	Try(11, bCakeHome && bCakeWasInHill);
	Try(12, bCakeHome && R.Tier >= 4);

	// Who came home, and the colony's own highlights.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AAntCharacter* A = It->IsValid() ? Cast<AAntCharacter>((*It)->GetPawn()) : nullptr;
		if (!A || A->Team != 0 || A->Creature != 0) { continue; }
		FAntRaidPlayerLine& L = LineOf(A);
		L.bCameHome = !bWiped && !A->bDead && IsHome(A);
		if (L.bCameHome) { L.Feats.AddUnique(TEXT("survived")); }
	}
	const FAntRaidPlayerLine* Best = nullptr;
	for (const TPair<FString, FAntRaidPlayerLine>& P : RaidLines)
	{
		if (P.Value.Feats.Contains(TEXT("cake_home"))) { R.Heroes.Add(FString::Printf(TEXT("ПОДВИГ|%s донёс торт до входа"), *P.Value.Name)); }
		if (P.Value.Rescues > 0) { R.Heroes.Add(FString::Printf(TEXT("СПАСАТЕЛЬ|%s вынес тело товарища"), *P.Value.Name)); }
		if (P.Value.FoodTotal() > 0 && (!Best || P.Value.FoodTotal() > Best->FoodTotal())) { Best = &P.Value; }
	}
	if (Best && RaidLines.Num() > 1) { R.Heroes.Add(FString::Printf(TEXT("ЛУЧШИЙ НОСИЛЬЩИК|%s: %d еды"), *Best->Name, Best->FoodTotal())); }
	while (R.Heroes.Num() > 4) { R.Heroes.RemoveAt(R.Heroes.Num() - 1); }

	// The chronicle.
	const TCHAR* How_ = bWiped ? TEXT("отряд погиб, красные растащили часть добычи.") : bCakeHome ? TEXT("торт дома!")
		: How == AntRaidEnd::Timeout ? TEXT("время вышло, без торта.") : TEXT("без торта.");
	R.Chronicle = FString::Printf(TEXT("Рейд %d («%s»): %s +%d еды, +%d молочка%s. Королева %s"), R.RaidNo, TierDef().Name, How_, R.Kept, R.Jelly,
		R.Deaths > 0 ? *FString::Printf(TEXT(", потерь %d"), R.Deaths) : TEXT(""), AntNest::MoodName(Nest.Mood));
	AntColony::Chronicle(Nest, R.Chronicle);
	for (uint8 Id : R.NewTrophies) { AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: в трофейной новый трофей — %s"), R.RaidNo, AntColony::Trophy(Id).Name)); }
	if (R.NewTier > 0) { AntColony::Chronicle(Nest, FString::Printf(TEXT("Рейд %d: открыта угроза «%s»"), R.RaidNo, AntTier::Def(R.NewTier).Name)); }

	// The summary on the meta bus, for the personal layer (ranks, medals, veterans).
	FAntRaidSummary Sum;
	Sum.RaidNo = R.RaidNo;
	Sum.Tier = R.Tier;
	Sum.End = How;
	Sum.bCakeHome = bCakeHome;
	Sum.Kept = R.Kept;
	Sum.Jelly = R.Jelly;
	Sum.Larvae = Nest.Larvae;
	for (int32 k = 0; k < AntFoodKind::Count && k < R.Kinds.Num(); ++k) { Sum.Food[k] = R.Kinds[k]; }
	for (uint8 Id : R.NewTrophies) { Sum.Trophies.Add(FName(AntColony::Trophy(Id).Id)); }
	for (const TPair<FString, FAntRaidPlayerLine>& P : RaidLines)
	{
		FAntRaidPlayerLine L = P.Value;
		if (R.ContractsDone > 0 && L.bCameHome) { L.Feats.AddUnique(TEXT("contract")); }
		Sum.Players.Add(L);
	}
	AntMetaBus::LastSummary() = Sum;
	AntMetaBus::OnRaidSummary().Broadcast(Sum);
	++ColonyShow;
	UE_LOG(LogTemp, Display, TEXT("ANTCOLONY raid %d: kinds %d/%d/%d/%d, fungus +%d (+%d jelly), trophies +%d, heroes %d, players %d -> %s"), R.RaidNo,
		R.Kinds.IsValidIndex(3) ? R.Kinds[0] : 0, R.Kinds.IsValidIndex(3) ? R.Kinds[1] : 0, R.Kinds.IsValidIndex(3) ? R.Kinds[2] : 0, R.Kinds.IsValidIndex(3) ? R.Kinds[3] : 0,
		R.Grown, R.GrownJelly, R.NewTrophies.Num(), R.Heroes.Num(), Sum.Players.Num(), *AntColony::Info(Nest));
}

// ---------------------------------------------------------------------------------------------
// Dev / admin: colony ...
// ---------------------------------------------------------------------------------------------

bool AAntRaid::ColonyCommand(const TArray<FString>& A)
{
	const FString Sub = A.IsValidIndex(1) ? A[1].ToLower() : FString();
	const FString Arg = A.IsValidIndex(2) ? A[2].ToLower() : FString();
	auto Num = [&A](int32 i, int32 Def) { return A.IsValidIndex(i) ? FCString::Atoi(*A[i]) : Def; };
	auto ChamberOf = [](const FString& S) -> int32
	{
		if (S.IsNumeric()) { return FMath::Clamp(FCString::Atoi(*S), 0, AntColony::Count - 1); }
		for (uint8 c = 0; c < AntColony::Count; ++c) { if (S.Equals(AntColony::Def(c).Id, ESearchCase::IgnoreCase)) { return c; } }
		return -1;
	};
	AAntPlayerController* PC = Cast<AAntPlayerController>(GetWorld()->GetFirstPlayerController());
	bool bSave = true;
	if (Sub == TEXT("info"))
	{
		UE_LOG(LogTemp, Display, TEXT("ANTCOLONY info: %s | view: %s"), *AntColony::Info(Nest), *AntColonyView::DevStats());
		for (const FString& L : Nest.Chronicle) { UE_LOG(LogTemp, Display, TEXT("ANTCOLONY chronicle: %s"), *L); }
		bSave = false;
	}
	else if (Sub == TEXT("give"))
	{
		const int32 N = Num(3, 0);
		static const TCHAR* Kinds[] = { TEXT("sugar"), TEXT("protein"), TEXT("fat"), TEXT("crumbs") };
		bool bKind = false;
		for (int32 k = 0; k < AntFoodKind::Count; ++k) { if (Arg == Kinds[k]) { AntColony::AddFood(Nest, (uint8)k, N); bKind = true; } }
		if (bKind) {}
		else if (Arg == TEXT("food")) { for (int32 k = 0; k < AntFoodKind::Count; ++k) { AntColony::AddFood(Nest, (uint8)k, N / 4); } }
		else if (Arg == TEXT("jelly")) { Nest.Jelly = FMath::Max(0, Nest.Jelly + N); }
		else if (Arg == TEXT("larvae")) { Nest.Larvae = FMath::Clamp(Nest.Larvae + N, 0, AntNest::MaxLarvae); }
		else if (Arg == TEXT("workers")) { Nest.Workers = FMath::Clamp(Nest.Workers + N, 0, AntNest::MaxWorkers); }
		else if (Arg == TEXT("trophy"))
		{
			const FString W = A.IsValidIndex(3) ? A[3].ToLower() : FString(TEXT("random"));
			if (W == TEXT("all")) { for (int32 i = 0; i < AntColony::TrophyCount(); ++i) { AntColony::GiveTrophy(Nest, i); } }
			else if (W == TEXT("none")) { Nest.Trophies.Reset(); }
			else if (W == TEXT("random"))
			{
				TArray<int32> Free;
				for (int32 i = 0; i < AntColony::TrophyCount(); ++i) { if (!AntColony::HasTrophy(Nest, i)) { Free.Add(i); } }
				if (Free.Num() > 0) { AntColony::GiveTrophy(Nest, Free[FMath::RandRange(0, Free.Num() - 1)]); }
			}
			else { AntColony::GiveTrophy(Nest, FCString::Atoi(*W)); }
		}
		else { return false; }
	}
	else if (Sub == TEXT("dig"))
	{
		if (Arg == TEXT("all")) { Nest.Dug = 7; }
		else if (Arg == TEXT("none")) { Nest.Dug = 0; }
		else if (Arg == TEXT("buy") && PC) { PlayerCommand(PC, AntRaidCmd::Dig, Num(3, 0), 0); bSave = false; }
		else { Nest.Dug |= (uint8)(1 << FMath::Clamp(FCString::Atoi(*Arg), 0, 2)); }
	}
	else if (Sub == TEXT("set") || Sub == TEXT("setall"))
	{
		// set <chamber> <tier> | setall <tier>: built for free (the tunnels too)
		const int32 T = FMath::Clamp(Sub == TEXT("setall") ? Num(2, 1) : Num(3, 1), 0, AntColony::MaxTier);
		if (Nest.Up.Num() < AntUp::Count) { Nest.Up.SetNumZeroed(AntUp::Count); }
		if (Nest.Own.Num() < AntColony::OwnCount) { Nest.Own.SetNumZeroed(AntColony::OwnCount); }
		for (uint8 c = 1; c < AntColony::Count; ++c)
		{
			if (Sub == TEXT("set") && c != ChamberOf(Arg)) { continue; }
			const AntColony::FChamberDef& D = AntColony::Def(c);
			if (D.Branch >= 0) { Nest.Up[D.Branch] = (uint8)T; }
			if (D.Own >= 0) { Nest.Own[D.Own] = (uint8)T; }
			if (T > 0 && D.Dig >= 0) { Nest.Dug |= (uint8)(1 << D.Dig); }
		}
		if (Sub == TEXT("setall") && T == 0) { Nest.Dug = 0; }
	}
	else if (Sub == TEXT("up") && PC) { PlayerCommand(PC, AntRaidCmd::Chamber, ChamberOf(Arg), 0); bSave = false; }
	else if (Sub == TEXT("mood")) { Nest.Mood = FMath::Clamp(Num(2, 50), 0, 100); }
	else if (Sub == TEXT("sim"))
	{
		// sim N [nobuild]: N raids played on paper, then the post-raid show of the last one
		const int32 N = FMath::Clamp(Num(2, 1), 1, 40);
		for (int32 i = 0; i < N; ++i)
		{
			FAntRaidResult R;
			AntColony::SimulateRaid(Nest, R, Nest.Raids * 7919 + 13, !A.Contains(TEXT("nobuild")));
			Result = R;
		}
		RaidNo = Nest.Raids;
		Tier = (uint8)FMath::Clamp(Nest.Tier, 1, Nest.MaxTier);
		++ColonyShow;
	}
	else if (Sub == TEXT("show")) { ++ColonyShow; bSave = false; }
	else if (Sub == TEXT("showat")) { AntColonyView::DevShowAt(FCString::Atof(*Arg)); bSave = false; }
	else if (Sub == TEXT("reset"))
	{
		AntNest::Reset();
		AntNest::Fresh(Nest);
		AntColony::Chronicle(Nest, TEXT("Основана Великая Колония Щели. Королева-Мать заняла земляной трон"));
		Result = FAntRaidResult();
		RaidNo = 0;
		Tier = 1;
	}
	else if (Sub == TEXT("v1save"))
	{
		// tests of the migration: the colony as an old save; the next start of the game reads and migrates it
		UE_LOG(LogTemp, Display, TEXT("ANTCOLONY wrote a v1 save: %s"), AntNest::SaveAsV1(Nest) ? TEXT("ok") : TEXT("FAILED"));
		return true;
	}
	else if (Sub == TEXT("reload"))
	{
		FString Why;
		AntNest::Load(Nest, &Why);
		UE_LOG(LogTemp, Display, TEXT("ANTCOLONY reloaded (%s): %s"), *Why, *AntColony::Info(Nest));
	}
	// this machine's screen
	else if (Sub == TEXT("view")) { AntColonyView::SetOn(true); bSave = false; }
	else if (Sub == TEXT("sheet")) { AntColonyView::SetOn(false); bSave = false; }
	else if (Sub == TEXT("open")) { AntColonyView::DevOpen(ChamberOf(Arg)); bSave = false; }
	else if (Sub == TEXT("close")) { AntColonyView::DevOpen(-1); bSave = false; }
	else if (Sub == TEXT("zoom")) { AntColonyView::DevZoom(FCString::Atof(*Arg), A.IsValidIndex(3) ? FCString::Atof(*A[3]) : 0.5f, A.IsValidIndex(4) ? FCString::Atof(*A[4]) : 0.5f); bSave = false; }
	else if (Sub == TEXT("sleep")) { AntColonyView::DevSleep(); bSave = false; }
	else { return false; }
	if (bSave)
	{
		AntColony::Sync(Nest);
		SaveNest();
		CountStartCost();
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// The colony for other systems (tutorial, events): AntColony::GiveGift / ReplayShow
// ---------------------------------------------------------------------------------------------

namespace AntColony
{

bool GiveGift(const UObject* WorldContext, const FAntColonyGift& Gift)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	AAntRaid* Raid = W ? AAntRaid::Get(W) : nullptr;
	if (!Raid || !Raid->HasAuthority())
	{
		return false;
	}
	FAntNestState& N = Raid->Nest;
	int32 Total = 0;
	for (int32 k = 0; k < AntFoodKind::Count; ++k)
	{
		AddFood(N, (uint8)k, FMath::Max(0, Gift.Food[k]));
		Total += FMath::Max(0, Gift.Food[k]);
	}
	N.Jelly = FMath::Max(0, N.Jelly + Gift.Jelly);
	N.Larvae = FMath::Clamp(N.Larvae + Gift.Larvae, 0, AntNest::MaxLarvae);
	N.Workers = FMath::Clamp(N.Workers + Gift.Workers, 0, AntNest::MaxWorkers);
	if (!Gift.Chronicle.IsEmpty()) { Chronicle(N, Gift.Chronicle); }
	Raid->SaveNest();
	if (!Gift.Announce.IsEmpty())
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			if (AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get())) { PC->ClientToast(Gift.Announce); }
		}
	}
	UE_LOG(LogTemp, Display, TEXT("ANTCOLONY gift: food %d (%d/%d/%d/%d), jelly %d, larvae %d, workers %d -> %s"), Total, Gift.Food[0], Gift.Food[1], Gift.Food[2], Gift.Food[3],
		Gift.Jelly, Gift.Larvae, Gift.Workers, *Info(N));
	return true;
}

bool ReplayShow(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	AAntRaid* Raid = W ? AAntRaid::Get(W) : nullptr;
	if (!Raid || !Raid->HasAuthority() || Raid->Result.RaidNo <= 0)
	{
		return false;
	}
	++Raid->ColonyShow;
	return true;
}

}   // namespace AntColony
