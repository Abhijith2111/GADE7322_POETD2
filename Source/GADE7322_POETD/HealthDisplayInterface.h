#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HealthDisplayInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UHealthDisplayInterface : public UInterface
{
	GENERATED_BODY()
};

class GADE7322_POETD_API IHealthDisplayInterface
{
	GENERATED_BODY()

public:
	// 0.0 - 1.0. Each implementing class wraps its own existing health logic.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Health")
	float GetDisplayHealthPercent() const;

	// True once the unit should stop showing a health bar.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Health")
	bool IsUnitDestroyed() const;
};