#include "UpgradeSparkle.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AUpgradeSparkle::AUpgradeSparkle()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AUpgradeSparkle::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* BaseMat = Sphere ? Sphere->GetMaterial(0) : nullptr;

	for (int32 Index = 0; Index < 18; ++Index)
	{
		UStaticMeshComponent* Spark = NewObject<UStaticMeshComponent>(this);
		Spark->SetupAttachment(RootComponent);
		Spark->RegisterComponent();
		Spark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spark->SetCastShadow(false);
		if (Sphere)
		{
			Spark->SetStaticMesh(Sphere);
		}
		Spark->SetWorldScale3D(FVector(0.07f));
		Spark->SetWorldLocation(GetActorLocation() + FMath::VRand() * 18.f);
		if (BaseMat)
		{
			if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
			{
				const FLinearColor Gold(1.f, 0.85f, 0.2f, 1.f);
				DynMat->SetVectorParameterValue(TEXT("Color"), Gold);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), Gold);
				Spark->SetMaterial(0, DynMat);
			}
		}
		Sparks.Add(Spark);
		Velocities.Add(FMath::VRand() * FMath::FRandRange(180.f, 420.f) + FVector(0.f, 0.f, 140.f));
	}

	SetLifeSpan(1.1f);
}

void AUpgradeSparkle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Life += DeltaSeconds;
	const float Fade = FMath::Clamp(1.f - Life / 1.1f, 0.f, 1.f);
	for (int32 Index = 0; Index < Sparks.Num(); ++Index)
	{
		if (!Sparks[Index])
		{
			continue;
		}
		Sparks[Index]->AddWorldOffset(Velocities[Index] * DeltaSeconds, false);
		Sparks[Index]->SetWorldScale3D(FVector(0.07f * Fade));
	}
}
