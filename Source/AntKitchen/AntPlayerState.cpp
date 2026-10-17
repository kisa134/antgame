#include "AntPlayerState.h"
#include "Net/UnrealNetwork.h"

void AAntPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAntPlayerState, Delivered);
	DOREPLIFETIME(AAntPlayerState, Deaths);
	DOREPLIFETIME(AAntPlayerState, Kills);
	DOREPLIFETIME(AAntPlayerState, bIsHuman);
	DOREPLIFETIME(AAntPlayerState, AntColor);
}
