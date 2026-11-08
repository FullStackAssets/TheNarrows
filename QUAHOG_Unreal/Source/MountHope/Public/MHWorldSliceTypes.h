#pragma once

#include "CoreMinimal.h"
#include "MHWorldSliceTypes.generated.h"

USTRUCT(BlueprintType)
struct FMHPolyline2D
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    TArray<FVector2D> Points;
};

USTRUCT(BlueprintType)
struct FMHRoadRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FString Highway;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    float Width = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FMHPolyline2D Centerline;
};

USTRUCT(BlueprintType)
struct FMHBuildingRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    float Height = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FMHPolyline2D Footprint;
};

USTRUCT(BlueprintType)
struct FMHLandmarkRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FString Name;

    // OSM kind string from the slice (`museum`, `attraction`, ...). Empty when
    // the file omits it. Local to this track; MountHope_Unreal's landmark
    // record does not have this field.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FString Kind;

    // World metres after slice conversion: X = east, Y = up, Z = -north.
    // A 2-component slice position is lifted with Y = 0. A 3-component position
    // is stored as given.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    bool bHero = false;
};

// One water ring from the slice. Points stay in slice-local 2D (X = east
// metres, Y = north metres), the same space as road centerlines. This record
// exists only on the QUAHOG_Unreal track.
USTRUCT(BlueprintType)
struct FMHWaterPolygon
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MountHope|World")
    FMHPolyline2D Ring;
};
