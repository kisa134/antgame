#include "AntColonyVote.h"
#include "AntDifficulty.h"
#include "AntRaid.h"
#include "AntNest.h"
#include "AntColony.h"
#include "AntGameState.h"
#include "AntPlayerState.h"
#include "AntPlayerController.h"
#include "AntKitchenGameMode.h"
#include "AntUiStyle.h"
#include "AntUiCanvas.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

using namespace AntUi;

namespace
{
	const TCHAR* VoteSection = TEXT("AntColonyVote");
	TWeakObjectPtr<AAntColonyVote> GVote;
	double GVoteNextFind = 0.0;

	const TCHAR* RomanV(int32 N) { static const TCHAR* R[] = { TEXT("0"), TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV") }; return R[FMath::Clamp(N, 0, 4)]; }

	FString CostText(int32 Food, int32 Jelly)
	{
		return Jelly > 0 ? FString::Printf(TEXT("%d еды + %d мол."), Food, Jelly) : FString::Printf(TEXT("%d еды"), Food);
	}

	bool IsBuildCmd(uint8 Cmd)
	{
		return Cmd == AntRaidCmd::Buy || Cmd == AntRaidCmd::Dig || Cmd == AntRaidCmd::Chamber || Cmd == AntRaidCmd::Larva || Cmd == AntRaidCmd::Workers;
	}

	/** What the proposal is, in words; false (and why) when the colony cannot do it right now. */
	bool Describe(const AAntRaid* Raid, uint8 Cmd, int32 A, FString& Out, FString& Why)
	{
		const FAntNestState& N = Raid->Nest;
		if (Cmd == AntRaidCmd::Dig)
		{
			if (A < 0 || A >= AntColony::DigCount) { Why = TEXT("нет такого хода"); return false; }
			if (!AntColony::CanDig(N, (uint8)A, &Why)) { return false; }
			int32 F = 0, J = 0;
			AntColony::DigCost(N, F, J);
			Out = FString::Printf(TEXT("Прорыть: %s (%s) — %s"), AntColony::Dig((uint8)A).Name, AntColony::Dig((uint8)A).Toward, *CostText(F, J));
			return true;
		}
		if (Cmd == AntRaidCmd::Chamber)
		{
			if (A < 0 || A >= AntColony::Count) { Why = TEXT("нет такого зала"); return false; }
			if (!AntColony::CanUpgrade(N, (uint8)A, &Why)) { return false; }
			int32 F = 0, J = 0, M = 0;
			AntColony::Cost(N, (uint8)A, F, J, M);
			const int32 T = AntColony::TierOf(N, (uint8)A) + 1;
			Out = FString::Printf(TEXT("Построить: %s %s «%s» — %s"), AntColony::Def((uint8)A).Name, RomanV(T), *AntColony::TierName((uint8)A, T), *CostText(F, J));
			return true;
		}
		if (Cmd == AntRaidCmd::Buy)
		{
			if (A < 0 || A >= AntUp::Count) { Why = TEXT("нет такой ветки"); return false; }
			if (!AntNest::CanBuy(N, (uint8)A, &Why)) { return false; }
			const int32 T = N.Tier_((uint8)A) + 1;
			const AntUp::FDef& D = AntUp::Def((uint8)A, T);
			Out = FString::Printf(TEXT("Построить: %s %s «%s» — %s"), AntUp::BranchName((uint8)A), RomanV(T), D.Name, *CostText(D.Food, D.Jelly));
			return true;
		}
		if (Cmd == AntRaidCmd::Larva)
		{
			if (N.Larvae >= AntNest::MaxLarvae) { Why = TEXT("камера полна"); return false; }
			if (N.Food < AntNest::LarvaCost) { Why = FString::Printf(TEXT("на личинку нужно %d еды"), AntNest::LarvaCost); return false; }
			Out = FString::Printf(TEXT("Вырастить личинку — %d еды"), AntNest::LarvaCost);
			return true;
		}
		if (Cmd == AntRaidCmd::Workers)
		{
			if (N.Workers >= AntNest::MaxWorkers) { Why = TEXT("в гнезде нет места"); return false; }
			if (N.Food < AntNest::WorkerPackCost) { Why = FString::Printf(TEXT("на рабочих нужно %d еды"), AntNest::WorkerPackCost); return false; }
			Out = FString::Printf(TEXT("Вырастить %d рабочих — %d еды"), AntNest::WorkerPack, AntNest::WorkerPackCost);
			return true;
		}
		Why = TEXT("это не решают голосованием");
		return false;
	}

	int32 CountPlayers(UWorld* W, AAntPlayerController** OutHost = nullptr)
	{
		int32 N = 0;
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get());
			if (!PC || !PC->GetPlayerState<AAntPlayerState>()) { continue; }
			++N;
			if (OutHost && PC->IsLocalController() && !*OutHost) { *OutHost = PC; }
		}
		return N;
	}

