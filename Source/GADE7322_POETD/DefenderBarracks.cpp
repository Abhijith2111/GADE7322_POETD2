#include "DefenderBarracks.h"
#include "DefenderKnight.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "ProceduralTerrain.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "TDGameInstance.h"
#include "WeatherManager.h"

ADefenderBarracks::ADefenderBarracks()
{
	MaxHealth = 400.f;
	AttackDamage = 0.f;
	AttackRange = 0.f;
	KnightCount = 5;
	KnightTrainInterval = 10.f;

	if (DefenderMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> BarracksMesh(TEXT("/Game/Buildings/BarracksT1.BarracksT1"));
		if (BarracksMesh.Succeeded())
		{
			DefenderMesh->SetStaticMesh(BarracksMesh.Object);
		}
	}
}

void ADefenderBarracks::ApplyDefenderMesh()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* BarracksMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/BarracksT1.BarracksT1")))
	{
		DefenderMesh->SetStaticMesh(BarracksMesh);
		DefenderMesh->SetWorldScale3D(FVector(1.f));
	}

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
	}

	if (UStaticMesh* Mesh = DefenderMesh->GetStaticMesh())
	{
		const FBox MeshBox = Mesh->GetBoundingBox();
		const float MeshWidth = FMath::Max(FMath::Max(MeshBox.GetSize().X, MeshBox.GetSize().Y), 1.f);
		const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
		DefenderMesh->SetWorldScale3D(FVector(
			(TileSize * 0.9f) / MeshWidth,
			(TileSize * 0.9f) / MeshWidth,
			(TileSize * 0.75f) / MeshHeight));
	}
}

void ADefenderBarracks::BeginPlay()
{
	KnightCount = 5;
	KnightTrainInterval = 10.f;
	if (const UTDGameInstance* GI = Cast<UTDGameInstance>(GetGameInstance()))
	{
		if (GI->GetDifficulty() == ETDDifficulty::Medium)
		{
			KnightCount = 3;
			KnightTrainInterval = 15.f;
		}
		else if (GI->GetDifficulty() == ETDDifficulty::Hard)
		{
			KnightCount = 1;
			KnightTrainInterval = 30.f;
		}
	}

	Super::BeginPlay();

	if (AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		Weather->OnWeatherChanged.AddDynamic(this, &ADefenderBarracks::HandleWeatherChanged);
	}

	RefreshTrainingForWeather(true);
}

void ADefenderBarracks::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		Weather->OnWeatherChanged.RemoveDynamic(this, &ADefenderBarracks::HandleWeatherChanged);
	}

	GetWorldTimerManager().ClearTimer(TrainTimerHandle);
	GetWorldTimerManager().ClearTimer(InitialSpawnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

float ADefenderBarracks::GetEffectiveTrainInterval() const
{
	float Interval = KnightTrainInterval;
	if (const AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		if (Weather->IsNight())
		{
			Interval += 5.f;
		}
	}
	return FMath::Max(Interval, 1.f);
}

void ADefenderBarracks::RefreshTrainingForWeather(bool bAllowInitialSpawn)
{
	if (!GetWorld() || IsDestroyed())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(TrainTimerHandle);
	GetWorldTimerManager().ClearTimer(InitialSpawnTimerHandle);

	const float Interval = GetEffectiveTrainInterval();
	const bool bNight = AWeatherManager::Get(this) && AWeatherManager::Get(this)->IsNight();

	if (!bInitialKnightsSpawned)
	{
		if (!bAllowInitialSpawn)
		{
			return;
		}

		if (bNight)
		{
			GetWorldTimerManager().SetTimer(InitialSpawnTimerHandle, this, &ADefenderBarracks::SpawnInitialKnights, 5.f, false);
			GetWorldTimerManager().SetTimer(TrainTimerHandle, this, &ADefenderBarracks::TrainKnight, Interval, true, 5.f + Interval);
		}
		else
		{
			SpawnInitialKnights();
			GetWorldTimerManager().SetTimer(TrainTimerHandle, this, &ADefenderBarracks::TrainKnight, Interval, true, Interval);
		}
		return;
	}

	GetWorldTimerManager().SetTimer(TrainTimerHandle, this, &ADefenderBarracks::TrainKnight, Interval, true, Interval);
}

void ADefenderBarracks::SpawnInitialKnights()
{
	if (bInitialKnightsSpawned || IsDestroyed())
	{
		return;
	}

	SpawnKnights();
	bInitialKnightsSpawned = true;
}

void ADefenderBarracks::HandleWeatherChanged(EWeatherType NewWeather)
{
	RefreshTrainingForWeather(true);
}

void ADefenderBarracks::TrainKnight()
{
	if (IsDestroyed())
	{
		GetWorldTimerManager().ClearTimer(TrainTimerHandle);
		return;
	}

	if (!bInitialKnightsSpawned)
	{
		SpawnInitialKnights();
	}

	SpawnOneKnight();
}

void ADefenderBarracks::ScanAndAttack()
{
}

void ADefenderBarracks::SpawnKnights()
{
	const int32 Count = FMath::Max(KnightCount, 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		SpawnOneKnight();
	}
}

void ADefenderBarracks::SpawnOneKnight()
{
	UWorld* World = GetWorld();
	if (!World || IsDestroyed())
	{
		return;
	}

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(World, AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
	}

	const float Angle = FMath::DegreesToRadians(-80.f + (TrainedKnightCount % 5) * 40.f);
	const FVector Offset(FMath::Cos(Angle) * TileSize * 0.55f, FMath::Sin(Angle) * TileSize * 0.55f, 40.f);
	++TrainedKnightCount;

	TSubclassOf<ADefenderKnight> KnightClass = ADefenderKnight::StaticClass();
	if (const AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		if (Weather->IsNight() && NightKnightClass)
		{
			KnightClass = NightKnightClass;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ADefenderKnight>(KnightClass, GetActorLocation() + Offset, FRotator::ZeroRotator, SpawnParams);
}

void ADefenderBarracks::ApplyTrainUpgrade()
{
	KnightTrainInterval = FMath::Max(1.f, KnightTrainInterval - 3.f);
	RefreshTrainingForWeather(true);
}

void ADefenderBarracks::ApplyUpgradeVisual()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* UpgradeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/BarracksT2.BarracksT2")))
	{
		DefenderMesh->SetStaticMesh(UpgradeMesh);
		DefenderMesh->SetWorldScale3D(FVector(1.f));
		RefreshMeshPlacementAfterSwap();
	}
}
