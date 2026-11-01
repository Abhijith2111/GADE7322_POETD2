#include "EnemySpawner.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "EnemyBrute.h"
#include "EnemyTrojanHorse.h"
#include "TDGameMode.h"
#include "TDGameInstance.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	BruteClass = AEnemyBrute::StaticClass();
	TrojanClass = AEnemyTrojanHorse::StaticClass();

	static ConstructorHelpers::FClassFinder<AEnemyBase> NormalEnemyBP(TEXT("/Game/Gameplay/Enemies/Blueprints/BP_EnemyBase"));
	if (NormalEnemyBP.Succeeded())
	{
		EnemyClass = NormalEnemyBP.Class;
	}
	else
	{
		EnemyClass = AEnemyBase::StaticClass();
	}
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!EnemyClass)
	{
		EnemyClass = AEnemyBase::StaticClass();
	}
	if (!BruteClass)
	{
		BruteClass = AEnemyBrute::StaticClass();
	}
	if (!TrojanClass)
	{
		TrojanClass = AEnemyTrojanHorse::StaticClass();
	}

	TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));

	if (const ATDGameMode* GM = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (GM->bSkipMainMenu)
		{
			StartSpawning();
		}
	}
}

void AEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSpawnTimers();
	Super::EndPlay(EndPlayReason);
}

void AEnemySpawner::StartSpawning()
{
	if (!EnemyClass && !BruteClass && !TrojanClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: No enemy classes assigned. Spawning aborted."));
		return;
	}

	ClearSpawnTimers();
	bSpawning = true;
	MatchStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	NextLane = 0;
	OgresSpawned = 0;
	MediumStep = 0;
	bTrojanUnlocked = false;
	bSpawnedUnlockTrojan = false;
	OpeningGruntsRemaining = GetDifficulty() == ETDDifficulty::Easy ? FMath::RandRange(2, 3) : 0;

	PrepareMatchBoard();

	if (GetDifficulty() == ETDDifficulty::Hard)
	{
		const int32 LaneCount = GetPathCount();
		HardLaneTimers.SetNum(LaneCount);
		for (int32 Lane = 0; Lane < LaneCount; ++Lane)
		{
			SpawnOnLane(Lane);
		}
		return;
	}

	ScheduleNextGlobalSpawn();
}

void AEnemySpawner::StopSpawning()
{
	ClearSpawnTimers();
	UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Spawning stopped."));
}

void AEnemySpawner::ClearSpawnTimers()
{
	bSpawning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		for (FTimerHandle& Handle : HardLaneTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	HardLaneTimers.Reset();
}

void AEnemySpawner::PrepareMatchBoard()
{
	if (!TerrainRef)
	{
		TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));
	}
	if (!TerrainRef)
	{
		return;
	}

	const int32 LaneCount = GetDifficulty() == ETDDifficulty::Hard ? FMath::RandRange(3, 5) : 3;
	UE_LOG(LogTemp, Log, TEXT("EnemySpawner: Starting level on %s with %d lanes."),
		GetDifficulty() == ETDDifficulty::Hard ? TEXT("Difficult") : TEXT("Easy/Medium"),
		LaneCount);
	TerrainRef->PrepareMatchBoard(LaneCount);
}

void AEnemySpawner::ScheduleNextGlobalSpawn()
{
	if (!bSpawning || !GetWorld())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::SpawnNextEnemy, GetEasyMediumInterval(), false);
}

void AEnemySpawner::ScheduleHardLane(int32 LaneIndex)
{
	if (!bSpawning || !GetWorld())
	{
		return;
	}

	if (!HardLaneTimers.IsValidIndex(LaneIndex))
	{
		return;
	}

	FTimerDelegate Delegate;
	Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(AEnemySpawner, SpawnOnLane), LaneIndex);
	GetWorldTimerManager().SetTimer(HardLaneTimers[LaneIndex], Delegate, GetHardDelay(), false);
}

ETDDifficulty AEnemySpawner::GetDifficulty() const
{
	if (const UTDGameInstance* TDInstance = Cast<UTDGameInstance>(GetGameInstance()))
	{
		return TDInstance->GetDifficulty();
	}

	return ETDDifficulty::Easy;
}

