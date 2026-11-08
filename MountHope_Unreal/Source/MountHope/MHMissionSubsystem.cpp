#include "MHMissionSubsystem.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ResolvePath(const FString& RelativeOrAbsolutePath)
{
    if (FPaths::FileExists(RelativeOrAbsolutePath))
    {
        return RelativeOrAbsolutePath;
    }

    return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeOrAbsolutePath);
}

EMHObjectiveType ParseObjectiveType(const FString& ObjectiveTypeString)
{
    if (ObjectiveTypeString.Equals(TEXT("timed"), ESearchCase::IgnoreCase))
    {
        return EMHObjectiveType::Timed;
    }
    if (ObjectiveTypeString.Equals(TEXT("survive"), ESearchCase::IgnoreCase))
    {
        return EMHObjectiveType::Survive;
    }
    if (ObjectiveTypeString.Equals(TEXT("talk"), ESearchCase::IgnoreCase))
    {
        return EMHObjectiveType::Talk;
    }
    if (ObjectiveTypeString.Equals(TEXT("pay"), ESearchCase::IgnoreCase))
    {
        return EMHObjectiveType::Pay;
    }
    if (ObjectiveTypeString.Equals(TEXT("collect"), ESearchCase::IgnoreCase))
    {
        return EMHObjectiveType::Collect;
    }
    if (!ObjectiveTypeString.IsEmpty() && !ObjectiveTypeString.Equals(TEXT("reach"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("MountHope: Unknown objectiveType '%s' - treating as Reach"), *ObjectiveTypeString);
    }
    return EMHObjectiveType::Reach;
}

FName ReadNameField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    FString Value;
    if (Object.IsValid() && Object->TryGetStringField(Field, Value) && !Value.IsEmpty())
    {
        return FName(*Value);
    }
    return NAME_None;
}
}

