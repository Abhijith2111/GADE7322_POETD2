#include "TDHUDWidget.h"
#include "DefenderButtonWidget.h"
#include "DefenderBog.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "TDGameState.h"
#include "CentralTowerBase.h"
#include "UILayoutHelpers.h"

TSharedRef<SWidget> UTDHUDWidget::RebuildWidget()
{
	EnsureDefaultLayout();
	return Super::RebuildWidget();
}

void UTDHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureDefaultLayout();
}

void UTDHUDWidget::NativeConstruct()
{
	EnsureDefaultLayout();
	Super::NativeConstruct();

	CachedGameState = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr;

	if (IsValid(CachedGameState))
	{
		CachedGameState->OnMoneyChanged.AddDynamic(this, &UTDHUDWidget::HandleMoneyChanged);
		RefreshGold(CachedGameState->GetCurrentMoney());
	}
	else
	{
		RefreshGold(0);
	}

	EnsureDefenderButton();
	TryBindTowerHealth();
}

void UTDHUDWidget::EnsureDefaultLayout()
{
	if (GoldText || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = Root;

	UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRow"));
	UTextBlock* CoinsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoinsLabel"));
	TDUIStyleText(CoinsLabel, NSLOCTEXT("HUD", "CoinsLabel", "Coins:"), 26, FLinearColor(1.f, 0.85f, 0.2f), true);

	GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldText"));
	TDUIStyleText(GoldText, FText::AsNumber(0), 28, FLinearColor(1.f, 0.85f, 0.2f), true);

	GoldRow->AddChildToHorizontalBox(CoinsLabel);
	if (UHorizontalBoxSlot* GoldSlot = GoldRow->AddChildToHorizontalBox(GoldText))
	{
		GoldSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	}

	UCanvasPanelSlot* GoldCanvasSlot = Root->AddChildToCanvas(GoldRow);
	GoldCanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
	GoldCanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
	GoldCanvasSlot->SetPosition(FVector2D(32.f, 24.f));
	GoldCanvasSlot->SetAutoSize(true);

	UVerticalBox* TowerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TowerBox"));
	UTextBlock* TowerLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TowerLabel"));
	TDUIStyleText(TowerLabel, NSLOCTEXT("HUD", "TowerLabel", "Tower"), 20, FLinearColor::White, true);

	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TowerBarSize"));
	BarSize->SetWidthOverride(400.f);
	BarSize->SetHeightOverride(24.f);

	TowerHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TowerHealthBar"));
	TowerHealthBar->SetPercent(1.f);
	TowerHealthBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.82f, 0.22f, 1.f));
	BarSize->AddChild(TowerHealthBar);

	TowerHealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TowerHealthText"));
	TDUIStyleText(TowerHealthText, NSLOCTEXT("HUD", "TowerHealthPlaceholder", "0 / 0"), 18, FLinearColor::White, false);

	TowerBox->AddChildToVerticalBox(TowerLabel);
	if (UVerticalBoxSlot* BarSlot = TowerBox->AddChildToVerticalBox(BarSize))
	{
		BarSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
	}
	TowerBox->AddChildToVerticalBox(TowerHealthText);

	UCanvasPanelSlot* TowerCanvasSlot = Root->AddChildToCanvas(TowerBox);
	TowerCanvasSlot->SetAnchors(FAnchors(0.5f, 0.f));
	TowerCanvasSlot->SetAlignment(FVector2D(0.5f, 0.f));
	TowerCanvasSlot->SetPosition(FVector2D(0.f, 20.f));
	TowerCanvasSlot->SetAutoSize(true);

	UHorizontalBox* ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DefenderButtonContainer"));
	DefenderButtonContainer = ButtonBox;

	UCanvasPanelSlot* ButtonCanvasSlot = Root->AddChildToCanvas(ButtonBox);
	ButtonCanvasSlot->SetAnchors(FAnchors(0.5f, 1.f));
	ButtonCanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
	ButtonCanvasSlot->SetPosition(FVector2D(0.f, -36.f));
	ButtonCanvasSlot->SetAutoSize(true);
}

