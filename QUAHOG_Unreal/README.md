# QUAHOG_Unreal — active parallel UE5 track

This directory is an active Unreal Engine **5.8** C++ track for The Narrows.
It stays separate from `MountHope_Unreal/`. It does not replace `QUAHOG_Web/`,
which remains the runnable browser game.

`LEGACY.md` in this folder and the repository `ENGINES.md` are updated by the
MountHope track. They are unchanged in work that only touches `QUAHOG_Unreal/`.
This README is the development status for this directory.

## What is in this tree

- `MountHope.uproject` — module name `MountHope`, engine association `5.8`
- Game instance, game mode, character, and a wheeled-vehicle pawn shell
- JSON loaders for the world slice, missions, and businesses
- Save/load of cash, health, heat, weather, and owned businesses
- Data under `Data/Missions` and `Data/Economy`

Boot paths and the save slot live in `Config/DefaultGame.ini` under
`[/Script/MountHope.MountHopeGameInstance]`. The slot name is `QuahogUnrealSlot`.

The slice path is `../QUAHOG_Web/public/slice-newbedford.json`. Landmark
positions in that file are 2D east/north metres. The loader lifts them to
world space with X = east, Y = up, Z = -north, which matches
`QUAHOG_Web/src/slice.ts` (`toWorld`) and the Seamen's Bethel mission target.
Unnamed roads (`"name": null`) are kept. Water rings are stored in slice-local
2D. The buildings array in that file is empty because `buildingsStreamed` is
true; this track does not load a building sidecar.

## Checked here

From the repository root:

```bash
python3 QUAHOG_Unreal/Tools/validate_bootstrap.py
python3 QUAHOG_Unreal/Tools/check_cpp.py
python3 QUAHOG_Unreal/Tools/test_data_contract.py
```

`validate_bootstrap.py` checks the uproject, target rules, mission and economy
JSON, config keys, and the data contract. `test_data_contract.py` is that data
contract on its own. `check_cpp.py` checks C++ text structure (braces,
`*.generated.h` include order, `GENERATED_BODY`, module names in `Build.cs`,
dynamic-delegate `UFUNCTION`s).

Details and the latest counts are in `Docs/TRACK_STATUS.md`.

## Unrun

Unreal Editor, UnrealHeaderTool, UnrealBuildTool, the editor automation
tests, PIE, cooking, and packaging were not run for this tree. There is no
Unreal install in the sandbox that produced these changes. A green Python
result is not an engine build and not a playable verification.

Enhanced Input assets (`IA_Move`, `IA_Look`, `IA_Sprint`, `IA_Interact`, and
the mapping context) are unset pointers. No input asset was authored.

## Alignment

`Docs/ALIGNMENT.md` is a source reading of the two Unreal trees and a list of
follow-ups. It does not claim the modules link, share a save, or behave the
same in PIE. Do not copy `MountHope_Unreal` systems into this directory as a
merge. The other tree owns its own code.

## Content limits

This track does not add character likeness models, photo mode, Cesium, or
Google Photorealistic 3D Tiles. Mission and economy JSON text is unchanged.
The OSM slice stays in `QUAHOG_Web/public/` and is only read by path. Map data
remains © OpenStreetMap contributors, ODbL.
