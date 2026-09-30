#include "ProceduralTerrain.h"
#include "DrawDebugHelpers.h"
#include "TDGameState.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AProceduralTerrain::AProceduralTerrain()
{
	PrimaryActorTick.bCanEverTick = false;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	RootComponent = ProceduralMesh;

	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ProceduralMesh->SetCollisionProfileName(TEXT("BlockAll"));
	ProceduralMesh->bUseAsyncCooking = true;

	VoidPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VoidPlane"));
	VoidPlane->SetupAttachment(RootComponent);
	VoidPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VoidPlane->SetCastShadow(false);
	VoidPlane->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		VoidPlane->SetStaticMesh(PlaneMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMat.Succeeded())
	{
		VoidPlane->SetMaterial(0, ShapeMat.Object);
	}

	auto MakeTileISM = [this](FName Name) -> UInstancedStaticMeshComponent*
		{
			UInstancedStaticMeshComponent* ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
			ISM->SetupAttachment(RootComponent);
			ISM->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ISM->SetCollisionResponseToAllChannels(ECR_Ignore);
			ISM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			ISM->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
			ISM->SetGenerateOverlapEvents(false);
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

	if (CentralTowerClass)
	{
		FVector SpawnLocation = CentralTowerLocation;
		const FVector TraceStart(CentralTowerLocation.X, CentralTowerLocation.Y, CentralTowerLocation.Z + 2500.f);
		const FVector TraceEnd(CentralTowerLocation.X, CentralTowerLocation.Y, CentralTowerLocation.Z - 5000.f);

		FHitResult SurfaceHit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CentralTowerSpawnSnap), false, this);
		if (GetWorld()->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			SpawnLocation.Z = SurfaceHit.ImpactPoint.Z;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedCentralTower = GetWorld()->SpawnActor<ACentralTowerBase>(
			CentralTowerClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (SpawnedCentralTower)
		{
			if (ATDGameState* GS = GetWorld()->GetGameState<ATDGameState>())
			{
				GS->RegisterCentralTower(SpawnedCentralTower);
			}
		}
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
	NumPathways = 3;

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

	UpdateVoidPlane();
}

void AProceduralTerrain::UpdateVoidPlane()
{
	if (!VoidPlane || !VoidPlane->GetStaticMesh())
	{
		return;
	}

	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;
	const float BoardX = GridWidth * StepX;
	const float BoardY = GridHeight * StepY;
	const FBox MeshBox = VoidPlane->GetStaticMesh()->GetBoundingBox();
	const float MeshX = FMath::Max(MeshBox.GetSize().X, 1.f);
	const float MeshY = FMath::Max(MeshBox.GetSize().Y, 1.f);
	constexpr float Coverage = 8.f;

	VoidPlane->SetRelativeScale3D(FVector((BoardX * Coverage) / MeshX, (BoardY * Coverage) / MeshY, 1.f));
	VoidPlane->SetRelativeLocation(FVector(BoardX * 0.5f, BoardY * 0.5f, -80.f));

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!BaseMat)
	{
		BaseMat = VoidPlane->GetMaterial(0);
	}
	if (BaseMat)
	{
		if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			const FLinearColor Orange(1.f, 0.42f, 0.05f);
			DynMat->SetVectorParameterValue(TEXT("Color"), Orange);
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Orange);
			VoidPlane->SetMaterial(0, DynMat);
		}
	}
}

void AProceduralTerrain::RandomizeSeedAndRegenerate()
{
	Seed = FMath::Rand();
	GenerateTerrain();
}

FVector AProceduralTerrain::GridToWorldLocation(int32 GridX, int32 GridY) const
{
	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;
	return GetActorLocation() + FVector((GridX + 0.5f) * StepX, (GridY + 0.5f) * StepY, 0.f);
}

FVector AProceduralTerrain::GetStepVector(const FIntPoint& Direction) const
{
	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;
	return FVector(Direction.X * StepX, Direction.Y * StepY, 0.f);
}

