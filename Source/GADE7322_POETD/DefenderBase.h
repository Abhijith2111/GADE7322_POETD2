#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "HealthDisplayInterface.h"
#include "HitFlash.h"
#include "DefenderBase.generated.h"

class ADefenderBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDefenderHealthChanged, float, NewHealth, float, InMaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDefenderDestroyed, ADefenderBase*, DestroyedDefender);

UCLASS()
class GADE7322_POETD_API ADefenderBase : public AActor, public IHealthDisplayInterface
{
	GENERATED_BODY()

public:
	ADefenderBase();

	virtual float GetDisplayHealthPercent_Implementation() const override;
	virtual bool IsUnitDestroyed_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ScanAndAttack();
	virtual void ApplyDefenderMesh();
	AActor* FindNearestTarget() const;
	void SeatMeshOnPivot();
	void SnapToGround();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DefenderMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* HealthBarWidget;

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

private:
	bool bIsDestroyed = false;
	FTimerHandle AttackTimerHandle;
	FTimerHandle HitFlashTimer;
	FHitFlashState HitFlashState;

	UPROPERTY()
	TArray<UMeshComponent*> HitFlashMeshes;

	UPROPERTY()
	TArray<UMaterialInterface*> HitFlashMaterials;

	void PlayHitFlash();

	UFUNCTION()
	void RestoreHitFlash();
};