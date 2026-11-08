# Interface alignment plan

Source reading of `QUAHOG_Unreal/` and `MountHope_Unreal/` for a later
alignment pass. Both trees were read as files. They were not compiled together,
not loaded in one editor session, and not compared in PIE. Nothing here is a
runtime compatibility result.

This track does not copy or overwrite `MountHope_Unreal` systems (wanted,
dialogue, reputation, radio, collectibles, police, HUD, weather director, or
its save schema).

## Names that currently differ

| Role | This tree | MountHope_Unreal (header as read) |
| --- | --- | --- |
| Module | `MountHope` | `MountHope` |
| Uproject engine association | `5.8` | `5.8` |
| Game instance | `UMountHopeGameInstance` | `UMHGameInstance` |
| Game mode | `AMountHopeGameMode` | `AMHGameModeBase` |
| Player | `AMountHopeCharacter` | `AMHPlayerCharacter` |
| Vehicle | `AMountHopeVehiclePawn` | `AMHVehiclePawn` |
| Default game mode ini | `/Script/MountHope.MountHopeGameMode` | `/Script/MountHope.MHGameModeBase` |
| Default game instance ini | `/Script/MountHope.MountHopeGameInstance` | `/Script/MountHope.MHGameInstance` |
| Save slot default | `QuahogUnrealSlot` | `MountHopeSlot` (`MHGameInstance.h`) |
| Source layout | `Public/` and `Private/` | headers beside `.cpp` files |

Both modules use the same module name `MountHope`. Putting both into one
target was not tried. A rename is not part of this change.

## World slice

Shared struct names on both trees: `FMHPolyline2D`, `FMHRoadRecord`,
`FMHBuildingRecord`, `FMHLandmarkRecord`, and `UMHWorldSliceSubsystem` with
`LoadSliceFromJson` and `IsSliceLoaded`.

Local to this tree after the slice fix:

- `FMHLandmarkRecord::Kind`
- `FMHWaterPolygon` and `UMHWorldSliceSubsystem::Water`
- 2-component `landmarks[].pos` lifted by `Slice2DToWorld`

`MountHope_Unreal/Source/MountHope/MHWorldSliceSubsystem.cpp`, as read, still
assigns landmark location only when `pos` has at least three numbers, and it
calls `GetStringField` for road `name` and `GetBoolField` for `hero`. The
committed `QUAHOG_Web/public/slice-newbedford.json` has 2-component landmark
positions, null road names, and `hero` omitted on most landmarks. That file
was parsed by `Tools/slice_contract.py`. The MountHope loader was not executed
against it.

Both game instances default `SlicePath` to
`../QUAHOG_Web/public/slice-newbedford.json`. That is a shared path string in
source, not a shared running load.

Overlay files next to the slice (`islands-`, `barrier-`, `parks-`, `parking-`,
`pier-`, `rail-`, `beach-`, `cemetery-`, `wood-`, `church-`) are fetched by
`QUAHOG_Web/src/slice.ts`. Neither Unreal loader reads them. This track does
not add that fetch.

`buildings` is an empty array and `buildingsStreamed` is true. No building
sidecar is loaded here.

Road and water points stay in slice 2D (east, north). Only landmark `Location`
is lifted to world metres. Mission `target` values are already world triples.
A later pass can decide whether road splines should use the same lift. That
conversion is not implemented on either vehicle or road mesh path in this tree.

## Missions and economy

Both trees have `UMHMissionSubsystem` with `LoadMissionsFromJson`,
`ResetCampaign`, `AdvanceStep`, `GetCurrentMission`, and `GetCurrentStep`.

The MountHope header also has `RestartCurrentMission`, `IsMissionInProgress`,
completion and failure delegates, and step fields this tree does not have
(`ObjectiveType`, `TimeLimitSeconds`, `bIsCrime`, `CrimeSeverity`,
`ReputationFactionTag`, `ReputationDelta`, `bRequireNoHeat`, `WeatherOnStart`,
plus mission `Act`, `CompletionReward`, `CompletionMessage`). Those were not
ported.

`UMHGameStateSubsystem` on both trees exposes `AddHeat`, `DecayHeat`, `AddCash`,
`ApplyDamage`, `CycleWeather`, `BuyBusiness`, `GetPassiveDailyIncome`,
`LoadBusinessesFromJson`, `SaveToSlot`, and `LoadFromSlot`. The MountHope
header also has armor, safehouse, heal, busted, and weather delegates. Those
were not ported.

`UMHSaveGame` on this tree stores cash, health, both heat values, weather, and
owned business ids. It does not store mission index, step index, armor,
safehouse, collectibles, or reputation. The MountHope save header, as read,
stores armor, safehouse, collectible ids, and reputation, and also does not
store mission index. Mission progress remaining out of the save is a shared
gap in the files that were read, not a tested runtime result.

## Suggested order

Work that stays on this track until the other tree opts in:

1. Keep the save slot `QuahogUnrealSlot` so a local save from this uproject does not use the other track's slot name.
2. Keep `Kind` and water polygons local. Moving them across is a `MountHope_Unreal` edit, which this track does not make.
3. When an Unreal 5.8 editor is available, compile this uproject alone and PIE the Bethel trigger against the lifted landmark. That compile has not been run.
4. Author Enhanced Input assets inside this project, or document a manual setup, before claiming on-foot controls. Assets were not created.
5. Add mission index and step index to `UMHSaveGame` only together with an editor round-trip. Not done.
6. Leave overlay JSON and streamed buildings unloaded until a reader exists and a sample file is checked by the data contract.
7. Treat a future shared slice contract as a written rule both loaders implement, then re-read both cpp files. Do not describe that as done from this document.

## Content limits carried into alignment

No character likeness models, photo mode, Cesium, or Google Photorealistic 3D
Tiles are part of this alignment. Place satire stays in the existing mission
strings. The OSM slice is referenced in place under `QUAHOG_Web/public/`.
