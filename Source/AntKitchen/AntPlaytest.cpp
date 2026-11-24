#include "AntPlaytest.h"
#include "PlaytestKitSubsystem.h"
#include "AntRaid.h"
#include "AntCharacter.h"
#include "AntKitchenGameMode.h"
#include "AntGameState.h"
#include "AntCarry.h"
#include "AntMetaBus.h"
#include "AntFighter.h"
#include "AntTutorial.h"
#include "AntUiMenu.h"
#include "AntUiSettings.h"
#include "AntAdmin.h"
#include "KitchenLoot.h"
#include "HumanCharacter.h"
#include "Critters.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"

// ant.ptk.survey_every N: the post-session survey after every Nth raid of the session (0 = never; the quick "how fun was it" question stays).
static TAutoConsoleVariable<int32> CVarPtkSurveyEvery(TEXT("ant.ptk.survey_every"), 2, TEXT("PlaytestKit: show the post-session survey after every Nth raid (0 = off)"));

namespace
{
	// ---------------------------------------------------------------- cached world pointers for the cheap context provider
	TWeakObjectPtr<UWorld> GWorld_;
	TWeakObjectPtr<AAntRaid> GRaid;
	TWeakObjectPtr<AAntAdminState> GAdmin;
	double GFirstSeen = 0.0;
	bool bAttached = false;
	bool bQuitAsked = false;
	uint8 GLastPhase = 255;
	int32 GLastRaidNo = -1;
	double GResultsAt = 0.0, GRaidStartedAt = 0.0, GDebriefAt = 0.0;
	int32 GRaidsSeen = 0;
	bool bResultsLogged = false;
	FTSTicker::FDelegateHandle GTick;
	FDelegateHandle GSummaryH, GCarryH;

	const TCHAR* ScreenName(uint8 S)
	{
		static const TCHAR* N[] = { TEXT("boot"), TEXT("main"), TEXT("coop"), TEXT("colony"), TEXT("lobby"), TEXT("settings"), TEXT("credits"), TEXT("pause"), TEXT("loading"), TEXT("training"), TEXT("demo_end") };
		return S < UE_ARRAY_COUNT(N) ? N[S] : TEXT("unknown");
	}

	UWorld* FindGameWorld()
	{
		if (GEngine)
		{
			for (const FWorldContext& C : GEngine->GetWorldContexts())
			{
				if (C.WorldType == EWorldType::Game && C.World()) { return C.World(); }
			}
		}
		return nullptr;
	}

	void ProvideContext(FPTKContext& C)
	{
		const UWorld* W = GWorld_.Get();
		if (!W) { return; }
		if (const AAntRaid* R = GRaid.Get())
		{
			C.RaidNo = R->RaidNo;
			C.Tier = R->Tier;
			C.Mode = AntTut::Active(W) ? TEXT("tutorial") : R->Phase == AntRaidPhase::Raid ? TEXT("raid") : R->Phase == AntRaidPhase::Results ? TEXT("results") : TEXT("nest");
		}
		else
		{
			C.Mode = AntTut::Active(W) ? TEXT("tutorial") : TEXT("classic");
		}
		if (const AAntGameState* GS = W->GetGameState<AAntGameState>())
		{
			if (GS->LevelSeedKey >= 0) { C.Seed = GS->LevelSeed(); }
		}
		if (const AAntAdminState* A = GAdmin.Get())
		{
			static const TCHAR* Tod[] = { TEXT("day"), TEXT("evening"), TEXT("night") };
			if (A->Tod >= 0 && A->Tod < 3) { C.TimeOfDay = Tod[A->Tod]; }
		}
	}

