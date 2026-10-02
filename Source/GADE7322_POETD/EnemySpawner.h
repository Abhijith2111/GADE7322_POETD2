#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBase.h"
#include "ProceduralTerrain.h"
#include "TDGameInstance.h"
#include "EnemySpawner.generated.h"

class AEnemyBrute;
class AEnemyTrojanHorse;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemySpawned, AEnemyBase*, SpawnedEnemy);

UCLASS()
class GADE7322_POETD_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyBase> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (DisplayName = "Ogre Class"))
	TSubclassOf<AEnemyBase> BruteClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyBase> TrojanClass;

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
	TArray<FTimerHandle> HardLaneTimers;

	float MatchStartTime = 0.f;
	bool bSpawning = false;
	int32 NextLane = 0;
	int32 OpeningGruntsRemaining = 0;
	int32 OgresSpawned = 0;
	int32 MediumStep = 0;
	bool bTrojanUnlocked = false;
	bool bSpawnedUnlockTrojan = false;

	UFUNCTION()
	void SpawnNextEnemy();

	UFUNCTION()
	void SpawnOnLane(int32 LaneIndex);

	void ClearSpawnTimers();
	void PrepareMatchBoard();
	void ScheduleNextGlobalSpawn();
	void ScheduleHardLane(int32 LaneIndex);
	ETDDifficulty GetDifficulty() const;
	float GetMatchElapsed() const;
	float GetEasyMediumInterval() const;
	float GetHardDelay() const;
	int32 GetPathCount() const;
	int32 CountLiving(TSubclassOf<AEnemyBase> Class) const;
	TSubclassOf<AEnemyBase> GetGruntClass() const;
	TSubclassOf<AEnemyBase> GetOgreClass() const;
	TSubclassOf<AEnemyBase> GetTrojanClass() const;
	TSubclassOf<AEnemyBase> PickEasyClass();
	TSubclassOf<AEnemyBase> PickMediumClass();
	TSubclassOf<AEnemyBase> PickHardClass() const;
	TArray<FVector> GetWaypointsForPathIndex(int32 PathIndex) const;
	void SpawnEnemyOnLane(TSubclassOf<AEnemyBase> ClassToSpawn, int32 LaneIndex, bool bRandomEntry);
	void ApplyHealthScale(AEnemyBase* Enemy) const;
};
