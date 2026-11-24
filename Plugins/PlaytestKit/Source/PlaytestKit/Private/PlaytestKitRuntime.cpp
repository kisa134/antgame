// PlaytestKit: per-frame work (perf / net / hitch / breadcrumbs), map & net delegates, markers, screenshots, surveys, console commands.
#include "PlaytestKitSubsystem.h"
#include "PlaytestKit.h"
#include "PTKUploader.h"
#include "PTKUi.h"
#include "PTKLogCapture.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "UnrealClient.h"
#include "RHI.h"
#include "RenderCore.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HAL/IConsoleManager.h"

namespace
{
	FString Esc(const FString& S)
	{
		FString O;
		for (const TCHAR C : S)
		{
			switch (C) { case '"': O += TEXT("\\\""); break; case '\\': O += TEXT("\\\\"); break; case '\n': O += TEXT("\\n"); break; case '\r': break; case '\t': O += TEXT(" "); break; default: if (C >= 0x20) { O.AppendChar(C); } }
		}
		return O;
	}
}

// ------------------------------------------------------------------------------------------------- delegates

void UPlaytestKitSubsystem::BindDelegates()
{
	PreLoadH = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UPlaytestKitSubsystem::OnPreLoadMap);
	PostLoadH = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPlaytestKitSubsystem::OnPostLoadMap);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UPlaytestKitSubsystem::OnNetFailure);
		GEngine->OnTravelFailure().AddUObject(this, &UPlaytestKitSubsystem::OnTravelFailure);
	}
}

void UPlaytestKitSubsystem::UnbindDelegates()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadH);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadH);
	if (GEngine) { GEngine->OnNetworkFailure().RemoveAll(this); GEngine->OnTravelFailure().RemoveAll(this); }
}

void UPlaytestKitSubsystem::OnPreLoadMap(const FString& Map)
{
	const double Now = FPlatformTime::Seconds();
	if (!LastMap.IsEmpty())
	{
		Track(TEXT("level_end"), FPTKProps().S("map", LastMap).N("seconds", Now - MapStart).S("next", Map), TEXT("level"));
	}
	MapLoadStart = Now;
	if (Map.Contains(TEXT(":")) && !Map.Contains(TEXT("/Game/")) && !Map.Contains(TEXT("/Engine/")))
	{
		Track(TEXT("net_join_attempt"), FPTKProps(), TEXT("net"));   // a travel to host:port (the address itself is never recorded)
		JoinAttemptAt = Now;
	}
}

void UPlaytestKitSubsystem::OnPostLoadMap(UWorld* World)
{
	if (!World || !GetGameInstance() || World->GetGameInstance() != GetGameInstance()) { return; }
	const double Now = FPlatformTime::Seconds();
	LastMap = FPackageName::GetShortName(World->GetOutermost()->GetName());
	MapStart = Now;
	LastPlayers = -1;
	FIntPoint Res(0, 0);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) { Res = GEngine->GameViewport->Viewport->GetSizeXY(); }
	Track(TEXT("level_start"), FPTKProps().S("map", LastMap).N("load_ms", (Now - MapLoadStart) * 1000.0).S("role", RoleName()).S("res", FString::Printf(TEXT("%dx%d"), Res.X, Res.Y)), TEXT("level"));
	if (World->GetNetMode() == NM_Client)
	{
		Track(TEXT("net_join_ok"), FPTKProps().N("ms", JoinAttemptAt > 0.0 ? (Now - JoinAttemptAt) * 1000.0 : -1.0), TEXT("net"));
		JoinAttemptAt = 0.0;
	}
	if (bActive) { WriteSessionJson(false); }
}

void UPlaytestKitSubsystem::OnNetFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Msg)
{
	++NetErrCount;
	Track(TEXT("net_error"), FPTKProps().S("type", ENetworkFailure::ToString(Type)).S("msg", Msg.Left(200)).S("role", RoleName()), TEXT("net"));
}

void UPlaytestKitSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Msg)
{
	++NetErrCount;
	Track(TEXT("net_join_fail"), FPTKProps().S("type", ETravelFailure::ToString(Type)).S("msg", Msg.Left(200)), TEXT("net"));
	JoinAttemptAt = 0.0;
}

// ------------------------------------------------------------------------------------------------- tick

