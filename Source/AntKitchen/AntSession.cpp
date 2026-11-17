#include "AntSession.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	// Our lobbies carry this tag: AppID 480 is shared with every other project that tests on it.
	const FName AntTagKey(TEXT("ANTGAME"));
	const FString AntTagValue(TEXT("AntKitchen"));
	const TCHAR* AntMap = TEXT("/Engine/Maps/Entry");
	const int32 AntMaxPlayers = 8;
}

void UAntSession::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (IOnlineSessionPtr S = Sessions())
	{
		S->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UAntSession::OnCreated));
		S->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UAntSession::OnFound));
		S->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UAntSession::OnJoined));
		S->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UAntSession::OnInviteAccepted));
		S->AddOnFindFriendSessionCompleteDelegate_Handle(0, FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &UAntSession::OnFriendFound));
	}
	if (GEngine)
	{
		GEngine->OnNetworkFailure().AddUObject(this, &UAntSession::OnNetFail);
	}
	UE_LOG(LogTemp, Display, TEXT("AntSession: online subsystem = %s, steam = %d, nick = %s"),
		IOnlineSubsystem::Get() ? *IOnlineSubsystem::Get()->GetSubsystemName().ToString() : TEXT("none"), IsSteam() ? 1 : 0, *Nickname());
}

IOnlineSessionPtr UAntSession::Sessions() const
{
	IOnlineSubsystem* Oss = IOnlineSubsystem::Get();
	return Oss ? Oss->GetSessionInterface() : nullptr;
}

bool UAntSession::IsSteam() const
{
	IOnlineSubsystem* Oss = IOnlineSubsystem::Get();
	return Oss && Oss->GetSubsystemName() == STEAM_SUBSYSTEM;
}

FString UAntSession::Nickname() const
{
	IOnlineSubsystem* Oss = IOnlineSubsystem::Get();
	IOnlineIdentityPtr Identity = Oss ? Oss->GetIdentityInterface() : nullptr;
	return Identity.IsValid() ? Identity->GetPlayerNickname(0) : FString();
}

bool UAntSession::HasSession() const
{
	IOnlineSessionPtr S = Sessions();
	return S.IsValid() && S->GetNamedSession(NAME_GameSession) != nullptr;
}

void UAntSession::SetStatus(const FString& Text)
{
	Status = Text;
	UE_LOG(LogTemp, Display, TEXT("AntSession: %s"), *Text);
	OnChanged.Broadcast();
}

void UAntSession::Fail(const FString& Text)
{
	bHostPending = false;
	bJoinPending = false;
	bMenuDone = false;
	SetStatus(Text);
}

void UAntSession::OpenMap(bool bListen)
{
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(AntMap), true, bListen ? TEXT("listen") : TEXT(""));
	}
}

void UAntSession::PlaySolo()
{
	bMenuDone = true;
	Status.Reset();
	IOnlineSessionPtr S = Sessions();
	if (S.IsValid() && S->GetNamedSession(NAME_GameSession))
	{
		S->DestroySession(NAME_GameSession);
	}
	OpenMap(false);
}

void UAntSession::Host()
{
	IOnlineSessionPtr S = Sessions();
	if (!IsSteam() || !S.IsValid())
	{
		Fail(TEXT("Steam не запущен. Запусти Steam и перезапусти игру, тогда кооп заработает."));
		return;
	}
	bMenuDone = true;
	bHostPending = true;
	SetStatus(TEXT("Создаю игру..."));
	if (S->GetNamedSession(NAME_GameSession))
	{
		S->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateWeakLambda(this, [this](FName, bool) { CreateNow(); }));
	}
	else
	{
		CreateNow();
	}
}

void UAntSession::CreateNow()
{
	IOnlineSessionPtr S = Sessions();
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = AntMaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bUsesPresence = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bAllowInvites = true;
	Settings.Set(AntTagKey, AntTagValue, EOnlineDataAdvertisementType::ViaOnlineService);
	if (!S.IsValid() || !S->CreateSession(0, NAME_GameSession, Settings))
	{
		Fail(TEXT("Не удалось создать игру в Steam."));
	}
}

