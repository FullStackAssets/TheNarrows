#include "MHGameHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "MHCollectibleSubsystem.h"
#include "MHDialogueSubsystem.h"
#include "MHGameModeBase.h"
#include "MHGameStateSubsystem.h"
#include "MHMinimapCaptureActor.h"
#include "MHMissionSubsystem.h"
#include "MHPlayerCharacter.h"
#include "MHRadioSubsystem.h"
#include "MHWantedSubsystem.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"

namespace
{
UTextBlock* MakeHudTextBlock(UWidgetTree* Tree, const FName& Name, int32 FontSize, const FLinearColor& Color)
{
    UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    FSlateFontInfo FontInfo = TextBlock->GetFont();
    FontInfo.Size = FontSize;
    TextBlock->SetFont(FontInfo);
    TextBlock->SetColorAndOpacity(FSlateColor(Color));
    TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
    TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
    return TextBlock;
}
}

void UMHGameHudWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EnsureWidgetTreeBuilt();
    BindSubsystemDelegates();
    RefreshObjectiveAndStatus();
}

void UMHGameHudWidget::NativeDestruct()
{
    UnbindSubsystemDelegates();
    Super::NativeDestruct();
}

void UMHGameHudWidget::EnsureWidgetTreeBuilt()
{
    if (ObjectiveTextBlock && DialogueLineTextBlock)
    {
        return;
    }

    if (!WidgetTree)
    {
        return;
    }

    RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
    WidgetTree->RootWidget = RootOverlay;

    ObjectiveTextBlock = MakeHudTextBlock(WidgetTree, TEXT("ObjectiveText"), 20, FLinearColor(0.92f, 0.95f, 1.0f));
    if (UOverlaySlot* ObjectiveSlot = RootOverlay->AddChildToOverlay(ObjectiveTextBlock))
    {
        ObjectiveSlot->SetHorizontalAlignment(HAlign_Left);
        ObjectiveSlot->SetVerticalAlignment(VAlign_Top);
        ObjectiveSlot->SetPadding(FMargin(36.0f, 28.0f, 36.0f, 0.0f));
    }

    StatusTextBlock = MakeHudTextBlock(WidgetTree, TEXT("StatusText"), 16, FLinearColor(0.75f, 0.82f, 0.88f));
    if (UOverlaySlot* StatusSlot = RootOverlay->AddChildToOverlay(StatusTextBlock))
    {
        StatusSlot->SetHorizontalAlignment(HAlign_Right);
        StatusSlot->SetVerticalAlignment(VAlign_Top);
        StatusSlot->SetPadding(FMargin(0.0f, 28.0f, 36.0f, 0.0f));
    }

    WantedTextBlock = MakeHudTextBlock(WidgetTree, TEXT("WantedText"), 22, FLinearColor(1.0f, 0.85f, 0.2f));
    if (UOverlaySlot* WantedSlot = RootOverlay->AddChildToOverlay(WantedTextBlock))
    {
        WantedSlot->SetHorizontalAlignment(HAlign_Left);
        WantedSlot->SetVerticalAlignment(VAlign_Top);
        WantedSlot->SetPadding(FMargin(36.0f, 58.0f, 36.0f, 0.0f));
    }

    RadioTextBlock = MakeHudTextBlock(WidgetTree, TEXT("RadioText"), 15, FLinearColor(0.8f, 0.95f, 0.85f));
    if (UOverlaySlot* RadioSlot = RootOverlay->AddChildToOverlay(RadioTextBlock))
    {
        RadioSlot->SetHorizontalAlignment(HAlign_Right);
        RadioSlot->SetVerticalAlignment(VAlign_Top);
        RadioSlot->SetPadding(FMargin(0.0f, 52.0f, 36.0f, 0.0f));
    }
    RadioTextBlock->SetVisibility(ESlateVisibility::Collapsed);

    DialogueBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueBox"));
    SpeakerTextBlock = MakeHudTextBlock(WidgetTree, TEXT("SpeakerText"), 22, FLinearColor(0.55f, 0.85f, 1.0f));
    DialogueLineTextBlock = MakeHudTextBlock(WidgetTree, TEXT("DialogueLineText"), 20, FLinearColor::White);

    if (UVerticalBoxSlot* SpeakerSlot = DialogueBox->AddChildToVerticalBox(SpeakerTextBlock))
    {
        SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }
    DialogueBox->AddChildToVerticalBox(DialogueLineTextBlock);

    if (UOverlaySlot* DialogueSlot = RootOverlay->AddChildToOverlay(DialogueBox))
    {
        DialogueSlot->SetHorizontalAlignment(HAlign_Center);
        DialogueSlot->SetVerticalAlignment(VAlign_Bottom);
        DialogueSlot->SetPadding(FMargin(48.0f, 0.0f, 48.0f, 72.0f));
    }

    DialogueBox->SetVisibility(ESlateVisibility::Collapsed);

    ToastTextBlock = MakeHudTextBlock(WidgetTree, TEXT("ToastText"), 26, FLinearColor(1.0f, 0.92f, 0.6f));
    if (UOverlaySlot* ToastSlot = RootOverlay->AddChildToOverlay(ToastTextBlock))
    {
        ToastSlot->SetHorizontalAlignment(HAlign_Center);
        ToastSlot->SetVerticalAlignment(VAlign_Top);
        ToastSlot->SetPadding(FMargin(0.0f, 120.0f, 0.0f, 0.0f));
    }
    ToastTextBlock->SetJustification(ETextJustify::Center);
    ToastTextBlock->SetVisibility(ESlateVisibility::Collapsed);

    USizeBox* MinimapBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MinimapBox"));
    MinimapBox->SetWidthOverride(220.0f);
    MinimapBox->SetHeightOverride(220.0f);

    MinimapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MinimapImage"));
    MinimapBox->SetContent(MinimapImage);

    if (UOverlaySlot* MinimapSlot = RootOverlay->AddChildToOverlay(MinimapBox))
    {
        MinimapSlot->SetHorizontalAlignment(HAlign_Right);
        MinimapSlot->SetVerticalAlignment(VAlign_Bottom);
        MinimapSlot->SetPadding(FMargin(0.0f, 0.0f, 36.0f, 36.0f));
    }
}