bool UPlaytestKitSubsystem::OnTick(float)
{
	const uint64 C0 = FPlatformTime::Cycles64();
	const double Now = FPlatformTime::Seconds();
	const double Dt = Now - LastTickSecs;
	LastTickSecs = Now;
	const UPlaytestKitSettings* St = GetDefault<UPlaytestKitSettings>();
	const float Ms = (float)(Dt * 1000.0);
	if (FrameMs.Num() < 4096) { FrameMs.Add(Ms); }
	if (Dt > 0.0) { SmoothFps = SmoothFps <= 0.f ? (float)(1.0 / Dt) : FMath::Lerp(SmoothFps, (float)(1.0 / Dt), 0.05f); }
	if (bActive && Ms > St->HitchMs && Now - StartSecs > 2.0)
	{
		++HitchCount; ++TotalHitches;
		HitchWorst = FMath::Max(HitchWorst, Ms);
		if (Now - LastHitchEvent > 0.5)
		{
			LastHitchEvent = Now;
			Track(TEXT("hitch"), FPTKProps().N("ms", Ms).B("loading", Now - MapLoadStart < 6.0), TEXT("hitch"));
		}
	}
	if (ConsentState == 0 && Ui.IsValid() && !Ui->ConsentOpen())
	{
		if (GetLocalPC() && GEngine && GEngine->GameViewport)
		{
			if (ConsentWaitSince <= 0.0) { ConsentWaitSince = Now; }
			if (Now - ConsentWaitSince > 1.5) { Ui->OpenConsent(); }
		}
	}
	if (Ui.IsValid()) { Ui->Tick(Now); }
	if (ShotH.IsValid() && Now - ShotRequestedAt > 4.0)
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ShotH);
		ShotH.Reset();
		if (ShotDone) { TFunction<void(const FString&)> D = MoveTemp(ShotDone); D(FString()); }
	}
	if (bActive)
	{
		if (!Script.IsEmpty()) { ScriptTick(Now); }
		if (Now >= NextCrumb) { NextCrumb = Now + FMath::Max(0.5f, St->BreadcrumbIntervalSec); SampleBreadcrumb(); }
		if (Now >= NextPerf) { NextPerf = Now + FMath::Max(1.f, St->PerfIntervalSec); SamplePerf(); }
		if (Now >= NextNet) { NextNet = Now + FMath::Max(1.f, St->NetIntervalSec); SampleNet(); }
		if (Now >= NextWatch)
		{
			NextWatch = Now + 1.0;
			WatchPlayers();
			DrainLog();
			if (Uploader.IsValid()) { Uploader->Tick(Now, SessionId); }
		}
		if (Now >= NextFlush) { NextFlush = Now + 1.0; FlushFile(); }
		if (Now >= NextSessionJson) { NextSessionJson = Now + 30.0; WriteSessionJson(false); }
		SnapCheck(Dt);
	}
	else if (Uploader.IsValid() && Now >= NextWatch)
	{
		NextWatch = Now + 2.0;
		Uploader->Tick(Now, FString());   // consent declined / not given: leftovers of earlier consented sessions may still go out
	}
	const float Cost = (float)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - C0);
	TickCostSum += Cost; ++TickCostN; TickCostMax = FMath::Max(TickCostMax, Cost);
	return true;
}

void UPlaytestKitSubsystem::SnapCheck(double Dt)
{
	// rubber-banding indicator on clients: the local pawn jumps further than its own velocity explains
	UWorld* W = GetWorldSafe();
	if (!W || W->GetNetMode() != NM_Client) { bHadPos = false; return; }
	const APlayerController* PC = GetLocalPC();
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) { bHadPos = false; return; }
	const FVector Pos = P->GetActorLocation();
	if (bHadPos && Dt < 0.15)
	{
		const double Dist = FVector::Dist(Pos, LastPos);
		const double Expected = P->GetVelocity().Size() * Dt;
		if (Dist > 3.0 * Expected + 300.0)
		{
			++SnapCount;
			const double Now = FPlatformTime::Seconds();
			if (Now - LastSnapEvent > 1.0) { LastSnapEvent = Now; Track(TEXT("net_snap"), FPTKProps().N("dist", Dist).N("expected", Expected), TEXT("net")); }
		}
	}
	LastPos = Pos;
	bHadPos = true;
}

void UPlaytestKitSubsystem::SampleBreadcrumb()
{
	const APlayerController* PC = GetLocalPC();
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) { return; }
	Track(TEXT("breadcrumb"), FPTKProps().N("v", P->GetVelocity().Size()), TEXT("breadcrumb"));
}

