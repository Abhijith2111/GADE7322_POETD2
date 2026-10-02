#include "DefenderButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "TDPlayerController.h"
#include "UObject/ConstructorHelpers.h"

UDefenderButtonWidget::UDefenderButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = 100;
	ButtonLabel = NSLOCTEXT("UI", "Archer", "Archer");

	static ConstructorHelpers::FClassFinder<ADefenderBase> DefenderBP(TEXT("/Game/Gameplay/LevelObjects/BP_DefenderBase"));
	if (DefenderBP.Succeeded())
	{
		DefenderClassToBuild = DefenderBP.Class;
	}
}

void UDefenderButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UpdateDisplayedCost();
	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.RemoveDynamic(this, &UDefenderButtonWidget::OnPurchaseClicked);
		PurchaseButton->OnClicked.AddDynamic(this, &UDefenderButtonWidget::OnPurchaseClicked);
	}
}

void UDefenderButtonWidget::UpdateDisplayedCost()
{
	if (!CostText)
	{
		return;
	}

	const FText Label = ButtonLabel.IsEmpty()
		? NSLOCTEXT("UI", "Defender", "Defender")
		: ButtonLabel;
	CostText->SetText(FText::Format(
		NSLOCTEXT("UI", "CostFormatNamed", "{0}\n{1} coins"),
		Label,
		FText::AsNumber(Cost)));
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
		return;
	}

	PC->SetPendingDefender(DefenderClassToBuild, Cost);
}
