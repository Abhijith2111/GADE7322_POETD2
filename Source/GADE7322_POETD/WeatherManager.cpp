#include "WeatherManager.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DefenderBase.h"
#include "EnemyBase.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralTerrain.h"
#include "TDGameState.h"
#include "TimerManager.h"

AWeatherManager::AWeatherManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WeatherPP = CreateDefaultSubobject<UPostProcessComponent>(TEXT("WeatherPP"));
	WeatherPP->SetupAttachment(Root);
	WeatherPP->bUnbound = true;
	WeatherPP->bEnabled = true;
	WeatherPP->BlendWeight = 0.f;
}

AWeatherManager* AWeatherManager::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (ATDGameState* GS = World->GetGameState<ATDGameState>())
	{
		if (AWeatherManager* Registered = GS->GetWeatherManager())
		{
			return Registered;
		}
	}

	return Cast<AWeatherManager>(UGameplayStatics::GetActorOfClass(World, AWeatherManager::StaticClass()));
}

AWeatherManager* AWeatherManager::Ensure(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	if (AWeatherManager* Existing = Get(World))
	{
		return Existing;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWeatherManager* Manager = World->SpawnActor<AWeatherManager>(AWeatherManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (ATDGameState* GS = World->GetGameState<ATDGameState>())
	{
		GS->RegisterWeatherManager(Manager);
	}
	return Manager;
}

void AWeatherManager::BeginPlay()
{
	Super::BeginPlay();
	BuildStars();
	BuildWindStreaks();
	BuildRainDrops();
	SetStarsVisible(false);
	SetWindStreaksVisible(false);
	SetRainVisible(false);
	ApplyAtmosphere();

	if (ATDGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr)
	{
		GS->RegisterWeatherManager(this);
	}
}

void AWeatherManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWeatherCycle();
	Super::EndPlay(EndPlayReason);
}

void AWeatherManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CurrentWeather == EWeatherType::Windy)
	{
		UpdateWindStreaks(DeltaSeconds);
	}
	else if (CurrentWeather == EWeatherType::Rainy)
	{
		UpdateRainDrops(DeltaSeconds);
	}
}

void AWeatherManager::StartWeatherCycle()
{
	bCycleActive = true;
	ApplyWeather(static_cast<EWeatherType>(FMath::RandRange(0, 3)));
	ScheduleNextWeatherChange();
}

void AWeatherManager::StopWeatherCycle()
{
	bCycleActive = false;
	GetWorldTimerManager().ClearTimer(WeatherChangeTimer);
	GetWorldTimerManager().ClearTimer(LightningTimer);
	GetWorldTimerManager().ClearTimer(LightningFlashTimer);
	LightningFlash = 0.f;
	CurrentWeather = EWeatherType::Sunny;
	SetStarsVisible(false);
	SetWindStreaksVisible(false);
	SetRainVisible(false);
	SetActorTickEnabled(false);
	ApplyAtmosphere();
}

void AWeatherManager::ScheduleNextWeatherChange()
{
	if (!bCycleActive || !GetWorld())
	{
		return;
	}

	const float Delay = FMath::FRandRange(45.f, 90.f);
	GetWorldTimerManager().SetTimer(WeatherChangeTimer, this, &AWeatherManager::RollNextWeather, Delay, false);
}

void AWeatherManager::RollNextWeather()
{
	if (!bCycleActive)
	{
		return;
	}

	TArray<EWeatherType, TInlineAllocator<4>> Choices = {
		EWeatherType::Sunny,
		EWeatherType::Rainy,
		EWeatherType::Night,
		EWeatherType::Windy
	};
	Choices.Remove(CurrentWeather);
	const EWeatherType Next = Choices[FMath::RandRange(0, Choices.Num() - 1)];
	ApplyWeather(Next);
	ScheduleNextWeatherChange();
}

