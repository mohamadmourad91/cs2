#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "USTypes.h"
#include "USCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UInputAction;
class UInputMappingContext;
class AUSWeapon;
class USoundBase;
class UAudioComponent;

class AUSCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUSOnDeath, AUSCharacter*, Victim, AController*, Killer);

UCLASS()
class UMAYYADSTRIKE_API AUSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AUSCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Arms;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAudioComponent> BreathAudio;

	// ---- Tunables (CS-like movement) ----
	UPROPERTY(EditAnywhere, Category = "Movement") float RunSpeed = 500.f;     // ~250 u/s in Source ≈ 5 m/s
	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = 260.f;    // shift-walk: silent footsteps
	UPROPERTY(EditAnywhere, Category = "Movement") float CrouchSpeed = 170.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float GroundFriction = 6.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float BaseFOV = 90.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float ScopedFOV = 30.f;

	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> FootstepStone;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> FootstepMetal;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> FootstepWater;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> HitmarkerSound;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> HeadshotSound;
	UPROPERTY(EditAnywhere, Category = "Audio") TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditAnywhere, Category = "Loadout") TSubclassOf<AUSWeapon> WeaponClass;

	// ---- State ----
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly) float Health = 100.f;
	UPROPERTY(Replicated, BlueprintReadOnly) float Armor = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bHasHelmet = false;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bHasDefuseKit = false;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bCarryingBomb = false;
	UPROPERTY(ReplicatedUsing = OnRep_Weapons, BlueprintReadOnly) TArray<TObjectPtr<AUSWeapon>> Weapons;
	UPROPERTY(ReplicatedUsing = OnRep_Weapons, BlueprintReadOnly) int32 ActiveWeaponIndex = 0;

	UPROPERTY(BlueprintAssignable) FUSOnDeath OnDeath;

	UFUNCTION(BlueprintPure) bool IsAlive() const { return Health > 0.f; }
	UFUNCTION(BlueprintPure) AUSWeapon* GetActiveWeapon() const;
	UFUNCTION(BlueprintPure) EUSTeam GetTeam() const;

	void ApplyRecoil(float PitchDeg, float YawDeg);
	void ReceiveWeaponDamage(float Damage, float ArmorPen, bool bHeadshot, AUSCharacter* InstigatorChar, AActor* Causer, const FVector& Dir);

	/** Server: give a weapon by preset id (used by buy menu & round start) */
	void GiveWeapon(FName PresetId, EUSWeaponSlot Slot);
	void ResetForRound();

	UFUNCTION(Server, Reliable) void ServerEquip(int32 Index);
	UFUNCTION(Server, Reliable) void ServerSetUsing(bool bUsing);

	bool IsUsing() const { return bUsingHeld; }
	bool IsWalking() const { return bWalkHeld; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION() void OnRep_Health(float OldHealth);
	UFUNCTION() void OnRep_Weapons();
	UFUNCTION(NetMulticast, Reliable) void MulticastDie(FVector_NetQuantizeNormal Impulse);
	UFUNCTION(Client, Unreliable) void ClientHitConfirm(bool bHeadshot, bool bKill);

	void Die(AController* Killer, const FVector& Dir);
	void AttachWeapons();
	void UpdateMovementSpeed();
	void TickFootsteps(float DeltaSeconds);
	void TickRecoilRecovery(float DeltaSeconds);

	// Input
	void BuildInput();
	void Move(const FInputActionValue& V);
	void Look(const FInputActionValue& V);
	void FirePressed();
	void FireReleased();
	void ReloadPressed();
	void ScopePressed();
	void WalkPressed() { bWalkHeld = true; UpdateMovementSpeed(); ServerSetWalk(true); }
	void WalkReleased() { bWalkHeld = false; UpdateMovementSpeed(); ServerSetWalk(false); }
	void CrouchPressed() { Crouch(); }
	void CrouchReleased() { UnCrouch(); }
	void UsePressed() { bUsingHeld = true; ServerSetUsing(true); }
	void UseReleased() { bUsingHeld = false; ServerSetUsing(false); }
	void Equip1() { ServerEquip(0); }
	void Equip2() { ServerEquip(1); }
	void Equip3() { ServerEquip(2); }
	void Equip4() { ServerEquip(3); }
	void ToggleBuyMenu();

	UFUNCTION(Server, Reliable) void ServerSetWalk(bool bWalk);

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UInputAction>> Actions;

	bool bWalkHeld = false;
	bool bUsingHeld = false;
	bool bScoped = false;
	float FootstepAccumulator = 0.f;
	FVector2D RecoilDebt = FVector2D::ZeroVector;  // how much aim punch to recover
};
