#pragma once

#include "CoreMinimal.h"
#include "USTypes.generated.h"

UENUM(BlueprintType)
enum class EUSTeam : uint8
{
	None,
	Attackers,   // المهاجمون - plant the charge
	Defenders    // المدافعون - defend the sites
};

UENUM(BlueprintType)
enum class EUSRoundPhase : uint8
{
	Warmup,
	BuyTime,
	Live,
	BombPlanted,
	RoundEnd,
	MatchEnd
};

UENUM(BlueprintType)
enum class EUSWeaponSlot : uint8
{
	Primary,
	Secondary,
	Melee,
	Bomb
};

USTRUCT(BlueprintType)
struct FUSWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id = TEXT("AK");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Price = 2700;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 36.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeadshotMultiplier = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ArmorPenetration = 0.775f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RoundsPerMinute = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MagazineSize = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ReserveAmmo = 90;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReloadTime = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 8192.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RangeFalloffPer500 = 0.98f;
	/** Degrees of random spread when standing still / moving / airborne */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpreadStanding = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpreadMoving = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpreadAir = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutomatic = true;
	/** Learnable spray pattern: pitch/yaw kick per shot index (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector2D> RecoilPattern;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecoilRecoverySpeed = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MoveSpeedMultiplier = 0.86f;
};

UENUM(BlueprintType)
enum class EUSAnnouncement : uint8
{
	RoundStart,
	BombPlanted,
	RoundWon,
	RoundLost
};
