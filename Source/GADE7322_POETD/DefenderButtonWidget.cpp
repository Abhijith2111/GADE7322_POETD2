#include "DefenderButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "TDPlayerController.h"
#include "UILayoutHelpers.h"
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

TSharedRef<SWidget> UDefenderButtonWidget::RebuildWidget()
{
	EnsureDefaultLayout();
	return Super::RebuildWidget();
}

void UDefenderButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDefaultLayout();
}

void UDefenderButtonWidget::NativeConstruct()
{
	EnsureDefaultLayout();
	Super::NativeConstruct();

	if (CostText)
	{
		const FText Label = ButtonLabel.IsEmpty()
			? NSLOCTEXT("UI", "Defender", "Defender")
			: ButtonLabel;
		CostText->SetText(FText::Format(
			NSLOCTEXT("UI", "CostFormatNamed", "{0}\n{1} coins"),
			Label,
			FText::AsNumber(Cost)));
	}
	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.AddDynamic(this, &UDefenderButtonWidget::OnPurchaseClicked);
	}
}

void UDefenderButtonWidget::EnsureDefaultLayout()
{
	if (PurchaseButton || !WidgetTree)
	{
		return;
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PurchaseSizeBox"));
	SizeBox->SetWidthOverride(180.f);
	SizeBox->SetHeightOverride(56.f);

	PurchaseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PurchaseButton"));
	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CostText"));
	TDUIStyleText(CostText, NSLOCTEXT("UI", "CostPlaceholder", "100 coins"), 20, FLinearColor::White, true);
	PurchaseButton->AddChild(CostText);
	SizeBox->AddChild(PurchaseButton);

	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = SizeBox;
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
		return;
	}

	PC->SetPendingDefender(DefenderClassToBuild, Cost);
}
