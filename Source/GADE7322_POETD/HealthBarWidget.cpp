#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"

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
	}
}

void UHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AActor* Owner = OwningActor.Get();
	if (!IsValid(Owner) || !HealthProgressBar)
	{
		SetVisibility(ESlateVisibility::Collapsed);
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

	const float NewPercent = IHealthDisplayInterface::Execute_GetDisplayHealthPercent(Owner);
	HealthProgressBar->SetPercent(NewPercent);

	if (NewPercent < LastKnownPercent - KINDA_SMALL_NUMBER)
	{
		PlayDamageFlash();
	}
	LastKnownPercent = NewPercent;
}