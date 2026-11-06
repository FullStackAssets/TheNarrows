# OSM contract for the Godot track

The Godot slice reads the **committed** shared bake. It does not call Overpass, and it does not run `bake_meshes.py`, `make_slice.py`, or `make_tiles.py` (those scripts write the shared files). Figures below were read from the files in this repo. If a file changes, `python3 -m unittest QUAHOG_Godot.tests.test_osm_contract` is the check that should fail first.

`data/southcoast-roads.json` is a byte copy of `quahog-project-files/mapdata/southcoast-roads.json`. The test compares the two hashes. Refresh it by copying that file. Do not hand-edit coordinates.

## Road graph (`bake_meshes.py`)

Top-level keys: `origin`, `meters_per_degree`, `axes`, `attribution`, `road_count`, `roads`.

Each road: `name`, `highway`, `city`, `width`, `oneway`, `points`.

| Field | How Godot uses it |
| --- | --- |
| `points` | `[east, north]` meters. Godot `x = east`, `z = north`, `y = 0` |
| `axes` | Published as `x=east(m), z=north(m), y=up` |
| `width` | Full width in meters. The ribbon uses half of the **stored** number |
| `city` | Shown on the street banner **only when it is a non-empty string** |
| `name` / `highway` | Banner and neon color. A null name falls back to the highway tag |
| `origin`, `meters_per_degree` | Inverse of the bake projection, used only when converting from the slice frame |

`attribution` is `© OpenStreetMap contributors, ODbL`.

Current bake (not a new pull):

- `road_count` matches `len(roads)` (12,257)
- `origin` is about lat `41.74524735`, lon `-71.0503516`
- `meters_per_degree.lat` is `111320`, `lon` is about `83057.255`
- `city` is JSON `null` on **every** road
- no road has `bridge`

`southcoast.obj` comments the same origin rounded to six decimals (`%.6f` in `bake_meshes.py`). That is display rounding, not a second origin. Water in the OBJ is dropped to `y = -0.2`. The road-graph JSON has no water.

## Slice (`make_slice.py`, then `make_tiles.py`)

File: `QUAHOG_Web/public/slice-newbedford.json`. Not copied into this Godot project.

Top-level keys include `name`, `origin`, `meters_per_degree`, `axes`, `note`, `attribution`, `buildings`, `roads`, `water`, `landmarks`, `buildingsStreamed`.

| Field | Published meaning | Godot |
| --- | --- | --- |
| Road / water / footprint points | 2D `[east, north]` | 3D `x = east`, `z = -north` (the slice `note`, same as the web `toWorld`) |
| `axes` | The string still says `z=south is +?` | Ambiguous. The `note` is the axis Godot follows |
| `origin` | lat `41.636`, lon `-70.9205` | **Not** the road-graph origin |
| `meters_per_degree.lon` | about `83198.429` | **Not** the road-graph longitude scale |
| `width` | Full width from `make_slice.py`'s table | Used as stored. Not rewritten to the bake table |
| `bridge`, `layer` | Present on some slice roads | **No deck height** is published. Ribbons stay at `y = 0` and a warning is logged |
| `buildings` | Empty array | See tiles below |
| `landmarks` | `name`, `kind`, `pos`, optional `hero` | Not placed in the playable world. Mixing them into the road-graph frame requires the WGS84 step below |

Slice roads do not have a `city` key at all.

## Building tiles (`make_tiles.py`)

`QUAHOG_Web/public/tiles/manifest.json`: `tile` 500, `origin` equal to the slice origin, `count` 68,482, one `b_<key>.json` per key.

Tile key is `floor(east / 500)_floor(north / 500)` using the slice's 2D north, **not** Godot `z`. Negative tiles need `floor`. GDScript `int()` truncates toward zero and would name `-50, -600` as `0_-1` instead of `-1_-2`.

`make_slice.py` does not write `buildingsStreamed`. `make_tiles.py` sets `buildings` to `[]` and `buildingsStreamed` to true after it writes the tiles. Godot does not import those tiles.

## Mismatches (left visible)

1. **City is empty.** `bake_meshes.py` copies `properties.city`. The committed `new-bedford.geojson` and `fall-river.geojson` highway features do not have `city`. `split_layers.py` would stamp a city from the filename, but `layers/` is not in the tree and the baker does not read it. Godot does **not** fill city from the filename, from a bounding box, or from a landmark.
2. **Two local frames.** A slice point has to go slice → lat/lon with the slice scale, then lat/lon → road graph with the graph scale. Those longitude scales differ by more than a meter per 0.01 degree. The adapter does not pretend they share one origin.
3. **Width tables differ.** Examples read from the two JSON files: residential 6 (graph) vs 7 (slice), service 4 vs 4.5, footway 2 vs 2.5, motorway_link 8 vs 6, unclassified 6 vs 7, track 3 vs 6. Shared classes that matched in this read include motorway 16, primary 12, secondary 10, tertiary 8. Godot uses the number on the feature it loaded.
4. **Bridges.** Slice has bridge flags (276 in this read). The road graph has none. No height is invented.
5. **Water height.** Only the OBJ baker uses `y = -0.2`. The slice water rings are 2D. The playable Godot world does not mesh them.
6. **Buildings.** The slice array is empty because they were streamed. This project does not vendor the tiles, so the export will not contain them.
7. **Spawn.** Older `GameWorld` looked for `city == "New Bedford"`, which matches nothing in the current file, then fell through to road 0. The spawn is now the first road with a non-empty `name` and two points (still road 0 while that road is named). The contact sits 40 m along that polyline, plus a 1.5 m prop offset off the centerline. That offset is not an OSM point.

## Neon

Highway colors in `scripts/world/HighwayNeon.gd` are lighting. They follow the `highway` tag. They are not district borders. Parody names in `MissionManager` and `CoastalNeonPostProcess` (New Sefton, Dighton, Taunton Hill, Sawyer) stay mission and grade enums. The street banner does not print them.

`Seamen's Bethel` is a hero landmark in the slice at the published point `[-272.23, -106.5]` (slice east/north). This track does not move that point.

Map data © OpenStreetMap contributors, ODbL.
