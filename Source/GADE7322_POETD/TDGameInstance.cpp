#include "TDGameInstance.h"

void UTDGameInstance::SetDifficulty(ETDDifficulty InDifficulty)
{
	Difficulty = InDifficulty;
}

ETDDifficulty UTDGameInstance::GetDifficulty() const
{
	return Difficulty;
}
