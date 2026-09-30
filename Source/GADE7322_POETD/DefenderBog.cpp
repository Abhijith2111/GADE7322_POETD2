#include "DefenderBog.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EnemyBase.h"
#include "HealthBarWidget.h"
#include "TimerManager.h"
#include "ProceduralTerrain.h"
#include "Kismet/GameplayStatics.h"

ADefenderBog::ADefenderBog()
{
	MaxHealth = 300.f;
	AttackRange = 220.f;
	AttackDamage = 0.f;
	AttackInterval = 0.25f;
	DigestDuration = 20.f;

	if (DefenderMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
		if (ConeMesh.Succeeded())
		{
			DefenderMesh->SetStaticMesh(ConeMesh.Object);
		}
	}
}

void ADefenderBog::ApplyDefenderMesh()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")))
	{
		DefenderMesh->SetStaticMesh(ConeMesh);
	}

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
	}

	const FBox ConeBox = DefenderMesh->GetStaticMesh() ? DefenderMesh->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
	const float MeshWidth = FMath::Max(ConeBox.GetSize().X, 1.f);
	const float MeshHeight = FMath::Max(ConeBox.GetSize().Z, 1.f);
	DefenderMesh->SetWorldScale3D(FVector(
		(TileSize * 0.55f) / MeshWidth,
		(TileSize * 0.55f) / MeshWidth,
		(TileSize * 0.85f) / MeshHeight));

	if (UMaterialInterface* BaseMat = DefenderMesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			const FLinearColor Purple(0.45f, 0.12f, 0.72f);
			DynMat->SetVectorParameterValue(TEXT("Color"), Purple);
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Purple);
			DefenderMesh->SetMaterial(0, DynMat);
		}
	}
}

void ADefenderBog::BeginPlay()
{
	Super::BeginPlay();

	if (DefenderMesh && DefenderMesh->GetStaticMesh() && HealthBarWidget)
	{
		const FBox LocalBounds = DefenderMesh->GetStaticMesh()->GetBoundingBox();
		const float ScaleZ = FMath::Max(DefenderMesh->GetComponentScale().Z, KINDA_SMALL_NUMBER);
		const FVector BarOffset(
			(LocalBounds.Min.X + LocalBounds.Max.X) * 0.5f,
			(LocalBounds.Min.Y + LocalBounds.Max.Y) * 0.5f,
			LocalBounds.Max.Z + (24.f / ScaleZ));
		UHealthBarWidget::ConfigureComponent(HealthBarWidget, BarOffset, FVector2D(90.f, 12.f));
	}
}

void ADefenderBog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DigestTimerHandle);
	Super::EndPlay(EndPlayReason);
}

float ADefenderBog::GetGrabRange() const
{
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		return FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y) * 1.15f;
	}

	return FMath::Max(AttackRange, 400.f);
}

AEnemyBase* ADefenderBog::FindEnemyToEat() const
{
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), FoundEnemies);

	AEnemyBase* Nearest = nullptr;
	float NearestDistSq = FMath::Square(GetGrabRange());

	for (AActor* Actor : FoundEnemies)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		if (!Enemy || Enemy->IsDefeated() || Enemy->IsBeingEaten())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(GetActorLocation(), Enemy->GetActorLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Enemy;
		}
	}

	return Nearest;
}

void ADefenderBog::ScanAndAttack()
{
	if (IsDestroyed() || bIsDigesting)
	{
		return;
	}

	AEnemyBase* Enemy = PulledEnemy.Get();
	if (!Enemy || Enemy->IsDefeated())
	{
		PulledEnemy = nullptr;
		Enemy = FindEnemyToEat();
		if (!Enemy)
		{
			return;
		}

		Enemy->BeginBogPull(this);
		PulledEnemy = Enemy;
	}

	const float EatDistance = FMath::Clamp(GetGrabRange() * 0.28f, 90.f, 280.f);
	if (FVector::DistSquared2D(GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(EatDistance))
	{
		return;
	}

	Enemy->TakeDamageFromDefender(Enemy->CurrentHealth);
	PulledEnemy = nullptr;
	bIsDigesting = true;

	UE_LOG(LogTemp, Log, TEXT("Bog %s ate %s. Digesting for %.0f seconds."), *GetName(), *Enemy->GetName(), DigestDuration);

	GetWorldTimerManager().SetTimer(DigestTimerHandle, this, &ADefenderBog::FinishDigesting, DigestDuration, false);
}

void ADefenderBog::FinishDigesting()
{
	bIsDigesting = false;
}