void UTDHUDWidget::EnsureDefenderButton()
{
	if (!DefenderButtonContainer || DefenderButtonContainer->GetChildrenCount() > 0)
	{
		return;
	}

	auto AddBuyButton = [this](TSubclassOf<ADefenderBase> DefenderType, int32 InCost, const FText& Label)
	{
		UDefenderButtonWidget* BuyBtn = nullptr;
		if (APlayerController* PC = GetOwningPlayer())
		{
			BuyBtn = CreateWidget<UDefenderButtonWidget>(PC);
		}
		else
		{
			BuyBtn = CreateWidget<UDefenderButtonWidget>(this);
		}

		if (!BuyBtn)
		{
			return;
		}

		if (DefenderType)
		{
			BuyBtn->DefenderClassToBuild = DefenderType;
		}
		BuyBtn->Cost = InCost;
		BuyBtn->ButtonLabel = Label;
		DefenderButtonContainer->AddChild(BuyBtn);

		if (IsValid(CachedGameState))
		{
			BuyBtn->RefreshAffordability(CachedGameState->GetCurrentMoney());
		}
	};

	AddBuyButton(nullptr, 100, NSLOCTEXT("UI", "Archer", "Archer"));
	AddBuyButton(ADefenderBog::StaticClass(), 200, NSLOCTEXT("UI", "Bog", "Bog"));
}

void UTDHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(CachedGameState))
	{
		RebindPollTimer += InDeltaTime;
		if (RebindPollTimer > 0.5f)
		{
			RebindPollTimer = 0.f;
			CachedGameState = GetWorld() ? GetWorld()->GetGameState<ATDGameState>() : nullptr;
			if (IsValid(CachedGameState))
			{
				CachedGameState->OnMoneyChanged.AddDynamic(this, &UTDHUDWidget::HandleMoneyChanged);
				RefreshGold(CachedGameState->GetCurrentMoney());
			}
		}
	}

	if (!bTowerBound)
	{
		TryBindTowerHealth();
	}
}

void UTDHUDWidget::TryBindTowerHealth()
{
	if (!IsValid(CachedGameState))
	{
		return;
	}

	ACentralTowerBase* Tower = CachedGameState->GetCentralTower();
	if (!IsValid(Tower))
	{
		return;
	}

	Tower->OnHealthChanged.AddDynamic(this, &UTDHUDWidget::HandleTowerHealthChanged);
	HandleTowerHealthChanged(Tower->CurrentHealth, Tower->MaxHealth);
	bTowerBound = true;
}

void UTDHUDWidget::HandleMoneyChanged(int32 NewAmount)
{
	RefreshGold(NewAmount);
}

void UTDHUDWidget::RefreshGold(int32 GoldAmount)
{
	if (GoldText)
	{
		GoldText->SetText(FText::AsNumber(GoldAmount));
	}

	if (DefenderButtonContainer)
	{
		for (UWidget* Child : DefenderButtonContainer->GetAllChildren())
		{
			if (UDefenderButtonWidget* Btn = Cast<UDefenderButtonWidget>(Child))
			{
				Btn->RefreshAffordability(GoldAmount);
			}
		}
	}
}

void UTDHUDWidget::HandleTowerHealthChanged(float NewHealth, float InMaxHealth)
{
	if (TowerHealthBar)
	{
		const float Percent = InMaxHealth > 0.f ? FMath::Clamp(NewHealth / InMaxHealth, 0.f, 1.f) : 0.f;
		TowerHealthBar->SetPercent(Percent);
		TowerHealthBar->SetFillColorAndOpacity(Percent > 0.35f
			? FLinearColor(0.15f, 0.82f, 0.22f, 1.f)
			: FLinearColor(0.9f, 0.15f, 0.12f, 1.f));
	}
	if (TowerHealthText)
	{
		TowerHealthText->SetText(FText::Format(
			NSLOCTEXT("HUD", "TowerHealthFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(NewHealth)),
			FText::AsNumber(FMath::RoundToInt(InMaxHealth))));
	}
}
