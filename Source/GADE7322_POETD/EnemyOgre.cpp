#include "EnemyOgre.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "DefenderBase.h"
#include "CentralTowerBase.h"
#include "HealthBarWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyOgre::AEnemyOgre()
{
	MaxHealth = 200.f;
	AttackDamage = 32.f;
	MoveSpeed = 260.f;
	AttackRange = 160.f;
	AggroRange = 160.f;
	RewardOnDeath = 75;

	OgreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OgreMesh"));
	OgreMesh->SetupAttachment(RootComponent);
	OgreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		OgreMesh->SetStaticMesh(CubeMesh.Object);
		OgreMesh->SetRelativeScale3D(FVector(2.4f));
		OgreMesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	}

	if (HealthBarWidget)
	{
		UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, 180.f), FVector2D(140.f, 18.f));
	}
}

void AEnemyOgre::BeginPlay()
{
	Super::BeginPlay();

	AllowPassThroughFights();
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetHiddenInGame(true);
		CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (OgreMesh)
	{
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			OgreMesh->SetStaticMesh(CubeMesh);
		}

		if (UStaticMesh* CubeAsset = OgreMesh->GetStaticMesh())
		{
			const FBox MeshBox = CubeAsset->GetBoundingBox();
			const float MeshWidth = FMath::Max(MeshBox.GetSize().X, 1.f);
			const float Scale = 240.f / MeshWidth;
			OgreMesh->SetRelativeScale3D(FVector(Scale));

			const float CapsuleHalf = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
			OgreMesh->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalf - MeshBox.Min.Z * Scale));

			if (HealthBarWidget)
			{
				const float TopZ = -CapsuleHalf + MeshBox.Max.Z * Scale;
				UHealthBarWidget::ConfigureComponent(HealthBarWidget, FVector(0.f, 0.f, TopZ + 24.f), FVector2D(140.f, 18.f));
			}
		}

		UMaterialInterface* BaseMat = OgreMesh->GetMaterial(0);
		if (!BaseMat)
		{
			BaseMat = OgreMesh->GetStaticMesh() ? OgreMesh->GetStaticMesh()->GetMaterial(0) : nullptr;
		}
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.08f, 0.08f));
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.08f, 0.08f));
				OgreMesh->SetMaterial(0, DynMat);
			}
		}
	}
}

bool AEnemyOgre::ShouldEngageDefenders() const
{
	return false;
}

bool AEnemyOgre::ShouldBypassFights() const
{
	return true;
}

void AEnemyOgre::UpdateMovementAndCombat(float DeltaTime)
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

void AEnemyOgre::ApplyPassByDamage()
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
			UE_LOG(LogTemp, Log, TEXT("Ogre %s pass-hit Defender %s for %.1f"), *GetName(), *Defender->GetName(), AttackDamage);
		}
	}
}
