#include "GameOverWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "UILayoutHelpers.h"
#include "TDPlayerController.h"

TSharedRef<SWidget> UGameOverWidget::RebuildWidget()
{
	EnsureDefaultLayout();
	return Super::RebuildWidget();
}

void UGameOverWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDefaultLayout();
}

void UGameOverWidget::NativeConstruct()
{
	EnsureDefaultLayout();
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (RestartButton)
	{
		RestartButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnRestartClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnQuitClicked);
	}
}

void UGameOverWidget::EnsureDefaultLayout()
{
	if (ResultText || !WidgetTree)
	{
		return;
	}

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = Overlay;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	Dim->SetBrushColor(FLinearColor(0.12f, 0.f, 0.f, 0.7f));
	if (UOverlaySlot* DimSlot = Overlay->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));

	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultText"));
	TDUIStyleText(ResultText, NSLOCTEXT("UI", "Defeat", "GAME OVER"), 52, FLinearColor(1.f, 0.2f, 0.15f), true);
	Menu->AddChildToVerticalBox(ResultText);

	auto AddMenuButton = [&](UButton*& OutButton, const FName ButtonName, const FName LabelName, const FText& Label)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SizeBox->SetWidthOverride(260.f);
		SizeBox->SetHeightOverride(52.f);
		OutButton = TDUIMakeLabeledButton(WidgetTree, ButtonName, LabelName, Label, 22);
		SizeBox->AddChild(OutButton);
		if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(SizeBox))
		{
			Slot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}
	};

	AddMenuButton(RestartButton, TEXT("RestartButton"), TEXT("RestartLabel"), NSLOCTEXT("UI", "Restart", "Restart"));
	AddMenuButton(QuitToMenuButton, TEXT("QuitToMenuButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "Quit", "Quit"));

	if (UOverlaySlot* MenuSlot = Overlay->AddChildToOverlay(Menu))
	{
		MenuSlot->SetHorizontalAlignment(HAlign_Center);
		MenuSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UGameOverWidget::ShowResult(bool bVictory)
{
	EnsureDefaultLayout();

	if (ResultText)
	{
		ResultText->SetText(bVictory
			? NSLOCTEXT("UI", "Victory", "VICTORY")
			: NSLOCTEXT("UI", "Defeat", "GAME OVER"));
		ResultText->SetColorAndOpacity(FSlateColor(bVictory
			? FLinearColor(0.95f, 0.85f, 0.2f)
			: FLinearColor(1.f, 0.2f, 0.15f)));
	}
}

void UGameOverWidget::OnRestartClicked()
{
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		PC->RestartMatch();
		return;
	}

	UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (GetWorld())
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true)));
	}
}

void UGameOverWidget::OnQuitClicked()
{
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (!MainMenuLevel.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
	}
}
