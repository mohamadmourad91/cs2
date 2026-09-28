#include "USPlayerState.h"
#include "Net/UnrealNetwork.h"

void AUSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUSPlayerState, Team);
	DOREPLIFETIME(AUSPlayerState, Money);
	DOREPLIFETIME(AUSPlayerState, Kills);
	DOREPLIFETIME(AUSPlayerState, Deaths);
	DOREPLIFETIME(AUSPlayerState, Assists);
	DOREPLIFETIME(AUSPlayerState, DamageDealt);
	DOREPLIFETIME(AUSPlayerState, MVPs);
}
