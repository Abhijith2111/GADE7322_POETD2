#include "CentralTowerBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "EnemyBase.h"

ACentralTowerBase::ACentralTowerBase()
{
	PrimaryActorTick.bCanEverTick = false;

	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	RootComponent = TowerMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		TowerMesh->SetStaticMesh(CubeMeshAsset.Object);
		TowerMesh->SetWorldScale3D(FVector(2.f, 2.f, 4.f));
	}

	TowerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TowerMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ACentralTowerBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDestroyed = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ACentralTowerBase::ScanAndAttack, AttackInterval, true, 0.5f);
}

void ACentralTowerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ACentralTowerBase::ScanAndAttack()
{
	if (bIsDestroyed)
	{
		return;
	}

	AActor* Target = FindNearestEnemy();
	if (!Target)
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(Target);
	if (Enemy)
	{
		Enemy->TakeDamageFromDefender(AttackDamage);

		UWorld* World = GetWorld();
		if (World)
		{
			DrawDebugLine(World, GetActorLocation(), Enemy->GetActorLocation(), FColor::Cyan, false, AttackInterval * 0.5f, 0, 4.f);
		}

		UE_LOG(LogTemp, Log, TEXT("CentralTower attacked %s for %.1f damage"), *Enemy->GetName(), AttackDamage);
	}
}

AActor* ACentralTowerBase::FindNearestEnemy() const
{
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), FoundEnemies);

	AActor* Nearest = nullptr;
	float NearestDistSq = FMath::Square(AttackRange);

	for (AActor* Actor : FoundEnemies)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		if (!Enemy || Enemy->IsDefeated())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Enemy->GetActorLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Enemy;
		}
	}

	return Nearest;
}

void ACentralTowerBase::ApplyDamage(float DamageAmount)
{
	if (bIsDestroyed || DamageAmount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		bIsDestroyed = true;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		OnTowerDestroyed.Broadcast();
		UE_LOG(LogTemp, Warning, TEXT("CentralTowerBase destroyed!"));
	}
}

bool ACentralTowerBase::IsDestroyed() const
{
	return bIsDestroyed;
}

float ACentralTowerBase::GetHealthPercent() const
{
	return MaxHealth > 0.f ? (CurrentHealth / MaxHealth) : 0.f;
}