#include "MHGameModeBase.h"

#include "Kismet/GameplayStatics.h"
#include "MHEconomySubsystem.h"
#include "MHGameStateSubsystem.h"
#include "MHMinimapCaptureActor.h"
#include "MHMissionSubsystem.h"
#include "MHMissionTriggerActor.h"
#include "MHOpenWorldSubsystem.h"
#include "MHPedestrianSpawnerActor.h"
#include "MHPlayerCharacter.h"
#include "MHPlayerController.h"
#include "MHPoliceSpawnerActor.h"
#include "MHReputationSubsystem.h"
#include "MHTimeOfDaySubsystem.h"
#include "MHWantedSubsystem.h"
#include "MHWeatherDirectorActor.h"
#include "GameFramework/PlayerStart.h"
#include "Sound/SoundBase.h"

AMHGameModeBase::AMHGameModeBase()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AMHPlayerCharacter::StaticClass();
    PlayerControllerClass = AMHPlayerController::StaticClass();
}

void AMHGameModeBase::BeginPlay()
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

    if (const UMHOpenWorldSubsystem* OpenWorldSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMHOpenWorldSubsystem>() : nullptr)
    {
        const FMHMapSourceProfile Profile = OpenWorldSubsystem->GetDefaultMapSource();
        UE_LOG(LogTemp, Log, TEXT("MountHope: World source profile '%s' (%s)"), *Profile.Name.ToString(), *Profile.SourcePath);
    }

    if (GetWorld())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = TEXT("MH_WeatherDirector");
        GetWorld()->SpawnActor<AMHWeatherDirectorActor>(
            AMHWeatherDirectorActor::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams);

        FActorSpawnParameters MinimapSpawnParams;
        MinimapSpawnParams.Name = TEXT("MH_MinimapCapture");
        GetWorld()->SpawnActor<AMHMinimapCaptureActor>(
            AMHMinimapCaptureActor::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            MinimapSpawnParams);

        FActorSpawnParameters PedestrianSpawnerParams;
        PedestrianSpawnerParams.Name = TEXT("MH_PedestrianSpawner");
        GetWorld()->SpawnActor<AMHPedestrianSpawnerActor>(
            AMHPedestrianSpawnerActor::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            PedestrianSpawnerParams);

        FActorSpawnParameters PoliceSpawnerParams;
        PoliceSpawnerParams.Name = TEXT("MH_PoliceSpawner");
        GetWorld()->SpawnActor<AMHPoliceSpawnerActor>(
            AMHPoliceSpawnerActor::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            PoliceSpawnerParams);
    }

    RespawnAtSafehouseIfAvailable();

    if (UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>())
    {
        GameStateSubsystem->OnPlayerWasted.AddDynamic(this, &AMHGameModeBase::HandlePlayerWasted);
        GameStateSubsystem->OnPlayerBusted.AddDynamic(this, &AMHGameModeBase::HandlePlayerBusted);
    }

    if (UMHTimeOfDaySubsystem* TimeOfDaySubsystem = GetGameInstance()->GetSubsystem<UMHTimeOfDaySubsystem>())
    {
        TimeOfDaySubsystem->OnHourChanged.AddDynamic(this, &AMHGameModeBase::HandleHourChanged);
    }
}

void AMHGameModeBase::Tick(float DeltaSeconds)
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

    if (UMHTimeOfDaySubsystem* TimeOfDaySubsystem = GetGameInstance()->GetSubsystem<UMHTimeOfDaySubsystem>())
    {
        TimeOfDaySubsystem->AdvanceTime(DeltaSeconds);
    }

    if (UWorld* World = GetWorld())
    {
        if (UMHWantedSubsystem* WantedSubsystem = World->GetSubsystem<UMHWantedSubsystem>())
        {
            WantedSubsystem->TickWantedDecay(DeltaSeconds);
        }
    }

    TickBustedTimer(DeltaSeconds);
    TickStepTimer(DeltaSeconds);
}

