#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CreateUIWidgets.generated.h"

UCLASS()
class GADE7322_POETDEDITOR_API UCreateUIWidgetsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	static bool CreateProjectWidgets();
};
