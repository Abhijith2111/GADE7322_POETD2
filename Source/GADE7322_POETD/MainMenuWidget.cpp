#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TDPlayerController.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

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