void AMHGameModeBase::TickStepTimer(float DeltaSeconds)
{
    if (!bStepTimerActive)
    {
        return;
    }

    StepTimeRemaining -= DeltaSeconds;
    if (StepTimeRemaining > 0.0f)
    {
        return;
    }

    bStepTimerActive = false;
    StepTimeRemaining = 0.0f;

    if (bStepTimerIsSurvive)
    {
        // Survived the countdown: auto-complete the step (bypasses the vehicle/heat
        // gates, which don't apply to a hold-out objective).
        UMHMissionSubsystem* MissionSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHMissionSubsystem>() : nullptr;
        if (MissionSubsystem && MissionSubsystem->CanAcceptCompletion(EMHObjectiveCompletionChannel::Timer, NAME_None))
        {
            ApplyObjectiveCompletion();
        }
    }
    else
    {
        FailCurrentMission(TEXT("Out of time"));
    }
}

void AMHGameModeBase::FailCurrentMission(const FString& Reason)
{
    UMHMissionSubsystem* MissionSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHMissionSubsystem>() : nullptr;
    if (!MissionSubsystem || !MissionSubsystem->IsMissionInProgress())
    {
        return;
    }

    bStepTimerActive = false;
    MissionSubsystem->RestartCurrentMission();
    MissionSubsystem->OnMissionFailed.Broadcast(Reason);
    UE_LOG(LogTemp, Log, TEXT("MountHope: Mission failed (%s) - restarting from the top."), *Reason);
    UGameplayStatics::PlaySound2D(this, MissionFailedSound);

    RefreshObjectiveTrigger();
}

void AMHGameModeBase::TickBustedTimer(float DeltaSeconds)
{
    UWorld* World = GetWorld();
    UMHWantedSubsystem* WantedSubsystem = World ? World->GetSubsystem<UMHWantedSubsystem>() : nullptr;
    if (!WantedSubsystem)
    {
        return;
    }

    if (WantedSubsystem->GetWantedLevel() < 5)
    {
        TimeAtMaxWanted = 0.0f;
        return;
    }

    TimeAtMaxWanted += DeltaSeconds;
    if (TimeAtMaxWanted < MaxWantedBustedSeconds)
    {
        return;
    }

    TimeAtMaxWanted = 0.0f;
    if (UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>() : nullptr)
    {
        GameStateSubsystem->TriggerBusted();
    }
}

void AMHGameModeBase::HandlePlayerWasted()
{
    UE_LOG(LogTemp, Log, TEXT("MountHope: Player wasted - respawning at safehouse."));
    UGameplayStatics::PlaySound2D(this, BustedOrWastedSound);
    RespawnAtSafehouseIfAvailable();
    FailCurrentMission(TEXT("You were wasted"));
}

void AMHGameModeBase::HandlePlayerBusted()
{
    UE_LOG(LogTemp, Log, TEXT("MountHope: Player busted - respawning at safehouse."));
    UGameplayStatics::PlaySound2D(this, BustedOrWastedSound);
    RespawnAtSafehouseIfAvailable();
    FailCurrentMission(TEXT("You were busted"));
}

void AMHGameModeBase::HandleHourChanged(int32 Hour)
{
    // Pay out owned-business income once per in-game day (midnight rollover) rather than every
    // hour tick.
    if (Hour != 0)
    {
        return;
    }

    if (UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>() : nullptr)
    {
        const int32 PassiveIncome = GameStateSubsystem->GetPassiveDailyIncome();
        if (PassiveIncome > 0)
        {
            GameStateSubsystem->AddCash(PassiveIncome);
        }
    }
}

void AMHGameModeBase::RespawnAtSafehouseIfAvailable()
{
    if (!GetWorld())
    {
        return;
    }

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!PlayerPawn)
    {
        return;
    }

    UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>() : nullptr;
    if (GameStateSubsystem && GameStateSubsystem->bHasSafehouse)
    {
        PlayerPawn->SetActorLocation(GameStateSubsystem->SafehouseLocation);
        return;
    }

    // No safehouse claimed yet: fall back to the level's player start so a wasted/busted
    // consequence still moves the player instead of leaving them exactly where they died.
    if (AActor* Start = UGameplayStatics::GetActorOfClass(GetWorld(), APlayerStart::StaticClass()))
    {
        PlayerPawn->SetActorLocation(Start->GetActorLocation());
    }
}

