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
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/EngineVersion.h"
#include "Async/Async.h"
#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "UnrealClient.h"
#include "RHI.h"
#include "RenderCore.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HttpModule.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include <atomic>

DEFINE_LOG_CATEGORY(LogPTK);
IMPLEMENT_MODULE(FPlaytestKitModule, PlaytestKit)

static TWeakObjectPtr<UPlaytestKitSubsystem> GPTKInst;
static UPlaytestKitSubsystem* GPTKCrashInst = nullptr;

// ------------------------------------------------------------------------------------------------- props

static void AppendEsc(FString& Out, const FString& S)
{
	for (const TCHAR C : S)
	{
		switch (C)
		{
		case '"': Out += TEXT("\\\""); break;
		case '\\': Out += TEXT("\\\\"); break;
		case '\n': Out += TEXT("\\n"); break;
		case '\r': Out += TEXT("\\r"); break;
		case '\t': Out += TEXT("\\t"); break;
		default:
			if (C < 0x20) { Out.Appendf(TEXT("\\u%04x"), (uint32)C); }
			else { Out.AppendChar(C); }
		}
	}
}

void FPTKProps::Key(const TCHAR* K)
{
	if (!Json.IsEmpty()) { Json.AppendChar(','); }
	Json.AppendChar('"');
	Json.Append(K);
	Json.Append(TEXT("\":"));
}
FPTKProps& FPTKProps::S(const TCHAR* K, const FString& V) { Key(K); Json.AppendChar('"'); AppendEsc(Json, V); Json.AppendChar('"'); return *this; }
FPTKProps& FPTKProps::N(const TCHAR* K, double V)
{
	Key(K);
	if (!FMath::IsFinite(V)) { V = 0.0; }
	if (FMath::Abs(V) < 1e15 && FMath::IsNearlyEqual(V, FMath::RoundToDouble(V), 1e-9)) { Json.Appendf(TEXT("%lld"), (int64)FMath::RoundToDouble(V)); }
	else { Json.Appendf(TEXT("%.5g"), V); }
	return *this;
}
FPTKProps& FPTKProps::I(const TCHAR* K, int64 V) { Key(K); Json.Appendf(TEXT("%lld"), V); return *this; }
FPTKProps& FPTKProps::B(const TCHAR* K, bool V) { Key(K); Json.Append(V ? TEXT("true") : TEXT("false")); return *this; }
FPTKProps& FPTKProps::V3(const TCHAR* K, const FVector& V) { Key(K); Json.Appendf(TEXT("{\"x\":%.0f,\"y\":%.0f,\"z\":%.0f}"), V.X, V.Y, V.Z); return *this; }
FPTKProps& FPTKProps::Raw(const TCHAR* K, const FString& F) { Key(K); Json.Append(F); return *this; }

// ------------------------------------------------------------------------------------------------- log capture

// ------------------------------------------------------------------------------------------------- lifecycle

static bool PTKIsOffByCommandLine() { return FParse::Param(FCommandLine::Get(), TEXT("PTKOff")); }

bool UPlaytestKitSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningCommandlet() || PTKIsOffByCommandLine()) { return false; }
	const UPlaytestKitSettings* S = GetDefault<UPlaytestKitSettings>();
	return S && S->bEnabled;
}

UPlaytestKitSubsystem* UPlaytestKitSubsystem::Get() { return GPTKInst.Get(); }

void UPlaytestKitSubsystem::Event(const TCHAR* Name, const FPTKProps& Props, const TCHAR* Cat)
{
	if (UPlaytestKitSubsystem* S = GPTKInst.Get()) { S->Track(Name, Props, Cat ? FString(Cat) : FString()); }
}

bool UPlaytestKitSubsystem::Tr_IsRu() const { return !GetDefault<UPlaytestKitSettings>()->Language.StartsWith(TEXT("en"), ESearchCase::IgnoreCase); }
FString UPlaytestKitSubsystem::Tr(const TCHAR* Ru, const TCHAR* En) const { return Tr_IsRu() ? FString(Ru) : FString(En); }

UWorld* UPlaytestKitSubsystem::GetWorldSafe() const { return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr; }
APlayerController* UPlaytestKitSubsystem::GetLocalPC() const { return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr; }

FString UPlaytestKitSubsystem::RoleName() const
{
	if (const UWorld* W = GetWorldSafe())
	{
		switch (W->GetNetMode())
		{
		case NM_Client: return TEXT("client");
		case NM_ListenServer: case NM_DedicatedServer: return TEXT("host");
		default: break;
		}
	}
	return TEXT("solo");
}

