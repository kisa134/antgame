#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"
#include <atomic>

/** Counts errors/warnings of the whole process and keeps the first few error lines for the next drain (any thread). */
class FPTKLogCapture : public FOutputDevice
{
public:
	std::atomic<int32> Errors{ 0 };
	std::atomic<int32> Warnings{ 0 };
	FCriticalSection Lock;
	TArray<TPair<FString, FString>> Queue;

	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Warning) { ++Warnings; return; }
		if (Verbosity != ELogVerbosity::Error && Verbosity != ELogVerbosity::Fatal) { return; }
		if (Category == FName(TEXT("LogPTK"))) { return; }
		++Errors;
		FScopeLock L(&Lock);
		if (Queue.Num() < 20) { Queue.Emplace(Category.ToString(), FString(V).Left(300)); }
	}
};