void UMHGameHudWidget::BindSubsystemDelegates()
{
    if (bDelegatesBound)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return;
    }

    if (UMHDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UMHDialogueSubsystem>())
    {
        DialogueSubsystem->OnDialogueLineChanged.AddDynamic(this, &UMHGameHudWidget::HandleDialogueLineChanged);
        DialogueSubsystem->OnDialogueEnded.AddDynamic(this, &UMHGameHudWidget::HandleDialogueEnded);
    }

    if (UMHRadioSubsystem* RadioSubsystem = GameInstance->GetSubsystem<UMHRadioSubsystem>())
    {
        RadioSubsystem->OnStationChanged.AddDynamic(this, &UMHGameHudWidget::HandleStationChanged);
        RadioSubsystem->OnSongChanged.AddDynamic(this, &UMHGameHudWidget::HandleSongChanged);
    }

    if (UMHMissionSubsystem* MissionSubsystem = GameInstance->GetSubsystem<UMHMissionSubsystem>())
    {
        MissionSubsystem->OnMissionCompleted.AddDynamic(this, &UMHGameHudWidget::HandleMissionCompleted);
        MissionSubsystem->OnMissionFailed.AddDynamic(this, &UMHGameHudWidget::HandleMissionFailed);
    }

    if (UMHCollectibleSubsystem* CollectibleSubsystem = GameInstance->GetSubsystem<UMHCollectibleSubsystem>())
    {
        CollectibleSubsystem->OnCollectibleFound.AddDynamic(this, &UMHGameHudWidget::HandleCollectibleFound);
    }

    bDelegatesBound = true;
}

