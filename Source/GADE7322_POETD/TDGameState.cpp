#include "TDGameState.h"

ATDGameState::ATDGameState()
{
	PlayerMoney = 0;
}

void ATDGameState::BeginPlay()
{
	Super::BeginPlay();

	PlayerMoney = StartingMoney;
	OnMoneyChanged.Broadcast(PlayerMoney);

	UE_LOG(LogTemp, Log, TEXT("TDGameState: Economy initialised. Starting money: %d"), PlayerMoney);
}

void ATDGameState::AddMoney(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	PlayerMoney += Amount;
	OnMoneyChanged.Broadcast(PlayerMoney);
	OnMoneyEarned.Broadcast(Amount, PlayerMoney);

	UE_LOG(LogTemp, Log, TEXT("TDGameState: +%d gold earned. Total: %d"), Amount, PlayerMoney);
}

bool ATDGameState::SpendMoney(int32 Amount)
{
	if (Amount <= 0)
	{
		return false;
	}

	if (PlayerMoney < Amount)
	{
		UE_LOG(LogTemp, Warning, TEXT("TDGameState: Cannot spend %d gold - only %d available."), Amount, PlayerMoney);
		return false;
	}

	PlayerMoney -= Amount;
	OnMoneyChanged.Broadcast(PlayerMoney);
	OnMoneySpent.Broadcast(Amount, PlayerMoney);

	UE_LOG(LogTemp, Log, TEXT("TDGameState: -%d gold spent. Remaining: %d"), Amount, PlayerMoney);
	return true;
}

bool ATDGameState::CanAfford(int32 Amount) const
{
	return PlayerMoney >= Amount;
}

int32 ATDGameState::GetCurrentMoney() const
{
	return PlayerMoney;
}