#include "DummyEnemyTarget.h"
#include "UObject/ConstructorHelpers.h"

ADummyEnemyTarget::ADummyEnemyTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	RootComponent = TargetMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		TargetMesh->SetStaticMesh(SphereMeshAsset.Object);
		TargetMesh->SetWorldScale3D(FVector(0.8f, 0.8f, 0.8f));
	}

	TargetMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TargetMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ADummyEnemyTarget::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDestroyed = false;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void ADummyEnemyTarget::ApplyDamage(float DamageAmount)
{
	if (bIsDestroyed || DamageAmount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	UE_LOG(LogTemp, Log, TEXT("DummyEnemyTarget %s took %.1f damage, %.1f/%.1f HP remaining"),
		*GetName(), DamageAmount, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		bIsDestroyed = true;
		OnTargetDestroyed.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("DummyEnemyTarget %s destroyed"), *GetName());
		SetLifeSpan(0.15f);
	}
}

bool ADummyEnemyTarget::IsDestroyed() const
{
	return bIsDestroyed;
}