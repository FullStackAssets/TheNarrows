"""Godot-side adapter for the shared OSM artifacts.

This module does not pull Overpass and does not write map data. It reads the
committed outputs of ``quahog-project-files/mapdata/`` and describes how
``QUAHOG_Godot`` consumes them.

Two local frames are in the tree today (verified against the files, not a new
survey):

* Road graph (``bake_meshes.py`` → ``southcoast-roads.json``): points are
  ``[east, north]`` meters. Godot uses ``x = east``, ``z = north``, ``y = up``.
* Slice (``make_slice.py`` plus ``make_tiles.py`` → ``slice-newbedford.json``):
  2D points are ``[east, north]`` too, but the slice note says 3D is
  ``x = east``, ``z = -north``, ``y = up``. That matches the web ``toWorld``.

The frames do not share an origin or a longitude scale. Convert through the
lat/lon each file publishes. Do not treat a null ``city`` as a municipality.
"""

from __future__ import annotations

import math
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

# Lighting colors keyed by the OSM ``highway`` tag. Not municipalities.
HIGHWAY_RGB = {
    "motorway": (1.00, 0.24, 0.66),
    "trunk": (1.00, 0.24, 0.66),
    "primary": (1.00, 0.45, 0.72),
    "secondary": (0.25, 0.85, 0.90),
    "tertiary": (0.35, 0.75, 0.85),
    "residential": (0.95, 0.62, 0.28),
    "unclassified": (0.90, 0.55, 0.25),
    "living_street": (0.95, 0.70, 0.40),
    "service": (0.55, 0.30, 0.85),
    "pedestrian": (0.95, 0.75, 0.45),
    "footway": (0.35, 0.45, 0.75),
    "path": (0.35, 0.45, 0.75),
    "cycleway": (0.30, 0.70, 0.55),
    "steps": (0.35, 0.45, 0.75),
    "track": (0.45, 0.40, 0.30),
}
DEFAULT_RGB = (0.70, 0.70, 0.75)

# Must match WeatherController state names and HighwayNeon.gd.
WEATHER_EMISSION = {
    "CLEAR": 0.35,
    "DENSE_FOG": 0.20,
    "COASTAL_RAIN": 0.55,
    "NOREASTER": 0.15,
}
NIGHT_EMISSION_MULTIPLIER = 1.80
HIGHWAY_AMBIENT_MIX = 0.18
# WeatherController CLEAR ambient_color. Mixed only while that state is active.
CLEAR_AMBIENT = (0.55, 0.55, 0.60)

CELL_M = 64.0
TILE_M = 500.0
CONTACT_DISTANCE_M = 40.0
PROP_LATERAL_M = 1.5
OFFROAD_M = 80.0

Vec3 = Tuple[float, float, float]
Vec2 = Tuple[float, float]


def classify(doc: object) -> str:
    if not isinstance(doc, dict):
        return "unknown"
    if "roads" in doc and "road_count" in doc and "axes" in doc and "buildings" not in doc:
        return "road_graph"
    if "roads" in doc and "buildings" in doc and "landmarks" in doc:
        return "slice"
    return "unknown"


def osm_text(value: object) -> str:
    if not isinstance(value, str):
        return ""
    return value.strip()


def highway_rgb(highway: object) -> Tuple[float, float, float]:
    key = osm_text(highway)
    if key in HIGHWAY_RGB:
        return HIGHWAY_RGB[key]
    if key.endswith("_link"):
        base = key[: -len("_link")]
        if base in HIGHWAY_RGB:
            return HIGHWAY_RGB[base]
    return DEFAULT_RGB


def is_night(hour: int) -> bool:
    """Same window as PostProcessManager.on_hour_changed (hour >= 20 or < 6)."""
    return hour >= 20 or hour < 6


def emission_energy(weather_state: str, hour: int) -> float:
    base = WEATHER_EMISSION.get(weather_state, WEATHER_EMISSION["CLEAR"])
    if is_night(hour):
        return base * NIGHT_EMISSION_MULTIPLIER
    return base


def clear_ambient(highway: object) -> Tuple[float, float, float]:
    neon = highway_rgb(highway)
    mix = HIGHWAY_AMBIENT_MIX
    return tuple(CLEAR_AMBIENT[i] * (1.0 - mix) + neon[i] * mix for i in range(3))