void UPlaytestKitSubsystem::SamplePerf()
{
	if (FrameMs.Num() < 5) { FrameMs.Reset(); return; }
	double Sum = 0.0;
	float Max = 0.f;
	for (float M : FrameMs) { Sum += M; Max = FMath::Max(Max, M); }
	const float Avg = (float)(Sum / FrameMs.Num());
	FrameMs.Sort();
	const float P99 = FrameMs[FMath::Min(FrameMs.Num() - 1, (int32)(FrameMs.Num() * 0.99f))];
	const float Gpu = (float)FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	const float Gt = (float)FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float Rt = (float)FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const FPlatformMemoryStats MS = FPlatformMemory::GetStats();
	const int32 Err = LogCap ? LogCap->Errors.load() : 0, Warn = LogCap ? LogCap->Warnings.load() : 0;
	FPerfSample S{ FPlatformTime::Seconds() - StartSecs, 1000.f / FMath::Max(Avg, 0.01f), 1000.f / FMath::Max(P99, 0.01f), Avg, Gpu };
	PerfRing.Add(S);
	if (PerfRing.Num() > 16) { PerfRing.RemoveAt(0); }
	Track(TEXT("perf_sample"), FPTKProps().N("fps_avg", S.FpsAvg).N("fps_min", 1000.f / FMath::Max(Max, 0.01f)).N("fps_p1", S.FpsP1).N("frame_ms", Avg).N("frame_ms_p99", P99)
		.N("gpu_ms", Gpu).N("game_ms", Gt).N("render_ms", Rt).N("mem_mb", (double)(MS.UsedPhysical / 1048576)).I("hitches", HitchCount).N("hitch_worst_ms", HitchWorst)
		.I("log_errors", Err - PrevErr).I("log_warnings", Warn - PrevWarn).N("ptk_ms_avg", TickCostN ? TickCostSum / TickCostN : 0.0).N("ptk_ms_max", TickCostMax)
		.N("ptk_track_us", TrackN ? TrackUsSum / TrackN : 0.0).I("dropped", Dropped).S("role", RoleName()), TEXT("perf"));
	PrevErr = Err; PrevWarn = Warn;
	FrameMs.Reset(); HitchCount = 0; HitchWorst = 0.f;
	TickCostSum = 0.0; TickCostN = 0; TickCostMax = 0.f;
}

FString UPlaytestKitSubsystem::GetLastPerfJson(float Seconds) const
{
	const double Now = FPlatformTime::Seconds() - StartSecs;
	FString O = TEXT("[");
	bool bFirst = true;
	for (const FPerfSample& S : PerfRing)
	{
		if (Now - S.T > Seconds) { continue; }
		O.Appendf(TEXT("%s{\"t\":%d,\"fps\":%.0f,\"p1\":%.0f,\"ms\":%.1f,\"gpu\":%.1f}"), bFirst ? TEXT("") : TEXT(","), (int32)S.T, S.FpsAvg, S.FpsP1, S.FrameMs, S.GpuMs);
		bFirst = false;
	}
	return O + TEXT("]");
}

