// Automation tests for UMHMissionSubsystem step/mission progression, including
// the failure/restart additions. Pure logic — drives a NewObject subsystem with
// a hand-built campaign. See Docs/ARCHITECTURE.md.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MHMissionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UMHMissionSubsystem* MakeTwoStepCampaign()
{
    UMHMissionSubsystem* Missions = NewObject<UMHMissionSubsystem>();
    if (!Missions)
    {
        return nullptr;
    }

    FMHMission Mission;
    Mission.Title = TEXT("Test Mission");
    FMHMissionStep StepA;
    StepA.Text = TEXT("Step A");
    FMHMissionStep StepB;
    StepB.Text = TEXT("Step B");
    Mission.Steps.Add(StepA);
    Mission.Steps.Add(StepB);
    Missions->Missions.Add(Mission);
    Missions->ResetCampaign();
    return Missions;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMHMissionAdvanceTest,
    "MountHope.Mission.StepsAdvanceAndCampaignCompletes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMHMissionAdvanceTest::RunTest(const FString& Parameters)
{
    UMHMissionSubsystem* Missions = MakeTwoStepCampaign();
    if (!TestNotNull(TEXT("mission subsystem constructed"), Missions))
    {
        return false;
    }

    TestTrue(TEXT("mission is in progress at start"), Missions->IsMissionInProgress());
    TestEqual(TEXT("starts at step 0"), Missions->StepIndex, 0);

    Missions->AdvanceStep();
    TestEqual(TEXT("advances to step 1"), Missions->StepIndex, 1);

    Missions->AdvanceStep();
    TestTrue(TEXT("campaign completes after the final step"), Missions->bCampaignComplete);
    TestFalse(TEXT("no mission in progress once the campaign is complete"), Missions->IsMissionInProgress());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMHMissionRestartTest,
    "MountHope.Mission.RestartReturnsToFirstStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMHMissionRestartTest::RunTest(const FString& Parameters)
{
    UMHMissionSubsystem* Missions = MakeTwoStepCampaign();
    if (!TestNotNull(TEXT("mission subsystem constructed"), Missions))
    {
        return false;
    }

    Missions->AdvanceStep();
    TestEqual(TEXT("progressed off the first step"), Missions->StepIndex, 1);

    Missions->RestartCurrentMission();
    TestEqual(TEXT("restart returns to the first step"), Missions->StepIndex, 0);
    TestTrue(TEXT("mission is in progress again after restart"), Missions->IsMissionInProgress());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMHMissionObjectiveChannelTest,
    "MountHope.Mission.ObjectiveChannelsAcceptOnlyTheirSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMHMissionObjectiveChannelTest::RunTest(const FString& Parameters)
{
    UMHMissionSubsystem* Missions = NewObject<UMHMissionSubsystem>();
    if (!TestNotNull(TEXT("mission subsystem constructed"), Missions))
    {
        return false;
    }

    FMHMission Mission;
    Mission.Id = FName(TEXT("sample"));
    Mission.Title = TEXT("Sample");

    FMHMissionStep Talk;
    Talk.Text = TEXT("Talk");
    Talk.ObjectiveType = EMHObjectiveType::Talk;
    Talk.ConversationId = FName(TEXT("deacon_mealy_harbor_errand"));

    FMHMissionStep Pay;
    Pay.Text = TEXT("Pay");
    Pay.ObjectiveType = EMHObjectiveType::Pay;
    Pay.PayOfferId = FName(TEXT("dock_bribe"));
    Pay.PayAmount = 200;

    FMHMissionStep Collect;
    Collect.Text = TEXT("Collect");
    Collect.ObjectiveType = EMHObjectiveType::Collect;
    Collect.CollectSetId = FName(TEXT("bethel_pew"));
    Collect.CollectCount = 1;

    FMHMissionStep Survive;
    Survive.Text = TEXT("Survive");
    Survive.ObjectiveType = EMHObjectiveType::Survive;
    Survive.TimeLimitSeconds = 10.0f;

    Mission.Steps.Add(Talk);
    Mission.Steps.Add(Pay);
    Mission.Steps.Add(Collect);
    Mission.Steps.Add(Survive);
    Missions->Missions.Add(Mission);
    Missions->ResetCampaign();

    int32 FoundIndex = INDEX_NONE;
    TestTrue(TEXT("finds the mission id"), Missions->FindMissionIndexById(FName(TEXT("sample")), FoundIndex));
    TestEqual(TEXT("mission id index"), FoundIndex, 0);
    TestFalse(TEXT("unknown mission id misses"), Missions->FindMissionIndexById(FName(TEXT("missing")), FoundIndex));

    TestTrue(TEXT("talk accepts its conversation"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::Dialogue, Talk.ConversationId));
    TestFalse(TEXT("talk rejects a world trigger"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::World, NAME_None));
    TestFalse(TEXT("talk rejects a different conversation"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::Dialogue, FName(TEXT("other"))));

    Missions->AdvanceStep();
    TestTrue(TEXT("pay accepts its offer"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::Payment, Pay.PayOfferId));
    TestFalse(TEXT("pay rejects dialogue"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::Dialogue, Pay.PayOfferId));

    Missions->AdvanceStep();
    TestFalse(TEXT("collect rejects a short set"), Missions->CanAcceptCollection(NAME_None, Collect.CollectSetId, 0));
    TestTrue(TEXT("collect accepts a finished set"), Missions->CanAcceptCollection(NAME_None, Collect.CollectSetId, 1));
    TestFalse(TEXT("collect rejects a world trigger"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::World, Collect.CollectSetId));

    Missions->AdvanceStep();
    TestTrue(TEXT("survive accepts the timer"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::Timer, NAME_None));
    TestFalse(TEXT("survive rejects a world trigger"), Missions->CanAcceptCompletion(EMHObjectiveCompletionChannel::World, NAME_None));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