bool AMHGameModeBase::CompleteCurrentObjective(bool bPlayerInVehicle)
{
    return CompleteCurrentObjectiveFrom(EMHObjectiveCompletionChannel::World, NAME_None, bPlayerInVehicle);
}

bool AMHGameModeBase::CompleteCurrentObjectiveFrom(EMHObjectiveCompletionChannel Channel, FName ContextId, bool bPlayerInVehicle)
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

    if (!MissionSubsystem->CanAcceptCompletion(Channel, ContextId))
    {
        return false;
    }

    if (Step.bNeedVehicle && !bPlayerInVehicle)
    {
        UE_LOG(LogTemp, Warning, TEXT("MountHope: objective requires vehicle."));
        return false;
    }

    if (Step.bRequireNoHeat)
    {
        if (UWorld* World = GetWorld())
        {
            if (UMHWantedSubsystem* WantedSubsystem = World->GetSubsystem<UMHWantedSubsystem>())
            {
                if (WantedSubsystem->GetWantedLevel() > 0)
                {
                    UE_LOG(LogTemp, Warning, TEXT("MountHope: objective requires losing the heat first."));
                    return false;
                }
            }
        }
    }

    return ApplyObjectiveCompletion();
}

bool AMHGameModeBase::TryPayCurrentObjective(int32 DiscountPercent, bool bPlayerInVehicle)
{
    if (!GetGameInstance())
    {
        return false;
    }

    UMHMissionSubsystem* MissionSubsystem = GetGameInstance()->GetSubsystem<UMHMissionSubsystem>();
    UMHEconomySubsystem* EconomySubsystem = GetGameInstance()->GetSubsystem<UMHEconomySubsystem>();
    if (!MissionSubsystem || !EconomySubsystem)
    {
        return false;
    }

    FMHMissionStep Step;
    if (!MissionSubsystem->GetCurrentStep(Step))
    {
        return false;
    }

    if (!MissionSubsystem->CanAcceptCompletion(EMHObjectiveCompletionChannel::Payment, Step.PayOfferId))
    {
        return false;
    }

    if (Step.bNeedVehicle && !bPlayerInVehicle)
    {
        return false;
    }

    if (Step.bRequireNoHeat)
    {
        if (UWorld* World = GetWorld())
        {
            if (UMHWantedSubsystem* WantedSubsystem = World->GetSubsystem<UMHWantedSubsystem>())
            {
                if (WantedSubsystem->GetWantedLevel() > 0)
                {
                    return false;
                }
            }
        }
    }

    if (!EconomySubsystem->TryPurchaseOffer(Step.PayOfferId, DiscountPercent))
    {
        UE_LOG(LogTemp, Warning, TEXT("MountHope: cannot pay offer '%s' for the current objective."), *Step.PayOfferId.ToString());
        return false;
    }

    return CompleteCurrentObjectiveFrom(EMHObjectiveCompletionChannel::Payment, Step.PayOfferId, bPlayerInVehicle);
}

bool AMHGameModeBase::NotifyCollectibleProgress(FName ItemId, FName SetId, int32 FoundInSet)
{
    if (!GetGameInstance())
    {
        return false;
    }

    UMHMissionSubsystem* MissionSubsystem = GetGameInstance()->GetSubsystem<UMHMissionSubsystem>();
    if (!MissionSubsystem || !MissionSubsystem->CanAcceptCollection(ItemId, SetId, FoundInSet))
    {
        return false;
    }

    const FName ContextId = !SetId.IsNone() ? SetId : ItemId;
    return CompleteCurrentObjectiveFrom(EMHObjectiveCompletionChannel::Collection, ContextId, false);
}

