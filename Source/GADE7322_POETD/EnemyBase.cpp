#include "EnemyBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "DefenderBase.h"
#include "DefenderKnight.h"
#include "DefenderMineShaft.h"
#include "TDGameInstance.h"
#include "CentralTowerBase.h"
#include "TDGameState.h"
#include "HealthBarWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EnemyBrute.h"
#include "EnemyTrojanHorse.h"

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

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, 90.f), FVector2D(120.f, 16.f));
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
	UHealthBarWidget::BindToWidgetComponent(HealthBarWidget, this);
	ApplyGruntCube();
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	GetWorldTimerManager().ClearTimer(HitFlashTimer);
	if (HitFlashState.bActive)
	{
		EndHitFlash(HitFlashState, HitFlashMeshes, HitFlashMaterials);
	}
	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::ApplyGruntCube()
{
	if (IsA(AEnemyBrute::StaticClass()) || IsA(AEnemyTrojanHorse::StaticClass()))
	{
		return;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		return;
	}

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetHiddenInGame(true);
		CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);
	if (MeshComponents.Num() == 0)
	{
		UStaticMeshComponent* BodyMesh = NewObject<UStaticMeshComponent>(this, TEXT("BodyMesh"));
		BodyMesh->SetupAttachment(GetRootComponent());
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BodyMesh->SetRelativeScale3D(FVector(0.7f));
		BodyMesh->RegisterComponent();
		MeshComponents.Add(BodyMesh);
	}

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetStaticMesh(CubeMesh);
		}
	}
}

void AEnemyBase::PlayHitFlash()
{
	BeginHitFlash(this, HitFlashState, HitFlashMeshes, HitFlashMaterials);
	GetWorldTimerManager().SetTimer(HitFlashTimer, this, &AEnemyBase::RestoreHitFlash, 0.12f, false);
}

void AEnemyBase::RestoreHitFlash()
{
	EndHitFlash(HitFlashState, HitFlashMeshes, HitFlashMaterials);
}

void AEnemyBase::InitialiseWithWaypoints(const TArray<FVector>& InWaypoints)
{
	Waypoints = InWaypoints;
	CurrentWaypointIndex = 0;
	bWaypointsInitialised = true;

	UE_LOG(LogTemp, Log, TEXT("EnemyBase %s initialised with %d waypoints"), *GetName(), Waypoints.Num());
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDefeated || !bWaypointsInitialised)
	{
		return;
	}

	if (bIsBeingEaten)
	{
		UpdateBogPull(DeltaTime);
		return;
	}

	UpdateMovementAndCombat(DeltaTime);
}

void AEnemyBase::BeginBogPull(AActor* Bog)
{
	if (bIsDefeated || !Bog)
	{
		return;
	}

	bIsBeingEaten = true;
	BogPullTarget = Bog;
	bIsAttacking = false;
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
}

bool AEnemyBase::IsBeingEaten() const
{
	return bIsBeingEaten;
}

void AEnemyBase::UpdateBogPull(float DeltaTime)
{
	if (!BogPullTarget.IsValid())
	{
		bIsBeingEaten = false;
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
			Movement->MaxWalkSpeed = MoveSpeed;
		}
		return;
	}

	const FVector Current = GetActorLocation();
	const FVector ToBog = BogPullTarget->GetActorLocation() - Current;
	const float Dist = ToBog.Size2D();
	if (Dist <= 70.f)
	{
		return;
	}

	const float Step = FMath::Max(MoveSpeed, 200.f) * 3.f * DeltaTime;
	const FVector Next = Current + ToBog.GetSafeNormal2D() * FMath::Min(Step, Dist);
	SetActorLocation(FVector(Next.X, Next.Y, Current.Z), false);
}

bool AEnemyBase::IsEngagedInFight() const
{
	return bIsAttacking && !bIsDefeated && !bIsBeingEaten;
}

bool AEnemyBase::ShouldEngageDefenders() const
{
	return true;
}

bool AEnemyBase::ShouldBypassFights() const
{
	return false;
}

void AEnemyBase::AllowPassThroughFights()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}
}