	void SayAll(UWorld* W, const FString& Text, const FLinearColor& Col)
	{
		if (AAntGameState* GS = W ? W->GetGameState<AAntGameState>() : nullptr) { GS->MulticastMessage(Text, Col); }
	}
}

AAntColonyVote::AAntColonyVote()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(10.f);
}

void AAntColonyVote::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntColonyVote, Mode);
	DOREPLIFETIME(AAntColonyVote, bActive);
	DOREPLIFETIME(AAntColonyVote, Seq);
	DOREPLIFETIME(AAntColonyVote, Cmd);
	DOREPLIFETIME(AAntColonyVote, Arg);
	DOREPLIFETIME(AAntColonyVote, What);
	DOREPLIFETIME(AAntColonyVote, By);
	DOREPLIFETIME(AAntColonyVote, Ends);
	DOREPLIFETIME(AAntColonyVote, Yes);
	DOREPLIFETIME(AAntColonyVote, No);
	DOREPLIFETIME(AAntColonyVote, Voters);
	DOREPLIFETIME(AAntColonyVote, LastResult);
	DOREPLIFETIME(AAntColonyVote, LastAt);
	DOREPLIFETIME(AAntColonyVote, bLastPassed);
}

void AAntColonyVote::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!HasAuthority() || !bActive)
	{
		return;
	}
	const AAntRaid* Raid = AAntRaid::Get(GetWorld());
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	if (!Raid || Raid->Phase != AntRaidPhase::Nest)
	{
		Finish(false, TEXT("рейд начался"));
	}
	else if (GS && GS->GetServerWorldTimeSeconds() >= Ends)
	{
		Resolve(true);
	}
	else
	{
		Resolve(false);   // somebody left: the majority may be there already
	}
}

void AAntColonyVote::Resolve(bool bTimeUp)
{
	if (!bActive)
	{
		return;
	}
	Voters = (uint8)FMath::Clamp(CountPlayers(GetWorld()), 1, 255);
	const int32 Y = Yes.Num(), N = No.Num(), V = Voters;
	if (Y * 2 > V) { Finish(true, FString()); }
	else if (N * 2 > V) { Finish(false, FString()); }
	else if (Y + N >= V || bTimeUp)
	{
		if (Y > N) { Finish(true, FString()); }
		else if (N > Y) { Finish(false, FString()); }
		else if (bHostVoted) { Finish(bHostYes, TEXT("поровну: решил голос хоста")); }
		else { Finish(false, Y + N == 0 ? TEXT("никто не голосовал") : TEXT("поровну, хост не голосовал")); }
	}
}

void AAntColonyVote::Finish(bool bPassed, const FString& Why)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	bLastPassed = bPassed;
	const AAntGameState* GS = GetWorld()->GetGameState<AAntGameState>();
	LastAt = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
	LastResult = FString::Printf(TEXT("%s: %s (за %d, против %d%s)"), bPassed ? TEXT("ПРИНЯТО") : TEXT("ОТКЛОНЕНО"), *What, Yes.Num(), No.Num(), Why.IsEmpty() ? TEXT("") : *(TEXT("; ") + Why));
	UE_LOG(LogTemp, Display, TEXT("ANTVOTE result: %s | cmd %d arg %d by %s | yes %d [%s] no %d [%s] voters %d %s"), bPassed ? TEXT("PASSED") : TEXT("REJECTED"), (int32)Cmd, Arg, *By,
		Yes.Num(), *FString::Join(Yes, TEXT(",")), No.Num(), *FString::Join(No, TEXT(",")), (int32)Voters, *Why);
	SayAll(GetWorld(), TEXT("ГОЛОСОВАНИЕ — ") + LastResult, bPassed ? C::Leaf : C::Jam);
	AAntRaid* Raid = AAntRaid::Get(GetWorld());
	AAntPlayerController* Host = nullptr;
	CountPlayers(GetWorld(), &Host);
	if (bPassed && Raid && Host && Raid->Phase == AntRaidPhase::Nest)
	{
		const int32 FoodBefore = Raid->Nest.Food, JellyBefore = Raid->Nest.Jelly;
		bExecuting = true;
		Raid->PlayerCommand(Host, Cmd, Arg, 0);   // the same path as the host's own button
		bExecuting = false;
		const bool bDone = Raid->Nest.Food != FoodBefore || Raid->Nest.Jelly != JellyBefore;
		UE_LOG(LogTemp, Display, TEXT("ANTVOTE carried out: %s (food %d -> %d, jelly %d -> %d)"), bDone ? TEXT("yes") : TEXT("NO - the colony could not"), FoodBefore, Raid->Nest.Food, JellyBefore, Raid->Nest.Jelly);
		if (bDone)
		{
			AntColony::Chronicle(Raid->Nest, FString::Printf(TEXT("Рейд %d: решено голосованием — предложил %s, за %d, против %d"), Raid->Nest.Raids, *By, Yes.Num(), No.Num()));
			Raid->SaveNest();
		}
	}
	ForceNetUpdate();
}

