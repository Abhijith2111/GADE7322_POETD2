#include "EnemyBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "DefenderBase.h"
#include "CentralTowerBase.h"
#include "TDGameState.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	GetCharacterMovement()->DefaultLandMovementMode = MOVE_Walking;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->GravityScale = 1.f;
	GetCharacterMovement()->bConstrainToPlane = false;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDefeated = false;
	bIsAttacking = false;

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::InitialiseWithWaypoints(const TArray<FVector>& InWaypoints)
{
	Waypoints = InWaypoints;
	CurrentWaypointIndex = 0;
	bWaypointsInitialised = Waypoints.Num() > 0;

	UE_LOG(LogTemp, Log, TEXT("EnemyBase %s initialised with %d waypoints"), *GetName(), Waypoints.Num());
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDefeated || !bWaypointsInitialised)
	{
		return;
	}

	AActor* Target = FindNearestAttackTarget();

	if (Target)
	{
		const float DistToTarget = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
		if (DistToTarget <= AttackRange)
		{
			if (!bIsAttacking)
			{
				bIsAttacking = true;
				GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &AEnemyBase::ExecuteAttack, AttackInterval, true, 0.f);
			}
			return;
		}

		const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		AddMovementInput(Direction);

		DrawDebugLine(GetWorld(), GetActorLocation(), Target->GetActorLocation(), FColor::Orange, false, 0.05f, 0, 2.f);
		return;
	}

	if (bIsAttacking)
	{
		bIsAttacking = false;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	}

	if (CurrentWaypointIndex >= Waypoints.Num())
	{
		return;
	}

	const FVector& TargetWaypoint = Waypoints[CurrentWaypointIndex];
	const FVector ToWaypoint = TargetWaypoint - GetActorLocation();
	const float DistToWaypoint = ToWaypoint.Size2D();

	if (DistToWaypoint <= WaypointAcceptanceRadius)
	{
		++CurrentWaypointIndex;

		if (CurrentWaypointIndex >= Waypoints.Num())
		{
			UE_LOG(LogTemp, Log, TEXT("EnemyBase %s reached final waypoint (central tower)."), *GetName());
		}
		return;
	}

	const FVector MoveDir = ToWaypoint.GetSafeNormal2D();
	AddMovementInput(MoveDir);

	DrawDebugSphere(GetWorld(), TargetWaypoint, 20.f, 6, FColor::Yellow, false, 0.05f);
}

AActor* AEnemyBase::FindNearestAttackTarget() const
{
	AActor* BestTarget = nullptr;
	float BestDistSq = FMath::Square(AttackRange * 3.f);

	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderBase::StaticClass(), Defenders);
	for (AActor* Actor : Defenders)
	{
		ADefenderBase* Defender = Cast<ADefenderBase>(Actor);
		if (!Defender || Defender->IsDestroyed())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Defender->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestTarget = Defender;
		}
	}

	if (BestTarget)
	{
		return BestTarget;
	}

	TArray<AActor*> Towers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACentralTowerBase::StaticClass(), Towers);
	for (AActor* Actor : Towers)
	{
		ACentralTowerBase* Tower = Cast<ACentralTowerBase>(Actor);
		if (!Tower || Tower->IsDestroyed())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Tower->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestTarget = Tower;
		}
	}

	return BestTarget;
}

void AEnemyBase::ExecuteAttack()
{
	if (bIsDefeated)
	{
		return;
	}

	AActor* Target = FindNearestAttackTarget();
	if (!Target)
	{
		bIsAttacking = false;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		return;
	}

	const float DistToTarget = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (DistToTarget > AttackRange)
	{
		bIsAttacking = false;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		return;
	}

	ADefenderBase* Defender = Cast<ADefenderBase>(Target);
	if (Defender)
	{
		Defender->ApplyDamage(AttackDamage);
		UE_LOG(LogTemp, Log, TEXT("EnemyBase %s attacked Defender %s for %.1f damage"), *GetName(), *Defender->GetName(), AttackDamage);
		return;
	}

	ACentralTowerBase* Tower = Cast<ACentralTowerBase>(Target);
	if (Tower)
	{
		Tower->ApplyDamage(AttackDamage);
		UE_LOG(LogTemp, Log, TEXT("EnemyBase %s attacked CentralTower %s for %.1f damage"), *GetName(), *Tower->GetName(), AttackDamage);
	}
}

void AEnemyBase::TakeDamageFromDefender(float DamageAmount)
{
	if (bIsDefeated || DamageAmount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	UE_LOG(LogTemp, Log, TEXT("EnemyBase %s took %.1f damage, %.1f/%.1f HP remaining"), *GetName(), DamageAmount, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		HandleDeath();
	}
}

void AEnemyBase::HandleDeath()
{
	bIsDefeated = true;
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	ATDGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr;
	if (GS)
	{
		GS->AddMoney(RewardOnDeath);
		UE_LOG(LogTemp, Log, TEXT("EnemyBase %s destroyed. %d gold granted via TDGameState. Total: %d"),
			*GetName(), RewardOnDeath, GS->GetCurrentMoney());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyBase %s destroyed. Reward: %d gold. (TDGameState not found - running without economy.)"),
			*GetName(), RewardOnDeath);
	}

	OnEnemyDestroyed.Broadcast(RewardOnDeath);
	SetLifeSpan(0.1f);
}

bool AEnemyBase::IsDefeated() const
{
	return bIsDefeated;
}