#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyHealthChanged, float, NewHealth, float, InMaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDestroyed, int32, RewardAmount);

UCLASS()
class GADE7322_POETD_API AEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float CurrentHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float MoveSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.05"))
	float AttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float WaypointAcceptanceRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
	int32 RewardOnDeath = 50;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnEnemyHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Reward")
	FOnEnemyDestroyed OnEnemyDestroyed;

	UFUNCTION(BlueprintCallable, Category = "Waypoints")
	void InitialiseWithWaypoints(const TArray<FVector>& InWaypoints);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeDamageFromDefender(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDefeated() const;

private:
	TArray<FVector> Waypoints;
	int32 CurrentWaypointIndex = 0;
	bool bIsDefeated = false;
	bool bIsAttacking = false;
	bool bWaypointsInitialised = false;

	FTimerHandle AttackTimerHandle;

	AActor* FindNearestAttackTarget() const;
	void ExecuteAttack();
	void HandleDeath();
};