static FString PTKRoot() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Playtest")); }

void UPlaytestKitSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UPlaytestKitSettings* St = GetDefault<UPlaytestKitSettings>();
	GPTKInst = this;
	GPTKCrashInst = this;
	StartSecs = FPlatformTime::Seconds();
	StartUtc = FDateTime::UtcNow();
	LastTickSecs = StartSecs;
	SessionId = FString::Printf(TEXT("%s-%04x"), *StartUtc.ToString(TEXT("%Y%m%d-%H%M%S")), FMath::RandRange(0, 0xffff));
	ProjectId = St->ProjectId;
	{
		FString V = St->BuildVersion;
		if (V.IsEmpty()) { V = GetDefault<UGeneralProjectSettings>()->ProjectVersion; }
		BuildVer = V;
		GitSha = St->GitSha;
		FParse::Value(FCommandLine::Get(), TEXT("PTKSha="), GitSha);
		Platform = FPlatformProperties::PlatformName();
		Config = LexToString(FApp::GetBuildConfiguration());
	}
	SurveyFeatures = St->SurveyFeatures;
	MarkerKeyCached = St->MarkerKey;
	{
		FString K;
		if (FParse::Value(FCommandLine::Get(), TEXT("PTKMarkerKey="), K) && !K.IsEmpty()) { MarkerKeyCached = FKey(*K); }
	}
	IFileManager::Get().MakeDirectory(*PTKRoot(), true);
	// the anonymous id: random, persisted, never derived from the user or the machine name
	{
		const FString P = PTKRoot() / TEXT("anon_id.txt");
		if (!FFileHelper::LoadFileToString(AnonId, *P) || AnonId.TrimStartAndEnd().Len() < 8)
		{
			AnonId = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(16).ToLower();
			FFileHelper::SaveStringToFile(AnonId, *P);
		}
		AnonId = AnonId.TrimStartAndEnd();
	}
	LogCap = MakeShared<FPTKLogCapture>();
	GLog->AddOutputDevice(LogCap.Get());
	Ui = MakeShared<FPTKUi>(this);
	const bool bHasSlate = FSlateApplication::IsInitialized() && !IsRunningDedicatedServer();
	if (bHasSlate)
	{
		Input = MakeShared<FPTKInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Input, 0);
	}
	Uploader = MakeShared<FPTKUploader>();
	{
		FString Ep = St->Endpoint, Key = St->IngestKey;
		FParse::Value(FCommandLine::Get(), TEXT("PTKEndpoint="), Ep);
		FParse::Value(FCommandLine::Get(), TEXT("PTKKey="), Key);
		Uploader->Configure(Ep, Key, PTKRoot(), ProjectId, St->UploadBatchKB, St->bUploadScreenshots, St->UploadIntervalSec);
	}
	LoadConsent();
	if (ConsentState == 1) { OpenStorage(); }
	BindDelegates();
	RegisterConsoleOnce();
	{
		FString Scr;
		if (FParse::Value(FCommandLine::Get(), TEXT("PTKScript="), Scr, false))
		{
			TArray<FString> Items;
			Scr.ParseIntoArray(Items, TEXT(";"), true);
			for (const FString& It : Items)
			{
				int32 Colon;
				if (It.FindChar(':', Colon)) { Script.Add({ FCString::Atod(*It.Left(Colon)), It.Mid(Colon + 1).TrimStartAndEnd() }); }
			}
			ScriptT0 = -1.0;
		}
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPlaytestKitSubsystem::OnTick), 0.f);
	FCoreDelegates::OnHandleSystemError.AddStatic(&UPlaytestKitSubsystem::OnCrashStatic);
	FCoreDelegates::OnHandleSystemEnsure.AddStatic(&UPlaytestKitSubsystem::OnEnsureStatic);
	FString Machine;
	{
		FString OsLabel, OsSub;
		FPlatformMisc::GetOSVersions(OsLabel, OsSub);
		const FPlatformMemoryConstants& MC = FPlatformMemory::GetConstants();
		Machine = FString::Printf(TEXT("{\"os\":\"%s %s\",\"cpu\":\"%s\",\"cores\":%d,\"gpu\":\"%s\",\"ram_gb\":%d,\"engine\":\"%s\"}"), *OsLabel, *OsSub, *FPlatformMisc::GetCPUBrand().TrimStartAndEnd(), FPlatformMisc::NumberOfCores(), *FPlatformMisc::GetPrimaryGPUBrand(), (int32)MC.TotalPhysicalGB, *FEngineVersion::Current().ToString(EVersionComponent::Patch));
	}
	Track(TEXT("session_start"), FPTKProps().S("kit_version", TEXT("1.0")).B("upload", Uploader->Enabled()).Raw(TEXT("machine"), Machine).S("cmd_flags", CommandLineFlags()), TEXT("session"));
	UE_LOG(LogPTK, Display, TEXT("PlaytestKit session %s project=%s consent=%d upload=%d"), *SessionId, *ProjectId, (int32)ConsentState, Uploader->Enabled() ? 1 : 0);
}

