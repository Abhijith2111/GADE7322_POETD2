#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/WidgetTree.h"

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

TSubclassOf<UUserWidget> UHealthBarWidget::GetPreferredWidgetClass()
{
	static TSubclassOf<UUserWidget> CachedClass;
	if (!CachedClass)
	{
		CachedClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_HealthBar.WBP_HealthBar_C"));
		if (!CachedClass)
		{
			CachedClass = StaticClass();
		}
	}
	return CachedClass;
}

void UHealthBarWidget::BindToWidgetComponent(UWidgetComponent* Comp, AActor* Owner)
{
	if (!IsValid(Comp) || !IsValid(Owner))
	{
		return;
	}

	const TSubclassOf<UUserWidget> PreferredClass = GetPreferredWidgetClass();
	if (Comp->GetWidgetClass() != PreferredClass)
	{
		Comp->SetWidgetClass(PreferredClass);
	}

	Comp->InitWidget();

	if (UHealthBarWidget* Bar = Cast<UHealthBarWidget>(Comp->GetUserWidgetObject()))
	{
		Bar->InitializeWithOwner(Owner);
		return;
	}

	Comp->SetWidgetClass(TSubclassOf<UUserWidget>(StaticClass()));
	Comp->InitWidget();
	if (UHealthBarWidget* Bar = Cast<UHealthBarWidget>(Comp->GetUserWidgetObject()))
	{
		Bar->InitializeWithOwner(Owner);
	}
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

	AActor* Owner = OwningActor.Get();
	if (!IsValid(Owner) || !HealthProgressBar)
	{
		if (!OwningActor.IsValid())
		{
			TryAutoBindOwner();
		}
		if (!OwningActor.IsValid())
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
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
