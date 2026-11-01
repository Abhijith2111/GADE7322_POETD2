#include "EnemyTrojanHorse.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "CentralTowerBase.h"
#include "WorldHealthBar.h"
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CartMesh(TEXT("/Game/Buildings/Cart.Cart"));
	if (CartMesh.Succeeded())
	{
		CarrierMesh->SetStaticMesh(CartMesh.Object);
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
		UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, 160.f), FVector2D(160.f, 20.f));
	}
}

void AEnemyTrojanHorse::BeginPlay()
{
	Super::BeginPlay();

	AllowPassThroughFights();
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
		if (UStaticMesh* CartMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/Cart.Cart")))
		{
			CarrierMesh->SetStaticMesh(CartMesh);
		}

		if (UStaticMesh* CartAsset = CarrierMesh->GetStaticMesh())
		{
			const FBox MeshBox = CartAsset->GetBoundingBox();
			const float MeshWidth = FMath::Max(FMath::Max(MeshBox.GetSize().X, MeshBox.GetSize().Y), 1.f);
			const float Scale = 220.f / MeshWidth;
			CarrierMesh->SetRelativeScale3D(FVector(Scale));
			CarrierMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

			const float CapsuleHalf = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
			CarrierMesh->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalf - MeshBox.Min.Z * Scale));

			if (HealthBarWidget)
			{
				const float TopZ = -CapsuleHalf + MeshBox.Max.Z * Scale;
				UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, TopZ + 24.f), FVector2D(160.f, 20.f));
			}
		}
	}
}

bool AEnemyTrojanHorse::ShouldEngageDefenders() const
{
	return false;
}

bool AEnemyTrojanHorse::ShouldBypassFights() const
{
	return true;
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

void AEnemyTrojanHorse::OnReachedTower(ACentralTowerBase*)
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
