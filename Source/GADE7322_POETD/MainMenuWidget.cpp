#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TDPlayerController.h"
#include "UILayoutHelpers.h"

TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	EnsureDefaultLayout();
	return Super::RebuildWidget();
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDefaultLayout();

	if (PlayButton)
	{
		PlayButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnPlayClicked);
		PlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnPlayClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnQuitClicked);
		QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnQuitClicked);
	}
}

void UMainMenuWidget::NativeConstruct()
{
	EnsureDefaultLayout();
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UMainMenuWidget::EnsureDefaultLayout()
{
	if (PlayButton || !WidgetTree)
	{
		return;
	}

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = Overlay;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.62f));
	if (UOverlaySlot* DimSlot = Overlay->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	TDUIStyleText(Title, NSLOCTEXT("UI", "MainMenuTitle", "TOWER DEFENSE"), 48, FLinearColor::White, true);
	Menu->AddChildToVerticalBox(Title);

	auto AddMenuButton = [&](UButton*& OutButton, const FName ButtonName, const FName LabelName, const FText& Label)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SizeBox->SetWidthOverride(260.f);
		SizeBox->SetHeightOverride(52.f);
		OutButton = TDUIMakeLabeledButton(WidgetTree, ButtonName, LabelName, Label, 22);
		SizeBox->AddChild(OutButton);
		if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(SizeBox))
		{
			Slot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}
	};

	AddMenuButton(PlayButton, TEXT("PlayButton"), TEXT("PlayLabel"), NSLOCTEXT("UI", "Play", "Play"));
	AddMenuButton(QuitButton, TEXT("QuitButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "QuitGame", "Quit"));

	if (UOverlaySlot* MenuSlot = Overlay->AddChildToOverlay(Menu))
	{
		MenuSlot->SetHorizontalAlignment(HAlign_Center);
		MenuSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UMainMenuWidget::OnPlayClicked()
{
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		PC->StartMatchFromMenu();
	}
}

void UMainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
