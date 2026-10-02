#include "CreateUIWidgets.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DefenderButtonWidget.h"
#include "Engine/Font.h"
#include "GameOverWidget.h"
#include "HealthBarWidget.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MainMenuWidget.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "ObjectTools.h"
#include "PauseMenuWidget.h"
#include "TDHUDWidget.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

namespace
{
	const TCHAR* RobotoPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");

	UObject* RobotoFont()
	{
		return LoadObject<UObject>(nullptr, RobotoPath);
	}

	void StyleText(UTextBlock* Text, const FText& Content, int32 Size, const FLinearColor& Color, bool bBold, bool bWrap)
	{
		if (!Text)
		{
			return;
		}

		FSlateFontInfo Font;
		Font.FontObject = RobotoFont();
		Font.Size = Size;
		Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
		if (!Font.FontObject)
		{
			Font = FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
		}

		Text->SetText(Content);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(bWrap);
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	}

	bool AddChildWidget(UWidget* Parent, UWidget* Child)
	{
		UPanelWidget* Panel = Cast<UPanelWidget>(Parent);
		return Panel && Child && Panel->AddChild(Child) != nullptr;
	}

	void FillOverlay(UWidget* Widget)
	{
		if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget ? Widget->Slot : nullptr))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	void CenterOverlay(UWidget* Widget)
	{
		if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget ? Widget->Slot : nullptr))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	void PadVertical(UWidget* Widget, float Top)
	{
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget ? Widget->Slot : nullptr))
		{
			Slot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	void PlaceOnCanvas(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget ? Widget->Slot : nullptr))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetAutoSize(true);
		}
	}

	bool DeleteExisting(const FString& PackageName, const FString& AssetName)
	{
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		UObject* Existing = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Existing)
		{
			return true;
		}

		TArray<UObject*> ToDelete;
		ToDelete.Add(Existing);
		return ObjectTools::ForceDeleteObjects(ToDelete, false) > 0;
	}

	UWidgetBlueprint* MakeWidgetBlueprint(const FString& AssetName, UClass* ParentClass)
	{
		const FString PackageName = TEXT("/Game/UI/") + AssetName;
		if (!DeleteExisting(PackageName, AssetName))
		{
			UE_LOG(LogTemp, Error, TEXT("CreateUIWidgets: Could not replace %s."), *PackageName);
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		UWidgetBlueprint* Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			ParentClass,
			nullptr,
			NAME_None,
			false);

		if (!Blueprint || !Blueprint->WidgetTree)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateUIWidgets: Could not create %s."), *AssetName);
			return nullptr;
		}

		return Blueprint;
	}

	bool SaveBlueprint(UWidgetBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return false;
		}

		Blueprint->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		FAssetRegistryModule::AssetCreated(Blueprint);

		UPackage* Package = Blueprint->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const bool bSaved = UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs);
		if (!bSaved)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateUIWidgets: Could not save %s."), *Blueprint->GetName());
		}
		return bSaved;
	}

	USizeBox* MakeSizedButton(UWidgetBlueprint* Blueprint, UPanelWidget* Parent, const FName& ButtonName, const FName& LabelName, const FText& Label, float TopPadding)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(ButtonName.ToString() + TEXT("Size")));
		SizeBox->SetWidthOverride(260.f);
		SizeBox->SetHeightOverride(52.f);
		AddChildWidget(Parent, SizeBox);
		PadVertical(SizeBox, TopPadding);

		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		AddChildWidget(SizeBox, Button);

		UTextBlock* Caption = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		StyleText(Caption, Label, 22, FLinearColor::White, true, false);
		AddChildWidget(Button, Caption);
		return SizeBox;
	}

	bool BuildDefenderButton()
	{
		UWidgetBlueprint* Blueprint = MakeWidgetBlueprint(TEXT("WBP_DefenderButton"), UDefenderButtonWidget::StaticClass());
		if (!Blueprint)
		{
			return false;
		}

		UWidgetTree* Tree = Blueprint->WidgetTree;
		USizeBox* Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PurchaseSizeBox"));
		Root->SetWidthOverride(200.f);
		Root->SetHeightOverride(72.f);
		Tree->RootWidget = Root;

		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PurchaseButton"));
		AddChildWidget(Root, Button);

		UTextBlock* CostText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CostText"));
		StyleText(CostText, NSLOCTEXT("UI", "CostPlaceholder", "Archer\n100 coins"), 20, FLinearColor::White, true, true);
		AddChildWidget(Button, CostText);
		return SaveBlueprint(Blueprint);
	}

	bool BuildHealthBar()
	{
		UWidgetBlueprint* Blueprint = MakeWidgetBlueprint(TEXT("WBP_HealthBar"), UHealthBarWidget::StaticClass());
		if (!Blueprint)
		{
			return false;
		}

		UProgressBar* Bar = Blueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
		Blueprint->WidgetTree->RootWidget = Bar;
		Bar->SetPercent(1.f);
		Bar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.82f, 0.22f, 1.f));

		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.95f));
		Bar->SetWidgetStyle(Style);
		return SaveBlueprint(Blueprint);
	}

	bool BuildMenu(const FString& AssetName, UClass* ParentClass, const FText& Title, int32 TitleSize, const FLinearColor& DimColor, const FName& TitleName, const FLinearColor& TitleColor, const TArray<TTuple<FName, FName, FText>>& Buttons, float ButtonGap)
	{
		UWidgetBlueprint* Blueprint = MakeWidgetBlueprint(AssetName, ParentClass);
		if (!Blueprint)
		{
			return false;
		}

		UWidgetTree* Tree = Blueprint->WidgetTree;
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
		Tree->RootWidget = Overlay;

		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(DimColor);
		AddChildWidget(Overlay, Dim);
		FillOverlay(Dim);

		UVerticalBox* Menu = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
		AddChildWidget(Overlay, Menu);
		CenterOverlay(Menu);

		UTextBlock* TitleText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TitleName);
		StyleText(TitleText, Title, TitleSize, TitleColor, true, false);
		AddChildWidget(Menu, TitleText);
		PadVertical(TitleText, 0.f);

		for (const TTuple<FName, FName, FText>& ButtonInfo : Buttons)
		{
			MakeSizedButton(Blueprint, Menu, ButtonInfo.Get<0>(), ButtonInfo.Get<1>(), ButtonInfo.Get<2>(), ButtonGap);
		}

		return SaveBlueprint(Blueprint);
	}

	bool BuildHUD()
	{
		UWidgetBlueprint* Blueprint = MakeWidgetBlueprint(TEXT("WBP_TDHUD"), UTDHUDWidget::StaticClass());
		if (!Blueprint)
		{
			return false;
		}

		UWidgetTree* Tree = Blueprint->WidgetTree;
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UHorizontalBox* GoldRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRow"));
		AddChildWidget(Root, GoldRow);
		PlaceOnCanvas(GoldRow, FAnchors(0.f, 0.f, 0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(32.f, 24.f));

		UTextBlock* CoinsLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoinsLabel"));
		StyleText(CoinsLabel, NSLOCTEXT("HUD", "CoinsLabel", "Coins:"), 26, FLinearColor(1.f, 0.85f, 0.2f), true, false);
		AddChildWidget(GoldRow, CoinsLabel);

		UTextBlock* GoldText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldText"));
		StyleText(GoldText, FText::AsNumber(0), 28, FLinearColor(1.f, 0.85f, 0.2f), true, false);
		AddChildWidget(GoldRow, GoldText);
		if (UHorizontalBoxSlot* GoldSlot = Cast<UHorizontalBoxSlot>(GoldText->Slot))
		{
			GoldSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}

		UVerticalBox* TowerBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TowerBox"));
		AddChildWidget(Root, TowerBox);
		PlaceOnCanvas(TowerBox, FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));

		UTextBlock* TowerLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TowerLabel"));
		StyleText(TowerLabel, NSLOCTEXT("HUD", "TowerLabel", "Tower"), 20, FLinearColor::White, true, false);
		AddChildWidget(TowerBox, TowerLabel);
		PadVertical(TowerLabel, 0.f);

		USizeBox* BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TowerBarSize"));
		BarSize->SetWidthOverride(400.f);
		BarSize->SetHeightOverride(24.f);
		AddChildWidget(TowerBox, BarSize);
		if (UVerticalBoxSlot* BarSlot = Cast<UVerticalBoxSlot>(BarSize->Slot))
		{
			BarSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
			BarSlot->SetHorizontalAlignment(HAlign_Center);
		}

		UProgressBar* TowerBar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TowerHealthBar"));
		TowerBar->SetPercent(1.f);
		TowerBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.82f, 0.22f, 1.f));
		AddChildWidget(BarSize, TowerBar);

		UTextBlock* TowerHealthText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TowerHealthText"));
		StyleText(TowerHealthText, NSLOCTEXT("HUD", "TowerHealthPlaceholder", "0 / 0"), 18, FLinearColor::White, false, false);
		AddChildWidget(TowerBox, TowerHealthText);
		PadVertical(TowerHealthText, 0.f);

		UHorizontalBox* Buttons = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DefenderButtonContainer"));
		AddChildWidget(Root, Buttons);
		PlaceOnCanvas(Buttons, FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -36.f));

		return SaveBlueprint(Blueprint);
	}
}