void UAntSession::OnCreated(FName SessionName, bool bOk)
{
	if (!bHostPending)
	{
		return;
	}
	bHostPending = false;
	if (!bOk)
	{
		Fail(TEXT("Steam отказал в создании игры. Проверь, что Steam в сети."));
		return;
	}
	SetStatus(TEXT("Игра создана. Друзья могут зайти через «Найти игры друзей» или через список друзей Steam."));
	OpenMap(true);
}

void UAntSession::Find()
{
	IOnlineSessionPtr S = Sessions();
	if (!IsSteam() || !S.IsValid())
	{
		Fail(TEXT("Steam не запущен. Запусти Steam и перезапусти игру, тогда кооп заработает."));
		return;
	}
	if (bSearching)
	{
		return;
	}
	Games.Reset();
	FriendQueue.Reset();
	bSearching = true;
	bLobbyPhase = true;
	SetStatus(TEXT("Ищу игры..."));
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().SetTimer(SearchTimeout, this, &UAntSession::FinishSearch, 12.f, false);
	}
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = 50;
	Search->bIsLanQuery = false;
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	Search->QuerySettings.Set(AntTagKey, AntTagValue, EOnlineComparisonOp::Equals);
	if (!S->FindSessions(0, Search.ToSharedRef()))
	{
		OnFound(false);
	}
}

void UAntSession::OnFound(bool bOk)
{
	if (!bSearching || !bLobbyPhase)
	{
		return;
	}
	bLobbyPhase = false;
	if (bOk && Search.IsValid())
	{
		for (const FOnlineSessionSearchResult& R : Search->SearchResults)
		{
			AddGame(R, false);
		}
	}
	ReadFriends();
}

void UAntSession::ReadFriends()
{
	// The lobby list only covers nearby regions; friends' games are found directly, wherever they are.
	IOnlineSubsystem* Oss = IOnlineSubsystem::Get();
	IOnlineFriendsPtr Friends = Oss ? Oss->GetFriendsInterface() : nullptr;
	const FString ListName = EFriendsLists::ToString(EFriendsLists::Default);
	if (!Friends.IsValid() || !Friends->ReadFriendsList(0, ListName, FOnReadFriendsListComplete::CreateUObject(this, &UAntSession::OnFriendsRead)))
	{
		FinishSearch();
	}
}

void UAntSession::OnFriendsRead(int32 LocalUserNum, bool bOk, const FString& ListName, const FString& Error)
{
	if (!bSearching)
	{
		return;
	}
	IOnlineSubsystem* Oss = IOnlineSubsystem::Get();
	IOnlineFriendsPtr Friends = Oss ? Oss->GetFriendsInterface() : nullptr;
	TArray<TSharedRef<FOnlineFriend>> List;
	if (bOk && Friends.IsValid() && Friends->GetFriendsList(0, ListName, List))
	{
		for (const TSharedRef<FOnlineFriend>& F : List)
		{
			FriendQueue.Add(F->GetUserId());
		}
	}
	NextFriend();
}

void UAntSession::NextFriend()
{
	IOnlineSessionPtr S = Sessions();
	while (bSearching && S.IsValid() && FriendQueue.Num() > 0)
	{
		const FUniqueNetIdRef Id = FriendQueue.Pop(EAllowShrinking::No);
		// False = this friend is not in our game right now; true = a lookup started, OnFriendFound continues the queue.
		if (S->FindFriendSession(0, *Id))
		{
			return;
		}
	}
	FinishSearch();
}

void UAntSession::OnFriendFound(int32 LocalUserNum, bool bOk, const TArray<FOnlineSessionSearchResult>& Results)
{
	if (!bSearching)
	{
		return;
	}
	if (bOk)
	{
		for (const FOnlineSessionSearchResult& R : Results)
		{
			AddGame(R, true);
		}
	}
	NextFriend();
}

void UAntSession::AddGame(const FOnlineSessionSearchResult& Result, bool bCheckTag)
{
	if (!Result.IsValid())
	{
		return;
	}
	if (bCheckTag)
	{
		FString Tag;
		if (!Result.Session.SessionSettings.Get(AntTagKey, Tag) || Tag != AntTagValue)
		{
			return;
		}
	}
	const FString Id = Result.GetSessionIdStr();
	for (const FAntFoundGame& G : Games)
	{
		if (G.Result.GetSessionIdStr() == Id)
		{
			return;
		}
	}
	const int32 Max = Result.Session.SessionSettings.NumPublicConnections;
	const int32 Now = FMath::Clamp(Max - Result.Session.NumOpenPublicConnections, 1, FMath::Max(Max, 1));
	FAntFoundGame G;
	G.Label = FString::Printf(TEXT("%s  ·  %d/%d"), Result.Session.OwningUserName.IsEmpty() ? TEXT("Игра") : *Result.Session.OwningUserName, Now, Max);
	G.Result = Result;
	Games.Add(G);
	OnChanged.Broadcast();
}

