#include "WeatherStatusWidget.h"
#include "WeatherManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"

bool UWeatherStatusWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	BuildWidget();
	return bResult;
}

void UWeatherStatusWidget::BuildWidget()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WeatherRoot"));
	WidgetTree->RootWidget = Root;

	WeatherText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WeatherText"));
	WeatherText->SetText(NSLOCTEXT("Weather", "Sunny", "Sunny"));
	WeatherText->SetJustification(ETextJustify::Right);
	if (UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")))
	{
		WeatherText->SetFont(FSlateFontInfo(Font, 34, FName(TEXT("Bold"))));
	}
	WeatherText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.95f)));
	WeatherText->SetShadowOffset(FVector2D(1.5f, 1.5f));
	WeatherText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));

	if (UCanvasPanelSlot* TextSlot = Root->AddChildToCanvas(WeatherText))
	{
		TextSlot->SetAnchors(FAnchors(1.f, 0.f));
		TextSlot->SetAlignment(FVector2D(1.f, 0.f));
		TextSlot->SetPosition(FVector2D(-36.f, 28.f));
		TextSlot->SetAutoSize(true);
	}
}

void UWeatherStatusWidget::NativeDestruct()
{
	UnbindFromWeather();
	Super::NativeDestruct();
}

void UWeatherStatusWidget::BindToWeather(AWeatherManager* Weather)
{
	UnbindFromWeather();
	if (!Weather)
	{
		return;
	}

	BoundWeather = Weather;
	Weather->OnWeatherChanged.AddDynamic(this, &UWeatherStatusWidget::HandleWeatherChanged);
	SetWeather(Weather->GetCurrentWeather());
}

void UWeatherStatusWidget::UnbindFromWeather()
{
	if (AWeatherManager* Weather = BoundWeather.Get())
	{
		Weather->OnWeatherChanged.RemoveDynamic(this, &UWeatherStatusWidget::HandleWeatherChanged);
	}
	BoundWeather.Reset();
}

void UWeatherStatusWidget::HandleWeatherChanged(EWeatherType NewWeather)
{
	SetWeather(NewWeather);
}

void UWeatherStatusWidget::SetWeather(EWeatherType Weather)
{
	if (!WeatherText)
	{
		return;
	}

	switch (Weather)
	{
	case EWeatherType::Sunny:
		WeatherText->SetText(NSLOCTEXT("Weather", "Sunny", "Sunny"));
		break;
	case EWeatherType::Rainy:
		WeatherText->SetText(NSLOCTEXT("Weather", "Raining", "Raining"));
		break;
	case EWeatherType::Night:
		WeatherText->SetText(NSLOCTEXT("Weather", "Night", "Night"));
		break;
	case EWeatherType::Windy:
		WeatherText->SetText(NSLOCTEXT("Weather", "Windy", "Windy"));
		break;
	default:
		WeatherText->SetText(NSLOCTEXT("Weather", "Sunny", "Sunny"));
		break;
	}
}