FVector AEnemyBase::SteerAroundFights(const FVector& DesiredDir) const
{
	if (DesiredDir.IsNearlyZero())
	{
		return DesiredDir;
	}

	TArray<AActor*> Others;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), Others);

	FVector Avoid = FVector::ZeroVector;
	const FVector MyLoc = GetActorLocation();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, DesiredDir).GetSafeNormal();
	constexpr float AvoidRadius = 320.f;

	for (AActor* Actor : Others)
	{
		const AEnemyBase* Other = Cast<AEnemyBase>(Actor);
		if (!Other || Other == this || !Other->IsEngagedInFight())
		{
			continue;
		}

		const FVector FromOther = MyLoc - Other->GetActorLocation();
		const float Dist = FromOther.Size2D();
		if (Dist > AvoidRadius || Dist < 1.f)
		{
			continue;
		}

		const FVector AwayDir = FromOther.GetSafeNormal2D();
		if (FVector::DotProduct(DesiredDir, (Other->GetActorLocation() - MyLoc).GetSafeNormal2D()) < 0.f)
		{
			continue;
		}

		const float SideSign = FVector::DotProduct(Side, AwayDir) >= 0.f ? 1.f : -1.f;
		Avoid += Side * SideSign * (1.f - Dist / AvoidRadius);
	}

	if (Avoid.IsNearlyZero())
	{
		return DesiredDir;
	}

	return (DesiredDir + Avoid * 2.4f).GetSafeNormal2D();
}

void AEnemyBase::UpdateMovementAndCombat(float DeltaTime)
{
	ACentralTowerBase* Tower = FindCentralTower();
	const bool bTowerInRange = IsTowerInAttackRange(Tower);

	if (ShouldEngageDefenders())
	{
		ADefenderBase* AggroDefender = FindNearestDefender(AggroRange);
		ADefenderBase* AttackDefender = FindNearestDefender(AttackRange);

		if (AggroDefender)
		{
			DrawDebugLine(GetWorld(), GetActorLocation(), AggroDefender->GetActorLocation(), FColor::Orange, false, 0.1f, 0, 3.f);
			UpdateCombatState(AttackDefender, false);

			if (!AttackDefender)
			{
				const FVector ToDefender = AggroDefender->GetActorLocation() - GetActorLocation();
				AddMovementInput(ToDefender.GetSafeNormal2D());
			}
			return;
		}
	}

	if (bTowerInRange)
	{
		OnReachedTower(Tower);
		return;
	}

	UpdateCombatState(nullptr, false);
	FollowPath(DeltaTime);
}

void AEnemyBase::OnReachedTower(ACentralTowerBase* Tower)
{
	UpdateCombatState(nullptr, true);

	if (Tower && CurrentWaypointIndex >= Waypoints.Num())
	{
		AddMovementInput((Tower->GetActorLocation() - GetActorLocation()).GetSafeNormal2D());
	}
}

void AEnemyBase::FollowPath(float DeltaTime)
{
	ACentralTowerBase* Tower = FindCentralTower();

	if (CurrentWaypointIndex >= Waypoints.Num())
	{
		if (Tower)
		{
			FVector MoveDir = (Tower->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			if (ShouldBypassFights())
			{
				MoveDir = SteerAroundFights(MoveDir);
			}
			AddMovementInput(MoveDir);
		}
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
			if (Tower && IsTowerInAttackRange(Tower))
			{
				OnReachedTower(Tower);
			}
		}
		return;
	}

	FVector MoveDir = ToWaypoint.GetSafeNormal2D();
	if (ShouldBypassFights())
	{
		MoveDir = SteerAroundFights(MoveDir);
	}
	AddMovementInput(MoveDir);
	DrawDebugSphere(GetWorld(), TargetWaypoint, 20.f, 6, FColor::Yellow, false, 0.05f);
}

void AEnemyBase::UpdateCombatState(ADefenderBase* AttackTarget, bool bCanAttackTower)
{
	const bool bShouldAttack = AttackTarget != nullptr || bCanAttackTower;
	if (bShouldAttack)
	{
		if (!bIsAttacking)
		{
			bIsAttacking = true;
			GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &AEnemyBase::ExecuteAttack, AttackInterval, true, 0.f);
		}
	}
	else if (bIsAttacking)
	{
		bIsAttacking = false;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	}
}

