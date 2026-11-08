// Automation tests for collectible set progress and one-shot completion bonuses.
// See Docs/ARCHITECTURE.md. These do not run without Unreal Engine.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MHCollectibleSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMHCollectibleSetBonusTest,
    "MountHope.Collectible.SetBonusAwardsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMHCollectibleSetBonusTest::RunTest(const FString& Parameters)
{
    UMHCollectibleSubsystem* Collectibles = NewObject<UMHCollectibleSubsystem>();
    if (!TestNotNull(TEXT("collectible subsystem constructed"), Collectibles))
    {
        return false;
    }

    FMHCollectibleRecord First;
    First.Id = FName(TEXT("scrimshaw_bethel_pew"));
    First.DisplayName = TEXT("Bethel");
    FMHCollectibleRecord Second;
    Second.Id = FName(TEXT("scrimshaw_fish_pier"));
    Second.DisplayName = TEXT("Pier");
    Collectibles->Collectibles.Add(First);
    Collectibles->Collectibles.Add(Second);

    FMHCollectibleSet Set;
    Set.Id = FName(TEXT("narrows_pair"));
    Set.DisplayName = TEXT("Narrows Pair");
    Set.MemberIds.Add(First.Id);
    Set.MemberIds.Add(Second.Id);
    Set.BonusCash = 400;
    Collectibles->CollectibleSets.Add(Set);

    TestTrue(TEXT("first pickup is new"), Collectibles->CollectItem(First.Id));
    TestFalse(TEXT("duplicate pickup is ignored"), Collectibles->CollectItem(First.Id));

    int32 Found = 0;
    int32 Total = 0;
    TestTrue(TEXT("set progress is readable"), Collectibles->GetSetProgress(Set.Id, Found, Total));
    TestEqual(TEXT("one of two found"), Found, 1);
    TestEqual(TEXT("set size"), Total, 2);

    TArray<FName> Completed;
    TestEqual(TEXT("no bonus before the set is complete"), Collectibles->ConsumePendingSetBonus(Completed), 0);

    TestTrue(TEXT("second pickup completes the set"), Collectibles->CollectItem(Second.Id));
    TestEqual(TEXT("bonus cash is pending"), Collectibles->ConsumePendingSetBonus(Completed), 400);
    TestEqual(TEXT("one set id reported"), Completed.Num(), 1);
    TestEqual(TEXT("bonus does not pay twice"), Collectibles->ConsumePendingSetBonus(Completed), 0);
    TestFalse(TEXT("unknown item is not collected"), Collectibles->CollectItem(FName(TEXT("missing"))));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
