#include "EnemyBrute.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefenderBase.h"
#include "CentralTowerBase.h"
#include "HealthBarWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyBrute::AEnemyBrute()
{
	MaxHealth = 200.f;
	AttackDamage = 32.f;
	MoveSpeed = 260.f;
	AttackRange = 160.f;
	AggroRange = 160.f;
	RewardOnDeath = 75;

	BruteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BruteMesh"));
	BruteMesh->SetupAttachment(RootComponent);
	BruteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BruteMesh->SetStaticMesh(SphereMesh.Object);
		BruteMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 1.6f));
		BruteMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	}

	if (HealthBarWidget)
	{
		UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, 140.f), FVector2D(140.f, 18.f));
	}
}

void AEnemyBrute::BeginPlay()
{
	Super::BeginPlay();

	AllowPassThroughFights();
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetHiddenInGame(true);
		CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (BruteMesh)
	{
		UMaterialInterface* BaseMat = BruteMesh->GetMaterial(0);
		if (!BaseMat)
		{
			BaseMat = BruteMesh->GetStaticMesh() ? BruteMesh->GetStaticMesh()->GetMaterial(0) : nullptr;
		}
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.08f, 0.08f));
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.08f, 0.08f));
				BruteMesh->SetMaterial(0, DynMat);
			}
		}
	}
}

bool AEnemyBrute::ShouldEngageDefenders() const
{
	return false;
}

bool AEnemyBrute::ShouldBypassFights() const
{
	return true;
}

void AEnemyBrute::UpdateMovementAndCombat(float DeltaTime)
{
	ApplyPassByDamage();

	ACentralTowerBase* Tower = FindCentralTower();
	if (IsTowerInAttackRange(Tower))
	{
		OnReachedTower(Tower);
		return;
	}

	UpdateCombatState(nullptr, false);
	FollowPath(DeltaTime);
}

void AEnemyBrute::ApplyPassByDamage()
{
	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderBase::StaticClass(), Defenders);

	const float RangeSq = FMath::Square(AttackRange);
	const FVector SelfLoc = GetActorLocation();

	for (AActor* Actor : Defenders)
	{
		ADefenderBase* Defender = Cast<ADefenderBase>(Actor);
		if (!Defender || Defender->IsDestroyed())
		{
			continue;
		}

		if (DamagedDefenders.Contains(Defender))
		{
			continue;
		}

		if (FVector::DistSquared2D(SelfLoc, Defender->GetActorLocation()) <= RangeSq)
		{
			Defender->ApplyDamage(AttackDamage);
			DamagedDefenders.Add(Defender);
			UE_LOG(LogTemp, Log, TEXT("Brute %s pass-hit Defender %s for %.1f"), *GetName(), *Defender->GetName(), AttackDamage);
		}
	}
}
