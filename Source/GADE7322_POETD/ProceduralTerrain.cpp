#include "ProceduralTerrain.h"
#include "DrawDebugHelpers.h"

AProceduralTerrain::AProceduralTerrain()
{
	PrimaryActorTick.bCanEverTick = false;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	RootComponent = ProceduralMesh;

	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProceduralMesh->SetCollisionProfileName(TEXT("BlockAll"));
	ProceduralMesh->bUseAsyncCooking = true;

	auto MakeTileISM = [this](FName Name) -> UInstancedStaticMeshComponent*
		{
			UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
			ISM->SetupAttachment(RootComponent);
			ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ISM->SetMobility(EComponentMobility::Movable);
			return ISM;
		};

	StraightTileISM = MakeTileISM(TEXT("StraightTileISM"));
	TurnLeftTileISM = MakeTileISM(TEXT("TurnLeftTileISM"));
	TurnRightTileISM = MakeTileISM(TEXT("TurnRightTileISM"));
	TJunctionTileISM = MakeTileISM(TEXT("TJunctionTileISM"));

	GroundTileISM_A = MakeTileISM(TEXT("GroundTileISM_A"));
	GroundTileISM_B = MakeTileISM(TEXT("GroundTileISM_B"));
	GroundTileISM_C = MakeTileISM(TEXT("GroundTileISM_C"));
	GroundTileISM_D = MakeTileISM(TEXT("GroundTileISM_D"));
}

void AProceduralTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateTerrain();
}

void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	if (bRandomizeSeedOnPlay)
	{
		Seed = FMath::Rand();
	}

	GenerateTerrain();

	if (bDrawDebug)
	{
		DrawDebugVisualization();
	}

	if (CentralTowerClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedCentralTower = GetWorld()->SpawnActor<ACentralTowerBase>(
			CentralTowerClass, CentralTowerLocation, FRotator::ZeroRotator, SpawnParams);
	}
}

#if WITH_EDITOR
void AProceduralTerrain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		RerunConstructionScripts();
	}
}
#endif

void AProceduralTerrain::GenerateTerrain()
{
	GridWidth = FMath::Max(GridWidth, 4);
	GridHeight = FMath::Max(GridHeight, 4);
	NumPathways = FMath::Max(NumPathways, 3);

	if (bAutoDetectTileDimensions)
	{
		DetectTileDimensionsFromMesh();
	}

	RandomStream = FRandomStream(Seed);

	PathCellSet.Empty();
	PathwayNodes.Empty();
	Pathways.Empty();
	BuildGridLocations.Empty();
	BuildGridCells.Empty();

	GenerateGridMesh();
	GeneratePathways();
	GenerateBuildLocations();

	CentralTowerLocation = GridToWorldLocation(GridWidth / 2, GridHeight / 2);

	if (bSpawnPathTiles)
	{
		SpawnPathTiles();
	}

	if (bSpawnGroundTiles)
	{
		SpawnGroundTiles();
	}
}

void AProceduralTerrain::RandomizeSeedAndRegenerate()
{
	Seed = FMath::Rand();
	GenerateTerrain();

	if (bDrawDebug && GetWorld() && GetWorld()->IsGameWorld())
	{
		DrawDebugVisualization();
	}
}

FVector AProceduralTerrain::GridToWorldLocation(int32 GridX, int32 GridY) const
{
	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;
	return GetActorLocation() + FVector(GridX * StepX, GridY * StepY, 0.f);
}

FVector AProceduralTerrain::GetStepVector(const FIntPoint& Direction) const
{
	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;
	return FVector(Direction.X * StepX, Direction.Y * StepY, 0.f);
}

