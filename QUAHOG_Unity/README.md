# QUAHOG Unity

Active parallel track for The Narrows. Unity 6 (`6000.0.23f1`), C#, built-in
render pipeline unless someone enables URP in an editor. The playable slice is
an on-foot character, an arcade car (`E` or `F` to enter), kinematic street
life, and a low-poly district built from the shared OpenStreetMap products.

This directory is the Unity project. Open it (not the repo root) in Unity Hub.
The project subdirectory for a cloud build is `QUAHOG_Unity`.

## What is developed here

- `Assets/Scripts/Player/` — third-person walk, run, hop. WASD works even when
  the legacy Input Manager axes are missing.
- `Assets/Scripts/Vehicles/CarController.cs` — arcade rigidbody car, handbrake
  on Space, visual wheel spin.
- `Assets/Scripts/World/StreetLife.cs` — kinematic pedestrians and patrol cars.
  On the OSM clip they follow real centre-lines. The greybox keeps its old box.
- `Assets/Scripts/World/Gis/` — map contracts, GeoJSON, ear clipping, city mesh.
- The district the bootstrap loads is `Assets/Resources/gis/waterfront.json`.
  `Assets/Resources/gis/newbedford.json` is a hand-authored fallback grid, not
  an Overpass extract.

Map provenance, axis conventions, and how to regenerate the clip:
[`OSM_CONTRACT.md`](OSM_CONTRACT.md).

WebGL dispatch and the license secret: [`WEBGL.md`](WEBGL.md).
Build Automation layout: [`BUILD_AUTOMATION.md`](BUILD_AUTOMATION.md).

## Checked in this sandbox

From the repo root, after the .NET 8 SDK is available:

```bash
tools/csharp/compile-check.sh
tools/csharp/run-tests.sh
```

`compile-check.sh` type-checks `Assets/Scripts` against a `UnityEngine` shim
(C# 9). `run-tests.sh` runs the logic tests: GeoJSON, ear clipping, the
committed web slice, `southcoast-roads.json`, and the proof that
`waterfront.json` is a clip of those files rather than a new drawing.

That gate does not run the Unity editor, a WebGL player, physics, rendering,
or input. A green script result is not a play-mode result.

## Not checked here

- Unity 6000.0.23f1 import, Play Mode, lighting, or URP
- GameCI / the WebGL player (`workflow_dispatch` only; not dispatched by the
  slice work)
- Android, iOS, or a device build
- A fresh Overpass download (`tools/mapgen/` was not run)

Controls once a player is actually running: click the game view, WASD to walk,
Left Shift to run, Space to hop, `E` or `F` near the car to drive, Space to
handbrake while driving.
