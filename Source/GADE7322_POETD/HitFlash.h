#pragma once

#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"

struct FHitFlashState
{
	bool bActive = false;
	TArray<int32> MaterialCounts;
};

inline void BeginHitFlash(AActor* Actor, FHitFlashState& State, TArray<UMeshComponent*>& Meshes, TArray<UMaterialInterface*>& Saved)
{
	if (!Actor)
	{
		return;
	}

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* RedMat = BaseMat ? UMaterialInstanceDynamic::Create(BaseMat, Actor) : nullptr;
	if (RedMat)
	{
		const FLinearColor Red(1.f, 0.05f, 0.05f);
		RedMat->SetVectorParameterValue(TEXT("Color"), Red);
		RedMat->SetVectorParameterValue(TEXT("BaseColor"), Red);
	}
	if (!RedMat)
	{
		return;
	}

	if (!State.bActive)
	{
		Meshes.Reset();
		Saved.Reset();
		State.MaterialCounts.Reset();

		TArray<UMeshComponent*> Found;
		Actor->GetComponents<UMeshComponent>(Found);
		for (UMeshComponent* Mesh : Found)
		{
			if (!Mesh || Mesh->IsA<UWidgetComponent>() || !Mesh->IsVisible())
			{
				continue;
			}

			Meshes.Add(Mesh);
			const int32 Count = Mesh->GetNumMaterials();
			State.MaterialCounts.Add(Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Saved.Add(Mesh->GetMaterial(Index));
				Mesh->SetMaterial(Index, RedMat);
			}
		}
		State.bActive = true;
		return;
	}

	for (UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			Mesh->SetMaterial(Index, RedMat);
		}
	}
}

inline void EndHitFlash(FHitFlashState& State, TArray<UMeshComponent*>& Meshes, TArray<UMaterialInterface*>& Saved)
{
	int32 MaterialIndex = 0;
	for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
	{
		UMeshComponent* Mesh = Meshes[MeshIndex];
		const int32 Count = State.MaterialCounts.IsValidIndex(MeshIndex) ? State.MaterialCounts[MeshIndex] : 0;
		if (Mesh)
		{
			for (int32 Index = 0; Index < Count && Saved.IsValidIndex(MaterialIndex + Index); ++Index)
			{
				Mesh->SetMaterial(Index, Saved[MaterialIndex + Index]);
			}
		}
		MaterialIndex += Count;
	}

	Meshes.Reset();
	Saved.Reset();
	State.MaterialCounts.Reset();
	State.bActive = false;
}
