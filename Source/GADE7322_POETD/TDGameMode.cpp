#include "TDGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "TDGameState.h"

ATDGameMode::ATDGameMode()
{
	GameStateClass = ATDGameState::StaticClass();
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(TowerMonitorHandle, this, &ATDGameMode::MonitorCentralTower, 0.5f, false);

	UE_LOG(LogTemp, Log, TEXT("TDGameMode: Initialised. Waiting to start."));
}

void ATDGameMode::StartGame()
{
	if (CurrentPhase != EGamePhase::WaitingToStart)
	{
		return;
	}

	SetGamePhase(EGamePhase::InProgress);
	UE_LOG(LogTemp, Log, TEXT("TDGameMode: Game started. Survive %d waves to win."), WaveCountToWin);
}

void ATDGameMode::NotifyWaveCompleted()
{
	if (CurrentPhase != EGamePhase::InProgress)
	{
		return;
	}

	++WavesCompleted;
	UE_LOG(LogTemp, Log, TEXT("TDGameMode: Wave %d/%d completed."), WavesCompleted, WaveCountToWin);

	if (WavesCompleted >= WaveCountToWin)
	{
		SetGamePhase(EGamePhase::GameOverVictory);
		OnVictory.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("TDGameMode: ===== VICTORY - All waves survived! ====="));
	}
}

bool ATDGameMode::IsGameActive() const
{
	return CurrentPhase == EGamePhase::InProgress;
}

void ATDGameMode::SetGamePhase(EGamePhase NewPhase)
{
	CurrentPhase = NewPhase;
	OnGamePhaseChanged.Broadcast(NewPhase);
}

void ATDGameMode::MonitorCentralTower()
{
	CentralTowerRef = Cast<ACentralTowerBase>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ACentralTowerBase::StaticClass()));

	if (CentralTowerRef)
	{
		BindToCentralTower();
		UE_LOG(LogTemp, Log, TEXT("TDGameMode: Central tower found and bound."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: No central tower found - retrying in 1 second."));
		GetWorldTimerManager().SetTimer(TowerMonitorHandle, this, &ATDGameMode::MonitorCentralTower, 1.f, false);
	}
}

void ATDGameMode::BindToCentralTower()
{
	if (!CentralTowerRef)
	{
		return;
	}

	CentralTowerRef->OnTowerDestroyed.AddDynamic(this, &ATDGameMode::HandleCentralTowerDestroyed);
	StartGame();
}

void ATDGameMode::HandleCentralTowerDestroyed()
{
	if (CurrentPhase == EGamePhase::InProgress)
	{
		SetGamePhase(EGamePhase::GameOverLoss);
		OnLoss.Broadcast();
		UE_LOG(LogTemp, Warning, TEXT("TDGameMode: ===== GAME OVER - Central tower destroyed! ====="));
	}
}