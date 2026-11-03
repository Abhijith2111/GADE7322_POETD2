#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeatherTypes.h"
#include "WeatherStatusWidget.generated.h"

class AWeatherManager;
class UTextBlock;

UCLASS()
class GADE7322_POETD_API UWeatherStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindToWeather(AWeatherManager* Weather);
	void UnbindFromWeather();
	void SetWeather(EWeatherType Weather);

	virtual bool Initialize() override;

protected:
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleWeatherChanged(EWeatherType NewWeather);

	void BuildWidget();

	UPROPERTY()
	TObjectPtr<UTextBlock> WeatherText;

	TWeakObjectPtr<AWeatherManager> BoundWeather;
};
