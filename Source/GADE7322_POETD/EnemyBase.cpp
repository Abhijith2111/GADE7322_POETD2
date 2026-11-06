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
#include "WorldHealthBar.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EnemyBrute.h"
#include "EnemyTrojanHorse.h"
#include "WeatherManager.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->bConstrainToPlane = false;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, 90.f), FVector2D(120.f, 16.f));
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDefeated = false;
	bIsAttacking = false;

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->MaxFlySpeed = MoveSpeed;
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, HealthBarWidget->GetRelativeLocation(), HealthBarWidget->GetDrawSize());
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

	const TCHAR* MeshPath = bCartPassenger
		? TEXT("/Game/Characters/CartEnemy_N_.CartEnemy_N_")
		: TEXT("/Game/Characters/EnemyKnight_N_.EnemyKnight_N_");
	UStaticMesh* BodyMeshAsset = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!BodyMeshAsset)
	{
		BodyMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/EnemyKnight_N_.EnemyKnight_N_"));
	}
	if (!BodyMeshAsset)
	{
		BodyMeshAsset = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!BodyMeshAsset)
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
		BodyMesh->RegisterComponent();
		MeshComponents.Add(BodyMesh);
	}

	const FBox MeshBox = BodyMeshAsset->GetBoundingBox();
	const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
	const float Uniform = 110.f / MeshHeight;
	const FVector MeshOffset(0.f, 0.f, 20.f - MeshBox.Min.Z * Uniform);

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp)
		{
			continue;
		}

		MeshComp->SetStaticMesh(BodyMeshAsset);
		MeshComp->SetRelativeScale3D(FVector(Uniform));
		MeshComp->SetRelativeLocation(MeshOffset);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (HealthBarWidget)
	{
		const float HeadZ = MeshOffset.Z + MeshBox.Max.Z * Uniform;
		UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, HeadZ + 16.f), FVector2D(120.f, 16.f));
	}
}

void AEnemyBase::PrepareCartPassenger()
{
	bCartPassenger = true;
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

float AEnemyBase::GetMeshBottomOffset() const
{
	float Lowest = 0.f;
	float LowestHeight = 0.f;
	bool bFound = false;
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Body : Meshes)
	{
		if (!Body || !Body->IsVisible() || !Body->GetStaticMesh())
		{
			continue;
		}

		const FBox Box = Body->GetStaticMesh()->GetBoundingBox();
		const float ScaleZ = FMath::Abs(Body->GetRelativeScale3D().Z);
		const float Bottom = Body->GetRelativeLocation().Z + Box.Min.Z * ScaleZ;
		if (!bFound || Bottom < Lowest)
		{
			Lowest = Bottom;
			LowestHeight = Box.GetSize().Z * ScaleZ;
			bFound = true;
		}
	}

	return bFound ? Lowest + LowestHeight * 0.5f : 0.f;
}

void AEnemyBase::KeepAboveGround()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = 0.f;
		Movement->MaxFlySpeed = MoveSpeed;
		if (Movement->MovementMode != MOVE_Flying && !bIsBeingEaten)
		{
			Movement->SetMovementMode(MOVE_Flying);
		}
	}

	if (Waypoints.Num() == 0)
	{
		return;
	}

	float PathZ = Waypoints[0].Z;
	for (const FVector& Point : Waypoints)
	{
		PathZ = FMath::Min(PathZ, Point.Z);
	}
	const float GroundZ = PathZ - 90.f;
	const FVector Loc = GetActorLocation();
	SetActorLocation(FVector(Loc.X, Loc.Y, GroundZ + 8.f - GetMeshBottomOffset()), false);
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	KeepAboveGround();

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

void AEnemyBase::ConvertToBogAlly()
{
	bIsBogAlly = true;
	bIsBeingEaten = false;
	BogPullTarget = nullptr;
	EngagedAlly = nullptr;
	bIsAttacking = false;
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = 0.f;
		Movement->MaxFlySpeed = MoveSpeed;
		Movement->SetMovementMode(MOVE_Flying);
		Movement->bRunPhysicsWithNoController = true;
	}
}

AEnemyBase* AEnemyBase::FindNearestHostile(float Range) const
{
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), Enemies);

	AEnemyBase* Nearest = nullptr;
	float NearestDistSq = FMath::Square(Range);
	for (AActor* Actor : Enemies)
	{
		AEnemyBase* Other = Cast<AEnemyBase>(Actor);
		if (!Other || Other == this || Other->IsDefeated() || Other->IsBogAlly() || Other->IsBeingEaten())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(GetActorLocation(), Other->GetActorLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Other;
		}
	}

	return Nearest;
}

