#include "DefenderMineShaft.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralTerrain.h"
#include "TDGameState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ADefenderMineShaft::ADefenderMineShaft()
{
	MaxHealth = 200.f;
	AttackDamage = 0.f;
	AttackRange = 0.f;
	CoinsPerPayout = 25;
	PayoutInterval = 12.f;

	if (DefenderMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderMesh.Succeeded())
		{
			DefenderMesh->SetStaticMesh(CylinderMesh.Object);
		}
	}
}

void ADefenderMineShaft::ApplyDefenderMesh()
{
	if (!DefenderMesh)
	{
		return;
	}

	if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		DefenderMesh->SetStaticMesh(CylinderMesh);
	}

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
	}

	const FBox MeshBox = DefenderMesh->GetStaticMesh() ? DefenderMesh->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
	const float MeshWidth = FMath::Max(MeshBox.GetSize().X, 1.f);
	const float MeshHeight = FMath::Max(MeshBox.GetSize().Z, 1.f);
	DefenderMesh->SetWorldScale3D(FVector(
		(TileSize * 0.55f) / MeshWidth,
		(TileSize * 0.55f) / MeshWidth,
		(TileSize * 0.8f) / MeshHeight));

	if (UMaterialInterface* BaseMat = DefenderMesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			const FLinearColor Stone(0.28f, 0.26f, 0.24f);
			DynMat->SetVectorParameterValue(TEXT("Color"), Stone);
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Stone);
			DefenderMesh->SetMaterial(0, DynMat);
		}
	}
}

void ADefenderMineShaft::BeginPlay()
{
	Super::BeginPlay();

	const float Interval = FMath::Max(PayoutInterval, 1.f);
	GetWorldTimerManager().SetTimer(IncomeTimerHandle, this, &ADefenderMineShaft::GenerateIncome, Interval, true, Interval);
}

void ADefenderMineShaft::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(IncomeTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ADefenderMineShaft::ScanAndAttack()
{
}

void ADefenderMineShaft::GenerateIncome()
{
	if (IsDestroyed())
	{
		GetWorldTimerManager().ClearTimer(IncomeTimerHandle);
		return;
	}

	if (ATDGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr)
	{
		GS->AddMoney(FMath::Max(CoinsPerPayout, 0));
	}
}