void AProceduralTerrain::DetectTileDimensionsFromMesh()
{
	if (!StraightPathMesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = StraightPathMesh->GetBounds();
	const FVector FullSize = Bounds.BoxExtent * 2.f;

	if (FullSize.X <= 0.f || FullSize.Y <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProceduralTerrain: StraightPathMesh has invalid bounds - keeping manual TileDimensions."));
		return;
	}

	TileDimensions = FVector2D(FullSize.X, FullSize.Y);

	UE_LOG(LogTemp, Log, TEXT("ProceduralTerrain: Auto-detected TileDimensions from StraightPathMesh bounds: X=%.1f Y=%.1f"),
		TileDimensions.X, TileDimensions.Y);
}

void AProceduralTerrain::GenerateGridMesh()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	TArray<FColor> VertexColors;

	const int32 VertsX = GridWidth + 1;
	const int32 VertsY = GridHeight + 1;
	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;

	Vertices.Reserve(VertsX * VertsY);
	Normals.Reserve(VertsX * VertsY);
	UVs.Reserve(VertsX * VertsY);
	Tangents.Reserve(VertsX * VertsY);

	for (int32 Y = 0; Y < VertsY; ++Y)
	{
		for (int32 X = 0; X < VertsX; ++X)
		{
			Vertices.Add(FVector(X * StepX, Y * StepY, 0.f));
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D(static_cast<float>(X) / GridWidth, static_cast<float>(Y) / GridHeight));
			Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		}
	}

	Triangles.Reserve(GridWidth * GridHeight * 6);
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const int32 TopLeft = Y * VertsX + X;
			const int32 TopRight = TopLeft + 1;
			const int32 BottomLeft = (Y + 1) * VertsX + X;
			const int32 BottomRight = BottomLeft + 1;

			Triangles.Add(TopLeft);
			Triangles.Add(BottomLeft);
			Triangles.Add(TopRight);

			Triangles.Add(TopRight);
			Triangles.Add(BottomLeft);
			Triangles.Add(BottomRight);
		}
	}

	ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	if (TerrainMaterial)
	{
		ProceduralMesh->SetMaterial(0, TerrainMaterial);
	}
}

FIntPoint AProceduralTerrain::GetRandomEdgeCell(int32 EdgeIndex) const
{
	switch (EdgeIndex % 4)
	{
	case 0:
		return FIntPoint(RandomStream.RandRange(0, GridWidth - 1), 0);
	case 1:
		return FIntPoint(RandomStream.RandRange(0, GridWidth - 1), GridHeight - 1);
	case 2:
		return FIntPoint(0, RandomStream.RandRange(0, GridHeight - 1));
	default:
		return FIntPoint(GridWidth - 1, RandomStream.RandRange(0, GridHeight - 1));
	}
}

void AProceduralTerrain::GeneratePathways()
{
	const FIntPoint CenterCell(GridWidth / 2, GridHeight / 2);
	const int32 MaxSteps = (GridWidth + GridHeight) * 3;

	TArray<int32> EdgeOrder = { 0, 1, 2, 3 };
	for (int32 i = EdgeOrder.Num() - 1; i > 0; --i)
	{
		const int32 j = RandomStream.RandRange(0, i);
		EdgeOrder.Swap(i, j);
	}

	static const FColor PathColors[] = { FColor::Red, FColor::Blue, FColor::Yellow, FColor::Cyan, FColor::Magenta, FColor::Orange };

	for (int32 PathIndex = 0; PathIndex < NumPathways; ++PathIndex)
	{
		FProceduralPathway NewPath;
		NewPath.DebugColor = PathColors[PathIndex % UE_ARRAY_COUNT(PathColors)];

		FIntPoint Current = GetRandomEdgeCell(EdgeOrder[PathIndex % EdgeOrder.Num()]);
		FVector CurrentWorldLocation = GridToWorldLocation(Current.X, Current.Y);

		int32 Steps = 0;
		while (Current != CenterCell && Steps < MaxSteps)
		{
			PathCellSet.Add(Current);
			NewPath.Nodes.Add(CurrentWorldLocation);
			NewPath.Cells.Add(Current);

			const int32 DeltaX = CenterCell.X - Current.X;
			const int32 DeltaY = CenterCell.Y - Current.Y;

			bool bMoveX;
			if (RandomStream.FRand() < 0.75f)
			{
				bMoveX = FMath::Abs(DeltaX) >= FMath::Abs(DeltaY);
			}
			else
			{
				bMoveX = RandomStream.FRand() < 0.5f;
			}

			FIntPoint Direction(0, 0);
			if (bMoveX && DeltaX != 0)
			{
				Direction.X = FMath::Sign(DeltaX);
			}
			else if (DeltaY != 0)
			{
				Direction.Y = FMath::Sign(DeltaY);
			}
			else if (DeltaX != 0)
			{
				Direction.X = FMath::Sign(DeltaX);
			}

			const FIntPoint NextCell(
				FMath::Clamp(Current.X + Direction.X, 0, GridWidth - 1),
				FMath::Clamp(Current.Y + Direction.Y, 0, GridHeight - 1));

			const FIntPoint ActualDirection = NextCell - Current;
			CurrentWorldLocation += GetStepVector(ActualDirection);
			Current = NextCell;

			++Steps;
		}

		PathCellSet.Add(CenterCell);
		NewPath.Nodes.Add(GridToWorldLocation(CenterCell.X, CenterCell.Y));
		NewPath.Cells.Add(CenterCell);

		Pathways.Add(NewPath);
		PathwayNodes.Append(NewPath.Nodes);
	}
}

