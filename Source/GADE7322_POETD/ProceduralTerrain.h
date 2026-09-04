#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CentralTowerBase.h"
#include "ProceduralTerrain.generated.h"

USTRUCT(BlueprintType)
struct FProceduralPathway
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pathway")
	TArray<FVector> Nodes;

	UPROPERTY(BlueprintReadOnly, Category = "Pathway")
	TArray<FIntPoint> Cells;

	UPROPERTY(BlueprintReadOnly, Category = "Pathway")
	FColor DebugColor = FColor::Red;
};

UCLASS()
class GADE7322_POETD_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrain();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "4"))
	int32 GridWidth = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "4"))
	int32 GridHeight = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "10.0"))
	FVector2D TileDimensions = FVector2D(200.f, 200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid")
	bool bAutoDetectTileDimensions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid", meta = (ClampMin = "0.0"))
	float TileSpacing = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid")
	int32 Seed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid")
	bool bRandomizeSeedOnPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Grid")
	UMaterialInterface* TerrainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Pathways", meta = (ClampMin = "3"))
	int32 NumPathways = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Pathways", meta = (ClampMin = "0"))
	int32 PathBufferCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Tower")
	TSubclassOf<ACentralTowerBase> CentralTowerClass;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain|Tower")
	FVector CentralTowerLocation;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain|Tower")
	ACentralTowerBase* SpawnedCentralTower;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* StraightTileISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* TurnLeftTileISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* TurnRightTileISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* TJunctionTileISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* GroundTileISM_A;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* GroundTileISM_B;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* GroundTileISM_C;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* GroundTileISM_D;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles")
	bool bSpawnPathTiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles")
	UStaticMesh* StraightPathMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles")
	UStaticMesh* TurnLeftPathMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles")
	UStaticMesh* TurnRightPathMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles")
	UStaticMesh* TJunctionPathMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float PathTileYawOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|PathTiles")
	float PathTileZOffset = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	bool bSpawnGroundTiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	UStaticMesh* GroundTileMeshA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	UStaticMesh* GroundTileMeshB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	UStaticMesh* GroundTileMeshC;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	UStaticMesh* GroundTileMeshD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	bool bRandomizeGroundTileRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	float GroundTileZOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|GroundTiles")
	bool bHideGroundMeshVisualWhenTilesActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug", meta = (ClampMin = "0.0"))
	float DebugSphereRadius = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Debug")
	FColor BuildLocationDebugColor = FColor::Green;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain|Output")
	TArray<FVector> PathwayNodes;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain|Output")
	TArray<FVector> BuildGridLocations;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain|Output")
	TArray<FIntPoint> BuildGridCells;

	UPROPERTY(BlueprintReadOnly, Category = "Terrain|Output")
	TArray<FProceduralPathway> Pathways;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain")
	void GenerateTerrain();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Terrain")
	void RandomizeSeedAndRegenerate();

	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GridToWorldLocation(int32 GridX, int32 GridY) const;

	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetStepVector(const FIntPoint& Direction) const;

private:
	FRandomStream RandomStream;
	TSet<FIntPoint> PathCellSet;

	void GenerateGridMesh();
	void GeneratePathways();
	void GenerateBuildLocations();
	void DrawDebugVisualization();
	void DetectTileDimensionsFromMesh();

	bool IsNearPathCell(const FIntPoint& Cell) const;
	FIntPoint GetRandomEdgeCell(int32 EdgeIndex) const;

	TSet<FIntPoint> PlacedTileCells;

	void SpawnPathTiles();
	void SpawnGroundTiles();
	void SpawnTileInstance(UInstancedStaticMeshComponent* ISM, const FIntPoint& Cell, float Yaw, float ZOffset);
	float YawForDirection(const FIntPoint& Dir) const;
};