bool UMHMissionSubsystem::LoadMissionsFromJson(const FString& RelativeOrAbsolutePath)
{
    const FString FullPath = ResolvePath(RelativeOrAbsolutePath);
    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Unable to load mission JSON: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Invalid mission JSON: %s"), *FullPath);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* MissionsArray = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("missions"), MissionsArray) || !MissionsArray)
    {
        return false;
    }

    Missions.Reset();
    for (const TSharedPtr<FJsonValue>& MissionValue : *MissionsArray)
    {
        const TSharedPtr<FJsonObject> MissionObject = MissionValue->AsObject();
        if (!MissionObject.IsValid())
        {
            continue;
        }

        FString Title;
        if (!MissionObject->TryGetStringField(TEXT("title"), Title) || Title.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping mission entry with missing/empty title"));
            continue;
        }

        FMHMission Mission;
        Mission.Title = Title;
        Mission.Id = ReadNameField(MissionObject, TEXT("id"));
        MissionObject->TryGetStringField(TEXT("act"), Mission.Act);
        MissionObject->TryGetStringField(TEXT("completionMessage"), Mission.CompletionMessage);

        int32 CompletionRewardValue = Mission.CompletionReward;
        if (MissionObject->TryGetNumberField(TEXT("completionReward"), CompletionRewardValue))
        {
            Mission.CompletionReward = CompletionRewardValue;
        }

        const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
        if (MissionObject->TryGetArrayField(TEXT("steps"), StepsArray) && StepsArray)
        {
            for (const TSharedPtr<FJsonValue>& StepValue : *StepsArray)
            {
                const TSharedPtr<FJsonObject> StepObject = StepValue->AsObject();
                if (!StepObject.IsValid())
                {
                    continue;
                }

                FString StepText;
                if (!StepObject->TryGetStringField(TEXT("text"), StepText) || StepText.IsEmpty())
                {
                    UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping mission step with missing/empty text"));
                    continue;
                }

                FMHMissionStep Step;
                Step.Text = StepText;
                Step.Radius = static_cast<float>(StepObject->GetNumberField(TEXT("radius")));

                FString ObjectiveTypeString;
                if (StepObject->TryGetStringField(TEXT("objectiveType"), ObjectiveTypeString))
                {
                    Step.ObjectiveType = ParseObjectiveType(ObjectiveTypeString);
                }

                double TimeLimitValue = Step.TimeLimitSeconds;
                if (StepObject->TryGetNumberField(TEXT("timeLimitSeconds"), TimeLimitValue))
                {
                    Step.TimeLimitSeconds = static_cast<float>(TimeLimitValue);
                }

                Step.bNeedVehicle = StepObject->GetBoolField(TEXT("needVehicle"));
                Step.Reward = StepObject->GetIntegerField(TEXT("reward"));

                if (StepObject->HasField(TEXT("crime")))
                {
                    Step.bIsCrime = StepObject->GetBoolField(TEXT("crime"));
                }

                int32 CrimeSeverityValue = Step.CrimeSeverity;
                if (StepObject->TryGetNumberField(TEXT("crimeSeverity"), CrimeSeverityValue))
                {
                    Step.CrimeSeverity = CrimeSeverityValue;
                }

                FString FactionTagString;
                if (StepObject->TryGetStringField(TEXT("reputationFaction"), FactionTagString) && !FactionTagString.IsEmpty())
                {
                    Step.ReputationFactionTag = FGameplayTag::RequestGameplayTag(FName(*FactionTagString), false);
                }

                int32 ReputationDeltaValue = Step.ReputationDelta;
                if (StepObject->TryGetNumberField(TEXT("reputationDelta"), ReputationDeltaValue))
                {
                    Step.ReputationDelta = ReputationDeltaValue;
                }

                if (StepObject->HasField(TEXT("requireNoHeat")))
                {
                    Step.bRequireNoHeat = StepObject->GetBoolField(TEXT("requireNoHeat"));
                }

                StepObject->TryGetStringField(TEXT("weatherOnStart"), Step.WeatherOnStart);
                Step.ConversationId = ReadNameField(StepObject, TEXT("conversationId"));
                Step.PayOfferId = ReadNameField(StepObject, TEXT("payOfferId"));
                Step.CollectSetId = ReadNameField(StepObject, TEXT("collectSet"));
                Step.CollectItemId = ReadNameField(StepObject, TEXT("collectItem"));

                int32 PayAmountValue = Step.PayAmount;
                if (StepObject->TryGetNumberField(TEXT("payAmount"), PayAmountValue))
                {
                    Step.PayAmount = PayAmountValue;
                }

                int32 CollectCountValue = Step.CollectCount;
                if (StepObject->TryGetNumberField(TEXT("collectCount"), CollectCountValue))
                {
                    Step.CollectCount = CollectCountValue;
                }

                const TArray<TSharedPtr<FJsonValue>>* TargetArray = nullptr;
                if (StepObject->TryGetArrayField(TEXT("target"), TargetArray) && TargetArray && TargetArray->Num() >= 3)
                {
                    Step.Target = FVector(
                        static_cast<float>((*TargetArray)[0]->AsNumber()),
                        static_cast<float>((*TargetArray)[1]->AsNumber()),
                        static_cast<float>((*TargetArray)[2]->AsNumber()));
                }

                Mission.Steps.Add(MoveTemp(Step));
            }
        }

        if (Mission.Steps.Num() > 0)
        {
            Missions.Add(MoveTemp(Mission));
        }
    }

    ResetCampaign();
    UE_LOG(LogTemp, Log, TEXT("MountHope: Loaded %d missions"), Missions.Num());
    return Missions.Num() > 0;
}

void UMHMissionSubsystem::ResetCampaign()
{
    MissionIndex = 0;
    StepIndex = 0;
    bCampaignComplete = Missions.Num() == 0;
}

