#include "PTKUploader.h"
#include "PlaytestKit.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"

void FPTKUploader::Configure(const FString& InEndpoint, const FString& InKey, const FString& InRoot, const FString& InProject, int32 InBatchKB, bool bInShots, float InIntervalSec)
{
	Endpoint = InEndpoint.TrimStartAndEnd();
	while (Endpoint.EndsWith(TEXT("/"))) { Endpoint.LeftChopInline(1); }
	Key = InKey;
	Root = InRoot;
	Project = InProject;
	BatchBytes = FMath::Clamp(InBatchKB, 16, 900) * 1024;
	bShots = bInShots;
	Interval = FMath::Clamp(InIntervalSec, 1.f, 120.f);
}

FPTKUploader::FState& FPTKUploader::StateOf(const FString& Session)
{
	FState& S = States.FindOrAdd(Session);
	if (!S.bLoaded)
	{
		S.bLoaded = true;
		FString Txt;
		if (FFileHelper::LoadFileToString(Txt, *(Root / Session / TEXT("upload.json"))))
		{
			TSharedPtr<FJsonObject> O;
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Txt), O) && O.IsValid())
			{
				S.Offset = (int64)O->GetNumberField(TEXT("offset"));
				S.bMetaSent = O->GetBoolField(TEXT("meta_sent"));
				const TArray<TSharedPtr<FJsonValue>>* F;
				if (O->TryGetArrayField(TEXT("files"), F)) { for (const TSharedPtr<FJsonValue>& V : *F) { S.Files.Add(V->AsString()); } }
			}
		}
	}
	return S;
}

