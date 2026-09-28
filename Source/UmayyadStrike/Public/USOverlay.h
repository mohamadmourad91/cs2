#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AUSPlayerController;

/** Slate layer: Arabic-shaped banners, round result text and the buy menu. */
class UMAYYADSTRIKE_API SUSOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUSOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AUSPlayerController>, OwnerPC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeBuyButton(FName Id, const FText& Label, int32 Price);
	FText GetBannerText() const;
	EVisibility GetBannerVisibility() const;
	FText GetRoundMessage() const;
	EVisibility GetRoundMessageVisibility() const;
	EVisibility GetBuyMenuVisibility() const;

	TWeakObjectPtr<AUSPlayerController> PC;
};