	// ---------------------------------------------------------------- the game's own log lines -> events
	struct FLogRule { const TCHAR* Prefix; const TCHAR* Must; const TCHAR* Name; const TCHAR* Cat; };
	static const FLogRule GRules[] = {
		{ TEXT("ANTSNOW offer"), nullptr, TEXT("snowball_offer"), TEXT("snowball") },
		{ TEXT("ANTSNOW pick"), nullptr, TEXT("snowball_pick"), TEXT("snowball") },
		{ TEXT("ANTSNOW auto pick"), nullptr, TEXT("snowball_autopick"), TEXT("snowball") },
		{ TEXT("ANTSNOW threshold"), nullptr, TEXT("snowball_level"), TEXT("snowball") },
		{ TEXT("ANTSNOW begin"), nullptr, TEXT("snowball_begin"), TEXT("snowball") },
		{ TEXT("ANTSNOW end"), nullptr, TEXT("snowball_end"), TEXT("snowball") },
		{ TEXT("ANTCARD picked"), nullptr, TEXT("card_pick"), TEXT("run") },
		{ TEXT("ANTCHAIN C"), TEXT(" +"), TEXT("chain_step"), TEXT("chain") },
		{ TEXT("ANTREACT SHORT"), nullptr, TEXT("power_short"), TEXT("chain") },
		{ TEXT("ANTREACT BLACKOUT"), nullptr, TEXT("power_blackout"), TEXT("chain") },
		{ TEXT("ANTREACT power back"), nullptr, TEXT("power_back"), TEXT("chain") },
		{ TEXT("ANTREACT water reaches"), nullptr, TEXT("water_power"), TEXT("chain") },
		{ TEXT("ANTREACT the Dad stepped"), nullptr, TEXT("human_react"), TEXT("human") },
		{ TEXT("ANTFALL the Human slips"), nullptr, TEXT("human_slip"), TEXT("human") },
		{ TEXT("ANTFALL the Human hits the floor"), nullptr, TEXT("human_fall"), TEXT("human") },
		{ TEXT("ANTFALL"), TEXT("squashed under"), TEXT("human_fall_squash"), TEXT("human") },
		{ TEXT("ANTSTUN"), nullptr, TEXT("human_stunned"), TEXT("human") },
		{ TEXT("ANTHEAVY topple"), nullptr, TEXT("human_topple"), TEXT("human") },
		{ TEXT("ANTHEAVY jump-stomp lands"), nullptr, TEXT("human_stomp"), TEXT("human") },
		{ TEXT("ANTHUMAN goal"), nullptr, TEXT("human_goal"), TEXT("human") },
		{ TEXT("ANTWRECK broke"), nullptr, TEXT("wreck_break"), TEXT("wreck") },
		{ TEXT("ANTWRECK hit"), nullptr, TEXT("wreck_hit"), TEXT("wreck") },
		{ TEXT("ANTWRECK"), TEXT(": stage "), TEXT("wreck_stage"), TEXT("wreck") },
		{ TEXT("ANTFLUID"), TEXT("fell into"), TEXT("fluid_fall"), TEXT("water") },
		{ TEXT("ANTFLUID"), TEXT("oil film"), TEXT("fluid_oil"), TEXT("water") },
		{ TEXT("ANTSPILL"), TEXT("tipped over by"), TEXT("spill_tipped"), TEXT("water") },
		{ TEXT("ANTMESS stain"), nullptr, TEXT("mess_stain"), TEXT("mess") },
		{ TEXT("ANTMESS burst"), nullptr, TEXT("mess_burst"), TEXT("mess") },
		{ TEXT("ANTMESS clean"), nullptr, TEXT("mess_clean"), TEXT("mess") },
		{ TEXT("ANTINSIDE"), TEXT(" enters "), TEXT("inside_enter"), TEXT("inside") },
		{ TEXT("ANTINSIDE"), TEXT("is caught"), TEXT("inside_caught"), TEXT("inside") },
		{ TEXT("ANTINSIDE"), TEXT(" steals "), TEXT("inside_steal"), TEXT("inside") },
		{ TEXT("ANTINSIDE"), TEXT("ants"), TEXT("inside_shaken"), TEXT("inside") },
		{ TEXT("ANTCOLONY built"), nullptr, TEXT("colony_build"), TEXT("colony") },
		{ TEXT("ANTCOLONY dug"), nullptr, TEXT("colony_dig"), TEXT("colony") },
		{ TEXT("ANTVET FALLEN"), nullptr, TEXT("vet_fallen"), TEXT("fighter") },
		{ TEXT("ANTVET LOST"), nullptr, TEXT("vet_lost"), TEXT("fighter") },
		{ TEXT("ANTVET new veteran"), nullptr, TEXT("vet_new"), TEXT("fighter") },
		{ TEXT("ANTVET"), TEXT("came home"), TEXT("vet_home"), TEXT("fighter") },
		{ TEXT("ANTFIGHTER"), TEXT("medal"), TEXT("fighter_medal"), TEXT("fighter") },
		{ TEXT("ANTFIGHTER"), TEXT("rank"), TEXT("fighter_rank"), TEXT("fighter") },
		{ TEXT("ANTVERSUS start"), nullptr, TEXT("versus_start"), TEXT("versus") },
		{ TEXT("ANTVERSUS end"), nullptr, TEXT("versus_end"), TEXT("versus") },
		{ TEXT("ANTADMIN"), nullptr, TEXT("admin_cmd"), TEXT("admin") },
		{ TEXT("ANTSQUAD"), TEXT(" died"), TEXT("squad_died"), TEXT("squad") },
		{ TEXT("ANTSQUAD order"), nullptr, TEXT("squad_order"), TEXT("squad") },
		{ TEXT("ANTCAT swipe landed"), nullptr, TEXT("cat_swipe"), TEXT("creature") },
		{ TEXT("ANTCAT bitten"), nullptr, TEXT("cat_bitten"), TEXT("creature") },
		{ TEXT("ANTDOG bitten"), nullptr, TEXT("dog_bitten"), TEXT("creature") },
		{ TEXT("ANTDOG ate"), nullptr, TEXT("dog_ate"), TEXT("creature") },
		{ TEXT("ANTKID"), nullptr, TEXT("kid_event"), TEXT("creature") },
		{ TEXT("ANTDOOR opened"), nullptr, TEXT("door_open"), TEXT("gadget") },
		{ TEXT("ANTLIFE vacuum out"), nullptr, TEXT("life_vacuum"), TEXT("life") },
		{ TEXT("ANTLIFE dishwasher cycle starts"), nullptr, TEXT("life_dishwasher"), TEXT("life") },
		{ TEXT("ANTCLAN found the cake"), nullptr, TEXT("clan_found_cake"), TEXT("clan") },
		{ TEXT("ANTCLAN ambush"), nullptr, TEXT("clan_ambush"), TEXT("clan") },
		{ TEXT("ANTCLOTH"), TEXT("bitten by"), TEXT("cloth_bitten"), TEXT("cloth") },
		{ TEXT("ANTCLOTH"), TEXT("yanked"), TEXT("cloth_yank"), TEXT("cloth") },
	};

