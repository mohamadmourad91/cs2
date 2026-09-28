#include "USOverlay.h"
#include "USPlayerController.h"
#include "USGameState.h"
#include "USPlayerState.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "USOverlay"

void SUSOverlay::Construct(const FArguments& InArgs)
{
	PC = InArgs._OwnerPC;

	const FSlateFontInfo BannerFont = FCoreStyle::GetDefaultFontStyle("Bold", 44);
	const FSlateFontInfo MsgFont = FCoreStyle::GetDefaultFontStyle("Regular", 24);
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 26);

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(6.f));
	struct FItem { FName Id; FText Label; int32 Price; };
	const TArray<FItem> Items = {
		{ TEXT("Glock"),        LOCTEXT("Glock", "Glock-18"),               200 },
		{ TEXT("USP"),          LOCTEXT("USP", "USP-S"),                    200 },
		{ TEXT("Deagle"),       LOCTEXT("Deagle", "Desert Eagle"),          700 },
		{ TEXT("AK"),           LOCTEXT("AK", "AK-47"),                     2700 },
		{ TEXT("M4"),           LOCTEXT("M4", "M4A4"),                      3100 },
		{ TEXT("AWP"),          LOCTEXT("AWP", "AWP"),                      4750 },
		{ TEXT("Kevlar"),       LOCTEXT("Kevlar", "درع"),                   650 },
		{ TEXT("KevlarHelmet"), LOCTEXT("KevlarHelmet", "درع + خوذة"),      1000 },
		{ TEXT("DefuseKit"),    LOCTEXT("DefuseKit", "عدة التفكيك"),        400 },
	};
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		Grid->AddSlot(i % 3, i / 3)[ MakeBuyButton(Items[i].Id, Items[i].Label, Items[i].Price) ];
	}

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		// Big centred banner ("تم زرع العبوة", "فوز الجولة" ...)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 180, 0, 0)
		[
			SNew(STextBlock)
			.Visibility(this, &SUSOverlay::GetBannerVisibility)
			.Text(this, &SUSOverlay::GetBannerText)
			.Font(BannerFont)
			.ColorAndOpacity(FLinearColor(0.98f, 0.76f, 0.30f))
			.ShadowOffset(FVector2D(2, 2))
		]

		// Round result reason
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, 90, 0, 0)
		[
			SNew(STextBlock)
			.Visibility(this, &SUSOverlay::GetRoundMessageVisibility)
			.Text(this, &SUSOverlay::GetRoundMessage)
			.Font(MsgFont)
			.ColorAndOpacity(FLinearColor(0.96f, 0.93f, 0.85f))
			.ShadowOffset(FVector2D(1, 1))
		]

		// Buy menu
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.Visibility(this, &SUSOverlay::GetBuyMenuVisibility)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f))
			.Padding(24.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 16)
				[
					SNew(STextBlock).Text(LOCTEXT("BuyTitle", "قائمة الشراء  —  B للإغلاق")).Font(TitleFont)
					.ColorAndOpacity(FLinearColor(0.98f, 0.76f, 0.30f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(720.f)[ Grid ]
				]
			]
		]
	];
}

TSharedRef<SWidget> SUSOverlay::MakeBuyButton(FName Id, const FText& Label, int32 Price)
{
	return SNew(SButton)
		.ButtonColorAndOpacity(FLinearColor(0.08f, 0.22f, 0.18f))
		.ContentPadding(FMargin(12.f, 16.f))
		.OnClicked_Lambda([this, Id]()
		{
			if (PC.IsValid()) PC->ServerBuy(Id);
			return FReply::Handled();
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[ SNew(STextBlock).Text(Label).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(FLinearColor(0.96f, 0.93f, 0.85f)) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[ SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("$%d"), Price))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).ColorAndOpacity(FLinearColor(0.98f, 0.76f, 0.30f)) ]
		];
}

FText SUSOverlay::GetBannerText() const
{
	return PC.IsValid() ? FText::FromString(PC->BannerText) : FText::GetEmpty();
}

EVisibility SUSOverlay::GetBannerVisibility() const
{
	if (!PC.IsValid() || !PC->GetWorld()) return EVisibility::Collapsed;
	return PC->GetWorld()->GetTimeSeconds() - PC->BannerTime < 3.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SUSOverlay::GetRoundMessage() const
{
	const AUSGameState* GS = PC.IsValid() && PC->GetWorld() ? PC->GetWorld()->GetGameState<AUSGameState>() : nullptr;
	if (!GS) return FText::GetEmpty();
	if (GS->Phase == EUSRoundPhase::Warmup) return LOCTEXT("Warmup", "إحماء — ساحة الأمويين، دمشق");
	if (GS->Phase == EUSRoundPhase::MatchEnd)
	{
		return FText::FromString(GS->AttackerScore > GS->DefenderScore ? TEXT("انتهت المباراة — فاز المهاجمون") : TEXT("انتهت المباراة — فاز المدافعون"));
	}
	return FText::FromString(GS->LastRoundMessage);
}

EVisibility SUSOverlay::GetRoundMessageVisibility() const
{
	const AUSGameState* GS = PC.IsValid() && PC->GetWorld() ? PC->GetWorld()->GetGameState<AUSGameState>() : nullptr;
	const bool bShow = GS && (GS->Phase == EUSRoundPhase::RoundEnd || GS->Phase == EUSRoundPhase::Warmup || GS->Phase == EUSRoundPhase::MatchEnd);
	return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SUSOverlay::GetBuyMenuVisibility() const
{
	return PC.IsValid() && PC->IsBuyMenuOpen() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