def full_width(road: dict, fallback: float = 6.0) -> float:
    """Use the width stored on the feature. Do not re-derive it from highway.

    ``bake_meshes.py`` and ``make_slice.py`` publish different full-width
    tables. Re-deriving would hide that mismatch.
    """
    width = road.get("width")
    if isinstance(width, (int, float)) and not isinstance(width, bool) and width > 0:
        return float(width)
    return fallback


def road_graph_point_to_godot(east: float, north: float, y: float = 0.0) -> Vec3:
    return (east, y, north)


def slice_point_to_godot(east: float, north: float, y: float = 0.0) -> Vec3:
    return (east, y, -north)


def slice_to_wgs84(east: float, north: float, origin: dict, meters_per_degree: dict) -> Vec2:
    lat = float(origin["lat"]) + north / float(meters_per_degree["lat"])
    lon = float(origin["lon"]) + east / float(meters_per_degree["lon"])
    return (lat, lon)


def wgs84_to_road_graph_xz(lat: float, lon: float, origin: dict, meters_per_degree: dict) -> Vec2:
    x = (lon - float(origin["lon"])) * float(meters_per_degree["lon"])
    z = (lat - float(origin["lat"])) * float(meters_per_degree["lat"])
    return (x, z)


def slice_point_to_road_graph_xz(east: float, north: float, slice_doc: dict, graph_doc: dict) -> Vec2:
    lat, lon = slice_to_wgs84(east, north, slice_doc["origin"], slice_doc["meters_per_degree"])
    return wgs84_to_road_graph_xz(lat, lon, graph_doc["origin"], graph_doc["meters_per_degree"])


def building_tile_key(east: float, north: float, tile_m: float = TILE_M) -> str:
    """Match ``make_tiles.py``: ``floor(east / tile)_floor(north / tile)``.

    GDScript ``int()`` truncates toward zero. Negative tiles need ``floor``.
    """
    return f"{math.floor(east / tile_m)}_{math.floor(north / tile_m)}"


def banner_for_road(road: dict) -> str:
    parts = []
    city = osm_text(road.get("city"))
    name = osm_text(road.get("name"))
    highway = osm_text(road.get("highway"))
    if city:
        parts.append(city)
    if name:
        parts.append(name)
    elif highway:
        parts.append(highway)
    return " · ".join(parts)


def pick_spawn_road(roads: Sequence[dict]) -> Optional[int]:
    """First named road with at least two points.

    The current bake stores JSON null for ``city`` on every road, so a filter
    on ``city == "New Bedford"`` matches nothing. This does not guess a city
    from coordinates or from the source filename.
    """
    fallback = None
    for i, road in enumerate(roads):
        pts = road.get("points") or []
        if len(pts) < 2:
            continue
        if fallback is None:
            fallback = i
        if osm_text(road.get("name")):
            return i
    return fallback


def point_along(points: Sequence[Sequence[float]], distance: float, frame: str = "road_graph") -> Optional[Vec3]:
    """Walk a polyline by meters in the Godot XZ plane. Returns the end if short."""
    if len(points) < 2:
        return None
    to_godot = road_graph_point_to_godot if frame == "road_graph" else slice_point_to_godot
    prev = to_godot(float(points[0][0]), float(points[0][1]), 0.0)
    walked = 0.0
    for raw in points[1:]:
        cur = to_godot(float(raw[0]), float(raw[1]), 0.0)
        step = math.hypot(cur[0] - prev[0], cur[2] - prev[2])
        if step < 1e-8:
            prev = cur
            continue
        if walked + step >= distance:
            t = (distance - walked) / step
            return (
                prev[0] + (cur[0] - prev[0]) * t,
                0.0,
                prev[2] + (cur[2] - prev[2]) * t,
            )
        walked += step
        prev = cur
    last = to_godot(float(points[-1][0]), float(points[-1][1]), 0.0)
    return (last[0], 0.0, last[2])


def tangent_at(points: Sequence[Sequence[float]], distance: float, frame: str = "road_graph") -> Vec2:
    """Unit XZ tangent at ``distance`` along the polyline. Falls back to +X."""
    if len(points) < 2:
        return (1.0, 0.0)
    to_godot = road_graph_point_to_godot if frame == "road_graph" else slice_point_to_godot
    prev = to_godot(float(points[0][0]), float(points[0][1]), 0.0)
    walked = 0.0
    last_dir = (1.0, 0.0)
    for raw in points[1:]:
        cur = to_godot(float(raw[0]), float(raw[1]), 0.0)
        dx = cur[0] - prev[0]
        dz = cur[2] - prev[2]
        step = math.hypot(dx, dz)
        if step < 1e-8:
            prev = cur
            continue
        last_dir = (dx / step, dz / step)
        if walked + step >= distance:
            return last_dir
        walked += step
        prev = cur
    return last_dir


