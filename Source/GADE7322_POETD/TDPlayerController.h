#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DefenderBase.h"
#include "ProceduralTerrain.h"
#include "TDGameState.h"
#include "Blueprint/UserWidget.h"
#include "TDPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDefenderPlacementSucceeded, ADefenderBase*, PlacedDefender);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDefenderPlacementFailed, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenderUpgraded, ADefenderBase*, UpgradedDefender, int32, NewLevel);

UCLASS()
class GADE7322_POETD_API ATDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATDPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSubclassOf<ADefenderBase> DefenderClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0"))
	int32 DefenderCost = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	int32 LocalTestingGold = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	float PathExclusionDistance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 UpgradeCost = 75;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade", meta = (ClampMin = "1"))
	int32 MaxUpgradeLevel = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	float UpgradeHealthBonus = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade")
	float UpgradeDamageBonus = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> PauseMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> HUDClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> GameOverClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> MainMenuClass;

	UPROPERTY(BlueprintAssignable, Category = "Placement")
	FOnDefenderPlacementSucceeded OnDefenderPlacementSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Placement")
	FOnDefenderPlacementFailed OnDefenderPlacementFailed;

	UPROPERTY(BlueprintAssignable, Category = "Upgrade")
	FOnDefenderUpgraded OnDefenderUpgraded;

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void TryPlaceDefender();

	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void TryUpgradeDefender();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePause();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ResumeFromPause();

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool CanAffordCost(int32 Cost) const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetPendingDefender(TSubclassOf<ADefenderBase> InDefenderClass, int32 InCost);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RestartMatch();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void StartMatchFromMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ReturnToMainMenu();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "400.0"))
	float OverviewCameraHeight = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "-89.0", ClampMax = "-20.0"))
	float OverviewCameraPitch = -55.f;

private:
	UPROPERTY()
	AProceduralTerrain* TerrainRef;

	UPROPERTY()
	UUserWidget* PauseMenuInstance;

	UPROPERTY()
	UUserWidget* HUDInstance;

	UPROPERTY()
	UUserWidget* GameOverInstance;

	UPROPERTY()
	UUserWidget* MainMenuInstance;

	bool bIsPaused = false;
	bool bInMainMenu = false;

	void SetPausedState(bool bPause);
	void EnsureWidgetClasses();

	UFUNCTION()
	void HandleGameLoss();

	UFUNCTION()
	void HandleGameVictory();

	void ShowGameOver(bool bVictory);
	void ShowMainMenu();
	bool ShouldShowMainMenu() const;

	TSet<int32> OccupiedGridIndices;
	TMap<ADefenderBase*, int32> DefenderGridIndices;
	TMap<ADefenderBase*, int32> DefenderUpgradeLevels;

	UFUNCTION()
	void HandleDefenderDestroyed(ADefenderBase* DestroyedDefender);

	ATDGameState* GetGameState() const;
	bool HasEnoughMoney(int32 Cost) const;
	bool SpendMoney(int32 Cost);

	bool FindNearestBuildLocation(const FVector& ClickLocation, FVector& OutLocation, int32& OutIndex) const;
	bool IsFarEnoughFromPathways(const FVector& Location) const;

	FTimerHandle OverviewCameraHandle;
	int32 OverviewCameraAttempts = 0;

	UFUNCTION()
	void PlaceOverviewCamera();

	void ClampOverviewCamera();
};