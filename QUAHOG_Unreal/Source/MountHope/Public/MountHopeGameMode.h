#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MountHopeGameMode.generated.h"

class AMHMissionTriggerActor;
struct FMHMissionStep;

UCLASS()
class MOUNTHOPE_API AMountHopeGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMountHopeGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "MountHope|Mission")
    bool CompleteCurrentObjective(bool bPlayerInVehicle);

    UFUNCTION(BlueprintCallable, Category = "MountHope|Mission")
    bool TryCompleteVehicleObjective(bool bPlayerInVehicle);

protected:
    UPROPERTY(Transient)
    TObjectPtr<AMHMissionTriggerActor> ObjectiveTrigger = nullptr;

private:
    bool IsWorldTargetObjective(const FMHMissionStep& Step) const;
    void RefreshObjectiveTrigger();
    FString ResolveSaveSlotName() const;
};
