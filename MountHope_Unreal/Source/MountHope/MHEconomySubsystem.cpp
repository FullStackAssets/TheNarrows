#include "MHEconomySubsystem.h"

#include "Dom/JsonObject.h"
#include "MHGameStateSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 MaxDiscountPercent = 75;
constexpr int32 MaxLedgerEntries = 24;
}

FString UMHEconomySubsystem::ResolvePath(const FString& RelativeOrAbsolutePath)
{
    if (FPaths::FileExists(RelativeOrAbsolutePath))
    {
        return RelativeOrAbsolutePath;
    }

    return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeOrAbsolutePath);
}

int32 UMHEconomySubsystem::ComputeDiscountedPrice(int32 BasePrice, int32 DiscountPercent)
{
    if (BasePrice <= 0)
    {
        return 0;
    }

    const int32 Clamped = FMath::Clamp(DiscountPercent, 0, MaxDiscountPercent);
    const float Scale = static_cast<float>(100 - Clamped) / 100.0f;
    return FMath::Max(0, FMath::RoundToInt(static_cast<float>(BasePrice) * Scale));
}

bool UMHEconomySubsystem::LoadOffersFromJson(const FString& RelativeOrAbsolutePath)
{
    const FString FullPath = ResolvePath(RelativeOrAbsolutePath);
    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Unable to load economy JSON: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Invalid economy JSON: %s"), *FullPath);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* OffersArray = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("offers"), OffersArray) || !OffersArray)
    {
        UE_LOG(LogTemp, Warning, TEXT("MountHope: Economy JSON has no offers array: %s"), *FullPath);
        return false;
    }

    Offers.Reset();
    TSet<FName> SeenIds;
    for (const TSharedPtr<FJsonValue>& Value : *OffersArray)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid())
        {
            continue;
        }

        FString IdString;
        FString DisplayName;
        if (!Object->TryGetStringField(TEXT("id"), IdString) || IdString.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping economy offer with missing id"));
            continue;
        }
        if (!Object->TryGetStringField(TEXT("name"), DisplayName) || DisplayName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping economy offer '%s' with missing name"), *IdString);
            continue;
        }

        double PriceValue = 0.0;
        if (!Object->TryGetNumberField(TEXT("price"), PriceValue) || PriceValue < 0.0)
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping economy offer '%s' with invalid price"), *IdString);
            continue;
        }

        const FName OfferId(*IdString);
        if (SeenIds.Contains(OfferId))
        {
            UE_LOG(LogTemp, Warning, TEXT("MountHope: Skipping duplicate economy offer '%s'"), *IdString);
            continue;
        }

        FMHEconomyOffer Offer;
        Offer.Id = OfferId;
        Offer.DisplayName = DisplayName;
        Offer.BasePrice = FMath::RoundToInt(PriceValue);
        Object->TryGetStringField(TEXT("category"), Offer.Category);
        SeenIds.Add(OfferId);
        Offers.Add(MoveTemp(Offer));
    }

    UE_LOG(LogTemp, Log, TEXT("MountHope: Loaded %d economy offers"), Offers.Num());
    return Offers.Num() > 0;
}

bool UMHEconomySubsystem::FindOffer(FName OfferId, FMHEconomyOffer& OutOffer) const
{
    for (const FMHEconomyOffer& Offer : Offers)
    {
        if (Offer.Id == OfferId)
        {
            OutOffer = Offer;
            return true;
        }
    }

    return false;
}

int32 UMHEconomySubsystem::QuoteOfferPrice(FName OfferId, int32 DiscountPercent) const
{
    FMHEconomyOffer Offer;
    if (!FindOffer(OfferId, Offer))
    {
        return -1;
    }

    return ComputeDiscountedPrice(Offer.BasePrice, DiscountPercent);
}

bool UMHEconomySubsystem::TryPurchaseOffer(FName OfferId, int32 DiscountPercent)
{
    const int32 Price = QuoteOfferPrice(OfferId, DiscountPercent);
    if (Price < 0)
    {
        return false;
    }

    return SpendCash(Price, OfferId);
}

int32 UMHEconomySubsystem::GetCashBalance() const
{
    if (const UMHGameStateSubsystem* GameState = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>()
        : nullptr)
    {
        return GameState->Cash;
    }

    return 0;
}

void UMHEconomySubsystem::AddCash(int32 Amount, FName Reason)
{
    if (Amount <= 0)
    {
        return;
    }

    if (UMHGameStateSubsystem* GameState = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>()
        : nullptr)
    {
        GameState->AddCash(Amount);
        RecordTransaction(Reason, Amount, GameState->Cash);
    }
}

bool UMHEconomySubsystem::SpendCash(int32 Amount, FName Reason)
{
    if (Amount <= 0)
    {
        return false;
    }

    UMHGameStateSubsystem* GameState = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UMHGameStateSubsystem>()
        : nullptr;
    if (!GameState || GameState->Cash < Amount)
    {
        return false;
    }

    GameState->AddCash(-Amount);
    RecordTransaction(Reason, -Amount, GameState->Cash);
    return true;
}

bool UMHEconomySubsystem::GetLatestTransaction(FMHEconomyTransaction& OutTransaction) const
{
    if (Ledger.Num() == 0)
    {
        return false;
    }

    OutTransaction = Ledger.Last();
    return true;
}

void UMHEconomySubsystem::RecordTransaction(FName Reason, int32 Delta, int32 BalanceAfter)
{
    FMHEconomyTransaction Transaction;
    Transaction.Reason = Reason;
    Transaction.Delta = Delta;
    Transaction.BalanceAfter = BalanceAfter;
    Ledger.Add(Transaction);
    if (Ledger.Num() > MaxLedgerEntries)
    {
        Ledger.RemoveAt(0, Ledger.Num() - MaxLedgerEntries);
    }
}
