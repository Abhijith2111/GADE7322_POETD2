#include "DifficultyBriefingWidget.h"
#include "TDPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

namespace
{
	void StyleBriefingText(UTextBlock* Text, const FText& Value, float Size, const FLinearColor& Color, bool bWrap)
	{
		if (!Text)
		{
			return;
		}

		Text->SetText(Value);
		if (UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")))
		{
			Text->SetFont(FSlateFontInfo(Font, Size, FName(TEXT("Bold"))));
		}
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(bWrap);
	}

	FText BuildTitle(ETDDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case ETDDifficulty::Medium:
			return NSLOCTEXT("Briefing", "MediumTitle", "Medium");
		case ETDDifficulty::Hard:
			return NSLOCTEXT("Briefing", "HardTitle", "Difficult");
		default:
			return NSLOCTEXT("Briefing", "EasyTitle", "Easy");
		}
	}

	FText BuildBody(ETDDifficulty Difficulty)
	{
		const FText Defenders = NSLOCTEXT(
			"Briefing",
			"DefendersShared",
			"Defenders\n"
			"- Archer: ranged tower that shoots enemies on the path.\n"
			"- Barracks: trains knights that fight nearby threats.\n"
			"- Bog: pulls in and digests enemies (place off the path).\n"
			"- Mine: support building.\n\n"
			"Upgrades\n"
			"Click an archer, barracks, bog, mine, or the temple. Each upgrade costs coins and adds health.\n"
			"- Archer, 100 coins: a new look, then choose health and attack speed or attack power. The last upgrade stays on that choice.\n"
			"- Barracks, 150 coins: faster training, then stronger knights trained after the upgrade.\n"
			"- Bog, 200 coins: faster digestion, then a chance to turn an eaten enemy into an ally.\n"
			"- Mine, 150 coins: faster payouts, then more coins.\n"
			"- Temple, 250 coins: harder hits, then faster attacks.\n"
			"Knights cannot be upgraded.\n\n");

		const FText Weather = NSLOCTEXT(
			"Briefing",
			"WeatherShared",
			"\n\nWeather\n"
			"Weather changes every 45 to 90 seconds. The current weather is shown at the top right.\n"
			"- Sunny: no extra effect on attackers or defenders.\n"
			"- Raining: the sky turns grey and rain falls. Bogs finish digesting 5 seconds sooner. On every difficulty, lightning can strike a random attacker or a random defender and defeat them.\n"
			"- Night: the map goes dark. Barracks take 5 seconds longer to train each knight. An attacker fighting your defenders sometimes strikes another attacker instead.\n"
			"- Windy: wind crosses the map. About one in four archer shots goes astray and can hit another enemy, one of your own defenders, or miss.");

		switch (Difficulty)
		{
		case ETDDifficulty::Medium:
			return FText::Format(
				NSLOCTEXT(
					"Briefing",
					"MediumBody",
					"{0}"
					"Enemies\n"
					"- Faster waves than Easy.\n"
					"- Opens with grunts, then mixes in ogres and trojans (up to 5 living ogres).\n"
					"- Uses 3 lanes into the temple.\n\n"
					"Limits\n"
					"- You may place at most 5 bogs.\n"
					"- Bog digestion takes about 45 seconds."
					"{1}"),
				Defenders,
				Weather);
		case ETDDifficulty::Hard:
			return FText::Format(
				NSLOCTEXT(
					"Briefing",
					"HardBody",
					"{0}"
					"Enemies\n"
					"- Spawns on every lane at once.\n"
					"- Mixed grunts, ogres, and trojans from early on.\n"
					"- Map uses 3 to 5 lanes.\n"
					"- Enemies will hunt your mines.\n\n"
					"Limits\n"
					"- You may place at most 3 bogs.\n"
					"- You may place only one barracks.\n"
					"- Bog digestion takes about 60 seconds."
					"{1}"),
				Defenders,
				Weather);
		default:
			return FText::Format(
				NSLOCTEXT(
					"Briefing",
					"EasyBody",
					"{0}"
					"Enemies\n"
					"- Slower waves that ramp up over time.\n"
					"- Starts with a few grunts before ogres and trojans appear.\n"
					"- Uses 3 lanes into the temple.\n\n"
					"Limits\n"
					"- No bog placement limit.\n"
					"- Bog digestion takes about 20 seconds."
					"{1}"),
				Defenders,
				Weather);
		}
	}
}

bool UDifficultyBriefingWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	BuildWidget();
	return bResult;
}

void UDifficultyBriefingWidget::BuildWidget()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BriefingRoot"));
	WidgetTree->RootWidget = Root;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BriefingBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.96f));
	if (UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BriefingSize"));
	SizeBox->SetWidthOverride(820.f);
	SizeBox->SetMinDesiredHeight(520.f);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BriefingPanel"));
	Panel->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.09f, 0.98f));
	SizeBox->AddChild(Panel);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BriefingColumn"));
	Panel->AddChild(Column);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BriefingTitle"));
	StyleBriefingText(TitleText, NSLOCTEXT("Briefing", "EasyTitle", "Easy"), 36.f, FLinearColor(1.f, 0.85f, 0.25f, 1.f), false);
	TitleText->SetJustification(ETextJustify::Center);

	BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BriefingBody"));
	StyleBriefingText(BodyText, FText::GetEmpty(), 18.f, FLinearColor(0.92f, 0.92f, 0.95f, 1.f), true);

	USizeBox* BodySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BriefingBodySize"));
	BodySize->SetMaxDesiredHeight(460.f);
	UScrollBox* BodyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BriefingBodyScroll"));
	BodyScroll->AddChild(BodyText);
	BodySize->AddChild(BodyScroll);

	ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueButton"));
	ContinueButton->SetBackgroundColor(FLinearColor(1.f, 0.55f, 0.05f, 1.f));
	UTextBlock* ContinueLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueLabel"));
	StyleBriefingText(ContinueLabel, NSLOCTEXT("Briefing", "Continue", "Continue"), 22.f, FLinearColor(0.05f, 0.05f, 0.05f, 1.f), false);
	ContinueLabel->SetJustification(ETextJustify::Center);
	ContinueButton->AddChild(ContinueLabel);

	auto AddRow = [Column](UWidget* Child, float BottomPad)
	{
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(28.f, 12.f, 28.f, BottomPad));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	AddRow(TitleText, 8.f);
	AddRow(BodySize, 16.f);
	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(ContinueButton))
	{
		ButtonSlot->SetPadding(FMargin(28.f, 8.f, 28.f, 28.f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(SizeBox))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D(0.f, 0.f));
		PanelSlot->SetAutoSize(true);
	}
}

void UDifficultyBriefingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UDifficultyBriefingWidget::HandleContinueClicked);
	}
}

void UDifficultyBriefingWidget::ShowForDifficulty(ETDDifficulty Difficulty, ATDPlayerController* InController)
{
	OwnerController = InController;
	if (TitleText)
	{
		TitleText->SetText(BuildTitle(Difficulty));
	}
	if (BodyText)
	{
		BodyText->SetText(BuildBody(Difficulty));
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UDifficultyBriefingWidget::HandleContinueClicked()
{
	if (ATDPlayerController* PC = OwnerController.Get())
	{
		PC->ConfirmDifficultyBriefing();
	}
}
