#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "MainMenuWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* PlayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	class UButton* QuitButton;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnPlayClicked();

	UFUNCTION()
	void OnQuitClicked();

	void EnsureDefaultLayout();
};
