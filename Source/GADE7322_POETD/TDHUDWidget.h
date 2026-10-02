#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TDHUDWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UTDHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTDHUDWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* GoldText;

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* TowerHealthBar;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TowerHealthText;

	UPROPERTY(meta = (BindWidget))
	class UPanelWidget* DefenderButtonContainer;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleMoneyChanged(int32 NewAmount);

	UFUNCTION()
	void HandleTowerHealthChanged(float NewHealth, float InMaxHealth);

	void RefreshGold(int32 GoldAmount);
	void TryBindTowerHealth();
	void EnsureDefenderButton();

	UPROPERTY()
	class ATDGameState* CachedGameState;

	UPROPERTY()
	TSubclassOf<class UDefenderButtonWidget> DefenderButtonClass;

	bool bTowerBound = false;
	float RebindPollTimer = 0.f;
};