void UPlaytestKitSubsystem::SampleNet()
{
	UWorld* W = GetWorldSafe();
	if (!W || W->GetNetMode() == NM_Standalone) { return; }
	UNetDriver* D = W->GetNetDriver();
	if (!D) { return; }
	TArray<UNetConnection*> Conns;
	if (W->GetNetMode() == NM_Client) { if (D->ServerConnection) { Conns.Add(D->ServerConnection); } }
	else { for (UNetConnection* C : D->ClientConnections) { if (C) { Conns.Add(C); } } }
	int64 InB = 0, OutB = 0, InP = 0, OutP = 0, InL = 0, OutL = 0;
	float PingMax = 0.f, PingSum = 0.f;
	int32 PingN = 0;
	FString PerConn;
	for (UNetConnection* C : Conns)
	{
		InB += C->InTotalBytes; OutB += C->OutTotalBytes; InP += C->InTotalPackets; OutP += C->OutTotalPackets; InL += C->InTotalPacketsLost; OutL += C->OutTotalPacketsLost;
		float Ping = C->AvgLag * 1000.f;
		if (C->PlayerController && C->PlayerController->PlayerState) { Ping = C->PlayerController->PlayerState->GetPingInMilliseconds(); }
		else if (W->GetNetMode() == NM_Client) { if (const APlayerController* PC = GetLocalPC()) { if (PC->PlayerState) { Ping = PC->PlayerState->GetPingInMilliseconds(); } } }
		PingMax = FMath::Max(PingMax, Ping); PingSum += Ping; ++PingN;
		PerConn.Appendf(TEXT("%s%.0f"), PerConn.IsEmpty() ? TEXT("") : TEXT(","), Ping);
	}
	const double Dt = FMath::Max(0.5, FPlatformTime::Seconds() - LastNetAt);
	LastNetAt = FPlatformTime::Seconds();
	auto Delta = [](int64 Now, int64& Prev) { const int64 D2 = FMath::Max<int64>(0, Now - Prev); Prev = Now; return D2; };
	const int64 dIB = Delta(InB, PrevNet[0]), dOB = Delta(OutB, PrevNet[1]), dIP = Delta(InP, PrevNet[2]), dOP = Delta(OutP, PrevNet[3]), dIL = Delta(InL, PrevNet[4]), dOL = Delta(OutL, PrevNet[5]);
	Track(TEXT("net_sample"), FPTKProps().S("role", RoleName()).I("conns", Conns.Num()).N("ping_avg", PingN ? PingSum / PingN : 0.f).N("ping_max", PingMax).Raw(TEXT("pings"), TEXT("[") + PerConn + TEXT("]"))
		.N("loss_in_pct", dIP > 0 ? 100.0 * dIL / (double)(dIP + dIL) : 0.0).N("loss_out_pct", dOP > 0 ? 100.0 * dOL / (double)(dOP + dOL) : 0.0)
		.N("in_kbps", dIB * 8.0 / 1000.0 / Dt).N("out_kbps", dOB * 8.0 / 1000.0 / Dt).I("snaps", SnapCount).I("net_errors", NetErrCount), TEXT("net"));
	SnapCount = 0;
}

void UPlaytestKitSubsystem::WatchPlayers()
{
	const UWorld* W = GetWorldSafe();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	if (!GS) { return; }
	const int32 N = GS->PlayerArray.Num();
	if (LastPlayers >= 0 && N != LastPlayers)
	{
		Track(N > LastPlayers ? TEXT("player_join") : TEXT("player_leave"), FPTKProps().I("from", LastPlayers).I("to", N), TEXT("net"));
	}
	LastPlayers = N;
}

void UPlaytestKitSubsystem::DrainLog()
{
	if (!LogCap.IsValid()) { return; }
	TArray<TPair<FString, FString>> Q;
	{
		FScopeLock L(&LogCap->Lock);
		Q = MoveTemp(LogCap->Queue);
	}
	int32 Emitted = 0;
	for (const TPair<FString, FString>& E : Q)
	{
		if (++Emitted > 5) { break; }
		Track(TEXT("error"), FPTKProps().S("log_cat", E.Key).S("msg", E.Value), TEXT("error"));
	}
}

void UPlaytestKitSubsystem::ScriptTick(double Now)
{
	if (ScriptT0 < 0.0) { ScriptT0 = Now; }
	for (int32 i = 0; i < Script.Num(); ++i)
	{
		if (Now - ScriptT0 >= Script[i].At)
		{
			const FString Cmd = Script[i].Cmd.StartsWith(TEXT("ptk.")) ? Script[i].Cmd : TEXT("ptk.") + Script[i].Cmd;   // "marker fun x" or "ptk.marker fun x"
			Script.RemoveAt(i);
			UE_LOG(LogPTK, Display, TEXT("script: %s"), *Cmd);
			IConsoleManager::Get().ProcessUserConsoleInput(*Cmd, *GLog, GetWorldSafe());
			return;
		}
	}
}

// ------------------------------------------------------------------------------------------------- markers & screenshots

void UPlaytestKitSubsystem::OpenMarkerOverlay() { if (Ui.IsValid() && bActive) { Ui->OpenMarker(); } }

