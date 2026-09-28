#include "USBomb.h"
#include "USCharacter.h"
#include "USGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/CameraShakeBase.h"

AUSBomb::AUSBomb()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetRelativeScale3D(FVector(0.35f, 0.25f, 0.12f));
	Body->SetCollisionProfileName(TEXT("NoCollision"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
	RootComponent = Body;

	Blinker = CreateDefaultSubobject<UPointLightComponent>(TEXT("Blinker"));
	Blinker->SetupAttachment(Body);
	Blinker->SetRelativeLocation(FVector(0, 0, 60.f));
	Blinker->SetLightColor(FLinearColor(1.f, 0.05f, 0.02f));
	Blinker->SetIntensity(0.f);
	Blinker->SetAttenuationRadius(300.f);
	Blinker->SetCastShadows(false);
}

void AUSBomb::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		ExplodeServerTime = float(GetWorld()->GetTimeSeconds()) + FuseTime;
	}
}

void AUSBomb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUSBomb, ExplodeServerTime);
	DOREPLIFETIME(AUSBomb, DefuseProgress);
	DOREPLIFETIME(AUSBomb, bDefused);
}

void AUSBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDefused || bExploded) return;

	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float Now = float(GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds());
	const float Remaining = ExplodeServerTime - Now;

	// Beep interval shrinks from 1s to 0.1s as the fuse runs out - the classic tension ramp
	if (Now >= NextBeepTime && Remaining > 0.f)
	{
		const float Interval = FMath::GetMappedRangeValueClamped(FVector2D(FuseTime, 0.f), FVector2D(1.f, 0.1f), Remaining);
		NextBeepTime = Now + Interval;
		UGameplayStatics::PlaySoundAtLocation(this, BeepSound, GetActorLocation());
		Blinker->SetIntensity(8000.f);
	}
	Blinker->SetIntensity(FMath::FInterpTo(Blinker->Intensity, 0.f, DeltaSeconds, 12.f));

	if (HasAuthority() && Remaining <= 0.f) Explode();
}

bool AUSBomb::TickDefuse(float DeltaSeconds, bool bHasKit)
{
	if (bDefused || bExploded) return false;
	DefuseProgress += DeltaSeconds / (bHasKit ? DefuseTimeWithKit : DefuseTime);
	if (DefuseProgress >= 1.f)
	{
		bDefused = true;
		MulticastDefused();
		return true;
	}
	return false;
}

void AUSBomb::Explode()
{
	bExploded = true;
	TArray<AActor*> Ignore;
	for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It)
	{
		AUSCharacter* C = *It;
		const float Dist = FVector::Dist(C->GetActorLocation(), GetActorLocation());
		if (Dist < BlastRadius && C->IsAlive())
		{
			// CS-like gaussian-ish falloff
			const float Falloff = FMath::Exp(-FMath::Square(Dist / (BlastRadius / 3.f)));
			C->ReceiveWeaponDamage(BlastDamage * Falloff, 1.f, false, nullptr, this, (C->GetActorLocation() - GetActorLocation()).GetSafeNormal());
		}
	}
	MulticastExplode();
	if (AUSGameMode* GM = GetWorld()->GetAuthGameMode<AUSGameMode>()) GM->OnBombExploded();
	SetLifeSpan(3.f);
}

void AUSBomb::MulticastExplode_Implementation()
{
	bExploded = true;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionFX, GetActorLocation());
	UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
	// Big screen shake for everyone nearby
	if (ExplosionShake) UGameplayStatics::PlayWorldCameraShake(this, ExplosionShake, GetActorLocation(), 0.f, BlastRadius * 2.f);
	Blinker->SetIntensity(200000.f);
	Blinker->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Blinker->SetAttenuationRadius(6000.f);
}

void AUSBomb::MulticastDefused_Implementation()
{
	bDefused = true;
	Blinker->SetIntensity(0.f);
	UGameplayStatics::PlaySoundAtLocation(this, DefusedSound, GetActorLocation());
}
