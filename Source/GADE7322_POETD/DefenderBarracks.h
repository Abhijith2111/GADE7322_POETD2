#pragma once

#include "CoreMinimal.h"
#include "DefenderBase.h"
#include "WeatherTypes.h"
#include "DefenderBarracks.generated.h"

class ADefenderKnight;

UCLASS()
class GADE7322_POETD_API ADefenderBarracks : public ADefenderBase
{
	GENERATED_BODY()

public:
	ADefenderBarracks();
	void ApplyTrainUpgrade();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyDefenderMesh() override;
	virtual void ScanAndAttack() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knights", meta = (ClampMin = "1"))
	int32 KnightCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knights", meta = (ClampMin = "1.0"))
	float KnightTrainInterval = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knights")
	TSubclassOf<ADefenderKnight> NightKnightClass;

	void SpawnKnights();
	void RefreshTrainingForWeather(bool bAllowInitialSpawn);
	float GetEffectiveTrainInterval() const;

	UFUNCTION()
	void TrainKnight();

	UFUNCTION()
	void SpawnInitialKnights();

	UFUNCTION()
	void HandleWeatherChanged(EWeatherType NewWeather);

	void SpawnOneKnight();

	FTimerHandle TrainTimerHandle;
	FTimerHandle InitialSpawnTimerHandle;
	int32 TrainedKnightCount = 0;
	bool bInitialKnightsSpawned = false;
};