	class FAntLogBridge : public FOutputDevice
	{
	public:
		FCriticalSection Lock;
		TArray<FString> Queue;
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category != NAME_None && Category != FName(TEXT("LogTemp"))) { return; }
			if (V[0] != 'A' || V[1] != 'N' || V[2] != 'T') { return; }
			FScopeLock L(&Lock);
			if (Queue.Num() < 400) { Queue.Add(FString(V).Left(260)); }
		}
	};
	FAntLogBridge* GBridge = nullptr;
	TMap<FString, int32> GTagCounts;
	TMap<FName, TPair<double, int32>> GBucket;   // per event name: window start, count
	double GNextCounts = 0.0;

	void DrainBridge(UPlaytestKitSubsystem* K)
	{
		if (!GBridge) { return; }
		TArray<FString> Q;
		{
			FScopeLock L(&GBridge->Lock);
			Q = MoveTemp(GBridge->Queue);
		}
		const double Now = FPlatformTime::Seconds();
		for (const FString& Line : Q)
		{
			FString Tag;
			int32 Sp;
			if (Line.FindChar(' ', Sp)) { Tag = Line.Left(Sp); } else { Tag = Line; }
			GTagCounts.FindOrAdd(Tag)++;
			for (const FLogRule& R : GRules)
			{
				if (!Line.StartsWith(R.Prefix) || (R.Must && !Line.Contains(R.Must))) { continue; }
				TPair<double, int32>& B = GBucket.FindOrAdd(FName(R.Name), TPair<double, int32>(Now, 0));
				if (Now - B.Key > 1.0) { B.Key = Now; B.Value = 0; }
				if (++B.Value > 10) { break; }   // at most 10 per second per event name
				FPTKProps P;
				P.S("tag", Tag).S("msg", Line.Mid(FMath::Min(Line.Len(), Tag.Len() + 1)).Left(200));
				if (FCString::Strcmp(R.Name, TEXT("chain_step")) == 0) { P.S("chain", Tag.Len() > 7 ? Tag.Mid(7) : Tag); }
				K->Track(R.Name, P, R.Cat);
				break;
			}
		}
		if (Now >= GNextCounts)
		{
			GNextCounts = Now + 60.0;
			if (GTagCounts.Num())
			{
				FString J;
				for (const TPair<FString, int32>& KV : GTagCounts) { J += (J.IsEmpty() ? TEXT("") : TEXT(",")) + FString::Printf(TEXT("\"%s\":%d"), *KV.Key, KV.Value); }
				K->Track(TEXT("game_log_counts"), FPTKProps().Raw("tags", TEXT("{") + J + TEXT("}")), TEXT("custom"));
				GTagCounts.Reset();
			}
		}
	}

	// ---------------------------------------------------------------- delegates
	void OnCarry(AKitchenLoot* L, uint8 Kind, uint8 Team)
	{
		UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
		if (!K || !L || Kind > AntCarryEvent::Snatched) { return; }
		static const TCHAR* N[] = { TEXT("delivered"), TEXT("dropped"), TEXT("broken"), TEXT("snatched") };
		K->Track(TEXT("carry_event"), FPTKProps().S("kind", N[Kind]).S("loot", L->LootName).I("value", L->Value).I("team", Team).V3("at", L->GetActorLocation()), TEXT("carry"));
		if (Kind == AntCarryEvent::Delivered)
		{
			K->Track(TEXT("economy_source"), FPTKProps().S("item", L->LootName).N("amount", L->Value).S("src", "carry_delivered"), TEXT("economy"));
		}
	}

	void OnSummary(const FAntRaidSummary& S)
	{
		UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
		if (!K) { return; }
		static const TCHAR* Kind[] = { TEXT("sugar"), TEXT("protein"), TEXT("fat"), TEXT("crumbs") };
		FString Pl;
		for (const FAntRaidPlayerLine& L : S.Players)
		{
			FString Feats;
			for (const FName& F : L.Feats) { Feats += (Feats.IsEmpty() ? TEXT("\"") : TEXT(",\"")) + F.ToString() + TEXT("\""); }
			Pl += FString::Printf(TEXT("%s{\"food\":%d,\"deaths\":%d,\"rescues\":%d,\"home\":%s,\"feats\":[%s]}"), Pl.IsEmpty() ? TEXT("") : TEXT(","), L.FoodTotal(), L.Deaths, L.Rescues, L.bCameHome ? TEXT("true") : TEXT("false"), *Feats);
		}
		FString Troph;
		for (const FName& T : S.Trophies) { Troph += (Troph.IsEmpty() ? TEXT("\"") : TEXT(",\"")) + T.ToString() + TEXT("\""); }
		K->Track(TEXT("raid_summary"), FPTKProps().I("raid_no", S.RaidNo).I("tier", S.Tier).I("end", S.End).B("cake_home", S.bCakeHome).I("kept", S.Kept).I("jelly", S.Jelly).I("larvae", S.Larvae)
			.I("sugar", S.Food[0]).I("protein", S.Food[1]).I("fat", S.Food[2]).I("crumbs", S.Food[3]).I("players", S.Players.Num())
			.Raw("players_lines", TEXT("[") + Pl + TEXT("]")).Raw("trophies", TEXT("[") + Troph + TEXT("]")), TEXT("raid"));
		for (int32 i = 0; i < AntFoodKind::Count; ++i)
		{
			if (S.Food[i] > 0) { K->Track(TEXT("economy_source"), FPTKProps().S("item", Kind[i]).N("amount", S.Food[i]).S("src", "raid_haul").I("raid_no", S.RaidNo), TEXT("economy")); }
		}
	}

	// ---------------------------------------------------------------- the 0.5 s watcher: raid phases, survey prompts
	bool Watch(float)
	{
		UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
		if (!K) { return true; }
		const double Now = FPlatformTime::Seconds();
		if (!bAttached)
		{
			bAttached = true;
			GFirstSeen = Now;
			K->SetContextProvider(FPTKContextProvider::CreateStatic(&ProvideContext));
			if (!GBridge) { GBridge = new FAntLogBridge(); GLog->AddOutputDevice(GBridge); }
			GSummaryH = AntMetaBus::OnRaidSummary().AddStatic(&OnSummary);
			GCarryH = AntCarry::Events().AddStatic(&OnCarry);
		}
		UWorld* W = FindGameWorld();
		if (W != GWorld_.Get())
		{
			GWorld_ = W;
			GRaid.Reset(); GAdmin.Reset();
			GLastPhase = 255;
		}
		if (W)
		{
			if (!GRaid.IsValid()) { GRaid = AAntRaid::Get(W); }
			if (!GAdmin.IsValid()) { GAdmin = AAntAdminState::Get(W, false); }
		}
		DrainBridge(K);
		AAntRaid* R = GRaid.Get();
		if (!R || !K->IsActive()) { return true; }
		if (R->Phase != GLastPhase)
		{
			const uint8 Prev = GLastPhase;
			GLastPhase = R->Phase;
			if (R->Phase == AntRaidPhase::Raid)
			{
				GRaidStartedAt = Now;
				bResultsLogged = false;
				GLastRaidNo = R->RaidNo;
				K->Track(TEXT("raid_start"), FPTKProps().I("raid_no", R->RaidNo).I("tier", R->Tier).I("larvae", R->Nest.Larvae).I("workers", R->Nest.Workers).I("raids_done", R->Nest.Raids), TEXT("raid"));
				K->Track(TEXT("objective_start"), FPTKProps().S("id", "raid_cake").I("raid_no", R->RaidNo), TEXT("progress"));
			}
			else if (R->Phase == AntRaidPhase::Results && Prev == AntRaidPhase::Raid)
			{
				GResultsAt = Now + 1.2;   // the replicated Result arrives a moment after the phase
				bResultsLogged = false;
			}
			else if (R->Phase == AntRaidPhase::Nest && Prev == AntRaidPhase::Results)
			{
				++GRaidsSeen;
				GDebriefAt = Now + 1.5;
			}
		}
		if (!bResultsLogged && R->Phase == AntRaidPhase::Results && GResultsAt > 0.0 && Now >= GResultsAt)
		{
			bResultsLogged = true;
			const FAntRaidResult& Rs = R->Result;
			const bool bWon = Rs.End == AntRaidEnd::Left;
			FPTKProps P;
			P.I("raid_no", Rs.RaidNo).I("tier", Rs.Tier).I("end", Rs.End).S("end_name", Rs.End == AntRaidEnd::Left ? TEXT("left") : Rs.End == AntRaidEnd::Timeout ? TEXT("timeout") : TEXT("wiped"))
				.N("seconds", GRaidStartedAt > 0.0 ? Now - GRaidStartedAt - 1.2 : 0.0).I("banked", Rs.Banked).I("kept", Rs.Kept).I("ask", Rs.Ask).I("jelly", Rs.Jelly).I("cake", Rs.Cake)
				.I("deaths", Rs.Deaths).I("lost", Rs.Lost).I("squad_out", Rs.SquadOut).I("squad_back", Rs.SquadBack).I("contracts_done", Rs.ContractsDone).I("contracts_taken", Rs.ContractsTaken)
				.I("mood_before", Rs.MoodBefore).I("mood_after", Rs.MoodAfter).I("larvae_laid", Rs.LarvaeLaid).I("workers_hatched", Rs.WorkersHatched).I("queen_ate", Rs.QueenAte).I("larvae_left", Rs.LarvaeLeft)
				.I("new_tier", Rs.NewTier).I("seed", Rs.Seed).B("simulated", Rs.bSimulated);
			K->Track(TEXT("raid_end"), P, TEXT("raid"));
			K->Track(bWon ? TEXT("objective_complete") : TEXT("objective_fail"), FPTKProps().S("id", "raid_cake").I("raid_no", Rs.RaidNo).B("cake_home", Rs.Cake == AntCake::Home).S("reason", bWon ? TEXT("") : Rs.End == AntRaidEnd::Timeout ? TEXT("timeout") : TEXT("wiped"))
				.N("seconds", GRaidStartedAt > 0.0 ? Now - GRaidStartedAt : 0.0), TEXT("progress"));
			if (Rs.Banked > 0) { K->Track(TEXT("progress_step"), FPTKProps().S("id", "banked").N("value", Rs.Banked), TEXT("progress")); }
			if (Rs.NewTier > 0) { K->Track(TEXT("progress_step"), FPTKProps().S("id", "tier_unlocked").N("value", Rs.NewTier), TEXT("progress")); }
			const FAntProfile& Pf = AntFighter::Profile();
			K->Track(TEXT("fighter_profile"), FPTKProps().I("xp", Pf.Xp).I("raids", Pf.Raids).I("rank", Pf.Rank()).I("kills", Pf.TotalKills).I("rescues", Pf.TotalRescues).I("food_total", Pf.TotalFood), TEXT("fighter"));
		}
		if (GDebriefAt > 0.0 && Now >= GDebriefAt)
		{
			GDebriefAt = 0.0;
			if (!AntTut::Active(W) && !FParse::Param(FCommandLine::Get(), TEXT("PTKNoAutoSurvey")) && !K->IsSurveyOpen())
			{
				const int32 Every = CVarPtkSurveyEvery.GetValueOnGameThread();
				if (Every > 0 && GRaidsSeen % Every == 0) { K->ShowPostSurvey(TEXT("raid_debrief")); }
				else { K->AskOne(TEXT("raid_fun"), TEXT("Рейд: насколько было весело?  (1 - скучно, 5 - супер)"), 1, 5, 20.f); }
			}
		}
		return true;
	}

	struct FAutoStart
	{
		FAutoStart()
		{
			FCoreDelegates::GetOnPostEngineInit().AddLambda([]() { GTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Watch), 0.5f); });
		}
	};
	static FAutoStart GAutoStart;
}

