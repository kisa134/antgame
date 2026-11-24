#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"
#include "PlaytestKitSubsystem.generated.h"

class FPTKUploader;
class FPTKUi;
class FPTKLogCapture;
class FPTKInputProcessor;
class UNetDriver;

/** A small JSON object builder for event props: FPTKProps().S("cause","acid").N("dmg",12).B("lethal",true). */
struct PLAYTESTKIT_API FPTKProps
{
	FString Json;   // the members, no braces
	FPTKProps& S(const TCHAR* Key, const FString& V);
	FPTKProps& N(const TCHAR* Key, double V);
	FPTKProps& I(const TCHAR* Key, int64 V);
	FPTKProps& B(const TCHAR* Key, bool V);
	FPTKProps& V3(const TCHAR* Key, const FVector& V);   // {"x":..,"y":..,"z":..} in world units, rounded
	/** Key with a ready JSON fragment (array, object). */
	FPTKProps& Raw(const TCHAR* Key, const FString& Fragment);
	FPTKProps& S(const ANSICHAR* K, const FString& V) { return S(ANSI_TO_TCHAR(K), V); }
	FPTKProps& N(const ANSICHAR* K, double V) { return N(ANSI_TO_TCHAR(K), V); }
	FPTKProps& I(const ANSICHAR* K, int64 V) { return I(ANSI_TO_TCHAR(K), V); }
	FPTKProps& B(const ANSICHAR* K, bool V) { return B(ANSI_TO_TCHAR(K), V); }
	FPTKProps& V3(const ANSICHAR* K, const FVector& V) { return V3(ANSI_TO_TCHAR(K), V); }
	FPTKProps& Raw(const ANSICHAR* K, const FString& F) { return Raw(ANSI_TO_TCHAR(K), F); }
	bool IsEmpty() const { return Json.IsEmpty(); }
	FString ToObject() const { return TEXT("{") + Json + TEXT("}"); }
private:
	void Key(const TCHAR* K);
};

/** The per-event context (map, players, position and fps are filled by the kit; the game adds the rest through the provider). */
struct PLAYTESTKIT_API FPTKContext
{
	FString Map;
	FString Mode;
	int64 Seed = INT64_MIN;
	int32 RaidNo = -1;
	int32 Tier = -1;
	FString TimeOfDay;
	int32 PlayersCount = -1;
	bool bHasPos = false;
	FVector Pos = FVector::ZeroVector;
	float Fps = -1.f;
	/** Extra JSON members, no braces, written as ctx.extra. */
	FString ExtraJson;
};

/** One question of a survey. */
struct PLAYTESTKIT_API FPTKQuestion
{
	enum EType : uint8 { QScale, QChoice, QText };
	FString Id;
	FString Text;
	EType Type = QScale;
	int32 Min = 1;
	int32 Max = 5;
	TArray<FString> Options;   // Choice
	bool bRequired = false;
	FString LowLabel, HighLabel;
};

struct PLAYTESTKIT_API FPTKSurveyDef
{
	FString Id;
	FString Title;
	TArray<FPTKQuestion> Questions;
};

DECLARE_DELEGATE_OneParam(FPTKContextProvider, FPTKContext&);

/**
 * PlaytestKit: one per GameInstance. C++ API (all calls are safe when the kit is off - no-ops):
 *   PTK_EVENT("death", FPTKProps().S("cause","acid"));
 *   UPlaytestKitSubsystem::Event(TEXT("objective_complete"), Props, TEXT("progress"));
 * Event schema: PlaytestKit/schema/event.schema.json. Everything else: PlaytestKit/docs/PLAYTEST_SYSTEM.md.
 */
UCLASS()
class PLAYTESTKIT_API UPlaytestKitSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The live instance or nullptr (the kit is off / not started yet). */
	static UPlaytestKitSubsystem* Get();
	/** Static shortcut: no-op without an instance. Cat null/empty: the part of the name before the first '_'. */
	static void Event(const TCHAR* Name, const FPTKProps& Props = FPTKProps(), const TCHAR* Cat = nullptr);

	void Track(const FString& Name, const FPTKProps& Props = FPTKProps(), const FString& Cat = FString());

	/** Context hook of the game: raid number, tier, time of day, mode, seed. Called per event on the game thread: keep it cheap. */
	void SetContextProvider(FPTKContextProvider Provider) { ContextProvider = Provider; }
	void SetSteamId(const FString& SteamId);
	void SetSurveyFeatures(const TArray<FString>& Features) { SurveyFeatures = Features; }

	// ---- markers & surveys
	void AddMarker(const FString& Tag, const FString& Text = FString());
	void OpenMarkerOverlay();
	/** Non-blocking in-game question (answer: Alt+1..9, Alt+X skips; times out). Scale: Min..Max. */
	void AskOne(const FString& QuestionId, const FString& Text, int32 ScaleMin = 1, int32 ScaleMax = 5, float TimeoutSec = 25.f);
	void AskChoice(const FString& QuestionId, const FString& Text, const TArray<FString>& Options, float TimeoutSec = 25.f);
	/** The standard post-session survey (a window with the mouse). Reason: "raid_debrief", "quit", ... OnDone runs when it closes (answered or skipped). */
	void ShowPostSurvey(const FString& Reason = TEXT("manual"), TFunction<void()> OnDone = nullptr);
	void ShowSurvey(const FPTKSurveyDef& Def, TFunction<void()> OnDone = nullptr);
	bool IsSurveyOpen() const;
	static FPTKSurveyDef MakeStandardPostSurvey(const TArray<FString>& Features);

	// ---- state
	bool IsActive() const { return bActive; }
	const FString& GetSessionId() const { return SessionId; }
	FString GetStatusText() const;
	FString GetSessionDir() const { return SessionDir; }
	/** Force the files to disk and an upload attempt now. */
	void FlushNow();
	/** Dev: measure the cost of Track(); logs and returns microseconds per event. */
	double SelfTest(int32 Count = 5000);
	/** Dev/automation: answer the open survey with test values and submit it. */
	void AutoSurvey();

	// ---- used by the Ui / uploader (not a public API)
	bool Tr_IsRu() const;
	FString Tr(const TCHAR* Ru, const TCHAR* En) const;
	void OnConsentAnswer(bool bAccepted);
	void SurveyAnswered(const FString& SurveyId, const FPTKQuestion& Q, const FString& Value, double Num);
	FString GetLastPerfJson(float Seconds) const;
	bool WantsKey(const FKeyEvent& E, bool& bHandled);
	FKey MarkerKeyCached;

