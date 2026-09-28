#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "USTypes.h"
#include "USPlayerState.generated.h"

UCLASS()
class UMAYYADSTRIKE_API AUSPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadOnly) EUSTeam Team = EUSTeam::None;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Money = 800;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Kills = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Deaths = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Assists = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 DamageDealt = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 MVPs = 0;

	static constexpr int32 MaxMoney = 16000;
	void AddMoney(int32 Amount) { Money = FMath::Clamp(Money + Amount, 0, MaxMoney); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
