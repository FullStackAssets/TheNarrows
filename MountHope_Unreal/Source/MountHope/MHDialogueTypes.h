#pragma once

#include "CoreMinimal.h"
#include "MHDialogueTypes.generated.h"

USTRUCT(BlueprintType)
struct FMHDialogueChoice
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FString Text;

    // Index into the conversation's line array. INDEX_NONE ends the conversation.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    int32 NextLineIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FMHDialogueLine
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FString Text;

    // Empty falls back to the conversation speaker.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FString Speaker;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    TArray<FMHDialogueChoice> Choices;

    // When true, advancing past this line ends the conversation instead of
    // falling through to the next array entry (used by branching endings).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    bool bEndsConversation = false;
};

USTRUCT(BlueprintType)
struct FMHDialogueCompletion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    bool bCompleteObjective = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    bool bRequireOnFoot = true;
};

USTRUCT(BlueprintType)
struct FMHDialogueConversation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FName ConversationId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FString Speaker;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FString Prompt;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    TArray<FMHDialogueLine> Lines;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    FMHDialogueCompletion OnComplete;
};
