#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MHMissionSubsystem.h"
#include "MHGameModeBase.generated.h"

class AMHMissionTriggerActor;
class USoundBase;

UCLASS()
class MOUNTHOPE_API AMHGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMHGameModeBase();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool CompleteCurrentObjective(bool bPlayerInVehicle);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool CompleteCurrentObjectiveFrom(EMHObjectiveCompletionChannel Channel, FName ContextId, bool bPlayerInVehicle);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool TryCompleteVehicleObjective(bool bPlayerInVehicle);

    // Spend the current Pay step's catalog offer, then complete it. DiscountPercent clamps inside the economy quote.
    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool TryPayCurrentObjective(int32 DiscountPercent, bool bPlayerInVehicle);

    // Complete a Collect step once CanAcceptCollection passes.
    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool NotifyCollectibleProgress(FName ItemId, FName SetId, int32 FoundInSet);

    // Fail the in-progress mission (if any): restart it from its first step and
    // broadcast UMHMissionSubsystem::OnMissionFailed. No-op if no mission active.
    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    void FailCurrentMission(const FString& Reason);

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool IsCurrentStepTimed() const { return bStepTimerActive; }

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    float GetCurrentStepTimeRemaining() const { return StepTimeRemaining; }

protected:
    UPROPERTY(Transient)
    TObjectPtr<AMHMissionTriggerActor> ObjectiveTrigger = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Mount Hope|Police")
    float MaxWantedBustedSeconds = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Audio")
    TObjectPtr<USoundBase> MissionCompleteSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Audio")
    TObjectPtr<USoundBase> ObjectiveUpdateSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Audio")
    TObjectPtr<USoundBase> BustedOrWastedSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Audio")
    TObjectPtr<USoundBase> MissionFailedSound;

private:
    bool IsWorldTargetObjective(const FMHMissionStep& Step) const;
    void RefreshObjectiveTrigger();
    void RespawnAtSafehouseIfAvailable();
    void ApplyWeatherFromString(const FString& WeatherName) const;
    void TickBustedTimer(float DeltaSeconds);
    void TickStepTimer(float DeltaSeconds);

    // Post-gate objective completion (rewards, crime, reputation, advance,
    // mission-complete, save, refresh). Shared by CompleteCurrentObjective (after
    // its vehicle/heat gates) and the Survive-timer auto-complete path.
    bool ApplyObjectiveCompletion();

    UFUNCTION()
    void HandlePlayerWasted();

    UFUNCTION()
    void HandlePlayerBusted();

    UFUNCTION()
    void HandleHourChanged(int32 Hour);

    float TimeAtMaxWanted = 0.0f;

    // Active-step countdown for Timed / Survive objectives. Armed by
    // RefreshObjectiveTrigger whenever the current step changes.
    bool bStepTimerActive = false;
    bool bStepTimerIsSurvive = false;
    float StepTimeRemaining = 0.0f;
};
