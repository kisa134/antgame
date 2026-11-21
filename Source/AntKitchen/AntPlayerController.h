#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetSerialization.h"
#include "AntPlayerController.generated.h"

struct FAntToast
{
	FString Text;
	float Time = 0.f;
};

struct FAntDamageNumber
{
	FVector Location = FVector::ZeroVector;
	int32 Amount = 0;
	bool bCrit = false;
	float Time = 0.f;
};

UCLASS()
class ANTKITCHEN_API AAntPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PawnLeavingGame() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Main menu (solo / host / find) on a fresh start, or the pause menu on Esc. */
	void OpenMenu(bool bPause);
	void CloseMenu();
	void ToggleMenu();

	UFUNCTION(Client, Reliable) void ClientToast(const FString& Text);
	UFUNCTION(Client, Unreliable) void ClientDamageNumber(FVector_NetQuantize Location, int32 Amount, bool bCrit);
	/** You killed someone: sound + big marker + streak name ("" if none). */
	UFUNCTION(Client, Reliable) void ClientKillConfirm(const FString& Victim, const FString& Streak);
	UFUNCTION(Server, Reliable) void ServerToggleHuman();
	UFUNCTION(Server, Reliable) void ServerToggleCreature(uint8 Kind);
	void BecomeFly() { ServerToggleCreature(1); }
	void BecomeSpider() { ServerToggleCreature(2); }
	/** [P] play for the other colony (in your nest or while dead). */
	UFUNCTION(Server, Reliable) void ServerSwitchTeam();
	void SwitchTeam() { ServerSwitchTeam(); }
	/** F9 / F10: one more ally / enemy bot (console: AddBot <team> <n>). */
	/** Middle mouse button: a marker in the world the whole team sees for a few seconds ("Трофей", "Человек!", "Сюда", "Дверь"). */
	void PingPressed();
	UFUNCTION(Server, Reliable) void ServerPing(FVector_NetQuantize Location);
	/** The card vote between shifts (keys 1, 2, 3). True if a vote is on and the key was used for it. */
	bool TryVote(int32 Option);
	UFUNCTION(Server, Reliable) void ServerVoteCard(uint8 Option);
	void Vote1() { TryVote(0); }
	void Vote2() { TryVote(1); }
	void Vote3() { TryVote(2); }
	/** The raid and the nest (AntRaidCmd): leave, ready, start, buy, squad size, tools, orders to the squad. */
	UFUNCTION(Server, Reliable) void ServerRaid(uint8 Cmd, int32 A, int32 B);
	/** An order to my squad (AntSquad::EOrder) with the point under the crosshair: [Q] haul, [B] hold. */
	UFUNCTION(Server, Reliable) void ServerSquad(uint8 Order, FVector_NetQuantize Aim);
	void SquadHaul();
	void SquadHold();
	void UseTool0();
	void UseTool1();
	FVector CrosshairPoint() const;
	/** F1: show the controls line again for a while (it fades after the first minute). */
	void ShowHelp();
	float HelpAskedAt = -100.f;
	float NextPingTime = 0.f;
	int32 MyVote = -1;
	void AddAllyBot() { ServerAddBot(0, 1); }
	void AddEnemyBot() { ServerAddBot(1, 1); }

	/** Console: HostGame  -> reopens the current map as a listen server. */
	UFUNCTION(Exec) void HostGame();
	/** Console: JoinGame 192.168.1.10  -> connects to a host. */
	UFUNCTION(Exec) void JoinGame(const FString& Address);
	/** Dev console: Tp -6800 -1500 60  -> teleport your ant. Also -AntTp=x,y,z on the command line. */
	UFUNCTION(Exec) void Tp(float X, float Y, float Z);
	/** Dev console: Peace  -> the AI Human stops hunting (toggle). Also -AntPeace on the command line. */
	UFUNCTION(Exec) void Peace();
	UFUNCTION(Server, Reliable) void ServerTp(FVector Location);
	UFUNCTION(Server, Reliable) void ServerPeace();
	/** Dev console: SwarmTest 30000 -> lays test trails with that many worker ants. */
	UFUNCTION(Exec) void SwarmTest(int32 Ants);
	UFUNCTION(Server, Reliable) void ServerSwarmTest(int32 Ants);
	/** Dev console: AddBot 0 2 -> two more teammates; AddBot 1 3 -> three more fire ants. */
	UFUNCTION(Exec) void AddBot(int32 Team, int32 Count);
	/** Dev console: Grab -> same as pressing E (also -AntGrabAt=seconds). */
	UFUNCTION(Exec) void Grab();
	UFUNCTION(Server, Reliable) void ServerGrab();
	UFUNCTION(Server, Reliable) void ServerAddBot(int32 Team, int32 Count);

	TSharedPtr<class SWidget> MenuWidget;
	/** Filming mode (F8, AntFilm.h): made for the local player only. */
	UPROPERTY() class UAntFilm* Film = nullptr;
	bool bShowScores = false;
	float HitMarkerTime = -100.f;
	bool bHitCrit = false;
	float KillMarkerTime = -100.f;
	FString KillText;
	TArray<FAntToast> Toasts;
	TArray<FAntDamageNumber> DamageNumbers;

protected:
	void RequestToggleHuman();
	void ShowScores();
	void HideScores();
};
