# Engine tracks

The Narrows is developed as **active parallel engine tracks**. They share the
South Coast setting, the OSM-grounded map idea, and the original crime-game
fiction. No track in this file is a retired reference copy.

This note does not claim that every track has been compiled or played. The
cloud sandbox used for this work can run the browser project. It does not
include Unreal Editor, Unity Editor, or Godot Editor, so those tracks were not
built or launched here.

## Tracks

| Track | Engine | In this repo | Not shown by this sandbox |
| --- | --- | --- | --- |
| `QUAHOG_Web/` | Three.js / React Three Fiber + Rapier | Browser game. Vite build is the typecheck. OSM New Bedford slice is committed. | A fresh production deploy was not run for this note. The recorded public URL is https://projectsouthcoast.vercel.app. |
| `MountHope_Unreal/` | Unreal Engine 5.8 | PC/console vertical slice: C++, JSON campaign, Python scaffold checks (`validate_scaffold.py`, `check_cpp.py`). | UBT compile, cook, and the UE automation tests were not run. |
| `QUAHOG_Unreal/` | Unreal Engine 5.8 | Parallel Unreal project. Earlier bootstrap whose mission/economy/world-slice patterns also live in `MountHope_Unreal/`. See [`QUAHOG_Unreal/TRACK.md`](QUAHOG_Unreal/TRACK.md). | No Unreal compile was run. |
| `QUAHOG_Unity/` | Unity 6 (`6000.0.23f1`) | Parallel Unity track, including the WebGL slice from PR #18. See [`QUAHOG_Unity/TRACK.md`](QUAHOG_Unity/TRACK.md). | Unity Editor / Build Automation was not run. `tools/csharp/` can headless-compile gameplay scripts; that was not run for this note. |
| `QUAHOG_Godot/` | Godot 4 (GDScript) | Parallel Godot port. Setup: `QUAHOG_Godot/GODOT_SETUP.md`. See [`QUAHOG_Godot/TRACK.md`](QUAHOG_Godot/TRACK.md). | Godot was not launched. |
| `QUAHOG_GODOT1/` | Godot 4.6 (`project.godot`, features `4.6`) | Parallel Godot project named **The Narrows** (`res://scenes/main.tscn`). | Godot was not launched. |

`QUAHOG_Unreal/LEGACY.md`, `QUAHOG_Unity/LEGACY.md`, and `QUAHOG_Godot/LEGACY.md`
only redirect to each track's `TRACK.md`, so older links still resolve.

## History worth keeping

- Street-life pedestrians and arcade car-feel notes from the Unity slice were
  also brought into `QUAHOG_Web/` (`StreetLife.tsx` and the web car tuning).
  The Unity project remains an active track.
- `QUAHOG_Unreal/` is where the JSON-driven Unreal mission, economy, and world
  slice were first sketched. `MountHope_Unreal/` continues that PC/console slice
  with the systems described in `MountHope_Unreal/Docs/VERTICAL_SLICE.md`.
- `quahog-project-files/mapdata/` is the shared OSM pipeline (GeoJSON, OBJ, and
  the web slice). `MountHope_Unreal/Scripts/prepare_osm_import.py` only reads
  those artifacts and writes an Unreal-local import manifest. `tools/mapgen/`
  remains the older Unity-oriented fetcher.

## What to run where

- Browser loop: `QUAHOG_Web/` (`npm run dev`, `npm run build`). See `QUAHOG_Web/README.md`.
- Unreal slice checks that do not need the editor: `python3 MountHope_Unreal/Scripts/validate_scaffold.py` and `python3 MountHope_Unreal/Scripts/check_cpp.py`.
- Unreal, Unity, and Godot playthroughs need the matching editor on a machine that has it.