void FPTKUploader::SaveState(const FString& Session)
{
	const FState& S = StateOf(Session);
	FString Files;
	for (const FString& F : S.Files) { Files += (Files.IsEmpty() ? TEXT("\"") : TEXT(",\"")) + F + TEXT("\""); }
	const FString Txt = FString::Printf(TEXT("{\"offset\":%lld,\"meta_sent\":%s,\"files\":[%s]}"), S.Offset, S.bMetaSent ? TEXT("true") : TEXT("false"), *Files);
	FFileHelper::SaveStringToFile(Txt, *(Root / Session / TEXT("upload.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

int64 FPTKUploader::QueuedBytes() const
{
	int64 Total = 0;
	TArray<FString> Dirs;
	IFileManager::Get().FindFiles(Dirs, *(Root / TEXT("*")), false, true);
	for (const FString& D : Dirs)
	{
		const int64 Size = IFileManager::Get().FileSize(*(Root / D / TEXT("events.jsonl")));
		if (Size <= 0) { continue; }
		int64 Off = 0;
		if (const FState* S = States.Find(D)) { Off = S->Offset; }
		else
		{
			FString Txt;
			if (FFileHelper::LoadFileToString(Txt, *(Root / D / TEXT("upload.json"))))
			{
				TSharedPtr<FJsonObject> O;
				if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Txt), O) && O.IsValid()) { Off = (int64)O->GetNumberField(TEXT("offset")); }
			}
		}
		Total += FMath::Max<int64>(0, Size - Off);
	}
	return Total;
}

FString FPTKUploader::Status() const
{
	if (!Enabled()) { return TEXT("upload: OFF (no endpoint; local files only)"); }
	return FString::Printf(TEXT("upload: %s | sent %d batches (%lld KB), failed %d, rejected %d, last http %d %s | queued %lld KB"), *Endpoint, Sent, SentBytes / 1024, Failed, Rejected, LastCode, *LastError, QueuedBytes() / 1024);
}

void FPTKUploader::Done(bool bOk, int32 Code, const FString& Err, bool bPermanent)
{
	bBusy = false;
	LastCode = Code;
	LastError = Err;
	if (bOk)
	{
		Backoff = 2.0;
		if (!FlightFile.IsEmpty())
		{
			StateOf(FlightSession).Files.Add(FlightFile);
		}
		else
		{
			FState& S = StateOf(FlightSession);
			S.Offset += FlightAdvance;
			if (bFlightMeta) { S.bMetaSent = true; }
			SentBytes += FlightAdvance;
		}
		SaveState(FlightSession);
		++Sent;
		NextAttempt = 0.0;   // more may be waiting: go on at the next tick
	}
	else if (bPermanent)
	{
		++Rejected;
		UE_LOG(LogPTK, Warning, TEXT("upload rejected (%d): %s - dropped"), Code, *Err);
		if (!FlightFile.IsEmpty()) { StateOf(FlightSession).Files.Add(FlightFile); }
		else { StateOf(FlightSession).Offset += FlightAdvance; }
		SaveState(FlightSession);
		NextAttempt = FPlatformTime::Seconds() + 1.0;
	}
	else
	{
		++Failed;
		Backoff = FMath::Min(Backoff * 2.0, 120.0);
		NextAttempt = FPlatformTime::Seconds() + Backoff;
		UE_LOG(LogPTK, Log, TEXT("upload failed (%d %s), retry in %.0fs"), Code, *Err, Backoff);
	}
	FlightFile.Reset();
	bFlightMeta = false;
}

void FPTKUploader::Tick(double Now, const FString& CurrentSession)
{
	if (!Enabled() || bBusy || Now < NextAttempt) { return; }
	if (Now - LastDirScan > 30.0)
	{
		LastDirScan = Now;
		TArray<FString> Dirs;
		IFileManager::Get().FindFiles(Dirs, *(Root / TEXT("*")), false, true);
		Dirs.Sort([](const FString& A, const FString& B) { return A > B; });   // newest first (names start with the date)
		OldSessions.Reset();
		for (const FString& D : Dirs) { if (D != CurrentSession && OldSessions.Num() < 12) { OldSessions.Add(D); } }
	}
	// current session first, then the leftovers of the previous launches
	if (!CurrentSession.IsEmpty() && TrySendEvents(CurrentSession, true)) { return; }
	for (const FString& D : OldSessions) { if (TrySendEvents(D, false)) { return; } }
	if (bShots)
	{
		if (!CurrentSession.IsEmpty() && TrySendFile(CurrentSession)) { return; }
		for (const FString& D : OldSessions) { if (TrySendFile(D)) { return; } }
	}
	NextAttempt = Now + Interval;
}

bool FPTKUploader::TrySendEvents(const FString& Session, bool bCurrent)
{
	const FString Path = Root / Session / TEXT("events.jsonl");
	const int64 Size = IFileManager::Get().FileSize(*Path);
	FState& S = StateOf(Session);
	if (Size <= S.Offset) { return false; }
	TUniquePtr<FArchive> Rd(IFileManager::Get().CreateFileReader(*Path, FILEREAD_AllowWrite));
	if (!Rd) { return false; }
	const int64 Want = FMath::Min<int64>(Size - S.Offset, BatchBytes);
	TArray<uint8> Buf;
	Buf.SetNumUninitialized((int32)Want);
	Rd->Seek(S.Offset);
	Rd->Serialize(Buf.GetData(), Want);
	Rd.Reset();
	int32 Cut = Buf.Num();
	while (Cut > 0 && Buf[Cut - 1] != '\n') { --Cut; }
	if (Cut <= 0)
	{
		if (Want < Size - S.Offset) { S.Offset += Want; SaveState(Session); }   // a line longer than a batch: skip it
		return false;
	}
	FUTF8ToTCHAR Conv((const ANSICHAR*)Buf.GetData(), Cut);
	FString Text(Conv.Length(), Conv.Get());
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, true);
	if (Lines.Num() > 600) { Lines.SetNum(600); }
	int64 Consumed = 0;
	FString Events;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		Events += (i ? TEXT(",") : TEXT("")) + Lines[i];
		FTCHARToUTF8 U(*Lines[i]);
		Consumed += U.Length() + 1;
	}
	if (Consumed > Cut) { Consumed = Cut; }
	FString Meta = TEXT("null");
	const bool bSendMeta = !S.bMetaSent;
	if (bSendMeta)
	{
		FString M;
		if (FFileHelper::LoadFileToString(M, *(Root / Session / TEXT("session.json"))) && !M.IsEmpty()) { Meta = M; }
	}
	const FString Body = FString::Printf(TEXT("{\"schema_v\":1,\"project\":\"%s\",\"session_id\":\"%s\",\"offset\":%lld,\"session\":%s,\"events\":[%s]}"), *Project, *Session, S.Offset, *Meta, *Events);
	FTCHARToUTF8 Raw(*Body);
	TArray<uint8> Payload;
	int32 Bound = FCompression::CompressMemoryBound(NAME_Gzip, Raw.Length());
	Payload.SetNumUninitialized(Bound);
	int32 CSize = Bound;
	const bool bGz = FCompression::CompressMemory(NAME_Gzip, Payload.GetData(), CSize, Raw.Get(), Raw.Length());
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Endpoint);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetHeader(TEXT("x-ptk-key"), Key);
	Req->SetHeader(TEXT("x-ptk-project"), Project);
	if (!Key.IsEmpty()) { Req->SetHeader(TEXT("apikey"), Key); }
	if (bGz)
	{
		Payload.SetNum(CSize);
		Req->SetHeader(TEXT("Content-Encoding"), TEXT("gzip"));
		Req->SetContent(MoveTemp(Payload));
	}
	else
	{
		Req->SetContent(TArray<uint8>((const uint8*)Raw.Get(), Raw.Length()));
	}
	Req->SetTimeout(20.f);
	bBusy = true;
	FlightSession = Session;
	FlightAdvance = Consumed;
	FlightFile.Reset();
	bFlightMeta = bSendMeta;
	TWeakPtr<FPTKUploader> Weak = AsShared();
	Req->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		TSharedPtr<FPTKUploader> Self = Weak.Pin();
		if (!Self.IsValid()) { return; }
		if (!bOk || !Resp.IsValid()) { Self->Done(false, 0, TEXT("no response"), false); return; }
		const int32 Code = Resp->GetResponseCode();
		if (Code >= 200 && Code < 300) { Self->Done(true, Code, FString(), false); return; }
		const FString Msg = Resp->GetContentAsString().Left(160);
		const bool bTransient = Code >= 500 || Code == 429 || Code == 408;
		Self->Done(false, Code, Msg, !bTransient);
	});
	Req->ProcessRequest();
	return true;
}

