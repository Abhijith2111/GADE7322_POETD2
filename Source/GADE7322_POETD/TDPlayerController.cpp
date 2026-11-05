#include "TDPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/EngineTypes.h"
#include "TDGameMode.h"
#include "GameOverViewInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "CentralTowerBase.h"
#include "DefenderBog.h"
#include "DefenderBarracks.h"
#include "DefenderKnight.h"
#include "DefenderMineShaft.h"
#include "EnemySpawner.h"
#include "UpgradeMenuWidget.h"
#include "DifficultyBriefingWidget.h"
#include "WeatherStatusWidget.h"
#include "GameVFX.h"
#include "WeatherManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

namespace
{
	void ApplyReadableFont(UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
			if (Font)
			{
				const FSlateFontInfo Current = Text->GetFont();
				const FName Face = Current.TypefaceFontName.IsNone() ? FName(TEXT("Bold")) : Current.TypefaceFontName;
				const float Size = Current.Size > 1.f ? Current.Size : 22.f;
				Text->SetFont(FSlateFontInfo(Font, Size, Face));
			}
			if (Cast<UButton>(Text->GetParent()))
			{
				Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
			}
		}

		if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
		{
			ApplyReadableFont(Nested->GetRootWidget());
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				ApplyReadableFont(Panel->GetChildAt(ChildIndex));
			}
		}
	}
}

ATDPlayerController::ATDPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	static ConstructorHelpers::FClassFinder<ADefenderBase> DefenderBP(TEXT("/Game/Gameplay/LevelObjects/BP_DefenderBase"));
	if (DefenderBP.Succeeded())
	{
		DefenderClass = DefenderBP.Class;
	}
}

void ATDPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bIsPaused = false;
	bInMainMenu = false;

	const bool bOpenOnMenu = ShouldShowMainMenu();
	if (!bOpenOnMenu)
	{
		if (UWorld* World = GetWorld())
		{
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->SetPauserPlayerState(nullptr);
			}
			UGameplayStatics::SetGamePaused(World, false);
		}
		SetPause(false);

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		if (APawn* ControlledPawn = GetPawn())
		{
			ControlledPawn->EnableInput(this);
		}
	}

	TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));

	EnsureUIClasses();

	if (HUDClass)
	{
		HUDInstance = CreateWidget<UUserWidget>(this, HUDClass);
		if (HUDInstance)
		{
			ApplyReadableFont(HUDInstance);
			HUDInstance->AddToViewport(0);
		}
	}

	WeatherStatusInstance = CreateWidget<UWeatherStatusWidget>(this, UWeatherStatusWidget::StaticClass());
	if (WeatherStatusInstance)
	{
		WeatherStatusInstance->AddToViewport(1);
		WeatherStatusInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (ATDGameMode* GM = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->OnLoss.AddDynamic(this, &ATDPlayerController::HandleGameLoss);
		GM->OnVictory.AddDynamic(this, &ATDPlayerController::HandleGameVictory);
	}

	OverviewCameraAttempts = 0;
	PlaceOverviewCamera();

	if (bOpenOnMenu)
	{
		ShowMainMenu();
	}
	else
	{
		ShowWeatherStatus();
	}
}

void ATDPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		InPawn->SetActorEnableCollision(false);
		if (UPawnMovementComponent* Movement = InPawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}

	OverviewCameraAttempts = 0;
	GetWorldTimerManager().SetTimer(OverviewCameraHandle, this, &ATDPlayerController::PlaceOverviewCamera, 0.1f, false);
}

