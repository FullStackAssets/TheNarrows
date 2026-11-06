# OSM to Unreal Import Notes

The existing web game and map pipeline already contain usable source data for
an Unreal graybox. The first Unreal pass should consume those artifacts before
adding a new city.

## Available inputs

| Source | Use in Unreal |
| --- | --- |
| `../quahog-project-files/mapdata/southcoast.obj` | Initial static mesh for roads and water across New Bedford/Fall River. |
| `../quahog-project-files/mapdata/southcoast-roads.json` | Runtime/editor road centerlines for traffic, pedestrians, navigation annotations, and future spline roads. |
| `../QUAHOG_Web/public/slice-newbedford.json` | Current web vertical-slice blockout with roads, building footprints, landmarks, and local metric coordinates. |
| `../quahog-project-files/mapdata/*.geojson` | Source GIS layers for re-baking custom Unreal meshes or adding metadata. |

## Prepared import contract

`Scripts/prepare_osm_import.py` reads the committed slice and
`southcoast-roads.json` and writes `Data/OSM/import_manifest.json`. It does
not modify the shared map pipeline and it does not change either importer's
coordinate math. `Scripts/validate_scaffold.py` runs the script with `--check`
so the manifest cannot drift from those inputs.

The manifest records two conversions that already exist in the editor scripts.
They do not share a ground plane:

| Contract | Unreal centimeters | Used by |
| --- | --- | --- |
| Slice importer | `X = east * 100`, `Y = -north * 100`, `Z = up * 100` | `Scripts/editor_import_osm.py` |
| Mission plane | `X = east * 100`, `Y = up * 100`, `Z = north * 100` | `Scripts/editor_import_southcoast_roads.py` and mission `target` arrays |

`editor_import_osm.py` reads `playableDistrict.radiusMeters` from the manifest
(400 m from the slice origin) when deciding which debug road splines to spawn.
The OBJ blockout import is unchanged. If the manifest is missing, the script
falls back to its own radius constant.

Regenerate the manifest after a slice or southcoast-roads refresh:

```bash
python3 MountHope_Unreal/Scripts/prepare_osm_import.py
python3 MountHope_Unreal/Scripts/prepare_osm_import.py --check
```

## Coordinate conversion

The baked map data is already in local meters:

- `x` = east
- `z` = north in the mapdata pipeline
- `y` = up

Unreal uses centimeters:

- Unreal `X` = source `x * 100`
- Unreal `Y` = source `z * 100`
- Unreal `Z` = source `y * 100`

The web slice uses `z = -north`; confirm the source artifact before import and
flip the Unreal `Y` axis only for data that came directly from the web slice.

## First import path

### Automated (recommended)

With the editor open on `MH_VerticalSlice`, run:

```text
Tools → Execute Python Script → Scripts/editor_import_osm.py
```

This imports `southcoast.obj` to `/Game/OSM/SM_SouthCoast_Blockout`, places it
at the origin with 100× scale, and spawns debug splines for slice roads near the
playable district.

Tune `MAX_ROAD_SPLINES` and `SLICE_ROAD_BBOX_METERS` at the top of the script.

### Manual

1. Import `southcoast.obj` into `/Game/OSM/SouthCoast`.
2. Keep scale at `1.0` if the OBJ importer treats units as centimeters; use
   `100.0` only if the mesh appears meter-scaled in Unreal.
3. Split roads and water into separate materials.
4. Create collision as simple complex collision only for the graybox pass.
5. Add a World Partition Open World map at `/Game/Maps/MH_VerticalSlice`.
6. Place the imported road/water mesh at the world origin.
7. Add a small authored playable district volume instead of trying to polish
   the entire city at once.

## Next import path

`Scripts/editor_import_osm.py` covers the first graybox pass. After the mesh is
visible, extend with an Editor Utility that:

- reads `southcoast-roads.json` (full South Coast, 12k+ roads);
- creates road splines per OSM road;
- assigns road width by `highway` class;
- adds traffic lane metadata;
- tags mission-relevant roads and parking spots;
- exports simplified navigation blockers for sidewalks, water, and buildings.

The earlier plan to write this as a one-off Python script or C++ commandlet
remains the next step once the slice splines prove the coordinate conversion.

`Scripts/editor_import_southcoast_roads.py` imports a **bounded** subset from
`southcoast-roads.json` (default: 900 m radius around the waterfront mission
anchor). Increase `MAX_ROAD_SPLINES` / `RADIUS_M` as needed.

## Brockton expansion

Brockton is a good candidate for the next real-world anchor if the game needs a
more inland crime/satire identity than the waterfront. Add it by extending the
existing Overpass/GeoJSON pipeline first, then import the same road JSON and
mesh formats into Unreal.

Do not hand-build Brockton before the automated import path works; otherwise the
project loses the main advantage of keeping OSM canonical.
