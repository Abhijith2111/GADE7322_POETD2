#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefenderBase.h"
#include "DefenderButtonWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UDefenderButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender")
	TSubclassOf<ADefenderBase> DefenderClassToBuild;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defender", meta = (ClampMin = "0"))
	int32 Cost = 100;

	UPROPERTY(meta = (BindWidget))
	class UButton* PurchaseButton;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* CostText;

	UFUNCTION(BlueprintCallable, Category = "Defender")
	void RefreshAffordability(int32 CurrentGold);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnPurchaseClicked();
};