// ------------------------------------------------------------------------------------------------- hooks called from the game

void AntPlaytest::OnDeath(AAntCharacter* Ant, AController* Killer, AActor* Causer)
{
	UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
	if (!K || !Ant) { return; }
	const AHumanCharacter* Human = Cast<AHumanCharacter>(Causer);
	const UWorld* W = Ant->GetWorld();
	const bool bHazard = W && (W->GetTimeSeconds() - Ant->LastHazardTime) < 3.f && !Ant->LastHazard.IsEmpty();
	FString Cause = Human ? (Human->bSpraying ? TEXT("human_spray") : TEXT("human_stomp")) : Cast<ACritter>(Causer) ? TEXT("critter") : bHazard ? TEXT("hazard") : Causer ? Causer->GetClass()->GetName() : TEXT("unknown");
	const APlayerState* PS = Ant->GetPlayerState();
	FPTKProps P;
	P.S("cause", Cause).S("player", PS ? FString::Printf(TEXT("P%d"), PS->GetPlayerId()) : TEXT("bot")).B("human_player", Ant->IsPlayerControlled()).B("local", Ant->IsLocallyControlled())
		.I("team", Ant->Team).I("creature", Ant->Creature).I("death_kind", Ant->DeathKind).V3("at", Ant->GetActorLocation());
	if (bHazard) { P.S("hazard", Ant->LastHazard); }
	if (Killer && Killer->GetPawn()) { P.S("killer", Killer->GetPawn()->GetClass()->GetName()); }
	K->Track(TEXT("death"), P, TEXT("combat"));
}

