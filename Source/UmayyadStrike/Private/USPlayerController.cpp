#include "USPlayerController.h"
#include "USGameMode.h"
#include "Kismet/GameplayStatics.h"

AUSPlayerController::AUSPlayerController()
{
	bEnableClickEvents = true;
}

void AUSPlayerController::ClientAnnounce_Implementation(EUSAnnouncement What)
{
	if (const TObjectPtr<USoundBase>* S = AnnouncerLines.Find(What)) UGameplayStatics::PlaySound2D(this, *S);
	if (const TObjectPtr<USoundBase>* M = MusicStingers.Find(What)) UGameplayStatics::PlaySound2D(this, *M);

	switch (What)
	{
	case EUSAnnouncement::RoundStart:  BannerText = TEXT("!انطلق"); break;
	case EUSAnnouncement::BombPlanted: BannerText = TEXT("تم زرع العبوة"); break;
	case EUSAnnouncement::RoundWon:    BannerText = TEXT("فوز الجولة"); break;
	case EUSAnnouncement::RoundLost:   BannerText = TEXT("خسارة الجولة"); break;
	}
	BannerTime = float(GetWorld()->GetTimeSeconds());
}

void AUSPlayerController::ServerBuy_Implementation(FName ItemId)
{
	if (AUSGameMode* GM = GetWorld()->GetAuthGameMode<AUSGameMode>()) GM->TryBuy(this, ItemId);
}

void AUSPlayerController::ToggleBuyMenu()
{
	bBuyMenuOpen = !bBuyMenuOpen;
	bShowMouseCursor = bBuyMenuOpen;
	if (bBuyMenuOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetIgnoreLookInput(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		ResetIgnoreLookInput();
	}
}

void AUSPlayerController::ShowHitMarker(bool bHeadshot, bool bKill)
{
	HitMarkerTime = float(GetWorld()->GetTimeSeconds());
	bLastHitHead = bHeadshot;
	bLastHitKill = bKill;
}

void AUSPlayerController::ShowDamageVignette(float Amount)
{
	DamageFlash = FMath::Clamp(DamageFlash + Amount / 60.f, 0.f, 1.f);
}

void AUSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	DamageFlash = FMath::FInterpTo(DamageFlash, 0.f, DeltaTime, 2.5f);
}