void ATDPlayerController::PlaceOverviewCamera()
{
	if (!TerrainRef)
	{
		TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));
	}

	FVector FocusPoint = FVector::ZeroVector;
	bool bHaveFocus = false;

	if (TerrainRef)
	{
		FocusPoint = TerrainRef->CentralTowerLocation;
		bHaveFocus = true;
	}
	else if (ACentralTowerBase* Tower = Cast<ACentralTowerBase>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ACentralTowerBase::StaticClass())))
	{
		FocusPoint = Tower->GetActorLocation();
		bHaveFocus = true;
	}

	if (!bHaveFocus || !GetPawn())
	{
		if (OverviewCameraAttempts < 10)
		{
			++OverviewCameraAttempts;
			GetWorldTimerManager().SetTimer(OverviewCameraHandle, this, &ATDPlayerController::PlaceOverviewCamera, 0.2f, false);
		}
		return;
	}

	const float Height = (OverviewCameraHeight > 2200.f)
		? 1400.f
		: FMath::Clamp(OverviewCameraHeight, 800.f, 2000.f);
	const FVector CameraLocation = FocusPoint + FVector(-Height * 0.3f, 0.f, Height);
	const FRotator CameraRotation(OverviewCameraPitch, 0.f, 0.f);

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->SetActorEnableCollision(false);
		ControlledPawn->SetActorLocationAndRotation(CameraLocation, CameraRotation, false, nullptr, ETeleportType::TeleportPhysics);
		if (UPawnMovementComponent* Movement = ControlledPawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
			Movement->Velocity = FVector::ZeroVector;
		}
	}

	SetControlRotation(CameraRotation);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -89.f;
		PlayerCameraManager->ViewPitchMax = -20.f;
		PlayerCameraManager->SetGameCameraCutThisFrame();
	}

	ClampOverviewCamera();
}

void ATDPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bUpgradeCameraActive)
	{
		ClampOverviewCamera();
	}
}

void ATDPlayerController::ClampOverviewCamera()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	if (!TerrainRef)
	{
		TerrainRef = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass()));
	}
	if (!TerrainRef)
	{
		return;
	}

	const float StepX = TerrainRef->TileDimensions.X + TerrainRef->TileSpacing;
	const float StepY = TerrainRef->TileDimensions.Y + TerrainRef->TileSpacing;
	const FVector Origin = TerrainRef->GetActorLocation();
	const float BoardX = TerrainRef->GridWidth * StepX;
	const float BoardY = TerrainRef->GridHeight * StepY;
	const float MarginX = BoardX * 0.12f;
	const float MarginY = BoardY * 0.12f;
	const float BoardSpan = FMath::Max(BoardX, BoardY);
	const float MinZ = Origin.Z + 280.f;
	const float MaxHeightAbove = FMath::Clamp(BoardSpan * 0.65f, OverviewCameraHeight, OverviewCameraHeight * 2.2f);
	const float MaxZ = Origin.Z + MaxHeightAbove;

	FVector Loc = ControlledPawn->GetActorLocation();
	const FVector Clamped(
		FMath::Clamp(Loc.X, Origin.X - MarginX, Origin.X + BoardX + MarginX),
		FMath::Clamp(Loc.Y, Origin.Y - MarginY, Origin.Y + BoardY + MarginY),
		FMath::Clamp(Loc.Z, MinZ, MaxZ));

	if (Clamped.Equals(Loc, 0.5f))
	{
		return;
	}

	ControlledPawn->SetActorLocation(Clamped, false, nullptr, ETeleportType::TeleportPhysics);
	if (UPawnMovementComponent* Movement = ControlledPawn->GetMovementComponent())
	{
		FVector Velocity = Movement->Velocity;
		if (!FMath::IsNearlyEqual(Clamped.X, Loc.X))
		{
			Velocity.X = 0.f;
		}
		if (!FMath::IsNearlyEqual(Clamped.Y, Loc.Y))
		{
			Velocity.Y = 0.f;
		}
		if (!FMath::IsNearlyEqual(Clamped.Z, Loc.Z))
		{
			Velocity.Z = 0.f;
		}
		Movement->Velocity = Velocity;
	}
}

void ATDPlayerController::RestartMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		Settings->SetPauserPlayerState(nullptr);
	}
	UGameplayStatics::SetGamePaused(World, false);
	SetPause(false);
	bIsPaused = false;

	if (GameOverInstance)
	{
		GameOverInstance->RemoveFromParent();
		GameOverInstance = nullptr;
	}
	if (PauseMenuInstance)
	{
		PauseMenuInstance->RemoveFromParent();
		PauseMenuInstance = nullptr;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	UGameplayStatics::OpenLevel(this, FName(*LevelName), true, TEXT("SkipMenu=1"));
}

