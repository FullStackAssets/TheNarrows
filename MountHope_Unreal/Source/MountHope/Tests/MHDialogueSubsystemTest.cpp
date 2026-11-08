// Automation tests for branching dialogue. Pure logic on a NewObject subsystem.
// See Docs/ARCHITECTURE.md. These do not run without Unreal Engine.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MHDialogueSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UMHDialogueSubsystem* MakeBranchingConversation()
{
    UMHDialogueSubsystem* Dialogue = NewObject<UMHDialogueSubsystem>();
    if (!Dialogue)
    {
        return nullptr;
    }

    FMHDialogueConversation Conversation;
    Conversation.ConversationId = FName(TEXT("branch"));
    Conversation.Speaker = TEXT("Deacon");

    FMHDialogueLine Prompt;
    Prompt.Text = TEXT("How do you want this done?");
    FMHDialogueChoice Quiet;
    Quiet.Text = TEXT("Quiet");
    Quiet.NextLineIndex = 1;
    FMHDialogueChoice Loud;
    Loud.Text = TEXT("Loud");
    Loud.NextLineIndex = 2;
    Prompt.Choices.Add(Quiet);
    Prompt.Choices.Add(Loud);

    FMHDialogueLine QuietLine;
    QuietLine.Text = TEXT("Quiet money.");
    QuietLine.Speaker = TEXT("Deacon");
    QuietLine.bEndsConversation = true;

    FMHDialogueLine LoudLine;
    LoudLine.Text = TEXT("Loud is a choice.");
    LoudLine.bEndsConversation = true;

    Conversation.Lines.Add(Prompt);
    Conversation.Lines.Add(QuietLine);
    Conversation.Lines.Add(LoudLine);
    Dialogue->Conversations.Add(Conversation);
    return Dialogue;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMHDialogueChoiceBranchTest,
    "MountHope.Dialogue.ChoiceSelectsBranchAndAdvanceEnds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMHDialogueChoiceBranchTest::RunTest(const FString& Parameters)
{
    UMHDialogueSubsystem* Dialogue = MakeBranchingConversation();
    if (!TestNotNull(TEXT("dialogue subsystem constructed"), Dialogue))
    {
        return false;
    }

    TestTrue(TEXT("conversation starts"), Dialogue->StartConversation(FName(TEXT("branch"))));
    TestEqual(TEXT("prompt exposes two choices"), Dialogue->GetCurrentChoiceCount(), 2);
    TestFalse(TEXT("advance does not skip a choice line"), Dialogue->AdvanceConversation(false));
    TestTrue(TEXT("conversation stays active until a choice is picked"), Dialogue->IsConversationActive());
    TestFalse(TEXT("out-of-range choice is rejected"), Dialogue->SelectChoice(5, false));

    TestTrue(TEXT("first choice is accepted"), Dialogue->SelectChoice(0, false));
    FText Line;
    FName Speaker;
    TestTrue(TEXT("quiet branch is showing"), Dialogue->GetCurrentLine(Line, Speaker));
    TestEqual(TEXT("quiet line text"), Line.ToString(), FString(TEXT("Quiet money.")));
    TestEqual(TEXT("line speaker override"), Speaker.ToString(), FString(TEXT("Deacon")));
    TestEqual(TEXT("ending line has no choices"), Dialogue->GetCurrentChoiceCount(), 0);

    TestFalse(TEXT("advancing an ending line closes the conversation"), Dialogue->AdvanceConversation(false));
    TestFalse(TEXT("conversation is inactive after the ending line"), Dialogue->IsConversationActive());

    TestTrue(TEXT("conversation can restart"), Dialogue->StartConversation(FName(TEXT("branch"))));
    TestTrue(TEXT("second choice is accepted"), Dialogue->SelectChoice(1, false));
    TestTrue(TEXT("loud branch is showing"), Dialogue->GetCurrentLine(Line, Speaker));
    TestEqual(TEXT("loud line text"), Line.ToString(), FString(TEXT("Loud is a choice.")));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
