#include "DefenderMineShaft.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralTerrain.h"
#include "TDGameState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "TDGameInstance.h"

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
	PayoutInterval = 12.f;
	if (const UTDGameInstance* GI = Cast<UTDGameInstance>(GetGameInstance()))
	{
		if (GI->GetDifficulty() == ETDDifficulty::Medium)
		{
			PayoutInterval = 20.f;
		}
		else if (GI->GetDifficulty() == ETDDifficulty::Hard)
		{
			PayoutInterval = 30.f;
		}
	}

	Super::BeginPlay();
	RestartIncomeTimer();
}

void ADefenderMineShaft::ApplyPayoutUpgrade()
{
	PayoutInterval = FMath::Max(4.f, PayoutInterval - 3.f);
	RestartIncomeTimer();
}

void ADefenderMineShaft::ApplyRicherUpgrade()
{
	CoinsPerPayout += 10;
}

void ADefenderMineShaft::ApplyUpgradeVisual()
{
	if (!DefenderMesh)
	{
		return;
	}

	UStaticMesh* Cart = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Buildings/Cart.Cart"));
	if (!Cart)
	{
		ShowUpgradeRing(1);
		return;
	}

	const FVector FootprintAnchor = GetMeshFootprintAnchor();
	const FVector OldScale = DefenderMesh->GetComponentScale();
	const FBox OldBox = DefenderMesh->GetStaticMesh() ? DefenderMesh->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
	const float OldWidth = FMath::Max(OldBox.GetSize().X * OldScale.X, OldBox.GetSize().Y * OldScale.Y);
	const float OldHeight = FMath::Max(OldBox.GetSize().Z * OldScale.Z, 1.f);
	DefenderMesh->SetStaticMesh(Cart);
	const FBox NewBox = Cart->GetBoundingBox();
	const float NewWidth = FMath::Max(FMath::Max(NewBox.GetSize().X, NewBox.GetSize().Y), 1.f);
	const float NewHeight = FMath::Max(NewBox.GetSize().Z, 1.f);
	DefenderMesh->SetWorldScale3D(FVector(OldWidth / NewWidth, OldWidth / NewWidth, OldHeight / NewHeight));
	RefreshMeshPlacementAfterSwap(FootprintAnchor);
}

void ADefenderMineShaft::RestartIncomeTimer()
{
	if (!GetWorld())
	{
		return;
	}

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
