#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "GameOverWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* ResultText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* RestartButton;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* QuitToMenuButton;

	UPROPERTY(EditDefaultsOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowResult(bool bVictory);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnRestartClicked();

	UFUNCTION()
	void OnQuitClicked();

	void EnsureDefaultLayout();
};
