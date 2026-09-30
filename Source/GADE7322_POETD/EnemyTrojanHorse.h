#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyTrojanHorse.generated.h"

UCLASS()
class GADE7322_POETD_API AEnemyTrojanHorse : public AEnemyBase
{
	GENERATED_BODY()

public:
	AEnemyTrojanHorse();

protected:
	virtual void BeginPlay() override;
	virtual bool ShouldEngageDefenders() const override;
	virtual bool ShouldBypassFights() const override;
	virtual void UpdateMovementAndCombat(float DeltaTime) override;
	virtual void OnReachedTower(ACentralTowerBase* Tower) override;
	virtual void HandleDeath() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CarrierMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trojan")
	TSubclassOf<AEnemyBase> PassengerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trojan", meta = (ClampMin = "1"))
	int32 PassengerCount = 5;

private:
	bool bHasBurst = false;

	void Burst();
};
