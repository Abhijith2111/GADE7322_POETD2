#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameOverViewInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UGameOverViewInterface : public UInterface
{
	GENERATED_BODY()
};

class GADE7322_POETD_API IGameOverViewInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI")
	void ShowResult(bool bVictory);
};
