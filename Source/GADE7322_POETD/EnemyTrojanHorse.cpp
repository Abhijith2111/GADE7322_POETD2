#include "EnemyTrojanHorse.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "CentralTowerBase.h"
#include "HealthBarWidget.h"
#include "TDGameState.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyTrojanHorse::AEnemyTrojanHorse()
{
	MaxHealth = 120.f;
	AttackDamage = 0.f;
	MoveSpeed = 220.f;
	AttackRange = 140.f;
	AggroRange = 0.f;
	RewardOnDeath = 60;
	PassengerCount = 5;

	CarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarrierMesh"));
	CarrierMesh->SetupAttachment(RootComponent);
	CarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		CarrierMesh->SetStaticMesh(CubeMesh.Object);
		CarrierMesh->SetRelativeScale3D(FVector(2.2f, 1.4f, 1.5f));
		CarrierMesh->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	}

	static ConstructorHelpers::FClassFinder<AEnemyBase> NormalEnemyBP(TEXT("/Game/Gameplay/Enemies/Blueprints/BP_EnemyBase"));
	if (NormalEnemyBP.Succeeded())
	{
		PassengerClass = NormalEnemyBP.Class;
	}
	else
	{
		PassengerClass = AEnemyBase::StaticClass();
	}

	if (HealthBarWidget)
	{
		UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, 160.f), FVector2D(160.f, 20.f));
	}
}

void AEnemyTrojanHorse::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	bHasBurst = false;

	if (!PassengerClass)
	{
		PassengerClass = AEnemyBase::StaticClass();
	}

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetHiddenInGame(true);
		CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (CarrierMesh)
	{
		UMaterialInterface* BaseMat = CarrierMesh->GetMaterial(0);
		if (!BaseMat)
		{
			BaseMat = CarrierMesh->GetStaticMesh() ? CarrierMesh->GetStaticMesh()->GetMaterial(0) : nullptr;
		}
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.28f, 0.12f));
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.45f, 0.28f, 0.12f));
				CarrierMesh->SetMaterial(0, DynMat);
			}
		}
	}
}

bool AEnemyTrojanHorse::ShouldEngageDefenders() const
{
	return false;
}

void AEnemyTrojanHorse::UpdateMovementAndCombat(float DeltaTime)
{
	ACentralTowerBase* Tower = FindCentralTower();
	if (IsTowerInAttackRange(Tower) || CurrentWaypointIndex >= Waypoints.Num())
	{
		OnReachedTower(Tower);
		return;
	}

	UpdateCombatState(nullptr, false);
	FollowPath(DeltaTime);
}

void AEnemyTrojanHorse::OnReachedTower(ACentralTowerBase* /*Tower*/)
{
	Burst();
}

void AEnemyTrojanHorse::HandleDeath()
{
	Burst();
}

void AEnemyTrojanHorse::Burst()
{
	if (bHasBurst || !GetWorld())
	{
		return;
	}

	bHasBurst = true;
	bIsDefeated = true;
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	TSubclassOf<AEnemyBase> ClassToSpawn = PassengerClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AEnemyBase::StaticClass();
	}

	const TArray<FVector> Remaining = GetRemainingWaypoints();
	const FVector Origin = GetActorLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	for (int32 i = 0; i < PassengerCount; ++i)
	{
		const float Angle = (2.f * PI * static_cast<float>(i)) / static_cast<float>(FMath::Max(PassengerCount, 1));
		const FVector Offset(FMath::Cos(Angle) * 80.f, FMath::Sin(Angle) * 80.f, 0.f);
		const FVector SpawnLoc = Origin + Offset;

		AEnemyBase* Passenger = GetWorld()->SpawnActor<AEnemyBase>(ClassToSpawn, SpawnLoc, GetActorRotation(), SpawnParams);
		if (!Passenger)
		{
			continue;
		}

		Passenger->InitialiseWithWaypoints(Remaining);

		UE_LOG(LogTemp, Log, TEXT("Trojan %s burst passenger %s"), *GetName(), *Passenger->GetName());
	}

	ATDGameState* GS = GetWorld()->GetGameState<ATDGameState>();
	if (GS)
	{
		GS->AddMoney(RewardOnDeath);
	}

	OnEnemyDestroyed.Broadcast(RewardOnDeath);
	SetLifeSpan(0.1f);
}
