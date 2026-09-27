#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/SWidget.h"
#include "TDHUDWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UTDHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* GoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UProgressBar* TowerHealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* TowerHealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UPanelWidget* DefenderButtonContainer;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleMoneyChanged(int32 NewAmount);

	UFUNCTION()
	void HandleTowerHealthChanged(float NewHealth, float InMaxHealth);

	void RefreshGold(int32 GoldAmount);
	void TryBindTowerHealth();
	void EnsureDefaultLayout();
	void EnsureDefenderButton();

	UPROPERTY()
	class ATDGameState* CachedGameState;

	bool bTowerBound = false;
	float RebindPollTimer = 0.f;
};
