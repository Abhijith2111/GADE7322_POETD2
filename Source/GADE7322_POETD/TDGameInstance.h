#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TDGameInstance.generated.h"

UENUM(BlueprintType)
enum class ETDDifficulty : uint8
{
	Easy,
	Medium,
	Hard
};

UCLASS()
class GADE7322_POETD_API UTDGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	void SetDifficulty(ETDDifficulty InDifficulty);
	ETDDifficulty GetDifficulty() const;

private:
	ETDDifficulty Difficulty = ETDDifficulty::Easy;
};
