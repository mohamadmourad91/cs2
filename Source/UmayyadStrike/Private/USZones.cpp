#include "USZones.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"

AUSBombSite::AUSBombSite()
{
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(FVector(800.f, 800.f, 300.f));
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->ShapeColor = FColor(255, 80, 40);
	RootComponent = Volume;

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Volume);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(300.f);
	Label->SetTextRenderColor(FColor(255, 80, 40));
	Label->SetRelativeLocation(FVector(0, 0, 350.f));
	Label->bHiddenInGame = true;
}

void AUSBombSite::OnConstruction(const FTransform& T)
{
	Super::OnConstruction(T);
	Label->SetText(FText::FromName(SiteName));
}

bool AUSBombSite::Contains(const FVector& P) const
{
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(P);
	const FVector E = Volume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

AUSBuyZone::AUSBuyZone()
{
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(FVector(1200.f, 1200.f, 400.f));
	Volume->SetCollisionProfileName(TEXT("NoCollision"));
	Volume->ShapeColor = FColor(60, 200, 90);
	RootComponent = Volume;
}

bool AUSBuyZone::Contains(const FVector& P) const
{
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(P);
	const FVector E = Volume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}