void ATDPlayerController::StartMatchFromMenu(ETDDifficulty Difficulty)
{
	if (UTDGameInstance* TDInstance = Cast<UTDGameInstance>(GetGameInstance()))
	{
		TDInstance->SetDifficulty(Difficulty);
	}

	bInMainMenu = false;
	if (MainMenuInstance)
	{
		MainMenuInstance->RemoveFromParent();
		MainMenuInstance = nullptr;
	}

	if (HUDInstance)
	{
		HUDInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	HideWeatherStatus();

	bIsPaused = true;
	SetPause(true);

	if (!DifficultyBriefingInstance)
	{
		DifficultyBriefingInstance = CreateWidget<UDifficultyBriefingWidget>(this, UDifficultyBriefingWidget::StaticClass());
		if (DifficultyBriefingInstance)
		{
			DifficultyBriefingInstance->AddToViewport(110);
		}
	}
	if (DifficultyBriefingInstance)
	{
		DifficultyBriefingInstance->ShowForDifficulty(Difficulty, this);
	}

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (DifficultyBriefingInstance)
	{
		InputMode.SetWidgetToFocus(DifficultyBriefingInstance->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->DisableInput(this);
	}
}

void ATDPlayerController::ConfirmDifficultyBriefing()
{
	if (DifficultyBriefingInstance)
	{
		DifficultyBriefingInstance->RemoveFromParent();
		DifficultyBriefingInstance = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		if (AWorldSettings* Settings = World->GetWorldSettings())
		{
			Settings->SetPauserPlayerState(nullptr);
		}
		UGameplayStatics::SetGamePaused(World, false);
	}
	SetPause(false);
	bIsPaused = false;
	bInMainMenu = false;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->EnableInput(this);
	}
	if (HUDInstance)
	{
		HUDInstance->SetVisibility(ESlateVisibility::Visible);
	}

	PlaceOverviewCamera();

	TArray<AActor*> Spawners;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemySpawner::StaticClass(), Spawners);
	for (AActor* Actor : Spawners)
	{
		if (AEnemySpawner* Spawner = Cast<AEnemySpawner>(Actor))
		{
			Spawner->StartSpawning();
		}
	}

	ShowWeatherStatus();
}

