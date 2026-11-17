#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "AntSession.generated.h"

class UNetDriver;

struct FAntFoundGame
{
	FString Label;
	FOnlineSessionSearchResult Result;
};

/**
 * Co-op over Steam: host a lobby, find friends' lobbies, join, accept invites. Lives in the game instance, so it
 * survives map travel. Without Steam the game still runs solo.
 */
UCLASS()
class ANTKITCHEN_API UAntSession : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsSteam() const;
	FString Nickname() const;
	bool HasSession() const;

	void PlaySolo();
	void Host();
	void Find();
	void Join(int32 Index);
	void Invite();
	void ReturnToMenu();

	/** The main menu was answered (solo / host / join); do not show it again after the map reloads. */
	bool bMenuDone = false;
	bool bSearching = false;
	FString Status;
	TArray<FAntFoundGame> Games;
	FSimpleMulticastDelegate OnChanged;

private:
	IOnlineSessionPtr Sessions() const;
	void SetStatus(const FString& Text);
	void OpenMap(bool bListen);
	void CreateNow();
	void JoinResult(const FOnlineSessionSearchResult& Result);
	void JoinNow();
	void ReadFriends();
	void NextFriend();
	void AddGame(const FOnlineSessionSearchResult& Result, bool bCheckTag);
	void FinishSearch();
	void Fail(const FString& Text);

	void OnCreated(FName SessionName, bool bOk);
	void OnFound(bool bOk);
	void OnJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnInviteAccepted(const bool bOk, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result);
	void OnFriendsRead(int32 LocalUserNum, bool bOk, const FString& ListName, const FString& Error);
	void OnFriendFound(int32 LocalUserNum, bool bOk, const TArray<FOnlineSessionSearchResult>& Results);
	void OnNetFail(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);

	TSharedPtr<FOnlineSessionSearch> Search;
	FOnlineSessionSearchResult Pending;
	TArray<FUniqueNetIdRef> FriendQueue;
	FTimerHandle SearchTimeout;
	bool bHostPending = false;
	bool bJoinPending = false;
	bool bLobbyPhase = false;
};