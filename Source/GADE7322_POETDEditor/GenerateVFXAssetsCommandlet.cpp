#include "GenerateVFXAssetsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Misc/PackageName.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/SavePackage.h"

UGenerateVFXAssetsCommandlet::UGenerateVFXAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

namespace
{
	void DeleteExistingAsset(const FString& PackageName, const FString& AssetName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().Delete(*Filename, false, true);
		if (UPackage* ExistingPackage = FindPackage(nullptr, *PackageName))
		{
			ResetLoaders(ExistingPackage);
			if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *AssetName))
			{
				ExistingAsset->ClearFlags(RF_Standalone | RF_Public);
				ExistingAsset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_AllowPackageLinkerMismatch);
			}
			ExistingPackage->ClearFlags(RF_Standalone | RF_Public);
			ExistingPackage->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_AllowPackageLinkerMismatch);
		}
	}

	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset)
		{
			return false;
		}

		FAssetRegistryModule::AssetCreated(Asset);
		Asset->MarkPackageDirty();
		Asset->GetOutermost()->MarkPackageDirty();

		const FString PackageName = Asset->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs);
	}

	UMaterial* CreateMaterialAsset(const FString& AssetName)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/VFX/%s"), *AssetName);
		IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("VFX")), true);
		DeleteExistingAsset(PackageName, AssetName);

		UPackage* Package = CreatePackage(*PackageName);
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(
			UMaterial::StaticClass(), Package, *AssetName, RF_Public | RF_Standalone, nullptr, GWarn));
		if (Material)
		{
			Material->AddToRoot();
		}
		return Material;
	}

	UMaterialExpression* Expr(UMaterial* Material, TSubclassOf<UMaterialExpression> Class, int32 X, int32 Y)
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y);
	}

	void Connect(UMaterialExpression* From, const FString& FromPin, UMaterialExpression* To, const FString& ToPin)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromPin, To, ToPin);
	}

	void ConnectProp(UMaterialExpression* From, const FString& FromPin, EMaterialProperty Property)
	{
		UMaterialEditingLibrary::ConnectMaterialProperty(From, FromPin, Property);
	}

	UMaterialExpression* MakePulse(UMaterial* Material, float Speed, int32 BaseX, int32 BaseY)
	{
		UMaterialExpressionTime* TimeNode = Cast<UMaterialExpressionTime>(Expr(Material, UMaterialExpressionTime::StaticClass(), BaseX, BaseY));
		UMaterialExpressionConstant* SpeedNode = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), BaseX - 200, BaseY + 80));
		SpeedNode->R = Speed;
		UMaterialExpressionMultiply* TimeScale = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), BaseX + 180, BaseY));
		Connect(TimeNode, TEXT(""), TimeScale, TEXT("A"));
		Connect(SpeedNode, TEXT(""), TimeScale, TEXT("B"));

		UMaterialExpressionSine* SineNode = Cast<UMaterialExpressionSine>(Expr(Material, UMaterialExpressionSine::StaticClass(), BaseX + 360, BaseY));
		Connect(TimeScale, TEXT(""), SineNode, TEXT(""));

		UMaterialExpressionConstant* Half = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), BaseX + 360, BaseY + 120));
		Half->R = 0.5f;
		UMaterialExpressionMultiply* HalfSine = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), BaseX + 540, BaseY));
		Connect(SineNode, TEXT(""), HalfSine, TEXT("A"));
		Connect(Half, TEXT(""), HalfSine, TEXT("B"));

		UMaterialExpressionAdd* Pulse = Cast<UMaterialExpressionAdd>(Expr(Material, UMaterialExpressionAdd::StaticClass(), BaseX + 720, BaseY));
		Connect(HalfSine, TEXT(""), Pulse, TEXT("A"));
		Connect(Half, TEXT(""), Pulse, TEXT("B"));
		return Pulse;
	}

	bool BuildBogSlime()
	{
		UMaterial* Material = CreateMaterialAsset(TEXT("M_BogSlime"));
		if (!Material)
		{
			return false;
		}

		UMaterialExpression* Pulse = MakePulse(Material, 1.8f, -900, -200);

		UMaterialExpressionConstant3Vector* Teal = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, -320));
		Teal->Constant = FLinearColor(0.12f, 0.45f, 0.28f);
		UMaterialExpressionConstant3Vector* Lime = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, -180));
		Lime->Constant = FLinearColor(0.25f, 0.7f, 0.2f);

		UMaterialExpressionLinearInterpolate* BaseLerp = Cast<UMaterialExpressionLinearInterpolate>(Expr(Material, UMaterialExpressionLinearInterpolate::StaticClass(), 0, -260));
		Connect(Teal, TEXT(""), BaseLerp, TEXT("A"));
		Connect(Lime, TEXT(""), BaseLerp, TEXT("B"));
		Connect(Pulse, TEXT(""), BaseLerp, TEXT("Alpha"));
		ConnectProp(BaseLerp, TEXT(""), MP_BaseColor);

		UMaterialExpressionConstant* EmissiveScale = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), 0, -80));
		EmissiveScale->R = 0.45f;
		UMaterialExpressionMultiply* Emissive = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), 220, -120));
		Connect(BaseLerp, TEXT(""), Emissive, TEXT("A"));
		Connect(EmissiveScale, TEXT(""), Emissive, TEXT("B"));
		ConnectProp(Emissive, TEXT(""), MP_EmissiveColor);

		UMaterialExpressionVertexNormalWS* NormalWS = Cast<UMaterialExpressionVertexNormalWS>(Expr(Material, UMaterialExpressionVertexNormalWS::StaticClass(), -900, 120));
		UMaterialExpressionObjectPositionWS* ObjectPos = Cast<UMaterialExpressionObjectPositionWS>(Expr(Material, UMaterialExpressionObjectPositionWS::StaticClass(), -900, 260));
		UMaterialExpressionComponentMask* PosMask = Cast<UMaterialExpressionComponentMask>(Expr(Material, UMaterialExpressionComponentMask::StaticClass(), -700, 260));
		PosMask->R = 1;
		PosMask->G = 1;
		PosMask->B = 0;
		PosMask->A = 0;
		Connect(ObjectPos, TEXT(""), PosMask, TEXT(""));

		UMaterialExpressionTime* TimeNode = Cast<UMaterialExpressionTime>(Expr(Material, UMaterialExpressionTime::StaticClass(), -900, 400));
		UMaterialExpressionConstant* WobbleSpeed = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), -900, 480));
		WobbleSpeed->R = 2.4f;
		UMaterialExpressionMultiply* TimeWobble = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), -700, 400));
		Connect(TimeNode, TEXT(""), TimeWobble, TEXT("A"));
		Connect(WobbleSpeed, TEXT(""), TimeWobble, TEXT("B"));

		UMaterialExpressionAdd* Phase = Cast<UMaterialExpressionAdd>(Expr(Material, UMaterialExpressionAdd::StaticClass(), -500, 320));
		Connect(TimeWobble, TEXT(""), Phase, TEXT("A"));
		Connect(PosMask, TEXT(""), Phase, TEXT("B"));

		UMaterialExpressionSine* WobbleSine = Cast<UMaterialExpressionSine>(Expr(Material, UMaterialExpressionSine::StaticClass(), -300, 320));
		Connect(Phase, TEXT(""), WobbleSine, TEXT(""));

		UMaterialExpressionScalarParameter* Amplitude = Cast<UMaterialExpressionScalarParameter>(Expr(Material, UMaterialExpressionScalarParameter::StaticClass(), -300, 440));
		Amplitude->ParameterName = TEXT("Amplitude");
		Amplitude->DefaultValue = 12.f;

		UMaterialExpressionMultiply* DisplaceAmount = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), -80, 320));
		Connect(WobbleSine, TEXT(""), DisplaceAmount, TEXT("A"));
		Connect(Amplitude, TEXT(""), DisplaceAmount, TEXT("B"));

		UMaterialExpressionMultiply* WPO = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), 140, 220));
		Connect(NormalWS, TEXT(""), WPO, TEXT("A"));
		Connect(DisplaceAmount, TEXT(""), WPO, TEXT("B"));
		ConnectProp(WPO, TEXT(""), MP_WorldPositionOffset);

		UMaterialExpressionConstant* Roughness = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), 140, 40));
		Roughness->R = 0.65f;
		ConnectProp(Roughness, TEXT(""), MP_Roughness);

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return SaveAssetPackage(Material);
	}

	bool BuildUpgradeGoldPulse()
	{
		UMaterial* Material = CreateMaterialAsset(TEXT("M_UpgradeGoldPulse"));
		if (!Material)
		{
			return false;
		}

		UMaterialExpression* Pulse = MakePulse(Material, 3.2f, -900, -40);

		UMaterialExpressionConstant3Vector* Neutral = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, -160));
		Neutral->Constant = FLinearColor(0.35f, 0.32f, 0.28f);
		UMaterialExpressionConstant3Vector* Gold = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, 0));
		Gold->Constant = FLinearColor(1.f, 0.85f, 0.15f);

		UMaterialExpressionLinearInterpolate* BaseLerp = Cast<UMaterialExpressionLinearInterpolate>(Expr(Material, UMaterialExpressionLinearInterpolate::StaticClass(), 40, -80));
		Connect(Neutral, TEXT(""), BaseLerp, TEXT("A"));
		Connect(Gold, TEXT(""), BaseLerp, TEXT("B"));
		Connect(Pulse, TEXT(""), BaseLerp, TEXT("Alpha"));
		ConnectProp(BaseLerp, TEXT(""), MP_BaseColor);

		UMaterialExpressionConstant* GlowScale = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), 40, 120));
		GlowScale->R = 2.5f;
		UMaterialExpressionMultiply* GlowPulse = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), 240, 40));
		Connect(Pulse, TEXT(""), GlowPulse, TEXT("A"));
		Connect(GlowScale, TEXT(""), GlowPulse, TEXT("B"));
		UMaterialExpressionMultiply* Emissive = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), 440, 0));
		Connect(Gold, TEXT(""), Emissive, TEXT("A"));
		Connect(GlowPulse, TEXT(""), Emissive, TEXT("B"));
		ConnectProp(Emissive, TEXT(""), MP_EmissiveColor);

		UMaterialExpressionConstant* Roughness = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), 440, 160));
		Roughness->R = 0.35f;
		ConnectProp(Roughness, TEXT(""), MP_Roughness);

		UMaterialExpressionConstant* Metallic = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), 440, 240));
		Metallic->R = 0.55f;
		ConnectProp(Metallic, TEXT(""), MP_Metallic);

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return SaveAssetPackage(Material);
	}

	bool BuildTempleStrain()
	{
		UMaterial* Material = CreateMaterialAsset(TEXT("M_PP_TempleStrain"));
		if (!Material)
		{
			return false;
		}

		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorAfterTonemapping;

		UMaterialExpressionSceneTexture* SceneTex = Cast<UMaterialExpressionSceneTexture>(Expr(Material, UMaterialExpressionSceneTexture::StaticClass(), -800, 0));
		SceneTex->SceneTextureId = PPI_PostProcessInput0;

		UMaterialExpressionScreenPosition* ScreenPos = Cast<UMaterialExpressionScreenPosition>(Expr(Material, UMaterialExpressionScreenPosition::StaticClass(), -800, 200));
		UMaterialExpressionConstant* HalfUV = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), -800, 320));
		HalfUV->R = 0.5f;
		UMaterialExpressionAdd* Centered = Cast<UMaterialExpressionAdd>(Expr(Material, UMaterialExpressionAdd::StaticClass(), -560, 220));
		Connect(ScreenPos, TEXT(""), Centered, TEXT("A"));
		UMaterialExpressionConstant3Vector* NegHalf = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -800, 400));
		NegHalf->Constant = FLinearColor(-0.5f, -0.5f, 0.f);
		Connect(NegHalf, TEXT(""), Centered, TEXT("B"));

		UMaterialExpressionComponentMask* CenterMask = Cast<UMaterialExpressionComponentMask>(Expr(Material, UMaterialExpressionComponentMask::StaticClass(), -360, 220));
		CenterMask->R = 1;
		CenterMask->G = 1;
		CenterMask->B = 0;
		CenterMask->A = 0;
		Connect(Centered, TEXT(""), CenterMask, TEXT(""));

		UMaterialExpressionDotProduct* DistSq = Cast<UMaterialExpressionDotProduct>(Expr(Material, UMaterialExpressionDotProduct::StaticClass(), -160, 220));
		Connect(CenterMask, TEXT(""), DistSq, TEXT("A"));
		Connect(CenterMask, TEXT(""), DistSq, TEXT("B"));

		UMaterialExpressionConstant* VignetteScale = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), -160, 360));
		VignetteScale->R = 2.2f;
		UMaterialExpressionMultiply* VignetteRaw = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), 40, 220));
		Connect(DistSq, TEXT(""), VignetteRaw, TEXT("A"));
		Connect(VignetteScale, TEXT(""), VignetteRaw, TEXT("B"));

		UMaterialExpressionScalarParameter* Intensity = Cast<UMaterialExpressionScalarParameter>(Expr(Material, UMaterialExpressionScalarParameter::StaticClass(), 40, 360));
		Intensity->ParameterName = TEXT("Intensity");
		Intensity->DefaultValue = 0.f;

		UMaterialExpressionMultiply* Vignette = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), 260, 260));
		Connect(VignetteRaw, TEXT(""), Vignette, TEXT("A"));
		Connect(Intensity, TEXT(""), Vignette, TEXT("B"));

		UMaterialExpressionDesaturation* Desat = Cast<UMaterialExpressionDesaturation>(Expr(Material, UMaterialExpressionDesaturation::StaticClass(), -360, 0));
		Connect(SceneTex, TEXT(""), Desat, TEXT("Input"));
		Connect(Intensity, TEXT(""), Desat, TEXT("Fraction"));

		UMaterialExpressionConstant3Vector* StrainTint = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -360, 120));
		StrainTint->Constant = FLinearColor(0.55f, 0.08f, 0.08f);
		UMaterialExpressionMultiply* Tinted = Cast<UMaterialExpressionMultiply>(Expr(Material, UMaterialExpressionMultiply::StaticClass(), -120, 40));
		Connect(Desat, TEXT(""), Tinted, TEXT("A"));
		Connect(StrainTint, TEXT(""), Tinted, TEXT("B"));

		UMaterialExpressionLinearInterpolate* EdgeMix = Cast<UMaterialExpressionLinearInterpolate>(Expr(Material, UMaterialExpressionLinearInterpolate::StaticClass(), 260, 40));
		Connect(SceneTex, TEXT(""), EdgeMix, TEXT("A"));
		Connect(Tinted, TEXT(""), EdgeMix, TEXT("B"));
		Connect(Vignette, TEXT(""), EdgeMix, TEXT("Alpha"));
		ConnectProp(EdgeMix, TEXT(""), MP_EmissiveColor);

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return SaveAssetPackage(Material);
	}

	bool BuildParticleSpriteMaterial(const FString& AssetName, const FLinearColor& Color)
	{
		UMaterial* Material = CreateMaterialAsset(AssetName);
		if (!Material)
		{
			return false;
		}

		Material->BlendMode = BLEND_Translucent;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;
		UMaterialEditingLibrary::SetBaseMaterialUsage(Material, MATUSAGE_NiagaraSprites, true);

		UMaterialExpressionConstant3Vector* Tint = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, 0));
		Tint->Constant = Color;
		ConnectProp(Tint, TEXT(""), MP_EmissiveColor);

		UMaterialExpressionConstant* Opacity = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), -200, 120));
		Opacity->R = 0.85f;
		ConnectProp(Opacity, TEXT(""), MP_Opacity);

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return SaveAssetPackage(Material);
	}

	bool BuildWindStreakMaterial()
	{
		UMaterial* Material = CreateMaterialAsset(TEXT("M_WindStreak"));
		if (!Material)
		{
			return false;
		}

		Material->BlendMode = BLEND_Translucent;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;

		UMaterialExpressionConstant3Vector* Tint = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, 0));
		Tint->Constant = FLinearColor(0.82f, 0.9f, 0.95f);
		ConnectProp(Tint, TEXT(""), MP_EmissiveColor);

		UMaterialExpressionConstant* Opacity = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), -200, 120));
		Opacity->R = 0.22f;
		ConnectProp(Opacity, TEXT(""), MP_Opacity);

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return SaveAssetPackage(Material);
	}

	bool BuildRainDropMaterial()
	{
		UMaterial* Material = CreateMaterialAsset(TEXT("M_RainDrop"));
		if (!Material)
		{
			return false;
		}

		Material->BlendMode = BLEND_Translucent;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;

		UMaterialExpressionConstant3Vector* Tint = Cast<UMaterialExpressionConstant3Vector>(Expr(Material, UMaterialExpressionConstant3Vector::StaticClass(), -200, 0));
		Tint->Constant = FLinearColor(0.7f, 0.85f, 1.f);
		ConnectProp(Tint, TEXT(""), MP_EmissiveColor);

		UMaterialExpressionConstant* Opacity = Cast<UMaterialExpressionConstant>(Expr(Material, UMaterialExpressionConstant::StaticClass(), -200, 120));
		Opacity->R = 0.7f;
		ConnectProp(Opacity, TEXT(""), MP_Opacity);

		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return SaveAssetPackage(Material);
	}

	void AssignSpriteMaterial(UNiagaraSystem* System, UMaterialInterface* SpriteMaterial)
	{
		if (!System || !SpriteMaterial)
		{
			return;
		}

		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData())
			{
				Data->ForEachRenderer([SpriteMaterial](UNiagaraRendererProperties* Renderer)
				{
					if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
					{
						Sprite->Material = SpriteMaterial;
					}
				});
			}
		}
	}

	UNiagaraSystem* CreateNiagaraAsset(const FString& AssetName, UMaterialInterface* SpriteMaterial)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/VFX/%s"), *AssetName);
		IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("VFX")), true);
		DeleteExistingAsset(PackageName, AssetName);

		UPackage* Package = CreatePackage(*PackageName);
		UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		UNiagaraSystemFactoryNew::InitializeSystem(System, true);

		UNiagaraEmitter* Emitter = NewObject<UNiagaraEmitter>(GetTransientPackage(), NAME_None, RF_Transactional);
		UNiagaraEmitterFactoryNew::InitializeEmitter(Emitter, true);
		const FGuid EmitterVersion = Emitter->GetExposedVersion().VersionGuid;
		FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Emitter, EmitterVersion);
		AssignSpriteMaterial(System, SpriteMaterial);

		System->RequestCompile(false);
		System->WaitForCompilationComplete();
		System->AddToRoot();
		return System;
	}

	bool BuildUpgradeBurst(UMaterialInterface* GoldSprite)
	{
		UNiagaraSystem* System = CreateNiagaraAsset(TEXT("NS_UpgradeBurst"), GoldSprite);
		return System && SaveAssetPackage(System);
	}

	bool BuildDeathDust(UMaterialInterface* DustSprite)
	{
		UNiagaraSystem* System = CreateNiagaraAsset(TEXT("NS_DefenderDeathDust"), DustSprite);
		return System && SaveAssetPackage(System);
	}
}

int32 UGenerateVFXAssetsCommandlet::Main(const FString& Params)
{
	bool bOk = true;
	bOk &= BuildBogSlime();
	bOk &= BuildUpgradeGoldPulse();
	bOk &= BuildTempleStrain();
	bOk &= BuildParticleSpriteMaterial(TEXT("M_GoldParticle"), FLinearColor(1.f, 0.9f, 0.15f));
	bOk &= BuildParticleSpriteMaterial(TEXT("M_DustParticle"), FLinearColor(0.45f, 0.35f, 0.22f));
	bOk &= BuildWindStreakMaterial();
	bOk &= BuildRainDropMaterial();

	UMaterialInterface* GoldSprite = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_GoldParticle.M_GoldParticle"));
	UMaterialInterface* DustSprite = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/M_DustParticle.M_DustParticle"));
	bOk &= BuildUpgradeBurst(GoldSprite);
	bOk &= BuildDeathDust(DustSprite);

	if (!bOk)
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateVFXAssetsCommandlet failed to create one or more assets."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("GenerateVFXAssetsCommandlet created Content/VFX materials and Niagara systems."));
	return 0;
}
