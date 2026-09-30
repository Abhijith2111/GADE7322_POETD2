#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBrute.generated.h"

UCLASS()
class GADE7322_POETD_API AEnemyBrute : public AEnemyBase
{
	GENERATED_BODY()

public:
	AEnemyBrute();

protected:
	virtual void BeginPlay() override;
	virtual bool ShouldEngageDefenders() const override;
	virtual bool ShouldBypassFights() const override;
	virtual void UpdateMovementAndCombat(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BruteMesh;

private:
	TSet<TWeakObjectPtr<ADefenderBase>> DamagedDefenders;

	void ApplyPassByDamage();
};
