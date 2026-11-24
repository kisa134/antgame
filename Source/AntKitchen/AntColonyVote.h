#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "AntColonyVote.generated.h"

class AAntRaid;
class AAntPlayerController;
class AAntKitchenGameMode;
class APlayerState;
class AHUD;
class UCanvas;

/** Who decides what the colony builds: the host alone, or the squad by a vote. */
namespace AntVoteMode { enum : uint8 { Host, Vote, Count }; }

/**
 * Colony decisions in co-op (docs/COLONY.md "Кто решает"). The colony is one per group and lives on the host. With
 * "Голосование" on (the host's switch in the lobby / the nest; saved on his machine) ANY player may propose a build or a dig on
 * the colony screen; everybody gets a prompt (За / Против, 20 s); a majority of those who voted wins, the host's vote breaks a
 * tie; then the server carries the proposal out as if the host had pressed the button. With "Решает хост" it is as before: the
 * host builds, the others watch. Solo: no votes. One small replicated actor carries the mode and the running vote.
 */
UCLASS()
class ANTKITCHEN_API AAntColonyVote : public AInfo
{
	GENERATED_BODY()

public:
	AAntColonyVote();
	virtual void Tick(float Dt) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) uint8 Mode = AntVoteMode::Vote;
	/** A vote is running. */
	UPROPERTY(Replicated) bool bActive = false;
	/** Bumped with every new proposal (the screens notice a new one by it). */
	UPROPERTY(Replicated) uint8 Seq = 0;
	UPROPERTY(Replicated) uint8 Cmd = 0;          // AntRaidCmd: Buy, Dig, Chamber, Larva, Workers
	UPROPERTY(Replicated) int32 Arg = 0;
	UPROPERTY(Replicated) FString What;           // "Прорыть ход к трубе — 120 еды"
	UPROPERTY(Replicated) FString By;
	UPROPERTY(Replicated) float Ends = 0.f;       // server world time
	UPROPERTY(Replicated) TArray<FString> Yes;
	UPROPERTY(Replicated) TArray<FString> No;
	UPROPERTY(Replicated) uint8 Voters = 0;       // players who may vote
	/** How the last vote ended, and when (server time): shown for a few seconds. */
	UPROPERTY(Replicated) FString LastResult;
	UPROPERTY(Replicated) float LastAt = -100.f;
	UPROPERTY(Replicated) bool bLastPassed = false;

	// server
	TArray<TWeakObjectPtr<APlayerState>> VotedPS;
	bool bHostVoted = false;
	bool bHostYes = false;
	bool bExecuting = false;
	void Resolve(bool bTimeUp);
	void Finish(bool bPassed, const FString& Why);
};

namespace AntColonyVote
{
	constexpr float Seconds = 20.f;

	ANTKITCHEN_API AAntColonyVote* Info(const UObject* Ctx);
	/** Votes are on right now: the mode is "Голосование" and there are two players or more. */
	ANTKITCHEN_API bool VotingOn(const UObject* Ctx);
	/** May this machine press the colony's build buttons (the host always; a client when votes are on)? */
	ANTKITCHEN_API bool CanBuild(const UObject* Ctx);
	ANTKITCHEN_API uint8 Mode(const UObject* Ctx);
	/** AAntRaid::PlayerCommand's hook. Handles AntRaidCmdX::VoteMode / Vote, and turns a build command into a proposal when votes
	 *  are on. True: handled, the raid does nothing more with it. */
	ANTKITCHEN_API bool Command(AAntRaid* Raid, AAntPlayerController* PC, uint8 Cmd, int32 A, int32 B);
	/** The prompt, over the nest screens (HUD). */
	ANTKITCHEN_API void DrawHud(AHUD* Hud, UCanvas* Cv, AAntPlayerController* PC);
	/** One line for the screens' hint strips ("Голосование: предложи стройку — отряд решит"). */
	ANTKITCHEN_API FString HintLine(const UObject* Ctx);
	/** Dev (-AntDo): vote mode 0|1 | vote propose dig|chamber|buy N | vote yes|no | vote info | vote bot yes|no (the other players' votes). */
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