float AEnemySpawner::GetMatchElapsed() const
{
	if (!GetWorld())
	{
		return 0.f;
	}

	return FMath::Max(0.f, GetWorld()->GetTimeSeconds() - MatchStartTime);
}

float AEnemySpawner::GetEasyMediumInterval() const
{
	const float Elapsed = GetMatchElapsed();
	if (GetDifficulty() == ETDDifficulty::Easy)
	{
		return FMath::Max(4.f, 8.f - Elapsed * (4.f / 180.f));
	}

	return FMath::Max(2.f, 4.5f - Elapsed * (2.5f / 120.f));
}

float AEnemySpawner::GetHardDelay() const
{
	if (GetMatchElapsed() < 45.f)
	{
		return 9.f;
	}

	return FMath::FRandRange(4.f, 9.f);
}

int32 AEnemySpawner::GetPathCount() const
{
	if (TerrainRef && TerrainRef->Pathways.Num() > 0)
	{
		return TerrainRef->Pathways.Num();
	}

	return 1;
}

int32 AEnemySpawner::CountLiving(TSubclassOf<AEnemyBase> Class) const
{
	if (!Class || !GetWorld())
	{
		return 0;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), Class, Found);
	int32 Count = 0;
	for (AActor* Actor : Found)
	{
		const AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		if (Enemy && !Enemy->IsDefeated())
		{
			++Count;
		}
	}

	return Count;
}

TSubclassOf<AEnemyBase> AEnemySpawner::GetGruntClass() const
{
	return EnemyClass ? EnemyClass : TSubclassOf<AEnemyBase>(AEnemyBase::StaticClass());
}

TSubclassOf<AEnemyBase> AEnemySpawner::GetOgreClass() const
{
	return BruteClass ? BruteClass : TSubclassOf<AEnemyBase>(AEnemyBrute::StaticClass());
}

TSubclassOf<AEnemyBase> AEnemySpawner::GetTrojanClass() const
{
	return TrojanClass ? TrojanClass : TSubclassOf<AEnemyBase>(AEnemyTrojanHorse::StaticClass());
}

TSubclassOf<AEnemyBase> AEnemySpawner::PickEasyClass()
{
	if (OpeningGruntsRemaining > 0)
	{
		--OpeningGruntsRemaining;
		return GetGruntClass();
	}

	const int32 LivingOgres = CountLiving(GetOgreClass());
	const int32 LivingTrojans = CountLiving(GetTrojanClass());
	if (OgresSpawned > 0 && LivingOgres == 0)
	{
		bTrojanUnlocked = true;
	}

	if (bTrojanUnlocked && !bSpawnedUnlockTrojan && LivingTrojans == 0)
	{
		bSpawnedUnlockTrojan = true;
		return GetTrojanClass();
	}

	if (LivingOgres == 0)
	{
		return GetOgreClass();
	}

	if (bTrojanUnlocked && LivingTrojans == 0)
	{
		return GetTrojanClass();
	}

	return GetGruntClass();
}

TSubclassOf<AEnemyBase> AEnemySpawner::PickMediumClass()
{
	if (MediumStep < 3)
	{
		return GetGruntClass();
	}
	if (MediumStep == 3)
	{
		return GetOgreClass();
	}
	if (MediumStep == 4)
	{
		return GetTrojanClass();
	}

	const int32 Roll = FMath::RandRange(0, 2);
	if (Roll == 1)
	{
		if (CountLiving(GetOgreClass()) >= 5)
		{
			return GetGruntClass();
		}
		return GetOgreClass();
	}
	if (Roll == 2)
	{
		return GetTrojanClass();
	}

	return GetGruntClass();
}

