#include "TDHUDWidget.h"
#include "DefenderButtonWidget.h"
#include "DefenderBog.h"
#include "DefenderBarracks.h"
#include "DefenderMineShaft.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "TDGameState.h"
#include "CentralTowerBase.h"
#include "UObject/ConstructorHelpers.h"

UTDHUDWidget::UTDHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UDefenderButtonWidget> ButtonBP(TEXT("/Game/UI/WBP_DefenderButton"));
	if (ButtonBP.Succeeded())
	{
		DefenderButtonClass = ButtonBP.Class;
	}
}

void UTDHUDWidget::NativeConstruct()
{
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

void UTDHUDWidget::EnsureDefenderButton()
{
	if (!DefenderButtonContainer || DefenderButtonContainer->GetChildrenCount() > 0)
	{
		return;
	}

	if (!DefenderButtonClass)
	{
		DefenderButtonClass = LoadClass<UDefenderButtonWidget>(nullptr, TEXT("/Game/UI/WBP_DefenderButton.WBP_DefenderButton_C"));
	}
	if (!DefenderButtonClass)
	{
		UE_LOG(LogTemp, Error, TEXT("TDHUDWidget: /Game/UI/WBP_DefenderButton was not found."));
		DefenderButtonClass = UDefenderButtonWidget::StaticClass();
	}

	auto AddBuyButton = [this](TSubclassOf<ADefenderBase> DefenderType, int32 InCost, const FText& Label)
	{
		UDefenderButtonWidget* BuyBtn = nullptr;
		if (APlayerController* PC = GetOwningPlayer())
		{
			BuyBtn = CreateWidget<UDefenderButtonWidget>(PC, DefenderButtonClass);
		}
		else
		{
			BuyBtn = CreateWidget<UDefenderButtonWidget>(this, DefenderButtonClass);
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
		BuyBtn->UpdateDisplayedCost();

		if (IsValid(CachedGameState))
		{
			BuyBtn->RefreshAffordability(CachedGameState->GetCurrentMoney());
		}
	};

	AddBuyButton(nullptr, 100, NSLOCTEXT("UI", "Archer", "Archer"));
	AddBuyButton(ADefenderBog::StaticClass(), 200, NSLOCTEXT("UI", "Bog", "Bog"));
	AddBuyButton(ADefenderBarracks::StaticClass(), 250, NSLOCTEXT("UI", "Knights", "Knights"));
	AddBuyButton(ADefenderMineShaft::StaticClass(), 0, NSLOCTEXT("UI", "Mine", "Mine"));
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
