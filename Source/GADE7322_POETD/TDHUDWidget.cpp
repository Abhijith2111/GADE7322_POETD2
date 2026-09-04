#include "TDHUDWidget.h"
#include "DefenderButtonWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/PanelWidget.h"
#include "TDGameState.h"
#include "CentralTowerBase.h"

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
		// Safe fallback so the HUD isn't blank while GameState is still spinning up.
		RefreshGold(0);
	}

	TryBindTowerHealth();
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
		return; // Tower hasn't self-registered yet — retried each Tick until it does.
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
	}
	if (TowerHealthText)
	{
		TowerHealthText->SetText(FText::Format(
			NSLOCTEXT("HUD", "TowerHealthFormat", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(NewHealth)),
			FText::AsNumber(FMath::RoundToInt(InMaxHealth))));
	}
}