#include "DefenderBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnemyBase.h"
#include "CentralTowerBase.h"
#include "WorldHealthBar.h"
#include "GameVFX.h"
#include "WeatherManager.h"

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
	UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, 100.f), FVector2D(140.f, 18.f));
}

void ADefenderBase::SeatMeshOnPivot()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (DefenderMesh->GetAttachParent())
	{
		DefenderMesh->SetRelativeLocation(FVector::ZeroVector);
		DefenderMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	DefenderMesh->SetHiddenInGame(false);
	DefenderMesh->SetVisibility(true, true);
}

void ADefenderBase::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World || !DefenderMesh)
	{
		return;
	}

	const FVector ActorLoc = GetActorLocation();
	const FVector TraceStart(ActorLoc.X, ActorLoc.Y, ActorLoc.Z + 2500.f);
	const FVector TraceEnd(ActorLoc.X, ActorLoc.Y, ActorLoc.Z - 5000.f);

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

	const FBox LocalBox = DefenderMesh->GetStaticMesh()
		? DefenderMesh->GetStaticMesh()->GetBoundingBox()
		: FBox(FVector::ZeroVector, FVector::ZeroVector);
	const FVector Scale = DefenderMesh->GetComponentScale();
	const float CenterX = (LocalBox.Min.X + LocalBox.Max.X) * 0.5f * Scale.X;
	const float CenterY = (LocalBox.Min.Y + LocalBox.Max.Y) * 0.5f * Scale.Y;
	const float BottomZ = LocalBox.Min.Z * Scale.Z;
	SetActorLocation(FVector(
		ActorLoc.X - CenterX,
		ActorLoc.Y - CenterY,
		Hit.ImpactPoint.Z - BottomZ + 16.f));
}

void ADefenderBase::ApplyDefenderMesh()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* ArcherMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/ArcherTowerT1.ArcherTowerT1")))
	{
		DefenderMesh->SetStaticMesh(ArcherMesh);
		DefenderMesh->SetWorldScale3D(FVector(1.f));
	}
}

void ADefenderBase::BeginPlay()
{
	Super::BeginPlay();

	ApplyDefenderMesh();
	SeatMeshOnPivot();
	SnapToGround();

	if (DefenderMesh && DefenderMesh->GetStaticMesh())
	{
		const FBox LocalBounds = DefenderMesh->GetStaticMesh()->GetBoundingBox();
		const float ScaleZ = FMath::Max(DefenderMesh->GetComponentScale().Z, KINDA_SMALL_NUMBER);
		const FVector BarOffset(
			(LocalBounds.Min.X + LocalBounds.Max.X) * 0.5f,
			(LocalBounds.Min.Y + LocalBounds.Max.Y) * 0.5f,
			LocalBounds.Max.Z + (24.f / ScaleZ));
		UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, BarOffset, FVector2D(140.f, 18.f));
	}

	CurrentHealth = MaxHealth;
	bIsDestroyed = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, HealthBarWidget->GetRelativeLocation(), HealthBarWidget->GetDrawSize());

	const float InitialDelay = FMath::FRandRange(0.f, AttackInterval);
	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ADefenderBase::ScanAndAttack, AttackInterval, true, InitialDelay);
}

void ADefenderBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	GetWorldTimerManager().ClearTimer(HitFlashTimer);
	if (HitFlashState.bActive)
	{
		EndHitFlash(HitFlashState, HitFlashMeshes, HitFlashMaterials);
	}
	Super::EndPlay(EndPlayReason);
}

void ADefenderBase::PlayHitFlash()
{
	BeginHitFlash(this, HitFlashState, HitFlashMeshes, HitFlashMaterials);
	GetWorldTimerManager().SetTimer(HitFlashTimer, this, &ADefenderBase::RestoreHitFlash, 0.12f, false);
}