ADefenderBase* AEnemyBase::FindNearestDefender(float Range) const
{
	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderBase::StaticClass(), Defenders);

	ADefenderBase* Nearest = nullptr;
	ADefenderKnight* NearestKnight = nullptr;
	ADefenderMineShaft* NearestMine = nullptr;
	float NearestDistSq = FMath::Square(Range);
	float NearestKnightDistSq = NearestDistSq;
	float NearestMineDistSq = NearestDistSq;

	const UTDGameInstance* TDInstance = Cast<UTDGameInstance>(GetGameInstance());
	const bool bHuntMine = TDInstance && TDInstance->GetDifficulty() == ETDDifficulty::Hard;

	for (AActor* Actor : Defenders)
	{
		ADefenderBase* Defender = Cast<ADefenderBase>(Actor);
		if (!Defender || Defender->IsDestroyed())
		{
			continue;
		}

		const FVector SelfLoc = FVector(GetActorLocation().X, GetActorLocation().Y, 0.f);
		const FVector DefLoc = FVector(Defender->GetActorLocation().X, Defender->GetActorLocation().Y, 0.f);
		const float DistSq = FVector::DistSquared(SelfLoc, DefLoc);
		if (DistSq > NearestDistSq && DistSq > NearestKnightDistSq && DistSq > NearestMineDistSq)
		{
			continue;
		}

		if (bHuntMine)
		{
			if (ADefenderMineShaft* Mine = Cast<ADefenderMineShaft>(Defender))
			{
				if (DistSq <= NearestMineDistSq)
				{
					NearestMineDistSq = DistSq;
					NearestMine = Mine;
				}
				continue;
			}
		}

		if (ADefenderKnight* Knight = Cast<ADefenderKnight>(Defender))
		{
			if (DistSq <= NearestKnightDistSq)
			{
				NearestKnightDistSq = DistSq;
				NearestKnight = Knight;
			}
		}
		else if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Defender;
		}
	}

	if (bHuntMine && NearestMine)
	{
		return NearestMine;
	}

	if (ShouldEngageDefenders() && NearestKnight)
	{
		return NearestKnight;
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

bool AEnemyBase::IsTowerInAttackRange(const ACentralTowerBase* Tower) const
{
	if (!Tower || Tower->IsDestroyed())
	{
		return false;
	}

	float Range = FMath::Max(AttackRange, 0.f);
	if (Tower->TowerMesh)
	{
		const FVector Extent = Tower->TowerMesh->Bounds.BoxExtent;
		Range += FMath::Max(Extent.X, Extent.Y);
	}

	return FVector::DistSquared2D(GetActorLocation(), Tower->GetActorLocation()) <= FMath::Square(Range);
}

void AEnemyBase::ExecuteAttack()
{
	if (bIsDefeated)
	{
		return;
	}

	if (ShouldEngageDefenders())
	{
		ADefenderBase* NearestDefender = FindNearestDefender(AttackRange);
		if (NearestDefender)
		{
			NearestDefender->ApplyDamage(AttackDamage);
			UE_LOG(LogTemp, Log, TEXT("EnemyBase %s attacked Defender %s for %.1f damage"),
				*GetName(), *NearestDefender->GetName(), AttackDamage);
			return;
		}
	}

	ACentralTowerBase* Tower = FindCentralTower();
	if (IsTowerInAttackRange(Tower))
	{
		Tower->ApplyDamage(AttackDamage);
		UE_LOG(LogTemp, Log, TEXT("EnemyBase %s attacked CentralTower for %.1f damage"),
			*GetName(), AttackDamage);
		return;
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
	PlayHitFlash();

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

TArray<FVector> AEnemyBase::GetRemainingWaypoints() const
{
	TArray<FVector> Remaining;
	if (CurrentWaypointIndex < Waypoints.Num())
	{
		for (int32 i = CurrentWaypointIndex; i < Waypoints.Num(); ++i)
		{
			Remaining.Add(Waypoints[i]);
		}
	}
	return Remaining;
}

bool AEnemyBase::IsDefeated() const
{
	return bIsDefeated;
}

float AEnemyBase::GetDisplayHealthPercent_Implementation() const
{
	return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
}

bool AEnemyBase::IsUnitDestroyed_Implementation() const
{
	return bIsDefeated;
}