TSubclassOf<AEnemyBase> AEnemySpawner::PickHardClass() const
{
	const int32 Roll = FMath::RandRange(0, 2);
	if (Roll == 1)
	{
		return GetOgreClass();
	}
	if (Roll == 2)
	{
		return GetTrojanClass();
	}

	return GetGruntClass();
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
	if (!bSpawning)
	{
		return;
	}

	const ETDDifficulty Difficulty = GetDifficulty();
	TSubclassOf<AEnemyBase> ClassToSpawn = Difficulty == ETDDifficulty::Medium ? PickMediumClass() : PickEasyClass();
	const int32 PathCount = GetPathCount();
	int32 Lane = NextLane % PathCount;

	if (Difficulty == ETDDifficulty::Medium && MediumStep < 3)
	{
		Lane = MediumStep % PathCount;
	}
	else if (Difficulty == ETDDifficulty::Medium && MediumStep <= 4)
	{
		Lane = FMath::RandRange(0, PathCount - 1);
	}
	else
	{
		NextLane = (NextLane + 1) % PathCount;
	}

	if (Difficulty == ETDDifficulty::Medium)
	{
		++MediumStep;
	}

	SpawnEnemyOnLane(ClassToSpawn, Lane, false);
	ScheduleNextGlobalSpawn();
}

void AEnemySpawner::SpawnOnLane(int32 LaneIndex)
{
	if (!bSpawning)
	{
		return;
	}

	SpawnEnemyOnLane(PickHardClass(), LaneIndex, true);
	ScheduleHardLane(LaneIndex);
}

void AEnemySpawner::SpawnEnemyOnLane(TSubclassOf<AEnemyBase> ClassToSpawn, int32 LaneIndex, bool bRandomEntry)
{
	if (!ClassToSpawn || !GetWorld())
	{
		return;
	}

	TArray<FVector> Waypoints = GetWaypointsForPathIndex(LaneIndex);
	if (Waypoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: No waypoints available for path %d. Skipping spawn."), LaneIndex);
		return;
	}

	int32 EntryIndex = 0;
	if (bRandomEntry && Waypoints.Num() > 1)
	{
		EntryIndex = FMath::RandRange(0, Waypoints.Num() / 2);
	}

	TArray<FVector> ElevatedWaypoints;
	ElevatedWaypoints.Reserve(Waypoints.Num() - EntryIndex);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	for (int32 Index = EntryIndex; Index < Waypoints.Num(); ++Index)
	{
		const FVector& Node = Waypoints[Index];
		const FVector NodeTraceStart(Node.X, Node.Y, Node.Z + 2000.f);
		const FVector NodeTraceEnd(Node.X, Node.Y, Node.Z - 500.f);
		FHitResult NodeHit;
		if (GetWorld()->LineTraceSingleByChannel(NodeHit, NodeTraceStart, NodeTraceEnd, ECC_Visibility, QueryParams))
		{
			ElevatedWaypoints.Add(NodeHit.ImpactPoint + FVector(0.f, 0.f, SpawnHeightOffset));
		}
		else
		{
			ElevatedWaypoints.Add(Node + FVector(0.f, 0.f, SpawnHeightOffset));
		}
	}

	if (ElevatedWaypoints.Num() == 0)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AEnemyBase* NewEnemy = GetWorld()->SpawnActor<AEnemyBase>(ClassToSpawn, ElevatedWaypoints[0], FRotator::ZeroRotator, SpawnParams);
	if (!NewEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner: Failed to spawn enemy on path %d."), LaneIndex);
		return;
	}

	if (ClassToSpawn == GetOgreClass())
	{
		++OgresSpawned;
	}

	ApplyHealthScale(NewEnemy);
	NewEnemy->InitialiseWithWaypoints(ElevatedWaypoints);
	OnEnemySpawned.Broadcast(NewEnemy);
}

void AEnemySpawner::ApplyHealthScale(AEnemyBase* Enemy) const
{
	if (!Enemy)
	{
		return;
	}

	const float Elapsed = GetMatchElapsed();
	float Bonus = 0.f;
	switch (GetDifficulty())
	{
	case ETDDifficulty::Easy:
		Bonus = 0.10f * (Elapsed / 45.f);
		break;
	case ETDDifficulty::Medium:
		Bonus = 0.15f * (Elapsed / 30.f);
		break;
	default:
		Bonus = 0.20f * (Elapsed / 25.f);
		break;
	}

	const float Scale = 1.f + Bonus;
	Enemy->MaxHealth *= Scale;
	Enemy->CurrentHealth = Enemy->MaxHealth;
	Enemy->OnHealthChanged.Broadcast(Enemy->CurrentHealth, Enemy->MaxHealth);
}
