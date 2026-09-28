#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "USHUD.generated.h"

class SUSOverlay;
class SWidget;

/**
 * Asset-free HUD: Canvas draws the fast-changing gameplay layer (crosshair, bars, ammo,
 * hit markers, vignette, scope). A Slate overlay (SUSOverlay) handles Arabic text shaping
 * (banners, round messages, buy menu), since Canvas text does not shape RTL scripts.
 */
UCLASS()
class UMAYYADSTRIKE_API AUSHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

protected:
	void DrawCrosshair(float Spread);
	void DrawBottomBar();
	void DrawTopScore();
	void DrawKillFeed();
	void DrawProgress();
	void DrawHitMarker();
	void DrawVignette();
	void DrawScope();
	void Box(float X, float Y, float W, float H, const FLinearColor& C);
	void Label(const FString& S, float X, float Y, const FLinearColor& C, float Scale = 1.f, bool bCenter = false);

	TSharedPtr<SUSOverlay> Overlay;
	TSharedPtr<SWidget> OverlayHost;
};