void AWeatherManager::ApplyWeather(EWeatherType NewWeather)
{
	CurrentWeather = NewWeather;
	GetWorldTimerManager().ClearTimer(LightningTimer);
	GetWorldTimerManager().ClearTimer(LightningFlashTimer);
	LightningFlash = 0.f;

	if (CurrentWeather == EWeatherType::Windy)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		WindDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f).GetSafeNormal();
		RefreshWindStreakOrientations();
		SetWindStreaksVisible(true);
		SetRainVisible(false);
	}
	else if (CurrentWeather == EWeatherType::Rainy)
	{
		SetWindStreaksVisible(false);
		ResetRainDropPositions();
		SetRainVisible(true);
	}
	else
	{
		SetWindStreaksVisible(false);
		SetRainVisible(false);
	}

	RefreshWeatherTickState();
	SetStarsVisible(CurrentWeather == EWeatherType::Night);
	ApplyAtmosphere();

	if (CurrentWeather == EWeatherType::Rainy && GetWorld())
	{
		GetWorldTimerManager().SetTimer(LightningTimer, this, &AWeatherManager::HandleLightningTick, 8.f, true);
	}

	OnWeatherChanged.Broadcast(CurrentWeather);
	UE_LOG(LogTemp, Log, TEXT("WeatherManager: weather is now %d"), static_cast<int32>(CurrentWeather));
}

void AWeatherManager::ApplyAtmosphere()
{
	if (!WeatherPP)
	{
		return;
	}

	FPostProcessSettings& Settings = WeatherPP->Settings;
	Settings = FPostProcessSettings();

	switch (CurrentWeather)
	{
	case EWeatherType::Sunny:
		WeatherPP->BlendWeight = 0.05f;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = 0.15f;
		break;
	case EWeatherType::Rainy:
		WeatherPP->BlendWeight = 0.95f;
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.2f, 0.22f, 0.25f, 1.f);
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(0.95f, 0.95f, 0.98f, 1.f);
		Settings.bOverride_ColorGain = true;
		Settings.ColorGain = FVector4(0.72f, 0.74f, 0.78f, 1.f);
		Settings.bOverride_ColorGamma = true;
		Settings.ColorGamma = FVector4(0.95f, 0.96f, 1.0f, 1.f);
		Settings.bOverride_SceneColorTint = true;
		Settings.SceneColorTint = FLinearColor(0.55f, 0.58f, 0.62f);
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = -0.75f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.4f;
		break;
	case EWeatherType::Night:
		WeatherPP->BlendWeight = 0.9f;
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.65f, 0.7f, 1.05f, 1.f);
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = -1.35f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.55f;
		break;
	case EWeatherType::Windy:
		WeatherPP->BlendWeight = 0.55f;
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.9f, 0.9f, 0.85f, 1.f);
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = -0.1f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.2f;
		break;
	}

	if (LightningFlash > KINDA_SMALL_NUMBER)
	{
		WeatherPP->BlendWeight = FMath::Max(WeatherPP->BlendWeight, 0.95f);
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = 2.5f * LightningFlash;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 4.f * LightningFlash;
	}
}

void AWeatherManager::BuildStars()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* BaseMat = Sphere ? Sphere->GetMaterial(0) : nullptr;
	const FLinearColor StarColor(0.95f, 0.95f, 1.f, 1.f);

	for (int32 Index = 0; Index < 28; ++Index)
	{
		UStaticMeshComponent* Star = NewObject<UStaticMeshComponent>(this);
		Star->SetupAttachment(Root);
		Star->RegisterComponent();
		Star->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Star->SetCastShadow(false);
		if (Sphere)
		{
			Star->SetStaticMesh(Sphere);
		}
		Star->SetWorldScale3D(FVector(FMath::FRandRange(0.08f, 0.18f)));
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::FRandRange(1800.f, 4200.f);
		Star->SetWorldLocation(FVector(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			FMath::FRandRange(1200.f, 2200.f)));
		if (BaseMat)
		{
			if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this))
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), StarColor);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), StarColor);
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), StarColor * 12.f);
				Star->SetMaterial(0, DynMat);
			}
		}
		Star->SetVisibility(false);
		StarMeshes.Add(Star);
	}
}