bool UMHMissionSubsystem::AdvanceStep()
{
    if (bCampaignComplete || Missions.Num() == 0 || !Missions.IsValidIndex(MissionIndex))
    {
        return false;
    }

    const FMHMission& Mission = Missions[MissionIndex];
    if (!Mission.Steps.IsValidIndex(StepIndex))
    {
        return false;
    }

    ++StepIndex;
    if (Mission.Steps.IsValidIndex(StepIndex))
    {
        return true;
    }

    ++MissionIndex;
    StepIndex = 0;
    bCampaignComplete = !Missions.IsValidIndex(MissionIndex);
    return true;
}

void UMHMissionSubsystem::RestartCurrentMission()
{
    if (Missions.IsValidIndex(MissionIndex))
    {
        StepIndex = 0;
    }
}

bool UMHMissionSubsystem::IsMissionInProgress() const
{
    if (bCampaignComplete || !Missions.IsValidIndex(MissionIndex))
    {
        return false;
    }
    return Missions[MissionIndex].Steps.IsValidIndex(StepIndex);
}

bool UMHMissionSubsystem::GetCurrentMission(FMHMission& OutMission) const
{
    if (Missions.IsValidIndex(MissionIndex))
    {
        OutMission = Missions[MissionIndex];
        return true;
    }
    return false;
}

bool UMHMissionSubsystem::GetCurrentStep(FMHMissionStep& OutStep) const
{
    if (!Missions.IsValidIndex(MissionIndex))
    {
        return false;
    }

    const FMHMission& Mission = Missions[MissionIndex];
    if (Mission.Steps.IsValidIndex(StepIndex))
    {
        OutStep = Mission.Steps[StepIndex];
        return true;
    }

    return false;
}

bool UMHMissionSubsystem::FindMissionIndexById(FName MissionId, int32& OutIndex) const
{
    if (MissionId.IsNone())
    {
        return false;
    }

    for (int32 Index = 0; Index < Missions.Num(); ++Index)
    {
        if (Missions[Index].Id == MissionId)
        {
            OutIndex = Index;
            return true;
        }
    }

    return false;
}

bool UMHMissionSubsystem::CanAcceptCompletion(EMHObjectiveCompletionChannel Channel, FName ContextId) const
{
    FMHMissionStep Step;
    if (!GetCurrentStep(Step))
    {
        return false;
    }

    switch (Step.ObjectiveType)
    {
    case EMHObjectiveType::Reach:
    case EMHObjectiveType::Timed:
        return Channel == EMHObjectiveCompletionChannel::World || Channel == EMHObjectiveCompletionChannel::Dialogue;
    case EMHObjectiveType::Survive:
        return Channel == EMHObjectiveCompletionChannel::Timer;
    case EMHObjectiveType::Talk:
        return Channel == EMHObjectiveCompletionChannel::Dialogue
            && !Step.ConversationId.IsNone()
            && Step.ConversationId == ContextId;
    case EMHObjectiveType::Pay:
        return Channel == EMHObjectiveCompletionChannel::Payment
            && !Step.PayOfferId.IsNone()
            && Step.PayOfferId == ContextId
            && Step.PayAmount > 0;
    case EMHObjectiveType::Collect:
        if (Channel != EMHObjectiveCompletionChannel::Collection)
        {
            return false;
        }
        if (!Step.CollectSetId.IsNone())
        {
            return ContextId == Step.CollectSetId && Step.CollectCount > 0;
        }
        return !Step.CollectItemId.IsNone() && ContextId == Step.CollectItemId;
    default:
        return false;
    }
}

bool UMHMissionSubsystem::CanAcceptCollection(FName ItemId, FName SetId, int32 FoundInSet) const
{
    FMHMissionStep Step;
    if (!GetCurrentStep(Step) || Step.ObjectiveType != EMHObjectiveType::Collect)
    {
        return false;
    }

    if (!Step.CollectSetId.IsNone())
    {
        return SetId == Step.CollectSetId
            && Step.CollectCount > 0
            && FoundInSet >= Step.CollectCount
            && CanAcceptCompletion(EMHObjectiveCompletionChannel::Collection, SetId);
    }

    return ItemId == Step.CollectItemId
        && CanAcceptCompletion(EMHObjectiveCompletionChannel::Collection, ItemId);
}
