#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthDisplayInterface.h"
#include "HealthBarWidget.generated.h"

class UWidgetComponent;

UCLASS()
class GADE7322_POETD_API UHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Health")
	void InitializeWithOwner(AActor* InOwner);

	UFUNCTION(BlueprintCallable, Category = "Health")
	static void BindToWidgetComponent(UWidgetComponent* Comp, AActor* Owner);

	static void ConfigureComponent(UWidgetComponent* Comp, const FVector& RelativeOffset, const FVector2D& DrawSize);

	static void OrientComponentTowardCamera(UWidgetComponent* Comp);

	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthProgressBar;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintNativeEvent, Category = "Health")
	void PlayDamageFlash();
	virtual void PlayDamageFlash_Implementation();

	void TryAutoBindOwner();
	void UpdateDamageFlash(float InDeltaTime);
	void FaceOwnerBarTowardCamera();

	UPROPERTY()
	TWeakObjectPtr<AActor> OwningActor;

	float LastKnownPercent = 1.f;
	float PollTimer = 0.f;
	float DamageFlashRemaining = 0.f;

	FLinearColor HealthyFillColor = FLinearColor(0.15f, 0.82f, 0.22f, 1.f);
	FLinearColor FlashFillColor = FLinearColor(1.f, 0.95f, 0.95f, 1.f);
};
