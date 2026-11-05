#include "UpgradeMenuWidget.h"
#include "TDPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

namespace
{
	void StyleMenuText(UTextBlock* Text, const FText& Value, float Size, const FLinearColor& Color)
	{
		if (!Text)
		{
			return;
		}
		Text->SetText(Value);
		if (UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")))
		{
			Text->SetFont(FSlateFontInfo(Font, Size, FName(TEXT("Bold"))));
		}
		Text->SetColorAndOpacity(FSlateColor(Color));
	}

	UButton* MakeMenuButton(UWidgetTree* Tree, const FName Name, const FText& Caption, UTextBlock*& OutLabel)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(FLinearColor(1.f, 0.55f, 0.05f, 1.f));
		OutLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *Name.ToString()));
		StyleMenuText(OutLabel, Caption, 18.f, FLinearColor(0.05f, 0.05f, 0.05f, 1.f));
		Button->AddChild(OutLabel);
		return Button;
	}
}

bool UUpgradeMenuWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	BuildMenu();
	return bResult;
}

void UUpgradeMenuWidget::BuildMenu()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("UpgradeRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("UpgradePanel"));
	Panel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.07f, 0.92f));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UpgradeColumn"));
	Panel->AddChild(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeTitle"));
	StyleMenuText(TitleText, NSLOCTEXT("Upgrade", "Title", "Upgrade"), 26.f, FLinearColor::White);
	BonusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeBonus"));
	StyleMenuText(BonusTextBlock, FText::GetEmpty(), 18.f, FLinearColor(0.9f, 0.9f, 0.9f, 1.f));
	BonusTextBlock->SetAutoWrapText(true);
	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeCost"));
	StyleMenuText(CostText, FText::GetEmpty(), 18.f, FLinearColor(1.f, 0.85f, 0.2f, 1.f));

	UTextBlock* UpgradeLabel = nullptr;
	UTextBlock* PowerLabel = nullptr;
	UTextBlock* CloseLabel = nullptr;
	UpgradeButton = MakeMenuButton(WidgetTree, TEXT("UpgradeButton"), NSLOCTEXT("Upgrade", "Buy", "Upgrade"), UpgradeLabel);
	PowerButton = MakeMenuButton(WidgetTree, TEXT("PowerButton"), NSLOCTEXT("Upgrade", "Power", "Attack power"), PowerLabel);
	CloseButton = MakeMenuButton(WidgetTree, TEXT("CloseButton"), NSLOCTEXT("Upgrade", "Close", "Close"), CloseLabel);
	UpgradeButtonLabel = UpgradeLabel;
	PowerButtonLabel = PowerLabel;

	auto AddRow = [Column](UWidget* Child, float BottomPad)
	{
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(16.f, 8.f, 16.f, BottomPad));
		}
	};
	AddRow(TitleText, 4.f);
	AddRow(BonusTextBlock, 4.f);
	AddRow(CostText, 12.f);
	AddRow(UpgradeButton, 8.f);
	AddRow(PowerButton, 8.f);
	AddRow(CloseButton, 16.f);

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(1.f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(1.f, 0.5f));
		PanelSlot->SetPosition(FVector2D(-36.f, 0.f));
		PanelSlot->SetAutoSize(true);
	}
}

void UUpgradeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UpgradeButton)
	{
		UpgradeButton->OnClicked.AddDynamic(this, &UUpgradeMenuWidget::HandleUpgradeClicked);
	}
	if (PowerButton)
	{
		PowerButton->OnClicked.AddDynamic(this, &UUpgradeMenuWidget::HandlePowerClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UUpgradeMenuWidget::HandleCloseClicked);
	}
}

void UUpgradeMenuWidget::BindController(ATDPlayerController* InController)
{
	OwnerController = InController;
}

void UUpgradeMenuWidget::ShowForDefender(ADefenderBase* Defender, int32 Cost, int32 Level, int32 MaxLevel, bool bArcherChoice, const FText& BonusText)
{
	(void)Defender;
	const bool bMaxed = Level >= MaxLevel;
	if (TitleText)
	{
		TitleText->SetText(bMaxed
			? NSLOCTEXT("Upgrade", "Maxed", "Already upgraded")
			: (bArcherChoice
				? NSLOCTEXT("Upgrade", "ChooseTitle", "Choose an upgrade")
				: NSLOCTEXT("Upgrade", "Title", "Upgrade")));
	}
	if (BonusTextBlock)
	{
		BonusTextBlock->SetText(BonusText);
	}
	if (CostText)
	{
		CostText->SetText(bMaxed
			? NSLOCTEXT("Upgrade", "NoCost", "")
			: FText::Format(NSLOCTEXT("Upgrade", "CostFmt", "{0} coins"), FText::AsNumber(Cost)));
	}
	if (UpgradeButtonLabel)
	{
		UpgradeButtonLabel->SetText(bArcherChoice
			? NSLOCTEXT("Upgrade", "HealthSpeed", "Health and attack speed")
			: NSLOCTEXT("Upgrade", "Buy", "Upgrade"));
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(!bMaxed);
		UpgradeButton->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (PowerButton)
	{
		PowerButton->SetIsEnabled(bArcherChoice);
		PowerButton->SetVisibility(bArcherChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UUpgradeMenuWidget::HandleUpgradeClicked()
{
	if (ATDPlayerController* PC = OwnerController.Get())
	{
		PC->ConfirmSelectedUpgrade();
	}
}

void UUpgradeMenuWidget::HandlePowerClicked()
{
	if (ATDPlayerController* PC = OwnerController.Get())
	{
		PC->ConfirmArcherPowerUpgrade();
	}
}

void UUpgradeMenuWidget::HandleCloseClicked()
{
	if (ATDPlayerController* PC = OwnerController.Get())
	{
		PC->CloseUpgradeMenu();
	}
}
