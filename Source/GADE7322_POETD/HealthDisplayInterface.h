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
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Health")
	float GetDisplayHealthPercent() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Health")
	bool IsUnitDestroyed() const;
};