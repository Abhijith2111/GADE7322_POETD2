#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldHealthBar.generated.h"

class UProgressBar;
class UWidgetComponent;

UCLASS()
class GADE7322_POETD_API UWorldHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToComponent(UWidgetComponent* InComponent);
	void SetHealthVisual(float Percent, const FLinearColor& FillColor);
	UWidgetComponent* GetBoundComponent() const { return BoundComponent.Get(); }

	virtual bool Initialize() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildBar();
	void RefreshBar();

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	TWeakObjectPtr<UWidgetComponent> BoundComponent;
	FTimerHandle UpdateTimer;

	void StartUpdateTimer();
};

UCLASS()
class GADE7322_POETD_API UWorldHealthBarLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	static void ConfigureWorldHealthBar(UWidgetComponent* Comp, FVector RelativeOffset, FVector2D DrawSize);

	UFUNCTION(BlueprintPure, Category = "UI")
	static UWidgetComponent* GetOwningWidgetComponent(UUserWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = "UI")
	static void TickHealthBar(UUserWidget* Widget, float DeltaSeconds);
};
