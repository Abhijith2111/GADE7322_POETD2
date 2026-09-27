#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

inline FSlateFontInfo TDUIFont(int32 Size, const FName Typeface = TEXT("Regular"))
{
	return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
}

inline void TDUIStyleText(UTextBlock* Text, const FText& Content, int32 Size, const FLinearColor& Color, bool bBold = false)
{
	if (!Text)
	{
		return;
	}

	Text->SetText(Content);
	Text->SetFont(TDUIFont(Size, bBold ? TEXT("Bold") : TEXT("Regular")));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetJustification(ETextJustify::Center);
}

inline UButton* TDUIMakeLabeledButton(UWidgetTree* Tree, const FName ButtonName, const FName LabelName, const FText& Label, int32 FontSize = 22)
{
	if (!Tree)
	{
		return nullptr;
	}

	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
	TDUIStyleText(Text, Label, FontSize, FLinearColor::White, true);
	Button->AddChild(Text);
	return Button;
}
