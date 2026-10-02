#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "TDGameInstance.h"
#include "MainMenuWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* StartButton;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* EasyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* MediumButton;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* HardButton;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* QuitButton;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnStartClicked();

	UFUNCTION()
	void OnEasyClicked();

	UFUNCTION()
	void OnMediumClicked();

	UFUNCTION()
	void OnHardClicked();

	UFUNCTION()
	void OnQuitClicked();

	void EnsureDefaultLayout();
	void StartAtDifficulty(ETDDifficulty Difficulty);
	void SetChoiceVisible(class UButton* Button, bool bVisible);
};
