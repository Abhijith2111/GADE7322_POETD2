#include "DefenderKnight.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnemyBase.h"
#include "EnemyBrute.h"
#include "EnemyTrojanHorse.h"
#include "ProceduralTerrain.h"
#include "Kismet/GameplayStatics.h"

ADefenderKnight::ADefenderKnight()
{
	PrimaryActorTick.bCanEverTick = true;
	MaxHealth = 150.f;
	AttackDamage = 30.f;
	AttackRange = 100.f;
	AttackInterval = 0.8f;
	SightRange = 1400.f;
	MoveSpeed = 340.f;

	if (DefenderMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderMesh.Succeeded())
		{
			DefenderMesh->SetStaticMesh(CylinderMesh.Object);
		}
	}
}

void ADefenderKnight::ApplyDefenderMesh()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		DefenderMesh->SetStaticMesh(CylinderMesh);
	}

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
		SightRange = FMath::Max(TileSize * 4.f, 900.f);
	}

	const FBox MeshBox = DefenderMesh->GetStaticMesh() ? DefenderMesh->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
	const float MeshWidth = FMath::Max(MeshBox.GetSize().X, 1.f);
	const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
	DefenderMesh->SetWorldScale3D(FVector(
		(TileSize * 0.18f) / MeshWidth,
		(TileSize * 0.18f) / MeshWidth,
		(TileSize * 0.32f) / MeshHeight));

	if (UMaterialInterface* BaseMat = DefenderMesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			const FLinearColor Steel(0.25f, 0.4f, 0.85f);
			DynMat->SetVectorParameterValue(TEXT("Color"), Steel);
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Steel);
			DefenderMesh->SetMaterial(0, DynMat);
		}
	}
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
		if (!Enemy || Enemy->IsDefeated() || Enemy->IsBeingEaten())
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
