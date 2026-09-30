#pragma once

#include "CoreMinimal.h"
#include "DefenderBase.h"

class AEnemyBase;

#include "DefenderBog.generated.h"

UCLASS()
class GADE7322_POETD_API ADefenderBog : public ADefenderBase
{
	GENERATED_BODY()

public:
	ADefenderBog();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyDefenderMesh() override;
	virtual void ScanAndAttack() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1"))
	float DigestDuration = 20.f;

private:
	bool bIsDigesting = false;
	FTimerHandle DigestTimerHandle;
	TWeakObjectPtr<AEnemyBase> PulledEnemy;

	float GetGrabRange() const;
	AEnemyBase* FindEnemyToEat() const;

	UFUNCTION()
	void FinishDigesting();
};
