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

	ADefenderBase* NearestDefender = FindNearestDefenderInRange();
	const bool bDefenderInRange = NearestDefender != nullptr;

	ACentralTowerBase* Tower = FindCentralTower();
	const bool bAtFinalWaypoint = CurrentWaypointIndex >= Waypoints.Num();
	const bool bTowerInRange = Tower && bAtFinalWaypoint &&
		FVector::Dist(GetActorLocation(), Tower->GetActorLocation()) <= AttackRange;

	const bool bShouldAttack = bDefenderInRange || bTowerInRange;

	if (bShouldAttack)
	{
		if (!bIsAttacking)
		{
			bIsAttacking = true;
			GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &AEnemyBase::ExecuteAttack, AttackInterval, true, 0.f);
		}
	}
	else
	{
		if (bIsAttacking)
		{
			bIsAttacking = false;
			GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		}
	}

	if (bAtFinalWaypoint)
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

ADefenderBase* AEnemyBase::FindNearestDefenderInRange() const
{
	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderBase::StaticClass(), Defenders);

	ADefenderBase* Nearest = nullptr;
	float NearestDistSq = FMath::Square(AttackRange);

	for (AActor* Actor : Defenders)
	{
		ADefenderBase* Defender = Cast<ADefenderBase>(Actor);
		if (!Defender || Defender->IsDestroyed())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(GetActorLocation(), Defender->GetActorLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Defender;
		}
	}

	return Nearest;
}

ACentralTowerBase* AEnemyBase::FindCentralTower() const
{
	TArray<AActor*> Towers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACentralTowerBase::StaticClass(), Towers);

	for (AActor* Actor : Towers)
	{
		ACentralTowerBase* Tower = Cast<ACentralTowerBase>(Actor);
		if (Tower && !Tower->IsDestroyed())
		{
			return Tower;
		}
	}

	return nullptr;
}

void AEnemyBase::ExecuteAttack()
{
	if (bIsDefeated)
	{
		return;
	}

	ADefenderBase* NearestDefender = FindNearestDefenderInRange();
	if (NearestDefender)
	{
		NearestDefender->ApplyDamage(AttackDamage);
		UE_LOG(LogTemp, Log, TEXT("EnemyBase %s attacked Defender %s for %.1f damage"),
			*GetName(), *NearestDefender->GetName(), AttackDamage);
		return;
	}

	ACentralTowerBase* Tower = FindCentralTower();
	if (Tower && CurrentWaypointIndex >= Waypoints.Num())
	{
		const float DistToTower = FVector::Dist(GetActorLocation(), Tower->GetActorLocation());
		if (DistToTower <= AttackRange)
		{
			Tower->ApplyDamage(AttackDamage);
			UE_LOG(LogTemp, Log, TEXT("EnemyBase %s attacked CentralTower for %.1f damage"),
				*GetName(), AttackDamage);
			return;
		}
	}

	bIsAttacking = false;
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
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