#include "USWeapon.h"
#include "USCharacter.h"
#include "UmayyadStrike.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"

AUSWeapon::AUSWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->CastShadow = false;
	RootComponent = Mesh;

	BlockoutMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockoutMesh"));
	BlockoutMesh->SetupAttachment(Mesh);
	BlockoutMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockoutMesh->SetRelativeScale3D(FVector(0.55f, 0.06f, 0.1f));
	BlockoutMesh->SetRelativeLocation(FVector(20.f, 0.f, 0.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) { BlockoutMesh->SetStaticMesh(Cube.Object); }

	Stats = MakePreset(TEXT("AK"));
}

FUSWeaponStats AUSWeapon::MakePreset(FName Id)
{
	FUSWeaponStats S;
	S.Id = Id;

	// Procedural "7"-shaped spray: vertical climb for ~9 shots, then left/right sway.
	auto BuildPattern = [](float Climb, float Sway, int32 N)
	{
		TArray<FVector2D> P;
		for (int32 i = 0; i < N; ++i)
		{
			const float Up = i < 9 ? Climb * (1.f + i * 0.12f) : Climb * 0.25f;
			const float Side = i < 9 ? FMath::Sin(i * 0.7f) * 0.08f : FMath::Sin((i - 9) * 0.45f) * Sway;
			P.Add(FVector2D(Up, Side));
		}
		return P;
	};

	if (Id == TEXT("AK"))
	{
		S.DisplayName = NSLOCTEXT("US", "AK", "AK-47");
		S.Price = 2700; S.Damage = 36; S.ArmorPenetration = 0.775f; S.RoundsPerMinute = 600;
		S.MagazineSize = 30; S.ReserveAmmo = 90; S.ReloadTime = 2.43f;
		S.RecoilPattern = BuildPattern(0.55f, 0.9f, 30);
	}
	else if (Id == TEXT("M4"))
	{
		S.DisplayName = NSLOCTEXT("US", "M4", "M4A4");
		S.Price = 3100; S.Damage = 33; S.ArmorPenetration = 0.7f; S.RoundsPerMinute = 666;
		S.MagazineSize = 30; S.ReserveAmmo = 90; S.ReloadTime = 3.07f; S.SpreadStanding = 0.12f;
		S.RecoilPattern = BuildPattern(0.42f, 0.7f, 30);
	}
	else if (Id == TEXT("AWP"))
	{
		S.DisplayName = NSLOCTEXT("US", "AWP", "AWP");
		S.Price = 4750; S.Damage = 115; S.ArmorPenetration = 0.975f; S.RoundsPerMinute = 41;
		S.MagazineSize = 5; S.ReserveAmmo = 30; S.ReloadTime = 3.67f; S.bAutomatic = false;
		S.SpreadStanding = 0.02f; S.SpreadMoving = 12.f; S.MoveSpeedMultiplier = 0.8f;
		S.RecoilPattern = { FVector2D(2.5f, 0.f) };
	}
	else if (Id == TEXT("Glock"))
	{
		S.DisplayName = NSLOCTEXT("US", "Glock", "Glock-18");
		S.Price = 200; S.Damage = 30; S.ArmorPenetration = 0.47f; S.RoundsPerMinute = 400;
		S.MagazineSize = 20; S.ReserveAmmo = 120; S.ReloadTime = 2.27f; S.bAutomatic = false;
		S.SpreadMoving = 2.f; S.MoveSpeedMultiplier = 1.f;
		S.RecoilPattern = { FVector2D(0.6f, 0.05f) };
	}
	else if (Id == TEXT("USP"))
	{
		S.DisplayName = NSLOCTEXT("US", "USP", "USP-S");
		S.Price = 200; S.Damage = 35; S.ArmorPenetration = 0.505f; S.RoundsPerMinute = 352;
		S.MagazineSize = 12; S.ReserveAmmo = 24; S.ReloadTime = 2.17f; S.bAutomatic = false;
		S.SpreadMoving = 1.8f; S.MoveSpeedMultiplier = 1.f;
		S.RecoilPattern = { FVector2D(0.7f, 0.f) };
	}
	else if (Id == TEXT("Deagle"))
	{
		S.DisplayName = NSLOCTEXT("US", "Deagle", "Desert Eagle");
		S.Price = 700; S.Damage = 53; S.ArmorPenetration = 0.93f; S.RoundsPerMinute = 267;
		S.MagazineSize = 7; S.ReserveAmmo = 35; S.ReloadTime = 2.2f; S.bAutomatic = false;
		S.SpreadMoving = 4.f; S.MoveSpeedMultiplier = 0.92f;
		S.RecoilPattern = { FVector2D(2.2f, 0.2f) };
	}
	return S;
}

