#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "USTypes.h"
#include "USPlayerController.generated.h"

class USoundBase;

UCLASS()
class UMAYYADSTRIKE_API AUSPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AUSPlayerController();

	/** Arabic voice-over announcer lines (record with a native Damascene voice actor) */
	UPROPERTY(EditAnywhere, Category = "Audio") TMap<EUSAnnouncement, TObjectPtr<USoundBase>> AnnouncerLines;
	/** Stingers: short musical cues built on maqam Hijaz / oud + percussion */
	UPROPERTY(EditAnywhere, Category = "Audio") TMap<EUSAnnouncement, TObjectPtr<USoundBase>> MusicStingers;

	UFUNCTION(Client, Reliable) void ClientAnnounce(EUSAnnouncement What);
	UFUNCTION(Server, Reliable) void ServerBuy(FName ItemId);

	void ToggleBuyMenu();
	bool IsBuyMenuOpen() const { return bBuyMenuOpen; }
	void ShowHitMarker(bool bHeadshot, bool bKill);
	void ShowDamageVignette(float Amount);
	void SetScopeOverlay(bool bOn) { bScopeOverlay = bOn; }

	// Read by the HUD
	float HitMarkerTime = -10.f;
	bool bLastHitHead = false;
	bool bLastHitKill = false;
	float DamageFlash = 0.f;
	bool bScopeOverlay = false;
	FString BannerText;
	float BannerTime = -10.f;

	virtual void PlayerTick(float DeltaTime) override;

protected:
	bool bBuyMenuOpen = false;
};
