#include "EnemySpawner.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));

	if (TerrainRef)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Found ProceduralTerrain with %d pathways."), TerrainRef->Pathways.Num());
	}
	else if (ManualWaypoints.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner: No ProceduralTerrain found - using %d manual waypoints."), ManualWaypoints.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: No ProceduralTerrain and no ManualWaypoints assigned. Enemies will not move."));
	}

	StartSpawning();
}

void AEnemySpawner::StartSpawning()
{
	if (!EnemyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: EnemyClass is not assigned. Spawning aborted."));
		return;
	}

	SpawnedThisWave = 0;
	CurrentPathIndex = 0;

	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::SpawnNextEnemy, SpawnInterval, true, 0.f);
}

void AEnemySpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Spawning stopped."));
}

bool AEnemySpawner::HasValidWaypoints() const
{
	if (TerrainRef && TerrainRef->Pathways.Num() > 0)
	{
		return true;
	}

	return ManualWaypoints.Num() > 0;
}

TArray<FVector> AEnemySpawner::GetWaypointsForPathIndex(int32 PathIndex) const
{
	if (TerrainRef && TerrainRef->Pathways.IsValidIndex(PathIndex))
	{
		return TerrainRef->Pathways[PathIndex].Nodes;
	}

	return ManualWaypoints;
}

void AEnemySpawner::SpawnNextEnemy()
{
	if (!EnemyClass)
	{
		return;
	}

	if (SpawnedThisWave >= MaxEnemiesPerWave)
	{
		if (bLoopWaves)
		{
			SpawnedThisWave = 0;
			UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Wave complete, looping."));
		}
		else
		{
			StopSpawning();
			return;
		}
	}

	const bool bHasTerrain = TerrainRef && TerrainRef->Pathways.Num() > 0;
	const int32 PathCount = bHasTerrain ? TerrainRef->Pathways.Num() : 1;
	CurrentPathIndex = SpawnedThisWave % PathCount;

	TArray<FVector> Waypoints = GetWaypointsForPathIndex(CurrentPathIndex);
	if (Waypoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: No waypoints available for path %d. Skipping spawn."), CurrentPathIndex);
		++SpawnedThisWave;
		return;
	}

	const FVector PathStartXY = Waypoints[0];
	const float TraceStartZ = PathStartXY.Z + 2000.f;
	const float TraceEndZ = PathStartXY.Z - 500.f;

	const FVector TraceStart(PathStartXY.X, PathStartXY.Y, TraceStartZ);
	const FVector TraceEnd(PathStartXY.X, PathStartXY.Y, TraceEndZ);

	FHitResult SurfaceHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FVector SpawnLocation;
	const bool bFoundSurface = GetWorld()->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	if (bFoundSurface)
	{
		SpawnLocation = SurfaceHit.ImpactPoint + FVector(0.f, 0.f, SpawnHeightOffset);
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Surface found at Z=%.1f, spawning at Z=%.1f"), SurfaceHit.ImpactPoint.Z, SpawnLocation.Z);
	}
	else
	{
		SpawnLocation = PathStartXY + FVector(0.f, 0.f, SpawnHeightOffset);
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: No surface found under path start - using fallback Z offset."));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyBase* NewEnemy = GetWorld()->SpawnActor<AEnemyBase>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (NewEnemy)
	{
		TArray<FVector> ElevatedWaypoints;
		ElevatedWaypoints.Reserve(Waypoints.Num());

		FCollisionQueryParams WaypointQueryParams;
		WaypointQueryParams.AddIgnoredActor(this);
		WaypointQueryParams.AddIgnoredActor(NewEnemy);

		for (const FVector& Node : Waypoints)
		{
			const FVector NodeTraceStart(Node.X, Node.Y, Node.Z + 2000.f);
			const FVector NodeTraceEnd(Node.X, Node.Y, Node.Z - 500.f);

			FHitResult NodeHit;
			const bool bNodeHit = GetWorld()->LineTraceSingleByChannel(NodeHit, NodeTraceStart, NodeTraceEnd, ECC_Visibility, WaypointQueryParams);

			if (bNodeHit)
			{
				ElevatedWaypoints.Add(NodeHit.ImpactPoint + FVector(0.f, 0.f, SpawnHeightOffset));
			}
			else
			{
				ElevatedWaypoints.Add(Node + FVector(0.f, 0.f, SpawnHeightOffset));
			}
		}

		NewEnemy->InitialiseWithWaypoints(ElevatedWaypoints);
		OnEnemySpawned.Broadcast(NewEnemy);

		UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Spawned %s on path %d at %s"),
			*NewEnemy->GetName(), CurrentPathIndex, *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: Failed to spawn enemy on path %d."), CurrentPathIndex);
	}

	++SpawnedThisWave;
}