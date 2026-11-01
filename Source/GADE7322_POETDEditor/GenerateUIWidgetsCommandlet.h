#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateUIWidgetsCommandlet.generated.h"

UCLASS()
class UGenerateUIWidgetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGenerateUIWidgetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
