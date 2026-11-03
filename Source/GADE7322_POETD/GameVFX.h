#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UStaticMeshComponent;
class UWorld;

namespace GameVFX
{
	UMaterialInterface* LoadBogSlimeMaterial();
	UMaterialInterface* LoadUpgradeGoldMaterial();
	UMaterialInterface* LoadTempleStrainMaterial();
	UNiagaraSystem* LoadUpgradeBurstSystem();
	UNiagaraSystem* LoadDeathDustSystem();

	void ApplyMaterialToMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material);
	UMaterialInstanceDynamic* ApplyDynamicMaterialToMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material);
	void SpawnNiagaraAt(UWorld* World, UNiagaraSystem* System, const FVector& Location);
}