void ADefenderBase::RestoreHitFlash()
{
	EndHitFlash(HitFlashState, HitFlashMeshes, HitFlashMaterials);
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

	UWorld* World = GetWorld();
	FVector ShotOrigin = GetActorLocation();
	if (DefenderMesh)
	{
		DefenderMesh->UpdateBounds();
		ShotOrigin = DefenderMesh->Bounds.Origin;
	}

	AWeatherManager* Weather = AWeatherManager::Get(this);
	if (Weather && Weather->IsWindy() && FMath::FRand() < 0.25f)
	{
		AActor* StrayTarget = nullptr;
		if (FMath::FRand() < 0.5f)
		{
			const FVector WindDir = Weather->GetWindDirection();
			float BestScore = 0.25f;

			TArray<AActor*> Enemies;
			UGameplayStatics::GetAllActorsOfClass(World, AEnemyBase::StaticClass(), Enemies);
			for (AActor* Actor : Enemies)
			{
				AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
				if (!Enemy || Enemy->IsDefeated() || Actor == Target)
				{
					continue;
				}

				const FVector ToTarget = (Enemy->GetActorLocation() - ShotOrigin).GetSafeNormal2D();
				const float DistSq = FVector::DistSquared(ShotOrigin, Enemy->GetActorLocation());
				if (DistSq > FMath::Square(AttackRange))
				{
					continue;
				}

				const float Score = FVector::DotProduct(ToTarget, WindDir);
				if (Score > BestScore)
				{
					BestScore = Score;
					StrayTarget = Enemy;
				}
			}

			TArray<AActor*> Defenders;
			UGameplayStatics::GetAllActorsOfClass(World, ADefenderBase::StaticClass(), Defenders);
			for (AActor* Actor : Defenders)
			{
				ADefenderBase* Defender = Cast<ADefenderBase>(Actor);
				if (!Defender || Defender->IsDestroyed() || Defender == this)
				{
					continue;
				}

				const FVector ToTarget = (Defender->GetActorLocation() - ShotOrigin).GetSafeNormal2D();
				const float DistSq = FVector::DistSquared(ShotOrigin, Defender->GetActorLocation());
				if (DistSq > FMath::Square(AttackRange))
				{
					continue;
				}

				const float Score = FVector::DotProduct(ToTarget, WindDir);
				if (Score > BestScore)
				{
					BestScore = Score;
					StrayTarget = Defender;
				}
			}
		}

		if (StrayTarget)
		{
			if (AEnemyBase* Enemy = Cast<AEnemyBase>(StrayTarget))
			{
				Enemy->TakeDamageFromDefender(AttackDamage);
			}
			else if (ADefenderBase* Defender = Cast<ADefenderBase>(StrayTarget))
			{
				Defender->ApplyDamage(AttackDamage);
			}

			if (World)
			{
				DrawDebugLine(World, ShotOrigin, StrayTarget->GetActorLocation(), FColor::Orange, false, AttackInterval * 0.5f, 0, 3.f);
			}
			return;
		}

		if (World)
		{
			const FVector MissEnd = ShotOrigin + Weather->GetWindDirection() * AttackRange * 0.75f + FVector(0.f, 0.f, 40.f);
			DrawDebugLine(World, ShotOrigin, MissEnd, FColor::Silver, false, AttackInterval * 0.5f, 0, 2.f);
		}
		return;
	}

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		Enemy->TakeDamageFromDefender(AttackDamage);
	}

	if (World)
	{
		DrawDebugLine(World, ShotOrigin, Target->GetActorLocation(), FColor::Red, false, AttackInterval * 0.5f, 0, 3.f);
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
		if (!Enemy || Enemy->IsDefeated() || Enemy->IsBogAlly())
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

void ADefenderBase::RestartAttackTimer()
{
	if (!GetWorld() || bIsDestroyed)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	const float Interval = FMath::Max(AttackInterval, 0.05f);
	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ADefenderBase::ScanAndAttack, Interval, true, Interval);
}

void ADefenderBase::ApplyDamage(float DamageAmount)
{
	if (bIsDestroyed || DamageAmount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	PlayHitFlash();

	if (CurrentHealth <= 0.f)
	{
		bIsDestroyed = true;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		GameVFX::SpawnNiagaraAt(GetWorld(), GameVFX::LoadDeathDustSystem(), GetActorLocation() + FVector(0.f, 0.f, 60.f));
		if (DefenderMesh)
		{
			DefenderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DefenderMesh->SetVisibility(false, true);
		}
		if (HealthBarWidget)
		{
			HealthBarWidget->SetVisibility(false);
		}
		OnDefenderDestroyed.Broadcast(this);
		SetLifeSpan(1.1f);
	}
}

FVector ADefenderBase::GetMeshFootprintAnchor() const
{
	const FVector ActorLoc = GetActorLocation();
	if (!DefenderMesh || !DefenderMesh->GetStaticMesh())
	{
		return ActorLoc;
	}

	const FBox LocalBox = DefenderMesh->GetStaticMesh()->GetBoundingBox();
	const FVector Scale = DefenderMesh->GetComponentScale();
	const FVector LocalAnchor(
		(LocalBox.Min.X + LocalBox.Max.X) * 0.5f * Scale.X,
		(LocalBox.Min.Y + LocalBox.Max.Y) * 0.5f * Scale.Y,
		LocalBox.Min.Z * Scale.Z);
	return ActorLoc + GetActorRotation().RotateVector(LocalAnchor);
}

void ADefenderBase::RefreshMeshPlacementAfterSwap(const FVector& FootprintAnchor)
{
	SeatMeshOnPivot();

	if (DefenderMesh && DefenderMesh->GetStaticMesh())
	{
		const FBox LocalBox = DefenderMesh->GetStaticMesh()->GetBoundingBox();
		const FVector Scale = DefenderMesh->GetComponentScale();
		const FVector LocalAnchor(
			(LocalBox.Min.X + LocalBox.Max.X) * 0.5f * Scale.X,
			(LocalBox.Min.Y + LocalBox.Max.Y) * 0.5f * Scale.Y,
			LocalBox.Min.Z * Scale.Z);
		SetActorLocation(FootprintAnchor - GetActorRotation().RotateVector(LocalAnchor));
	}

	if (DefenderMesh && DefenderMesh->GetStaticMesh() && HealthBarWidget)
	{
		const FBox LocalBounds = DefenderMesh->GetStaticMesh()->GetBoundingBox();
		const float ScaleZ = FMath::Max(DefenderMesh->GetComponentScale().Z, KINDA_SMALL_NUMBER);
		const FVector BarOffset(
			(LocalBounds.Min.X + LocalBounds.Max.X) * 0.5f,
			(LocalBounds.Min.Y + LocalBounds.Max.Y) * 0.5f,
			LocalBounds.Max.Z + (24.f / ScaleZ));
		UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, BarOffset, FVector2D(140.f, 18.f));
	}
}

void ADefenderBase::ApplyUpgradeVisual()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* UpgradeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/ArcherTowerT2.ArcherTowerT2")))
	{
		const FVector FootprintAnchor = GetMeshFootprintAnchor();
		DefenderMesh->SetStaticMesh(UpgradeMesh);
		DefenderMesh->SetWorldScale3D(FVector(1.f));
		RefreshMeshPlacementAfterSwap(FootprintAnchor);
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