#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameOverWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ResultText;

	UPROPERTY(meta = (BindWidget))
	class UButton* RestartButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* QuitToMenuButton;

	UPROPERTY(EditDefaultsOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowResult(bool bVictory);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnRestartClicked();

	UFUNCTION()
	void OnQuitClicked();
};