void UPlaytestKitSubsystem::AddMarker(const FString& Tag, const FString& Text)
{
	if (!bActive) { return; }
	const int32 MarkerSeq = Seq + 1;
	FString Recent = TEXT("[");
	for (int32 i = 0; i < RecentNames.Num(); ++i) { Recent.Appendf(TEXT("%s\"%s\""), i ? TEXT(",") : TEXT(""), *Esc(RecentNames[i])); }
	Recent += TEXT("]");
	const bool bShot = GetDefault<UPlaytestKitSettings>()->bMarkerScreenshot && GEngine && GEngine->GameViewport;
	const FString ShotName = FString::Printf(TEXT("m%d.jpg"), MarkerSeq);
	FPTKProps P;
	P.S("tag", Tag).S("text", Text).Raw(TEXT("perf30"), GetLastPerfJson(30.f)).Raw(TEXT("recent"), Recent);
	if (bShot) { P.S("shot", ShotName); }
	Track(TEXT("marker"), P, TEXT("marker"));
	if (bShot)
	{
		CaptureShot(ShotName, [this, MarkerSeq](const FString& File)
		{
			Track(TEXT("marker_shot"), FPTKProps().I("marker_seq", MarkerSeq).S("file", File).B("ok", !File.IsEmpty()), TEXT("marker"));
		});
	}
}

void UPlaytestKitSubsystem::CaptureShot(const FString& Name, TFunction<void(const FString&)> Done)
{
	if (ShotH.IsValid() || FScreenshotRequest::IsScreenshotRequested()) { if (Done) { Done(FString()); } return; }
	ShotPendingName = Name;
	ShotDone = MoveTemp(Done);
	ShotRequestedAt = FPlatformTime::Seconds();
	ShotH = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UPlaytestKitSubsystem::OnShotCaptured);
	FScreenshotRequest::RequestScreenshot(true);
}

void UPlaytestKitSubsystem::OnShotCaptured(int32 W, int32 H, const TArray<FColor>& Bmp)
{
	UGameViewportClient::OnScreenshotCaptured().Remove(ShotH);
	ShotH.Reset();
	const FString Path = SessionDir / TEXT("shots") / ShotPendingName;
	const FString Name = ShotPendingName;
	TFunction<void(const FString&)> Done = MoveTemp(ShotDone);
	const int32 MaxW = GetDefault<UPlaytestKitSettings>()->bUploadScreenshots ? 1280 : 1600;
	TArray<FColor> Copy = Bmp;
	TWeakObjectPtr<UPlaytestKitSubsystem> Weak(this);
	Async(EAsyncExecution::ThreadPool, [Copy = MoveTemp(Copy), W, H, MaxW, Path, Name, Done = MoveTemp(Done)]() mutable
	{
		const int32 OW = FMath::Min(W, MaxW);
		const int32 OH = FMath::Max(1, (int32)((int64)H * OW / FMath::Max(1, W)));
		TArray<FColor> Out;
		Out.SetNumUninitialized(OW * OH);
		for (int32 y = 0; y < OH; ++y)
		{
			const int32 SY = FMath::Min(H - 1, (int32)((int64)y * H / OH));
			for (int32 x = 0; x < OW; ++x) { Out[y * OW + x] = Copy[SY * W + FMath::Min(W - 1, (int32)((int64)x * W / OW))]; }
		}
		IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wr = IW.CreateImageWrapper(EImageFormat::JPEG);
		FString Result;
		if (Wr.IsValid() && Wr->SetRaw(Out.GetData(), Out.Num() * sizeof(FColor), OW, OH, ERGBFormat::BGRA, 8))
		{
			const TArray64<uint8>& Jpg = Wr->GetCompressed(78);
			if (Jpg.Num() > 0 && FFileHelper::SaveArrayToFile(Jpg, *Path)) { Result = Name; }
		}
		AsyncTask(ENamedThreads::GameThread, [Done = MoveTemp(Done), Result]() { if (Done) { Done(Result); } });
	});
}

// ------------------------------------------------------------------------------------------------- surveys

void UPlaytestKitSubsystem::SurveyAnswered(const FString& SurveyId, const FPTKQuestion& Q, const FString& Value, double Num)
{
	const TCHAR* Type = Q.Type == FPTKQuestion::QScale ? TEXT("scale") : Q.Type == FPTKQuestion::QChoice ? TEXT("choice") : TEXT("text");
	FPTKProps P;
	P.S("survey_id", SurveyId).S("qid", Q.Id).S("type", Type);
	if (Q.Type == FPTKQuestion::QText) { P.S("text", Value.Left(1500)); }
	else { P.S("value", Value).N("num", Num); }
	Track(TEXT("survey_answer"), P, TEXT("survey"));
}

