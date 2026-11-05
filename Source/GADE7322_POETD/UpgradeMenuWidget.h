#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UpgradeMenuWidget.generated.h"

class ADefenderBase;
class ATDPlayerController;
class UButton;
class UTextBlock;

UCLASS()
class GADE7322_POETD_API UUpgradeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindController(ATDPlayerController* InController);
	void ShowForDefender(ADefenderBase* Defender, int32 Cost, int32 Level, int32 MaxLevel, bool bArcherChoice, const FText& BonusText);

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION()
	void HandlePowerClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void BuildMenu();

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BonusTextBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY()
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY()
	TObjectPtr<UButton> PowerButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> UpgradeButtonLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> PowerButtonLabel;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<ATDPlayerController> OwnerController;
};
