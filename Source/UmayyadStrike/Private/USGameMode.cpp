#include "USGameMode.h"
#include "USGameState.h"
#include "USPlayerState.h"
#include "USPlayerController.h"
#include "USCharacter.h"
#include "USWeapon.h"
#include "USBomb.h"
#include "USZones.h"
#include "USHUD.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AUSGameMode::AUSGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AUSCharacter::StaticClass();
	PlayerControllerClass = AUSPlayerController::StaticClass();
	PlayerStateClass = AUSPlayerState::StaticClass();
	GameStateClass = AUSGameState::StaticClass();
	HUDClass = AUSHUD::StaticClass();
	BombClass = AUSBomb::StaticClass();
}

AUSGameState* AUSGameMode::GS() const { return GetGameState<AUSGameState>(); }

void AUSGameMode::StartPlay()
{
	Super::StartPlay();
	SetPhase(EUSRoundPhase::Warmup, WarmupTime);
}

void AUSGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Auto-balance: join the smaller team
	int32 A = 0, D = 0;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const AUSPlayerState* U = Cast<AUSPlayerState>(PS))
		{
			A += U->Team == EUSTeam::Attackers; D += U->Team == EUSTeam::Defenders;
		}
	}
	if (AUSPlayerState* PS = NewPlayer->GetPlayerState<AUSPlayerState>())
	{
		PS->Team = A <= D ? EUSTeam::Attackers : EUSTeam::Defenders;
		PS->Money = StartMoney;
	}
	Super::PostLogin(NewPlayer);
}

AActor* AUSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AUSPlayerState* PS = Player ? Player->GetPlayerState<AUSPlayerState>() : nullptr;
	const FName Tag = PS && PS->Team == EUSTeam::Defenders ? FName(TEXT("Defenders")) : FName(TEXT("Attackers"));

	TArray<APlayerStart*> Free, All;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag != Tag) continue;
		All.Add(*It);
		bool bBlocked = false;
		for (TActorIterator<AUSCharacter> C(GetWorld()); C; ++C)
		{
			if (C->IsAlive() && FVector::DistSquared(C->GetActorLocation(), It->GetActorLocation()) < FMath::Square(120.f)) { bBlocked = true; break; }
		}
		if (!bBlocked) Free.Add(*It);
	}
	if (Free.Num()) return Free[FMath::RandRange(0, Free.Num() - 1)];
	if (All.Num()) return All[FMath::RandRange(0, All.Num() - 1)];
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AUSGameMode::SetPhase(EUSRoundPhase NewPhase, float Duration)
{
	if (AUSGameState* S = GS())
	{
		S->Phase = NewPhase;
		S->PhaseEndServerTime = float(GetWorld()->GetTimeSeconds()) + Duration;
	}
}

void AUSGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AUSGameState* S = GS();
	if (!S) return;

	const bool bExpired = GetWorld()->GetTimeSeconds() >= S->PhaseEndServerTime;
	switch (S->Phase)
	{
	case EUSRoundPhase::Warmup:
		if (bExpired) { S->AttackerScore = S->DefenderScore = 0; S->RoundNumber = 0; StartRound(); }
		break;
	case EUSRoundPhase::BuyTime:
		if (bExpired)
		{
			SetPhase(EUSRoundPhase::Live, RoundTime);
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (AUSPlayerController* PC = Cast<AUSPlayerController>(It->Get())) PC->ClientAnnounce(EUSAnnouncement::RoundStart);
			}
		}
		break;
	case EUSRoundPhase::Live:
		TickPlantAndDefuse(DeltaSeconds);
		if (bExpired) EndRound(EUSTeam::Defenders, TEXT("انتهى الوقت - فاز المدافعون"));
		break;
	case EUSRoundPhase::BombPlanted:
		TickPlantAndDefuse(DeltaSeconds);
		break;
	case EUSRoundPhase::RoundEnd:
		if (bExpired)
		{
			const int32 Played = S->AttackerScore + S->DefenderScore;
			if (S->AttackerScore >= RoundsToWin || S->DefenderScore >= RoundsToWin)
			{
				SetPhase(EUSRoundPhase::MatchEnd, 15.f);
			}
			else
			{
				if (Played == RoundsPerHalf) SwapSides();
				StartRound();
			}
		}
		break;
	case EUSRoundPhase::MatchEnd:
		if (bExpired) SetPhase(EUSRoundPhase::Warmup, WarmupTime);
		break;
	}
}

