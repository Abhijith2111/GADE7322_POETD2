#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateVFXAssetsCommandlet.generated.h"

UCLASS()
class UGenerateVFXAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGenerateVFXAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