void AWeatherManager::SetStarsVisible(bool bVisible)
{
	for (UStaticMeshComponent* Star : StarMeshes)
	{
		if (Star)
		{
			Star->SetVisibility(bVisible, true);
		}
	}
}

FVector AWeatherManager::MakeWindStreakSpawnLocation(bool bUpwindEdge) const
{
	const FVector Wind = WindDirection.GetSafeNormal2D();
	const FVector Side(-Wind.Y, Wind.X, 0.f);
	const float Across = FMath::FRandRange(-WindVolumeRadius, WindVolumeRadius);
	const float Along = bUpwindEdge
		? -WindVolumeRadius
		: FMath::FRandRange(-WindVolumeRadius, WindVolumeRadius);
	const float Height = FMath::FRandRange(80.f, 600.f);
	return Wind * Along + Side * Across + FVector(0.f, 0.f, Height);
}

void AWeatherManager::BuildWindStreaks()
{
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* WindMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_WindStreak.M_WindStreak"));
	UMaterialInterface* FallbackMat = Plane ? Plane->GetMaterial(0) : nullptr;
	UMaterialInterface* UseMat = WindMat ? WindMat : FallbackMat;
	const FLinearColor StreakColor(0.82f, 0.9f, 0.95f, 1.f);

	WindStreakMeshes.Reset();
	WindStreakData.Reset();

	for (int32 Index = 0; Index < 50; ++Index)
	{
		UStaticMeshComponent* Streak = NewObject<UStaticMeshComponent>(this);
		Streak->SetupAttachment(Root);
		Streak->RegisterComponent();
		Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Streak->SetCastShadow(false);
		Streak->SetAbsolute(true, true, true);
		if (Plane)
		{
			Streak->SetStaticMesh(Plane);
		}

		FWindStreakData Data;
		Data.Speed = FMath::FRandRange(400.f, 900.f);
		Data.Phase = FMath::FRandRange(0.f, 2.f * PI);
		Data.BobAmplitude = FMath::FRandRange(12.f, 35.f);
		Data.Scale = FVector(
			FMath::FRandRange(8.f, 18.f),
			FMath::FRandRange(0.05f, 0.12f),
			1.f);
		const FVector SpawnLoc = MakeWindStreakSpawnLocation(false);
		Data.BaseZ = SpawnLoc.Z;

		Streak->SetWorldScale3D(Data.Scale);
		Streak->SetWorldLocation(SpawnLoc);
		if (UseMat)
		{
			if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(UseMat, this))
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), StreakColor);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), StreakColor);
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), StreakColor * 2.5f);
				DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.22f);
				Streak->SetMaterial(0, DynMat);
			}
		}
		Streak->SetVisibility(false);
		WindStreakMeshes.Add(Streak);
		WindStreakData.Add(Data);
	}

	RefreshWindStreakOrientations();
}

void AWeatherManager::SetWindStreaksVisible(bool bVisible)
{
	for (UStaticMeshComponent* Streak : WindStreakMeshes)
	{
		if (Streak)
		{
			Streak->SetVisibility(bVisible, true);
		}
	}
}

void AWeatherManager::RefreshWindStreakOrientations()
{
	const FRotator WindRot = WindDirection.Rotation();
	for (int32 Index = 0; Index < WindStreakMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Streak = WindStreakMeshes[Index];
		if (!Streak)
		{
			continue;
		}

		Streak->SetWorldRotation(FRotator(0.f, WindRot.Yaw, 90.f));
		if (Index < WindStreakData.Num())
		{
			const FVector SpawnLoc = MakeWindStreakSpawnLocation(false);
			WindStreakData[Index].BaseZ = SpawnLoc.Z;
			Streak->SetWorldLocation(SpawnLoc);
			Streak->SetWorldScale3D(WindStreakData[Index].Scale);
		}
	}
}