bool AProceduralTerrain::IsNearPathCell(const FIntPoint& Cell) const
{
	for (int32 OffsetX = -PathBufferCells; OffsetX <= PathBufferCells; ++OffsetX)
	{
		for (int32 OffsetY = -PathBufferCells; OffsetY <= PathBufferCells; ++OffsetY)
		{
			if (PathCellSet.Contains(FIntPoint(Cell.X + OffsetX, Cell.Y + OffsetY)))
			{
				return true;
			}
		}
	}
	return false;
}

void AProceduralTerrain::GenerateBuildLocations()
{
	const FIntPoint CenterCell(GridWidth / 2, GridHeight / 2);

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FIntPoint Cell(X, Y);

			if (Cell == CenterCell)
			{
				continue;
			}

			if (IsNearPathCell(Cell))
			{
				continue;
			}

			BuildGridLocations.Add(GridToWorldLocation(X, Y));
			BuildGridCells.Add(Cell);
		}
	}
}

void AProceduralTerrain::DrawDebugVisualization()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ZOffset = 10.f;

	for (const FProceduralPathway& Path : Pathways)
	{
		for (int32 i = 0; i < Path.Nodes.Num(); ++i)
		{
			const FVector NodeLocation = Path.Nodes[i] + FVector(0.f, 0.f, ZOffset);
			DrawDebugSphere(World, NodeLocation, DebugSphereRadius, 8, Path.DebugColor, true, -1.f, 0, 2.f);

			if (i > 0)
			{
				const FVector PrevLocation = Path.Nodes[i - 1] + FVector(0.f, 0.f, ZOffset);
				DrawDebugLine(World, PrevLocation, NodeLocation, Path.DebugColor, true, -1.f, 0, 4.f);
			}
		}
	}

	for (const FVector& BuildLocation : BuildGridLocations)
	{
		DrawDebugSphere(World, BuildLocation + FVector(0.f, 0.f, ZOffset), DebugSphereRadius * 0.5f,
			6, BuildLocationDebugColor, true, -1.f, 0, 1.f);
	}

	DrawDebugSphere(World, CentralTowerLocation + FVector(0.f, 0.f, ZOffset * 2.f),
		DebugSphereRadius * 2.f, 12, FColor::White, true, -1.f, 0, 3.f);
}

float AProceduralTerrain::YawForDirection(const FIntPoint& Dir) const
{
	if (Dir.X == 1) return 0.f;
	if (Dir.Y == 1) return 90.f;
	if (Dir.X == -1) return 180.f;
	if (Dir.Y == -1) return 270.f;
	return 0.f;
}

void AProceduralTerrain::SpawnTileInstance(UInstancedStaticMeshComponent* ISM, const FIntPoint& Cell, float Yaw, float ZOffset)
{
	if (!ISM || !ISM->GetStaticMesh())
	{
		return;
	}

	const FRotator Rotation(0.f, Yaw, 0.f);

	const FBoxSphereBounds Bounds = ISM->GetStaticMesh()->GetBounds();
	const FVector LocalCenterOffsetXY(Bounds.Origin.X, Bounds.Origin.Y, 0.f);
	const FVector RotatedOffset = Rotation.RotateVector(LocalCenterOffsetXY);

	const FVector CellCenter = GridToWorldLocation(Cell.X, Cell.Y) + FVector(0.f, 0.f, ZOffset);
	const FVector PivotLocation = CellCenter - RotatedOffset;

	ISM->AddInstance(FTransform(Rotation, PivotLocation, FVector(1.f)));
}

