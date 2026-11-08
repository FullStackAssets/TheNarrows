#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MountHopeGameInstance.generated.h"

UCLASS(Config=Game)
class MOUNTHOPE_API UMountHopeGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "MountHope|Boot")
    FString SlicePath = TEXT("../QUAHOG_Web/public/slice-newbedford.json");

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "MountHope|Boot")
    FString MissionPath = TEXT("Data/Missions/vertical_slice.json");

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "MountHope|Boot")
    FString EconomyPath = TEXT("Data/Economy/businesses.json");

    // Distinct from the MountHope_Unreal slot so the two tracks do not share a
    // save file. Override in Config/DefaultGame.ini.
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "MountHope|Boot")
    FString SaveSlotName = TEXT("QuahogUnrealSlot");
};
