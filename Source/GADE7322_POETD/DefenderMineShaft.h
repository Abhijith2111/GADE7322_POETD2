#pragma once

#include "CoreMinimal.h"
#include "DefenderBase.h"
#include "DefenderMineShaft.generated.h"

UCLASS()
class GADE7322_POETD_API ADefenderMineShaft : public ADefenderBase
{
	GENERATED_BODY()

public:
	ADefenderMineShaft();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyDefenderMesh() override;
	virtual void ScanAndAttack() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Income", meta = (ClampMin = "1"))
	int32 CoinsPerPayout = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Income", meta = (ClampMin = "1.0"))
	float PayoutInterval = 12.f;

	UFUNCTION()
	void GenerateIncome();

	FTimerHandle IncomeTimerHandle;
};
