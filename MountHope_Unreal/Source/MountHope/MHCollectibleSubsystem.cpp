#include "MHCollectibleSubsystem.h"

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
}

bool UMHCollectibleSubsystem::LoadCollectiblesFromJson(const FString& RelativeOrAbsolutePath)
{
    const FString FullPath = ResolvePath(RelativeOrAbsolutePath);
    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Unable to load collectibles JSON: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Invalid collectibles JSON: %s"), *FullPath);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* CollectiblesArray = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("collectibles"), CollectiblesArray) || !CollectiblesArray)
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Missing 'collectibles' field in %s"), *FullPath);
        return false;
    }

    Collectibles.Reset();
    Collectibles.Reserve(CollectiblesArray->Num());
    for (const TSharedPtr<FJsonValue>& Value : *CollectiblesArray)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid())
        {
            continue;
        }

        FString IdString;
        if (!Object->TryGetStringField(TEXT("id"), IdString) || IdString.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping collectible entry with missing/empty id"));
            continue;
        }

        FString DisplayName;
        if (!Object->TryGetStringField(TEXT("name"), DisplayName) || DisplayName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping collectible '%s' with missing/empty name"), *IdString);
            continue;
        }

        FMHCollectibleRecord Record;
        Record.Id = FName(*IdString);
        Record.DisplayName = DisplayName;
        Object->TryGetStringField(TEXT("flavor"), Record.FlavorText);

        int32 Reward = Record.CashReward;
        if (Object->TryGetNumberField(TEXT("cashReward"), Reward))
        {
            Record.CashReward = Reward;
        }

        const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
        if (Object->TryGetArrayField(TEXT("pos"), Pos) && Pos && Pos->Num() >= 3)
        {
            Record.WorldLocation = FVector(
                static_cast<float>((*Pos)[0]->AsNumber()),
                static_cast<float>((*Pos)[1]->AsNumber()),
                static_cast<float>((*Pos)[2]->AsNumber()));
        }

        if (!Record.Id.IsNone())
        {
            Collectibles.Add(MoveTemp(Record));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* SetsArray = nullptr;
    if (RootObject->TryGetArrayField(TEXT("sets"), SetsArray) && SetsArray)
    {
        CollectibleSets.Reset();
        TSet<FName> SeenSetIds;
        for (const TSharedPtr<FJsonValue>& Value : *SetsArray)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (!Object.IsValid())
            {
                continue;
            }

            FString SetIdString;
            FString SetName;
            if (!Object->TryGetStringField(TEXT("id"), SetIdString) || SetIdString.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping collectible set with missing id"));
                continue;
            }
            if (!Object->TryGetStringField(TEXT("name"), SetName) || SetName.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping collectible set '%s' with missing name"), *SetIdString);
                continue;
            }

            const FName SetId(*SetIdString);
            if (SeenSetIds.Contains(SetId))
            {
                UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping duplicate collectible set '%s'"), *SetIdString);
                continue;
            }

            FMHCollectibleSet Set;
            Set.Id = SetId;
            Set.DisplayName = SetName;
            int32 Bonus = Set.BonusCash;
            if (Object->TryGetNumberField(TEXT("bonusCash"), Bonus) && Bonus >= 0)
            {
                Set.BonusCash = Bonus;
            }

            const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
            if (Object->TryGetArrayField(TEXT("members"), Members) && Members)
            {
                for (const TSharedPtr<FJsonValue>& MemberValue : *Members)
                {
                    const FString MemberId = MemberValue->AsString();
                    if (!MemberId.IsEmpty())
                    {
                        Set.MemberIds.Add(FName(*MemberId));
                    }
                }
            }

            if (Set.MemberIds.Num() == 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping collectible set '%s' with no members"), *SetIdString);
                continue;
            }

            SeenSetIds.Add(SetId);
            CollectibleSets.Add(MoveTemp(Set));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("MountHope: Loaded %d collectibles (%d sets)"), Collectibles.Num(), CollectibleSets.Num());
    return Collectibles.Num() > 0;
}

bool UMHCollectibleSubsystem::CollectItem(FName ItemId)
{
    if (ItemId.IsNone() || CollectedIds.Contains(ItemId))
    {
        return false;
    }

    FMHCollectibleRecord Record;
    if (!FindCollectible(ItemId, Record))
    {
        return false;
    }

    CollectedIds.Add(ItemId);
    EvaluateSetBonuses();
    OnCollectibleFound.Broadcast(ItemId, GetCollectedCount(), GetTotalCount());
    return true;
}

bool UMHCollectibleSubsystem::GetSetProgress(FName SetId, int32& OutFound, int32& OutTotal) const
{
    OutFound = 0;
    OutTotal = 0;
    for (const FMHCollectibleSet& Set : CollectibleSets)
    {
        if (Set.Id != SetId)
        {
            continue;
        }

        OutTotal = Set.MemberIds.Num();
        for (const FName& MemberId : Set.MemberIds)
        {
            if (CollectedIds.Contains(MemberId))
            {
                ++OutFound;
            }
        }
        return true;
    }

    return false;
}

int32 UMHCollectibleSubsystem::ConsumePendingSetBonus(TArray<FName>& OutCompletedSetIds)
{
    OutCompletedSetIds = PendingCompletedSetIds;
    const int32 Bonus = PendingSetBonus;
    PendingSetBonus = 0;
    PendingCompletedSetIds.Reset();
    return Bonus;
}

void UMHCollectibleSubsystem::EvaluateSetBonuses()
{
    for (FMHCollectibleSet& Set : CollectibleSets)
    {
        if (Set.bBonusAwarded || Set.BonusCash <= 0 || Set.MemberIds.Num() == 0)
        {
            continue;
        }

        int32 Found = 0;
        for (const FName& MemberId : Set.MemberIds)
        {
            if (CollectedIds.Contains(MemberId))
            {
                ++Found;
            }
        }

        if (Found == Set.MemberIds.Num())
        {
            Set.bBonusAwarded = true;
            PendingSetBonus += Set.BonusCash;
            PendingCompletedSetIds.Add(Set.Id);
        }
    }
}

bool UMHCollectibleSubsystem::IsCollected(FName ItemId) const
{
    return CollectedIds.Contains(ItemId);
}

bool UMHCollectibleSubsystem::FindCollectible(FName ItemId, FMHCollectibleRecord& OutRecord) const
{
    for (const FMHCollectibleRecord& Record : Collectibles)
    {
        if (Record.Id == ItemId)
        {
            OutRecord = Record;
            return true;
        }
    }

    return false;
}

TArray<FString> UMHCollectibleSubsystem::GetCollectedIdsAsStrings() const
{
    TArray<FString> Result;
    Result.Reserve(CollectedIds.Num());
    for (const FName& Id : CollectedIds)
    {
        Result.Add(Id.ToString());
    }

    return Result;
}

void UMHCollectibleSubsystem::RestoreCollectedIds(const TArray<FString>& SavedIds)
{
    CollectedIds.Reset();
    for (const FString& Id : SavedIds)
    {
        if (!Id.IsEmpty())
        {
            CollectedIds.Add(FName(*Id));
        }
    }
}
