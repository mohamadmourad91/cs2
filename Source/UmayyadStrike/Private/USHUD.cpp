#include "USHUD.h"
#include "USOverlay.h"
#include "USCharacter.h"
#include "USWeapon.h"
#include "USBomb.h"
#include "USGameState.h"
#include "USPlayerState.h"
#include "USPlayerController.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Widgets/SWeakWidget.h"

namespace USColors
{
	static const FLinearColor Ivory(0.96f, 0.93f, 0.85f, 1.f);        // Damascene limestone
	static const FLinearColor Gold(0.98f, 0.76f, 0.30f, 1.f);         // mosaic gold
	static const FLinearColor Jade(0.20f, 0.72f, 0.58f, 1.f);         // Umayyad mosaic green
	static const FLinearColor Crimson(0.86f, 0.18f, 0.16f, 1.f);
	static const FLinearColor Panel(0.02f, 0.02f, 0.025f, 0.55f);
}

void AUSHUD::BeginPlay()
{
	Super::BeginPlay();
	if (GEngine && GEngine->GameViewport && GetOwningPlayerController() && GetOwningPlayerController()->IsLocalController())
	{
		Overlay = SNew(SUSOverlay).OwnerPC(Cast<AUSPlayerController>(GetOwningPlayerController()));
		OverlayHost = SNew(SWeakWidget).PossiblyNullContent(Overlay.ToSharedRef());
		GEngine->GameViewport->AddViewportWidgetContent(OverlayHost.ToSharedRef(), 10);
	}
}

void AUSHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (OverlayHost.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OverlayHost.ToSharedRef());
	}
	OverlayHost.Reset();
	Overlay.Reset();
	Super::EndPlay(Reason);
}

void AUSHUD::Box(float X, float Y, float W, float H, const FLinearColor& C)
{
	FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), C);
	T.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(T);
}

void AUSHUD::Label(const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter)
{
	FCanvasTextItem T(FVector2D(X, Y), FText::FromString(S), GEngine->GetLargeFont(), C);
	T.Scale = FVector2D(Scale);
	T.bCentreX = bCenter;
	T.EnableShadow(FLinearColor(0, 0, 0, 0.8f));
	Canvas->DrawItem(T);
}

void AUSHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	const AUSCharacter* C = Cast<AUSCharacter>(GetOwningPawn());
	const AUSPlayerController* PC = Cast<AUSPlayerController>(GetOwningPlayerController());

	DrawVignette();
	if (PC && PC->bScopeOverlay) DrawScope();
	else if (C && C->IsAlive())
	{
		const AUSWeapon* W = C->GetActiveWeapon();
		DrawCrosshair(W ? W->GetCurrentSpread() : 0.f);
	}
	DrawHitMarker();
	DrawTopScore();
	DrawKillFeed();
	DrawProgress();
	if (C && C->IsAlive()) DrawBottomBar();
}

void AUSHUD::DrawCrosshair(float Spread)
{
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const float Gap = 4.f + Spread * 6.f;
	const float Len = 8.f, Th = 2.f;
	const FLinearColor Col = USColors::Jade;
	Box(CX - Gap - Len, CY - Th * 0.5f, Len, Th, Col);
	Box(CX + Gap, CY - Th * 0.5f, Len, Th, Col);
	Box(CX - Th * 0.5f, CY - Gap - Len, Th, Len, Col);
	Box(CX - Th * 0.5f, CY + Gap, Th, Len, Col);
}

void AUSHUD::DrawHitMarker()
{
	const AUSPlayerController* PC = Cast<AUSPlayerController>(GetOwningPlayerController());
	if (!PC) return;
	const float Age = float(GetWorld()->GetTimeSeconds()) - PC->HitMarkerTime;
	if (Age > 0.25f) return;
	const float A = 1.f - Age / 0.25f;
	const FLinearColor Col = PC->bLastHitKill ? USColors::Crimson : (PC->bLastHitHead ? USColors::Gold : FLinearColor::White);
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const float In = 6.f, Out = PC->bLastHitKill ? 18.f : 13.f;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector2D D(i < 2 ? -1.f : 1.f, (i % 2) ? -1.f : 1.f);
		FCanvasLineItem L(FVector2D(CX, CY) + D * In, FVector2D(CX, CY) + D * Out);
		L.SetColor(Col.CopyWithNewOpacity(A));
		L.LineThickness = 2.f;
		Canvas->DrawItem(L);
	}
}

void AUSHUD::DrawVignette()
{
	const AUSPlayerController* PC = Cast<AUSPlayerController>(GetOwningPlayerController());
	if (!PC || PC->DamageFlash < 0.01f) return;
	const float A = PC->DamageFlash * 0.45f;
	const float W = Canvas->ClipX, H = Canvas->ClipY, E = 90.f;
	const FLinearColor C = USColors::Crimson.CopyWithNewOpacity(A);
	Box(0, 0, W, E, C); Box(0, H - E, W, E, C); Box(0, E, E, H - 2 * E, C); Box(W - E, E, E, H - 2 * E, C);
}

