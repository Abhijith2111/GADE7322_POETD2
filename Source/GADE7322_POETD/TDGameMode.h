#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CentralTowerBase.h"
#include "TDGameMode.generated.h"

UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	WaitingToStart,
	InProgress,
	Victory,
	Defeat
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGamePhaseChanged, EGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVictory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoss);

UCLASS()
class GADE7322_POETD_API ATDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATDGameMode();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintReadOnly, Category = "GameState")
	EGamePhase CurrentPhase = EGamePhase::WaitingToStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameState", meta = (ClampMin = "1"))
	int32 WaveCountToWin = 5;

	UPROPERTY(BlueprintReadOnly, Category = "GameState")
	int32 WavesCompleted = 0;

	UPROPERTY(BlueprintAssignable, Category = "GameState")
	FOnGamePhaseChanged OnGamePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "GameState")
	FOnVictory OnVictory;

	UPROPERTY(BlueprintAssignable, Category = "GameState")
	FOnLoss OnLoss;

	UFUNCTION(BlueprintCallable, Category = "GameState")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "GameState")
	void NotifyWaveCompleted();

	UFUNCTION(BlueprintPure, Category = "GameState")
	bool IsGameActive() const;

private:
	UPROPERTY()
	ACentralTowerBase* CentralTowerRef;

	FTimerHandle TowerMonitorHandle;

	void SetGamePhase(EGamePhase NewPhase);
	void MonitorCentralTower();
	void BindToCentralTower();
	void HandleCentralTowerDestroyed();
};