void UMHGameHudWidget::UnbindSubsystemDelegates()
{
    if (!bDelegatesBound)
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UMHDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UMHDialogueSubsystem>())
        {
            DialogueSubsystem->OnDialogueLineChanged.RemoveDynamic(this, &UMHGameHudWidget::HandleDialogueLineChanged);
            DialogueSubsystem->OnDialogueEnded.RemoveDynamic(this, &UMHGameHudWidget::HandleDialogueEnded);
        }

        if (UMHRadioSubsystem* RadioSubsystem = GameInstance->GetSubsystem<UMHRadioSubsystem>())
        {
            RadioSubsystem->OnStationChanged.RemoveDynamic(this, &UMHGameHudWidget::HandleStationChanged);
            RadioSubsystem->OnSongChanged.RemoveDynamic(this, &UMHGameHudWidget::HandleSongChanged);
        }

        if (UMHMissionSubsystem* MissionSubsystem = GameInstance->GetSubsystem<UMHMissionSubsystem>())
        {
            MissionSubsystem->OnMissionCompleted.RemoveDynamic(this, &UMHGameHudWidget::HandleMissionCompleted);
            MissionSubsystem->OnMissionFailed.RemoveDynamic(this, &UMHGameHudWidget::HandleMissionFailed);
        }

        if (UMHCollectibleSubsystem* CollectibleSubsystem = GameInstance->GetSubsystem<UMHCollectibleSubsystem>())
        {
            CollectibleSubsystem->OnCollectibleFound.RemoveDynamic(this, &UMHGameHudWidget::HandleCollectibleFound);
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ToastTimerHandle);
    }

    bDelegatesBound = false;
}

void UMHGameHudWidget::RefreshObjectiveAndStatus()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return;
    }

    FText Objective = FText::FromString(TEXT("Explore the waterfront."));
    if (UMHMissionSubsystem* MissionSubsystem = GameInstance->GetSubsystem<UMHMissionSubsystem>())
    {
        FMHMissionStep Step;
        if (MissionSubsystem->GetCurrentStep(Step))
        {
            Objective = FText::FromString(Step.Text);
        }
        else if (MissionSubsystem->bCampaignComplete)
        {
            Objective = FText::FromString(TEXT("Campaign complete."));
        }
    }

    // Append the live countdown for a Timed / Survive objective.
    if (const AMHGameModeBase* GameMode = GetWorld() ? Cast<AMHGameModeBase>(GetWorld()->GetAuthGameMode()) : nullptr)
    {
        if (GameMode->IsCurrentStepTimed())
        {
            Objective = FText::FromString(FString::Printf(
                TEXT("%s  [%ds]"),
                *Objective.ToString(),
                FMath::CeilToInt(GameMode->GetCurrentStepTimeRemaining())));
        }
    }

    SetObjectiveText(Objective);

    FString StatusString = TEXT("Cash: $0  |  Weather: Clear");
    if (UMHGameStateSubsystem* GameState = GameInstance->GetSubsystem<UMHGameStateSubsystem>())
    {
        FString WeatherLabel;
        switch (GameState->Weather)
        {
        case EMHWeatherState::DenseFog:
            WeatherLabel = TEXT("Dense fog");
            break;
        case EMHWeatherState::CoastalRain:
            WeatherLabel = TEXT("Coastal rain");
            break;
        case EMHWeatherState::Noreaster:
            WeatherLabel = TEXT("Nor'easter");
            break;
        default:
            WeatherLabel = TEXT("Clear");
            break;
        }

        StatusString = FString::Printf(
            TEXT("Cash: $%d  |  HP: %d  |  Weather: %s"),
            GameState->Cash,
            FMath::RoundToInt(GameState->Health),
            *WeatherLabel);
        if (GameState->Armor > 0.0f)
        {
            StatusString += FString::Printf(TEXT("  |  Armor: %d"), FMath::RoundToInt(GameState->Armor));
        }
    }

    if (const AMHPlayerCharacter* PlayerCharacter = Cast<AMHPlayerCharacter>(GetOwningPlayerPawn()))
    {
        StatusString += FString::Printf(TEXT("  |  Stamina: %d%%"), FMath::RoundToInt(PlayerCharacter->GetStaminaPercent() * 100.0f));
        if (PlayerCharacter->HasPistol())
        {
            StatusString += FString::Printf(TEXT("  |  Ammo: %d"), PlayerCharacter->GetPistolAmmo());
        }
    }

    SetStatusText(FText::FromString(StatusString));

    FString WantedString;
    if (UWorld* World = GetWorld())
    {
        if (UMHWantedSubsystem* WantedSubsystem = World->GetSubsystem<UMHWantedSubsystem>())
        {
            const int32 Level = WantedSubsystem->GetWantedLevel();
            for (int32 Index = 0; Index < 5; ++Index)
            {
                WantedString += Index < Level ? TEXT("*") : TEXT("-");
            }
        }
    }
    SetWantedText(FText::FromString(WantedString));

    RefreshMinimap();
}

