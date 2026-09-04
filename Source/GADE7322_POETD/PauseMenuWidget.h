#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UButton* ResumeButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* RestartButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* QuitToMenuButton;

	UPROPERTY(EditDefaultsOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> MainMenuLevel;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnResumeClicked();

	UFUNCTION()
	void OnRestartClicked();

	UFUNCTION()
	void OnQuitToMenuClicked();
};