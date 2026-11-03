#include "UpgradeSparkle.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AUpgradeSparkle::AUpgradeSparkle()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(Root);
	GlowLight->SetLightColor(FLinearColor(1.f, 1.f, 0.15f));
	GlowLight->SetIntensity(12000.f);
	GlowLight->SetAttenuationRadius(420.f);
	GlowLight->SetCastShadows(false);
}

void AUpgradeSparkle::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* BaseMat = Sphere ? Sphere->GetMaterial(0) : nullptr;
	const FLinearColor Yellow(1.f, 1.f, 0.15f, 1.f);

	for (int32 Index = 0; Index < 22; ++Index)
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
		Spark->SetWorldScale3D(FVector(0.18f));
		Spark->SetWorldLocation(GetActorLocation() + FMath::VRand() * 42.f);
		if (BaseMat)
		{
			if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), Yellow);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), Yellow);
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), Yellow * 8.f);
				Spark->SetMaterial(0, DynMat);
			}
		}
		Sparks.Add(Spark);
		Velocities.Add(FMath::VRand() * FMath::FRandRange(220.f, 520.f) + FVector(0.f, 0.f, 180.f));
	}

	SetLifeSpan(1.1f);
}

void AUpgradeSparkle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Life += DeltaSeconds;
	const float Fade = FMath::Clamp(1.f - Life / 1.1f, 0.f, 1.f);
	if (GlowLight)
	{
		GlowLight->SetIntensity(12000.f * Fade);
	}
	for (int32 Index = 0; Index < Sparks.Num(); ++Index)
	{
		if (!Sparks[Index])
		{
			continue;
		}
		Sparks[Index]->AddWorldOffset(Velocities[Index] * DeltaSeconds, false);
		Sparks[Index]->SetWorldScale3D(FVector(0.18f * Fade));
	}
}