bool AProceduralTerrain::WorldToGridCell(const FVector& WorldLocation, FIntPoint& OutCell) const
{
	const float StepX = TileDimensions.X + TileSpacing;
	const float StepY = TileDimensions.Y + TileSpacing;
	if (StepX <= KINDA_SMALL_NUMBER || StepY <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Local = WorldLocation - GetActorLocation();
	OutCell = FIntPoint(FMath::FloorToInt(Local.X / StepX), FMath::FloorToInt(Local.Y / StepY));
	return OutCell.X >= 0 && OutCell.X < GridWidth && OutCell.Y >= 0 && OutCell.Y < GridHeight;
}

bool AProceduralTerrain::FindBuildSlotAtWorld(const FVector& WorldLocation, int32& OutIndex, FVector& OutLocation) const
{
	FIntPoint ClickedCell;
	if (WorldToGridCell(WorldLocation, ClickedCell))
	{
		for (int32 i = 0; i < BuildGridCells.Num(); ++i)
		{
			if (BuildGridCells[i] == ClickedCell)
			{
				OutIndex = i;
				OutLocation = BuildGridLocations[i];
				return true;
			}
		}
	}

	int32 BestIndex = INDEX_NONE;
	float BestDistSq = FLT_MAX;
	for (int32 i = 0; i < BuildGridLocations.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared2D(WorldLocation, BuildGridLocations[i]);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	const float SnapRadius = FMath::Max(TileDimensions.X, TileDimensions.Y) * 0.55f;
	if (BestIndex == INDEX_NONE || BestDistSq > FMath::Square(SnapRadius))
	{
		return false;
	}

	OutIndex = BestIndex;
	OutLocation = BuildGridLocations[BestIndex];
	return true;
}

bool AProceduralTerrain::IsWorldOnPath(const FVector& WorldLocation) const
{
	FIntPoint Cell;
	if (!WorldToGridCell(WorldLocation, Cell))
	{
		return false;
	}

	return PathCellSet.Contains(Cell);
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
	NumPathways = 3;

	TArray<int32> EdgeOrder = { 0, 1, 2, 3 };
	for (int32 i = EdgeOrder.Num() - 1; i > 0; --i)
	{
		const int32 j = RandomStream.RandRange(0, i);
		EdgeOrder.Swap(i, j);
	}

	TSet<int32> UsedColumns;
	TSet<int32> UsedRows;
	static const FColor PathColors[] = { FColor::Red, FColor::Blue, FColor::Yellow, FColor::Cyan, FColor::Magenta, FColor::Orange };

	auto BuildLane = [CenterCell](FIntPoint Start, bool bMoveXFirst) -> TArray<FIntPoint>
	{
		TArray<FIntPoint> Cells;
		Cells.Add(Start);
		FIntPoint Current = Start;
		const int32 Axes[2] = { bMoveXFirst ? 0 : 1, bMoveXFirst ? 1 : 0 };
		for (int32 Axis : Axes)
		{
			while ((Axis == 0 && Current.X != CenterCell.X) || (Axis == 1 && Current.Y != CenterCell.Y))
			{
				if (Axis == 0)
				{
					Current.X += FMath::Sign(CenterCell.X - Current.X);
				}
				else
				{
					Current.Y += FMath::Sign(CenterCell.Y - Current.Y);
				}
				Cells.Add(Current);
			}
		}
		if (Cells.Last() != CenterCell)
		{
			Cells.Add(CenterCell);
		}
		return Cells;
	};

	for (int32 PathIndex = 0; PathIndex < NumPathways; ++PathIndex)
	{
		const int32 Edge = EdgeOrder[PathIndex];
		const bool bHorizontalEdge = (Edge == 0 || Edge == 1);
		const bool bMoveXFirst = !bHorizontalEdge;

		TArray<int32> Slots;
		const int32 SlotCount = bHorizontalEdge ? GridWidth : GridHeight;
		Slots.Reserve(SlotCount);
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			Slots.Add(Slot);
		}
		for (int32 i = Slots.Num() - 1; i > 0; --i)
		{
			const int32 j = RandomStream.RandRange(0, i);
			Slots.Swap(i, j);
		}

		TArray<FIntPoint> Cells;
		for (int32 Slot : Slots)
		{
			if (bHorizontalEdge)
			{
				if (Slot == CenterCell.X || UsedColumns.Contains(Slot))
				{
					continue;
				}
			}
			else if (Slot == CenterCell.Y || UsedRows.Contains(Slot))
			{
				continue;
			}

			FIntPoint Start;
			switch (Edge)
			{
			case 0: Start = FIntPoint(Slot, 0); break;
			case 1: Start = FIntPoint(Slot, GridHeight - 1); break;
			case 2: Start = FIntPoint(0, Slot); break;
			default: Start = FIntPoint(GridWidth - 1, Slot); break;
			}

			Cells = BuildLane(Start, bMoveXFirst);
			break;
		}

		if (Cells.Num() == 0)
		{
			const int32 Fallback = bHorizontalEdge
				? (CenterCell.X == 0 ? 1 : 0)
				: (CenterCell.Y == 0 ? 1 : 0);
			FIntPoint Start;
			switch (Edge)
			{
			case 0: Start = FIntPoint(Fallback, 0); break;
			case 1: Start = FIntPoint(Fallback, GridHeight - 1); break;
			case 2: Start = FIntPoint(0, Fallback); break;
			default: Start = FIntPoint(GridWidth - 1, Fallback); break;
			}
			Cells = BuildLane(Start, bMoveXFirst);
		}

		if (bHorizontalEdge)
		{
			UsedColumns.Add(Cells[0].X);
		}
		else
		{
			UsedRows.Add(Cells[0].Y);
		}

		FProceduralPathway NewPath;
		NewPath.DebugColor = PathColors[PathIndex % UE_ARRAY_COUNT(PathColors)];
		for (const FIntPoint& Cell : Cells)
		{
			PathCellSet.Add(Cell);
			NewPath.Cells.Add(Cell);
			NewPath.Nodes.Add(GridToWorldLocation(Cell.X, Cell.Y));
		}

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

void AProceduralTerrain::SpawnTileInstance(UInstancedStaticMeshComponent* ISM, const FIntPoint& Cell, float Yaw, float ZOffset, float XYScaleMultiplier)
{
	if (!ISM || !ISM->GetStaticMesh())
	{
		return;
	}

	UStaticMesh* Mesh = ISM->GetStaticMesh();
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector MeshSize = Bounds.BoxExtent * 2.f;
	if (MeshSize.X <= KINDA_SMALL_NUMBER || MeshSize.Y <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float SafeScale = FMath::Clamp(XYScaleMultiplier, 0.5f, 1.f);
	const FVector Scale(
		(TileDimensions.X * SafeScale) / MeshSize.X,
		(TileDimensions.Y * SafeScale) / MeshSize.Y,
		1.f);

	const FRotator Rotation(0.f, Yaw, 0.f);
	const FVector CellCenter = GridToWorldLocation(Cell.X, Cell.Y) + FVector(0.f, 0.f, ZOffset);

	const FVector LocalCenter(Bounds.Origin.X * Scale.X, Bounds.Origin.Y * Scale.Y, Bounds.Origin.Z * Scale.Z);
	const FVector RotatedCenter = Rotation.RotateVector(LocalCenter);
	const FVector PivotLocation = CellCenter - FVector(RotatedCenter.X, RotatedCenter.Y, 0.f);

	ISM->AddInstance(FTransform(Rotation, PivotLocation, Scale));
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

	static const FIntPoint Cardinals[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	const float PathZ = FMath::Max(PathTileZOffset, GroundTileZOffset + 4.f);
	constexpr float PathFit = 0.995f;

	auto PickTurnISM = [this](int32 CrossZ) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Preferred = (CrossZ > 0) ? TurnRightTileISM : TurnLeftTileISM;
		if (Preferred && Preferred->GetStaticMesh())
		{
			return Preferred;
		}
		Preferred = (CrossZ > 0) ? TurnLeftTileISM : TurnRightTileISM;
		if (Preferred && Preferred->GetStaticMesh())
		{
			return Preferred;
		}
		return StraightTileISM;
	};

	for (const FIntPoint& Cell : PathCellSet)
	{
		TArray<FIntPoint, TInlineAllocator<4>> NeighborDirs;
		for (const FIntPoint& Dir : Cardinals)
		{
			if (PathCellSet.Contains(Cell + Dir))
			{
				NeighborDirs.Add(Dir);
			}
		}

		UInstancedStaticMeshComponent* TileISM = StraightTileISM;
		float Yaw = PathTileYawOffset;

		if (NeighborDirs.Num() == 0)
		{
			Yaw = PathTileYawOffset;
			TileISM = StraightTileISM;
		}
		else if (NeighborDirs.Num() == 1 ||
			(NeighborDirs.Num() == 2 && NeighborDirs[0] + NeighborDirs[1] == FIntPoint(0, 0)))
		{
			Yaw = YawForDirection(NeighborDirs[0]) + PathTileYawOffset;
			TileISM = StraightTileISM;
		}
		else if (NeighborDirs.Num() == 2)
		{
			FIntPoint InDir(-NeighborDirs[0].X, -NeighborDirs[0].Y);
			FIntPoint OutDir = NeighborDirs[1];
			int32 CrossZ = InDir.X * OutDir.Y - InDir.Y * OutDir.X;
			if (CrossZ == 0)
			{
				InDir = FIntPoint(-NeighborDirs[1].X, -NeighborDirs[1].Y);
				OutDir = NeighborDirs[0];
				CrossZ = InDir.X * OutDir.Y - InDir.Y * OutDir.X;
			}
			Yaw = YawForDirection(InDir) + PathTileYawOffset;
			TileISM = PickTurnISM(CrossZ);
		}
		else
		{
			FIntPoint Stem = NeighborDirs[0];
			for (const FIntPoint& Dir : NeighborDirs)
			{
				if (!NeighborDirs.Contains(FIntPoint(-Dir.X, -Dir.Y)))
				{
					Stem = Dir;
					break;
				}
			}

			Yaw = YawForDirection(Stem) - 90.f + PathTileYawOffset;
			TileISM = (TJunctionTileISM && TJunctionTileISM->GetStaticMesh()) ? TJunctionTileISM : StraightTileISM;
		}

		SpawnTileInstance(TileISM, Cell, Yaw, PathZ, PathFit);
		PlacedTileCells.Add(Cell);
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
	constexpr float GroundFit = 0.99f;

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
			SpawnTileInstance(ChosenISM, Cell, Yaw, GroundTileZOffset, GroundFit);
		}
	}

	if (bHideGroundMeshVisualWhenTilesActive)
	{
		ProceduralMesh->SetVisibility(false);
	}
}