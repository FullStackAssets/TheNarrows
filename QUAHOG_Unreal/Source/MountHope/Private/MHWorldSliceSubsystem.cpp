#include "MHWorldSliceSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ResolvePath(const FString& RelativeOrAbsolutePath)
{
    if (FPaths::FileExists(RelativeOrAbsolutePath))
    {
        return RelativeOrAbsolutePath;
    }

    return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeOrAbsolutePath);
}

bool TryReadNumber(const TSharedPtr<FJsonValue>& Value, float& OutNumber)
{
    if (!Value.IsValid() || Value->Type != EJson::Number)
    {
        return false;
    }

    OutNumber = static_cast<float>(Value->AsNumber());
    return true;
}

// Slice 2D contract shared with QUAHOG_Web/src/slice.ts `toWorld`:
// east -> +X, up -> +Y, north -> -Z. Mission targets in
// Data/Missions/vertical_slice.json are already in this space.
bool Slice2DToWorld(float EastMeters, float NorthMeters, FVector& OutWorld)
{
    OutWorld = FVector(EastMeters, 0.0f, -NorthMeters);
    return true;
}

bool ReadVec2Array(const TArray<TSharedPtr<FJsonValue>>& JsonPoints, TArray<FVector2D>& OutPoints)
{
    OutPoints.Reset();
    OutPoints.Reserve(JsonPoints.Num());

    for (const TSharedPtr<FJsonValue>& PointValue : JsonPoints)
    {
        const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
        if (!PointValue.IsValid() || !PointValue->TryGetArray(Pair) || !Pair || Pair->Num() < 2)
        {
            OutPoints.Reset();
            return false;
        }

        float X = 0.0f;
        float Y = 0.0f;
        if (!TryReadNumber((*Pair)[0], X) || !TryReadNumber((*Pair)[1], Y))
        {
            OutPoints.Reset();
            return false;
        }

        OutPoints.Add(FVector2D(X, Y));
    }

    return OutPoints.Num() >= 2;
}

bool ReadLandmarkLocation(const TArray<TSharedPtr<FJsonValue>>& Pos, FVector& OutLocation)
{
    if (Pos.Num() >= 3)
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        if (!TryReadNumber(Pos[0], X) || !TryReadNumber(Pos[1], Y) || !TryReadNumber(Pos[2], Z))
        {
            return false;
        }

        OutLocation = FVector(X, Y, Z);
        return true;
    }

    if (Pos.Num() == 2)
    {
        float East = 0.0f;
        float North = 0.0f;
        if (!TryReadNumber(Pos[0], East) || !TryReadNumber(Pos[1], North))
        {
            return false;
        }

        return Slice2DToWorld(East, North, OutLocation);
    }

    return false;
}

FString ReadOptionalString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    FString Value;
    if (Object.IsValid() && Object->TryGetStringField(Field, Value))
    {
        return Value;
    }
    return FString();
}
}

