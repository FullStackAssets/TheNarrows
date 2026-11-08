# QUAHOG_Godot

Godot 4 GDScript track for **The Narrows** (South Coast, 2026). This directory is in **active development**. It is **not a production build**, and it does not replace `QUAHOG_Web/`.

Repo-level status files are updated separately:

- `ENGINES.md`
- `QUAHOG_Godot/LEGACY.md` and the other `LEGACY.md` files

Those files are owned by the MountHope agent. Until that update lands, they may still call this track legacy. This README is the Godot track's own status.

## What runs

`scenes/world/Game.tscn` builds the slice in code:

- OSM road ribbons from `data/southcoast-roads.json` (a byte copy of the shared bake; see `OSM_CONTRACT.md`)
- on-foot player, dusk lighting, weather particles, highway neon
- street banner from the road's OSM `name` and `highway` tag
- one contact on that road which completes mission `ns_01_intro`

Parody district names in `MissionManager` (New Sefton and the rest) are mission flavor. They are not labels for OSM cities. The current road graph stores `city: null` on every road.

## Docs

- [`GODOT_SETUP.md`](GODOT_SETUP.md) — open the project in Godot 4.2+
- [`EXPORT.md`](EXPORT.md) — export steps and the gates that are still closed
- [`OSM_CONTRACT.md`](OSM_CONTRACT.md) — shared map contract and the mismatches this adapter does not paper over

## Checks that can run here

```sh
python3 -m unittest QUAHOG_Godot.tests.test_osm_contract
```

That suite reads the committed JSON and checks the GDScript tables against `tools/osm_contract.py`. It does not start Godot and does not pull new OSM data.

A separate headless Godot 4.2.2 dummy-renderer boot of `Game.tscn` is recorded in [`EXPORT.md`](EXPORT.md). It is not a windowed playtest and not an export.

Map data © OpenStreetMap contributors, ODbL.