def contact_pose(points: Sequence[Sequence[float]], distance: float = CONTACT_DISTANCE_M) -> Optional[dict]:
    """Gameplay prop on the real polyline, shifted sideways so it does not block the centerline.

    The lateral shift is a prop offset, not an OSM coordinate.
    """
    pos = point_along(points, distance, "road_graph")
    if pos is None:
        return None
    tx, tz = tangent_at(points, distance, "road_graph")
    # Left of the travel direction in the XZ plane.
    lx, lz = -tz, tx
    return {
        "position": (pos[0] + lx * PROP_LATERAL_M, 0.0, pos[2] + lz * PROP_LATERAL_M),
        "on_road": pos,
    }


def bounds_xz(roads: Sequence[dict]) -> Optional[dict]:
    min_x = min_z = math.inf
    max_x = max_z = -math.inf
    count = 0
    for road in roads:
        for point in road.get("points") or []:
            if len(point) < 2:
                continue
            x = float(point[0])
            z = float(point[1])
            min_x = min(min_x, x)
            max_x = max(max_x, x)
            min_z = min(min_z, z)
            max_z = max(max_z, z)
            count += 1
    if count == 0:
        return None
    return {
        "min": (min_x, min_z),
        "max": (max_x, max_z),
        "center": ((min_x + max_x) * 0.5, (min_z + max_z) * 0.5),
        "count": count,
    }


def _sample_polyline(points: Sequence[Sequence[float]], spacing: float) -> Iterable[Vec2]:
    if not points:
        return
    yield (float(points[0][0]), float(points[0][1]))
    for i in range(len(points) - 1):
        x1, z1 = float(points[i][0]), float(points[i][1])
        x2, z2 = float(points[i + 1][0]), float(points[i + 1][1])
        dist = math.hypot(x2 - x1, z2 - z1)
        if dist < 1e-8:
            continue
        steps = int(math.floor(dist / spacing))
        for s in range(1, steps + 1):
            t = (s * spacing) / dist
            if t >= 1.0:
                break
            yield (x1 + (x2 - x1) * t, z1 + (z2 - z1) * t)
        yield (x2, z2)


def build_cell_index(roads: Sequence[dict], cell_m: float = CELL_M) -> Dict[Tuple[int, int], List[int]]:
    cells: Dict[Tuple[int, int], List[int]] = {}
    for i, road in enumerate(roads):
        seen = set()
        for east, north in _sample_polyline(road.get("points") or [], cell_m):
            key = (math.floor(east / cell_m), math.floor(north / cell_m))
            if key in seen:
                continue
            seen.add(key)
            cells.setdefault(key, []).append(i)
    return cells


def _segment_distance(px: float, pz: float, ax: float, az: float, bx: float, bz: float) -> float:
    abx = bx - ax
    abz = bz - az
    length_sq = abx * abx + abz * abz
    if length_sq < 1e-8:
        return math.hypot(px - ax, pz - az)
    t = max(0.0, min(1.0, ((px - ax) * abx + (pz - az) * abz) / length_sq))
    return math.hypot(px - (ax + abx * t), pz - (az + abz * t))


def road_distance(road: dict, x: float, z: float) -> float:
    pts = road.get("points") or []
    if len(pts) == 1:
        return math.hypot(x - float(pts[0][0]), z - float(pts[0][1]))
    best = math.inf
    for i in range(len(pts) - 1):
        best = min(
            best,
            _segment_distance(
                x, z,
                float(pts[i][0]), float(pts[i][1]),
                float(pts[i + 1][0]), float(pts[i + 1][1]),
            ),
        )
    return best


def nearest_road(
    roads: Sequence[dict],
    cells: Dict[Tuple[int, int], List[int]],
    x: float,
    z: float,
    max_dist: float = OFFROAD_M,
    cell_m: float = CELL_M,
) -> Optional[dict]:
    cx = math.floor(x / cell_m)
    cz = math.floor(z / cell_m)
    best_i = -1
    best_d = max_dist
    for ix in range(cx - 1, cx + 2):
        for iz in range(cz - 1, cz + 2):
            for i in cells.get((ix, iz), ()):
                dist = road_distance(roads[i], x, z)
                if dist < best_d:
                    best_d = dist
                    best_i = i
    if best_i < 0:
        return None
    return {"index": best_i, "distance": best_d, "road": roads[best_i]}