void AWeatherManager::UpdateWindStreaks(float DeltaSeconds)
{
	const FVector Wind = WindDirection.GetSafeNormal2D();
	if (Wind.IsNearlyZero())
	{
		return;
	}

	const float TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (int32 Index = 0; Index < WindStreakMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Streak = WindStreakMeshes[Index];
		if (!Streak || Index >= WindStreakData.Num())
		{
			continue;
		}

		FWindStreakData& Data = WindStreakData[Index];
		FVector Location = Streak->GetComponentLocation();
		Location += Wind * (Data.Speed * DeltaSeconds);

		const float Along = FVector::DotProduct(FVector(Location.X, Location.Y, 0.f), Wind);
		if (Along > WindVolumeRadius)
		{
			Location = MakeWindStreakSpawnLocation(true);
			Data.BaseZ = Location.Z;
			Data.Phase = FMath::FRandRange(0.f, 2.f * PI);
		}

		Location.Z = Data.BaseZ + FMath::Sin(TimeSeconds * 1.6f + Data.Phase) * Data.BobAmplitude;
		Streak->SetWorldLocation(Location);
	}
}

FVector AWeatherManager::MakeRainDropSpawnLocation(bool bAtTop) const
{
	FVector Min(0.f, 0.f, 0.f);
	FVector Max(4000.f, 4000.f, 0.f);
	if (const AProceduralTerrain* Terrain = Cast<AProceduralTerrain>(UGameplayStatics::GetActorOfClass(GetWorld(), AProceduralTerrain::StaticClass())))
	{
		const FVector First = Terrain->GridToWorldLocation(0, 0);
		const FVector Last = Terrain->GridToWorldLocation(FMath::Max(Terrain->GridWidth - 1, 0), FMath::Max(Terrain->GridHeight - 1, 0));
		const FVector Half(Terrain->TileDimensions.X * 0.5f, Terrain->TileDimensions.Y * 0.5f, 0.f);
		Min = FVector(FMath::Min(First.X, Last.X), FMath::Min(First.Y, Last.Y), 0.f) - Half;
		Max = FVector(FMath::Max(First.X, Last.X), FMath::Max(First.Y, Last.Y), 0.f) + Half;
	}

	const float Height = bAtTop
		? FMath::FRandRange(RainMaxZ - 120.f, RainMaxZ)
		: FMath::FRandRange(RainMinZ, RainMaxZ);
	return FVector(FMath::FRandRange(Min.X, Max.X), FMath::FRandRange(Min.Y, Max.Y), Height);
}

void AWeatherManager::BuildRainDrops()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* RainMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_RainDrop.M_RainDrop"));
	if (!RainMat)
	{
		RainMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_WindStreak.M_WindStreak"));
	}
	UMaterialInterface* FallbackMat = Sphere ? Sphere->GetMaterial(0) : nullptr;
	UMaterialInterface* UseMat = RainMat ? RainMat : FallbackMat;
	const FLinearColor DropColor(0.75f, 0.88f, 1.f, 1.f);

	for (UStaticMeshComponent* Existing : RainDropMeshes)
	{
		if (Existing)
		{
			Existing->DestroyComponent();
		}
	}
	RainDropMeshes.Reset();
	RainDropData.Reset();

	for (int32 Index = 0; Index < 240; ++Index)
	{
		UStaticMeshComponent* Drop = NewObject<UStaticMeshComponent>(this);
		Drop->SetupAttachment(Root);
		Drop->RegisterComponent();
		Drop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Drop->SetCastShadow(false);
		Drop->SetAbsolute(true, true, true);
		Drop->SetHiddenInGame(false);
		if (Sphere)
		{
			Drop->SetStaticMesh(Sphere);
		}

		FRainDropData Data;
		Data.FallSpeed = FMath::FRandRange(850.f, 1600.f);
		Data.DriftX = FMath::FRandRange(-80.f, 80.f);
		Data.DriftY = FMath::FRandRange(-80.f, 80.f);

		const float Scale = FMath::FRandRange(0.18f, 0.32f);
		Drop->SetWorldScale3D(FVector(Scale));
		Drop->SetWorldRotation(FRotator::ZeroRotator);
		Drop->SetWorldLocation(MakeRainDropSpawnLocation(false));
		if (UseMat)
		{
			if (UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(UseMat, this))
			{
				DynMat->SetVectorParameterValue(TEXT("Color"), DropColor);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), DropColor);
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), DropColor * 6.f);
				DynMat->SetScalarParameterValue(TEXT("Opacity"), 0.65f);
				Drop->SetMaterial(0, DynMat);
			}
		}
		Drop->SetVisibility(false, true);
		RainDropMeshes.Add(Drop);
		RainDropData.Add(Data);
	}
}

