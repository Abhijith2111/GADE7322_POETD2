#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyOgre.generated.h"

UCLASS()
class GADE7322_POETD_API AEnemyOgre : public AEnemyBase
{
	GENERATED_BODY()

public:
	AEnemyOgre();

protected:
	virtual void BeginPlay() override;
	virtual bool ShouldEngageDefenders() const override;
	virtual bool ShouldBypassFights() const override;
	virtual void UpdateMovementAndCombat(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* OgreMesh;

private:
	TSet<TWeakObjectPtr<ADefenderBase>> DamagedDefenders;

	void ApplyPassByDamage();
};
