#include "GameVFX.h"
#include "Components/StaticMeshComponent.h"
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
