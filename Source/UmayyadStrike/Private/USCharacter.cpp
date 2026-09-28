#include "USCharacter.h"
#include "USWeapon.h"
#include "USPlayerState.h"
#include "USPlayerController.h"
#include "USGameMode.h"
#include "UmayyadStrike.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Algo/Sort.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Net/UnrealNetwork.h"

AUSCharacter::AUSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);

	// Third-person mesh (seen by others) blocks weapon traces per-bone via its physics asset
	GetMesh()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
	GetMesh()->SetOwnerNoSee(true);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(BaseFOV);

	Arms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arms"));
	Arms->SetupAttachment(Camera);
	Arms->SetOnlyOwnerSee(true);
	Arms->CastShadow = false;
	Arms->SetRelativeLocation(FVector(10.f, 12.f, -22.f));

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(Camera);
	BreathAudio->bAutoActivate = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = RunSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->GroundFriction = GroundFriction;
	Move->BrakingDecelerationWalking = 3200.f;   // snappy counter-strafe
	Move->MaxAcceleration = 3800.f;
	Move->JumpZVelocity = 460.f;
	Move->AirControl = 0.3f;
	Move->NavAgentProps.bCanCrouch = true;
	Move->SetCrouchedHalfHeight(62.f);

	bUseControllerRotationYaw = true;
	WeaponClass = AUSWeapon::StaticClass();
}

void AUSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUSCharacter, Health);
	DOREPLIFETIME(AUSCharacter, Armor);
	DOREPLIFETIME(AUSCharacter, bHasHelmet);
	DOREPLIFETIME(AUSCharacter, bHasDefuseKit);
	DOREPLIFETIME(AUSCharacter, bCarryingBomb);
	DOREPLIFETIME(AUSCharacter, Weapons);
	DOREPLIFETIME(AUSCharacter, ActiveWeaponIndex);
}

void AUSCharacter::BeginPlay()
{
	Super::BeginPlay();
	Camera->SetFieldOfView(BaseFOV);
}

void AUSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// PlayerState (and therefore team) is only known once possessed
	if (HasAuthority() && Weapons.Num() == 0)
	{
		GiveWeapon(GetTeam() == EUSTeam::Defenders ? TEXT("USP") : TEXT("Glock"), EUSWeaponSlot::Secondary);
	}
}

EUSTeam AUSCharacter::GetTeam() const
{
	const AUSPlayerState* PS = GetPlayerState<AUSPlayerState>();
	return PS ? PS->Team : EUSTeam::None;
}

AUSWeapon* AUSCharacter::GetActiveWeapon() const
{
	return Weapons.IsValidIndex(ActiveWeaponIndex) ? Weapons[ActiveWeaponIndex].Get() : nullptr;
}

void AUSCharacter::GiveWeapon(FName PresetId, EUSWeaponSlot Slot)
{
	if (!HasAuthority()) return;

	// Replace existing weapon in same slot
	for (int32 i = Weapons.Num() - 1; i >= 0; --i)
	{
		if (Weapons[i] && Weapons[i]->Slot == Slot) { Weapons[i]->Destroy(); Weapons.RemoveAt(i); }
	}

	FActorSpawnParameters P;
	P.Owner = this;
	P.Instigator = this;
	AUSWeapon* W = GetWorld()->SpawnActor<AUSWeapon>(WeaponClass ? *WeaponClass : AUSWeapon::StaticClass(), GetActorTransform(), P);
	W->Stats = AUSWeapon::MakePreset(PresetId);
	W->Slot = Slot;
	W->AmmoInMag = W->Stats.MagazineSize;
	W->AmmoReserve = W->Stats.ReserveAmmo;

	Weapons.Add(W);
	Algo::Sort(Weapons, [](const TObjectPtr<AUSWeapon>& A, const TObjectPtr<AUSWeapon>& B) { return uint8(A->Slot) < uint8(B->Slot); });
	ActiveWeaponIndex = Weapons.IndexOfByKey(W);
	AttachWeapons();
	UpdateMovementSpeed();
}

void AUSCharacter::OnRep_Weapons() { AttachWeapons(); }

void AUSCharacter::AttachWeapons()
{
	for (int32 i = 0; i < Weapons.Num(); ++i)
	{
		AUSWeapon* W = Weapons[i];
		if (!W) continue;
		USceneComponent* Parent = IsLocallyControlled() ? static_cast<USceneComponent*>(Camera) : static_cast<USceneComponent*>(GetMesh());
		W->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			IsLocallyControlled() ? NAME_None : FName(TEXT("hand_r")));
		if (IsLocallyControlled()) W->SetActorRelativeLocation(FVector(35.f, 14.f, -18.f));
		W->SetActorHiddenInGame(i != ActiveWeaponIndex);
	}
}