void UMHGameHudWidget::RefreshMinimap()
{
    if (!MinimapImage)
    {
        return;
    }

    if (!CachedMinimapCapture)
    {
        if (UWorld* World = GetWorld())
        {
            TArray<AActor*> FoundActors;
            UGameplayStatics::GetAllActorsOfClass(World, AMHMinimapCaptureActor::StaticClass(), FoundActors);
            if (FoundActors.Num() > 0)
            {
                CachedMinimapCapture = Cast<AMHMinimapCaptureActor>(FoundActors[0]);
            }
        }
    }

    if (!CachedMinimapCapture || !CachedMinimapCapture->GetRenderTarget())
    {
        return;
    }

    FSlateBrush Brush;
    Brush.SetResourceObject(CachedMinimapCapture->GetRenderTarget());
    Brush.ImageSize = FVector2D(220.0f, 220.0f);
    MinimapImage->SetBrush(Brush);
}

void UMHGameHudWidget::RefreshHud()
{
    RefreshObjectiveAndStatus();
}

void UMHGameHudWidget::SetDialogueLine(const FText& Speaker, const FText& Line)
{
    if (DialogueBox)
    {
        DialogueBox->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    SetTextBlockContent(SpeakerTextBlock, Speaker, false);
    SetTextBlockContent(DialogueLineTextBlock, Line, false);
}

void UMHGameHudWidget::ClearDialogue()
{
    SetTextBlockContent(SpeakerTextBlock, FText::GetEmpty(), true);
    SetTextBlockContent(DialogueLineTextBlock, FText::GetEmpty(), true);
    if (DialogueBox)
    {
        DialogueBox->SetVisibility(ESlateVisibility::Collapsed);
    }

    RefreshObjectiveAndStatus();
}

void UMHGameHudWidget::SetObjectiveText(const FText& Objective)
{
    SetTextBlockContent(ObjectiveTextBlock, Objective, false);
}

void UMHGameHudWidget::SetStatusText(const FText& Status)
{
    SetTextBlockContent(StatusTextBlock, Status, false);
}

void UMHGameHudWidget::SetWantedText(const FText& Wanted)
{
    SetTextBlockContent(WantedTextBlock, Wanted, false);
}

void UMHGameHudWidget::SetRadioText(const FText& Radio)
{
    SetTextBlockContent(RadioTextBlock, Radio, true);
}

void UMHGameHudWidget::HandleDialogueLineChanged(FName ConversationId, FName Speaker, FText LineText)
{
    FString Shown = LineText.ToString();
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UMHDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UMHDialogueSubsystem>())
        {
            const int32 ChoiceCount = DialogueSubsystem->GetCurrentChoiceCount();
            for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceCount; ++ChoiceIndex)
            {
                FString ChoiceText;
                if (DialogueSubsystem->GetCurrentChoiceText(ChoiceIndex, ChoiceText))
                {
                    Shown += FString::Printf(TEXT("\n%d) %s"), ChoiceIndex + 1, *ChoiceText);
                }
            }
        }
    }

    SetDialogueLine(FText::FromName(Speaker), FText::FromString(Shown));
}

