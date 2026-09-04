#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TDGameState.generated.h"

class ACentralTowerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoneyChanged, int32, NewAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoneyTransaction, int32, Amount, int32, NewTotal);

UCLASS()
class GADE7322_POETD_API ATDGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATDGameState();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy", meta = (ClampMin = "0"))
	int32 StartingMoney = 500;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 PlayerMoney;

	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnMoneyChanged OnMoneyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnMoneyTransaction OnMoneyEarned;

	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnMoneyTransaction OnMoneySpent;

	UFUNCTION(BlueprintCallable, Category = "Economy")
	void AddMoney(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Economy")
	bool SpendMoney(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Economy")
	bool CanAfford(int32 Amount) const;

	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetCurrentMoney() const;

	// --- Phase 5: UI ---
	UFUNCTION(BlueprintCallable, Category = "UI")
	void RegisterCentralTower(ACentralTowerBase* InTower);

	UFUNCTION(BlueprintPure, Category = "UI")
	ACentralTowerBase* GetCentralTower() const;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<ACentralTowerBase> CentralTowerRef;
};