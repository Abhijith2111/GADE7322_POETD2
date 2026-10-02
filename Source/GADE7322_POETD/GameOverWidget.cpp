#include "GameOverWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "TDPlayerController.h"

void UGameOverWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (RestartButton)
	{
		RestartButton->OnClicked.RemoveDynamic(this, &UGameOverWidget::OnRestartClicked);
		RestartButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnRestartClicked);
	}
	if (QuitToMenuButton)
	{
		QuitToMenuButton->OnClicked.RemoveDynamic(this, &UGameOverWidget::OnQuitClicked);
		QuitToMenuButton->OnClicked.AddDynamic(this, &UGameOverWidget::OnQuitClicked);
	}
}

void UGameOverWidget::ShowResult(bool bVictory)
{
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
