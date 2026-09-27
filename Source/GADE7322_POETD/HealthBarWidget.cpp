#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

void UHealthBarWidget::InitializeWithOwner(AActor* InOwner)
{
	if (!IsValid(InOwner))
	{
		return;
	}

	OwningActor = InOwner;

	if (!InOwner->GetClass()->ImplementsInterface(UHealthDisplayInterface::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("HealthBarWidget: Owner %s does not implement IHealthDisplayInterface."), *InOwner->GetName());
		return;
	}

	LastKnownPercent = IHealthDisplayInterface::Execute_GetDisplayHealthPercent(InOwner);
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(LastKnownPercent);
		HealthProgressBar->SetFillColorAndOpacity(HealthyFillColor);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHealthBarWidget::ConfigureComponent(UWidgetComponent* Comp, const FVector& RelativeOffset, const FVector2D& DrawSize)
{
	if (!Comp)
	{
		return;
	}

	Comp->SetRelativeLocation(RelativeOffset);
	Comp->SetWidgetSpace(EWidgetSpace::World);
	Comp->SetDrawAtDesiredSize(false);
	Comp->SetDrawSize(DrawSize);
	Comp->SetPivot(FVector2D(0.5f, 1.f));
	Comp->SetTwoSided(true);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetHiddenInGame(false);
	Comp->SetVisibility(true);
	Comp->SetTickWhenOffscreen(true);
	Comp->SetBlendMode(EWidgetBlendMode::Transparent);
	Comp->SetWidgetClass(StaticClass());
}

void UHealthBarWidget::OrientComponentTowardCamera(UWidgetComponent* Comp)
{
	if (!IsValid(Comp))
	{
		return;
	}

	UWorld* World = Comp->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector ToCamera = CameraLocation - Comp->GetComponentLocation();
	if (!ToCamera.IsNearlyZero())
	{
		Comp->SetWorldRotation(ToCamera.GetSafeNormal().Rotation());
	}
}

void UHealthBarWidget::BindToWidgetComponent(UWidgetComponent* Comp, AActor* Owner)
{
	if (!IsValid(Comp) || !IsValid(Owner))
	{
		return;
	}

	Comp->SetHiddenInGame(false);
	Comp->SetVisibility(true);
	Comp->SetDrawAtDesiredSize(false);
	Comp->SetWidgetSpace(EWidgetSpace::World);
	Comp->SetWidgetClass(StaticClass());
	Comp->InitWidget();
	Comp->RequestRedraw();

	if (UHealthBarWidget* Bar = Cast<UHealthBarWidget>(Comp->GetUserWidgetObject()))
	{
		Bar->InitializeWithOwner(Owner);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	OrientComponentTowardCamera(Comp);
}

TSharedRef<SWidget> UHealthBarWidget::RebuildWidget()
{
	EnsureDefaultLayout();
	return Super::RebuildWidget();
}

void UHealthBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDefaultLayout();
}

void UHealthBarWidget::NativeConstruct()
{
	EnsureDefaultLayout();
	Super::NativeConstruct();
	TryAutoBindOwner();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHealthBarWidget::EnsureDefaultLayout()
{
	if (HealthProgressBar || !WidgetTree)
	{
		return;
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthSizeBox"));
	SizeBox->SetWidthOverride(120.f);
	SizeBox->SetHeightOverride(16.f);

	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	HealthProgressBar->SetPercent(1.f);
	HealthProgressBar->SetFillColorAndOpacity(HealthyFillColor);

	SizeBox->AddChild(HealthProgressBar);

	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = SizeBox;
	}
}

void UHealthBarWidget::TryAutoBindOwner()
{
	if (OwningActor.IsValid())
	{
		return;
	}

	for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (UWidgetComponent* Comp = Cast<UWidgetComponent>(Outer))
		{
			InitializeWithOwner(Comp->GetOwner());
			return;
		}

		if (AActor* Actor = Cast<AActor>(Outer))
		{
			InitializeWithOwner(Actor);
			return;
		}
	}
}

void UHealthBarWidget::FaceOwnerBarTowardCamera()
{
	AActor* Owner = OwningActor.Get();
	if (!IsValid(Owner))
	{
		return;
	}

	TArray<UWidgetComponent*> Components;
	Owner->GetComponents<UWidgetComponent>(Components);
	for (UWidgetComponent* Comp : Components)
	{
		if (Comp && Comp->GetUserWidgetObject() == this)
		{
			OrientComponentTowardCamera(Comp);
			return;
		}
	}
}

void UHealthBarWidget::PlayDamageFlash_Implementation()
{
	DamageFlashRemaining = 0.25f;
}

void UHealthBarWidget::UpdateDamageFlash(float InDeltaTime)
{
	if (!HealthProgressBar || DamageFlashRemaining <= 0.f)
	{
		return;
	}

	DamageFlashRemaining = FMath::Max(0.f, DamageFlashRemaining - InDeltaTime);
	const float Alpha = FMath::Clamp(DamageFlashRemaining / 0.25f, 0.f, 1.f);
	HealthProgressBar->SetFillColorAndOpacity(FMath::Lerp(HealthyFillColor, FlashFillColor, Alpha));
	HealthProgressBar->SetRenderOpacity(FMath::Lerp(1.f, 0.4f, Alpha));
}

void UHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateDamageFlash(InDeltaTime);
	FaceOwnerBarTowardCamera();

	if (!HealthProgressBar)
	{
		EnsureDefaultLayout();
	}

	AActor* Owner = OwningActor.Get();
	if (!IsValid(Owner))
	{
		if (!OwningActor.IsValid())
		{
			TryAutoBindOwner();
		}
		return;
	}

	if (!HealthProgressBar)
	{
		return;
	}

	PollTimer += InDeltaTime;
	if (PollTimer < 0.1f)
	{
		return;
	}
	PollTimer = 0.f;

	if (!Owner->GetClass()->ImplementsInterface(UHealthDisplayInterface::StaticClass()))
	{
		return;
	}

	if (IHealthDisplayInterface::Execute_IsUnitDestroyed(Owner))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	const float NewPercent = IHealthDisplayInterface::Execute_GetDisplayHealthPercent(Owner);
	HealthProgressBar->SetPercent(NewPercent);

	if (NewPercent < LastKnownPercent - KINDA_SMALL_NUMBER)
	{
		PlayDamageFlash();
	}
	LastKnownPercent = NewPercent;
}