private:
	friend class FPTKLogCapture;
	friend class FPTKUi;
	bool OnTick(float Dt);
	void OpenStorage();
	void WriteLine(const FString& Line, bool bEssential);
	void FlushFile();
	void SamplePerf();
	void SampleNet();
	void SampleBreadcrumb();
	void SnapCheck(double Dt);
	void WatchPlayers();
	void DrainLog();
	void BuildContext(FPTKContext& C, bool bCompact) const;
	void AppendContextJson(FString& Out, const FPTKContext& C, bool bCompact) const;
	void WriteSessionJson(bool bEnd);
	void WriteCrash(bool bFatal);
	static void OnCrashStatic();
	static void OnEnsureStatic();
	FString CommandLineFlags() const;
	void LoadConsent();
	void EnforceStorageCap();
	void BindDelegates();
	void UnbindDelegates();
	void OnPreLoadMap(const FString& Map);
	void OnPostLoadMap(UWorld* World);
	void OnNetFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Msg);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Msg);
	void ScriptTick(double Now);
	void CaptureShot(const FString& Name, TFunction<void(const FString&)> Done);
	void OnShotCaptured(int32 W, int32 H, const TArray<FColor>& Bmp);
	void RegisterConsoleOnce();
	FString RoleName() const;
	UWorld* GetWorldSafe() const;
	APlayerController* GetLocalPC() const;

	// state
	bool bActive = false;          // recording
	bool bDisabled = false;
	uint8 ConsentState = 0;        // 0 unknown, 1 yes, 2 no
	FString SessionId, SessionDir, AnonId, SteamHash;
	FString ProjectId, BuildVer, GitSha, Platform, Config;
	double StartSecs = 0.0;
	FDateTime StartUtc;
	int32 Seq = 0;
	int32 Dropped = 0;
	int32 EventCount = 0;
	int64 FileBytes = 0;
	FString Pending;
	TArray<FString> PreConsent;
	FArchive* Writer = nullptr;
	FPTKContextProvider ContextProvider;
	TArray<FString> SurveyFeatures;
	TArray<FString> RecentNames;

	// sampling
	FTSTicker::FDelegateHandle TickHandle;
	double LastTickSecs = 0.0, NextPerf = 0.0, NextNet = 0.0, NextCrumb = 0.0, NextFlush = 0.0, NextSessionJson = 0.0, NextWatch = 0.0;
	TArray<float> FrameMs;
	int32 HitchCount = 0, TotalHitches = 0; float HitchWorst = 0.f; double LastHitchEvent = -10.0;
	double TickCostSum = 0.0; float TickCostMax = 0.f; int32 TickCostN = 0;
	double TrackUsSum = 0.0; int64 TrackN = 0;
	float SmoothFps = 0.f;
	struct FPerfSample { double T; float FpsAvg, FpsP1, FrameMs, GpuMs; };
	TArray<FPerfSample> PerfRing;
	int32 LastPlayers = -1;
	FVector LastPos = FVector::ZeroVector; bool bHadPos = false; double LastSnapEvent = 0.0;
	int32 SnapCount = 0;
	int64 PrevNet[6] = { 0, 0, 0, 0, 0, 0 };
	double LastNetAt = 0.0;
	int32 PrevErr = 0, PrevWarn = 0;
	FString LastMap; double MapLoadStart = 0.0, MapStart = 0.0, JoinAttemptAt = 0.0, ConsentWaitSince = 0.0;
	int32 NetErrCount = 0;
	int32 RateWindowN = 0; double RateWindowStart = 0.0;
	bool bEnded = false, bSelfTest = false;

	// pieces
	TSharedPtr<FPTKUploader> Uploader;
	TSharedPtr<FPTKUi> Ui;
	TSharedPtr<FPTKInputProcessor> Input;
	TSharedPtr<FPTKLogCapture> LogCap;
	FDelegateHandle PreLoadH, PostLoadH;

	// screenshot
	FString ShotPendingName; TFunction<void(const FString&)> ShotDone; double ShotRequestedAt = 0.0; FDelegateHandle ShotH;
	// script (-PTKScript="sec:cmd;sec:cmd")
	struct FScriptItem { double At; FString Cmd; };
	TArray<FScriptItem> Script; double ScriptT0 = 0.0;
};

#define PTK_EVENT(NameLiteral, ...) UPlaytestKitSubsystem::Event(TEXT(NameLiteral), ##__VA_ARGS__)
