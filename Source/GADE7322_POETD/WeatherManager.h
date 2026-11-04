#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeatherTypes.h"
#include "WeatherManager.generated.h"

class UPostProcessComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherChanged, EWeatherType, NewWeather);

UCLASS()
class GADE7322_POETD_API AWeatherManager : public AActor
{
	GENERATED_BODY()

public:
	AWeatherManager();

	static AWeatherManager* Get(const UObject* WorldContext);
	static AWeatherManager* Ensure(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void StartWeatherCycle();

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void StopWeatherCycle();

	UFUNCTION(BlueprintPure, Category = "Weather")
	EWeatherType GetCurrentWeather() const { return CurrentWeather; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	FVector GetWindDirection() const { return WindDirection; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool IsRainy() const { return CurrentWeather == EWeatherType::Rainy; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool IsNight() const { return CurrentWeather == EWeatherType::Night; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool IsWindy() const { return CurrentWeather == EWeatherType::Windy; }

	UPROPERTY(BlueprintAssignable, Category = "Weather")
	FOnWeatherChanged OnWeatherChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FWindStreakData
	{
		float Speed = 600.f;
		float Phase = 0.f;
		float BobAmplitude = 20.f;
		float BaseZ = 200.f;
		FVector Scale = FVector(12.f, 0.08f, 0.08f);
	};

	struct FRainDropData
	{
		float FallSpeed = 1000.f;
		float DriftX = 0.f;
		float DriftY = 0.f;
	};

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> WeatherPP;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> StarMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WindStreakMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> RainDropMeshes;

	TArray<FWindStreakData> WindStreakData;
	TArray<FRainDropData> RainDropData;

	EWeatherType CurrentWeather = EWeatherType::Sunny;
	FVector WindDirection = FVector(1.f, 0.f, 0.f);
	bool bCycleActive = false;

	FTimerHandle WeatherChangeTimer;
	FTimerHandle LightningTimer;
	FTimerHandle LightningFlashTimer;

	float LightningFlash = 0.f;
	static constexpr float WindVolumeRadius = 3200.f;
	static constexpr float RainVolumeRadius = 3200.f;
	static constexpr float RainMinZ = 40.f;
	static constexpr float RainMaxZ = 1100.f;

	void ScheduleNextWeatherChange();
	void RollNextWeather();
	void ApplyWeather(EWeatherType NewWeather);
	void ApplyAtmosphere();
	void SetStarsVisible(bool bVisible);
	void BuildStars();
	void BuildWindStreaks();
	void SetWindStreaksVisible(bool bVisible);
	void RefreshWindStreakOrientations();
	void UpdateWindStreaks(float DeltaSeconds);
	FVector MakeWindStreakSpawnLocation(bool bUpwindEdge) const;
	void BuildRainDrops();
	void SetRainVisible(bool bVisible);
	void ResetRainDropPositions();
	void UpdateRainDrops(float DeltaSeconds);
	FVector MakeRainDropSpawnLocation(bool bAtTop) const;
	void RefreshWeatherTickState();

	UFUNCTION()
	void HandleLightningTick();

	UFUNCTION()
	void ClearLightningFlash();

	void StrikeLightning();
};
