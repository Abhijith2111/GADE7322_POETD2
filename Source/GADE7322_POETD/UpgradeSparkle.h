#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UpgradeSparkle.generated.h"

UCLASS()
class GADE7322_POETD_API AUpgradeSparkle : public AActor
{
	GENERATED_BODY()

public:
	AUpgradeSparkle();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Sparks;

	TArray<FVector> Velocities;
	float Life = 0.f;
};
