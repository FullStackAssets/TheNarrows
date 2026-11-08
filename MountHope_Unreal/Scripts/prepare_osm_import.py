#!/usr/bin/env python3
"""Build or check the Unreal OSM import manifest.

Reads the committed slice and southcoast road JSON. Writes
``MountHope_Unreal/Data/OSM/import_manifest.json``. Does not modify the shared
map pipeline or either editor importer's coordinate math.

    python3 Scripts/prepare_osm_import.py           # write the manifest
    python3 Scripts/prepare_osm_import.py --check   # fail if the file drifted
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = PROJECT_ROOT.parent
MANIFEST_PATH = PROJECT_ROOT / "Data" / "OSM" / "import_manifest.json"

SLICE_RELATIVE = "QUAHOG_Web/public/slice-newbedford.json"
ROADS_RELATIVE = "quahog-project-files/mapdata/southcoast-roads.json"
OBJ_RELATIVE = "quahog-project-files/mapdata/southcoast.obj"

PLAYABLE_RADIUS_M = 400.0
LANDMARK_RADIUS_M = 800.0
METERS_TO_CM = 100.0


def round2(value: float) -> float:
    return round(float(value), 2)


def slice_importer_cm(east_m: float, north_m: float) -> list[float]:
    """editor_import_osm.py: X=east, Y=-north, Z=up."""
    return [round2(east_m * METERS_TO_CM), round2(-north_m * METERS_TO_CM), 0.0]


def mission_plane_cm(east_m: float, north_m: float) -> list[float]:
    """editor_import_southcoast_roads.py and mission targets: X=east, Y=up, Z=north."""
    return [round2(east_m * METERS_TO_CM), 0.0, round2(north_m * METERS_TO_CM)]


def point_pair(point: object) -> tuple[float, float] | None:
    if not isinstance(point, (list, tuple)) or len(point) < 2:
        return None
    try:
        return float(point[0]), float(point[1])
    except (TypeError, ValueError):
        return None


def within_radius(east_m: float, north_m: float, radius_m: float) -> bool:
    return (east_m * east_m) + (north_m * north_m) <= radius_m * radius_m


def build_manifest() -> dict:
    slice_path = REPO_ROOT / SLICE_RELATIVE
    roads_path = REPO_ROOT / ROADS_RELATIVE
    obj_path = REPO_ROOT / OBJ_RELATIVE

    slice_data = json.loads(slice_path.read_text(encoding="utf-8"))
    roads_data = json.loads(roads_path.read_text(encoding="utf-8"))

    roads = slice_data.get("roads") or []
    landmarks = slice_data.get("landmarks") or []
    water = slice_data.get("water") or []
    buildings = slice_data.get("buildings") or []

    histogram: dict[str, int] = {}
    min_x = min_y = float("inf")
    max_x = max_y = float("-inf")
    near_origin = 0
    playable_roads: list[dict] = []

    for road in roads:
        if not isinstance(road, dict):
            continue
        highway = str(road.get("highway") or "unknown")
        histogram[highway] = histogram.get(highway, 0) + 1
        points = [pair for pair in (point_pair(point) for point in road.get("points") or []) if pair]
        if not points:
            continue
        for east_m, north_m in points:
            min_x = min(min_x, east_m)
            min_y = min(min_y, north_m)
            max_x = max(max_x, east_m)
            max_y = max(max_y, north_m)
        if any(within_radius(east_m, north_m, PLAYABLE_RADIUS_M) for east_m, north_m in points):
            near_origin += 1
            start_east, start_north = points[0]
            width = road.get("width")
            playable_roads.append(
                {
                    "name": road.get("name") or "",
                    "highway": highway,
                    "width": round2(width) if isinstance(width, (int, float)) else None,
                    "pointCount": len(points),
                    "sliceStartMeters": [round2(start_east), round2(start_north)],
                    "sliceImporterCm": slice_importer_cm(start_east, start_north),
                    "missionPlaneCm": mission_plane_cm(start_east, start_north),
                }
            )

    playable_roads.sort(key=lambda road: (road["name"], road["highway"], road["sliceStartMeters"][0], road["sliceStartMeters"][1]))

    playable_landmarks: list[dict] = []
    for landmark in landmarks:
        if not isinstance(landmark, dict):
            continue
        pair = point_pair(landmark.get("pos"))
        if pair is None:
            continue
        east_m, north_m = pair
        hero = bool(landmark.get("hero"))
        if not hero and not within_radius(east_m, north_m, LANDMARK_RADIUS_M):
            continue
        playable_landmarks.append(
            {
                "name": landmark.get("name") or "",
                "kind": landmark.get("kind") or "",
                "hero": hero,
                "sliceMeters": [round2(east_m), round2(north_m)],
                "sliceImporterCm": slice_importer_cm(east_m, north_m),
                "missionPlaneCm": mission_plane_cm(east_m, north_m),
            }
        )
    playable_landmarks.sort(key=lambda landmark: (not landmark["hero"], landmark["name"], landmark["sliceMeters"][0]))

    parsed_roads = roads_data.get("roads") or []
    if not isinstance(parsed_roads, list):
        parsed_roads = []

    bbox = None
    if min_x != float("inf"):
        bbox = {
            "minX": round2(min_x),
            "minY": round2(min_y),
            "maxX": round2(max_x),
            "maxY": round2(max_y),
        }

    return {
        "schema": 1,
        "note": (
            "Prepared import contract for Mount Hope. The slice importer and the "
            "southcoast-roads importer do not share a ground plane; both conversions "
            "are recorded and neither script's math is changed here. The shared map "
            "pipeline is read-only input."
        ),
        "sources": {
            "slice": SLICE_RELATIVE,
            "southcoastRoads": ROADS_RELATIVE,
            "southcoastObj": OBJ_RELATIVE,
            "southcoastObjPresent": obj_path.is_file(),
        },
        "coordinateContracts": [
            {
                "id": "slice-importer",
                "sourceAxes": "x=east meters, y=north meters",
                "unrealCm": "X = east * 100, Y = -north * 100, Z = up * 100",
                "usedBy": "Scripts/editor_import_osm.py",
            },
            {
                "id": "mission-plane",
                "sourceAxes": "x=east meters, point[1] or z = north meters, y = up",
                "unrealCm": "X = east * 100, Y = up * 100, Z = north * 100",
                "usedBy": "Scripts/editor_import_southcoast_roads.py and Data/Missions world targets",
            },
        ],
        "sliceOrigin": slice_data.get("origin"),
        "sliceAxesNote": slice_data.get("note"),
        "counts": {
            "sliceRoads": len(roads),
            "sliceLandmarks": len(landmarks),
            "sliceWater": len(water),
            "sliceBuildings": len(buildings),
            "sliceRoadsNearOrigin400m": near_origin,
            "southcoastRoadCountField": roads_data.get("road_count"),
            "southcoastParsedRoads": len(parsed_roads),
        },
        "southcoastAxes": roads_data.get("axes"),
        "southcoastOrigin": roads_data.get("origin"),
        "roadClassHistogram": dict(sorted(histogram.items())),
        "bboxMeters": bbox,
        "playableDistrict": {
            "radiusMeters": PLAYABLE_RADIUS_M,
            "landmarkRadiusMeters": LANDMARK_RADIUS_M,
            "roads": playable_roads,
            "landmarks": playable_landmarks,
        },
    }


def canonical(manifest: dict) -> str:
    return json.dumps(manifest, indent=2, ensure_ascii=False, sort_keys=True) + "\n"


def main() -> int:
    check_only = "--check" in sys.argv[1:]
    manifest = build_manifest()
    text = canonical(manifest)
    if check_only:
        if not MANIFEST_PATH.is_file():
            print(f"ERROR: missing {MANIFEST_PATH.relative_to(PROJECT_ROOT)}", file=sys.stderr)
            print("Run Scripts/prepare_osm_import.py to write it.", file=sys.stderr)
            return 1
        existing = MANIFEST_PATH.read_text(encoding="utf-8")
        if existing != text:
            print(
                "ERROR: Data/OSM/import_manifest.json does not match the slice/roads inputs.",
                file=sys.stderr,
            )
            print("Run Scripts/prepare_osm_import.py and commit the result.", file=sys.stderr)
            return 1
        print(
            "OSM import manifest matches "
            f"({manifest['counts']['sliceRoads']} slice roads, "
            f"{manifest['counts']['sliceRoadsNearOrigin400m']} inside {int(PLAYABLE_RADIUS_M)} m)"
        )
        return 0

    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(text, encoding="utf-8")
    print(f"Wrote {MANIFEST_PATH.relative_to(PROJECT_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
