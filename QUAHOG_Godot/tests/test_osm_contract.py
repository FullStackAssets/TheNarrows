"""Adapter checks against the committed OSM artifacts.

These tests read the shared map files and the Godot copy. They do not call
Overpass, do not run bake_meshes.py / make_slice.py / make_tiles.py (those
scripts write outputs), and they do not start Godot.

Run from the repo root:

    python3 -m unittest QUAHOG_Godot.tests.test_osm_contract
"""

from __future__ import annotations

import hashlib
import json
import math
import re
import unittest
from pathlib import Path

from QUAHOG_Godot.tools.osm_contract import (
    CELL_M,
    CLEAR_AMBIENT,
    CONTACT_DISTANCE_M,
    DEFAULT_RGB,
    HIGHWAY_AMBIENT_MIX,
    HIGHWAY_RGB,
    NIGHT_EMISSION_MULTIPLIER,
    PROP_LATERAL_M,
    TILE_M,
    WEATHER_EMISSION,
    banner_for_road,
    bounds_xz,
    build_cell_index,
    building_tile_key,
    classify,
    clear_ambient,
    contact_pose,
    emission_energy,
    full_width,
    highway_rgb,
    nearest_road,
    pick_spawn_road,
    point_along,
    road_graph_point_to_godot,
    slice_point_to_godot,
    slice_point_to_road_graph_xz,
    slice_to_wgs84,
)

ROOT = Path(__file__).resolve().parents[2]
GODOT = ROOT / "QUAHOG_Godot"
MAP = ROOT / "quahog-project-files" / "mapdata"
WEB = ROOT / "QUAHOG_Web" / "public"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _load(path: Path):
    with path.open() as handle:
        return json.load(handle)


def _gd(name: str) -> str:
    return (GODOT / name).read_text()


def _parse_gd_array_dict(text: str, const_name: str) -> dict:
    match = re.search(rf"const {const_name} := \{{(.*?)\n\}}", text, re.S)
    if not match:
        raise AssertionError(f"missing {const_name}")
    found = {}
    for key, nums in re.findall(r'"([^"]+)": \[([^\]]+)\]', match.group(1)):
        found[key] = tuple(float(part.strip()) for part in nums.split(","))
    return found


def _parse_gd_float_dict(text: str, const_name: str) -> dict:
    match = re.search(rf"const {const_name} := \{{(.*?)\n\}}", text, re.S)
    if not match:
        raise AssertionError(f"missing {const_name}")
    return {key: float(value) for key, value in re.findall(r'"([^"]+)": ([0-9.]+)', match.group(1))}