bool FPTKUploader::TrySendFile(const FString& Session)
{
	FState& S = StateOf(Session);
	TArray<FString> Shots;
	IFileManager::Get().FindFiles(Shots, *(Root / Session / TEXT("shots") / TEXT("*.jpg")), true, false);
	for (const FString& F : Shots)
	{
		if (S.Files.Contains(F)) { continue; }
		TArray<uint8> Data;
		if (!FFileHelper::LoadFileToArray(Data, *(Root / Session / TEXT("shots") / F)) || Data.Num() == 0 || Data.Num() > 900 * 1024)
		{
			S.Files.Add(F);
			SaveState(Session);
			continue;
		}
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(Endpoint + TEXT("/file?session=") + Session + TEXT("&name=") + F);
		Req->SetVerb(TEXT("POST"));
		Req->SetHeader(TEXT("Content-Type"), TEXT("image/jpeg"));
		Req->SetHeader(TEXT("x-ptk-key"), Key);
		Req->SetHeader(TEXT("x-ptk-project"), Project);
		if (!Key.IsEmpty()) { Req->SetHeader(TEXT("apikey"), Key); }
		Req->SetContent(MoveTemp(Data));
		Req->SetTimeout(30.f);
		bBusy = true;
		FlightSession = Session;
		FlightAdvance = 0;
		FlightFile = F;
		TWeakPtr<FPTKUploader> Weak = AsShared();
		Req->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			TSharedPtr<FPTKUploader> Self = Weak.Pin();
			if (!Self.IsValid()) { return; }
			if (!bOk || !Resp.IsValid()) { Self->Done(false, 0, TEXT("no response"), false); return; }
			const int32 Code = Resp->GetResponseCode();
			if (Code >= 200 && Code < 300) { Self->Done(true, Code, FString(), false); return; }
			const bool bTransient = Code >= 500 || Code == 429 || Code == 408;
			Self->Done(false, Code, Resp->GetContentAsString().Left(160), !bTransient);
		});
		Req->ProcessRequest();
		return true;
	}
	return false;
}
