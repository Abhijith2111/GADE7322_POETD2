#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthDisplayInterface.h"
#include "HealthBarWidget.generated.h"

UCLASS()
class GADE7322_POETD_API UHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Health")
	void InitializeWithOwner(AActor* InOwner);

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthProgressBar;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Implement in the WBP as a UMG Animation named "FlashRed" played from this event.
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void PlayDamageFlash();

	UPROPERTY()
	TWeakObjectPtr<AActor> OwningActor;

	float LastKnownPercent = 1.f;
	float PollTimer = 0.f;
};