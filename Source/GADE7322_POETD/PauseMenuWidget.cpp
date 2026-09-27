#include "PauseMenuWidget.h"
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
#include "TDPlayerController.h"
#include "UILayoutHelpers.h"

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	EnsureDefaultLayout();
	return Super::RebuildWidget();
}

void UPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDefaultLayout();

	if (ResumeButton)
	{
		ResumeButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnResumeClicked);
		ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}
	if (RestartButton)
	{
		RestartButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnRestartClicked);
		RestartButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnRestartClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.RemoveDynamic(this, &UPauseMenuWidget::OnQuitToMenuClicked);
		QuitToMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuitToMenuClicked);
	}
}

void UPauseMenuWidget::NativeConstruct()
{
	EnsureDefaultLayout();
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UPauseMenuWidget::EnsureDefaultLayout()
{
	if (ResumeButton || !WidgetTree)
	{
		return;
	}

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = Overlay;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	if (UOverlaySlot* DimSlot = Overlay->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Menu = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PausedTitle"));
	TDUIStyleText(Title, NSLOCTEXT("UI", "Paused", "PAUSED"), 42, FLinearColor::White, true);
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
			Slot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}
	};

	AddMenuButton(ResumeButton, TEXT("ResumeButton"), TEXT("ResumeLabel"), NSLOCTEXT("UI", "Resume", "Resume"));
	AddMenuButton(RestartButton, TEXT("RestartButton"), TEXT("RestartLabel"), NSLOCTEXT("UI", "Restart", "Restart"));
	AddMenuButton(QuitToMenuButton, TEXT("QuitToMenuButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "Quit", "Quit"));

	if (UOverlaySlot* MenuSlot = Overlay->AddChildToOverlay(Menu))
	{
		MenuSlot->SetHorizontalAlignment(HAlign_Center);
		MenuSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UPauseMenuWidget::OnResumeClicked()
{
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		PC->ResumeFromPause();
	}
}

void UPauseMenuWidget::OnRestartClicked()
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

void UPauseMenuWidget::OnQuitToMenuClicked()
{
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	if (!MainMenuLevel.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
	}
}
