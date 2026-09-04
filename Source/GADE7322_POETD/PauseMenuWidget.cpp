#include "PauseMenuWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "TDPlayerController.h"

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnResumeClicked);
	}
	if (RestartButton)
	{
		RestartButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnRestartClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::OnQuitToMenuClicked);
	}
}

void UPauseMenuWidget::OnResumeClicked()
{
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(GetOwningPlayer()))
	{
		PC->TogglePauseMenu();
	}
}

void UPauseMenuWidget::OnRestartClicked()
{
	UGameplayStatics::SetGamePaused(GetWorld(), false); // unpause BEFORE traveling, or the load can hang

	if (GetWorld())
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(GetWorld())));
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