void UPlaytestKitSubsystem::AskOne(const FString& QuestionId, const FString& Text, int32 ScaleMin, int32 ScaleMax, float TimeoutSec)
{
	if (!bActive || !Ui.IsValid()) { return; }
	FPTKQuestion Q;
	Q.Id = QuestionId; Q.Text = Text; Q.Type = FPTKQuestion::QScale; Q.Min = ScaleMin; Q.Max = FMath::Clamp(ScaleMax, ScaleMin, 10);
	Track(TEXT("survey_ask"), FPTKProps().S("survey_id", TEXT("micro")).S("qid", QuestionId).S("text", Text), TEXT("survey"));
	Ui->OpenAsk(Q, TimeoutSec, TEXT("micro"));
}

void UPlaytestKitSubsystem::AskChoice(const FString& QuestionId, const FString& Text, const TArray<FString>& Options, float TimeoutSec)
{
	if (!bActive || !Ui.IsValid() || Options.Num() == 0) { return; }
	FPTKQuestion Q;
	Q.Id = QuestionId; Q.Text = Text; Q.Type = FPTKQuestion::QChoice; Q.Options = Options;
	Track(TEXT("survey_ask"), FPTKProps().S("survey_id", TEXT("micro")).S("qid", QuestionId).S("text", Text), TEXT("survey"));
	Ui->OpenAsk(Q, TimeoutSec, TEXT("micro"));
}

void UPlaytestKitSubsystem::AutoSurvey() { if (Ui.IsValid()) { Ui->AutoAnswer(); } }

bool UPlaytestKitSubsystem::IsSurveyOpen() const { return Ui.IsValid() && (Ui->SurveyOpen() || Ui->AskOpen()); }

FPTKSurveyDef UPlaytestKitSubsystem::MakeStandardPostSurvey(const TArray<FString>& Features)
{
	const bool bRu = !GetDefault<UPlaytestKitSettings>()->Language.StartsWith(TEXT("en"), ESearchCase::IgnoreCase);
	auto T = [bRu](const TCHAR* Ru, const TCHAR* En) { return bRu ? FString(Ru) : FString(En); };
	FPTKSurveyDef D;
	D.Id = TEXT("post_session_v1");
	D.Title = T(TEXT("Как прошла сессия? (1-2 минуты)"), TEXT("How was the session? (1-2 minutes)"));
	auto Add = [&D](const TCHAR* Id, const FString& Text, FPTKQuestion::EType Type, bool bReq) -> FPTKQuestion& { FPTKQuestion Q; Q.Id = Id; Q.Text = Text; Q.Type = Type; Q.bRequired = bReq; return D.Questions.Add_GetRef(Q); };
	FPTKQuestion& Overall = Add(TEXT("overall"), T(TEXT("Общая оценка сессии"), TEXT("Overall rating of the session")), FPTKQuestion::QScale, true);
	Overall.Min = 1; Overall.Max = 10; Overall.LowLabel = T(TEXT("ужасно"), TEXT("awful")); Overall.HighLabel = T(TEXT("отлично"), TEXT("great"));
	FPTKQuestion& Again = Add(TEXT("play_again"), T(TEXT("Сыграл бы ещё?"), TEXT("Would you play again?")), FPTKQuestion::QChoice, true);
	Again.Options = { T(TEXT("Точно да"), TEXT("Definitely")), T(TEXT("Скорее да"), TEXT("Probably")), T(TEXT("Скорее нет"), TEXT("Probably not")), T(TEXT("Точно нет"), TEXT("No")) };
	FPTKQuestion& Diff = Add(TEXT("difficulty"), T(TEXT("Сложность"), TEXT("Difficulty")), FPTKQuestion::QChoice, false);
	Diff.Options = { T(TEXT("Слишком легко"), TEXT("Too easy")), T(TEXT("Нормально"), TEXT("Just right")), T(TEXT("Слишком сложно"), TEXT("Too hard")) };
	Add(TEXT("best_moment"), T(TEXT("Самый лучший момент (что и где)"), TEXT("Best moment (what and where)")), FPTKQuestion::QText, false);
	Add(TEXT("worst_moment"), T(TEXT("Самый неприятный момент"), TEXT("Worst moment")), FPTKQuestion::QText, false);
	Add(TEXT("confusing"), T(TEXT("Что было непонятно?"), TEXT("What was confusing?")), FPTKQuestion::QText, false);
	for (const FString& F : Features)
	{
		FPTKQuestion& Q = Add(*(FString(TEXT("feat_")) + F.Replace(TEXT(" "), TEXT("_"))), T(TEXT("Оцени: "), TEXT("Rate: ")) + F, FPTKQuestion::QScale, false);
		Q.Min = 1; Q.Max = 5; Q.LowLabel = T(TEXT("плохо"), TEXT("bad")); Q.HighLabel = T(TEXT("круто"), TEXT("great"));
	}
	Add(TEXT("comment"), T(TEXT("Что ещё хочешь сказать?"), TEXT("Anything else?")), FPTKQuestion::QText, false);
	return D;
}

