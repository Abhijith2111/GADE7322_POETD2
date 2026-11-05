#include "GameVFX.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

namespace GameVFX
{
	UMaterialInterface* LoadBogSlimeMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_BogSlime.M_BogSlime"));
	}

	UMaterialInterface* LoadUpgradeGoldMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_UpgradeGoldPulse.M_UpgradeGoldPulse"));
	}

	UMaterialInterface* LoadTempleStrainMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_PP_TempleStrain.M_PP_TempleStrain"));
	}

	UNiagaraSystem* LoadUpgradeBurstSystem()
	{
		return LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/VFX/NS_UpgradeBurst.NS_UpgradeBurst"));
	}

	UNiagaraSystem* LoadDeathDustSystem()
	{
		return LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/VFX/NS_DefenderDeathDust.NS_DefenderDeathDust"));
	}

	void ApplyMaterialToMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material)
	{
		if (!Mesh || !Material)
		{
			return;
		}

		const int32 NumMaterials = FMath::Max(Mesh->GetNumMaterials(), 1);
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			Mesh->SetMaterial(Index, Material);
		}
	}

	UMaterialInstanceDynamic* ApplyDynamicMaterialToMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material)
	{
		if (!Mesh || !Material)
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(Material, Mesh);
		if (!DynMat)
		{
			return nullptr;
		}

		const int32 NumMaterials = FMath::Max(Mesh->GetNumMaterials(), 1);
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			Mesh->SetMaterial(Index, DynMat);
		}
		return DynMat;
	}

	void UpdateGoldRing(AActor* Owner, UStaticMeshComponent* BuildingMesh, TObjectPtr<UStaticMeshComponent>& Ring, int32 Stage)
	{
		if (!Owner || !BuildingMesh || !BuildingMesh->GetStaticMesh())
		{
			return;
		}

		UInstancedStaticMeshComponent* Crown = Cast<UInstancedStaticMeshComponent>(Ring.Get());
		if (!Crown)
		{
			if (Ring)
			{
				Ring->DestroyComponent();
				Ring = nullptr;
			}

			Crown = NewObject<UInstancedStaticMeshComponent>(Owner);
			Crown->SetupAttachment(Owner->GetRootComponent());
			Crown->SetAbsolute(true, true, true);
			Crown->RegisterComponent();
			Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Crown->SetCastShadow(false);
			if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
			{
				Crown->SetStaticMesh(Sphere);
			}
			UMaterialInterface* GlowMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_GoldParticle.M_GoldParticle"));
			if (!GlowMat)
			{
				GlowMat = LoadUpgradeGoldMaterial();
			}
			ApplyDynamicMaterialToMesh(Crown, GlowMat);
			Ring = Crown;
		}

		if (!Crown || !Crown->GetStaticMesh())
		{
			return;
		}

		Crown->SetWorldLocation(FVector::ZeroVector);
		Crown->SetWorldRotation(FRotator::ZeroRotator);
		Crown->SetWorldScale3D(FVector::OneVector);

		const FBox LocalBox = BuildingMesh->GetStaticMesh()->GetBoundingBox();
		const FTransform MeshTransform = BuildingMesh->GetComponentTransform();
		const FVector WorldTop = MeshTransform.TransformPosition(FVector(LocalBox.GetCenter().X, LocalBox.GetCenter().Y, LocalBox.Max.Z));
		const FVector WorldExtent = LocalBox.GetExtent() * MeshTransform.GetScale3D().GetAbs();
		const float Orbit = FMath::Clamp(FMath::Min(WorldExtent.X, WorldExtent.Y) * 0.38f, 22.f, 70.f);
		const float Height = WorldTop.Z + (Stage >= 2 ? 14.f : 6.f);
		const int32 Count = Stage >= 2 ? 8 : 5;
		const float SphereRadius = FMath::Max(Crown->GetStaticMesh()->GetBoundingBox().GetExtent().X, 1.f);
		const float Uniform = (Stage >= 2 ? 11.f : 8.f) / SphereRadius;

		Crown->ClearInstances();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = (2.f * PI * Index) / static_cast<float>(Count);
			const FVector Location(
				WorldTop.X + FMath::Cos(Angle) * Orbit,
				WorldTop.Y + FMath::Sin(Angle) * Orbit,
				Height);
			Crown->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(Uniform)), true);
		}
		Crown->SetVisibility(true, true);
		Crown->SetHiddenInGame(false);
	}

	void SpawnNiagaraAt(UWorld* World, UNiagaraSystem* System, const FVector& Location)
	{
		if (!World || !System)
		{
			return;
		}

		UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			System,
			Location,
			FRotator::ZeroRotator,
			FVector(1.f),
			false,
			true,
			ENCPoolMethod::None,
			true);

		if (!Component)
		{
			return;
		}

		TWeakObjectPtr<UNiagaraComponent> WeakComponent(Component);
		FTimerHandle DestroyHandle;
		World->GetTimerManager().SetTimer(
			DestroyHandle,
			FTimerDelegate::CreateLambda([WeakComponent]()
			{
				if (UNiagaraComponent* Active = WeakComponent.Get())
				{
					Active->DeactivateImmediate();
					Active->DestroyComponent();
				}
			}),
			1.2f,
			false);
	}
}