void AUSCharacter::ServerEquip_Implementation(int32 Index)
{
	if (!Weapons.IsValidIndex(Index) || Index == ActiveWeaponIndex) return;
	if (AUSWeapon* Old = GetActiveWeapon()) Old->StopFire();
	ActiveWeaponIndex = Index;
	AttachWeapons();
	UpdateMovementSpeed();
}

void AUSCharacter::ServerSetWalk_Implementation(bool bWalk) { bWalkHeld = bWalk; UpdateMovementSpeed(); }
void AUSCharacter::ServerSetUsing_Implementation(bool bUsing) { bUsingHeld = bUsing; }

void AUSCharacter::UpdateMovementSpeed()
{
	const AUSWeapon* W = GetActiveWeapon();
	const float WeaponMul = W ? W->Stats.MoveSpeedMultiplier : 1.f;
	const float ScopeMul = bScoped ? 0.6f : 1.f;
	GetCharacterMovement()->MaxWalkSpeed = (bWalkHeld ? WalkSpeed : RunSpeed) * WeaponMul * ScopeMul;
}

void AUSCharacter::ResetForRound()
{
	Health = 100.f;
	bCarryingBomb = false;
	RecoilDebt = FVector2D::ZeroVector;
	for (AUSWeapon* W : Weapons)
	{
		if (W) { W->AmmoInMag = W->Stats.MagazineSize; W->AmmoReserve = W->Stats.ReserveAmmo; }
	}
}

void AUSCharacter::ApplyRecoil(float PitchDeg, float YawDeg)
{
	// Real aim change in degrees (bypasses legacy input scaling); half recovers over time (CS-style)
	if (AController* C = GetController())
	{
		C->SetControlRotation(C->GetControlRotation() + FRotator(PitchDeg, YawDeg, 0.f));
	}
	RecoilDebt += FVector2D(PitchDeg, YawDeg) * 0.5f;
}

void AUSCharacter::TickRecoilRecovery(float DeltaSeconds)
{
	const AUSWeapon* W = GetActiveWeapon();
	if (!W || RecoilDebt.IsNearlyZero(0.001f)) return;
	const FVector2D Step = RecoilDebt * FMath::Clamp(W->Stats.RecoilRecoverySpeed * DeltaSeconds, 0.f, 1.f);
	if (AController* C = GetController())
	{
		C->SetControlRotation(C->GetControlRotation() - FRotator(Step.X, Step.Y, 0.f));
	}
	RecoilDebt -= Step;
}

void AUSCharacter::ReceiveWeaponDamage(float Damage, float ArmorPen, bool bHeadshot, AUSCharacter* InstigatorChar, AActor* Causer, const FVector& Dir)
{
	if (!HasAuthority() || !IsAlive()) return;

	// No friendly fire in competitive default
	if (InstigatorChar && InstigatorChar != this && InstigatorChar->GetTeam() == GetTeam() && GetTeam() != EUSTeam::None) return;

	const bool bArmorApplies = Armor > 0.f && (!bHeadshot || bHasHelmet);
	float HealthDamage = Damage;
	if (bArmorApplies)
	{
		HealthDamage = Damage * ArmorPen;
		const float ArmorDamage = (Damage - HealthDamage) * 0.5f;
		Armor = FMath::Max(0.f, Armor - ArmorDamage);
	}

	const float Old = Health;
	Health = FMath::Max(0.f, Health - HealthDamage);
	OnRep_Health(Old);

	const bool bKill = Health <= 0.f;
	if (InstigatorChar) InstigatorChar->ClientHitConfirm(bHeadshot, bKill);
	if (AUSPlayerState* IPS = InstigatorChar ? InstigatorChar->GetPlayerState<AUSPlayerState>() : nullptr)
	{
		IPS->DamageDealt += FMath::RoundToInt(Old - Health);
	}

	// Tagging: getting shot slows you down
	GetCharacterMovement()->Velocity *= 0.5f;

	if (bKill) Die(InstigatorChar ? InstigatorChar->GetController() : nullptr, Dir);
}

void AUSCharacter::ClientHitConfirm_Implementation(bool bHeadshot, bool bKill)
{
	UGameplayStatics::PlaySound2D(this, bHeadshot ? HeadshotSound : HitmarkerSound);
	if (AUSPlayerController* PC = Cast<AUSPlayerController>(GetController()))
	{
		PC->ShowHitMarker(bHeadshot, bKill);
	}
}

void AUSCharacter::OnRep_Health(float OldHealth)
{
	if (IsLocallyControlled() && Health < OldHealth)
	{
		if (AUSPlayerController* PC = Cast<AUSPlayerController>(GetController()))
		{
			PC->ShowDamageVignette(OldHealth - Health);
		}
	}
}