void UPlaytestKitSubsystem::ShowPostSurvey(const FString& Reason, TFunction<void()> OnDone)
{
	if (!bActive || !Ui.IsValid() || Ui->SurveyOpen()) { if (OnDone) { OnDone(); } return; }
	Track(TEXT("survey_show"), FPTKProps().S("survey_id", TEXT("post_session_v1")).S("reason", Reason), TEXT("survey"));
	Ui->OpenSurvey(MakeStandardPostSurvey(SurveyFeatures), MoveTemp(OnDone));
}

void UPlaytestKitSubsystem::ShowSurvey(const FPTKSurveyDef& Def, TFunction<void()> OnDone)
{
	if (!bActive || !Ui.IsValid() || Ui->SurveyOpen()) { if (OnDone) { OnDone(); } return; }
	Ui->OpenSurvey(Def, MoveTemp(OnDone));
}

bool UPlaytestKitSubsystem::WantsKey(const FKeyEvent& E, bool& bHandled)
{
	bHandled = false;
	if (!Ui.IsValid() || bDisabled) { return false; }
	if (!bActive && !Ui->ConsentOpen()) { return false; }
	bHandled = Ui->HandleKey(E);
	return bHandled;
}

// ------------------------------------------------------------------------------------------------- status, selftest, console

FString UPlaytestKitSubsystem::GetStatusText() const
{
	return FString::Printf(TEXT("PlaytestKit 1.0 | project=%s build=%s sha=%s | session=%s | active=%d consent=%d | role=%s map=%s | events=%d dropped=%d file=%lld KB | errors=%d warnings=%d | track=%.1f us/event | marker key=%s\n  dir=%s\n  %s"),
		*ProjectId, *BuildVer, *GitSha, *SessionId, bActive ? 1 : 0, (int32)ConsentState, *RoleName(), *LastMap, EventCount, Dropped, FileBytes / 1024, LogCap ? LogCap->Errors.load() : 0, LogCap ? LogCap->Warnings.load() : 0,
		TrackN ? TrackUsSum / TrackN : 0.0, *MarkerKeyCached.ToString(), *SessionDir, Uploader.IsValid() ? *Uploader->Status() : TEXT("no uploader"));
}

double UPlaytestKitSubsystem::SelfTest(int32 Count)
{
	if (!bActive) { UE_LOG(LogPTK, Warning, TEXT("selftest: kit not active")); return -1.0; }
	bSelfTest = true;
	const int32 SavedSeq = Seq, SavedCount = EventCount, SavedDropped = Dropped;
	const double T0 = FPlatformTime::Seconds();
	for (int32 i = 0; i < Count; ++i)
	{
		Track(TEXT("selftest_event"), FPTKProps().I("i", i).S("cause", TEXT("acid")).N("dmg", i * 0.5), TEXT("selftest"));
	}
	const double Us = (FPlatformTime::Seconds() - T0) * 1e6 / FMath::Max(1, Count);
	bSelfTest = false;
	Seq = SavedSeq; EventCount = SavedCount; Dropped = SavedDropped; TrackUsSum = 0.0; TrackN = 0;
	FlushFile();
	Track(TEXT("selftest_result"), FPTKProps().I("count", Count).N("us_per_event", Us), TEXT("selftest"));
	UE_LOG(LogPTK, Display, TEXT("PTK SELFTEST: %d events, %.2f us per Track() (frame budget 0.1 ms = 100 us); at 60 events/s that is %.4f ms per frame"), Count, Us, Us * 60.0 / 60.0 / 1000.0);
	return Us;
}

