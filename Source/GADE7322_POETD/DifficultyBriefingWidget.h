#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDGameInstance.h"
#include "DifficultyBriefingWidget.generated.h"

class ATDPlayerController;
class UButton;
class UTextBlock;

UCLASS()
class GADE7322_POETD_API UDifficultyBriefingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowForDifficulty(ETDDifficulty Difficulty, ATDPlayerController* InController);

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleContinueClicked();

	void BuildWidget();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton;

	TWeakObjectPtr<ATDPlayerController> OwnerController;
};
