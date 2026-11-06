#include "DefenderMineShaft.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
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
		static ConstructorHelpers::FObjectFinder<UStaticMesh> MineMesh(TEXT("/Game/Buildings/MineT1.MineT1"));
		if (MineMesh.Succeeded())
		{
			DefenderMesh->SetStaticMesh(MineMesh.Object);
		}
	}
}

void ADefenderMineShaft::ApplyMineMesh(const TCHAR* MeshPath, bool bKeepFootprint)
{
	if (!DefenderMesh)
	{
		return;
	}

	UStaticMesh* MineMesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!MineMesh)
	{
		return;
	}

	const FVector FootprintAnchor = GetMeshFootprintAnchor();
	DefenderMesh->SetStaticMesh(MineMesh);

	float TileSize = 200.f;
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		TileSize = FMath::Max(Terrain->TileDimensions.X, Terrain->TileDimensions.Y);
	}

	const FBox MeshBox = MineMesh->GetBoundingBox();
	const float MeshWidth = FMath::Max(FMath::Max(MeshBox.GetSize().X, MeshBox.GetSize().Y), 1.f);
	const float Uniform = (TileSize * 0.7f) / MeshWidth;
	DefenderMesh->SetWorldScale3D(FVector(Uniform));

	if (bKeepFootprint)
	{
		RefreshMeshPlacementAfterSwap(FootprintAnchor);
	}
}

void ADefenderMineShaft::ApplyDefenderMesh()
{
	ApplyMineMesh(bUsingAdvancedMineMesh
		? TEXT("/Game/Buildings/MineT2.MineT2")
		: TEXT("/Game/Buildings/MineT1.MineT1"), false);
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

	ShowUpgradeRing(1);
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

	const int32 Payout = FMath::Max(CoinsPerPayout, 0);
	CoinsProduced += Payout;

	if (ATDGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr)
	{
		GS->AddMoney(Payout);
	}

	if (!bUsingAdvancedMineMesh && CoinsProduced >= 100)
	{
		bUsingAdvancedMineMesh = true;
		ApplyMineMesh(TEXT("/Game/Buildings/MineT2.MineT2"), true);
	}
}