void UPlaytestKitSubsystem::RegisterConsoleOnce()
{
	static bool bDone = false;
	if (bDone) { return; }
	bDone = true;
	static TArray<FAutoConsoleCommand*> Cmds;
	auto Reg = [](const TCHAR* Name, const TCHAR* Help, TFunction<void(const TArray<FString>&)> Fn)
	{
		Cmds.Add(new FAutoConsoleCommand(Name, Help, FConsoleCommandWithArgsDelegate::CreateLambda([Fn](const TArray<FString>& A) { Fn(A); })));
	};
	auto Join = [](const TArray<FString>& A, int32 From) { FString O; for (int32 i = From; i < A.Num(); ++i) { O += (O.IsEmpty() ? TEXT("") : TEXT(" ")) + A[i]; } return O; };
	Reg(TEXT("ptk.status"), TEXT("PlaytestKit status"), [](const TArray<FString>&) { if (UPlaytestKitSubsystem* S = Get()) { UE_LOG(LogPTK, Display, TEXT("%s"), *S->GetStatusText()); } else { UE_LOG(LogPTK, Display, TEXT("PlaytestKit is off")); } });
	Reg(TEXT("ptk.marker"), TEXT("ptk.marker <bug|confusing|boring|fun> [text]"), [Join](const TArray<FString>& A) { if (UPlaytestKitSubsystem* S = Get()) { S->AddMarker(A.Num() ? A[0] : TEXT("bug"), Join(A, 1)); } });
	Reg(TEXT("ptk.ask"), TEXT("ptk.ask <qid> <min> <max> <text...>"), [Join](const TArray<FString>& A) { if (UPlaytestKitSubsystem* S = Get()) { if (A.Num() >= 4) { S->AskOne(A[0], Join(A, 3), FCString::Atoi(*A[1]), FCString::Atoi(*A[2])); } } });
	Reg(TEXT("ptk.survey"), TEXT("ptk.survey [reason]: the post-session survey"), [](const TArray<FString>& A) { if (UPlaytestKitSubsystem* S = Get()) { S->ShowPostSurvey(A.Num() ? A[0] : TEXT("console")); } });
	Reg(TEXT("ptk.autosurvey"), TEXT("fill the open survey with test answers and submit (automation)"), [](const TArray<FString>&) { if (UPlaytestKitSubsystem* S = Get()) { S->AutoSurvey(); } });
	Reg(TEXT("ptk.consent"), TEXT("ptk.consent yes|no"), [](const TArray<FString>& A) { if (UPlaytestKitSubsystem* S = Get()) { S->OnConsentAnswer(A.Num() && A[0].StartsWith(TEXT("y"))); } });
	Reg(TEXT("ptk.track"), TEXT("ptk.track <name> [key=value ...]"), [](const TArray<FString>& A)
	{
		if (UPlaytestKitSubsystem* S = Get())
		{
			FPTKProps P;
			for (int32 i = 1; i < A.Num(); ++i) { FString K, V; if (A[i].Split(TEXT("="), &K, &V)) { P.S(*K, V); } }
			if (A.Num()) { S->Track(A[0], P); }
		}
	});
	Reg(TEXT("ptk.flush"), TEXT("write files and try an upload now"), [](const TArray<FString>&) { if (UPlaytestKitSubsystem* S = Get()) { S->FlushNow(); } });
	Reg(TEXT("ptk.selftest"), TEXT("ptk.selftest [count]: cost of Track()"), [](const TArray<FString>& A) { if (UPlaytestKitSubsystem* S = Get()) { S->SelfTest(A.Num() ? FCString::Atoi(*A[0]) : 5000); } });
	Reg(TEXT("ptk.openmarker"), TEXT("open the F7 overlay"), [](const TArray<FString>&) { if (UPlaytestKitSubsystem* S = Get()) { S->OpenMarkerOverlay(); } });
#if !UE_BUILD_SHIPPING
	Reg(TEXT("ptk.testensure"), TEXT("fire an ensure() to test the crash/ensure hook"), [](const TArray<FString>&) { ensureMsgf(false, TEXT("PTK test ensure: this is intentional")); });
	Reg(TEXT("ptk.testcrash"), TEXT("CRASH the game on purpose to test the crash hook"), [](const TArray<FString>&) { volatile int32* P = nullptr; *P = 1; });
#endif
	Reg(TEXT("ptk.quit"), TEXT("flush and exit the game (automation: -PTKScript=\"90:quit\")"), [](const TArray<FString>&) { if (UPlaytestKitSubsystem* S = Get()) { S->FlushNow(); } FPlatformMisc::RequestExit(false); });
}