FString UPlaytestKitSubsystem::CommandLineFlags() const
{
	TArray<FString> Toks;
	FString(FCommandLine::Get()).ParseIntoArray(Toks, TEXT(" "), true);
	FString Out;
	for (const FString& T : Toks)
	{
		if (!T.StartsWith(TEXT("-")) || T.Contains(TEXT("PTKKey"), ESearchCase::IgnoreCase) || T.Contains(TEXT("Endpoint"), ESearchCase::IgnoreCase)) { continue; }
		FString Name = T;
		int32 Eq;
		if (Name.FindChar('=', Eq)) { Name = Name.Left(Eq); }
		if (!Out.IsEmpty()) { Out += TEXT(" "); }
		Out += Name;
		if (Out.Len() > 400) { break; }
	}
	return Out;
}

void UPlaytestKitSubsystem::Deinitialize()
{
	if (!bEnded)
	{
		bEnded = true;
		if (bActive)
		{
			Track(TEXT("session_end"), FPTKProps().S("reason", TEXT("quit")).N("seconds", FPlatformTime::Seconds() - StartSecs).I("events", EventCount).I("dropped", Dropped)
				.I("log_errors", LogCap ? LogCap->Errors.load() : 0).I("log_warnings", LogCap ? LogCap->Warnings.load() : 0).N("ptk_track_us", TrackN ? TrackUsSum / TrackN : 0.0), TEXT("session"));
			FlushFile();
			WriteSessionJson(true);
		}
	}
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	UnbindDelegates();
	if (Input.IsValid() && FSlateApplication::IsInitialized()) { FSlateApplication::Get().UnregisterInputPreProcessor(Input); }
	Input.Reset();
	if (Ui.IsValid()) { Ui->CloseAll(); }
	Ui.Reset();
	if (LogCap.IsValid()) { GLog->RemoveOutputDevice(LogCap.Get()); LogCap.Reset(); }
	if (Writer) { Writer->Close(); delete Writer; Writer = nullptr; }
	Uploader.Reset();
	GPTKCrashInst = nullptr;
	GPTKInst.Reset();
	Super::Deinitialize();
}

// ------------------------------------------------------------------------------------------------- consent & storage

void UPlaytestKitSubsystem::LoadConsent()
{
	const UPlaytestKitSettings* St = GetDefault<UPlaytestKitSettings>();
	FString Cli;
	if (FParse::Value(FCommandLine::Get(), TEXT("PTKConsent="), Cli))
	{
		ConsentState = (Cli.StartsWith(TEXT("y"), ESearchCase::IgnoreCase) || Cli == TEXT("1")) ? 1 : 2;
		return;
	}
	if (!St->bRequireConsent || IsRunningDedicatedServer() || !FSlateApplication::IsInitialized()) { ConsentState = 1; return; }
	FString Txt;
	if (FFileHelper::LoadFileToString(Txt, *(PTKRoot() / TEXT("consent.json"))))
	{
		TSharedPtr<FJsonObject> O;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Txt), O) && O.IsValid() && O->GetIntegerField(TEXT("v")) >= 1)
		{
			ConsentState = O->GetBoolField(TEXT("accepted")) ? 1 : 2;
			return;
		}
	}
	ConsentState = 0;
}

