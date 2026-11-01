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

	UButton* MakeMenuButton(UWidgetTree* Tree, const FName Name, const FText& Caption)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(FLinearColor(1.f, 0.55f, 0.05f, 1.f));
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *Name.ToString()));
		StyleMenuText(Label, Caption, 20.f, FLinearColor(0.05f, 0.05f, 0.05f, 1.f));
		Button->AddChild(Label);
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
	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeCost"));
	StyleMenuText(CostText, FText::GetEmpty(), 18.f, FLinearColor(1.f, 0.85f, 0.2f, 1.f));

	UpgradeButton = MakeMenuButton(WidgetTree, TEXT("UpgradeButton"), NSLOCTEXT("Upgrade", "Buy", "Upgrade"));
	CloseButton = MakeMenuButton(WidgetTree, TEXT("CloseButton"), NSLOCTEXT("Upgrade", "Close", "Close"));

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
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UUpgradeMenuWidget::HandleCloseClicked);
	}
}

void UUpgradeMenuWidget::BindController(ATDPlayerController* InController)
{
	OwnerController = InController;
}

void UUpgradeMenuWidget::ShowForDefender(ADefenderBase* Defender, int32 Cost, int32 Level, const FText& BonusText)
{
	(void)Defender;
	if (TitleText)
	{
		TitleText->SetText(Level >= 1
			? NSLOCTEXT("Upgrade", "Maxed", "Already upgraded")
			: NSLOCTEXT("Upgrade", "Title", "Upgrade"));
	}
	if (BonusTextBlock)
	{
		BonusTextBlock->SetText(BonusText);
	}
	if (CostText)
	{
		CostText->SetText(Level >= 1
			? NSLOCTEXT("Upgrade", "NoCost", "")
			: FText::Format(NSLOCTEXT("Upgrade", "CostFmt", "{0} coins"), FText::AsNumber(Cost)));
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetIsEnabled(Level < 1);
		UpgradeButton->SetVisibility(Level >= 1 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UUpgradeMenuWidget::HandleUpgradeClicked()
{
	if (ATDPlayerController* PC = OwnerController.Get())
	{
		PC->ConfirmSelectedUpgrade();
	}
}

void UUpgradeMenuWidget::HandleCloseClicked()
{
	if (ATDPlayerController* PC = OwnerController.Get())
	{
		PC->CloseUpgradeMenu();
	}
}