void UMHGameHudWidget::HandleDialogueEnded(FName ConversationId)
{
    ClearDialogue();
}

void UMHGameHudWidget::HandleStationChanged(FName StationId)
{
    if (StationId.IsNone())
    {
        SetRadioText(FText::GetEmpty());
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UMHRadioSubsystem* RadioSubsystem = GameInstance ? GameInstance->GetSubsystem<UMHRadioSubsystem>() : nullptr;

    FMHRadioStation Station;
    if (RadioSubsystem && RadioSubsystem->GetCurrentStation(Station))
    {
        SetRadioText(FText::FromString(Station.DisplayName));
    }
}

void UMHGameHudWidget::HandleSongChanged(FName StationId, FString SongTitle)
{
    UGameInstance* GameInstance = GetGameInstance();
    UMHRadioSubsystem* RadioSubsystem = GameInstance ? GameInstance->GetSubsystem<UMHRadioSubsystem>() : nullptr;

    FMHRadioStation Station;
    if (RadioSubsystem && RadioSubsystem->GetCurrentStation(Station))
    {
        SetRadioText(FText::FromString(FString::Printf(TEXT("%s - %s"), *Station.DisplayName, *SongTitle)));
    }
}

void UMHGameHudWidget::HandleMissionCompleted(FString Title, FString CompletionMessage)
{
    const FString ToastLine = CompletionMessage.IsEmpty()
        ? FString::Printf(TEXT("%s complete."), *Title)
        : CompletionMessage;

    SetTextBlockContent(ToastTextBlock, FText::FromString(ToastLine), true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(ToastTimerHandle, this, &UMHGameHudWidget::ClearToast, 5.0f, false);
    }
}

void UMHGameHudWidget::HandleMissionFailed(FString Reason)
{
    const FString ToastLine = Reason.IsEmpty()
        ? FString(TEXT("Mission failed."))
        : FString::Printf(TEXT("Mission failed: %s"), *Reason);

    SetTextBlockContent(ToastTextBlock, FText::FromString(ToastLine), true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(ToastTimerHandle, this, &UMHGameHudWidget::ClearToast, 5.0f, false);
    }
}

void UMHGameHudWidget::HandleCollectibleFound(FName ItemId, int32 CollectedCount, int32 TotalCount)
{
    FString DisplayName = ItemId.ToString();
    UGameInstance* GameInstance = GetGameInstance();
    if (UMHCollectibleSubsystem* CollectibleSubsystem = GameInstance ? GameInstance->GetSubsystem<UMHCollectibleSubsystem>() : nullptr)
    {
        FMHCollectibleRecord Record;
        if (CollectibleSubsystem->FindCollectible(ItemId, Record) && !Record.DisplayName.IsEmpty())
        {
            DisplayName = Record.DisplayName;
        }
    }

    const FString ToastLine = FString::Printf(TEXT("%s found (%d/%d)"), *DisplayName, CollectedCount, TotalCount);
    SetTextBlockContent(ToastTextBlock, FText::FromString(ToastLine), true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(ToastTimerHandle, this, &UMHGameHudWidget::ClearToast, 5.0f, false);
    }
}

void UMHGameHudWidget::ClearToast()
{
    SetTextBlockContent(ToastTextBlock, FText::GetEmpty(), true);
}

void UMHGameHudWidget::SetTextBlockContent(UTextBlock* TextBlock, const FText& Content, bool bCollapseWhenEmpty)
{
    if (!TextBlock)
    {
        return;
    }

    TextBlock->SetText(Content);
    TextBlock->SetVisibility(
        Content.IsEmpty() && bCollapseWhenEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
