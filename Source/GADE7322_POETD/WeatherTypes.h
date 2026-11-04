#pragma once

#include "CoreMinimal.h"
#include "WeatherTypes.generated.h"

UENUM(BlueprintType)
enum class EWeatherType : uint8
{
	Sunny UMETA(DisplayName = "Sunny"),
	Rainy UMETA(DisplayName = "Rainy"),
	Night UMETA(DisplayName = "Night"),
	Windy UMETA(DisplayName = "Windy")
};