void AntPlaytest::OnTutorialTelemetry(const TCHAR* What, const TCHAR* BeatId, int32 BeatNo, float Seconds, float Par, int32 Retries, int32 Assists, int32 Players)
{
	UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
	if (!K) { return; }
	const FString W(What);
	const TCHAR* Name = W == TEXT("start") ? TEXT("tutorial_step_start") : W == TEXT("stage") ? TEXT("tutorial_stage") : (W == TEXT("done")) ? TEXT("tutorial_step_complete") : W == TEXT("skipped") ? TEXT("tutorial_step_skip")
		: W == TEXT("fail") ? TEXT("tutorial_step_fail") : W == TEXT("assist") ? TEXT("tutorial_assist") : W == TEXT("death") ? TEXT("tutorial_death") : W == TEXT("finish") ? TEXT("tutorial_finish") : TEXT("tutorial_event");
	K->Track(Name, FPTKProps().S("id", BeatId).I("no", BeatNo).N("seconds", Seconds).N("par", Par).I("retries", Retries).I("assists", Assists).I("players", Players), TEXT("progress"));
	if (W == TEXT("finish")) { K->AskOne(TEXT("tutorial_clear"), TEXT("Тренировка: было ли всё понятно?  (1 - нет, 5 - да)"), 1, 5, 20.f); }
}

