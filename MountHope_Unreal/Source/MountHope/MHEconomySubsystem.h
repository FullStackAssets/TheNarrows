#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MHEconomySubsystem.generated.h"

/** One row in the purchasable catalog (`offers` in the economy JSON). */
USTRUCT(BlueprintType)
struct FMHEconomyOffer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    FName Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    int32 BasePrice = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    FString Category;
};

/** Append-only record of a cash change that actually landed. */
USTRUCT(BlueprintType)
struct FMHEconomyTransaction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    FName Reason = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    int32 Delta = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    int32 BalanceAfter = 0;
};

/**
 * Cash still lives on UMHGameStateSubsystem. This subsystem adds the offer
 * catalog, quoted prices, and a short ledger of successful cash changes.
 */
UCLASS()
class MOUNTHOPE_API UMHEconomySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Economy")
    TArray<FMHEconomyOffer> Offers;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Economy")
    bool LoadOffersFromJson(const FString& RelativeOrAbsolutePath);

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Economy")
    bool FindOffer(FName OfferId, FMHEconomyOffer& OutOffer) const;

    /** Discounted price, or -1 when the offer id is unknown. Discount clamps to 0–75. */
    UFUNCTION(BlueprintPure, Category = "Mount Hope|Economy")
    int32 QuoteOfferPrice(FName OfferId, int32 DiscountPercent) const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Economy")
    static int32 ComputeDiscountedPrice(int32 BasePrice, int32 DiscountPercent);

    /** Spend the quoted price. False when the offer is unknown or cash is short. */
    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Economy")
    bool TryPurchaseOffer(FName OfferId, int32 DiscountPercent);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Economy")
    int32 GetCashBalance() const;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Economy")
    void AddCash(int32 Amount, FName Reason);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Economy")
    bool SpendCash(int32 Amount, FName Reason);

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Economy")
    int32 GetLedgerCount() const { return Ledger.Num(); }

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Economy")
    bool GetLatestTransaction(FMHEconomyTransaction& OutTransaction) const;

private:
    UPROPERTY()
    TArray<FMHEconomyTransaction> Ledger;

    void RecordTransaction(FName Reason, int32 Delta, int32 BalanceAfter);
    static FString ResolvePath(const FString& RelativeOrAbsolutePath);
};