namespace AntColonyVote
{
	AAntColonyVote* Info(const UObject* Ctx)
	{
		UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		if (!W)
		{
			return nullptr;
		}
		if (AAntColonyVote* I = GVote.Get(); I && I->GetWorld() == W)
		{
			return I;
		}
		const double Now = FPlatformTime::Seconds();
		const bool bServer = W->GetNetMode() != NM_Client;
		if (!bServer && Now < GVoteNextFind)
		{
			return nullptr;
		}
		GVoteNextFind = Now + 0.5;
		for (TActorIterator<AAntColonyVote> It(W); It; ++It)
		{
			GVote = *It;
			return *It;
		}
		if (bServer && W->IsGameWorld() && !W->bIsTearingDown)
		{
			AAntColonyVote* I = W->SpawnActor<AAntColonyVote>();
			if (I)
			{
				int32 M = AntVoteMode::Vote;
				if (GConfig) { GConfig->GetInt(VoteSection, TEXT("Mode"), M, GGameUserSettingsIni); }
				FParse::Value(FCommandLine::Get(), TEXT("AntVoteMode="), M);   // dev / tests
				I->Mode = (uint8)FMath::Clamp(M, 0, AntVoteMode::Count - 1);
			}
			GVote = I;
			return I;
		}
		return nullptr;
	}

	uint8 Mode(const UObject* Ctx)
	{
		const AAntColonyVote* I = Info(Ctx);
		return I ? I->Mode : (uint8)AntVoteMode::Host;
	}

	bool VotingOn(const UObject* Ctx)
	{
		const AAntColonyVote* I = Info(Ctx);
		if (!I || I->Mode != AntVoteMode::Vote)
		{
			return false;
		}
		const AGameStateBase* GS = I->GetWorld()->GetGameState();
		int32 Players = 0;
		if (GS) { for (const APlayerState* P : GS->PlayerArray) { Players += (P && !P->IsABot()) ? 1 : 0; } }
		return Players >= 2;
	}

	bool CanBuild(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		return W && (W->GetNetMode() != NM_Client || VotingOn(Ctx));
	}

	FString HintLine(const UObject* Ctx)
	{
		if (VotingOn(Ctx)) { return TEXT("ГОЛОСОВАНИЕ: любой предлагает стройку — отряд решает (За/Против, 20 с; при равенстве решает хост)"); }
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		if (W && W->GetNetMode() == NM_Standalone) { return FString(); }
		return TEXT("РЕШАЕТ ХОСТ: колонию строит он (переключатель «Решает хост / Голосование» — в сборе отряда)");
	}