bool UCreateUIWidgetsLibrary::CreateProjectWidgets()
{
	if (!RobotoFont())
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateUIWidgets: Roboto was not found. Text will use the engine fallback font."));
	}

	TArray<TTuple<FName, FName, FText>> MainMenuButtons;
	MainMenuButtons.Emplace(TEXT("PlayButton"), TEXT("PlayLabel"), NSLOCTEXT("UI", "Play", "Play"));
	MainMenuButtons.Emplace(TEXT("QuitButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "QuitGame", "Quit"));

	TArray<TTuple<FName, FName, FText>> PauseButtons;
	PauseButtons.Emplace(TEXT("ResumeButton"), TEXT("ResumeLabel"), NSLOCTEXT("UI", "Resume", "Resume"));
	PauseButtons.Emplace(TEXT("RestartButton"), TEXT("RestartLabel"), NSLOCTEXT("UI", "Restart", "Restart"));
	PauseButtons.Emplace(TEXT("QuitToMenuButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "MainMenu", "Main Menu"));

	TArray<TTuple<FName, FName, FText>> GameOverButtons;
	GameOverButtons.Emplace(TEXT("RestartButton"), TEXT("RestartLabel"), NSLOCTEXT("UI", "Restart", "Restart"));
	GameOverButtons.Emplace(TEXT("QuitToMenuButton"), TEXT("QuitLabel"), NSLOCTEXT("UI", "MainMenu", "Main Menu"));

	const bool bOk =
		BuildDefenderButton()
		&& BuildHealthBar()
		&& BuildMenu(
			TEXT("WBP_MainMenu"),
			UMainMenuWidget::StaticClass(),
			NSLOCTEXT("UI", "MainMenuTitle", "TOWER DEFENSE"),
			48,
			FLinearColor(0.f, 0.f, 0.f, 0.62f),
			TEXT("Title"),
			FLinearColor::White,
			MainMenuButtons,
			16.f)
		&& BuildMenu(
			TEXT("WBP_PauseMenu"),
			UPauseMenuWidget::StaticClass(),
			NSLOCTEXT("UI", "Paused", "PAUSED"),
			42,
			FLinearColor(0.f, 0.f, 0.f, 0.55f),
			TEXT("Title"),
			FLinearColor::White,
			PauseButtons,
			12.f)
		&& BuildMenu(
			TEXT("WBP_GameOver"),
			UGameOverWidget::StaticClass(),
			NSLOCTEXT("UI", "Defeat", "GAME OVER"),
			52,
			FLinearColor(0.12f, 0.f, 0.f, 0.7f),
			TEXT("ResultText"),
			FLinearColor(1.f, 0.2f, 0.15f),
			GameOverButtons,
			14.f)
		&& BuildHUD();

	if (bOk)
	{
		UE_LOG(LogTemp, Display, TEXT("CreateUIWidgets: All widget blueprints were created."));
	}
	return bOk;
}
