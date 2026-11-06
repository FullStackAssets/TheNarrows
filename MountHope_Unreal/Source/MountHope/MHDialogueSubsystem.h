#pragma once

#include "CoreMinimal.h"
#include "MHDialogueTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MHDialogueSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FMHOnDialogueLineChanged,
    FName,
    ConversationId,
    FName,
    Speaker,
    FText,
    LineText);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMHOnDialogueEnded, FName, ConversationId);

UCLASS(BlueprintType)
class MOUNTHOPE_API UMHDialogueSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "Mount Hope|Dialogue")
    FMHOnDialogueLineChanged OnDialogueLineChanged;

    UPROPERTY(BlueprintAssignable, Category = "Mount Hope|Dialogue")
    FMHOnDialogueEnded OnDialogueEnded;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Dialogue")
    TArray<FMHDialogueConversation> Conversations;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Dialogue")
    bool LoadDialogueFromJson(const FString& RelativeOrAbsolutePath);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Dialogue")
    bool StartConversation(FName ConversationId);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Dialogue")
    bool AdvanceConversation(bool bPlayerInVehicle);

    // Pick a choice on the current line. Returns false when the line has no
    // choices or the index is out of range; does not fall through to Advance.
    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Dialogue")
    bool SelectChoice(int32 ChoiceIndex, bool bPlayerInVehicle);

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Dialogue")
    int32 GetCurrentChoiceCount() const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Dialogue")
    bool GetCurrentChoiceText(int32 ChoiceIndex, FString& OutText) const;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Dialogue")
    void EndConversation();

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Dialogue")
    bool IsConversationActive() const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Dialogue")
    bool GetActiveConversation(FMHDialogueConversation& OutConversation) const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Dialogue")
    bool GetCurrentLine(FText& OutLineText, FName& OutSpeaker) const;

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Dialogue")
    bool FindConversation(FName ConversationId, FMHDialogueConversation& OutConversation) const;

private:
    UPROPERTY()
    FName ActiveConversationId = NAME_None;

    UPROPERTY()
    int32 ActiveLineIndex = INDEX_NONE;

    bool ApplyCompletion(const FMHDialogueCompletion& Completion, bool bPlayerInVehicle, FName ConversationId);
    void BroadcastCurrentLine();
    bool FinishConversation(bool bPlayerInVehicle);
    bool CurrentLineHasChoices() const;
};