	bool Command(AAntRaid* Raid, AAntPlayerController* PC, uint8 Cmd, int32 A, int32 B)
	{
		if (!Raid || !PC)
		{
			return false;
		}
		AAntColonyVote* I = Info(Raid);
		if (!I)
		{
			return false;
		}
		UWorld* W = Raid->GetWorld();
		const AAntGameState* GS = W->GetGameState<AAntGameState>();
		const float Now = GS ? GS->GetServerWorldTimeSeconds() : 0.f;
		APlayerState* PS = PC->PlayerState;
		const FString Name = PS ? PS->GetPlayerName() : FString(TEXT("?"));
		const bool bHost = PC->IsLocalController();
		if (Cmd == AntRaidCmdX::VoteMode)
		{
			if (!bHost) { PC->ClientToast(TEXT("Кто решает за колонию — выбирает хост")); return true; }
			const uint8 NewMode = A != 0 ? AntVoteMode::Vote : AntVoteMode::Host;
			if (NewMode != I->Mode)
			{
				I->Mode = NewMode;
				if (GConfig) { GConfig->SetInt(VoteSection, TEXT("Mode"), NewMode, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
				UE_LOG(LogTemp, Display, TEXT("ANTVOTE mode: %s"), NewMode == AntVoteMode::Vote ? TEXT("vote") : TEXT("host decides"));
				if (I->bActive && NewMode == AntVoteMode::Host) { I->Finish(false, TEXT("хост забрал решение себе")); }
				SayAll(W, NewMode == AntVoteMode::Vote ? TEXT("КОЛОНИЯ: решения принимает отряд голосованием. Любой может предложить стройку")
					: TEXT("КОЛОНИЯ: решает хост. Остальные смотрят и советуют"), C::Amber);
				I->ForceNetUpdate();
			}
			return true;
		}
		if (Cmd == AntRaidCmdX::Vote)
		{
			if (!I->bActive || !PS) { return true; }
			if (I->VotedPS.Contains(PS)) { PC->ClientToast(TEXT("Ты уже проголосовал")); return true; }
			I->VotedPS.Add(PS);
			(A != 0 ? I->Yes : I->No).Add(Name);
			if (bHost) { I->bHostVoted = true; I->bHostYes = A != 0; }
			UE_LOG(LogTemp, Display, TEXT("ANTVOTE vote: %s %s (yes %d no %d of %d)"), *Name, A != 0 ? TEXT("YES") : TEXT("NO"), I->Yes.Num(), I->No.Num(), (int32)I->Voters);
			I->Resolve(false);
			I->ForceNetUpdate();
			return true;
		}
		if (!IsBuildCmd(Cmd) || I->bExecuting || Raid->Phase != AntRaidPhase::Nest || !VotingOn(Raid))
		{
			return false;   // the raid's own rules: the host builds, a client is told so
		}
		if (I->bActive)
		{
			PC->ClientToast(TEXT("Уже идёт голосование: сначала решите его"));
			return true;
		}
		FString What, Why;
		if (!Describe(Raid, Cmd, A, What, Why))
		{
			PC->ClientToast(FString::Printf(TEXT("Не предложить: %s"), *Why));
			return true;
		}
		I->bActive = true;
		++I->Seq;
		I->Cmd = Cmd;
		I->Arg = A;
		I->What = What;
		I->By = Name;
		I->Ends = Now + Seconds;
		I->Yes.Reset();
		I->No.Reset();
		I->VotedPS.Reset();
		I->bHostVoted = false;
		I->Voters = (uint8)FMath::Clamp(CountPlayers(W), 1, 255);
		// Whoever proposes is for it.
		if (PS) { I->VotedPS.Add(PS); }
		I->Yes.Add(Name);
		if (bHost) { I->bHostVoted = true; I->bHostYes = true; }
		UE_LOG(LogTemp, Display, TEXT("ANTVOTE proposed: «%s» by %s (cmd %d arg %d), voters %d, %.0f s"), *What, *Name, (int32)Cmd, A, (int32)I->Voters, Seconds);
		SayAll(W, FString::Printf(TEXT("%s предлагает: %s. Голосуйте: [Y] за · [N] против (%d с)"), *Name, *What, (int32)Seconds), C::Amber);
		I->ForceNetUpdate();
		I->Resolve(false);
		return true;
	}

	void DrawHud(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC)
	{
		const AAntColonyVote* I = Info(Hud);
		const AAntGameState* GS = Hud ? Hud->GetWorld()->GetGameState<AAntGameState>() : nullptr;
		if (!I || !Cv || !GS)
		{
			return;
		}
		const float Now = GS->GetServerWorldTimeSeconds();
		const float W = Cv->ClipX, H = Cv->ClipY;
		const float Fs = FMath::Max(0.6f, FMath::Min(W / 1920.f, H / 1080.f));
		const float PW = 660.f * Fs, PX = (W - PW) * 0.5f, PY = 96.f * Fs;
		if (!I->bActive)
		{
			// How the last one ended: a slip with a stamp, for six seconds.
			const float Age = Now - I->LastAt;
			if (Age >= 0.f && Age < 6.f && !I->LastResult.IsEmpty())
			{
				const float A = FMath::Clamp((6.f - Age) / 0.8f, 0.f, 1.f);
				const float PH = 92.f * Fs;
				AntUiCanvas::Rect(Cv, PX + 4.f, PY + 5.f, PW, PH, FLinearColor(0.f, 0.f, 0.f, 0.35f * A));
				AntUiCanvas::Rect(Cv, PX, PY, PW, PH, WithA(C::Paper, A));
				AntUiCanvas::Rect(Cv, PX, PY, 6.f * Fs, PH, WithA(I->bLastPassed ? C::LeafDeep : C::JamDeep, A));
				AntUiCanvas::Wrap(Cv, I->LastResult, PX + 22.f * Fs, PY + 12.f * Fs, PW - 190.f * Fs, EFont::Bold, 16.f * Fs, WithA(C::Graphite, A), 3);
				AntUiCanvas::Stamp(Cv, I->bLastPassed ? TEXT("ПРИНЯТО") : TEXT("ОТКЛОНЕНО"), PX + PW - 92.f * Fs, PY + PH * 0.5f, -8.f, WithA(I->bLastPassed ? C::LeafDeep : C::Jam, A), 18.f * Fs);
			}
			return;
		}
		const float PH = 196.f * Fs;
		const float Left = FMath::Max(0.f, I->Ends - Now);
		const FString MyName = PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
		const bool bMyYes = I->Yes.Contains(MyName), bMyNo = I->No.Contains(MyName);
		const bool bVoted = bMyYes || bMyNo;
		AntUiCanvas::Rect(Cv, PX + 5.f, PY + 6.f, PW, PH, FLinearColor(0.f, 0.f, 0.f, 0.4f));
		AntUiCanvas::Paper(Cv, PX, PY, PW, PH, true);
		AntUiCanvas::Text(Cv, TEXT("ГОЛОСОВАНИЕ КОЛОНИИ"), PX + 24.f * Fs, PY + 14.f * Fs, EFont::Display, 20.f * Fs, C::JamDeep);
		AntUiCanvas::Text(Cv, FString::Printf(TEXT("%d с"), FMath::CeilToInt(Left)), PX + PW - 24.f * Fs, PY + 14.f * Fs, EFont::Mono, 20.f * Fs, Left < 5.f ? C::Jam : C::Graphite, 2);
		AntUiCanvas::TextFit(Cv, I->What, PX + 24.f * Fs, PY + 48.f * Fs, PW - 48.f * Fs, EFont::Bold, 19.f * Fs, C::Graphite);
		AntUiCanvas::Text(Cv, FString::Printf(TEXT("предложил: %s"), *I->By), PX + 24.f * Fs, PY + 78.f * Fs, EFont::Hand, 17.f * Fs, WithA(C::Graphite, 0.8f));
		// The two buttons.
		float MX = -1.f, MY = -1.f;
		bool bClick = false;
		if (PC)
		{
			PC->GetMousePosition(MX, MY);
			bClick = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
		}
		const float BW = (PW - 48.f * Fs - 16.f * Fs) * 0.5f, BH = 46.f * Fs, BY = PY + 106.f * Fs;
		auto Btn = [&](float X, const FString& Label, const FLinearColor& Col, bool bMine, FKey Key) -> bool
		{
			const bool bHot = MX >= X && MX < X + BW && MY >= BY && MY < BY + BH;
			const float A = bVoted && !bMine ? 0.35f : 1.f;
			AntUiCanvas::Rect(Cv, X, BY, BW, BH, WithA(C::Cocoa, A));
			AntUiCanvas::Rect(Cv, X + 2.f, BY + 2.f, BW - 4.f, BH - 4.f, WithA(bHot && !bVoted ? Col * 1.25f : Col, A));
			if (bMine) { AntUiCanvas::Frame(Cv, X - 2.f, BY - 2.f, BW + 4.f, BH + 4.f, 3.f * Fs, C::Honey); }
			AntUiCanvas::Text(Cv, Label, X + BW * 0.5f, BY + 10.f * Fs, EFont::Display, 18.f * Fs, WithA(C::Sugar, A), 1, 0.4f);
			return !bVoted && ((bHot && bClick) || (PC && PC->WasInputKeyJustPressed(Key)));
		};
		const bool bYes = Btn(PX + 24.f * Fs, FString::Printf(TEXT("[Y]  ЗА · %d"), I->Yes.Num()), C::LeafDeep, bMyYes, EKeys::Y);
		const bool bNo = Btn(PX + 24.f * Fs + BW + 16.f * Fs, FString::Printf(TEXT("[N]  ПРОТИВ · %d"), I->No.Num()), C::JamDeep, bMyNo, EKeys::N);
		if (PC && (bYes || bNo)) { PC->ServerRaid(AntRaidCmdX::Vote, bYes ? 1 : 0, 0); }
		// Who voted how; the clock.
		const FString Names = FString::Printf(TEXT("за: %s   ·   против: %s   ·   голосуют %d"), I->Yes.Num() ? *FString::Join(I->Yes, TEXT(", ")) : TEXT("—"), I->No.Num() ? *FString::Join(I->No, TEXT(", ")) : TEXT("—"), (int32)I->Voters);
		AntUiCanvas::TextFit(Cv, Names, PX + 24.f * Fs, BY + BH + 8.f * Fs, PW - 48.f * Fs, EFont::Body, 14.f * Fs, WithA(C::Graphite, 0.85f));
		AntUiCanvas::Rect(Cv, PX + 24.f * Fs, PY + PH - 12.f * Fs, PW - 48.f * Fs, 5.f * Fs, WithA(C::Cocoa, 0.35f));
		AntUiCanvas::Rect(Cv, PX + 24.f * Fs, PY + PH - 12.f * Fs, (PW - 48.f * Fs) * FMath::Clamp(Left / Seconds, 0.f, 1.f), 5.f * Fs, Left < 5.f ? C::Jam : C::AmberDeep);
	}

	bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A)
	{
		if (A.Num() < 2 || A[0] != TEXT("vote") || !GM)
		{
			return false;
		}
		UWorld* W = GM->GetWorld();
		AAntRaid* Raid = AAntRaid::Get(W);
		AAntPlayerController* Host = nullptr;
		CountPlayers(W, &Host);
		AAntColonyVote* I = Info(GM);
		if (!Raid || !Host || !I)
		{
			UE_LOG(LogTemp, Display, TEXT("ANTVOTE dev: no raid / host"));
			return true;
		}
		const FString& Op = A[1];
		const int32 N = A.Num() >= 4 ? FCString::Atoi(*A[3]) : A.Num() >= 3 ? FCString::Atoi(*A[2]) : 0;
		if (Op == TEXT("mode")) { Raid->PlayerCommand(Host, AntRaidCmdX::VoteMode, N, 0); }
		else if (Op == TEXT("propose") && A.Num() >= 3)
		{
			const uint8 Cmd = A[2] == TEXT("dig") ? (uint8)AntRaidCmd::Dig : A[2] == TEXT("buy") ? (uint8)AntRaidCmd::Buy : A[2] == TEXT("larva") ? (uint8)AntRaidCmd::Larva : A[2] == TEXT("workers") ? (uint8)AntRaidCmd::Workers : (uint8)AntRaidCmd::Chamber;
			Raid->PlayerCommand(Host, Cmd, N, 0);
		}
		else if (Op == TEXT("yes") || Op == TEXT("no")) { Raid->PlayerCommand(Host, AntRaidCmdX::Vote, Op == TEXT("yes") ? 1 : 0, 0); }
		else if (Op == TEXT("bot") && A.Num() >= 3)
		{
			// The other players' votes, cast for them (tests of the count with one machine).
			for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
			{
				AAntPlayerController* PC = Cast<AAntPlayerController>(It->Get());
				if (PC && PC != Host) { Raid->PlayerCommand(PC, AntRaidCmdX::Vote, A[2] == TEXT("yes") ? 1 : 0, 0); }
			}
		}
		UE_LOG(LogTemp, Display, TEXT("ANTVOTE info: mode %d on %d active %d «%s» by %s yes %d no %d voters %d | last «%s» | food %d jelly %d dug %d level %d"), (int32)I->Mode, VotingOn(GM) ? 1 : 0, I->bActive ? 1 : 0, *I->What, *I->By,
			I->Yes.Num(), I->No.Num(), (int32)I->Voters, *I->LastResult, Raid->Nest.Food, Raid->Nest.Jelly, (int32)Raid->Nest.Dug, AntColony::Level(Raid->Nest));
		return true;
	}
}