void ATDPlayerController::ReturnToMainMenu()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		Weather->StopWeatherCycle();
	}

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		Settings->SetPauserPlayerState(nullptr);
	}
	UGameplayStatics::SetGamePaused(World, false);
	SetPause(false);
	bIsPaused = false;
	bInMainMenu = false;

	if (GameOverInstance)
	{
		GameOverInstance->RemoveFromParent();
		GameOverInstance = nullptr;
	}
	if (PauseMenuInstance)
	{
		PauseMenuInstance->RemoveFromParent();
		PauseMenuInstance = nullptr;
	}
	if (MainMenuInstance)
	{
		MainMenuInstance->RemoveFromParent();
		MainMenuInstance = nullptr;
	}
	if (DifficultyBriefingInstance)
	{
		DifficultyBriefingInstance->RemoveFromParent();
		DifficultyBriefingInstance = nullptr;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void ATDPlayerController::EnsureUIClasses()
{
	auto LoadUI = [](TSubclassOf<UUserWidget>& Slot, const TCHAR* Path)
	{
		if (Slot)
		{
			return;
		}

		Slot = LoadClass<UUserWidget>(nullptr, Path);
		if (!Slot)
		{
			UE_LOG(LogTemp, Warning, TEXT("TDPlayerController: Missing UI widget %s"), Path);
		}
	};

	LoadUI(HUDClass, TEXT("/Game/UI/WBP_TDHUD.WBP_TDHUD_C"));
	LoadUI(PauseMenuClass, TEXT("/Game/UI/WBP_PauseMenu.WBP_PauseMenu_C"));
	LoadUI(GameOverClass, TEXT("/Game/UI/WBP_GameOver.WBP_GameOver_C"));
	LoadUI(MainMenuClass, TEXT("/Game/UI/WBP_MainMenu.WBP_MainMenu_C"));
}

void ATDPlayerController::ShowMainMenu()
{
	CloseUpgradeMenu();
	EnsureUIClasses();
	if (!MainMenuInstance)
	{
		MainMenuInstance = CreateWidget<UUserWidget>(this, MainMenuClass);
		ApplyReadableFont(MainMenuInstance);
	}
	if (MainMenuInstance && !MainMenuInstance->IsInViewport())
	{
		MainMenuInstance->AddToViewport(100);
	}
	if (HUDInstance)
	{
		HUDInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	HideWeatherStatus();

	bInMainMenu = true;
	bIsPaused = true;
	SetPause(true);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (MainMenuInstance)
	{
		InputMode.SetWidgetToFocus(MainMenuInstance->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

bool ATDPlayerController::ShouldShowMainMenu() const
{
	const ATDGameMode* GM = Cast<ATDGameMode>(UGameplayStatics::GetGameMode(this));
	return !GM || !GM->bSkipMainMenu;
}

void ATDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("PlaceDefender"), IE_Pressed, this, &ATDPlayerController::TryPlaceDefender);
		InputComponent->BindAction(TEXT("UpgradeDefender"), IE_Pressed, this, &ATDPlayerController::TryUpgradeDefender);
		InputComponent->BindAction(TEXT("PauseGame"), IE_Pressed, this, &ATDPlayerController::TogglePause);
	}
}

void ATDPlayerController::TogglePause()
{
	SetPausedState(!bIsPaused);
}

void ATDPlayerController::ResumeFromPause()
{
	SetPausedState(false);
}

void ATDPlayerController::SetPausedState(bool bPause)
{
	if (bInMainMenu)
	{
		return;
	}

	if (GameOverInstance && GameOverInstance->IsInViewport())
	{
		return;
	}

	bIsPaused = bPause;
	SetPause(bIsPaused);

	if (bIsPaused)
	{
		if (!PauseMenuInstance && PauseMenuClass)
		{
			PauseMenuInstance = CreateWidget<UUserWidget>(this, PauseMenuClass);
			ApplyReadableFont(PauseMenuInstance);
		}

		if (PauseMenuInstance)
		{
			if (!PauseMenuInstance->IsInViewport())
			{
				PauseMenuInstance->AddToViewport(10);
			}
			PauseMenuInstance->SetVisibility(ESlateVisibility::Visible);
		}

		FInputModeGameAndUI InputMode;
		if (PauseMenuInstance)
		{
			InputMode.SetWidgetToFocus(PauseMenuInstance->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else
	{
		if (PauseMenuInstance)
		{
			PauseMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
		}

		FInputModeGameAndUI InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}

	UE_LOG(LogTemp, Log, TEXT("TDPlayerController: Game %s"), bIsPaused ? TEXT("paused") : TEXT("resumed"));
}

bool ATDPlayerController::CanAffordCost(int32 Cost) const
{
	return HasEnoughMoney(Cost);
}

void ATDPlayerController::SetPendingDefender(TSubclassOf<ADefenderBase> InDefenderClass, int32 InCost)
{
	DefenderClass = InDefenderClass;
	DefenderCost = InCost;
	UE_LOG(LogTemp, Log, TEXT("TDPlayerController: Pending defender set. Cost: %d"), InCost);
}

void ATDPlayerController::TogglePauseMenu()
{
	TogglePause();
}

void ATDPlayerController::HandleGameLoss()
{
	ShowGameOver(false);
}

void ATDPlayerController::HandleGameVictory()
{
	ShowGameOver(true);
}

void ATDPlayerController::ShowWeatherStatus()
{
	if (!WeatherStatusInstance)
	{
		WeatherStatusInstance = CreateWidget<UWeatherStatusWidget>(this, UWeatherStatusWidget::StaticClass());
		if (WeatherStatusInstance)
		{
			WeatherStatusInstance->AddToViewport(1);
		}
	}
	if (!WeatherStatusInstance)
	{
		return;
	}

	WeatherStatusInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		WeatherStatusInstance->BindToWeather(Weather);
		GetWorldTimerManager().ClearTimer(WeatherStatusBindHandle);
	}
	else if (GetWorld())
	{
		GetWorldTimerManager().SetTimer(WeatherStatusBindHandle, this, &ATDPlayerController::RetryBindWeatherStatus, 0.1f, true);
	}
}

void ATDPlayerController::HideWeatherStatus()
{
	GetWorldTimerManager().ClearTimer(WeatherStatusBindHandle);
	if (WeatherStatusInstance)
	{
		WeatherStatusInstance->UnbindFromWeather();
		WeatherStatusInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ATDPlayerController::RetryBindWeatherStatus()
{
	if (AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		if (WeatherStatusInstance)
		{
			WeatherStatusInstance->BindToWeather(Weather);
			WeatherStatusInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		GetWorldTimerManager().ClearTimer(WeatherStatusBindHandle);
	}
}

void ATDPlayerController::ShowGameOver(bool bVictory)
{
	if (AWeatherManager* Weather = AWeatherManager::Get(this))
	{
		Weather->StopWeatherCycle();
	}
	HideWeatherStatus();

	SetPause(true);
	bIsPaused = true;

	EnsureUIClasses();

	if (!GameOverInstance && GameOverClass)
	{
		GameOverInstance = CreateWidget<UUserWidget>(this, GameOverClass);
		ApplyReadableFont(GameOverInstance);
	}

	if (GameOverInstance && GameOverInstance->GetClass()->ImplementsInterface(UGameOverViewInterface::StaticClass()))
	{
		IGameOverViewInterface::Execute_ShowResult(GameOverInstance, bVictory);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TDPlayerController: Game over widget is missing ShowResult."));
	}

	if (GameOverInstance)
	{
		GameOverInstance->AddToViewport(20);
	}

	FInputModeUIOnly InputMode;
	if (GameOverInstance)
	{
		InputMode.SetWidgetToFocus(GameOverInstance->TakeWidget());
	}
	SetInputMode(InputMode);
	bShowMouseCursor = true;
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

	return TerrainRef->FindBuildSlotAtWorld(ClickLocation, OutIndex, OutLocation);
}

bool ATDPlayerController::IsFarEnoughFromPathways(const FVector& Location) const
{
	if (!TerrainRef)
	{
		return true;
	}

	for (const FVector& Node : TerrainRef->PathwayNodes)
	{
		if (FVector::DistSquared2D(Location, Node) < FMath::Square(PathExclusionDistance))
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

	ADefenderBase* ClickedDefender = nullptr;
	if (bHit && Hit.GetActor())
	{
		ClickedDefender = Cast<ADefenderBase>(Hit.GetActor());
	}

	if (ClickedDefender && IsUpgradableDefender(ClickedDefender))
	{
		OpenUpgradeMenu(ClickedDefender);
		return;
	}

	if (SelectedUpgradeTarget.IsValid())
	{
		CloseUpgradeMenu();
		return;
	}

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
	if (!FindNearestBuildLocation(Hit.ImpactPoint, SnappedLocation, GridIndex))
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

	if (DefenderClass->IsChildOf(ADefenderBog::StaticClass())
		&& TerrainRef
		&& (TerrainRef->IsWorldOnPath(Hit.ImpactPoint) || TerrainRef->IsWorldOnPath(SnappedLocation)))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Bogs cannot be placed on the path."));
		return;
	}

	if (DefenderClass->IsChildOf(ADefenderMineShaft::StaticClass()))
	{
		TArray<AActor*> ExistingMines;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderMineShaft::StaticClass(), ExistingMines);
		for (AActor* Actor : ExistingMines)
		{
			const ADefenderMineShaft* Mine = Cast<ADefenderMineShaft>(Actor);
			if (Mine && !Mine->IsDestroyed())
			{
				OnDefenderPlacementFailed.Broadcast(TEXT("Only one mine shaft can be placed."));
				return;
			}
		}
	}

	if (DefenderClass->IsChildOf(ADefenderBog::StaticClass()) && IsBogCapReached())
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Bog limit reached for this difficulty."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector DefenderSpawnPoint = SnappedLocation;
	{
		const FVector TraceStart(SnappedLocation.X, SnappedLocation.Y, SnappedLocation.Z + 5000.f);
		const FVector TraceEnd(SnappedLocation.X, SnappedLocation.Y, SnappedLocation.Z - 10000.f);
		FHitResult GroundHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DefenderSpawnGround), true);
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params)
			|| GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			DefenderSpawnPoint.Z = GroundHit.ImpactPoint.Z + 2.f;
		}
	}

	ADefenderBase* NewDefender = GetWorld()->SpawnActor<ADefenderBase>(DefenderClass, DefenderSpawnPoint, FRotator::ZeroRotator, SpawnParams);
	if (!NewDefender)
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Failed to spawn defender."));
		return;
	}

	SpendMoney(DefenderCost);
	OccupiedGridIndices.Add(GridIndex);
	DefenderGridIndices.Add(NewDefender, GridIndex);
	DefenderUpgradeLevels.Add(NewDefender, 0);
	NewDefender->OnDefenderDestroyed.AddDynamic(this, &ATDPlayerController::HandleDefenderDestroyed);

	UE_LOG(LogTemp, Log, TEXT("TDPlayerController: Placed defender at grid index %d."), GridIndex);
	OnDefenderPlacementSucceeded.Broadcast(NewDefender);
}

void ATDPlayerController::HandleDefenderDestroyed(ADefenderBase* DestroyedDefender)
{
	if (!DestroyedDefender)
	{
		return;
	}

	if (const int32* GridIndex = DefenderGridIndices.Find(DestroyedDefender))
	{
		OccupiedGridIndices.Remove(*GridIndex);
		DefenderGridIndices.Remove(DestroyedDefender);
	}

	DefenderUpgradeLevels.Remove(DestroyedDefender);

	if (SelectedUpgradeTarget.Get() == DestroyedDefender)
	{
		CloseUpgradeMenu();
	}
}

void ATDPlayerController::TryUpgradeDefender()
{
}

void ATDPlayerController::ApplyDefenderButtonVisual(UButton* Button, TSubclassOf<ADefenderBase> InDefenderClass, int32 Cost)
{
	if (!Button)
	{
		return;
	}

	bool bLocked = false;
	if (InDefenderClass && InDefenderClass->IsChildOf(ADefenderMineShaft::StaticClass()))
	{
		TArray<AActor*> ExistingMines;
		UGameplayStatics::GetAllActorsOfClass(this, ADefenderMineShaft::StaticClass(), ExistingMines);
		for (AActor* Actor : ExistingMines)
		{
			const ADefenderMineShaft* Mine = Cast<ADefenderMineShaft>(Actor);
			if (Mine && !Mine->IsDestroyed())
			{
				bLocked = true;
				break;
			}
		}
	}
	else if (InDefenderClass && InDefenderClass->IsChildOf(ADefenderBog::StaticClass()) && IsBogCapReached())
	{
		bLocked = true;
	}

	const ATDGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr;
	const int32 Money = GameState ? GameState->GetCurrentMoney() : 0;
	const bool bCanAfford = InDefenderClass && Money >= Cost && !bLocked;

	FLinearColor ButtonColor(1.f, 0.55f, 0.05f, 1.f);
	FLinearColor LabelColor(0.05f, 0.05f, 0.05f, 1.f);
	if (bLocked)
	{
		ButtonColor = FLinearColor(0.28f, 0.28f, 0.28f, 1.f);
		LabelColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.f);
	}
	else if (!bCanAfford)
	{
		ButtonColor = FLinearColor(0.45f, 0.06f, 0.05f, 1.f);
		LabelColor = FLinearColor::White;
	}

	Button->SetBackgroundColor(ButtonColor);
	Button->SetIsEnabled(!bLocked);
	if (UTextBlock* Caption = Cast<UTextBlock>(Button->GetChildAt(0)))
	{
		Caption->SetColorAndOpacity(FSlateColor(LabelColor));
	}
}