void AWeatherManager::SetRainVisible(bool bVisible)
{
	for (UStaticMeshComponent* Drop : RainDropMeshes)
	{
		if (Drop)
		{
			Drop->SetHiddenInGame(!bVisible);
			Drop->SetVisibility(bVisible, true);
		}
	}
}

void AWeatherManager::ResetRainDropPositions()
{
	for (int32 Index = 0; Index < RainDropMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Drop = RainDropMeshes[Index];
		if (!Drop)
		{
			continue;
		}

		Drop->SetWorldLocation(MakeRainDropSpawnLocation(false));
		Drop->SetWorldRotation(FRotator::ZeroRotator);
		if (Index < RainDropData.Num())
		{
			RainDropData[Index].FallSpeed = FMath::FRandRange(850.f, 1600.f);
			RainDropData[Index].DriftX = FMath::FRandRange(-80.f, 80.f);
			RainDropData[Index].DriftY = FMath::FRandRange(-80.f, 80.f);
		}
	}
}

void AWeatherManager::UpdateRainDrops(float DeltaSeconds)
{
	for (int32 Index = 0; Index < RainDropMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Drop = RainDropMeshes[Index];
		if (!Drop || Index >= RainDropData.Num())
		{
			continue;
		}

		const FRainDropData& Data = RainDropData[Index];
		FVector Location = Drop->GetComponentLocation();
		Location.X += Data.DriftX * DeltaSeconds;
		Location.Y += Data.DriftY * DeltaSeconds;
		Location.Z -= Data.FallSpeed * DeltaSeconds;

		if (Location.Z < RainMinZ)
		{
			Location = MakeRainDropSpawnLocation(true);
			Drop->SetWorldRotation(FRotator::ZeroRotator);
		}

		Drop->SetWorldLocation(Location);
	}
}

void AWeatherManager::RefreshWeatherTickState()
{
	const bool bNeedsTick = CurrentWeather == EWeatherType::Windy || CurrentWeather == EWeatherType::Rainy;
	SetActorTickEnabled(bNeedsTick);
}

void AWeatherManager::HandleLightningTick()
{
	if (!bCycleActive || CurrentWeather != EWeatherType::Rainy)
	{
		return;
	}

	if (FMath::FRand() > 0.10f)
	{
		return;
	}

	StrikeLightning();
}

void AWeatherManager::StrikeLightning()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Candidates;

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyBase::StaticClass(), Enemies);
	for (AActor* Actor : Enemies)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Actor);
		if (Enemy && !Enemy->IsDefeated())
		{
			Candidates.Add(Enemy);
		}
	}

	TArray<AActor*> Defenders;
	UGameplayStatics::GetAllActorsOfClass(World, ADefenderBase::StaticClass(), Defenders);
	for (AActor* Actor : Defenders)
	{
		ADefenderBase* Defender = Cast<ADefenderBase>(Actor);
		if (Defender && !Defender->IsDestroyed())
		{
			Candidates.Add(Defender);
		}
	}

	if (Candidates.Num() == 0)
	{
		return;
	}

	AActor* Victim = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Victim))
	{
		Enemy->TakeDamageFromDefender(Enemy->CurrentHealth);
	}
	else if (ADefenderBase* Defender = Cast<ADefenderBase>(Victim))
	{
		Defender->ApplyDamage(Defender->MaxHealth);
	}

	LightningFlash = 1.f;
	ApplyAtmosphere();
	GetWorldTimerManager().SetTimer(LightningFlashTimer, this, &AWeatherManager::ClearLightningFlash, 0.12f, false);
}

void AWeatherManager::ClearLightningFlash()
{
	LightningFlash = 0.f;
	ApplyAtmosphere();
}
