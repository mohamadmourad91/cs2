#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "USTypes.h"
#include "USGameMode.generated.h"

class AUSCharacter;
class AUSBomb;
class AUSGameState;

/**
 * Competitive bomb-defusal rules (MR12: first to 13, side swap after 12).
 * Attackers plant at A (Opera House) or B (Radio & TV plaza), fighting through the Sword monument mid; Defenders hold / defuse.
 */
UCLASS()
class UMAYYADSTRIKE_API AUSGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AUSGameMode();

	UPROPERTY(EditAnywhere, Category = "Rules") float WarmupTime = 20.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float BuyTime = 15.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float RoundTime = 115.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float RoundEndTime = 6.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float PlantTime = 3.2f;
	UPROPERTY(EditAnywhere, Category = "Rules") int32 RoundsPerHalf = 12;
	UPROPERTY(EditAnywhere, Category = "Rules") int32 RoundsToWin = 13;
	UPROPERTY(EditAnywhere, Category = "Economy") int32 StartMoney = 800;
	UPROPERTY(EditAnywhere, Category = "Economy") int32 KillReward = 300;
	UPROPERTY(EditAnywhere, Category = "Economy") int32 WinReward = 3250;
	UPROPERTY(EditAnywhere, Category = "Economy") int32 PlantReward = 300;
	UPROPERTY(EditAnywhere, Category = "Economy") int32 DefuseReward = 300;
	UPROPERTY(EditAnywhere, Category = "Economy") TArray<int32> LossBonusLadder = { 1400, 1900, 2400, 2900, 3400 };
	UPROPERTY(EditAnywhere, Category = "Classes") TSubclassOf<AUSBomb> BombClass;

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void StartPlay() override;

	void OnPlayerKilled(AUSCharacter* Victim, AController* Killer);
	void OnBombExploded();

	/** Server-side buy (called from the player controller RPC) */
	bool TryBuy(APlayerController* PC, FName ItemId);

protected:
	void SetPhase(EUSRoundPhase NewPhase, float Duration);
	void StartRound();
	void EndRound(EUSTeam Winner, const FString& Reason);
	void RespawnAll();
	void AssignBomb();
	void TickPlantAndDefuse(float DeltaSeconds);
	void CheckElimination();
	int32 CountAlive(EUSTeam Team) const;
	void SwapSides();
	AUSGameState* GS() const;

	UPROPERTY(Transient) TObjectPtr<AUSBomb> PlantedBomb;
	UPROPERTY(Transient) TMap<TObjectPtr<AController>, float> PlantProgress;
	int32 AttackerLossStreak = 0;
	int32 DefenderLossStreak = 0;
	FTimerHandle PhaseTimer;
};