void AntPlaytest::OnUiScreen(uint8 From, uint8 To)
{
	static double GLastScreenAt = 0.0;
	UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
	if (!K || From == To) { return; }
	const double Now = FPlatformTime::Seconds();
	K->Track(TEXT("ui_screen"), FPTKProps().S("screen", ScreenName(To)).S("from", ScreenName(From)).N("seconds_prev", GLastScreenAt > 0.0 ? Now - GLastScreenAt : 0.0), TEXT("ui"));
	GLastScreenAt = Now;
}

void AntPlaytest::OnSettingsApplied(const FAntUiSettings& O, const FAntUiSettings& N, const FAntGfx& OG, const FAntGfx& NG)
{
	UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
	if (!K) { return; }
	auto Ch = [K](const TCHAR* Key, const FString& From, const FString& To) { if (From != To) { K->Track(TEXT("setting_change"), FPTKProps().S("key", Key).S("from", From).S("to", To), TEXT("ui")); } };
	auto F = [](float V) { return FString::Printf(TEXT("%.2f"), V); };
	auto B = [](bool V) { return FString(V ? TEXT("1") : TEXT("0")); };
	Ch(TEXT("language"), FString::FromInt(O.Language), FString::FromInt(N.Language));
	Ch(TEXT("subtitles"), B(O.bSubtitles), B(N.bSubtitles));
	Ch(TEXT("hud_scale"), F(O.HudScale), F(N.HudScale));
	Ch(TEXT("colorblind"), B(O.bColorblind), B(N.bColorblind));
	Ch(TEXT("mouse_sens"), F(O.MouseSens), F(N.MouseSens));
	Ch(TEXT("invert_y"), B(O.bInvertY), B(N.bInvertY));
	static const TCHAR* VolNames[] = { TEXT("vol_master"), TEXT("vol_music"), TEXT("vol_sfx"), TEXT("vol_voice"), TEXT("vol_ui") };
	for (int32 i = 0; i < AntVol::Count && i < 5; ++i) { Ch(VolNames[i], F(O.Vol[i]), F(N.Vol[i])); }
	Ch(TEXT("dust"), B(O.bDust), B(N.bDust));
	Ch(TEXT("motion_blur"), B(O.bMotionBlur), B(N.bMotionBlur));
	Ch(TEXT("anti_aliasing"), FString::FromInt(O.AntiAliasing), FString::FromInt(N.AntiAliasing));
	Ch(TEXT("gfx_preset"), FString::FromInt(OG.Preset), FString::FromInt(NG.Preset));
	Ch(TEXT("resolution"), FString::Printf(TEXT("%dx%d"), OG.Res.X, OG.Res.Y), FString::Printf(TEXT("%dx%d"), NG.Res.X, NG.Res.Y));
	Ch(TEXT("window_mode"), FString::FromInt(OG.Mode), FString::FromInt(NG.Mode));
	Ch(TEXT("vsync"), B(OG.bVSync), B(NG.bVSync));
	Ch(TEXT("frame_cap"), FString::FromInt(OG.FrameCap), FString::FromInt(NG.FrameCap));
}

bool AntPlaytest::InterceptQuit(APlayerController* PC)
{
	UPlaytestKitSubsystem* K = UPlaytestKitSubsystem::Get();
	if (!K || !K->IsActive() || bQuitAsked || K->IsSurveyOpen() || FParse::Param(FCommandLine::Get(), TEXT("PTKNoAutoSurvey"))) { return false; }
	if (GFirstSeen <= 0.0 || FPlatformTime::Seconds() - GFirstSeen < 120.0) { return false; }   // a short look at the menu is not a session worth a survey
	bQuitAsked = true;
	TWeakObjectPtr<APlayerController> Weak(PC);
	K->ShowPostSurvey(TEXT("quit"), [Weak]()
	{
		if (Weak.IsValid()) { UKismetSystemLibrary::QuitGame(Weak.Get(), Weak.Get(), EQuitPreference::Quit, false); }
		else { FPlatformMisc::RequestExit(false); }
	});
	return true;
}
