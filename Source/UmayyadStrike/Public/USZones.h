#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "USTypes.h"
#include "USZones.generated.h"

class UBoxComponent;
class UTextRenderComponent;

/** Bomb site volume. Site A = Opera House forecourt, Site B = Radio & TV building plaza. Mid = Damascene Sword monument. */
UCLASS()
class UMAYYADSTRIKE_API AUSBombSite : public AActor
{
	GENERATED_BODY()
public:
	AUSBombSite();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Volume;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SiteName = TEXT("A");
	bool Contains(const FVector& P) const;
	virtual void OnConstruction(const FTransform& T) override;
};

UCLASS()
class UMAYYADSTRIKE_API AUSBuyZone : public AActor
{
	GENERATED_BODY()
public:
	AUSBuyZone();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Volume;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EUSTeam Team = EUSTeam::Attackers;
	bool Contains(const FVector& P) const;
};
