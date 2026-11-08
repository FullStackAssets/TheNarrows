# Unity map contract

The Unity district is a clip of files already committed for the other engines.
Nothing in this track calls Overpass, and `Assets/Resources/gis/newbedford.json`
is not that clip. It is a small hand-authored grid kept so a build still has a
block if the clip is missing.

## Sources

| Product | Path | Writer |
| --- | --- | --- |
| Web slice | `QUAHOG_Web/public/slice-newbedford.json` | `quahog-project-files/mapdata/make_slice.py` |
| Building tiles | `QUAHOG_Web/public/tiles/b_<gx>_<gn>.json` | tile stream. Manifest `tiles/manifest.json` (`tile` = 500 m, origin lat `41.636`, lon `-70.9205`) |
| Road graph | `quahog-project-files/mapdata/southcoast-roads.json` | `quahog-project-files/mapdata/bake_meshes.py` |
| Unity clip | `QUAHOG_Unity/Assets/Resources/gis/waterfront.json` | `QUAHOG_Unity/tools/extract_playable_slice.py` |

The slice header is the `make_slice.py` frame: origin lat `41.636`, lon
`-70.9205`, `meters_per_degree.lat` = `111320`, longitude scale
`111320 * cos(origin lat)`. `buildingsStreamed` is true, so the slice's
`buildings` array is empty. Footprints are the tile files. The slice was
produced from OSM; this track does not re-fetch it.

`southcoast-roads.json` is a second contract (both cities, its own origin).
The Unity clip does **not** mix it into the waterfront mesh, because that file
uses a different origin. The C# parser still reads it, and the logic tests
check `road_count`, the axis string, and the first published point (Copicut
Road). The slice has no `oneway` field (`make_slice.py` does not copy it). The
road graph does.

Attribution on both products is `© OpenStreetMap contributors, ODbL`.

## Axes

2D positions in the slice, the tiles, and the Unity clip are `[east, north]`
metres about the slice origin.

`bake_meshes.py` stores the same pair as `[x, z]` with `z = north`.

Unity world: `x = east`, `y = up`, `z = north`.

Three.js (`QUAHOG_Web/src/slice.ts`, `toWorld`) uses `z = -north` for the same
2D point. The Unity mesh does not apply that flip.

## The clip

`extract_playable_slice.py` keeps a 400 m half-extent square centred on the
slice origin (east 0, north 0).

- Road polylines are cut to the square. A vertex copied from the slice stays as
  published. A vertex that exists only because an edge crosses `x = ±400` or
  `north = ±400` is that intersection, rounded to centimetres.
- The harbour ring is cut with the same square. If the source polygon covers
  a corner of the square, that corner is added so the clipped ring stays
  closed. It is not an OSM node. The other harbour vertices lie on the source
  ring.
- A building is copied whole, footprint and height, when its centroid is inside
  the square. `sourceTile` and `sourceIndex` point at `b_<tile>.json`.
- Landmarks inside the square are copied, including the five hero names
  (Seamen's Bethel, the Whaling Museum, Mariner's Home, Double Bank Building,
  Rodman Candleworks).

Regenerate from the repo root after the slice or tiles change:

```bash
python3 QUAHOG_Unity/tools/extract_playable_slice.py
```

`tools/csharp/run-tests.sh` then checks that every clip road and harbour vertex
lies on its source polyline, that building footprints match the tile records,
and that landmark positions match the slice.

## Runtime

`GisCity` parses whatever text it is given:

- `unity-playable-clip` / `web-slice` — planar `[east, north]`
- `southcoast-roads` — planar `[east, north]` from `[x, z]`
- `geojson` — lon/lat, equirectangular about that file's own bbox centre

`GameBootstrap` loads `Resources/gis/waterfront`, then the hand-authored grid,
then the greybox town. Street life follows drivable centre-lines
(`motorway` through `service`, including `*_link`). Footways stay out of the
traffic routes. The player stands on the drivable point nearest Seamen's Bethel
when that landmark is in the document.
