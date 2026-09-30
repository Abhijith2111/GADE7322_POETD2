#include "DefenderBarracks.h"
#include "DefenderKnight.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "ProceduralTerrain.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
	Super::BeginPlay();
	SpawnKnights();

	const float Interval = FMath::Max(KnightTrainInterval, 1.f);
	GetWorldTimerManager().SetTimer(TrainTimerHandle, this, &ADefenderBarracks::TrainKnight, Interval, true, Interval);
}

void ADefenderBarracks::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TrainTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ADefenderBarracks::TrainKnight()
{
	if (IsDestroyed())
	{
		GetWorldTimerManager().ClearTimer(TrainTimerHandle);
		return;
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

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ADefenderKnight>(ADefenderKnight::StaticClass(), GetActorLocation() + Offset, FRotator::ZeroRotator, SpawnParams);
}