AEnemyBase* AEnemyBase::FindNearestBogAlly(float Range) const
{
	if (bIsBogAlly)
	{
		return nullptr;
	}

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), Enemies);

	AEnemyBase* Nearest = nullptr;
	float NearestDistSq = FMath::Square(Range);
	for (AActor* Actor : Enemies)
	{
		AEnemyBase* Other = Cast<AEnemyBase>(Actor);
		if (!Other || Other == this || Other->IsDefeated() || !Other->IsBogAlly())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(GetActorLocation(), Other->GetActorLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Other;
		}
	}

	return Nearest;
}

bool AEnemyBase::UpdateBogAllyBehavior()
{
	if (!bIsBogAlly)
	{
		return false;
	}

	EngagedAlly = nullptr;
	AEnemyBase* Chase = FindNearestHostile(AggroRange);
	AEnemyBase* Strike = FindNearestHostile(AttackRange);
	if (Chase && !Strike)
	{
		AddMovementInput(SteerAroundTower((Chase->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()));
	}
	SetCombatActive(Strike != nullptr);
	return true;
}

void AEnemyBase::SetCombatActive(bool bActive)
{
	if (bActive)
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

void AEnemyBase::UpdateBogPull(float DeltaTime)
{
	if (!BogPullTarget.IsValid())
	{
		bIsBeingEaten = false;
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->GravityScale = 0.f;
			Movement->MaxFlySpeed = MoveSpeed;
			Movement->SetMovementMode(MOVE_Flying);
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

bool AEnemyBase::GetTowerFootprint(FVector& OutCenter, float& OutRadius) const
{
	const ACentralTowerBase* Tower = FindCentralTower();
	if (!Tower || !Tower->TowerMesh)
	{
		return false;
	}

	const FBoxSphereBounds Bounds = Tower->TowerMesh->Bounds;
	OutCenter = Bounds.Origin;
	const float BodyRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.f;
	OutRadius = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) + BodyRadius + 30.f;
	return true;
}

FVector AEnemyBase::SteerAroundTower(const FVector& DesiredDir) const
{
	if (DesiredDir.IsNearlyZero())
	{
		return DesiredDir;
	}

	FVector Center;
	float Radius = 0.f;
	if (!GetTowerFootprint(Center, Radius))
	{
		return DesiredDir;
	}

	const FVector MyLoc = GetActorLocation();
	const FVector FromCenter = MyLoc - Center;
	const float Dist = FromCenter.Size2D();
	const FVector Away = FromCenter.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Away.IsNearlyZero() ? FVector::ForwardVector : Away);
	const float SideSign = FVector::DotProduct(Side, DesiredDir) >= 0.f ? 1.f : -1.f;
	const FVector Tangent = (Side * SideSign).GetSafeNormal2D();

	if (Dist < Radius)
	{
		const FVector PushOut = Away.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Away;
		return (Tangent * 0.55f + PushOut).GetSafeNormal2D();
	}

	if (FVector::Dist2D(MyLoc + DesiredDir * 140.f, Center) < Radius)
	{
		return Tangent;
	}

	return DesiredDir;
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
	if (UpdateBogAllyBehavior())
	{
		return;
	}

	ACentralTowerBase* Tower = FindCentralTower();
	const bool bTowerInRange = IsTowerInAttackRange(Tower);

	if (ShouldEngageDefenders())
	{
		AEnemyBase* AllyAggro = FindNearestBogAlly(AggroRange);
		AEnemyBase* AllyAttack = FindNearestBogAlly(AttackRange);
		ADefenderBase* AggroDefender = FindNearestDefender(AggroRange);
		ADefenderBase* AttackDefender = FindNearestDefender(AttackRange);
		const float AllyDist = AllyAggro ? FVector::DistSquared2D(GetActorLocation(), AllyAggro->GetActorLocation()) : BIG_NUMBER;
		const float DefenderDist = AggroDefender ? FVector::DistSquared2D(GetActorLocation(), AggroDefender->GetActorLocation()) : BIG_NUMBER;

		if (AllyAggro && AllyDist <= DefenderDist)
		{
			EngagedAlly = AllyAggro;
			DrawDebugLine(GetWorld(), GetActorLocation(), AllyAggro->GetActorLocation(), FColor::Orange, false, 0.1f, 0, 3.f);
			SetCombatActive(AllyAttack != nullptr);
			if (!AllyAttack)
			{
				AddMovementInput(SteerAroundTower((AllyAggro->GetActorLocation() - GetActorLocation()).GetSafeNormal2D()));
			}
			return;
		}

		EngagedAlly = nullptr;
		if (AggroDefender)
		{
			DrawDebugLine(GetWorld(), GetActorLocation(), AggroDefender->GetActorLocation(), FColor::Orange, false, 0.1f, 0, 3.f);
			UpdateCombatState(AttackDefender, false);

			if (!AttackDefender)
			{
				const FVector ToDefender = AggroDefender->GetActorLocation() - GetActorLocation();
				AddMovementInput(SteerAroundTower(ToDefender.GetSafeNormal2D()));
			}
			return;
		}
	}
	else
	{
		EngagedAlly = nullptr;
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

	FVector Center;
	float Radius = 0.f;
	if (!GetTowerFootprint(Center, Radius))
	{
		return;
	}

	if (FVector::Dist2D(GetActorLocation(), Center) < Radius)
	{
		const FVector Outward = (GetActorLocation() - Center).GetSafeNormal2D();
		AddMovementInput(Outward.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Outward);
	}
}

void AEnemyBase::FollowPath(float DeltaTime)
{
	ACentralTowerBase* Tower = FindCentralTower();

	FVector TowerCenter;
	float TowerRadius = 0.f;
	if (GetTowerFootprint(TowerCenter, TowerRadius))
	{
		while (CurrentWaypointIndex < Waypoints.Num() && FVector::Dist2D(Waypoints[CurrentWaypointIndex], TowerCenter) < TowerRadius)
		{
			++CurrentWaypointIndex;
		}
	}

	if (CurrentWaypointIndex >= Waypoints.Num())
	{
		if (Tower)
		{
			FVector MoveDir = (Tower->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			MoveDir = SteerAroundTower(MoveDir);
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

	FVector MoveDir = SteerAroundTower(ToWaypoint.GetSafeNormal2D());
	if (ShouldBypassFights())
	{
		MoveDir = SteerAroundFights(MoveDir);
	}
	AddMovementInput(MoveDir);
	DrawDebugSphere(GetWorld(), TargetWaypoint, 20.f, 6, FColor::Yellow, false, 0.05f);
}

void AEnemyBase::UpdateCombatState(ADefenderBase* AttackTarget, bool bCanAttackTower)
{
	SetCombatActive(AttackTarget != nullptr || bCanAttackTower);
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

	if (bIsBogAlly)
	{
		if (AEnemyBase* Hostile = FindNearestHostile(AttackRange))
		{
			Hostile->TakeDamageFromDefender(AttackDamage);
		}
		return;
	}

	if (AEnemyBase* Ally = EngagedAlly.Get())
	{
		if (!Ally->IsDefeated() && FVector::DistSquared2D(GetActorLocation(), Ally->GetActorLocation()) <= FMath::Square(AttackRange))
		{
			Ally->TakeDamageFromDefender(AttackDamage);
			return;
		}
	}

	if (ShouldEngageDefenders())
	{
		if (const AWeatherManager* Weather = AWeatherManager::Get(this))
		{
			if (Weather->IsNight() && FMath::FRand() < 0.15f)
			{
				AEnemyBase* ConfusedTarget = nullptr;
				float NearestDistSq = FMath::Square(FMath::Max(AttackRange, 250.f));
				TArray<AActor*> Enemies;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), Enemies);
				for (AActor* Actor : Enemies)
				{
					AEnemyBase* Other = Cast<AEnemyBase>(Actor);
					if (!Other || Other == this || Other->IsDefeated() || Other->IsBogAlly())
					{
						continue;
					}

					const float DistSq = FVector::DistSquared(GetActorLocation(), Other->GetActorLocation());
					if (DistSq <= NearestDistSq)
					{
						NearestDistSq = DistSq;
						ConfusedTarget = Other;
					}
				}

				if (ConfusedTarget)
				{
					ConfusedTarget->TakeDamageFromDefender(AttackDamage);
					UE_LOG(LogTemp, Log, TEXT("EnemyBase %s confused and attacked %s for %.1f damage"),
						*GetName(), *ConfusedTarget->GetName(), AttackDamage);
					return;
				}
			}
		}

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
