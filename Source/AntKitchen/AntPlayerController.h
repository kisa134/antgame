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

	UFUNCTION(Client, Reliable) void ClientToast(const FString& Text);
	UFUNCTION(Client, Unreliable) void ClientDamageNumber(FVector_NetQuantize Location, int32 Amount, bool bCrit);
	UFUNCTION(Server, Reliable) void ServerToggleHuman();

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

	bool bShowScores = false;
	TArray<FAntToast> Toasts;
	TArray<FAntDamageNumber> DamageNumbers;

protected:
	void RequestToggleHuman();
	void ShowScores();
	void HideScores();
};