void AUSCharacter::Die(AController* Killer, const FVector& Dir)
{
	for (AUSWeapon* W : Weapons) if (W) W->StopFire();
	OnDeath.Broadcast(this, Killer);
	if (AUSGameMode* GM = GetWorld()->GetAuthGameMode<AUSGameMode>())
	{
		GM->OnPlayerKilled(this, Killer);
	}
	MulticastDie(Dir);
	SetLifeSpan(20.f);
}

void AUSCharacter::MulticastDie_Implementation(FVector_NetQuantizeNormal Impulse)
{
	UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetMesh()->SetOwnerNoSee(false);
	if (GetMesh()->GetPhysicsAsset())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->AddImpulse(FVector(Impulse) * 6000.f, NAME_None, true);
	}
	for (AUSWeapon* W : Weapons) if (W) W->SetActorHiddenInGame(true);
}

void AUSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsLocallyControlled())
	{
		TickRecoilRecovery(DeltaSeconds);
		const float TargetFOV = bScoped ? ScopedFOV : BaseFOV;
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, 18.f));
	}
	TickFootsteps(DeltaSeconds);
}

void AUSCharacter::TickFootsteps(float DeltaSeconds)
{
	// Walking (shift) and crouching are silent - core CS information game
	const float Speed = GetVelocity().Size2D();
	if (!IsAlive() || GetCharacterMovement()->IsFalling() || bWalkHeld || bIsCrouched || Speed < 150.f) return;

	FootstepAccumulator += Speed * DeltaSeconds;
	if (FootstepAccumulator < 170.f) return;
	FootstepAccumulator = 0.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Footstep), false, this);
	Params.bReturnPhysicalMaterial = true;
	const FVector Start = GetActorLocation();
	USoundBase* Sound = FootstepStone;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 150.f), ECC_Visibility, Params) && Hit.PhysMaterial.IsValid())
	{
		switch (Hit.PhysMaterial->SurfaceType)
		{
		case SurfaceType2: Sound = FootstepMetal; break;
		case SurfaceType3: Sound = FootstepWater; break;
		default: break;
		}
	}
	UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.bBlockingHit ? Hit.ImpactPoint : Start);
}

// ------------------------------------------------------------------ Input
// Actions are created in code so the project runs with zero input assets.

void AUSCharacter::BuildInput()
{
	if (Mapping) return;
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Umayyad"));

	auto MakeAction = [this](FName Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		Actions.Add(Name, A);
		return A;
	};

	UInputAction* MoveA = MakeAction(TEXT("Move"), EInputActionValueType::Axis2D);
	UInputAction* LookA = MakeAction(TEXT("Look"), EInputActionValueType::Axis2D);

	// WASD -> Axis2D using modifiers
	{
		FEnhancedActionKeyMapping& W = Mapping->MapKey(MoveA, EKeys::W);
		UInputModifierSwizzleAxis* Sw = NewObject<UInputModifierSwizzleAxis>(this);
		W.Modifiers.Add(Sw);

		FEnhancedActionKeyMapping& S = Mapping->MapKey(MoveA, EKeys::S);
		UInputModifierSwizzleAxis* Sw2 = NewObject<UInputModifierSwizzleAxis>(this);
		S.Modifiers.Add(Sw2);
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		Mapping->MapKey(MoveA, EKeys::D);
		FEnhancedActionKeyMapping& A = Mapping->MapKey(MoveA, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	{
		Mapping->MapKey(LookA, EKeys::Mouse2D);
	}

	Mapping->MapKey(MakeAction(TEXT("Jump"), EInputActionValueType::Boolean), EKeys::SpaceBar);
	Mapping->MapKey(Actions[TEXT("Jump")], EKeys::MouseScrollDown);
	Mapping->MapKey(MakeAction(TEXT("Fire"), EInputActionValueType::Boolean), EKeys::LeftMouseButton);
	Mapping->MapKey(MakeAction(TEXT("Scope"), EInputActionValueType::Boolean), EKeys::RightMouseButton);
	Mapping->MapKey(MakeAction(TEXT("Reload"), EInputActionValueType::Boolean), EKeys::R);
	Mapping->MapKey(MakeAction(TEXT("Crouch"), EInputActionValueType::Boolean), EKeys::LeftControl);
	Mapping->MapKey(MakeAction(TEXT("Walk"), EInputActionValueType::Boolean), EKeys::LeftShift);
	Mapping->MapKey(MakeAction(TEXT("Use"), EInputActionValueType::Boolean), EKeys::E);
	Mapping->MapKey(MakeAction(TEXT("Buy"), EInputActionValueType::Boolean), EKeys::B);
	Mapping->MapKey(MakeAction(TEXT("Slot1"), EInputActionValueType::Boolean), EKeys::One);
	Mapping->MapKey(MakeAction(TEXT("Slot2"), EInputActionValueType::Boolean), EKeys::Two);
	Mapping->MapKey(MakeAction(TEXT("Slot3"), EInputActionValueType::Boolean), EKeys::Three);
	Mapping->MapKey(MakeAction(TEXT("Slot4"), EInputActionValueType::Boolean), EKeys::Four);
}

void AUSCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	BuildInput();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->ClearAllMappings();
			Sub->AddMappingContext(Mapping, 0);
		}
	}
}

void AUSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	EIC->BindAction(Actions[TEXT("Move")], ETriggerEvent::Triggered, this, &AUSCharacter::Move);
	EIC->BindAction(Actions[TEXT("Look")], ETriggerEvent::Triggered, this, &AUSCharacter::Look);
	EIC->BindAction(Actions[TEXT("Jump")], ETriggerEvent::Started, this, &ACharacter::Jump);
	EIC->BindAction(Actions[TEXT("Jump")], ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EIC->BindAction(Actions[TEXT("Fire")], ETriggerEvent::Started, this, &AUSCharacter::FirePressed);
	EIC->BindAction(Actions[TEXT("Fire")], ETriggerEvent::Completed, this, &AUSCharacter::FireReleased);
	EIC->BindAction(Actions[TEXT("Scope")], ETriggerEvent::Started, this, &AUSCharacter::ScopePressed);
	EIC->BindAction(Actions[TEXT("Reload")], ETriggerEvent::Started, this, &AUSCharacter::ReloadPressed);
	EIC->BindAction(Actions[TEXT("Crouch")], ETriggerEvent::Started, this, &AUSCharacter::CrouchPressed);
	EIC->BindAction(Actions[TEXT("Crouch")], ETriggerEvent::Completed, this, &AUSCharacter::CrouchReleased);
	EIC->BindAction(Actions[TEXT("Walk")], ETriggerEvent::Started, this, &AUSCharacter::WalkPressed);
	EIC->BindAction(Actions[TEXT("Walk")], ETriggerEvent::Completed, this, &AUSCharacter::WalkReleased);
	EIC->BindAction(Actions[TEXT("Use")], ETriggerEvent::Started, this, &AUSCharacter::UsePressed);
	EIC->BindAction(Actions[TEXT("Use")], ETriggerEvent::Completed, this, &AUSCharacter::UseReleased);
	EIC->BindAction(Actions[TEXT("Buy")], ETriggerEvent::Started, this, &AUSCharacter::ToggleBuyMenu);
	EIC->BindAction(Actions[TEXT("Slot1")], ETriggerEvent::Started, this, &AUSCharacter::Equip1);
	EIC->BindAction(Actions[TEXT("Slot2")], ETriggerEvent::Started, this, &AUSCharacter::Equip2);
	EIC->BindAction(Actions[TEXT("Slot3")], ETriggerEvent::Started, this, &AUSCharacter::Equip3);
	EIC->BindAction(Actions[TEXT("Slot4")], ETriggerEvent::Started, this, &AUSCharacter::Equip4);
}

void AUSCharacter::Move(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();   // X = right, Y = forward
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), In.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), In.X);
}

void AUSCharacter::Look(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	const float Sens = bScoped ? 0.35f : 1.f;
	AddControllerYawInput(In.X * Sens);
	AddControllerPitchInput(-In.Y * Sens);
}

void AUSCharacter::FirePressed()
{
	if (AUSPlayerController* PC = Cast<AUSPlayerController>(GetController()))
	{
		if (PC->IsBuyMenuOpen()) return;
	}
	if (AUSWeapon* W = GetActiveWeapon()) W->StartFire();
}

void AUSCharacter::FireReleased()
{
	if (AUSWeapon* W = GetActiveWeapon()) W->StopFire();
}

void AUSCharacter::ReloadPressed()
{
	if (AUSWeapon* W = GetActiveWeapon()) W->Reload();
}

void AUSCharacter::ScopePressed()
{
	const AUSWeapon* W = GetActiveWeapon();
	bScoped = W && W->Stats.Id == TEXT("AWP") ? !bScoped : false;
	UpdateMovementSpeed();
	if (AUSPlayerController* PC = Cast<AUSPlayerController>(GetController())) PC->SetScopeOverlay(bScoped);
}

void AUSCharacter::ToggleBuyMenu()
{
	if (AUSPlayerController* PC = Cast<AUSPlayerController>(GetController())) PC->ToggleBuyMenu();
}
