#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "USTypes.h"
#include "USWeapon.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;
class UMaterialInterface;
class AUSCharacter;

/** Surface-specific impact feedback (stone, marble, metal, glass, water...) */
USTRUCT(BlueprintType)
struct FUSImpactFX
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TObjectPtr<UNiagaraSystem> Particles = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> Sound = nullptr;
	UPROPERTY(EditAnywhere) TObjectPtr<UMaterialInterface> Decal = nullptr;
};

UCLASS(Blueprintable)
class UMAYYADSTRIKE_API AUSWeapon : public AActor
{
	GENERATED_BODY()

public:
	AUSWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Mesh;
	/** Placeholder body used until a real skeletal gun mesh is assigned */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BlockoutMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") FUSWeaponStats Stats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") EUSWeaponSlot Slot = EUSWeaponSlot::Primary;

	UPROPERTY(EditAnywhere, Category = "FX") TObjectPtr<UNiagaraSystem> MuzzleFlash;
	UPROPERTY(EditAnywhere, Category = "FX") TObjectPtr<UNiagaraSystem> Tracer;
	UPROPERTY(EditAnywhere, Category = "FX") TMap<TEnumAsByte<EPhysicalSurface>, FUSImpactFX> ImpactFX;
	UPROPERTY(EditAnywhere, Category = "FX") FUSImpactFX DefaultImpact;
	UPROPERTY(EditAnywhere, Category = "FX") TSubclassOf<UCameraShakeBase> FireShake;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> FireSound;       // close layer
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> FireTailSound;   // distant echo off the square's facades
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> DryFireSound;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(Replicated, BlueprintReadOnly) int32 AmmoInMag = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 AmmoReserve = 0;

	void StartFire();
	void StopFire();
	void Reload();
	bool IsReloading() const { return bReloading; }

	/** Called by the character; returns current spread in degrees */
	float GetCurrentSpread() const;

	UFUNCTION(BlueprintCallable) static FUSWeaponStats MakePreset(FName Id);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void FireOnce();
	void FinishReload();

	UFUNCTION(Server, Reliable) void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir, int32 ShotIndex);
	UFUNCTION(NetMulticast, Unreliable) void MulticastFireFX(FVector_NetQuantize End, bool bHit, FVector_NetQuantizeNormal Normal, uint8 Surface);
	UFUNCTION(Server, Reliable) void ServerReload();

	void TraceShot(const FVector& Origin, const FVector& Dir, int32 ShotIndex, FHitResult& OutHit) const;
	void PlayFireFX(const FVector& End, bool bHit, const FVector& Normal, EPhysicalSurface Surface);
	AUSCharacter* GetOwnerCharacter() const;

	FTimerHandle FireTimer;
	FTimerHandle ReloadTimer;
	bool bTriggerHeld = false;
	bool bReloading = false;
	int32 ShotsInBurst = 0;
	double LastFireTime = -1000.0;
	FVector2D PendingRecoil = FVector2D::ZeroVector;
};
