// Automation tests for the offer catalog quote. Cash spends need a game instance
// and are not covered here. See Docs/ARCHITECTURE.md.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MHEconomySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMHEconomyQuoteTest,
    "MountHope.Economy.OfferQuoteClampsDiscount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMHEconomyQuoteTest::RunTest(const FString& Parameters)
{
    UMHEconomySubsystem* Economy = NewObject<UMHEconomySubsystem>();
    if (!TestNotNull(TEXT("economy subsystem constructed"), Economy))
    {
        return false;
    }

    FMHEconomyOffer Offer;
    Offer.Id = FName(TEXT("dock_bribe"));
    Offer.DisplayName = TEXT("Dock Clerk Bribe");
    Offer.BasePrice = 200;
    Offer.Category = TEXT("bribe");
    Economy->Offers.Add(Offer);

    TestEqual(TEXT("full price"), Economy->QuoteOfferPrice(Offer.Id, 0), 200);
    TestEqual(TEXT("twenty-five percent off"), Economy->QuoteOfferPrice(Offer.Id, 25), 150);
    TestEqual(TEXT("discount clamps at 75 percent"), Economy->QuoteOfferPrice(Offer.Id, 100), 50);
    TestEqual(TEXT("negative discount is ignored"), Economy->QuoteOfferPrice(Offer.Id, -10), 200);
    TestEqual(TEXT("unknown offer"), Economy->QuoteOfferPrice(FName(TEXT("missing")), 0), -1);
    TestEqual(TEXT("static helper matches"), UMHEconomySubsystem::ComputeDiscountedPrice(80, 50), 40);
    TestFalse(TEXT("purchase without a game instance does not succeed"), Economy->TryPurchaseOffer(Offer.Id, 0));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
