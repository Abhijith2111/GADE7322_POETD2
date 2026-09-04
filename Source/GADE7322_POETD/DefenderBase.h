#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthDisplayInterface.h"
#include "DefenderBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenderHealthChanged, float, NewHealth, float, InMaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDefenderDestroyed);

UCLASS()
class GADE7322_POETD_API ADefenderBase : public AActor, public IHealthDisplayInterface
{
	GENERATED_BODY()

public:
	ADefenderBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DefenderMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	class UWidgetComponent* HealthBarWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.05"))
	float AttackInterval = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDefenderHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDefenderDestroyed OnDefenderDestroyed;

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDestroyed() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const;

	virtual float GetDisplayHealthPercent_Implementation() const override { return GetHealthPercent(); }
	virtual bool IsUnitDestroyed_Implementation() const override { return IsDestroyed(); }

private:
	bool bIsDestroyed = false;
	FTimerHandle AttackTimerHandle;

	void ScanAndAttack();
	AActor* FindNearestTarget() const;
};