bool ATDPlayerController::IsUpgradableDefender(const ADefenderBase* Defender) const
{
	if (!Defender || Defender->IsDestroyed())
	{
		return false;
	}
	if (Defender->IsA(ADefenderKnight::StaticClass()) || Defender->IsA(ADefenderMineShaft::StaticClass()))
	{
		return false;
	}
	return Defender->IsA(ADefenderBog::StaticClass())
		|| Defender->IsA(ADefenderBarracks::StaticClass())
		|| Defender->IsA(ADefenderBase::StaticClass());
}

int32 ATDPlayerController::GetUpgradeCostFor(const ADefenderBase* Defender) const
{
	if (!Defender)
	{
		return 0;
	}
	if (Defender->IsA(ADefenderBog::StaticClass()))
	{
		return 200;
	}
	if (Defender->IsA(ADefenderBarracks::StaticClass()))
	{
		return 150;
	}
	return 100;
}

int32 ATDPlayerController::GetMaxUpgradeLevel(const ADefenderBase* Defender) const
{
	if (!Defender)
	{
		return 1;
	}
	if (Defender->IsA(ADefenderBog::StaticClass()) || Defender->IsA(ADefenderBarracks::StaticClass()))
	{
		return 3;
	}
	if (IsArcherDefender(Defender))
	{
		return 2;
	}
	return 1;
}

