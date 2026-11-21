#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "AntVersus.generated.h"

class AHumanCharacter;
class AAntKitchenGameMode;
class AAntPlayerController;

/**
 * «Человек против муравьёв» (docs/GAME.md): one player is the Human, the others are ants (1 vs 1-7). It runs on top of a raid:
 * the ants' goal is the raid's own (the quota, the cake), the Human's goal is to keep them under it until the raid's time runs out.
 * The house degrades as he fights: every swing goes into the wreck system.
 *
 * First balance: the Human player has patience (100): a swing costs 10 + 6 per weapon tier (the bat 28), a stomp 12; it comes back
 * at 8 a second after 1.2 s without attacking; at zero he cannot attack ("Терпение лопнуло"). His heavy weapons are slow (their
 * wind-ups). He sees the loads the ants carry only in an x-ray glimpse: 3 s every 15 s (in a normal raid a player Human sees them
 * all the time). The ants have their usual tools.
 */
UCLASS()
class ANTKITCHEN_API AAntVersus : public AInfo
{
	GENERATED_BODY()

public:
	AAntVersus();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AAntVersus* Get(const UWorld* World);
	/** Server: starts the mode; HumanPC plays the Human. */
	static AAntVersus* Start(UWorld* World, AAntPlayerController* HumanPC);

	UPROPERTY(Replicated) bool bActive = false;
	UPROPERTY(Replicated) float Patience = 100.f;
	UPROPERTY(Replicated) float XrayUntil = -100.f;
	UPROPERTY(Replicated) float NextXray = 0.f;
	/** 0 playing, 1 the ants won, 2 the Human won. */
	UPROPERTY(Replicated) uint8 Winner = 0;
	UPROPERTY(Replicated) FString HumanName;

	float LastAttack = -100.f;
	int32 Swings = 0;
	int32 Refused = 0;
	bool bWasRaid = false;

	static constexpr float MaxPatience = 100.f;
	static constexpr float Regen = 8.f;
	static constexpr float XrayEvery = 15.f;
	static constexpr float XrayFor = 3.f;
};

namespace AntVersus
{
	/** -AntVersus on the command line, or the menu button: the host starts the next raid as versus. */
	ANTKITCHEN_API bool& Wanted();
	ANTKITCHEN_API bool Active(const UWorld* World);
	/** Server, from the Human's attacks: false = not enough patience (only a player Human in versus pays). */
	ANTKITCHEN_API bool Spend(AHumanCharacter* H, float Cost);
	ANTKITCHEN_API float SwingCost(const AHumanCharacter* H);
	constexpr float StompCost = 12.f;
	/** The heavy body's keys (HumanPhys.cpp): a kick, a jump-stomp, a charged swing (x the swing's cost). */
	constexpr float KickCost = 8.f;
	constexpr float JumpStompCost = 20.f;
	constexpr float ChargedK = 1.5f;
	/** Admin: «Бесконечное терпение» (the patience never drops). */
	ANTKITCHEN_API bool& InfinitePatience();
	/** May the Human player see the ants' loads now? (Always outside versus.) */
	ANTKITCHEN_API bool XrayOn(const UWorld* World);
	/** HUD lines for the Human player (weapon, patience, x-ray), on top of AAntHUD::DrawHuman. */
	ANTKITCHEN_API FString HudLine(const UWorld* World, const AHumanCharacter* H, float& OutPatience01);
	ANTKITCHEN_API bool DevCommand(AAntKitchenGameMode* GM, const TArray<FString>& A);
}
