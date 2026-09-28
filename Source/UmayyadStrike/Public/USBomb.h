#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "USBomb.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

/** The planted charge: accelerating beeps, blinking light, cinematic explosion */
UCLASS()
class UMAYYADSTRIKE_API AUSBomb : public AActor
{
	GENERATED_BODY()
public:
	AUSBomb();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Blinker;

	UPROPERTY(EditAnywhere, Category = "Bomb") float FuseTime = 40.f;
	UPROPERTY(EditAnywhere, Category = "Bomb") float DefuseTime = 10.f;
	UPROPERTY(EditAnywhere, Category = "Bomb") float DefuseTimeWithKit = 5.f;
	UPROPERTY(EditAnywhere, Category = "Bomb") float BlastRadius = 1750.f;
	UPROPERTY(EditAnywhere, Category = "Bomb") float BlastDamage = 500.f;

	UPROPERTY(EditAnywhere, Category = "FX") TObjectPtr<UNiagaraSystem> ExplosionFX;
	UPROPERTY(EditAnywhere, Category = "FX") TObjectPtr<USoundBase> BeepSound;
	UPROPERTY(EditAnywhere, Category = "FX") TObjectPtr<USoundBase> ExplosionSound;
	UPROPERTY(EditAnywhere, Category = "FX") TObjectPtr<USoundBase> DefusedSound;
	UPROPERTY(EditAnywhere, Category = "FX") TSubclassOf<UCameraShakeBase> ExplosionShake;

	UPROPERTY(Replicated, BlueprintReadOnly) float ExplodeServerTime = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly) float DefuseProgress = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bDefused = false;

	/** Server: advance defuse by DeltaSeconds. Returns true when complete. */
	bool TickDefuse(float DeltaSeconds, bool bHasKit);
	void ResetDefuse() { DefuseProgress = 0.f; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	void Explode();
	UFUNCTION(NetMulticast, Reliable) void MulticastExplode();
	UFUNCTION(NetMulticast, Reliable) void MulticastDefused();

	float NextBeepTime = 0.f;
	bool bExploded = false;
};
