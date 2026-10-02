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

	if (StartButton)
	{
		StartButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnStartClicked);
		StartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnStartClicked);
	}
	if (EasyButton)
	{
		EasyButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnEasyClicked);
		EasyButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnEasyClicked);
	}
	if (MediumButton)
	{
		MediumButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnMediumClicked);
		MediumButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnMediumClicked);
	}
	if (HardButton)
	{
		HardButton->OnClicked.RemoveDynamic(this, &UMainMenuWidget::OnHardClicked);
		HardButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnHardClicked);
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
	if (StartButton || !WidgetTree)
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
	TDUIStyleText(Title, NSLOCTEXT("UI", "MainMenuTitle", "Protect the Temple"), 48, FLinearColor::White, true);
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

	AddMenuButton(StartButton, TEXT("StartButton"), TEXT("StartLabel"), NSLOCTEXT("UI", "StartGame", "Start Game"));
	AddMenuButton(EasyButton, TEXT("EasyButton"), TEXT("EasyLabel"), NSLOCTEXT("UI", "Easy", "Easy"));
	AddMenuButton(MediumButton, TEXT("MediumButton"), TEXT("MediumLabel"), NSLOCTEXT("UI", "Medium", "Medium"));
	AddMenuButton(HardButton, TEXT("HardButton"), TEXT("HardLabel"), NSLOCTEXT("UI", "Hard", "Hard"));
	AddMenuButton(QuitButton, TEXT("QuitButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "QuitGame", "Quit"));

	SetChoiceVisible(EasyButton, false);
	SetChoiceVisible(MediumButton, false);
	SetChoiceVisible(HardButton, false);

	if (UOverlaySlot* MenuSlot = Overlay->AddChildToOverlay(Menu))
	{
		MenuSlot->SetHorizontalAlignment(HAlign_Center);
		MenuSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UMainMenuWidget::SetChoiceVisible(UButton* Button, bool bVisible)
{
	if (!Button)
	{
		return;
	}

	const ESlateVisibility ChoiceVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	Button->SetVisibility(ChoiceVisibility);
	if (UWidget* Parent = Button->GetParent())
	{
		Parent->SetVisibility(ChoiceVisibility);
	}
}

void UMainMenuWidget::OnStartClicked()
{
	SetChoiceVisible(StartButton, false);
	SetChoiceVisible(EasyButton, true);
	SetChoiceVisible(MediumButton, true);
	SetChoiceVisible(HardButton, true);
}

void UMainMenuWidget::StartAtDifficulty(ETDDifficulty Difficulty)
{
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		PC->StartMatchFromMenu(Difficulty);
	}
}

void UMainMenuWidget::OnEasyClicked()
{
	StartAtDifficulty(ETDDifficulty::Easy);
}

void UMainMenuWidget::OnMediumClicked()
{
	StartAtDifficulty(ETDDifficulty::Medium);
}

void UMainMenuWidget::OnHardClicked()
{
	StartAtDifficulty(ETDDifficulty::Hard);
}

void UMainMenuWidget::OnQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
