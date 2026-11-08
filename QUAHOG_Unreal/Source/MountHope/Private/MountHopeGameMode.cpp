#include "MountHopeGameMode.h"

#include "MHGameStateSubsystem.h"
#include "MHMissionSubsystem.h"
#include "MHMissionTriggerActor.h"
#include "MountHopeCharacter.h"
#include "MountHopeGameInstance.h"

AMountHopeGameMode::AMountHopeGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AMountHopeCharacter::StaticClass();
}

void AMountHopeGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!GetGameInstance())
    {
        return;
    }

    UMHMissionSubsystem* MissionSubsystem = GetGameInstance()->GetSubsystem<UMHMissionSubsystem>();
    FMHMissionStep Step;
    if (MissionSubsystem && MissionSubsystem->GetCurrentStep(Step))
    {
        UE_LOG(LogTemp, Log, TEXT("MountHope: Objective -> %s"), *Step.Text);
    }

    RefreshObjectiveTrigger();
}

void AMountHopeGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!GetGameInstance())
    {
        return;
    }

    if (UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>())
    {
        GameStateSubsystem->DecayHeat(DeltaSeconds);
    }
}

bool AMountHopeGameMode::CompleteCurrentObjective(bool bPlayerInVehicle)
{
    if (!GetGameInstance())
    {
        return false;
    }

    UMHMissionSubsystem* MissionSubsystem = GetGameInstance()->GetSubsystem<UMHMissionSubsystem>();
    UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>();
    if (!MissionSubsystem || !GameStateSubsystem)
    {
        return false;
    }

    FMHMissionStep Step;
    if (!MissionSubsystem->GetCurrentStep(Step))
    {
        return false;
    }

    if (Step.bNeedVehicle && !bPlayerInVehicle)
    {
        UE_LOG(LogTemp, Warning, TEXT("MountHope: objective requires vehicle."));
        return false;
    }

    if (Step.Reward > 0)
    {
        GameStateSubsystem->AddCash(Step.Reward);
    }

    const bool bAdvanced = MissionSubsystem->AdvanceStep();
    FMHMissionStep NextStep;
    if (bAdvanced && MissionSubsystem->GetCurrentStep(NextStep))
    {
        UE_LOG(LogTemp, Log, TEXT("MountHope: Next objective -> %s"), *NextStep.Text);
    }

    GameStateSubsystem->SaveToSlot(ResolveSaveSlotName());
    RefreshObjectiveTrigger();
    return bAdvanced;
}

FString AMountHopeGameMode::ResolveSaveSlotName() const
{
    if (const UMountHopeGameInstance* GameInstance = Cast<UMountHopeGameInstance>(GetGameInstance()))
    {
        if (!GameInstance->SaveSlotName.IsEmpty())
        {
            return GameInstance->SaveSlotName;
        }
    }

    return TEXT("QuahogUnrealSlot");
}

bool AMountHopeGameMode::TryCompleteVehicleObjective(bool bPlayerInVehicle)
{
    if (!GetGameInstance())
    {
        return false;
    }

    UMHMissionSubsystem* MissionSubsystem = GetGameInstance()->GetSubsystem<UMHMissionSubsystem>();
    if (!MissionSubsystem)
    {
        return false;
    }

    FMHMissionStep Step;
    if (!MissionSubsystem->GetCurrentStep(Step))
    {
        return false;
    }

    if (Step.bNeedVehicle == bPlayerInVehicle && !IsWorldTargetObjective(Step))
    {
        return CompleteCurrentObjective(bPlayerInVehicle);
    }

    return false;
}

bool AMountHopeGameMode::IsWorldTargetObjective(const FMHMissionStep& Step) const
{
    return Step.Radius > UE_KINDA_SMALL_NUMBER;
}

void AMountHopeGameMode::RefreshObjectiveTrigger()
{
    if (!GetWorld() || !GetGameInstance())
    {
        return;
    }

    UMHMissionSubsystem* MissionSubsystem = GetGameInstance()->GetSubsystem<UMHMissionSubsystem>();
    if (!MissionSubsystem)
    {
        return;
    }

    FMHMissionStep Step;
    const bool bHasStep = MissionSubsystem->GetCurrentStep(Step);
    const bool bNeedsTrigger = bHasStep && IsWorldTargetObjective(Step);

    if (!bNeedsTrigger)
    {
        if (ObjectiveTrigger)
        {
            ObjectiveTrigger->SetActorEnableCollision(false);
            ObjectiveTrigger->SetActorHiddenInGame(true);
        }
        return;
    }

    if (!ObjectiveTrigger)
    {
        ObjectiveTrigger = GetWorld()->SpawnActor<AMHMissionTriggerActor>(
            AMHMissionTriggerActor::StaticClass(),
            Step.Target,
            FRotator::ZeroRotator);
    }

    if (!ObjectiveTrigger)
    {
        return;
    }

    ObjectiveTrigger->SetTriggerRadius(Step.Radius);
    ObjectiveTrigger->SetActorLocation(Step.Target);
    ObjectiveTrigger->SetActorEnableCollision(true);
    ObjectiveTrigger->SetActorHiddenInGame(false);
    ObjectiveTrigger->ResetConsumed();
}
