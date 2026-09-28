#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "USTypes.h"
#include "USGameState.generated.h"

/** One line in the kill feed (top-right) */
USTRUCT(BlueprintType)
struct FUSKillFeedEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Killer;
	UPROPERTY(BlueprintReadOnly) FString Victim;
	UPROPERTY(BlueprintReadOnly) FString Weapon;
	UPROPERTY(BlueprintReadOnly) bool bHeadshot = false;
	UPROPERTY(BlueprintReadOnly) EUSTeam KillerTeam = EUSTeam::None;
	UPROPERTY(BlueprintReadOnly) float ServerTime = 0.f;
};

UCLASS()
class UMAYYADSTRIKE_API AUSGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadOnly) EUSRoundPhase Phase = EUSRoundPhase::Warmup;
	UPROPERTY(Replicated, BlueprintReadOnly) float PhaseEndServerTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundNumber = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 AttackerScore = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 DefenderScore = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) FName PlantedSite = NAME_None;   // "A" (Opera House forecourt) / "B" (Radio & TV plaza)
	UPROPERTY(Replicated, BlueprintReadOnly) FString LastRoundMessage;
	UPROPERTY(Replicated, BlueprintReadOnly) TArray<FUSKillFeedEntry> KillFeed;

	UFUNCTION(BlueprintPure) float GetPhaseTimeRemaining() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