void UAntSession::FinishSearch()
{
	if (!bSearching)
	{
		return;
	}
	bSearching = false;
	bLobbyPhase = false;
	FriendQueue.Reset();
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		World->GetTimerManager().ClearTimer(SearchTimeout);
	}
	SetStatus(Games.Num() > 0
		? FString::Printf(TEXT("Найдено игр: %d. Нажми на игру, чтобы войти."), Games.Num())
		: TEXT("Игр не найдено. Пусть хост нажмёт «Создать игру», потом поищи ещё раз."));
}

void UAntSession::Join(int32 Index)
{
	if (Games.IsValidIndex(Index))
	{
		JoinResult(Games[Index].Result);
	}
}

void UAntSession::JoinResult(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr S = Sessions();
	if (!S.IsValid() || !Result.IsValid())
	{
		Fail(TEXT("Эта игра уже недоступна."));
		return;
	}
	Pending = Result;
	// Steam treats these two as one flag and refuses the join when they differ.
	Pending.Session.SessionSettings.bUsesPresence = true;
	Pending.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	bMenuDone = true;
	bJoinPending = true;
	SetStatus(TEXT("Подключаюсь..."));
	if (S->GetNamedSession(NAME_GameSession))
	{
		S->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateWeakLambda(this, [this](FName, bool) { JoinNow(); }));
	}
	else
	{
		JoinNow();
	}
}

void UAntSession::JoinNow()
{
	IOnlineSessionPtr S = Sessions();
	if (!S.IsValid() || !S->JoinSession(0, NAME_GameSession, Pending))
	{
		Fail(TEXT("Не удалось подключиться к игре."));
	}
}

void UAntSession::OnJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!bJoinPending)
	{
		return;
	}
	bJoinPending = false;
	IOnlineSessionPtr S = Sessions();
	FString Url;
	const bool bOk = Result == EOnJoinSessionCompleteResult::Success || Result == EOnJoinSessionCompleteResult::AlreadyInSession;
	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (bOk && S.IsValid() && PC && S->GetResolvedConnectString(NAME_GameSession, Url) && !Url.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("AntSession: travelling to %s"), *Url);
		PC->ClientTravel(Url, TRAVEL_Absolute);
		return;
	}
	if (S.IsValid())
	{
		S->DestroySession(NAME_GameSession);
	}
	Fail(Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("В этой игре нет мест.") : TEXT("Не удалось подключиться к игре."));
}

void UAntSession::OnInviteAccepted(const bool bOk, const int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
	// Invite accepted in Steam, or "Join game" clicked in the friends list.
	if (bOk && Result.IsValid())
	{
		JoinResult(Result);
	}
}

void UAntSession::Invite()
{
	IOnlineSubsystem* Oss = IOnlineSubsystem::Get();
	IOnlineExternalUIPtr UI = Oss ? Oss->GetExternalUIInterface() : nullptr;
	if (!UI.IsValid() || !HasSession() || !UI->ShowInviteUI(0, NAME_GameSession))
	{
		SetStatus(TEXT("Окно приглашений не открылось. Друзья могут зайти через «Найти игры друзей»."));
	}
}

void UAntSession::ReturnToMenu()
{
	bMenuDone = false;
	Status.Reset();
	Games.Reset();
	IOnlineSessionPtr S = Sessions();
	if (S.IsValid() && S->GetNamedSession(NAME_GameSession))
	{
		S->DestroySession(NAME_GameSession);
	}
	OpenMap(false);
}

void UAntSession::OnNetFail(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	// The engine drops back to the start map by itself; show the menu there with the reason.
	IOnlineSessionPtr S = Sessions();
	if (S.IsValid() && S->GetNamedSession(NAME_GameSession))
	{
		S->DestroySession(NAME_GameSession);
	}
	Fail(FString::Printf(TEXT("Соединение прервано: %s"), ENetworkFailure::ToString(Type)));
}