void UPlaytestKitSubsystem::OnConsentAnswer(bool bAccepted)
{
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"v\":1,\"accepted\":%s,\"ts\":\"%s\"}"), bAccepted ? TEXT("true") : TEXT("false"), *FDateTime::UtcNow().ToIso8601()),
		*(PTKRoot() / TEXT("consent.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (Ui.IsValid()) { Ui->CloseAll(); }
	if (!bAccepted)
	{
		ConsentState = 2;
		bActive = false;
		PreConsent.Reset();
		UE_LOG(LogPTK, Display, TEXT("consent declined: nothing is recorded"));
		return;
	}
	ConsentState = 1;
	OpenStorage();
	for (const FString& L : PreConsent) { WriteLine(L, true); }
	PreConsent.Reset();
	Track(TEXT("consent"), FPTKProps().B("accepted", true).I("v", 1), TEXT("session"));
}

void UPlaytestKitSubsystem::OpenStorage()
{
	if (bActive || ConsentState != 1) { return; }
	SessionDir = PTKRoot() / SessionId;
	IFileManager::Get().MakeDirectory(*(SessionDir / TEXT("shots")), true);
	Writer = IFileManager::Get().CreateFileWriter(*(SessionDir / TEXT("events.jsonl")), FILEWRITE_Append | FILEWRITE_AllowRead);
	bActive = Writer != nullptr;
	if (!bActive) { UE_LOG(LogPTK, Warning, TEXT("cannot open %s"), *SessionDir); return; }
	WriteSessionJson(false);
	EnforceStorageCap();
}

void UPlaytestKitSubsystem::EnforceStorageCap()
{
	const UPlaytestKitSettings* St = GetDefault<UPlaytestKitSettings>();
	TArray<FString> Dirs;
	IFileManager::Get().FindFiles(Dirs, *(PTKRoot() / TEXT("*")), false, true);
	Dirs.Sort();
	auto SizeOf = [](const FString& D) { int64 T = 0; IFileManager::Get().IterateDirectoryRecursively(*D, [&T](const TCHAR* P, bool bDir) { if (!bDir) { T += FMath::Max<int64>(0, IFileManager::Get().FileSize(P)); } return true; }); return T; };
	int64 Total = 0;
	TArray<int64> Sizes;
	for (const FString& D : Dirs) { Sizes.Add(SizeOf(PTKRoot() / D)); Total += Sizes.Last(); }
	int32 Count = Dirs.Num();
	for (int32 i = 0; i < Dirs.Num() && (Total > (int64)St->MaxLocalMB * 1048576 || Count > St->MaxLocalSessions); ++i)
	{
		if (Dirs[i] == SessionId) { continue; }
		IFileManager::Get().DeleteDirectory(*(PTKRoot() / Dirs[i]), false, true);
		Total -= Sizes[i];
		--Count;
		UE_LOG(LogPTK, Log, TEXT("storage cap: removed old session %s"), *Dirs[i]);
	}
}

void UPlaytestKitSubsystem::SetSteamId(const FString& SteamId)
{
	const UPlaytestKitSettings* St = GetDefault<UPlaytestKitSettings>();
	if (!St->bHashSteamId || SteamId.IsEmpty()) { return; }
	FTCHARToUTF8 U(*(St->SteamSalt + SteamId));
	uint8 H[20];
	FSHA1::HashBuffer(U.Get(), U.Length(), H);
	SteamHash = BytesToHex(H, 8).ToLower();
	if (bActive) { WriteSessionJson(false); }
}

void UPlaytestKitSubsystem::WriteSessionJson(bool bEnd)
{
	if (SessionDir.IsEmpty()) { return; }
	FString Flags = CommandLineFlags(), FlagsJ;
	AppendEsc(FlagsJ, Flags);
	FString J;
	J.Appendf(TEXT("{\"schema_v\":1,\"project\":\"%s\",\"build\":{\"version\":\"%s\",\"git_sha\":\"%s\",\"platform\":\"%s\",\"config\":\"%s\"},\"session_id\":\"%s\","),
		*ProjectId, *BuildVer, *GitSha, *Platform, *Config, *SessionId);
	J.Appendf(TEXT("\"player\":{\"anon_id\":\"%s\",\"role\":\"%s\"%s},\"started\":\"%s\",\"ended\":%s,"), *AnonId, *RoleName(),
		SteamHash.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(",\"steam\":\"%s\""), *SteamHash), *StartUtc.ToString(TEXT("%Y-%m-%dT%H:%M:%S.%sZ")),
		bEnd ? *FString::Printf(TEXT("\"%s\""), *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%S.%sZ"))) : TEXT("null"));
	J.Appendf(TEXT("\"kit_version\":\"1.0\",\"flags\":\"%s\",\"counters\":{\"events\":%d,\"dropped\":%d,\"errors\":%d,\"warnings\":%d,\"hitches\":%d}}"), *FlagsJ, EventCount, Dropped, LogCap ? LogCap->Errors.load() : 0, LogCap ? LogCap->Warnings.load() : 0, TotalHitches);
	FFileHelper::SaveStringToFile(J, *(SessionDir / TEXT("session.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// ------------------------------------------------------------------------------------------------- events

void UPlaytestKitSubsystem::BuildContext(FPTKContext& C, bool bCompact) const
{
	C.Map = LastMap;
	if (!bCompact)
	{
		if (const UWorld* W = GetWorldSafe()) { if (W->GetGameState()) { C.PlayersCount = W->GetGameState()->PlayerArray.Num(); } }
		C.Fps = SmoothFps;
	}
	if (const APlayerController* PC = GetLocalPC())
	{
		if (const APawn* P = PC->GetPawn()) { C.bHasPos = true; C.Pos = P->GetActorLocation(); }
	}
	if (ContextProvider.IsBound()) { ContextProvider.Execute(C); }
}

void UPlaytestKitSubsystem::AppendContextJson(FString& Out, const FPTKContext& C, bool bCompact) const
{
	Out.Append(TEXT("{"));
	bool bFirst = true;
	auto Sep = [&]() { if (!bFirst) { Out.AppendChar(','); } bFirst = false; };
	if (!C.Map.IsEmpty()) { Sep(); Out.Append(TEXT("\"map\":\"")); AppendEsc(Out, C.Map); Out.AppendChar('"'); }
	if (!bCompact)
	{
		if (!C.Mode.IsEmpty()) { Sep(); Out.Append(TEXT("\"mode\":\"")); AppendEsc(Out, C.Mode); Out.AppendChar('"'); }
		Sep(); Out.Appendf(TEXT("\"role\":\"%s\""), *RoleName());
		if (C.Seed != INT64_MIN) { Sep(); Out.Appendf(TEXT("\"seed\":%lld"), C.Seed); }
	}
	if (C.RaidNo >= 0) { Sep(); Out.Appendf(TEXT("\"raid_no\":%d"), C.RaidNo); }
	if (!bCompact)
	{
		if (C.Tier >= 0) { Sep(); Out.Appendf(TEXT("\"tier\":%d"), C.Tier); }
		if (!C.TimeOfDay.IsEmpty()) { Sep(); Out.Append(TEXT("\"time_of_day\":\"")); AppendEsc(Out, C.TimeOfDay); Out.AppendChar('"'); }
		if (C.PlayersCount >= 0) { Sep(); Out.Appendf(TEXT("\"players_count\":%d"), C.PlayersCount); }
	}
	if (C.bHasPos) { Sep(); Out.Appendf(TEXT("\"pos\":{\"x\":%.0f,\"y\":%.0f,\"z\":%.0f}"), C.Pos.X, C.Pos.Y, C.Pos.Z); }
	if (!bCompact && C.Fps >= 0.f) { Sep(); Out.Appendf(TEXT("\"fps\":%.0f"), C.Fps); }
	if (!C.ExtraJson.IsEmpty()) { Sep(); Out.Append(TEXT("\"extra\":{")); Out.Append(C.ExtraJson); Out.AppendChar('}'); }
	Out.Append(TEXT("}"));
}

void UPlaytestKitSubsystem::Track(const FString& Name, const FPTKProps& Props, const FString& CatIn)
{
	if (bDisabled || ConsentState == 2 || bEnded) { return; }
	if (!IsInGameThread())
	{
		TWeakObjectPtr<UPlaytestKitSubsystem> Weak(this);
		AsyncTask(ENamedThreads::GameThread, [Weak, Name, Props, CatIn]() { if (UPlaytestKitSubsystem* S = Weak.Get()) { S->Track(Name, Props, CatIn); } });
		return;
	}
	const uint64 C0 = FPlatformTime::Cycles64();
	FString Cat = CatIn;
	if (Cat.IsEmpty()) { int32 U; Cat = Name.FindChar('_', U) ? Name.Left(U) : TEXT("custom"); }
	const double Now = FPlatformTime::Seconds();
	const bool bCrumb = Cat == TEXT("breadcrumb");
	const bool bNoisy = bCrumb || Cat == TEXT("perf") || Cat == TEXT("hitch") || Cat == TEXT("net");
	const bool bMust = Cat == TEXT("session") || Cat == TEXT("marker") || Cat == TEXT("survey") || Cat == TEXT("crash");
	if (!bMust && !bSelfTest)
	{
		if (Now - RateWindowStart > 1.0) { RateWindowStart = Now; RateWindowN = 0; }
		if (++RateWindowN > GetDefault<UPlaytestKitSettings>()->MaxEventsPerSec) { ++Dropped; return; }
	}
	++Seq;
	++EventCount;
	FPTKContext Cx;
	BuildContext(Cx, bCrumb);
	FString L;
	L.Reserve(320 + Props.Json.Len());
	L.Appendf(TEXT("{\"seq\":%d,\"t\":%lld,"), Seq, (int64)((Now - StartSecs) * 1000.0));
	if (!bCrumb) { L.Appendf(TEXT("\"ts\":\"%s\","), *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%S.%sZ"))); }
	L.Appendf(TEXT("\"name\":\"%s\",\"cat\":\"%s\",\"props\":{"), *Name, *Cat);
	L.Append(Props.Json);
	L.Append(TEXT("},\"ctx\":"));
	AppendContextJson(L, Cx, bCrumb);
	L.AppendChar('}');
	if (!bNoisy && !bSelfTest)
	{
		RecentNames.Add(FString::Printf(TEXT("%s@%d"), *Name, (int32)(Now - StartSecs)));
		if (RecentNames.Num() > 12) { RecentNames.RemoveAt(0); }
	}
	if (ConsentState == 0) { if (PreConsent.Num() < 4000) { PreConsent.Add(L); } }
	else { WriteLine(L, !bNoisy); }
	TrackUsSum += FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - C0) * 1000.0;
	++TrackN;
}

void UPlaytestKitSubsystem::WriteLine(const FString& Line, bool bEssential)
{
	if (!bActive || bSelfTest) { return; }   // selftest builds the full line but never persists it (keeps real data clean)
	if (!bEssential && FileBytes > (int64)GetDefault<UPlaytestKitSettings>()->MaxSessionMB * 1048576) { ++Dropped; return; }
	Pending.Append(Line);
	Pending.AppendChar('\n');
	FileBytes += Line.Len() + 1;
	if (Pending.Len() > 65536) { FlushFile(); }
}

void UPlaytestKitSubsystem::FlushFile()
{
	if (!Writer || Pending.IsEmpty()) { return; }
	FTCHARToUTF8 U(*Pending);
	Writer->Serialize((void*)U.Get(), U.Length());
	Writer->Flush();
	Pending.Reset();
}

void UPlaytestKitSubsystem::FlushNow()
{
	FlushFile();
	WriteSessionJson(false);
	if (Uploader.IsValid()) { Uploader->KickNow(); }
}

// ------------------------------------------------------------------------------------------------- crash

void UPlaytestKitSubsystem::OnCrashStatic()
{
	if (UPlaytestKitSubsystem* S = GPTKCrashInst) { S->WriteCrash(true); }
}
void UPlaytestKitSubsystem::OnEnsureStatic()
{
	if (UPlaytestKitSubsystem* S = GPTKCrashInst) { S->WriteCrash(false); }
}

void UPlaytestKitSubsystem::WriteCrash(bool bFatal)
{
	if (!bActive || !Writer) { return; }
	FString Msg;
	AppendEsc(Msg, FString(GErrorHist).Left(1200));
	FString L;
	L.Appendf(TEXT("{\"seq\":%d,\"t\":%lld,\"ts\":\"%s\",\"name\":\"%s\",\"cat\":\"crash\",\"props\":{\"fatal\":%s,\"msg\":\"%s\",\"recent\":["),
		++Seq, (int64)((FPlatformTime::Seconds() - StartSecs) * 1000.0), *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%S.%sZ")), bFatal ? TEXT("crash") : TEXT("ensure"), bFatal ? TEXT("true") : TEXT("false"), *Msg);
	for (int32 i = 0; i < RecentNames.Num(); ++i) { L.Appendf(TEXT("%s\"%s\""), i ? TEXT(",") : TEXT(""), *RecentNames[i]); }
	L.Append(TEXT("]},\"ctx\":{\"map\":\""));
	AppendEsc(L, LastMap);
	L.Append(TEXT("\"}}\n"));
	Pending.Append(L);
	FlushFile();
	if (bFatal) { WriteSessionJson(false); }
}
