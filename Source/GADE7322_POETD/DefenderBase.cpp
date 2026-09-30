#include "DefenderBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnemyBase.h"
#include "CentralTowerBase.h"
#include "HealthBarWidget.h"

ADefenderBase::ADefenderBase()
{
	PrimaryActorTick.bCanEverTick = false;

	DefenderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefenderMesh"));
	RootComponent = DefenderMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArcherTowerAsset(TEXT("/Game/Buildings/ArcherTowerT1.ArcherTowerT1"));
	if (ArcherTowerAsset.Succeeded())
	{
		DefenderMesh->SetStaticMesh(ArcherTowerAsset.Object);
		DefenderMesh->SetWorldScale3D(FVector(1.f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
		if (ConeMeshAsset.Succeeded())
		{
			DefenderMesh->SetStaticMesh(ConeMeshAsset.Object);
			DefenderMesh->SetWorldScale3D(FVector(1.f, 1.f, 1.5f));
		}
	}

	DefenderMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DefenderMesh->SetCollisionProfileName(TEXT("BlockAll"));

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, 100.f), FVector2D(56.f, 8.f));
}

void ADefenderBase::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World || !DefenderMesh || !DefenderMesh->GetStaticMesh())
	{
		return;
	}

	const FVector ActorLoc = GetActorLocation();
	const FVector TraceStart(ActorLoc.X, ActorLoc.Y, ActorLoc.Z + 5000.f);
	const FVector TraceEnd(ActorLoc.X, ActorLoc.Y, ActorLoc.Z - 10000.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DefenderGroundSnap), true, this);
	Params.AddIgnoredActor(this);

	TArray<AActor*> IgnoreActors;
	UGameplayStatics::GetAllActorsOfClass(World, ADefenderBase::StaticClass(), IgnoreActors);
	Params.AddIgnoredActors(IgnoreActors);
	TArray<AActor*> Towers;
	UGameplayStatics::GetAllActorsOfClass(World, ACentralTowerBase::StaticClass(), Towers);
	Params.AddIgnoredActors(Towers);
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyBase::StaticClass(), Enemies);
	Params.AddIgnoredActors(Enemies);

	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)
		&& !World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		return;
	}

	// Pivot is not at the visual feet for ArcherTowerT1 — lift by local mesh Min.Z so the
	// bottom of the mesh AABB sits on the tile surface (same idea as the temple snap).
	const FBox LocalBox = DefenderMesh->GetStaticMesh()->GetBoundingBox();
	const float ScaleZ = DefenderMesh->GetComponentScale().Z;
	const float SurfaceZ = Hit.ImpactPoint.Z + 2.f;
	const float NewActorZ = SurfaceZ - (LocalBox.Min.Z * ScaleZ);
	SetActorLocation(FVector(ActorLoc.X, ActorLoc.Y, NewActorZ));

	DefenderMesh->UpdateBounds();
	const FBoxSphereBounds WorldBounds = DefenderMesh->Bounds;
	const float CurrentBottom = WorldBounds.Origin.Z - WorldBounds.BoxExtent.Z;
	const float Fixup = SurfaceZ - CurrentBottom;
	if (!FMath::IsNearlyZero(Fixup, 0.5f))
	{
		SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, Fixup));
	}
}

void ADefenderBase::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* ArcherMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/ArcherTowerT1.ArcherTowerT1")))
	{
		DefenderMesh->SetStaticMesh(ArcherMesh);
		DefenderMesh->SetRelativeLocation(FVector::ZeroVector);
		DefenderMesh->SetRelativeRotation(FRotator::ZeroRotator);
		DefenderMesh->SetWorldScale3D(FVector(1.f));
	}

	SnapToGround();

	// Mesh/collision can finish registering a frame late — re-snap once more.
	if (UWorld* World = GetWorld())
	{
		FTimerHandle ResnapHandle;
		World->GetTimerManager().SetTimer(ResnapHandle, FTimerDelegate::CreateUObject(this, &ADefenderBase::SnapToGround), 0.05f, false);
	}

	if (DefenderMesh && DefenderMesh->GetStaticMesh())
	{
		const FBox LocalBounds = DefenderMesh->GetStaticMesh()->GetBoundingBox();
		const float TopZ = LocalBounds.Max.Z * DefenderMesh->GetComponentScale().Z;
		UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, TopZ + 20.f), FVector2D(72.f, 10.f));
	}

	CurrentHealth = MaxHealth;
	bIsDestroyed = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	UHealthBarWidget::BindToWidgetComponent(HealthBarWidget, this);

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
		if (DefenderMesh)
		{
			DefenderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		OnDefenderDestroyed.Broadcast(this);
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

float ADefenderBase::GetDisplayHealthPercent_Implementation() const
{
	return GetHealthPercent();
}

bool ADefenderBase::IsUnitDestroyed_Implementation() const
{
	return bIsDestroyed;
}