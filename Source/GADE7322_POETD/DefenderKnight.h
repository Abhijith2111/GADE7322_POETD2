#pragma once

#include "CoreMinimal.h"
#include "DefenderBase.h"
#include "DefenderKnight.generated.h"

class AEnemyBase;

UCLASS()
class GADE7322_POETD_API ADefenderKnight : public ADefenderBase
{
	GENERATED_BODY()

public:
	ADefenderKnight();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void ApplyDefenderMesh() override;
	virtual void ScanAndAttack() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "100.0"))
	float SightRange = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "50.0"))
	float MoveSpeed = 340.f;

	AEnemyBase* FindPriorityTarget() const;
};