bool AMHGameModeBase::ApplyObjectiveCompletion()
{
    UMHMissionSubsystem* MissionSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHMissionSubsystem>() : nullptr;
    UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>() : nullptr;
    if (!MissionSubsystem || !GameStateSubsystem)
    {
        return false;
    }

    FMHMissionStep Step;
    if (!MissionSubsystem->GetCurrentStep(Step))
    {
        return false;
    }

    if (Step.Reward > 0)
    {
        GameStateSubsystem->AddCash(Step.Reward);
    }

    if (Step.bIsCrime)
    {
        if (UWorld* World = GetWorld())
        {
            if (UMHWantedSubsystem* WantedSubsystem = World->GetSubsystem<UMHWantedSubsystem>())
            {
                WantedSubsystem->ReportCrime(EMHCrimeType::MissionHeat, Step.CrimeSeverity);
            }
        }
    }

    if (Step.ReputationFactionTag.IsValid() && Step.ReputationDelta != 0)
    {
        if (UMHReputationSubsystem* ReputationSubsystem = GetGameInstance()->GetSubsystem<UMHReputationSubsystem>())
        {
            ReputationSubsystem->AddReputation(Step.ReputationFactionTag, Step.ReputationDelta);
        }
    }

    const int32 PreviousMissionIndex = MissionSubsystem->MissionIndex;
    const bool bAdvanced = MissionSubsystem->AdvanceStep();

    if (bAdvanced
        && MissionSubsystem->MissionIndex != PreviousMissionIndex
        && MissionSubsystem->Missions.IsValidIndex(PreviousMissionIndex))
    {
        const FMHMission& CompletedMission = MissionSubsystem->Missions[PreviousMissionIndex];
        if (CompletedMission.CompletionReward > 0)
        {
            GameStateSubsystem->AddCash(CompletedMission.CompletionReward);
        }

        MissionSubsystem->OnMissionCompleted.Broadcast(CompletedMission.Title, CompletedMission.CompletionMessage);
        UE_LOG(LogTemp, Log, TEXT("MountHope: Mission complete -> %s"), *CompletedMission.Title);
        UGameplayStatics::PlaySound2D(this, MissionCompleteSound);
    }

    FMHMissionStep NextStep;
    if (bAdvanced && MissionSubsystem->GetCurrentStep(NextStep))
    {
        UE_LOG(LogTemp, Log, TEXT("MountHope: Next objective -> %s"), *NextStep.Text);
        if (!NextStep.WeatherOnStart.IsEmpty())
        {
            ApplyWeatherFromString(NextStep.WeatherOnStart);
        }
        UGameplayStatics::PlaySound2D(this, ObjectiveUpdateSound);
    }

    GameStateSubsystem->SaveToSlot();
    RefreshObjectiveTrigger();
    return bAdvanced;
}

void AMHGameModeBase::ApplyWeatherFromString(const FString& WeatherName) const
{
    UMHGameStateSubsystem* GameStateSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>() : nullptr;
    if (!GameStateSubsystem)
    {
        return;
    }

    if (WeatherName.Equals(TEXT("DenseFog"), ESearchCase::IgnoreCase))
    {
        GameStateSubsystem->SetWeather(EMHWeatherState::DenseFog);
    }
    else if (WeatherName.Equals(TEXT("CoastalRain"), ESearchCase::IgnoreCase))
    {
        GameStateSubsystem->SetWeather(EMHWeatherState::CoastalRain);
    }
    else if (WeatherName.Equals(TEXT("Noreaster"), ESearchCase::IgnoreCase))
    {
        GameStateSubsystem->SetWeather(EMHWeatherState::Noreaster);
    }
    else
    {
        GameStateSubsystem->SetWeather(EMHWeatherState::Clear);
    }
}

bool AMHGameModeBase::TryCompleteVehicleObjective(bool bPlayerInVehicle)
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

bool AMHGameModeBase::IsWorldTargetObjective(const FMHMissionStep& Step) const
{
    return Step.Radius > UE_KINDA_SMALL_NUMBER;
}

void AMHGameModeBase::RefreshObjectiveTrigger()
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

    // (Re)arm the countdown for the current step. Done here — before the trigger
    // early-returns below — because a Survive step has no world trigger but still
    // needs its timer, and every step change routes through this function.
    bStepTimerActive = false;
    if (bHasStep
        && Step.TimeLimitSeconds > 0.0f
        && (Step.ObjectiveType == EMHObjectiveType::Timed || Step.ObjectiveType == EMHObjectiveType::Survive))
    {
        bStepTimerActive = true;
        bStepTimerIsSurvive = (Step.ObjectiveType == EMHObjectiveType::Survive);
        StepTimeRemaining = Step.TimeLimitSeconds;
    }

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
