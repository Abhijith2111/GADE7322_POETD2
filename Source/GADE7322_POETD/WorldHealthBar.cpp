#include "WorldHealthBar.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HealthDisplayInterface.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor HealthFillGreen(0.15f, 0.82f, 0.22f, 1.f);
	const FLinearColor HealthFillRed(0.85f, 0.12f, 0.1f, 1.f);
	const FVector2D ScreenBarSize(120.f, 14.f);

	struct FHealthBarVisual
	{
		float FlashRemaining = 0.f;
		float LastPercent = 1.f;
	};

	TMap<TWeakObjectPtr<UWidgetComponent>, FHealthBarVisual> HealthBarVisuals;
}

void UWorldHealthBarLibrary::ConfigureWorldHealthBar(UWidgetComponent* Comp, FVector RelativeOffset, FVector2D DrawSize)
{
	if (!Comp)
	{
		return;
	}

	(void)DrawSize;
	Comp->SetRelativeLocation(RelativeOffset);
	Comp->SetWidgetSpace(EWidgetSpace::Screen);
	Comp->SetDrawAtDesiredSize(false);
	Comp->SetDrawSize(ScreenBarSize);
	Comp->SetPivot(FVector2D(0.5f, 1.f));
	Comp->SetTwoSided(false);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetHiddenInGame(false);
	Comp->SetVisibility(true);
	Comp->SetTickWhenOffscreen(true);
	Comp->SetTickMode(ETickMode::Enabled);
	Comp->SetManuallyRedraw(false);
	Comp->SetBackgroundColor(FLinearColor::Transparent);
	Comp->SetTintColorAndOpacity(FLinearColor::White);
	Comp->SetBlendMode(EWidgetBlendMode::Transparent);
	Comp->SetWidgetClass(UWorldHealthBarWidget::StaticClass());

	UWorld* World = Comp->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (APlayerController* Viewer = World->GetFirstPlayerController())
	{
		Comp->SetOwnerPlayer(Viewer->GetLocalPlayer());
	}

	UWorldHealthBarWidget* BarWidget = Cast<UWorldHealthBarWidget>(Comp->GetUserWidgetObject());
	if (!BarWidget)
	{
		BarWidget = CreateWidget<UWorldHealthBarWidget>(World, UWorldHealthBarWidget::StaticClass());
		Comp->SetWidget(BarWidget);
	}

	if (BarWidget)
	{
		BarWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		BarWidget->BindToComponent(Comp);
	}

	TickHealthBar(BarWidget, 0.f);
}

bool UWorldHealthBarWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	BuildBar();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return bResult;
}

void UWorldHealthBarWidget::BindToComponent(UWidgetComponent* InComponent)
{
	BoundComponent = InComponent;
	StartUpdateTimer();
}

void UWorldHealthBarWidget::StartUpdateTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(UpdateTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		UpdateTimer,
		FTimerDelegate::CreateUObject(this, &UWorldHealthBarWidget::RefreshBar),
		0.05f,
		true);
}

void UWorldHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildBar();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	StartUpdateTimer();
}

void UWorldHealthBarWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	Super::NativeDestruct();
}

void UWorldHealthBarWidget::BuildBar()
{
	if (!WidgetTree || HealthBar)
	{
		return;
	}

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	HealthBar->SetPercent(1.f);
	HealthBar->SetFillColorAndOpacity(HealthFillGreen);
	HealthBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = HealthBar;
}

void UWorldHealthBarWidget::SetHealthVisual(float Percent, const FLinearColor& FillColor)
{
	if (!HealthBar)
	{
		BuildBar();
	}
	if (!HealthBar)
	{
		return;
	}

	HealthBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	HealthBar->SetFillColorAndOpacity(FillColor);
}

void UWorldHealthBarWidget::RefreshBar()
{
	UWorldHealthBarLibrary::TickHealthBar(this, 0.05f);
}

void UWorldHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UWorldHealthBarLibrary::TickHealthBar(this, InDeltaTime);
}

UWidgetComponent* UWorldHealthBarLibrary::GetOwningWidgetComponent(UUserWidget* Widget)
{
	return Widget ? Widget->GetTypedOuter<UWidgetComponent>() : nullptr;
}

void UWorldHealthBarLibrary::TickHealthBar(UUserWidget* Widget, float DeltaSeconds)
{
	UWorldHealthBarWidget* BarWidget = Cast<UWorldHealthBarWidget>(Widget);
	if (!BarWidget)
	{
		return;
	}

	UWidgetComponent* Comp = BarWidget->GetBoundComponent();
	if (!Comp)
	{
		Comp = Widget->GetTypedOuter<UWidgetComponent>();
	}
	AActor* Owner = Comp ? Comp->GetOwner() : nullptr;
	if (!Comp || !Owner || !Owner->GetClass()->ImplementsInterface(UHealthDisplayInterface::StaticClass()))
	{
		return;
	}

	if (IHealthDisplayInterface::Execute_IsUnitDestroyed(Owner))
	{
		BarWidget->SetVisibility(ESlateVisibility::Collapsed);
		HealthBarVisuals.Remove(Comp);
		return;
	}

	BarWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	const float Percent = FMath::Clamp(IHealthDisplayInterface::Execute_GetDisplayHealthPercent(Owner), 0.f, 1.f);

	FHealthBarVisual& Visual = HealthBarVisuals.FindOrAdd(Comp);
	Visual.FlashRemaining = FMath::Max(0.f, Visual.FlashRemaining - DeltaSeconds);
	if (Percent + KINDA_SMALL_NUMBER < Visual.LastPercent)
	{
		Visual.FlashRemaining = 0.25f;
	}
	Visual.LastPercent = Percent;

	FLinearColor FillColor = Percent < 0.35f ? HealthFillRed : HealthFillGreen;
	if (Visual.FlashRemaining > 0.f)
	{
		FillColor = FLinearColor::White;
	}
	BarWidget->SetHealthVisual(Percent, FillColor);
}
