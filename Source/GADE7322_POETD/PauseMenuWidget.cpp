#include "PauseMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "TDPlayerController.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

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
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		PC->ReturnToMainMenu();
		return;
	}

	UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (GetWorld())
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true)));
	}
}