void AProceduralTerrain::SpawnPathTiles()
{
	StraightTileISM->ClearInstances();
	TurnLeftTileISM->ClearInstances();
	TurnRightTileISM->ClearInstances();
	TJunctionTileISM->ClearInstances();

	if (StraightPathMesh) StraightTileISM->SetStaticMesh(StraightPathMesh);
	if (TurnLeftPathMesh) TurnLeftTileISM->SetStaticMesh(TurnLeftPathMesh);
	if (TurnRightPathMesh) TurnRightTileISM->SetStaticMesh(TurnRightPathMesh);
	if (TJunctionPathMesh) TJunctionTileISM->SetStaticMesh(TJunctionPathMesh);

	PlacedTileCells.Empty();

	const FIntPoint CenterCell(GridWidth / 2, GridHeight / 2);

	for (const FProceduralPathway& Path : Pathways)
	{
		for (int32 i = 0; i < Path.Cells.Num() - 1; ++i)
		{
			const FIntPoint Cell = Path.Cells[i];
			if (Cell == CenterCell)
			{
				continue;
			}

			const FIntPoint OutDir = Path.Cells[i + 1] - Cell;
			const FIntPoint InDir = (i > 0) ? (Cell - Path.Cells[i - 1]) : OutDir;

			if (PlacedTileCells.Contains(Cell))
			{
				SpawnTileInstance(TJunctionTileISM, Cell, YawForDirection(OutDir) + PathTileYawOffset, PathTileZOffset);
				UE_LOG(LogTemp, Warning,
					TEXT("ProceduralTerrain: Cell (%d,%d) is shared by multiple pathways - placed a T-Junction tile. Verify orientation manually."),
					Cell.X, Cell.Y);
				continue;
			}

			if (InDir == OutDir)
			{
				SpawnTileInstance(StraightTileISM, Cell, YawForDirection(InDir) + PathTileYawOffset, PathTileZOffset);
			}
			else
			{
				const int32 CrossZ = InDir.X * OutDir.Y - InDir.Y * OutDir.X;
				UInstancedStaticMeshComponent* TurnISM = (CrossZ > 0) ? TurnLeftTileISM : TurnRightTileISM;
				SpawnTileInstance(TurnISM, Cell, YawForDirection(InDir) + PathTileYawOffset, PathTileZOffset);
			}

			PlacedTileCells.Add(Cell);
		}
	}
}

void AProceduralTerrain::SpawnGroundTiles()
{
	GroundTileISM_A->ClearInstances();
	GroundTileISM_B->ClearInstances();
	GroundTileISM_C->ClearInstances();
	GroundTileISM_D->ClearInstances();

	TArray<UInstancedStaticMeshComponent*> ValidGroundISMs;

	if (GroundTileMeshA) { GroundTileISM_A->SetStaticMesh(GroundTileMeshA); ValidGroundISMs.Add(GroundTileISM_A); }
	if (GroundTileMeshB) { GroundTileISM_B->SetStaticMesh(GroundTileMeshB); ValidGroundISMs.Add(GroundTileISM_B); }
	if (GroundTileMeshC) { GroundTileISM_C->SetStaticMesh(GroundTileMeshC); ValidGroundISMs.Add(GroundTileISM_C); }
	if (GroundTileMeshD) { GroundTileISM_D->SetStaticMesh(GroundTileMeshD); ValidGroundISMs.Add(GroundTileISM_D); }

	if (ValidGroundISMs.Num() == 0)
	{
		ProceduralMesh->SetVisibility(true);
		return;
	}

	const FIntPoint CenterCell(GridWidth / 2, GridHeight / 2);

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FIntPoint Cell(X, Y);

			if (Cell == CenterCell || PathCellSet.Contains(Cell))
			{
				continue;
			}

			UInstancedStaticMeshComponent* ChosenISM = ValidGroundISMs[RandomStream.RandRange(0, ValidGroundISMs.Num() - 1)];
			const float Yaw = bRandomizeGroundTileRotation ? RandomStream.RandRange(0, 3) * 90.f : 0.f;
			SpawnTileInstance(ChosenISM, Cell, Yaw, GroundTileZOffset);
		}
	}

	if (bHideGroundMeshVisualWhenTilesActive)
	{
		ProceduralMesh->SetVisibility(false);
	}
}