class ContractMathTests(unittest.TestCase):
    def test_frames_are_not_the_same_axis(self):
        self.assertEqual(road_graph_point_to_godot(10, 4), (10, 0, 4))
        self.assertEqual(slice_point_to_godot(10, 4), (10, 0, -4))

    def test_point_along_walks_real_segments(self):
        points = [[0, 0], [30, 0], [30, 40]]
        self.assertEqual(point_along(points, 40), (30.0, 0.0, 10.0))
        short = point_along([[0, 0], [5, 0]], 40)
        self.assertEqual(short, (5.0, 0.0, 0.0))

    def test_contact_is_a_prop_offset_not_a_new_centerline(self):
        points = [[0, 0], [40, 0]]
        pose = contact_pose(points, 40)
        on_road = pose["on_road"]
        pos = pose["position"]
        self.assertAlmostEqual(math.dist((on_road[0], on_road[2]), (pos[0], pos[2])), PROP_LATERAL_M)
        self.assertEqual(on_road, (40.0, 0.0, 0.0))

    def test_long_segment_is_indexed_between_vertices(self):
        roads = [{"name": "Span", "highway": "residential", "points": [[0, 0], [200, 0]]}]
        cells = build_cell_index(roads, CELL_M)
        hit = nearest_road(roads, cells, 100, 2)
        self.assertIsNotNone(hit)
        self.assertEqual(hit["index"], 0)
        self.assertLess(hit["distance"], 3)
        self.assertIsNone(nearest_road(roads, cells, 100, 500))

    def test_negative_tile_key_uses_floor_not_truncation(self):
        self.assertEqual(building_tile_key(-50, -600, TILE_M), "-1_-2")
        # GDScript int() truncates toward zero and would publish the wrong key.
        self.assertEqual(f"{int(-50 / TILE_M)}_{int(-600 / TILE_M)}", "0_-1")

    def test_banner_does_not_invent_a_city(self):
        self.assertEqual(banner_for_road({"name": "Copicut Road", "highway": "residential", "city": None}), "Copicut Road")
        self.assertEqual(banner_for_road({"name": None, "highway": "service", "city": None}), "service")
        self.assertEqual(
            banner_for_road({"name": "Union Street", "highway": "secondary", "city": "New Bedford"}),
            "New Bedford · Union Street",
        )

    def test_pick_spawn_ignores_null_city(self):
        roads = [
            {"name": None, "city": None, "points": [[0, 0], [1, 0]]},
            {"name": "Named", "city": None, "points": [[2, 0], [3, 0]]},
        ]
        self.assertEqual(pick_spawn_road(roads), 1)
        self.assertIsNone(pick_spawn_road([{"name": "x", "points": [[0, 0]]}]))

    def test_width_is_taken_from_the_feature(self):
        self.assertEqual(full_width({"width": 7, "highway": "residential"}), 7.0)
        self.assertEqual(full_width({"width": None, "highway": "residential"}), 6.0)

    def test_link_tags_reuse_the_parent_neon(self):
        self.assertEqual(highway_rgb("motorway_link"), HIGHWAY_RGB["motorway"])
        self.assertEqual(highway_rgb("not_a_real_class"), DEFAULT_RGB)

    def test_night_emission_matches_the_post_process_window(self):
        self.assertAlmostEqual(emission_energy("CLEAR", 19), WEATHER_EMISSION["CLEAR"])
        self.assertAlmostEqual(emission_energy("CLEAR", 20), WEATHER_EMISSION["CLEAR"] * NIGHT_EMISSION_MULTIPLIER)
        self.assertAlmostEqual(emission_energy("CLEAR", 5), WEATHER_EMISSION["CLEAR"] * NIGHT_EMISSION_MULTIPLIER)
        mixed = clear_ambient("residential")
        neon = HIGHWAY_RGB["residential"]
        self.assertAlmostEqual(mixed[0], CLEAR_AMBIENT[0] * (1 - HIGHWAY_AMBIENT_MIX) + neon[0] * HIGHWAY_AMBIENT_MIX)


class GodotSourceSyncTests(unittest.TestCase):
    def test_palette_and_scalars_match_the_gdscript_tables(self):
        neon = _gd("scripts/world/HighwayNeon.gd")
        parsed = _parse_gd_array_dict(neon, "HIGHWAY_RGB")
        self.assertEqual(set(parsed), set(HIGHWAY_RGB))
        for key, rgb in HIGHWAY_RGB.items():
            self.assertEqual(len(parsed[key]), 3)
            for got, want in zip(parsed[key], rgb):
                self.assertAlmostEqual(got, want)
        default = re.search(r"const DEFAULT_RGB := \[([^\]]+)\]", neon)
        self.assertIsNotNone(default)
        got_default = tuple(float(part.strip()) for part in default.group(1).split(","))
        for got, want in zip(got_default, DEFAULT_RGB):
            self.assertAlmostEqual(got, want)
        self.assertEqual(set(_parse_gd_float_dict(neon, "WEATHER_EMISSION")), set(WEATHER_EMISSION))
        for key, value in WEATHER_EMISSION.items():
            self.assertAlmostEqual(_parse_gd_float_dict(neon, "WEATHER_EMISSION")[key], value)
        self.assertIn("hour >= 20 or hour < 6", neon)
        self.assertRegex(neon, rf"const NIGHT_EMISSION_MULTIPLIER := {NIGHT_EMISSION_MULTIPLIER:.2f}")
        self.assertRegex(neon, rf"const HIGHWAY_AMBIENT_MIX := {HIGHWAY_AMBIENT_MIX:.2f}")

    def test_geometry_helpers_keep_the_documented_axes(self):
        contract = _gd("scripts/world/OsmMapContract.gd")
        self.assertIn("return Vector3(east, y, north)", contract)
        self.assertIn("return Vector3(east, y, -north)", contract)
        self.assertIn("int(floor(east / tile_m))", contract)
        self.assertNotIn("int(east / tile_m)", contract)
        self.assertIn("ns_01_intro", _gd("scripts/world/StreetLoop.gd"))
        self.assertIn("ns_01_intro", _gd("autoloads/MissionManager.gd"))
        for rel in (
            "scripts/world/OsmMapContract.gd",
            "scripts/world/HighwayNeon.gd",
            "scripts/world/StreetLoop.gd",
            "scripts/world/GameWorld.gd",
            "scripts/world/MapMeshBaker.gd",
        ):
            text = _gd(rel)
            self.assertFalse(any(line.startswith("    ") for line in text.splitlines()), rel)


class LiveArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.graph_path = MAP / "southcoast-roads.json"
        cls.godot_graph_path = GODOT / "data" / "southcoast-roads.json"
        cls.slice_path = WEB / "slice-newbedford.json"
        cls.graph = _load(cls.graph_path)
        cls.slice = _load(cls.slice_path)
        cls.manifest = _load(WEB / "tiles" / "manifest.json")

    def test_godot_road_graph_is_the_shared_file(self):
        self.assertEqual(_sha256(self.graph_path), _sha256(self.godot_graph_path))
        self.assertEqual(classify(self.graph), "road_graph")
        self.assertEqual(classify(self.slice), "slice")

    def test_road_graph_contract_and_null_city(self):
        for key in ("origin", "meters_per_degree", "axes", "attribution", "road_count", "roads"):
            self.assertIn(key, self.graph)
        self.assertEqual(self.graph["road_count"], len(self.graph["roads"]))
        self.assertIn("z=north", self.graph["axes"])
        self.assertIn("OpenStreetMap", self.graph["attribution"])
        self.assertIn("ODbL", self.graph["attribution"])
        named_city = 0
        sample = self.graph["roads"][0]
        for key in ("name", "highway", "city", "width", "oneway", "points"):
            self.assertIn(key, sample)
        for road in self.graph["roads"]:
            if isinstance(road.get("city"), str) and road["city"].strip():
                named_city += 1
        self.assertEqual(
            named_city,
            0,
            "shared road graph gained city values; update OSM_CONTRACT.md and stop treating city as empty",
        )
        self.assertEqual(pick_spawn_road(self.graph["roads"]), 0)
        spawn = self.graph["roads"][0]
        self.assertTrue(isinstance(spawn.get("name"), str) and spawn["name"].strip())
        self.assertEqual(banner_for_road(spawn), spawn["name"])
        pose = contact_pose(spawn["points"], CONTACT_DISTANCE_M)
        self.assertIsNotNone(pose)
        origin = road_graph_point_to_godot(spawn["points"][0][0], spawn["points"][0][1])
        on_road = pose["on_road"]
        self.assertGreater(math.dist((origin[0], origin[2]), (on_road[0], on_road[2])), 5.0)

    def test_source_geojson_has_no_city_and_layers_were_not_baked(self):
        self.assertFalse((MAP / "layers").exists())
        for slug in ("new-bedford", "fall-river"):
            fc = _load(MAP / f"{slug}.geojson")
            highways = [f for f in fc["features"] if f.get("properties", {}).get("highway")]
            self.assertGreater(len(highways), 0)
            self.assertTrue(all("city" not in f["properties"] for f in highways))
        bake_src = (MAP / "bake_meshes.py").read_text()
        self.assertIn('"city": t.get("city")', bake_src)
        self.assertIn('"residential":6', bake_src.replace(" ", ""))

    def test_slice_contract_mismatches(self):
        for key in ("origin", "meters_per_degree", "axes", "note", "attribution", "buildings", "roads", "water", "landmarks"):
            self.assertIn(key, self.slice)
        self.assertIn("z=-y(north)", self.slice["note"])
        self.assertIn("z=south", self.slice["axes"])
        self.assertNotIn("city", self.slice["roads"][0])
        self.assertEqual(self.slice["buildings"], [])
        self.assertIs(self.slice.get("buildingsStreamed"), True)
        self.assertGreater(sum(1 for road in self.slice["roads"] if road.get("bridge")), 0)
        self.assertEqual(sum(1 for road in self.graph["roads"] if road.get("bridge")), 0)
        graph_residential = {road["width"] for road in self.graph["roads"] if road["highway"] == "residential"}
        slice_residential = {road["width"] for road in self.slice["roads"] if road["highway"] == "residential"}
        self.assertEqual(graph_residential, {6.0})
        self.assertEqual(slice_residential, {7})
        self.assertNotEqual(self.graph["origin"], self.slice["origin"])
        self.assertNotEqual(self.graph["meters_per_degree"]["lon"], self.slice["meters_per_degree"]["lon"])
        make_slice = (MAP / "make_slice.py").read_text()
        make_tiles = (MAP / "make_tiles.py").read_text()
        self.assertNotIn("buildingsStreamed", make_slice)
        self.assertIn('s["buildingsStreamed"] = True', make_tiles)
        self.assertIn("TILE = 500", make_tiles)
        self.assertIn('"residential":7', make_slice.replace(" ", ""))
        # OBJ comment rounds the road-graph origin to 6 decimals. Same origin.
        header = (MAP / "southcoast.obj").read_text().splitlines()[0]
        origin = self.graph["origin"]
        self.assertIn(f"lat={origin['lat']:.6f}", header)
        self.assertIn(f"lon={origin['lon']:.6f}", header)
        self.assertIn("-0.2", (MAP / "bake_meshes.py").read_text())

    def test_slice_to_graph_transform_uses_each_files_scale(self):
        lat, lon = slice_to_wgs84(0, 0, self.slice["origin"], self.slice["meters_per_degree"])
        self.assertAlmostEqual(lat, self.slice["origin"]["lat"])
        self.assertAlmostEqual(lon, self.slice["origin"]["lon"])
        moved = slice_point_to_road_graph_xz(0, 0, self.slice, self.graph)
        self.assertGreater(abs(moved[0]) + abs(moved[1]), 1000.0)
        degree = 0.01
        self.assertGreater(
            abs(degree * self.graph["meters_per_degree"]["lon"] - degree * self.slice["meters_per_degree"]["lon"]),
            1.0,
        )

    def test_building_tiles_match_the_manifest(self):
        self.assertEqual(self.manifest["tile"], 500)
        self.assertEqual(self.manifest["origin"], self.slice["origin"])
        self.assertGreater(self.manifest["count"], 0)
        tiles = WEB / "tiles"
        missing = [key for key in self.manifest["keys"] if not (tiles / f"b_{key}.json").is_file()]
        self.assertEqual(missing, [])
        for key in ("0_0", "-1_-2"):
            buildings = _load(tiles / f"b_{key}.json")
            self.assertGreater(len(buildings), 0)
            for building in buildings:
                ring = building["footprint"]
                east = sum(point[0] for point in ring) / len(ring)
                north = sum(point[1] for point in ring) / len(ring)
                self.assertEqual(building_tile_key(east, north, self.manifest["tile"]), key)
                self.assertIn("height", building)

    def test_hero_landmark_is_present_and_not_relocated(self):
        heroes = [lm["name"] for lm in self.slice["landmarks"] if lm.get("hero")]
        self.assertIn("Seamen's Bethel", heroes)
        bethel = next(lm for lm in self.slice["landmarks"] if lm["name"] == "Seamen's Bethel")
        self.assertEqual(len(bethel["pos"]), 2)
        # Document the published slice position. Do not replace it.
        self.assertEqual(bethel["pos"], [-272.23, -106.5])
        bounds = bounds_xz(self.graph["roads"])
        self.assertIsNotNone(bounds)
        self.assertGreater(bounds["count"], 0)


if __name__ == "__main__":
    unittest.main()
