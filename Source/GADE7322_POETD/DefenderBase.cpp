#include "DefenderBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "EnemyBase.h"

ADefenderBase::ADefenderBase()
{
	PrimaryActorTick.bCanEverTick = false;

	DefenderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefenderMesh"));
	RootComponent = DefenderMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMeshAsset.Succeeded())
	{
		DefenderMesh->SetStaticMesh(ConeMeshAsset.Object);
		DefenderMesh->SetWorldScale3D(FVector(1.f, 1.f, 1.5f));
	}

	DefenderMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DefenderMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ADefenderBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDestroyed = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	const float InitialDelay = FMath::FRandRange(0.f, AttackInterval);
	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ADefenderBase::ScanAndAttack, AttackInterval, true, InitialDelay);
}

void ADefenderBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ADefenderBase::ScanAndAttack()
{
	if (bIsDestroyed)
	{
		return;
	}

	AActor* Target = FindNearestTarget();
	if (!Target)
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(Target);
	if (Enemy)
	{
		Enemy->TakeDamageFromDefender(AttackDamage);
	}

	UWorld* World = GetWorld();
	if (World)
	{
		DrawDebugLine(World, GetActorLocation(), Target->GetActorLocation(), FColor::Red, false, AttackInterval * 0.5f, 0, 3.f);
	}

	UE_LOG(LogTemp, Log, TEXT("Defender %s attacked %s for %.1f damage"), *GetName(), *Target->GetName(), AttackDamage);
}

AActor* ADefenderBase::FindNearestTarget() const
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

void ADefenderBase::ApplyDamage(float DamageAmount)
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
		OnDefenderDestroyed.Broadcast();
		SetLifeSpan(0.2f);
	}
}

bool ADefenderBase::IsDestroyed() const
{
	return bIsDestroyed;
}

float ADefenderBase::GetHealthPercent() const
{
	return MaxHealth > 0.f ? (CurrentHealth / MaxHealth) : 0.f;
}