void AUSGameMode::StartRound()
{
	AUSGameState* S = GS();
	++S->RoundNumber;
	S->PlantedSite = NAME_None;
	S->KillFeed.Reset();
	if (PlantedBomb) { PlantedBomb->Destroy(); PlantedBomb = nullptr; }
	PlantProgress.Reset();
	RespawnAll();
	AssignBomb();
	SetPhase(EUSRoundPhase::BuyTime, BuyTime);
}

void AUSGameMode::RespawnAll()
{
	const bool bPistolRound = GS()->RoundNumber == 1 || GS()->RoundNumber == RoundsPerHalf + 1;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		AUSPlayerState* PS = PC->GetPlayerState<AUSPlayerState>();
		if (bPistolRound && PS) PS->Money = StartMoney;

		AUSCharacter* C = Cast<AUSCharacter>(PC->GetPawn());
		if (C && C->IsAlive() && !bPistolRound)
		{
			// Survivors keep their gear, but go back to spawn
			AActor* Start = ChoosePlayerStart(PC);
			C->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
			PC->SetControlRotation(Start->GetActorRotation());
			C->ResetForRound();
			continue;
		}
		if (C) { PC->UnPossess(); C->Destroy(); }
		RestartPlayer(PC);
	}
}

void AUSGameMode::AssignBomb()
{
	TArray<AUSCharacter*> Attackers;
	for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It)
	{
		It->bCarryingBomb = false;
		if (It->IsAlive() && It->GetTeam() == EUSTeam::Attackers) Attackers.Add(*It);
	}
	if (Attackers.Num()) Attackers[FMath::RandRange(0, Attackers.Num() - 1)]->bCarryingBomb = true;
}

void AUSGameMode::TickPlantAndDefuse(float DeltaSeconds)
{
	AUSGameState* S = GS();
	for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It)
	{
		AUSCharacter* C = *It;
		AController* Ctrl = C->GetController();
		if (!C->IsAlive() || !Ctrl) continue;

		// ---- Planting ----
		if (S->Phase == EUSRoundPhase::Live && C->bCarryingBomb)
		{
			AUSBombSite* Site = nullptr;
			for (TActorIterator<AUSBombSite> SiteIt(GetWorld()); SiteIt; ++SiteIt)
			{
				if (SiteIt->Contains(C->GetActorLocation())) { Site = *SiteIt; break; }
			}
			const bool bStill = C->GetVelocity().Size2D() < 10.f && !C->GetMovementComponent()->IsFalling();
			if (Site && C->IsUsing() && bStill)
			{
				float& P = PlantProgress.FindOrAdd(Ctrl);
				P += DeltaSeconds;
				if (P >= PlantTime)
				{
					FActorSpawnParameters SP; SP.Owner = C;
					FHitResult Floor;
					const FVector L = C->GetActorLocation();
					GetWorld()->LineTraceSingleByChannel(Floor, L, L - FVector(0, 0, 300.f), ECC_Visibility);
					PlantedBomb = GetWorld()->SpawnActor<AUSBomb>(BombClass, Floor.bBlockingHit ? Floor.ImpactPoint : L, FRotator(0, C->GetActorRotation().Yaw, 0), SP);
					C->bCarryingBomb = false;
					S->PlantedSite = Site->SiteName;
					if (AUSPlayerState* PS = C->GetPlayerState<AUSPlayerState>()) PS->AddMoney(PlantReward);
					SetPhase(EUSRoundPhase::BombPlanted, PlantedBomb->FuseTime);
					PlantProgress.Reset();
					for (FConstPlayerControllerIterator PCI = GetWorld()->GetPlayerControllerIterator(); PCI; ++PCI)
					{
						if (AUSPlayerController* UPC = Cast<AUSPlayerController>(PCI->Get())) UPC->ClientAnnounce(EUSAnnouncement::BombPlanted);
					}
					return;
				}
			}
			else
			{
				PlantProgress.Remove(Ctrl);
			}
		}

		// ---- Defusing ----
		if (S->Phase == EUSRoundPhase::BombPlanted && PlantedBomb && C->GetTeam() == EUSTeam::Defenders)
		{
			const bool bInRange = FVector::Dist(C->GetActorLocation(), PlantedBomb->GetActorLocation()) < 150.f;
			if (bInRange && C->IsUsing())
			{
				if (PlantedBomb->TickDefuse(DeltaSeconds, C->bHasDefuseKit))
				{
					if (AUSPlayerState* PS = C->GetPlayerState<AUSPlayerState>()) PS->AddMoney(DefuseReward);
					EndRound(EUSTeam::Defenders, TEXT("تم تفكيك العبوة - فاز المدافعون"));
					return;
				}
			}
		}
	}

	// Nobody is defusing this frame -> reset progress (CS rule: releasing E resets)
	if (PlantedBomb)
	{
		bool bAnyDefusing = false;
		for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsAlive() && It->GetTeam() == EUSTeam::Defenders && It->IsUsing()
				&& FVector::Dist(It->GetActorLocation(), PlantedBomb->GetActorLocation()) < 150.f) { bAnyDefusing = true; break; }
		}
		if (!bAnyDefusing) PlantedBomb->ResetDefuse();
	}
}