void AUSHUD::DrawScope()
{
	const float W = Canvas->ClipX, H = Canvas->ClipY, R = H * 0.46f;
	const FLinearColor Black(0, 0, 0, 1);
	Box(0, 0, W * 0.5f - R, H, Black);
	Box(W * 0.5f + R, 0, W * 0.5f - R, H, Black);
	Box(0, H * 0.5f - 0.5f, W, 1.f, Black);
	Box(W * 0.5f - 0.5f, 0, 1.f, H, Black);
	FCanvasNGonItem Ring(FVector2D(W * 0.5f, H * 0.5f), FVector2D(R, R), 64, FLinearColor(0, 0, 0, 0.35f));
	Canvas->DrawItem(Ring);
}

void AUSHUD::DrawTopScore()
{
	const AUSGameState* GS = GetWorld()->GetGameState<AUSGameState>();
	if (!GS) return;
	const float CX = Canvas->ClipX * 0.5f;
	const int32 T = FMath::CeilToInt(GS->GetPhaseTimeRemaining());

	Box(CX - 160.f, 12.f, 320.f, 54.f, USColors::Panel);
	Box(CX - 160.f, 64.f, 320.f, 2.f, USColors::Gold);
	Label(FString::Printf(TEXT("%d"), GS->AttackerScore), CX - 110.f, 20.f, USColors::Crimson, 1.4f, true);
	Label(FString::Printf(TEXT("%d"), GS->DefenderScore), CX + 110.f, 20.f, USColors::Jade, 1.4f, true);

	const bool bPlanted = GS->Phase == EUSRoundPhase::BombPlanted;
	Label(bPlanted ? FString::Printf(TEXT("C4 · %s"), *GS->PlantedSite.ToString()) : FString::Printf(TEXT("%d:%02d"), T / 60, T % 60),
		CX, 20.f, bPlanted ? USColors::Crimson : USColors::Ivory, 1.4f, true);
}

void AUSHUD::DrawKillFeed()
{
	const AUSGameState* GS = GetWorld()->GetGameState<AUSGameState>();
	if (!GS) return;
	float Y = 20.f;
	for (int32 i = GS->KillFeed.Num() - 1; i >= 0; --i)
	{
		const FUSKillFeedEntry& E = GS->KillFeed[i];
		if (GS->GetServerWorldTimeSeconds() - E.ServerTime > 8.f) continue;
		const FString Line = FString::Printf(TEXT("%s  [%s]  %s"), *E.Killer, *E.Weapon, *E.Victim);
		const float X = Canvas->ClipX - 20.f - Line.Len() * 11.f;
		Box(X - 8.f, Y - 2.f, Line.Len() * 11.f + 16.f, 30.f, USColors::Panel);
		Label(Line, X, Y, E.KillerTeam == EUSTeam::Attackers ? USColors::Crimson : USColors::Jade, 0.8f);
		Y += 36.f;
	}
}

void AUSHUD::DrawProgress()
{
	for (TActorIterator<AUSBomb> It(GetWorld()); It; ++It)
	{
		if (It->DefuseProgress > 0.f && !It->bDefused)
		{
			const float W = 360.f, X = Canvas->ClipX * 0.5f - W * 0.5f, Y = Canvas->ClipY * 0.62f;
			Box(X, Y, W, 10.f, USColors::Panel);
			Box(X, Y, W * FMath::Clamp(It->DefuseProgress, 0.f, 1.f), 10.f, USColors::Jade);
		}
	}
}

void AUSHUD::DrawBottomBar()
{
	const AUSCharacter* C = Cast<AUSCharacter>(GetOwningPawn());
	const AUSPlayerState* PS = C ? C->GetPlayerState<AUSPlayerState>() : nullptr;
	const float H = Canvas->ClipY, W = Canvas->ClipX;

	// Health / armor (left)
	Box(24.f, H - 84.f, 300.f, 60.f, USColors::Panel);
	Label(FString::Printf(TEXT("+ %d"), FMath::CeilToInt(C->Health)), 40.f, H - 76.f, C->Health > 25.f ? USColors::Ivory : USColors::Crimson, 1.4f);
	Label(FString::Printf(TEXT("ARMOR %d%s"), FMath::CeilToInt(C->Armor), C->bHasHelmet ? TEXT("H") : TEXT("")), 180.f, H - 76.f, USColors::Ivory, 1.4f);
	Box(24.f, H - 26.f, 300.f * C->Health / 100.f, 3.f, C->Health > 25.f ? USColors::Jade : USColors::Crimson);

	// Money (left, above)
	if (PS) Label(FString::Printf(TEXT("$%d"), PS->Money), 40.f, H - 130.f, USColors::Gold, 1.2f);
	if (C->bCarryingBomb) Label(TEXT("C4"), 260.f, H - 130.f, USColors::Crimson, 1.2f);
	if (C->bHasDefuseKit) Label(TEXT("KIT"), 260.f, H - 130.f, USColors::Jade, 1.2f);

	// Ammo (right)
	if (const AUSWeapon* Wp = C->GetActiveWeapon())
	{
		Box(W - 324.f, H - 84.f, 300.f, 60.f, USColors::Panel);
		Label(Wp->Stats.DisplayName.ToString(), W - 310.f, H - 76.f, USColors::Ivory, 0.8f);
		Label(FString::Printf(TEXT("%d / %d"), Wp->AmmoInMag, Wp->AmmoReserve), W - 150.f, H - 76.f,
			Wp->AmmoInMag <= Wp->Stats.MagazineSize / 4 ? USColors::Crimson : USColors::Ivory, 1.4f);
		if (Wp->IsReloading()) Label(TEXT("RELOADING"), W * 0.5f, H * 0.58f, USColors::Gold, 0.9f, true);
	}
}
