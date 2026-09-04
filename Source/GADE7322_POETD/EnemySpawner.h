#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBase.h"
#include "ProceduralTerrain.h"
#include "EnemySpawner.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemySpawned, AEnemyBase*, SpawnedEnemy);

UCLASS()
class GADE7322_POETD_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (ClampMin = "0.1"))
	float SpawnInterval = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (ClampMin = "1"))
	int32 MaxEnemiesPerWave = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	bool bLoopWaves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (ClampMin = "0.0"))
	float SpawnHeightOffset = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|ManualFallback")
	TArray<FVector> ManualWaypoints;

	UPROPERTY(BlueprintAssignable, Category = "Spawner")
	FOnEnemySpawned OnEnemySpawned;

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void StopSpawning();

private:
	UPROPERTY()
	AProceduralTerrain* TerrainRef;

	FTimerHandle SpawnTimerHandle;
	int32 SpawnedThisWave = 0;
	int32 CurrentPathIndex = 0;

	void SpawnNextEnemy();
	TArray<FVector> GetWaypointsForPathIndex(int32 PathIndex) const;
	bool HasValidWaypoints() const;
};