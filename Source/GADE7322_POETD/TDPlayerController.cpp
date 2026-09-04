#include "TDPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"

void ATDPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));
}

void ATDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("PlaceDefender"), IE_Pressed, this, &ATDPlayerController::TryPlaceDefender);
		InputComponent->BindAction(TEXT("UpgradeDefender"), IE_Pressed, this, &ATDPlayerController::TryUpgradeDefender);
	}
}

ATDGameState* ATDPlayerController::GetGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr;
}

bool ATDPlayerController::HasEnoughMoney(int32 Cost) const
{
	ATDGameState* GS = GetGameState();
	if (GS)
	{
		return GS->CanAfford(Cost);
	}

	UE_LOG(LogTemp, Warning, TEXT("TDPlayerController: TDGameState not found - falling back to LocalTestingGold (%d)."), LocalTestingGold);
	return LocalTestingGold >= Cost;
}

bool ATDPlayerController::SpendMoney(int32 Cost)
{
	ATDGameState* GS = GetGameState();
	if (GS)
	{
		return GS->SpendMoney(Cost);
	}

	if (LocalTestingGold >= Cost)
	{
		LocalTestingGold -= Cost;
		UE_LOG(LogTemp, Warning, TEXT("TDPlayerController: Spent %d from LocalTestingGold. Remaining: %d"), Cost, LocalTestingGold);
		return true;
	}

	return false;
}

bool ATDPlayerController::FindNearestBuildLocation(const FVector& ClickLocation, FVector& OutLocation, int32& OutIndex) const
{
	if (!TerrainRef)
	{
		return false;
	}

	int32 BestIndex = INDEX_NONE;
	float BestDistSq = FLT_MAX;

	for (int32 i = 0; i < TerrainRef->BuildGridLocations.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared(ClickLocation, TerrainRef->BuildGridLocations[i]);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return false;
	}

	const float SnapRadius = FMath::Max(TerrainRef->TileDimensions.X, TerrainRef->TileDimensions.Y) * 0.5f;
	if (BestDistSq > FMath::Square(SnapRadius))
	{
		return false;
	}

	OutIndex = BestIndex;
	OutLocation = TerrainRef->BuildGridLocations[BestIndex];
	return true;
}

bool ATDPlayerController::IsFarEnoughFromPathways(const FVector& Location) const
{
	if (!TerrainRef)
	{
		return true;
	}

	for (const FVector& Node : TerrainRef->PathwayNodes)
	{
		if (FVector::DistSquared(Location, Node) < FMath::Square(PathExclusionDistance))
		{
			return false;
		}
	}

	return true;
}

void ATDPlayerController::TryPlaceDefender()
{
	FHitResult Hit;
	const bool bHit = GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit);

	if (!bHit)
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("No valid surface under cursor."));
		return;
	}

	if (!TerrainRef)
	{
		TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));
		if (!TerrainRef)
		{
			OnDefenderPlacementFailed.Broadcast(TEXT("No ProceduralTerrain found in level."));
			return;
		}
	}

	FVector SnappedLocation;
	int32 GridIndex;
	if (!FindNearestBuildLocation(Hit.Location, SnappedLocation, GridIndex))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Clicked location is not near a valid build cell."));
		return;
	}

	if (OccupiedGridIndices.Contains(GridIndex))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("This grid cell is already occupied."));
		return;
	}

	if (!IsFarEnoughFromPathways(SnappedLocation))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Too close to a pathway."));
		return;
	}

	if (!HasEnoughMoney(DefenderCost))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Not enough gold."));
		return;
	}

	if (!DefenderClass)
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("No DefenderClass assigned."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADefenderBase* NewDefender = GetWorld()->SpawnActor<ADefenderBase>(DefenderClass, SnappedLocation, FRotator::ZeroRotator, SpawnParams);
	if (!NewDefender)
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Failed to spawn defender."));
		return;
	}

	SpendMoney(DefenderCost);
	OccupiedGridIndices.Add(GridIndex);
	DefenderUpgradeLevels.Add(NewDefender, 0);

	UE_LOG(LogTemp, Log, TEXT("TDPlayerController: Placed defender at grid index %d."), GridIndex);
	OnDefenderPlacementSucceeded.Broadcast(NewDefender);
}

void ATDPlayerController::TryUpgradeDefender()
{
	FHitResult Hit;
	const bool bHit = GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit);

	if (!bHit || !Hit.GetActor())
	{
		return;
	}

	ADefenderBase* ClickedDefender = Cast<ADefenderBase>(Hit.GetActor());
	if (!ClickedDefender)
	{
		UE_LOG(LogTemp, Log, TEXT("TDPlayerController: Clicked actor is not a defender."));
		return;
	}

	int32* LevelPtr = DefenderUpgradeLevels.Find(ClickedDefender);
	if (!LevelPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("TDPlayerController: Clicked defender not tracked."));
		return;
	}

	const int32 CurrentLevel = *LevelPtr;
	if (CurrentLevel >= MaxUpgradeLevel)
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Defender is already at maximum upgrade level."));
		return;
	}

	if (!HasEnoughMoney(UpgradeCost))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Not enough gold to upgrade."));
		return;
	}

	SpendMoney(UpgradeCost);

	ClickedDefender->MaxHealth += UpgradeHealthBonus;
	ClickedDefender->CurrentHealth = FMath::Min(ClickedDefender->CurrentHealth + UpgradeHealthBonus, ClickedDefender->MaxHealth);
	ClickedDefender->AttackDamage += UpgradeDamageBonus;

	const int32 NewLevel = CurrentLevel + 1;
	DefenderUpgradeLevels[ClickedDefender] = NewLevel;

	UE_LOG(LogTemp, Log, TEXT("TDPlayerController: Defender upgraded to level %d. MaxHealth: %.0f, AttackDamage: %.1f"),
		NewLevel, ClickedDefender->MaxHealth, ClickedDefender->AttackDamage);

	OnDefenderUpgraded.Broadcast(ClickedDefender, NewLevel);
}