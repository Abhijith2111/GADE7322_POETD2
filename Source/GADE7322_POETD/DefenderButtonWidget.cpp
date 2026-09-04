#include "DefenderButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TDPlayerController.h"

void UDefenderButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CostText)
	{
		CostText->SetText(FText::AsNumber(Cost));
	}
	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.AddDynamic(this, &UDefenderButtonWidget::OnPurchaseClicked);
	}
}

void UDefenderButtonWidget::RefreshAffordability(int32 CurrentGold)
{
	if (PurchaseButton)
	{
		PurchaseButton->SetIsEnabled(DefenderClassToBuild != nullptr && CurrentGold >= Cost);
	}
}

void UDefenderButtonWidget::OnPurchaseClicked()
{
	if (!DefenderClassToBuild)
	{
		return;
	}

	ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer());
	if (!IsValid(PC))
	{
		return;
	}

	if (!PC->CanAffordCost(Cost))
	{
		return; // Button should already be disabled, but double-check defensively.
	}

	// Arms placement — the next world click ("PlaceDefender") spawns this type.
	PC->SetPendingDefender(DefenderClassToBuild, Cost);
}