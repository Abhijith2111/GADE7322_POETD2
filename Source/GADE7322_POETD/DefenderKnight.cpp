#include "DefenderKnight.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "EnemyBase.h"
#include "EnemyBrute.h"
#include "EnemyTrojanHorse.h"
#include "ProceduralTerrain.h"
#include "WeatherManager.h"
#include "Kismet/GameplayStatics.h"

ADefenderKnight::ADefenderKnight()
{
	PrimaryActorTick.bCanEverTick = true;
	MaxHealth = 150.f;
	AttackDamage = 30.f;
	AttackRange = 100.f;
	AttackInterval = 1.5f;
	SightRange = 1400.f;
	MoveSpeed = 340.f;

	if (DefenderMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> KnightMesh(TEXT("/Game/Characters/Knight_N_.Knight_N_"));
		if (KnightMesh.Succeeded())
		{
			DefenderMesh->SetStaticMesh(KnightMesh.Object);
		}
	}
}

void ADefenderKnight::BeginPlay()
{
	Super::BeginPlay();

	if (!DefenderMesh || !DefenderMesh->GetStaticMesh())
	{
		return;
	}

	const float ScaledHeight = DefenderMesh->GetStaticMesh()->GetBoundingBox().GetSize().Z * FMath::Abs(DefenderMesh->GetComponentScale().Z);
	AddActorWorldOffset(FVector(0.f, 0.f, ScaledHeight));
}

void ADefenderKnight::ApplyDefenderMesh()
{
	if (!DefenderMesh)
	{
		return;
	}

	const bool bNight = AWeatherManager::Get(this) && AWeatherManager::Get(this)->IsNight();
	const TCHAR* MeshPath = bNight
		? TEXT("/Game/Characters/KnightTorch_N_.KnightTorch_N_")
		: TEXT("/Game/Characters/Knight_N_.Knight_N_");
	UStaticMesh* KnightMesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!KnightMesh)
	{
		KnightMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Knight_N_.Knight_N_"));
	}
	if (KnightMesh)
	{
		DefenderMesh->SetStaticMesh(KnightMesh);
	}

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
		SightRange = FMath::Max(TileSize * 4.f, 900.f);
	}

	const FBox MeshBox = DefenderMesh->GetStaticMesh() ? DefenderMesh->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
	const float MeshWidth = FMath::Max(FMath::Max(MeshBox.GetSize().X, MeshBox.GetSize().Y), 1.f);
	const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
	const float Uniform = FMath::Min((TileSize * 0.55f) / MeshHeight, (TileSize * 0.35f) / MeshWidth);
	DefenderMesh->SetWorldScale3D(FVector(Uniform));
}

AEnemyBase* ADefenderKnight::FindPriorityTarget() const
{
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), FoundEnemies);

	AEnemyBase* NearestBrute = nullptr;
	AEnemyBase* NearestTrojan = nullptr;
	AEnemyBase* NearestNormal = nullptr;
	float BruteDistSq = FMath::Square(SightRange);
	float TrojanDistSq = BruteDistSq;
	float NormalDistSq = BruteDistSq;

	for (AActor* Actor : FoundEnemies)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		if (!Enemy || Enemy->IsDefeated() || Enemy->IsBeingEaten() || Enemy->IsBogAlly())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(GetActorLocation(), Enemy->GetActorLocation());
		if (Cast<AEnemyBrute>(Enemy))
		{
			if (DistSq <= BruteDistSq)
			{
				BruteDistSq = DistSq;
				NearestBrute = Enemy;
			}
		}
		else if (Cast<AEnemyTrojanHorse>(Enemy))
		{
			if (DistSq <= TrojanDistSq)
			{
				TrojanDistSq = DistSq;
				NearestTrojan = Enemy;
			}
		}
		else if (DistSq <= NormalDistSq)
		{
			NormalDistSq = DistSq;
			NearestNormal = Enemy;
		}
	}

	if (NearestBrute)
	{
		return NearestBrute;
	}
	if (NearestTrojan)
	{
		return NearestTrojan;
	}
	return NearestNormal;
}

void ADefenderKnight::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsDestroyed())
	{
		return;
	}

	AEnemyBase* Target = FindPriorityTarget();
	if (!Target)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	const FVector ToTarget = Target->GetActorLocation() - MyLoc;
	if (ToTarget.Size2D() <= AttackRange)
	{
		return;
	}

	const FVector Step = ToTarget.GetSafeNormal2D() * MoveSpeed * DeltaTime;
	SetActorLocation(FVector(MyLoc.X + Step.X, MyLoc.Y + Step.Y, MyLoc.Z), false);
	SetActorRotation(ToTarget.GetSafeNormal2D().Rotation());
}

void ADefenderKnight::ScanAndAttack()
{
	if (IsDestroyed())
	{
		return;
	}

	AEnemyBase* Target = FindPriorityTarget();
	if (!Target)
	{
		return;
	}

	if (FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(AttackRange))
	{
		return;
	}

	Target->TakeDamageFromDefender(AttackDamage);
}
