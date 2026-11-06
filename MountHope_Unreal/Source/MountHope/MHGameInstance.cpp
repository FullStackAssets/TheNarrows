#include "MHGameInstance.h"

#include "MHCollectibleSubsystem.h"
#include "MHDialogueSubsystem.h"
#include "MHEconomySubsystem.h"
#include "MHGameStateSubsystem.h"
#include "MHMissionSubsystem.h"
#include "MHRadioSubsystem.h"
#include "MHWorldSliceSubsystem.h"

void UMHGameInstance::Init()
{
    Super::Init();

    if (UMHWorldSliceSubsystem* SliceSubsystem = GetSubsystem<UMHWorldSliceSubsystem>())
    {
        SliceSubsystem->LoadSliceFromJson(SlicePath);
    }

    if (UMHMissionSubsystem* MissionSubsystem = GetSubsystem<UMHMissionSubsystem>())
    {
        MissionSubsystem->LoadMissionsFromJson(MissionPath);
    }

    if (UMHDialogueSubsystem* DialogueSubsystem = GetSubsystem<UMHDialogueSubsystem>())
    {
        DialogueSubsystem->LoadDialogueFromJson(DialoguePath);
    }

    if (UMHCollectibleSubsystem* CollectibleSubsystem = GetSubsystem<UMHCollectibleSubsystem>())
    {
        CollectibleSubsystem->LoadCollectiblesFromJson(CollectiblesPath);
    }

    if (UMHRadioSubsystem* RadioSubsystem = GetSubsystem<UMHRadioSubsystem>())
    {
        RadioSubsystem->LoadStationsFromJson(RadioPath);
    }

    if (UMHEconomySubsystem* EconomySubsystem = GetSubsystem<UMHEconomySubsystem>())
    {
        EconomySubsystem->LoadOffersFromJson(EconomyPath);
    }

    if (UMHGameStateSubsystem* GameStateSubsystem = GetSubsystem<UMHGameStateSubsystem>())
    {
        GameStateSubsystem->LoadBusinessesFromJson(EconomyPath);
        GameStateSubsystem->LoadFromSlot(SaveSlotName);
    }
}
