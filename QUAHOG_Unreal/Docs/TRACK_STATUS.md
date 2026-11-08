# QUAHOG_Unreal development status

This file describes the `QUAHOG_Unreal/` tree only.

## Active development

The C++ module, `Config/DefaultGame.ini`, the JSON data, and the Python checks
under `Tools/` are the development surface. Class and file layout stay in this
directory. `MountHope_Unreal/` is a separate track and is not edited from here.

`LEGACY.md` and the repository `ENGINES.md` are owned by the MountHope track.
They are not the status source for work inside this directory. Use `README.md`
and this file.

## What the loader accepts

Rules implemented in `Source/MountHope/Private/MHWorldSliceSubsystem.cpp` and
mirrored in `Tools/slice_contract.py`:

| JSON | Acceptance |
| --- | --- |
| `name` | Required non-empty string. |
| `origin.lat`, `origin.lon` | Optional numbers. |
| `roads[]` | Kept when `points` is an array of at least two `[east, north]` pairs. `name` and `highway` may be null or missing; null becomes an empty string. `width` defaults to 7. |
| `buildings[]` | Kept when `footprint` has at least two points. |
| `landmarks[]` | Kept when `name` is a non-empty string. `pos` of length 2 is lifted with `Slice2DToWorld`. `pos` of length 3 or more is stored as X, Y, Z. Missing `hero` is false. `kind` is optional. |
| `water[]` | Each ring with at least three `[east, north]` points is stored on `FMHWaterPolygon` in slice-local 2D. |
| `buildingsStreamed: true` with an empty `buildings` array | Logged. No sidecar file is opened. |

`Slice2DToWorld` is `FVector(EastMeters, 0, -NorthMeters)`.

Seamen's Bethel in the committed slice is `[-272.23, -106.5]`. The lift is
`[-272.23, 0, 106.5]`, the same triple as step 1 of `Data/Missions/vertical_slice.json`
("Meet Deacon Mealy at Seamen's Bethel").

## Save slot

`UMountHopeGameInstance::SaveSlotName` and the `SaveToSlot` / `LoadFromSlot`
default arguments are `QuahogUnrealSlot`. `AMountHopeGameMode` writes the
instance slot after an objective advance. The value is also set in
`Config/DefaultGame.ini`.

Campaign mission index and step index are not fields on `UMHSaveGame`. A load
restores player and economy fields only. `LoadMissionsFromJson` still calls
`ResetCampaign()`. That gap is recorded in `ALIGNMENT.md` and is left unfixed
until a save can be round-tripped in the editor.

## Checked without the engine

Commands, run from the repository root:

```bash
python3 QUAHOG_Unreal/Tools/validate_bootstrap.py
python3 QUAHOG_Unreal/Tools/check_cpp.py
python3 QUAHOG_Unreal/Tools/test_data_contract.py
```

`validate_bootstrap.py` covers:

- `MountHope.uproject` module `MountHope`, plugins EnhancedInput, ChaosVehiclesPlugin, WorldPartitionHLODUtilities, `EngineAssociation` `5.8`
- Target rules use `DefaultBuildSettings` and `IncludeOrderVersion`, and do not pin `EngineIncludeOrderVersion.Unreal5_4`
- No bare `KINDA_SMALL_NUMBER` in `Source/**/*.cpp`
- Expected framework headers and sources exist
- Mission and economy JSON shape
- `DefaultGame.ini` game instance class, game mode class, slice path, and `QuahogUnrealSlot`
- Game mode calls `ResolveSaveSlotName`
- Loader source contains `Slice2DToWorld`, `-NorthMeters`, `TryGetStringField`, `buildingsStreamed`, and `FMHWaterPolygon`
- The data contract below

`test_data_contract.py` reads `QUAHOG_Web/public/slice-newbedford.json` and the
mission file. On the file present when this document was written, the contract
reported:

- slice name `SouthCoast — New Bedford · Fall River · the towns`
- origin latitude `41.636`, longitude `-70.9205`
- 16167 roads kept, including unnamed roads (10977 with null `name`)
- 127 landmarks kept, all with 2-component `pos`
- 5 `hero: true` landmarks, including Seamen's Bethel
- Bethel world position matched the mission target
- 197 water rings kept
- `buildings` length 0 and `buildingsStreamed` true

Re-run the script for the current counts. The numbers above are the result of
that run, not a guess about a future slice.

`check_cpp.py` walks `Source/MountHope/**/*.h` and `*.cpp`. It does not invoke
a compiler. The run that wrote this note reported a pass over 20 files.

## Unrun engine work

The following were not executed for `QUAHOG_Unreal/`:

- Unreal Editor open of `MountHope.uproject`
- UnrealHeaderTool / `*.generated.h` generation
- UnrealBuildTool compile of `MountHope` or `MountHopeEditor`
- Session Frontend or `-ExecCmds="Automation RunTests ..."`
- PIE walk, drive, mission trigger, or save/load
- Cook and package

`Config/DefaultGame.ini` was edited as text. The editor did not load it, so
config-class registration is unverified in a running engine.

No deploy, account, or spend change was made.