bool UMHWorldSliceSubsystem::LoadSliceFromJson(const FString& RelativeOrAbsolutePath)
{
    const FString FullPath = ResolvePath(RelativeOrAbsolutePath);
    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Unable to load slice JSON: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Invalid JSON in slice file: %s"), *FullPath);
        return false;
    }

    if (!RootObject->TryGetStringField(TEXT("name"), SliceName) || SliceName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("MountHope: Slice JSON is missing a name: %s"), *FullPath);
        return false;
    }

    const TSharedPtr<FJsonObject>* OriginObject = nullptr;
    if (RootObject->TryGetObjectField(TEXT("origin"), OriginObject) && OriginObject && OriginObject->IsValid())
    {
        double Lat = 0.0;
        double Lon = 0.0;
        if ((*OriginObject)->TryGetNumberField(TEXT("lat"), Lat) && (*OriginObject)->TryGetNumberField(TEXT("lon"), Lon))
        {
            OriginLatLon.X = static_cast<float>(Lat);
            OriginLatLon.Y = static_cast<float>(Lon);
        }
    }

    Roads.Reset();
    Buildings.Reset();
    Landmarks.Reset();
    Water.Reset();

    const TArray<TSharedPtr<FJsonValue>>* RoadsArray = nullptr;
    if (RootObject->TryGetArrayField(TEXT("roads"), RoadsArray) && RoadsArray)
    {
        Roads.Reserve(RoadsArray->Num());
        for (const TSharedPtr<FJsonValue>& RoadValue : *RoadsArray)
        {
            const TSharedPtr<FJsonObject> RoadObject = RoadValue->AsObject();
            if (!RoadObject.IsValid())
            {
                continue;
            }

            FMHRoadRecord Record;
            // OSM roads often store `"name": null`. TryGetStringField leaves those
            // blank and keeps the centerline. GetStringField would log an error
            // for every unnamed road in the committed slice.
            Record.Name = ReadOptionalString(RoadObject, TEXT("name"));
            Record.Highway = ReadOptionalString(RoadObject, TEXT("highway"));
            double Width = Record.Width;
            if (RoadObject->TryGetNumberField(TEXT("width"), Width))
            {
                Record.Width = static_cast<float>(Width);
            }

            const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
            if (!RoadObject->TryGetArrayField(TEXT("points"), PointsArray) || !PointsArray
                || !ReadVec2Array(*PointsArray, Record.Centerline.Points))
            {
                continue;
            }

            Roads.Add(MoveTemp(Record));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* BuildingsArray = nullptr;
    if (RootObject->TryGetArrayField(TEXT("buildings"), BuildingsArray) && BuildingsArray)
    {
        Buildings.Reserve(BuildingsArray->Num());
        for (const TSharedPtr<FJsonValue>& BuildingValue : *BuildingsArray)
        {
            const TSharedPtr<FJsonObject> BuildingObject = BuildingValue->AsObject();
            if (!BuildingObject.IsValid())
            {
                continue;
            }

            FMHBuildingRecord Record;
            Record.Name = ReadOptionalString(BuildingObject, TEXT("name"));
            double Height = Record.Height;
            if (BuildingObject->TryGetNumberField(TEXT("height"), Height))
            {
                Record.Height = static_cast<float>(Height);
            }

            const TArray<TSharedPtr<FJsonValue>>* FootprintArray = nullptr;
            if (!BuildingObject->TryGetArrayField(TEXT("footprint"), FootprintArray) || !FootprintArray
                || !ReadVec2Array(*FootprintArray, Record.Footprint.Points))
            {
                continue;
            }

            Buildings.Add(MoveTemp(Record));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* LandmarksArray = nullptr;
    if (RootObject->TryGetArrayField(TEXT("landmarks"), LandmarksArray) && LandmarksArray)
    {
        Landmarks.Reserve(LandmarksArray->Num());
        for (const TSharedPtr<FJsonValue>& LandmarkValue : *LandmarksArray)
        {
            const TSharedPtr<FJsonObject> LandmarkObject = LandmarkValue->AsObject();
            if (!LandmarkObject.IsValid())
            {
                continue;
            }

            FMHLandmarkRecord Record;
            Record.Name = ReadOptionalString(LandmarkObject, TEXT("name"));
            Record.Kind = ReadOptionalString(LandmarkObject, TEXT("kind"));
            bool bHero = false;
            if (LandmarkObject->TryGetBoolField(TEXT("hero"), bHero))
            {
                Record.bHero = bHero;
            }

            const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
            if (LandmarkObject->TryGetArrayField(TEXT("pos"), Pos) && Pos)
            {
                FVector Location = FVector::ZeroVector;
                if (ReadLandmarkLocation(*Pos, Location))
                {
                    Record.Location = Location;
                }
            }

            if (!Record.Name.IsEmpty())
            {
                Landmarks.Add(MoveTemp(Record));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* WaterArray = nullptr;
    if (RootObject->TryGetArrayField(TEXT("water"), WaterArray) && WaterArray)
    {
        Water.Reserve(WaterArray->Num());
        for (const TSharedPtr<FJsonValue>& RingValue : *WaterArray)
        {
            const TArray<TSharedPtr<FJsonValue>>* Ring = nullptr;
            if (!RingValue.IsValid() || !RingValue->TryGetArray(Ring) || !Ring)
            {
                continue;
            }

            FMHWaterPolygon Polygon;
            if (!ReadVec2Array(*Ring, Polygon.Ring.Points) || Polygon.Ring.Points.Num() < 3)
            {
                continue;
            }

            Water.Add(MoveTemp(Polygon));
        }
    }

    bool bBuildingsStreamed = false;
    RootObject->TryGetBoolField(TEXT("buildingsStreamed"), bBuildingsStreamed);
    if (bBuildingsStreamed && Buildings.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("MountHope: buildings array is empty because buildingsStreamed is true. This track does not load a sidecar building file."));
    }

    UE_LOG(LogTemp, Log, TEXT("MountHope: Loaded slice '%s' (%d roads, %d buildings, %d landmarks, %d water rings)"),
        *SliceName, Roads.Num(), Buildings.Num(), Landmarks.Num(), Water.Num());

    return true;
}

bool UMHWorldSliceSubsystem::IsSliceLoaded() const
{
    return !SliceName.IsEmpty() && Roads.Num() > 0;
}