int32 AUSGameMode::CountAlive(EUSTeam Team) const
{
	int32 N = 0;
	for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && It->GetTeam() == Team) ++N;
	}
	return N;
}

void AUSGameMode::OnPlayerKilled(AUSCharacter* Victim, AController* Killer)
{
	AUSGameState* S = GS();
	AUSPlayerState* VPS = Victim->GetPlayerState<AUSPlayerState>();
	AUSPlayerState* KPS = Killer ? Killer->GetPlayerState<AUSPlayerState>() : nullptr;
	if (VPS) ++VPS->Deaths;
	if (KPS && KPS != VPS) { ++KPS->Kills; KPS->AddMoney(KillReward); }

	// Drop the bomb to a random living teammate (simplified; a physical drop is a TODO)
	if (Victim->bCarryingBomb)
	{
		Victim->bCarryingBomb = false;
		for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Victim && It->IsAlive() && It->GetTeam() == EUSTeam::Attackers) { It->bCarryingBomb = true; break; }
		}
	}

	FUSKillFeedEntry E;
	E.Killer = KPS ? KPS->GetPlayerName() : TEXT("World");
	E.Victim = VPS ? VPS->GetPlayerName() : TEXT("?");
	const AUSCharacter* KC = Killer ? Cast<AUSCharacter>(Killer->GetPawn()) : nullptr;
	E.Weapon = KC && KC->GetActiveWeapon() ? KC->GetActiveWeapon()->Stats.DisplayName.ToString() : TEXT("");
	E.KillerTeam = KPS ? KPS->Team : EUSTeam::None;
	E.ServerTime = float(GetWorld()->GetTimeSeconds());
	S->KillFeed.Add(E);
	if (S->KillFeed.Num() > 6) S->KillFeed.RemoveAt(0);

	CheckElimination();
}

void AUSGameMode::CheckElimination()
{
	AUSGameState* S = GS();
	if (S->Phase != EUSRoundPhase::Live && S->Phase != EUSRoundPhase::BombPlanted) return;

	if (CountAlive(EUSTeam::Defenders) == 0 && CountAlive(EUSTeam::Attackers) > 0)
	{
		EndRound(EUSTeam::Attackers, TEXT("تم القضاء على المدافعين"));
	}
	else if (CountAlive(EUSTeam::Attackers) == 0 && S->Phase == EUSRoundPhase::Live)
	{
		EndRound(EUSTeam::Defenders, TEXT("تم القضاء على المهاجمين"));
	}
}

void AUSGameMode::OnBombExploded()
{
	EndRound(EUSTeam::Attackers, TEXT("انفجرت العبوة - فاز المهاجمون"));
}