bool ATDPlayerController::IsArcherDefender(const ADefenderBase* Defender) const
{
	return Defender
		&& !Defender->IsA(ADefenderBog::StaticClass())
		&& !Defender->IsA(ADefenderBarracks::StaticClass())
		&& !Defender->IsA(ADefenderKnight::StaticClass())
		&& !Defender->IsA(ADefenderMineShaft::StaticClass());
}

FText ATDPlayerController::GetUpgradeBonusText(const ADefenderBase* Defender) const
{
	if (!Defender)
	{
		return FText::GetEmpty();
	}

	const int32* LevelPtr = DefenderUpgradeLevels.Find(Defender);
	const int32 Level = LevelPtr ? *LevelPtr : 0;
	if (Level >= GetMaxUpgradeLevel(Defender))
	{
		return NSLOCTEXT("Upgrade", "FullyUpgraded", "Fully upgraded");
	}

	if (Defender->IsA(ADefenderBog::StaticClass()))
	{
		if (Level <= 0)
		{
			return NSLOCTEXT("Upgrade", "BogBonus", "Level 1: digestion 3 seconds faster");
		}
		if (Level == 1)
		{
			return NSLOCTEXT("Upgrade", "BogBonus2", "Level 2: digestion 5 seconds faster");
		}
		return NSLOCTEXT("Upgrade", "BogBonus3", "Level 3: +50 health. 30% chance an eaten enemy becomes an ally.");
	}
	if (Defender->IsA(ADefenderBarracks::StaticClass()))
	{
		if (Level <= 0)
		{
			return NSLOCTEXT("Upgrade", "BarracksBonus", "Level 1: knights train 3 seconds faster");
		}
		if (Level == 1)
		{
			return NSLOCTEXT("Upgrade", "BarracksBonus2", "Level 2: knights train 3 seconds faster, and new knights hit for 1 more damage");
		}
		return NSLOCTEXT("Upgrade", "BarracksBonus3", "Level 3: new knights hit for 2 more damage and have 50 more health");
	}
	if (IsArcherDefender(Defender) && Level == 1)
	{
		return NSLOCTEXT("Upgrade", "ArcherChoice", "Choose one. Health and attack speed, or attack power.");
	}
	return NSLOCTEXT("Upgrade", "ArcherBonus", "Level 1: +20 health and +5 damage");
}

