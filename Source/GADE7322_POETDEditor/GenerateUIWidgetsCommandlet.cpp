#include "GenerateUIWidgetsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CentralTowerBase.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetComponent.h"
#include "DefenderBarracks.h"
#include "DefenderBase.h"
#include "DefenderBog.h"
#include "DefenderMineShaft.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Engine/Blueprint.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "GameOverViewInterface.h"
#include "HealthDisplayInterface.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Message.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "TDGameState.h"
#include "TDPlayerController.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "WorldHealthBar.h"

namespace
{
	struct FUIGraph
	{
		UEdGraph* Graph = nullptr;
		int32 X = 0;

		template <typename NodeType>
		NodeType* Spawn(TFunctionRef<void(NodeType*)> Init)
		{
			NodeType* Node = FEdGraphSchemaAction_K2NewNode::SpawnNode<NodeType>(
				Graph, FVector2D(X, 0.f), EK2NewNodeFlags::None, Init);
			X += 300;
			return Node;
		}

		UK2Node_CallFunction* Call(UClass* Class, FName Function)
		{
			UFunction* Found = Class ? Class->FindFunctionByName(Function) : nullptr;
			if (!Found)
			{
				UE_LOG(LogTemp, Warning, TEXT("UI graph: %s has no function %s"),
					Class ? *Class->GetName() : TEXT("null"), *Function.ToString());
			}
			return Spawn<UK2Node_CallFunction>([Found, Class, Function](UK2Node_CallFunction* Node)
			{
				if (Found)
				{
					Node->SetFromFunction(Found);
				}
				else
				{
					Node->FunctionReference.SetExternalMember(Function, Class);
				}
			});
		}

		UK2Node_Message* Message(UClass* Class, FName Function)
		{
			return Spawn<UK2Node_Message>([Class, Function](UK2Node_Message* Node)
			{
				Node->FunctionReference.SetExternalMember(Function, Class);
			});
		}

		UK2Node_VariableGet* Get(FName Name)
		{
			return Spawn<UK2Node_VariableGet>([Name](UK2Node_VariableGet* Node)
			{
				Node->VariableReference.SetSelfMember(Name);
			});
		}

		UK2Node_VariableSet* Set(FName Name)
		{
			return Spawn<UK2Node_VariableSet>([Name](UK2Node_VariableSet* Node)
			{
				Node->VariableReference.SetSelfMember(Name);
			});
		}

		UK2Node_Self* Self()
		{
			return Spawn<UK2Node_Self>([](UK2Node_Self*) {});
		}
	};

	UEdGraphPin* Pin(UEdGraphNode* Node, FName Name, EEdGraphPinDirection Direction = EGPD_MAX)
	{
		if (!Node)
		{
			return nullptr;
		}
		if (Direction == EGPD_MAX)
		{
			return Node->FindPin(Name);
		}
		return Node->FindPin(Name, Direction);
	}

