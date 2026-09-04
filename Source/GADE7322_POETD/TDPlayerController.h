#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DefenderBase.h"
#include "ProceduralTerrain.h"
#include "TDGameState.h"
#include "TDPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDefenderPlacementSucceeded, ADefenderBase*, PlacedDefender);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDefenderPlacementFailed, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenderUpgraded, ADefenderBase*, UpgradedDefender, int32, NewLevel);

UCLASS()
class GADE7322_POETD_API ATDPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

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

private:
	UPROPERTY()
	AProceduralTerrain* TerrainRef;

	TSet<int32> OccupiedGridIndices;
	TMap<ADefenderBase*, int32> DefenderUpgradeLevels;

	ATDGameState* GetGameState() const;
	bool HasEnoughMoney(int32 Cost) const;
	bool SpendMoney(int32 Cost);

	bool FindNearestBuildLocation(const FVector& ClickLocation, FVector& OutLocation, int32& OutIndex) const;
	bool IsFarEnoughFromPathways(const FVector& Location) const;
};