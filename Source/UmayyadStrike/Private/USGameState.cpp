#include "USGameState.h"
#include "Net/UnrealNetwork.h"

float AUSGameState::GetPhaseTimeRemaining() const
{
	return FMath::Max(0.f, PhaseEndServerTime - float(GetServerWorldTimeSeconds()));
}

void AUSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUSGameState, Phase);
	DOREPLIFETIME(AUSGameState, PhaseEndServerTime);
	DOREPLIFETIME(AUSGameState, RoundNumber);
	DOREPLIFETIME(AUSGameState, AttackerScore);
	DOREPLIFETIME(AUSGameState, DefenderScore);
	DOREPLIFETIME(AUSGameState, PlantedSite);
	DOREPLIFETIME(AUSGameState, LastRoundMessage);
	DOREPLIFETIME(AUSGameState, KillFeed);
}