	FString DescribeNode(UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("null");
		}
		FString Pins;
		for (UEdGraphPin* PinOnNode : Node->Pins)
		{
			Pins += FString::Printf(TEXT(" [%s %s]"),
				PinOnNode->Direction == EGPD_Input ? TEXT("in") : TEXT("out"),
				*PinOnNode->PinName.ToString());
		}
		return Node->GetClass()->GetName() + Pins;
	}

	bool Link(UEdGraphPin* From, UEdGraphPin* To)
	{
		if (!From || !To)
		{
			UE_LOG(LogTemp, Warning, TEXT("UI graph: missing pin %s -> %s | from %s | to %s"),
				From ? *From->PinName.ToString() : TEXT("null"),
				To ? *To->PinName.ToString() : TEXT("null"),
				*DescribeNode(From ? From->GetOwningNode() : nullptr),
				*DescribeNode(To ? To->GetOwningNode() : nullptr));
			return false;
		}
		const UEdGraphSchema* Schema = From->GetSchema();
		if (!Schema || !Schema->TryCreateConnection(From, To))
		{
			UE_LOG(LogTemp, Warning, TEXT("UI graph: rejected %s -> %s | from %s | to %s"),
				*From->PinName.ToString(), *To->PinName.ToString(),
				*DescribeNode(From->GetOwningNode()), *DescribeNode(To->GetOwningNode()));
			return false;
		}
		return true;
	}

	void Then(UK2Node* From, UK2Node* To)
	{
		Link(Pin(From, UEdGraphSchema_K2::PN_Then, EGPD_Output), Pin(To, UEdGraphSchema_K2::PN_Execute, EGPD_Input));
	}

	UEdGraphPin* Value(UK2Node_VariableGet* Node, FName Name)
	{
		if (UEdGraphPin* Named = Pin(Node, Name, EGPD_Output))
		{
			return Named;
		}
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* PinOnNode : Node->Pins)
		{
			if (PinOnNode->Direction == EGPD_Output && PinOnNode->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return PinOnNode;
			}
		}
		return nullptr;
	}

	UEdGraphPin* CastOut(UK2Node_DynamicCast* Node)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* PinOnNode : Node->Pins)
		{
			if (PinOnNode->Direction == EGPD_Output && PinOnNode->PinName.ToString().StartsWith(TEXT("As")))
			{
				return PinOnNode;
			}
		}
		return nullptr;
	}

	void AddFloatVar(UBlueprint* BP, FName Name, const TCHAR* DefaultValue)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Real;
		Type.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type, DefaultValue);
	}

	void AddIntVar(UBlueprint* BP, FName Name, const TCHAR* DefaultValue)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Int;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type, DefaultValue);
	}

	void AddTextVar(UBlueprint* BP, FName Name)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Text;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type);
	}

	void AddObjectVar(UBlueprint* BP, FName Name, UClass* Class)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Object;
		Type.PinSubCategoryObject = Class;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type);
	}

	void AddClassVar(UBlueprint* BP, FName Name, UClass* MetaClass)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Class;
		Type.PinSubCategoryObject = MetaClass;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type);
	}

	void AddBoolVar(UBlueprint* BP, FName Name, const TCHAR* DefaultValue)
	{
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		FBlueprintEditorUtils::AddMemberVariable(BP, Name, Type, DefaultValue);
	}

	void StyleText(UTextBlock* Text, const FText& Content, int32 Size, const FLinearColor& Color, bool bBold)
	{
		Text->SetText(Content);
		UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		if (Font)
		{
			Text->SetFont(FSlateFontInfo(Font, static_cast<float>(Size), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"))));
		}
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
	}

	UButton* MakeButton(UWidgetTree* Tree, const FName ButtonName, const FName LabelName, const FText& Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->bIsVariable = true;
		UTextBlock* Caption = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		StyleText(Caption, Label, 22, FLinearColor(0.05f, 0.05f, 0.05f), true);
		Button->AddChild(Caption);
		return Button;
	}

	UWidgetBlueprint* CreateWidgetAsset(const FString& Name)
	{
		const FString PackageName = FString::Printf(TEXT("/Game/UI/%s"), *Name);

		IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("UI")), true);

		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().Delete(*Filename, false, true);
		if (UPackage* ExistingPackage = FindPackage(nullptr, *PackageName))
		{
			ResetLoaders(ExistingPackage);
			if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *Name))
			{
				ExistingAsset->ClearFlags(RF_Standalone | RF_Public);
				ExistingAsset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
			}
			ExistingPackage->ClearFlags(RF_Standalone | RF_Public);
			ExistingPackage->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}

		UPackage* Package = CreatePackage(*PackageName);
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UUserWidget::StaticClass();
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(
			UWidgetBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));
		if (!Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("UI graph: failed to create %s"), *Name);
			return nullptr;
		}
		Blueprint->AddToRoot();
		return Blueprint;
	}

	void CompileAndSave(UWidgetBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		FAssetRegistryModule::AssetCreated(Blueprint);
		Blueprint->MarkPackageDirty();

		const FString PackageName = Blueprint->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs))
		{
			UE_LOG(LogTemp, Error, TEXT("UI graph: failed to save %s"), *PackageName);
		}
	}

	UEdGraph* EventGraph(UWidgetBlueprint* Blueprint)
	{
		return Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
	}

	UK2Node_Event* AddOverrideEvent(FUIGraph& UI, FName EventName)
	{
		return UI.Spawn<UK2Node_Event>([EventName](UK2Node_Event* Node)
		{
			Node->EventReference.SetExternalMember(EventName, UUserWidget::StaticClass());
			Node->bOverrideFunction = true;
		});
	}

	const UK2Node_ComponentBoundEvent* BindClick(UWidgetBlueprint* Blueprint, UButton* Button, FName ButtonName)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(Blueprint->SkeletonGeneratedClass, ButtonName);
		if (!Property)
		{
			UE_LOG(LogTemp, Warning, TEXT("UI graph: %s has no button property %s"), *Blueprint->GetName(), *ButtonName.ToString());
			return nullptr;
		}

		FKismetEditorUtilities::CreateNewBoundEventForComponent(Button, TEXT("OnClicked"), Blueprint, Property, false);
		return FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, TEXT("OnClicked"), ButtonName);
	}

	UK2Node_CallFunction* CallPlayerFunction(FUIGraph& UI, UK2Node* ExecFrom, FName Function, TFunctionRef<void(UK2Node_CallFunction*)> Setup, UK2Node** ExecOut = nullptr)
	{
		if (ExecFrom && ExecFrom->GetGraph())
		{
			UI.Graph = ExecFrom->GetGraph();
		}
		UK2Node_CallFunction* GetPlayer = UI.Call(UUserWidget::StaticClass(), FName(TEXT("GetOwningPlayer")));
		UK2Node_DynamicCast* CastNode = UI.Spawn<UK2Node_DynamicCast>([](UK2Node_DynamicCast* Node)
		{
			Node->TargetType = ATDPlayerController::StaticClass();
		});
		UK2Node_CallFunction* CallNode = UI.Call(ATDPlayerController::StaticClass(), Function);
		Link(Pin(GetPlayer, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(CastNode, TEXT("Object"), EGPD_Input));
		Link(CastOut(CastNode), Pin(CallNode, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Then(ExecFrom, CastNode);
		if (Pin(CallNode, UEdGraphSchema_K2::PN_Execute, EGPD_Input))
		{
			Then(CastNode, CallNode);
			if (ExecOut)
			{
				*ExecOut = CallNode;
			}
		}
		else if (ExecOut)
		{
			*ExecOut = CastNode;
		}
		Setup(CallNode);
		return CallNode;
	}

	UEdGraph* AddUserFunction(UWidgetBlueprint* Blueprint, FName Name)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint, Name, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, true, nullptr);
		return Graph;
	}

	void BuildDefenderButton(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PurchaseSize"));
		SizeBox->SetWidthOverride(180.f);
		SizeBox->SetHeightOverride(56.f);
		UButton* Button = MakeButton(Tree, TEXT("PurchaseButton"), TEXT("CostText"), NSLOCTEXT("UI", "CostPlaceholder", "100 coins"));
		UTextBlock* CostText = Cast<UTextBlock>(Button->GetChildAt(0));
		if (CostText)
		{
			CostText->bIsVariable = true;
			CostText->Rename(TEXT("CostText"), nullptr, REN_DontCreateRedirectors);
		}
		SizeBox->AddChild(Button);
		Tree->RootWidget = SizeBox;

		AddClassVar(Blueprint, TEXT("DefenderClassToBuild"), ADefenderBase::StaticClass());
		AddIntVar(Blueprint, TEXT("Cost"), TEXT("100"));
		AddTextVar(Blueprint, TEXT("ButtonLabel"));
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		{
			FUIGraph UI;
			UI.Graph = AddUserFunction(Blueprint, TEXT("ApplyLabel"));
			UK2Node_FunctionEntry* Entry = nullptr;
			for (UEdGraphNode* Node : UI.Graph->Nodes)
			{
				Entry = Cast<UK2Node_FunctionEntry>(Node);
				if (Entry)
				{
					break;
				}
			}

			UK2Node_VariableGet* Label = UI.Get(TEXT("ButtonLabel"));
			UK2Node_CallFunction* LabelString = UI.Call(UKismetTextLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_TextToString));
			UK2Node_VariableGet* Cost = UI.Get(TEXT("Cost"));
			UK2Node_CallFunction* CostString = UI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Conv_IntToString));
			UK2Node_CallFunction* JoinLabel = UI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
			UK2Node_CallFunction* JoinCost = UI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
			UK2Node_CallFunction* JoinCoins = UI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
			UK2Node_CallFunction* ToText = UI.Call(UKismetTextLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_StringToText));
			UK2Node_VariableGet* TextGet = UI.Get(TEXT("CostText"));
			UK2Node_CallFunction* SetText = UI.Call(UTextBlock::StaticClass(), TEXT("SetText"));

			Link(Value(Label, TEXT("ButtonLabel")), Pin(LabelString, TEXT("InText"), EGPD_Input));
			Link(Pin(LabelString, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinLabel, TEXT("A"), EGPD_Input));
			if (UEdGraphPin* Newline = Pin(JoinLabel, TEXT("B"), EGPD_Input))
			{
				Newline->DefaultValue = TEXT("\n");
			}
			Link(Value(Cost, TEXT("Cost")), Pin(CostString, TEXT("InInt"), EGPD_Input));
			Link(Pin(JoinLabel, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinCost, TEXT("A"), EGPD_Input));
			Link(Pin(CostString, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinCost, TEXT("B"), EGPD_Input));
			Link(Pin(JoinCost, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinCoins, TEXT("A"), EGPD_Input));
			if (UEdGraphPin* Suffix = Pin(JoinCoins, TEXT("B"), EGPD_Input))
			{
				Suffix->DefaultValue = TEXT(" coins");
			}
			Link(Pin(JoinCoins, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(ToText, TEXT("InString"), EGPD_Input));
			Link(Value(TextGet, TEXT("CostText")), Pin(SetText, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Link(Pin(ToText, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(SetText, TEXT("InText"), EGPD_Input));
			if (Entry)
			{
				Then(Entry, SetText);
			}
		}

		{
			FUIGraph UI;
			UI.Graph = AddUserFunction(Blueprint, TEXT("RefreshAffordability"));
			UK2Node_FunctionEntry* Entry = nullptr;
			for (UEdGraphNode* Node : UI.Graph->Nodes)
			{
				Entry = Cast<UK2Node_FunctionEntry>(Node);
				if (Entry)
				{
					break;
				}
			}
			if (Entry)
			{
				CallPlayerFunction(UI, Entry, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, ApplyDefenderButtonVisual), [&UI](UK2Node_CallFunction* CallNode)
				{
					UK2Node_VariableGet* ButtonGet = UI.Get(TEXT("PurchaseButton"));
					UK2Node_VariableGet* ClassGet = UI.Get(TEXT("DefenderClassToBuild"));
					UK2Node_VariableGet* CostGet = UI.Get(TEXT("Cost"));
					Link(Value(ButtonGet, TEXT("PurchaseButton")), Pin(CallNode, TEXT("Button"), EGPD_Input));
					Link(Value(ClassGet, TEXT("DefenderClassToBuild")), Pin(CallNode, TEXT("InDefenderClass"), EGPD_Input));
					Link(Value(CostGet, TEXT("Cost")), Pin(CallNode, TEXT("Cost"), EGPD_Input));
				});
			}
		}

		const UK2Node_ComponentBoundEvent* Click = BindClick(Blueprint, Cast<UButton>(Tree->FindWidget(TEXT("PurchaseButton"))), TEXT("PurchaseButton"));
		if (Click)
		{
			FUIGraph UI;
			UI.Graph = EventGraph(Blueprint);
			UI.X = 400;
			UK2Node* AffordExec = nullptr;
			UK2Node_CallFunction* Afford = CallPlayerFunction(UI, const_cast<UK2Node_ComponentBoundEvent*>(Click), GET_FUNCTION_NAME_CHECKED(ATDPlayerController, CanAffordCost), [&UI](UK2Node_CallFunction* CallNode)
			{
				UK2Node_VariableGet* Cost = UI.Get(TEXT("Cost"));
				Link(Value(Cost, TEXT("Cost")), Pin(CallNode, TEXT("Cost"), EGPD_Input));
			}, &AffordExec);
			UK2Node_IfThenElse* Branch = UI.Spawn<UK2Node_IfThenElse>([](UK2Node_IfThenElse*) {});
			Link(Pin(Afford, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(Branch, TEXT("Condition"), EGPD_Input));
			Then(AffordExec, Branch);
			CallPlayerFunction(UI, Branch, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, SetPendingDefender), [&UI](UK2Node_CallFunction* CallNode)
			{
				UK2Node_VariableGet* DefenderClass = UI.Get(TEXT("DefenderClassToBuild"));
				UK2Node_VariableGet* Cost = UI.Get(TEXT("Cost"));
				Link(Value(DefenderClass, TEXT("DefenderClassToBuild")), Pin(CallNode, TEXT("InDefenderClass"), EGPD_Input));
				Link(Value(Cost, TEXT("Cost")), Pin(CallNode, TEXT("InCost"), EGPD_Input));
			});
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	void WireMenuClick(UWidgetBlueprint* Blueprint, FName ButtonName, TFunctionRef<void(FUIGraph&, UK2Node*)> Build)
	{
		UButton* Button = Cast<UButton>(Blueprint->WidgetTree->FindWidget(ButtonName));
		const UK2Node_ComponentBoundEvent* Click = BindClick(Blueprint, Button, ButtonName);
		if (!Click)
		{
			return;
		}
		FUIGraph UI;
		UI.Graph = EventGraph(Blueprint);
		UI.X = 500;
		Build(UI, const_cast<UK2Node_ComponentBoundEvent*>(Click));
	}

	void SetSizeVisibility(FUIGraph& UI, UK2Node*& Exec, FName SizeName, const TCHAR* Visibility)
	{
		UK2Node_VariableGet* Size = UI.Get(SizeName);
		UK2Node_CallFunction* SetVisibility = UI.Call(UWidget::StaticClass(), TEXT("SetVisibility"));
		Link(Value(Size, SizeName), Pin(SetVisibility, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		if (UEdGraphPin* Vis = Pin(SetVisibility, TEXT("InVisibility"), EGPD_Input))
		{
			Vis->DefaultValue = Visibility;
		}
		Then(Exec, SetVisibility);
		Exec = SetVisibility;
	}

	UWidgetBlueprint* BuildMenu(const FString& Name, const FText& Title, const TArray<TPair<FName, FText>>& Buttons, const TArray<FName>& Hidden, FName HeadingName = TEXT("Title"))
	{
		UWidgetBlueprint* Blueprint = CreateWidgetAsset(Name);
		if (!Blueprint)
		{
			return nullptr;
		}

		UWidgetTree* Tree = Blueprint->WidgetTree;
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
		Tree->RootWidget = Overlay;
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.62f));
		if (UOverlaySlot* DimSlot = Overlay->AddChildToOverlay(Dim))
		{
			DimSlot->SetHorizontalAlignment(HAlign_Fill);
			DimSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UVerticalBox* Menu = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
		UTextBlock* Heading = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), HeadingName);
		Heading->bIsVariable = true;
		StyleText(Heading, Title, 48, FLinearColor::White, true);
		Menu->AddChildToVerticalBox(Heading);

		for (const TPair<FName, FText>& Entry : Buttons)
		{
			const FName SizeName = FName(*(Entry.Key.ToString() + TEXT("Size")));
			USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), SizeName);
			SizeBox->bIsVariable = true;
			SizeBox->SetWidthOverride(260.f);
			SizeBox->SetHeightOverride(52.f);
			if (Hidden.Contains(Entry.Key))
			{
				SizeBox->SetVisibility(ESlateVisibility::Collapsed);
			}
			UButton* Button = MakeButton(Tree, Entry.Key, FName(*(Entry.Key.ToString() + TEXT("Label"))), Entry.Value);
			SizeBox->AddChild(Button);
			if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(SizeBox))
			{
				Slot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
				Slot->SetHorizontalAlignment(HAlign_Center);
			}
		}

		if (UOverlaySlot* MenuSlot = Overlay->AddChildToOverlay(Menu))
		{
			MenuSlot->SetHorizontalAlignment(HAlign_Center);
			MenuSlot->SetVerticalAlignment(VAlign_Center);
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}
}

