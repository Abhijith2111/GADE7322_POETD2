#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	class UButton* PlayButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* QuitButton;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnPlayClicked();

	UFUNCTION()
	void OnQuitClicked();
};