void AUSWeapon::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		AmmoInMag = Stats.MagazineSize;
		AmmoReserve = Stats.ReserveAmmo;
	}
}

void AUSWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUSWeapon, AmmoInMag);
	DOREPLIFETIME(AUSWeapon, AmmoReserve);
}

AUSCharacter* AUSWeapon::GetOwnerCharacter() const
{
	return Cast<AUSCharacter>(GetOwner());
}

float AUSWeapon::GetCurrentSpread() const
{
	const AUSCharacter* C = GetOwnerCharacter();
	if (!C) return Stats.SpreadStanding;

	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	if (Move->IsFalling()) return Stats.SpreadAir;

	const float Speed = C->GetVelocity().Size2D();
	const float MoveAlpha = FMath::Clamp((Speed - 40.f) / 180.f, 0.f, 1.f);   // "counter-strafe" window
	float Spread = FMath::Lerp(Stats.SpreadStanding, Stats.SpreadMoving, MoveAlpha);
	if (C->bIsCrouched) Spread *= 0.7f;
	if (Stats.bAutomatic) Spread += FMath::Min(ShotsInBurst, 10) * 0.06f;      // first-shot accuracy
	return Spread;
}

void AUSWeapon::StartFire()
{
	bTriggerHeld = true;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Interval = 60.f / Stats.RoundsPerMinute;
	const float Delay = FMath::Max(0.f, float(LastFireTime + Interval - Now));
	GetWorldTimerManager().SetTimer(FireTimer, this, &AUSWeapon::FireOnce, Interval, Stats.bAutomatic, Delay > 0.f ? Delay : -1.f);
	if (Delay <= 0.f) FireOnce();
}

void AUSWeapon::StopFire()
{
	bTriggerHeld = false;
	GetWorldTimerManager().ClearTimer(FireTimer);
}

void AUSWeapon::FireOnce()
{
	if (bReloading) return;
	AUSCharacter* C = GetOwnerCharacter();
	if (!C || !C->IsAlive()) { StopFire(); return; }

	if (AmmoInMag <= 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DryFireSound, GetActorLocation());
		StopFire();
		Reload();
		return;
	}

	LastFireTime = GetWorld()->GetTimeSeconds();

	FVector Origin; FRotator ViewRot;
	C->GetActorEyesViewPoint(Origin, ViewRot);

	// Spread cone
	const float Spread = GetCurrentSpread();
	const FVector Dir = FMath::VRandCone(ViewRot.Vector(), FMath::DegreesToRadians(Spread * 0.5f));

	// Local prediction for responsiveness
	FHitResult Hit;
	TraceShot(Origin, Dir, ShotsInBurst, Hit);
	PlayFireFX(Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd, Hit.bBlockingHit, Hit.ImpactNormal,
		Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType_Default);

	// Recoil: learnable pattern + small random jitter
	if (Stats.RecoilPattern.Num() > 0)
	{
		const FVector2D Kick = Stats.RecoilPattern[FMath::Min(ShotsInBurst, Stats.RecoilPattern.Num() - 1)];
		C->ApplyRecoil(Kick.X + FMath::FRandRange(-0.05f, 0.05f), Kick.Y + FMath::FRandRange(-0.05f, 0.05f));
	}
	if (APlayerController* PC = Cast<APlayerController>(C->GetController()))
	{
		if (FireShake) PC->ClientStartCameraShake(FireShake);
	}

	ServerFire(Origin, Dir, ShotsInBurst);
	if (!HasAuthority()) { --AmmoInMag; }
	++ShotsInBurst;

	if (!Stats.bAutomatic) StopFire();
}

void AUSWeapon::TraceShot(const FVector& Origin, const FVector& Dir, int32 ShotIndex, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(USWeaponTrace), true, GetOwner());
	Params.AddIgnoredActor(this);
	Params.bReturnPhysicalMaterial = true;
	const FVector End = Origin + Dir * Stats.Range;
	if (!GetWorld()->LineTraceSingleByChannel(OutHit, Origin, End, COLLISION_WEAPON, Params))
	{
		OutHit.TraceStart = Origin;
		OutHit.TraceEnd = End;
	}
}