UGenerateUIWidgetsCommandlet::UGenerateUIWidgetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UGenerateUIWidgetsCommandlet::Main(const FString& Params)
{
	UWidgetBlueprint* ButtonBP = CreateWidgetAsset(TEXT("WBP_DefenderButton"));
	if (ButtonBP)
	{
		BuildDefenderButton(ButtonBP);
		CompileAndSave(ButtonBP);
	}

	UWidgetBlueprint* MainMenu = BuildMenu(TEXT("WBP_MainMenu"), NSLOCTEXT("UI", "MainMenuTitle", "Protect the Temple"),
		{
			{ TEXT("StartButton"), NSLOCTEXT("UI", "StartGame", "Start Game") },
			{ TEXT("EasyButton"), NSLOCTEXT("UI", "Easy", "Easy") },
			{ TEXT("MediumButton"), NSLOCTEXT("UI", "Medium", "Medium") },
			{ TEXT("HardButton"), NSLOCTEXT("UI", "Hard", "Difficult") },
			{ TEXT("QuitButton"), NSLOCTEXT("UI", "QuitGame", "Quit") }
		},
		{ TEXT("EasyButton"), TEXT("MediumButton"), TEXT("HardButton") });
	if (MainMenu)
	{
		WireMenuClick(MainMenu, TEXT("StartButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			UK2Node* Exec = Click;
			SetSizeVisibility(UI, Exec, TEXT("StartButtonSize"), TEXT("Collapsed"));
			SetSizeVisibility(UI, Exec, TEXT("EasyButtonSize"), TEXT("Visible"));
			SetSizeVisibility(UI, Exec, TEXT("MediumButtonSize"), TEXT("Visible"));
			SetSizeVisibility(UI, Exec, TEXT("HardButtonSize"), TEXT("Visible"));
		});
		auto StartAt = [](ETDDifficulty Difficulty)
		{
			return [Difficulty](FUIGraph& UI, UK2Node* Click)
			{
				CallPlayerFunction(UI, Click, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, StartMatchFromMenu), [Difficulty](UK2Node_CallFunction* CallNode)
				{
					if (UEdGraphPin* Diff = Pin(CallNode, TEXT("Difficulty"), EGPD_Input))
					{
						Diff->DefaultValue = StaticEnum<ETDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty));
					}
				});
			};
		};
		WireMenuClick(MainMenu, TEXT("EasyButton"), StartAt(ETDDifficulty::Easy));
		WireMenuClick(MainMenu, TEXT("MediumButton"), StartAt(ETDDifficulty::Medium));
		WireMenuClick(MainMenu, TEXT("HardButton"), StartAt(ETDDifficulty::Hard));
		WireMenuClick(MainMenu, TEXT("QuitButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			UK2Node_CallFunction* Quit = UI.Call(UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, QuitGame));
			UK2Node_CallFunction* Player = UI.Call(UUserWidget::StaticClass(), FName(TEXT("GetOwningPlayer")));
			Link(Pin(UI.Self(), UEdGraphSchema_K2::PN_Self, EGPD_Output), Pin(Quit, TEXT("WorldContextObject"), EGPD_Input));
			Link(Pin(Player, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(Quit, TEXT("SpecificPlayer"), EGPD_Input));
			if (UEdGraphPin* Pref = Pin(Quit, TEXT("QuitPreference"), EGPD_Input))
			{
				Pref->DefaultValue = TEXT("Quit");
			}
			Then(Click, Quit);
		});
		CompileAndSave(MainMenu);
	}

	UWidgetBlueprint* PauseMenu = BuildMenu(TEXT("WBP_PauseMenu"), NSLOCTEXT("UI", "Paused", "Paused"),
		{
			{ TEXT("ResumeButton"), NSLOCTEXT("UI", "Resume", "Resume") },
			{ TEXT("RestartButton"), NSLOCTEXT("UI", "Restart", "Restart") },
			{ TEXT("QuitToMenuButton"), NSLOCTEXT("UI", "MainMenu", "Main Menu") }
		},
		{});
	if (PauseMenu)
	{
		WireMenuClick(PauseMenu, TEXT("ResumeButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			CallPlayerFunction(UI, Click, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, ResumeFromPause), [](UK2Node_CallFunction*) {});
		});
		WireMenuClick(PauseMenu, TEXT("RestartButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			CallPlayerFunction(UI, Click, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, RestartMatch), [](UK2Node_CallFunction*) {});
		});
		WireMenuClick(PauseMenu, TEXT("QuitToMenuButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			CallPlayerFunction(UI, Click, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, ReturnToMainMenu), [](UK2Node_CallFunction*) {});
		});
		CompileAndSave(PauseMenu);
	}

	UWidgetBlueprint* GameOver = BuildMenu(TEXT("WBP_GameOver"), NSLOCTEXT("UI", "Defeat", "GAME OVER"),
		{
			{ TEXT("RestartButton"), NSLOCTEXT("UI", "Restart", "Restart") },
			{ TEXT("QuitToMenuButton"), NSLOCTEXT("UI", "MainMenu", "Main Menu") }
		},
		{},
		TEXT("ResultText"));
	if (GameOver)
	{
		FBlueprintEditorUtils::ImplementNewInterface(GameOver, FTopLevelAssetPath(TEXT("/Script/GADE7322_POETD.GameOverViewInterface")));
		FKismetEditorUtilities::CompileBlueprint(GameOver);

		UEdGraph* ShowResult = nullptr;
		for (UEdGraph* Graph : GameOver->FunctionGraphs)
		{
			if (Graph && Graph->GetName().Contains(TEXT("ShowResult")))
			{
				ShowResult = Graph;
				break;
			}
		}
		if (ShowResult)
		{
			FUIGraph UI;
			UI.Graph = ShowResult;
			UK2Node_FunctionEntry* Entry = nullptr;
			for (UEdGraphNode* Node : ShowResult->Nodes)
			{
				Entry = Cast<UK2Node_FunctionEntry>(Node);
				if (Entry)
				{
					break;
				}
			}
			UK2Node_IfThenElse* Branch = UI.Spawn<UK2Node_IfThenElse>([](UK2Node_IfThenElse*) {});
			UK2Node_VariableGet* ResultA = UI.Get(TEXT("ResultText"));
			UK2Node_CallFunction* VictoryText = UI.Call(UTextBlock::StaticClass(), TEXT("SetText"));
			UK2Node_VariableGet* ResultB = UI.Get(TEXT("ResultText"));
			UK2Node_CallFunction* DefeatText = UI.Call(UTextBlock::StaticClass(), TEXT("SetText"));
			if (Entry)
			{
				Link(Pin(Entry, TEXT("bVictory"), EGPD_Output), Pin(Branch, TEXT("Condition"), EGPD_Input));
				Then(Entry, Branch);
			}
			Link(Value(ResultA, TEXT("ResultText")), Pin(VictoryText, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Link(Value(ResultB, TEXT("ResultText")), Pin(DefeatText, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			if (UEdGraphPin* Victory = Pin(VictoryText, TEXT("InText"), EGPD_Input))
			{
				Victory->DefaultTextValue = NSLOCTEXT("UI", "Victory", "VICTORY");
			}
			if (UEdGraphPin* Defeat = Pin(DefeatText, TEXT("InText"), EGPD_Input))
			{
				Defeat->DefaultTextValue = NSLOCTEXT("UI", "Defeat", "GAME OVER");
			}
			Link(Pin(Branch, TEXT("then"), EGPD_Output), Pin(VictoryText, UEdGraphSchema_K2::PN_Execute, EGPD_Input));
			Link(Pin(Branch, TEXT("else"), EGPD_Output), Pin(DefeatText, UEdGraphSchema_K2::PN_Execute, EGPD_Input));
		}

		WireMenuClick(GameOver, TEXT("RestartButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			CallPlayerFunction(UI, Click, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, RestartMatch), [](UK2Node_CallFunction*) {});
		});
		WireMenuClick(GameOver, TEXT("QuitToMenuButton"), [](FUIGraph& UI, UK2Node* Click)
		{
			CallPlayerFunction(UI, Click, GET_FUNCTION_NAME_CHECKED(ATDPlayerController, ReturnToMainMenu), [](UK2Node_CallFunction*) {});
		});
		CompileAndSave(GameOver);
	}

	UWidgetBlueprint* Health = CreateWidgetAsset(TEXT("WBP_HealthBar"));
	if (Health)
	{
		UWidgetTree* Tree = Health->WidgetTree;
		USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthSize"));
		SizeBox->SetWidthOverride(120.f);
		SizeBox->SetHeightOverride(16.f);
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
		Bar->bIsVariable = true;
		Bar->SetPercent(1.f);
		Bar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.82f, 0.22f, 1.f));
		SizeBox->AddChild(Bar);
		Tree->RootWidget = SizeBox;
		FKismetEditorUtilities::CompileBlueprint(Health);

		FUIGraph TickUI;
		TickUI.Graph = EventGraph(Health);
		UK2Node_Event* Tick = AddOverrideEvent(TickUI, GET_FUNCTION_NAME_CHECKED(UUserWidget, Tick));
		UK2Node_CallFunction* UpdateBar = TickUI.Call(UWorldHealthBarLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UWorldHealthBarLibrary, TickHealthBar));
		Link(Pin(TickUI.Self(), UEdGraphSchema_K2::PN_Self, EGPD_Output), Pin(UpdateBar, TEXT("Widget"), EGPD_Input));
		Link(Pin(Tick, TEXT("InDeltaTime"), EGPD_Output), Pin(UpdateBar, TEXT("DeltaSeconds"), EGPD_Input));
		Then(Tick, UpdateBar);


		CompileAndSave(Health);
	}

	UClass* ButtonClass = ButtonBP ? ButtonBP->GeneratedClass : nullptr;
	if (!ButtonClass)
	{
		ButtonClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/UI/WBP_DefenderButton.WBP_DefenderButton_C"));
	}
	if (!ButtonClass && ButtonBP)
	{
		ButtonClass = ButtonBP->SkeletonGeneratedClass;
	}
	UE_LOG(LogTemp, Log, TEXT("UI graph: defender button class %s status %d"),
		ButtonClass ? *ButtonClass->GetPathName() : TEXT("null"),
		ButtonBP ? static_cast<int32>(ButtonBP->Status) : -1);

	UWidgetBlueprint* HUD = CreateWidgetAsset(TEXT("WBP_TDHUD"));
	if (HUD && ButtonClass)
	{
		UWidgetTree* Tree = HUD->WidgetTree;
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		Tree->RootWidget = Root;

		UHorizontalBox* GoldRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoldRow"));
		UTextBlock* CoinsLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoinsLabel"));
		StyleText(CoinsLabel, NSLOCTEXT("HUD", "CoinsLabel", "Coins:"), 26, FLinearColor(1.f, 0.85f, 0.2f), true);
		UTextBlock* GoldText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldText"));
		GoldText->bIsVariable = true;
		StyleText(GoldText, FText::AsNumber(0), 28, FLinearColor(1.f, 0.85f, 0.2f), true);
		GoldRow->AddChildToHorizontalBox(CoinsLabel);
		if (UHorizontalBoxSlot* GoldSlot = GoldRow->AddChildToHorizontalBox(GoldText))
		{
			GoldSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}
		if (UCanvasPanelSlot* GoldCanvas = Root->AddChildToCanvas(GoldRow))
		{
			GoldCanvas->SetAnchors(FAnchors(0.f, 0.f));
			GoldCanvas->SetPosition(FVector2D(32.f, 24.f));
			GoldCanvas->SetAutoSize(true);
		}

		UVerticalBox* TowerBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TowerBox"));
		UTextBlock* TowerLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TowerLabel"));
		StyleText(TowerLabel, NSLOCTEXT("HUD", "TowerLabel", "Tower"), 20, FLinearColor::White, true);
		USizeBox* BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TowerBarSize"));
		BarSize->SetWidthOverride(400.f);
		BarSize->SetHeightOverride(24.f);
		UProgressBar* TowerBar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TowerHealthBar"));
		TowerBar->bIsVariable = true;
		TowerBar->SetPercent(1.f);
		TowerBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.82f, 0.22f, 1.f));
		BarSize->AddChild(TowerBar);
		UTextBlock* TowerText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TowerHealthText"));
		TowerText->bIsVariable = true;
		StyleText(TowerText, NSLOCTEXT("HUD", "TowerHealthPlaceholder", "0 / 0"), 18, FLinearColor::White, false);
		TowerBox->AddChildToVerticalBox(TowerLabel);
		TowerBox->AddChildToVerticalBox(BarSize);
		TowerBox->AddChildToVerticalBox(TowerText);
		if (UCanvasPanelSlot* TowerCanvas = Root->AddChildToCanvas(TowerBox))
		{
			TowerCanvas->SetAnchors(FAnchors(0.5f, 0.f));
			TowerCanvas->SetAlignment(FVector2D(0.5f, 0.f));
			TowerCanvas->SetPosition(FVector2D(0.f, 20.f));
			TowerCanvas->SetAutoSize(true);
		}

		UHorizontalBox* Buttons = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DefenderButtonContainer"));
		Buttons->bIsVariable = true;
		if (UCanvasPanelSlot* ButtonCanvas = Root->AddChildToCanvas(Buttons))
		{
			ButtonCanvas->SetAnchors(FAnchors(0.5f, 1.f));
			ButtonCanvas->SetAlignment(FVector2D(0.5f, 1.f));
			ButtonCanvas->SetPosition(FVector2D(0.f, -36.f));
			ButtonCanvas->SetAutoSize(true);
		}

		AddObjectVar(HUD, TEXT("ArcherButton"), ButtonClass);
		AddObjectVar(HUD, TEXT("BogButton"), ButtonClass);
		AddObjectVar(HUD, TEXT("KnightsButton"), ButtonClass);
		AddObjectVar(HUD, TEXT("MineButton"), ButtonClass);
		AddBoolVar(HUD, TEXT("bTowerBound"), TEXT("false"));
		FKismetEditorUtilities::CompileBlueprint(HUD);

		FUIGraph TickUI;
		TickUI.Graph = EventGraph(HUD);
		UK2Node_Event* Tick = AddOverrideEvent(TickUI, GET_FUNCTION_NAME_CHECKED(UUserWidget, Tick));
		UK2Node_CallFunction* GameState = TickUI.Call(UGameplayStatics::StaticClass(), GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetGameState));
		UK2Node_DynamicCast* CastState = TickUI.Spawn<UK2Node_DynamicCast>([](UK2Node_DynamicCast* Node)
		{
			Node->TargetType = ATDGameState::StaticClass();
		});
		UK2Node_CallFunction* Money = TickUI.Call(ATDGameState::StaticClass(), GET_FUNCTION_NAME_CHECKED(ATDGameState, GetCurrentMoney));
		UK2Node_CallFunction* MoneyText = TickUI.Call(UKismetTextLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_IntToText));
		UK2Node_VariableGet* GoldGet = TickUI.Get(TEXT("GoldText"));
		UK2Node_CallFunction* SetGold = TickUI.Call(UTextBlock::StaticClass(), TEXT("SetText"));
		Link(Pin(TickUI.Self(), UEdGraphSchema_K2::PN_Self, EGPD_Output), Pin(GameState, TEXT("WorldContextObject"), EGPD_Input));
		Link(Pin(GameState, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(CastState, TEXT("Object"), EGPD_Input));
		Link(CastOut(CastState), Pin(Money, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Link(Pin(Money, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(MoneyText, TEXT("Value"), EGPD_Input));
		Link(Pin(MoneyText, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(SetGold, TEXT("InText"), EGPD_Input));
		Link(Value(GoldGet, TEXT("GoldText")), Pin(SetGold, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Then(Tick, CastState);
		Then(CastState, SetGold);

		UK2Node_CallFunction* Tower = TickUI.Call(ATDGameState::StaticClass(), GET_FUNCTION_NAME_CHECKED(ATDGameState, GetCentralTower));
		UK2Node_CallFunction* Current = TickUI.Call(ACentralTowerBase::StaticClass(), GET_FUNCTION_NAME_CHECKED(ACentralTowerBase, GetHealthPercent));
		UK2Node_VariableGet* TowerBarGet = TickUI.Get(TEXT("TowerHealthBar"));
		UK2Node_CallFunction* SetTower = TickUI.Call(UProgressBar::StaticClass(), TEXT("SetPercent"));
		Link(CastOut(CastState), Pin(Tower, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Link(Pin(Tower, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(Current, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Link(Pin(Current, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(SetTower, TEXT("InPercent"), EGPD_Input));
		Link(Value(TowerBarGet, TEXT("TowerHealthBar")), Pin(SetTower, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		UK2Node_CallFunction* Low = TickUI.Call(UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_DoubleDouble));
		UK2Node_IfThenElse* LowBranch = TickUI.Spawn<UK2Node_IfThenElse>([](UK2Node_IfThenElse*) {});
		UK2Node_VariableGet* RedBar = TickUI.Get(TEXT("TowerHealthBar"));
		UK2Node_VariableGet* HealthyBar = TickUI.Get(TEXT("TowerHealthBar"));
		UK2Node_CallFunction* Red = TickUI.Call(UProgressBar::StaticClass(), TEXT("SetFillColorAndOpacity"));
		UK2Node_CallFunction* Healthy = TickUI.Call(UProgressBar::StaticClass(), TEXT("SetFillColorAndOpacity"));
		UK2Node_VariableGet* CurrentHealth = TickUI.Spawn<UK2Node_VariableGet>([](UK2Node_VariableGet* Node)
		{
			Node->VariableReference.SetExternalMember(TEXT("CurrentHealth"), ACentralTowerBase::StaticClass());
		});
		UK2Node_VariableGet* MaxHealth = TickUI.Spawn<UK2Node_VariableGet>([](UK2Node_VariableGet* Node)
		{
			Node->VariableReference.SetExternalMember(TEXT("MaxHealth"), ACentralTowerBase::StaticClass());
		});
		UK2Node_CallFunction* CurrentString = TickUI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Conv_DoubleToString));
		UK2Node_CallFunction* MaxString = TickUI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Conv_DoubleToString));
		UK2Node_CallFunction* JoinHealth = TickUI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
		UK2Node_CallFunction* JoinMax = TickUI.Call(UKismetStringLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetStringLibrary, Concat_StrStr));
		UK2Node_CallFunction* HealthText = TickUI.Call(UKismetTextLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetTextLibrary, Conv_StringToText));
		UK2Node_VariableGet* TowerTextGet = TickUI.Get(TEXT("TowerHealthText"));
		UK2Node_CallFunction* SetTowerText = TickUI.Call(UTextBlock::StaticClass(), TEXT("SetText"));
		Link(Pin(Current, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(Low, TEXT("A"), EGPD_Input));
		if (UEdGraphPin* Danger = Pin(Low, TEXT("B"), EGPD_Input))
		{
			Danger->DefaultValue = TEXT("0.35");
		}
		Link(Pin(Low, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(LowBranch, TEXT("Condition"), EGPD_Input));
		Link(Value(RedBar, TEXT("TowerHealthBar")), Pin(Red, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Link(Value(HealthyBar, TEXT("TowerHealthBar")), Pin(Healthy, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		if (UEdGraphPin* RedColor = Pin(Red, TEXT("InColor"), EGPD_Input))
		{
			RedColor->DefaultValue = TEXT("(R=0.85,G=0.12,B=0.1,A=1.0)");
		}
		if (UEdGraphPin* HealthyColor = Pin(Healthy, TEXT("InColor"), EGPD_Input))
		{
			HealthyColor->DefaultValue = TEXT("(R=0.15,G=0.82,B=0.22,A=1.0)");
		}
		Link(Pin(Tower, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(CurrentHealth, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Link(Pin(Tower, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(MaxHealth, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Link(Value(CurrentHealth, TEXT("CurrentHealth")), Pin(CurrentString, TEXT("InDouble"), EGPD_Input));
		Link(Value(MaxHealth, TEXT("MaxHealth")), Pin(MaxString, TEXT("InDouble"), EGPD_Input));
		Link(Pin(CurrentString, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinHealth, TEXT("A"), EGPD_Input));
		if (UEdGraphPin* Slash = Pin(JoinHealth, TEXT("B"), EGPD_Input))
		{
			Slash->DefaultValue = TEXT(" / ");
		}
		Link(Pin(JoinHealth, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinMax, TEXT("A"), EGPD_Input));
		Link(Pin(MaxString, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(JoinMax, TEXT("B"), EGPD_Input));
		Link(Pin(JoinMax, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(HealthText, TEXT("InString"), EGPD_Input));
		Link(Pin(HealthText, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(SetTowerText, TEXT("InText"), EGPD_Input));
		Link(Value(TowerTextGet, TEXT("TowerHealthText")), Pin(SetTowerText, UEdGraphSchema_K2::PN_Self, EGPD_Input));
		Then(SetGold, SetTower);
		Then(SetTower, SetTowerText);
		Then(SetTowerText, LowBranch);
		Link(Pin(LowBranch, TEXT("then"), EGPD_Output), Pin(Red, UEdGraphSchema_K2::PN_Execute, EGPD_Input));
		Link(Pin(LowBranch, TEXT("else"), EGPD_Output), Pin(Healthy, UEdGraphSchema_K2::PN_Execute, EGPD_Input));

		auto Refresh = [&](FName ButtonVar, UK2Node* ExecFrom) -> UK2Node*
		{
			UK2Node_VariableGet* Btn = TickUI.Get(ButtonVar);
			UK2Node_CallFunction* RefreshCall = TickUI.Call(ButtonClass, TEXT("RefreshAffordability"));
			Link(Value(Btn, ButtonVar), Pin(RefreshCall, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Then(ExecFrom, RefreshCall);
			return RefreshCall;
		};
		UK2Node* ArcherRefresh = Refresh(TEXT("ArcherButton"), Red);
		Then(Healthy, ArcherRefresh);
		UK2Node* Last = ArcherRefresh;
		Last = Refresh(TEXT("BogButton"), Last);
		Last = Refresh(TEXT("KnightsButton"), Last);
		Refresh(TEXT("MineButton"), Last);

		FUIGraph ConstructUI;
		ConstructUI.Graph = EventGraph(HUD);
		ConstructUI.X = 200;
		UK2Node_Event* Construct = AddOverrideEvent(ConstructUI, GET_FUNCTION_NAME_CHECKED(UUserWidget, Construct));
		UK2Node* SpawnExec = Construct;
		auto SpawnBuy = [&](FName StoredName, UClass* DefenderType, int32 Cost, const FText& Label)
		{
			UK2Node_CallFunction* Create = ConstructUI.Call(UWidgetBlueprintLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UWidgetBlueprintLibrary, Create));
			UK2Node_CallFunction* Owner = ConstructUI.Call(UUserWidget::StaticClass(), FName(TEXT("GetOwningPlayer")));
			UK2Node_DynamicCast* CastButton = ConstructUI.Spawn<UK2Node_DynamicCast>([&](UK2Node_DynamicCast* Node)
			{
				Node->TargetType = ButtonClass;
			});
			UK2Node_VariableSet* Store = ConstructUI.Set(StoredName);
			UK2Node_VariableSet* SetClass = ConstructUI.Spawn<UK2Node_VariableSet>([&](UK2Node_VariableSet* Node)
			{
				Node->VariableReference.SetExternalMember(TEXT("DefenderClassToBuild"), ButtonClass);
			});
			UK2Node_VariableSet* SetCost = ConstructUI.Spawn<UK2Node_VariableSet>([&](UK2Node_VariableSet* Node)
			{
				Node->VariableReference.SetExternalMember(TEXT("Cost"), ButtonClass);
			});
			UK2Node_VariableSet* SetLabel = ConstructUI.Spawn<UK2Node_VariableSet>([&](UK2Node_VariableSet* Node)
			{
				Node->VariableReference.SetExternalMember(TEXT("ButtonLabel"), ButtonClass);
			});
			UK2Node_CallFunction* Apply = ConstructUI.Call(ButtonClass, TEXT("ApplyLabel"));
			UK2Node_VariableGet* Row = ConstructUI.Get(TEXT("DefenderButtonContainer"));
			UK2Node_CallFunction* Add = ConstructUI.Call(UPanelWidget::StaticClass(), TEXT("AddChild"));

			Link(Pin(ConstructUI.Self(), UEdGraphSchema_K2::PN_Self, EGPD_Output), Pin(Create, TEXT("WorldContextObject"), EGPD_Input));
			if (UEdGraphPin* TypePin = Pin(Create, TEXT("WidgetType"), EGPD_Input))
			{
				TypePin->DefaultObject = ButtonClass;
			}
			Link(Pin(Owner, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(Create, TEXT("OwningPlayer"), EGPD_Input));
			Link(Pin(Create, UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Pin(CastButton, TEXT("Object"), EGPD_Input));
			Link(CastOut(CastButton), Pin(Store, StoredName, EGPD_Input));
			Link(CastOut(CastButton), Pin(SetClass, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Link(CastOut(CastButton), Pin(SetCost, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Link(CastOut(CastButton), Pin(SetLabel, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Link(CastOut(CastButton), Pin(Apply, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			if (UEdGraphPin* ClassPin = Pin(SetClass, TEXT("DefenderClassToBuild"), EGPD_Input))
			{
				ClassPin->DefaultObject = DefenderType;
			}
			if (UEdGraphPin* CostPin = Pin(SetCost, TEXT("Cost"), EGPD_Input))
			{
				CostPin->DefaultValue = FString::FromInt(Cost);
			}
			if (UEdGraphPin* LabelPin = Pin(SetLabel, TEXT("ButtonLabel"), EGPD_Input))
			{
				LabelPin->DefaultTextValue = Label;
			}
			Link(Value(Row, TEXT("DefenderButtonContainer")), Pin(Add, UEdGraphSchema_K2::PN_Self, EGPD_Input));
			Link(CastOut(CastButton), Pin(Add, TEXT("Content"), EGPD_Input));
			Then(SpawnExec, Create);
			Then(Create, CastButton);
			Then(CastButton, Store);
			Then(Store, SetClass);
			Then(SetClass, SetCost);
			Then(SetCost, SetLabel);
			Then(SetLabel, Apply);
			Then(Apply, Add);
			SpawnExec = Add;
		};

		UClass* ArcherClass = LoadClass<ADefenderBase>(nullptr, TEXT("/Game/Gameplay/LevelObjects/BP_DefenderBase.BP_DefenderBase_C"));
		SpawnBuy(TEXT("ArcherButton"), ArcherClass ? ArcherClass : ADefenderBase::StaticClass(), 100, NSLOCTEXT("UI", "Archer", "Archer"));
		SpawnBuy(TEXT("BogButton"), ADefenderBog::StaticClass(), 200, NSLOCTEXT("UI", "Bog", "Bog"));
		SpawnBuy(TEXT("KnightsButton"), ADefenderBarracks::StaticClass(), 250, NSLOCTEXT("UI", "Knights", "Knights"));
		SpawnBuy(TEXT("MineButton"), ADefenderMineShaft::StaticClass(), 0, NSLOCTEXT("UI", "Mine", "Mine"));

		CompileAndSave(HUD);
	}

	UE_LOG(LogTemp, Log, TEXT("UI widgets generated under /Game/UI"));
	return 0;
}
