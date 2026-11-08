# Godot export

Active development track. **No export has been run for this revival**, and none is claimed. There is no `export_presets.cfg` because a preset would look like a finished target that has not been built or launched.

## Editor

- Godot **4.2** or newer, **standard** build (GDScript). Not the .NET build.
- Project features in `project.godot`: `4.2`, `Forward Plus`.
- Renderer: Forward+. A web export cannot use Forward+.

Open `QUAHOG_Godot/` and press Play on `res://scenes/world/Game.tscn`. Boot lines the scripts print:

- `[MapMeshBaker] baked … roads … frame=road_graph`
- `[OsmMapContract]` warning that `city` is empty on every road
- `[GameWorld] Ready — player spawned at …`
- `[MissionManager] started 'Welcome to New Sefton'`

A **headless** Godot 4.2.2 official binary (dummy renderer, `--quit-after 15`) printed those lines: 12,257 roads, 472,920 vertices, spawn `(815.09, 3, -5638.73)`, intro mission started, starting cash `$500`. That is not a windowed editor pass, not a desktop GPU check, and not a device launch. The same run logged `mesh_get_surface_count` null inside `servers/rendering/dummy/` with no script stack. Do not read that as a rendered frame.

`project.godot` still points at `res://icon.svg`. Only `icon.svg.import` is in the tree, so headless logs `Error opening file 'res://icon.svg'`. The source SVG was not added here.

## Desktop preset (when you have the editor)

1. Download export templates that match the editor version (Editor → Manage Export Templates).
2. Project → Export → Add… → Windows, Linux, or macOS.
3. Leave the renderer on Forward+ for desktop.
4. Export a build. Launch it. Walk, look, press E on the contact, press Esc to pause.

This repo does not contain those templates. Do not commit `build/` or `export/` (see `.gitignore`).

## Gates still closed

| Gate | Why it is still open |
| --- | --- |
| Editor import / Play in a window | Headless 4.2.2 import and a 15-frame boot ran. No window, no screenshot, no input |
| Desktop export | No templates, no preset, no packaged binary |
| Windows / macOS / Linux device pass | No device launch |
| Web export | Forward+ is the project renderer. Web needs Compatibility (or Mobile, where supported). Switching the project renderer without a Play test would change the desktop look unseen |
| Mobile export | No preset, no device |
| Controller pass | Input map has gamepad bindings; no pad was connected |
| Building tiles | `slice-newbedford.json` has `buildings: []` and `buildingsStreamed: true`. Tiles live under `QUAHOG_Web/public/tiles/` and are **not** copied into this project, so an export will not contain them |
| Slice water / landmarks | Different origin and axis from the road graph. Not merged into the playable mesh |
| Photo mode | Not added. Held: do not ship a photo-mode capture path or new photoreal stills from this track |
| Google photoreal tiles | Not part of this track. Do not vendor 3D Tiles or imagery keys |
| Production readiness | Active status means the GDScript track is being worked again. It is not a ship candidate |

## What an export must include

- `data/southcoast-roads.json` (already inside the project). It is the shared bake, not a new pull.
- The OpenStreetMap / ODbL credit. The JSON `attribution` field is `© OpenStreetMap contributors, ODbL`. Keep that credit on any build that ships the mesh.
- No new Overpass dump. If the shared bake changes, copy the file in and re-run the contract tests. Do not invent coordinates to fill gaps.

## `QUAHOG_GODOT1`

That tree is a different Godot project (web export scripts, its own scenes). This track does not reuse its templates or its Vercel packaging.
