#include "EnemyBrute.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefenderBase.h"
#include "CentralTowerBase.h"
#include "WorldHealthBar.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TDGameInstance.h"

AEnemyBrute::AEnemyBrute()
{
	MaxHealth = 250.f;
	AttackDamage = 32.f;
	MoveSpeed = 260.f;
	AttackRange = 160.f;
	AggroRange = 160.f;
	RewardOnDeath = 75;

	BruteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BruteMesh"));
	BruteMesh->SetupAttachment(RootComponent);
	BruteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TankMesh(TEXT("/Game/Characters/Brute_Tank_.Brute_Tank_"));
	if (TankMesh.Succeeded())
	{
		BruteMesh->SetStaticMesh(TankMesh.Object);
		const FBox MeshBox = TankMesh.Object->GetBoundingBox();
		const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
		const float Uniform = 170.f / MeshHeight;
		BruteMesh->SetRelativeScale3D(FVector(Uniform));
		BruteMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f - MeshBox.Min.Z * Uniform + MeshHeight * Uniform + 6.f));
	}

	if (HealthBarWidget)
	{
		UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, 140.f), FVector2D(140.f, 18.f));
	}
}

void AEnemyBrute::BeginPlay()
{
	Super::BeginPlay();

	if (const UTDGameInstance* GI = Cast<UTDGameInstance>(GetGameInstance()))
	{
		if (GI->GetDifficulty() == ETDDifficulty::Hard)
		{
			AttackDamage = 40.f;
		}
	}

	AllowPassThroughFights();
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetHiddenInGame(true);
		CharMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (BruteMesh)
	{
		if (UStaticMesh* TankMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Characters/Brute_Tank_.Brute_Tank_")))
		{
			BruteMesh->SetStaticMesh(TankMesh);
			const FBox MeshBox = TankMesh->GetBoundingBox();
			const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
			const float Uniform = 170.f / MeshHeight;
			const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
			const FVector MeshOffset(0.f, 0.f, -HalfHeight - MeshBox.Min.Z * Uniform + MeshHeight * Uniform + 6.f);
			BruteMesh->SetRelativeScale3D(FVector(Uniform));
			BruteMesh->SetRelativeLocation(MeshOffset);
			if (HealthBarWidget)
			{
				const float HeadZ = MeshOffset.Z + MeshBox.Max.Z * Uniform;
				UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, HeadZ + 18.f), FVector2D(140.f, 18.f));
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
	if (UpdateBogAllyBehavior())
	{
		return;
	}

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

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), Enemies);
	for (AActor* Actor : Enemies)
	{
		AEnemyBase* Ally = Cast<AEnemyBase>(Actor);
		if (!Ally || Ally == this || !Ally->IsBogAlly() || Ally->IsDefeated() || DamagedAllies.Contains(Ally))
		{
			continue;
		}

		if (FVector::DistSquared2D(SelfLoc, Ally->GetActorLocation()) <= RangeSq)
		{
			Ally->TakeDamageFromDefender(AttackDamage);
			DamagedAllies.Add(Ally);
		}
	}
}
