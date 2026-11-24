#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

/**
 * Batched HTTPS uploader. The local files are the queue: Saved/Playtest/<session>/events.jsonl is read from the offset kept in
 * upload.json, so an offline machine simply keeps the tail and the next launch sends it (current session first, then older ones).
 * One request in flight; exponential backoff on network errors / 5xx / 429; other 4xx drop the batch (counted) so one bad line
 * never blocks the queue. Screenshots go one by one to <endpoint>/file.
 */
class FPTKUploader : public TSharedFromThis<FPTKUploader>
{
public:
	void Configure(const FString& InEndpoint, const FString& InKey, const FString& InRoot, const FString& InProject, int32 InBatchKB, bool bInShots, float InIntervalSec);
	bool Enabled() const { return !Endpoint.IsEmpty(); }
	/** Once a second from the subsystem. */
	void Tick(double Now, const FString& CurrentSession);
	void KickNow() { NextAttempt = 0.0; }
	FString Status() const;
	int64 QueuedBytes() const;

private:
	struct FState { int64 Offset = 0; bool bMetaSent = false; TSet<FString> Files; bool bLoaded = false; };
	FState& StateOf(const FString& Session);
	void SaveState(const FString& Session);
	bool TrySendEvents(const FString& Session, bool bCurrent);
	bool TrySendFile(const FString& Session);
	void Done(bool bOk, int32 Code, const FString& Err, bool bPermanent);

	FString Endpoint, Key, Root, Project;
	int32 BatchBytes = 128 * 1024;
	bool bShots = true;
	float Interval = 5.f;
	bool bBusy = false;
	double NextAttempt = 0.0;
	double Backoff = 2.0;
	int32 LastCode = 0;
	int32 Sent = 0, Rejected = 0, Failed = 0;
	int64 SentBytes = 0;
	FString LastError;
	TMap<FString, FState> States;
	// the batch in flight
	FString FlightSession; int64 FlightAdvance = 0; FString FlightFile; bool bFlightMeta = false;
	double LastDirScan = -100.0;
	TArray<FString> OldSessions;
};
