#pragma once

#include "CoreMinimal.h"
#include "DefenderBase.h"
#include "DefenderBarracks.generated.h"

UCLASS()
class GADE7322_POETD_API ADefenderBarracks : public ADefenderBase
{
	GENERATED_BODY()

public:
	ADefenderBarracks();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyDefenderMesh() override;
	virtual void ScanAndAttack() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knights", meta = (ClampMin = "1"))
	int32 KnightCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knights", meta = (ClampMin = "1.0"))
	float KnightTrainInterval = 10.f;

	void SpawnKnights();

	UFUNCTION()
	void TrainKnight();

	void SpawnOneKnight();

	FTimerHandle TrainTimerHandle;
	int32 TrainedKnightCount = 0;
};
