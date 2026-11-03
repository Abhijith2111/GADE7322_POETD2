#include "CentralTowerBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnemyBase.h"
#include "WorldHealthBar.h"
#include "TDGameState.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameVFX.h"

ACentralTowerBase::ACentralTowerBase()
{
	PrimaryActorTick.bCanEverTick = false;

	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	RootComponent = TowerMesh;

	TempleStrainPP = CreateDefaultSubobject<UPostProcessComponent>(TEXT("TempleStrainPP"));
	TempleStrainPP->SetupAttachment(RootComponent);
	TempleStrainPP->bUnbound = true;
	TempleStrainPP->bEnabled = true;
	TempleStrainPP->BlendWeight = 0.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TempleMeshAsset(TEXT("/Game/Buildings/Temple.Temple"));
	if (TempleMeshAsset.Succeeded())
	{
		TowerMesh->SetStaticMesh(TempleMeshAsset.Object);
		TowerMesh->SetWorldScale3D(FVector(1.f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (CubeMeshAsset.Succeeded())
		{
			TowerMesh->SetStaticMesh(CubeMeshAsset.Object);
			TowerMesh->SetWorldScale3D(FVector(2.f, 2.f, 4.f));
		}
	}

	TowerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TowerMesh->SetCollisionProfileName(TEXT("BlockAll"));

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
	UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, 320.f), FVector2D(220.f, 28.f));
}

void ACentralTowerBase::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World || !TowerMesh)
	{
		return;
	}

	const FVector ActorLoc = GetActorLocation();
	const FVector TraceStart(ActorLoc.X, ActorLoc.Y, ActorLoc.Z + 2500.f);
	const FVector TraceEnd(ActorLoc.X, ActorLoc.Y, ActorLoc.Z - 5000.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CentralTowerGroundSnap), true, this);
	Params.AddIgnoredActor(this);

	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)
		&& !World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		return;
	}

	const FBox LocalBox = TowerMesh->GetStaticMesh()
		? TowerMesh->GetStaticMesh()->GetBoundingBox()
		: FBox(FVector::ZeroVector, FVector::ZeroVector);
	const float BottomZ = LocalBox.Min.Z * TowerMesh->GetComponentScale().Z;
	SetActorLocation(FVector(ActorLoc.X, ActorLoc.Y, Hit.ImpactPoint.Z - BottomZ + 16.f));

	if (TowerMesh->GetStaticMesh())
	{
		const FBox LocalBounds = TowerMesh->GetStaticMesh()->GetBoundingBox();
		const float TopZ = LocalBounds.Max.Z * TowerMesh->GetComponentScale().Z;
		UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, FVector(0.f, 0.f, TopZ + 40.f), FVector2D(220.f, 28.f));
	}
}

void ACentralTowerBase::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* TempleMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/Temple.Temple")))
	{
		TowerMesh->SetStaticMesh(TempleMesh);
		TowerMesh->SetWorldScale3D(FVector(1.f));
	}

	SnapToGround();

	CurrentHealth = MaxHealth;
	bIsDestroyed = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	UWorldHealthBarLibrary::ConfigureWorldHealthBar(HealthBarWidget, HealthBarWidget->GetRelativeLocation(), HealthBarWidget->GetDrawSize());

	if (UMaterialInterface* StrainMat = GameVFX::LoadTempleStrainMaterial())
	{
		TempleStrainMID = UMaterialInstanceDynamic::Create(StrainMat, this);
		if (TempleStrainMID && TempleStrainPP)
		{
			TempleStrainPP->Settings.WeightedBlendables.Array.Reset();
			FWeightedBlendable Blendable;
			Blendable.Object = TempleStrainMID;
			Blendable.Weight = 1.f;
			TempleStrainPP->Settings.WeightedBlendables.Array.Add(Blendable);
			TempleStrainMID->SetScalarParameterValue(TEXT("Intensity"), 0.f);
			TempleStrainPP->BlendWeight = 0.f;
		}
	}

	if (ATDGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr)
	{
		GS->RegisterCentralTower(this);
	}

	const float Interval = FMath::Max(AttackInterval, 0.05f);
	GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &ACentralTowerBase::ScanAndAttack, Interval, true, 0.1f);
}

void ACentralTowerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	GetWorldTimerManager().ClearTimer(HitFlashTimer);
	GetWorldTimerManager().ClearTimer(StrainDecayTimer);
	if (HitFlashState.bActive)
	{
		EndHitFlash(HitFlashState, HitFlashMeshes, HitFlashMaterials);
	}
	Super::EndPlay(EndPlayReason);
}

void ACentralTowerBase::PlayHitFlash()
{
	BeginHitFlash(this, HitFlashState, HitFlashMeshes, HitFlashMaterials);
	GetWorldTimerManager().SetTimer(HitFlashTimer, this, &ACentralTowerBase::RestoreHitFlash, 0.12f, false);
}

void ACentralTowerBase::RestoreHitFlash()
{
	EndHitFlash(HitFlashState, HitFlashMeshes, HitFlashMaterials);
}

void ACentralTowerBase::ScanAndAttack()
{
	if (bIsDestroyed)
	{
		return;
	}

	AEnemyBase* Enemy = FindNearestEnemy();
	if (!Enemy)
	{
		return;
	}

	const float Damage = FMath::Max(AttackDamage, 1.f);
	Enemy->TakeDamageFromDefender(Damage);

	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, GetActorLocation(), Enemy->GetActorLocation(), FColor::Cyan, false, 0.35f, 0, 6.f);
	}

	UE_LOG(LogTemp, Log, TEXT("CentralTower attacked %s for %.1f damage"), *Enemy->GetName(), Damage);
}

float ACentralTowerBase::GetEffectiveAttackRange() const
{
	float Range = AttackRange > 0.f ? AttackRange : 1200.f;

	if (TowerMesh)
	{
		const FVector Extent = TowerMesh->Bounds.BoxExtent;
		Range += FMath::Max(Extent.X, Extent.Y);
	}

	return Range + 150.f;
}

AEnemyBase* ACentralTowerBase::FindNearestEnemy() const
{
	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), FoundEnemies);

	AEnemyBase* Nearest = nullptr;
	float NearestDistSq = FMath::Square(GetEffectiveAttackRange());
	const FVector TowerLoc = GetActorLocation();

	for (AActor* Actor : FoundEnemies)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		if (!Enemy || Enemy->IsDefeated())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(TowerLoc, Enemy->GetActorLocation());
		if (DistSq <= NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Enemy;
		}
	}

	return Nearest;
}

void ACentralTowerBase::ApplyDamage(float DamageAmount)
{
	if (bIsDestroyed || DamageAmount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	PlayHitFlash();
	HitStrainSpike = 0.45f;
	UpdateTempleStrain();
	GetWorldTimerManager().SetTimer(StrainDecayTimer, this, &ACentralTowerBase::DecayHitStrain, 0.08f, true);

	if (CurrentHealth <= 0.f)
	{
		bIsDestroyed = true;
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		OnTowerDestroyed.Broadcast();
		UE_LOG(LogTemp, Warning, TEXT("CentralTowerBase destroyed!"));
	}
}

void ACentralTowerBase::UpdateTempleStrain()
{
	const float Strain = FMath::Clamp((1.f - GetHealthPercent()) * 0.85f + HitStrainSpike, 0.f, 1.f);
	if (TempleStrainMID)
	{
		TempleStrainMID->SetScalarParameterValue(TEXT("Intensity"), Strain);
	}
	if (TempleStrainPP)
	{
		TempleStrainPP->BlendWeight = Strain;
	}
}

void ACentralTowerBase::DecayHitStrain()
{
	HitStrainSpike = FMath::Max(0.f, HitStrainSpike - 0.08f);
	UpdateTempleStrain();
	if (HitStrainSpike <= KINDA_SMALL_NUMBER)
	{
		HitStrainSpike = 0.f;
		GetWorldTimerManager().ClearTimer(StrainDecayTimer);
		UpdateTempleStrain();
	}
}

bool ACentralTowerBase::IsDestroyed() const
{
	return bIsDestroyed;
}

float ACentralTowerBase::GetHealthPercent() const
{
	return MaxHealth > 0.f ? (CurrentHealth / MaxHealth) : 0.f;
}

float ACentralTowerBase::GetDisplayHealthPercent_Implementation() const
{
	return GetHealthPercent();
}

bool ACentralTowerBase::IsUnitDestroyed_Implementation() const
{
	return bIsDestroyed;
}