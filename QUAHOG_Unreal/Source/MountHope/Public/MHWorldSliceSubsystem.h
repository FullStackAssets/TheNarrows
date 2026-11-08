#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MHWorldSliceTypes.h"
#include "MHWorldSliceSubsystem.generated.h"

UCLASS(BlueprintType)
class MOUNTHOPE_API UMHWorldSliceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FString SliceName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    TArray<FMHRoadRecord> Roads;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    TArray<FMHBuildingRecord> Buildings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    TArray<FMHLandmarkRecord> Landmarks;

    // Slice water rings. Empty when the file has no `water` array. Not part of
    // the MountHope_Unreal subsystem surface.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    TArray<FMHWaterPolygon> Water;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FVector2D OriginLatLon = FVector2D::ZeroVector;

    UFUNCTION(BlueprintCallable, Category = "MountHope|World")
    bool LoadSliceFromJson(const FString& RelativeOrAbsolutePath);

    UFUNCTION(BlueprintPure, Category = "MountHope|World")
    bool IsSliceLoaded() const;
};
