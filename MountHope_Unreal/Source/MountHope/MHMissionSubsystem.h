#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MHMissionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMHOnMissionCompleted, FString, Title, FString, CompletionMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMHOnMissionFailed, FString, Reason);

UENUM(BlueprintType)
enum class EMHObjectiveType : uint8
{
    // Complete by satisfying the objective (reach the target / enter a vehicle /
    // finish the dialogue). Default; unchanged legacy behavior.
    Reach UMETA(DisplayName = "Reach"),
    // Same as Reach, but the step must be completed before TimeLimitSeconds
    // elapses or the mission fails and restarts from its first step.
    Timed UMETA(DisplayName = "Timed"),
    // Stay alive for TimeLimitSeconds; the step auto-completes when the timer
    // runs out (a "hold out" beat). No world target needed.
    Survive UMETA(DisplayName = "Survive"),
    // Finish a specific conversation (ContextId is the conversation id).
    Talk UMETA(DisplayName = "Talk"),
    // Buy a catalog offer (ContextId is the offer id). Cash is spent by the game mode.
    Pay UMETA(DisplayName = "Pay"),
    // Collect items from a set, or one specific item (ContextId is the set or item id).
    Collect UMETA(DisplayName = "Collect")
};

// How a caller is trying to complete the current step. Reach/Timed still accept
// world triggers and dialogue; the newer types only accept their own channel.
UENUM(BlueprintType)
enum class EMHObjectiveCompletionChannel : uint8
{
    World UMETA(DisplayName = "World"),
    Dialogue UMETA(DisplayName = "Dialogue"),
    Payment UMETA(DisplayName = "Payment"),
    Collection UMETA(DisplayName = "Collection"),
    Timer UMETA(DisplayName = "Timer")
};

USTRUCT(BlueprintType)
struct FMHMissionStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FString Text;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    EMHObjectiveType ObjectiveType = EMHObjectiveType::Reach;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    float TimeLimitSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FVector Target = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    float Radius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    bool bNeedVehicle = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 Reward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    bool bIsCrime = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 CrimeSeverity = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FGameplayTag ReputationFactionTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 ReputationDelta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    bool bRequireNoHeat = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FString WeatherOnStart;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FName ConversationId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FName PayOfferId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 PayAmount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FName CollectSetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FName CollectItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 CollectCount = 0;
};

USTRUCT(BlueprintType)
struct FMHMission
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FName Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FString Act;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    TArray<FMHMissionStep> Steps;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 CompletionReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    FString CompletionMessage;
};

UCLASS(BlueprintType)
class MOUNTHOPE_API UMHMissionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Mount Hope|Mission")
    FMHOnMissionCompleted OnMissionCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Mount Hope|Mission")
    FMHOnMissionFailed OnMissionFailed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    TArray<FMHMission> Missions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 MissionIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    int32 StepIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Mission")
    bool bCampaignComplete = false;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool LoadMissionsFromJson(const FString& RelativeOrAbsolutePath);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    void ResetCampaign();

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    bool AdvanceStep();

    // Reset progress to the first step of the current mission (a checkpoint at
    // mission start), used when a mission is failed.
    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Mission")
    void RestartCurrentMission();

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool IsMissionInProgress() const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool GetCurrentMission(FMHMission& OutMission) const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool GetCurrentStep(FMHMissionStep& OutStep) const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool FindMissionIndexById(FName MissionId, int32& OutIndex) const;

    // True when Channel/ContextId is a legal way to complete the current step.
    // Does not check vehicle, heat, cash, or collected counts — callers own those.
    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool CanAcceptCompletion(EMHObjectiveCompletionChannel Channel, FName ContextId) const;

    // Collection channel plus the count gate (FoundInSet is ignored for item objectives).
    UFUNCTION(BlueprintPure, Category = "Mount Hope|Mission")
    bool CanAcceptCollection(FName ItemId, FName SetId, int32 FoundInSet) const;
};