int32 ATDPlayerController::GetBogCap() const
{
	if (const UTDGameInstance* GI = Cast<UTDGameInstance>(GetGameInstance()))
	{
		if (GI->GetDifficulty() == ETDDifficulty::Medium)
		{
			return 5;
		}
		if (GI->GetDifficulty() == ETDDifficulty::Hard)
		{
			return 3;
		}
	}
	return 0;
}

int32 ATDPlayerController::CountLivingBogs() const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADefenderBog::StaticClass(), Found);
	int32 Count = 0;
	for (AActor* Actor : Found)
	{
		const ADefenderBog* Bog = Cast<ADefenderBog>(Actor);
		if (Bog && !Bog->IsDestroyed())
		{
			++Count;
		}
	}
	return Count;
}

bool ATDPlayerController::IsBogCapReached() const
{
	const int32 Cap = GetBogCap();
	return Cap > 0 && CountLivingBogs() >= Cap;
}

void ATDPlayerController::ZoomToDefender(ADefenderBase* Defender)
{
	if (!Defender || !GetPawn())
	{
		return;
	}

	const FVector Focus = Defender->GetActorLocation();
	const float Height = 520.f;
	const FVector CameraLocation = Focus + FVector(-Height * 0.55f, 0.f, Height);
	const FRotator CameraRotation = (Focus - CameraLocation).Rotation();

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->SetActorEnableCollision(false);
		ControlledPawn->SetActorLocationAndRotation(CameraLocation, CameraRotation, false, nullptr, ETeleportType::TeleportPhysics);
		if (UPawnMovementComponent* Movement = ControlledPawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
			Movement->Velocity = FVector::ZeroVector;
		}
	}

	SetControlRotation(CameraRotation);
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -89.f;
		PlayerCameraManager->ViewPitchMax = 0.f;
		PlayerCameraManager->SetGameCameraCutThisFrame();
	}
	bUpgradeCameraActive = true;
}

void ATDPlayerController::OpenUpgradeMenu(ADefenderBase* Defender)
{
	if (!Defender || !IsUpgradableDefender(Defender))
	{
		return;
	}

	SelectedUpgradeTarget = Defender;
	if (!UpgradeMenuInstance)
	{
		UpgradeMenuInstance = CreateWidget<UUpgradeMenuWidget>(this, UUpgradeMenuWidget::StaticClass());
		if (UpgradeMenuInstance)
		{
			UpgradeMenuInstance->BindController(this);
			UpgradeMenuInstance->AddToViewport(20);
		}
	}
	if (UpgradeMenuInstance)
	{
		UpgradeMenuInstance->SetVisibility(ESlateVisibility::Visible);
		RefreshUpgradeMenu();
	}
	ZoomToDefender(Defender);
}