void AUSGameMode::EndRound(EUSTeam Winner, const FString& Reason)
{
	AUSGameState* S = GS();
	if (S->Phase == EUSRoundPhase::RoundEnd || S->Phase == EUSRoundPhase::MatchEnd) return;

	(Winner == EUSTeam::Attackers ? S->AttackerScore : S->DefenderScore)++;
	S->LastRoundMessage = Reason;

	int32& LoserStreak = Winner == EUSTeam::Attackers ? DefenderLossStreak : AttackerLossStreak;
	int32& WinnerStreak = Winner == EUSTeam::Attackers ? AttackerLossStreak : DefenderLossStreak;
	const int32 LossBonus = LossBonusLadder[FMath::Clamp(LoserStreak, 0, LossBonusLadder.Num() - 1)];
	LoserStreak = FMath::Min(LoserStreak + 1, LossBonusLadder.Num() - 1);
	WinnerStreak = FMath::Max(WinnerStreak - 1, 0);

	for (APlayerState* P : S->PlayerArray)
	{
		if (AUSPlayerState* PS = Cast<AUSPlayerState>(P))
		{
			PS->AddMoney(PS->Team == Winner ? WinReward : LossBonus);
		}
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AUSPlayerController* PC = Cast<AUSPlayerController>(It->Get()))
		{
			const AUSPlayerState* PS = PC->GetPlayerState<AUSPlayerState>();
			PC->ClientAnnounce(PS && PS->Team == Winner ? EUSAnnouncement::RoundWon : EUSAnnouncement::RoundLost);
		}
	}
	SetPhase(EUSRoundPhase::RoundEnd, RoundEndTime);
}

void AUSGameMode::SwapSides()
{
	AUSGameState* S = GS();
	Swap(S->AttackerScore, S->DefenderScore);
	Swap(AttackerLossStreak, DefenderLossStreak);
	for (APlayerState* P : S->PlayerArray)
	{
		if (AUSPlayerState* PS = Cast<AUSPlayerState>(P))
		{
			PS->Team = PS->Team == EUSTeam::Attackers ? EUSTeam::Defenders : EUSTeam::Attackers;
			PS->Money = StartMoney;
		}
	}
	// Force full respawn with default pistols
	for (TActorIterator<AUSCharacter> It(GetWorld()); It; ++It) It->Destroy();
}

bool AUSGameMode::TryBuy(APlayerController* PC, FName ItemId)
{
	AUSGameState* S = GS();
	AUSCharacter* C = PC ? Cast<AUSCharacter>(PC->GetPawn()) : nullptr;
	AUSPlayerState* PS = PC ? PC->GetPlayerState<AUSPlayerState>() : nullptr;
	if (!C || !PS || !C->IsAlive()) return false;

	const bool bBuyWindow = S->Phase == EUSRoundPhase::BuyTime || S->Phase == EUSRoundPhase::Warmup
		|| (S->Phase == EUSRoundPhase::Live && S->GetPhaseTimeRemaining() > RoundTime - 20.f);
	if (!bBuyWindow) return false;

	bool bInZone = false;
	for (TActorIterator<AUSBuyZone> It(GetWorld()); It; ++It)
	{
		if (It->Team == PS->Team && It->Contains(C->GetActorLocation())) { bInZone = true; break; }
	}
	if (!bInZone && S->Phase != EUSRoundPhase::Warmup) return false;

	int32 Price = 0;
	if (ItemId == TEXT("Kevlar")) Price = 650;
	else if (ItemId == TEXT("KevlarHelmet")) Price = 1000;
	else if (ItemId == TEXT("DefuseKit")) Price = PS->Team == EUSTeam::Defenders ? 400 : -1;
	else Price = AUSWeapon::MakePreset(ItemId).Price;

	if (Price < 0 || PS->Money < Price) return false;
	PS->AddMoney(-Price);

	if (ItemId == TEXT("Kevlar")) { C->Armor = 100.f; }
	else if (ItemId == TEXT("KevlarHelmet")) { C->Armor = 100.f; C->bHasHelmet = true; }
	else if (ItemId == TEXT("DefuseKit")) { C->bHasDefuseKit = true; }
	else
	{
		const bool bPistol = ItemId == TEXT("Glock") || ItemId == TEXT("USP") || ItemId == TEXT("Deagle");
		C->GiveWeapon(ItemId, bPistol ? EUSWeaponSlot::Secondary : EUSWeaponSlot::Primary);
	}
	return true;
}