void AUSWeapon::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir, int32 ShotIndex)
{
	AUSCharacter* C = GetOwnerCharacter();
	if (!C || !C->IsAlive() || AmmoInMag <= 0 || bReloading) return;
	--AmmoInMag;

	FHitResult Hit;
	TraceShot(Origin, Dir, ShotIndex, Hit);

	const EPhysicalSurface Surface = Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->SurfaceType.GetValue() : SurfaceType_Default;
	if (Hit.bBlockingHit && Hit.GetActor())
	{
		const float Dist = Hit.Distance;
		float Damage = Stats.Damage * FMath::Pow(Stats.RangeFalloffPer500, Dist / 500.f);
		const bool bHead = Hit.BoneName.ToString().Contains(TEXT("head")) || (Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("Head")));
		if (bHead) Damage *= Stats.HeadshotMultiplier;

		if (AUSCharacter* Victim = Cast<AUSCharacter>(Hit.GetActor()))
		{
			Victim->ReceiveWeaponDamage(Damage, Stats.ArmorPenetration, bHead, C, this, Dir);
		}
		else
		{
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Dir, Hit, C->GetController(), this, nullptr);
		}
	}
	MulticastFireFX(Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd, Hit.bBlockingHit, Hit.ImpactNormal, uint8(Surface));
}

void AUSWeapon::MulticastFireFX_Implementation(FVector_NetQuantize End, bool bHit, FVector_NetQuantizeNormal Normal, uint8 Surface)
{
	// Owner already played predicted FX
	const APawn* P = Cast<APawn>(GetOwner());
	if (P && P->IsLocallyControlled()) return;
	PlayFireFX(End, bHit, Normal, EPhysicalSurface(Surface));
}

void AUSWeapon::PlayFireFX(const FVector& End, bool bHit, const FVector& Normal, EPhysicalSurface Surface)
{
	const FVector Muzzle = Mesh->DoesSocketExist(TEXT("Muzzle")) ? Mesh->GetSocketLocation(TEXT("Muzzle"))
		: BlockoutMesh->GetComponentLocation() + GetActorForwardVector() * 30.f;

	if (MuzzleFlash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFlash, Mesh, TEXT("Muzzle"), FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
	if (Tracer)
	{
		if (UNiagaraComponent* T = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Tracer, Muzzle, (End - Muzzle).Rotation()))
		{
			T->SetVectorParameter(TEXT("BeamEnd"), End);
		}
	}
	UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle);
	UGameplayStatics::PlaySoundAtLocation(this, FireTailSound, Muzzle);

	if (bHit)
	{
		const FUSImpactFX* FX = ImpactFX.Find(Surface);
		if (!FX) FX = &DefaultImpact;
		if (FX->Particles) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FX->Particles, End, Normal.Rotation());
		if (FX->Sound) UGameplayStatics::PlaySoundAtLocation(this, FX->Sound, End);
		if (FX->Decal)
		{
			UGameplayStatics::SpawnDecalAtLocation(this, FX->Decal, FVector(4.f, 6.f, 6.f), End, (-Normal).Rotation(), 20.f);
		}
	}
}

void AUSWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Reset spray index once the trigger has been released long enough
	if (!bTriggerHeld && ShotsInBurst > 0 && GetWorld()->GetTimeSeconds() - LastFireTime > 0.35)
	{
		ShotsInBurst = 0;
	}
}

void AUSWeapon::Reload()
{
	if (bReloading || AmmoInMag >= Stats.MagazineSize || AmmoReserve <= 0) return;
	StopFire();
	bReloading = true;
	UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AUSWeapon::FinishReload, Stats.ReloadTime, false);
	if (!HasAuthority()) ServerReload();
}

void AUSWeapon::ServerReload_Implementation()
{
	Reload();
}

void AUSWeapon::FinishReload()
{
	bReloading = false;
	if (HasAuthority())
	{
		const int32 Needed = Stats.MagazineSize - AmmoInMag;
		const int32 Taken = FMath::Min(Needed, AmmoReserve);
		AmmoInMag += Taken;
		AmmoReserve -= Taken;
	}
}