void ATDPlayerController::RefreshUpgradeMenu()
{
	ADefenderBase* Defender = SelectedUpgradeTarget.Get();
	if (!UpgradeMenuInstance || !Defender)
	{
		return;
	}

	const int32* LevelPtr = DefenderUpgradeLevels.Find(Defender);
	const int32 Level = LevelPtr ? *LevelPtr : 0;
	const int32 MaxLevel = GetMaxUpgradeLevel(Defender);
	UpgradeMenuInstance->ShowForDefender(
		Defender,
		GetUpgradeCostFor(Defender),
		Level,
		MaxLevel,
		IsArcherDefender(Defender) && Level == 1,
		GetUpgradeBonusText(Defender));
}

void ATDPlayerController::CloseUpgradeMenu()
{
	SelectedUpgradeTarget.Reset();
	if (UpgradeMenuInstance)
	{
		UpgradeMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
	bUpgradeCameraActive = false;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -89.f;
		PlayerCameraManager->ViewPitchMax = -20.f;
	}
	PlaceOverviewCamera();
}

void ATDPlayerController::ConfirmSelectedUpgrade()
{
	ApplyChosenUpgrade(0);
}

void ATDPlayerController::ConfirmArcherPowerUpgrade()
{
	ApplyChosenUpgrade(1);
}

void ATDPlayerController::ApplyChosenUpgrade(int32 Choice)
{
	ADefenderBase* Defender = SelectedUpgradeTarget.Get();
	if (!Defender || !IsUpgradableDefender(Defender))
	{
		return;
	}

	int32* LevelPtr = DefenderUpgradeLevels.Find(Defender);
	if (!LevelPtr)
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Clicked defender not tracked."));
		return;
	}
	if (*LevelPtr >= GetMaxUpgradeLevel(Defender))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("No further upgrades."));
		RefreshUpgradeMenu();
		return;
	}

	const int32 Cost = GetUpgradeCostFor(Defender);
	if (!HasEnoughMoney(Cost))
	{
		OnDefenderPlacementFailed.Broadcast(TEXT("Not enough gold to upgrade."));
		return;
	}

	SpendMoney(Cost);
	const int32 NextLevel = *LevelPtr + 1;

	if (ADefenderBog* Bog = Cast<ADefenderBog>(Defender))
	{
		if (NextLevel <= 1)
		{
			Bog->ApplyDigestUpgrade();
		}
		else if (NextLevel == 2)
		{
			Bog->ApplyFastDigestUpgrade();
		}
		else
		{
			Bog->ApplyConvertUpgrade();
		}
	}
	else if (ADefenderBarracks* Barracks = Cast<ADefenderBarracks>(Defender))
	{
		if (NextLevel <= 1)
		{
			Barracks->ApplyTrainUpgrade();
		}
		else if (NextLevel == 2)
		{
			Barracks->ApplyProductionUpgrade();
		}
		else
		{
			Barracks->ApplyTroopUpgrade();
		}
	}
	else if (NextLevel <= 1)
	{
		Defender->MaxHealth += 20.f;
		Defender->CurrentHealth = FMath::Min(Defender->CurrentHealth + 20.f, Defender->MaxHealth);
		Defender->AttackDamage += 5.f;
		Defender->OnHealthChanged.Broadcast(Defender->CurrentHealth, Defender->MaxHealth);
	}
	else if (Choice == 0)
	{
		Defender->MaxHealth += 30.f;
		Defender->CurrentHealth = FMath::Min(Defender->CurrentHealth + 30.f, Defender->MaxHealth);
		Defender->OnHealthChanged.Broadcast(Defender->CurrentHealth, Defender->MaxHealth);
		Defender->AttackInterval = FMath::Max(0.4f, Defender->AttackInterval - 0.25f);
		Defender->RestartAttackTimer();
	}
	else
	{
		Defender->AttackDamage += 10.f;
	}

	if (NextLevel == 1)
	{
		Defender->ApplyUpgradeVisual();
	}

	*LevelPtr = NextLevel;
	PlayUpgradeSparkle(Defender);
	OnDefenderUpgraded.Broadcast(Defender, NextLevel);
	RefreshUpgradeMenu();
}

void ATDPlayerController::PlayUpgradeSparkle(ADefenderBase* Defender)
{
	if (!Defender || !GetWorld())
	{
		return;
	}

	GameVFX::SpawnNiagaraAt(GetWorld(), GameVFX::LoadUpgradeBurstSystem(), Defender->GetActorLocation() + FVector(0.f, 0.f, 80.f));
}