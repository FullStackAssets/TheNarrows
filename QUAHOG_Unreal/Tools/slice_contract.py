"""Data contract for the slice file QUAHOG_Unreal boots.

This module reads JSON. It does not compile or run the Unreal loader. The
acceptance rules mirror `MHWorldSliceSubsystem.cpp` so a change in either
place can be caught without an engine. A passing result is not a UE build
and not evidence that this module's types match MountHope_Unreal at runtime.
"""

from __future__ import annotations

import json
from pathlib import Path


TRACK_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = TRACK_ROOT.parent
SLICE_PATH = REPO_ROOT / "QUAHOG_Web" / "public" / "slice-newbedford.json"
MISSION_PATH = TRACK_ROOT / "Data" / "Missions" / "vertical_slice.json"

# Same numbers as the first step of "Off the Boat" and the Seamen's Bethel
# landmark. Locked here so the 2D->world lift stays tied to authored missions.
BETHEL_NAME = "Seamen's Bethel"
BETHEL_SLICE_POS = (-272.23, -106.5)
BETHEL_WORLD = (-272.23, 0.0, 106.5)


def slice_2d_to_world(east: float, north: float) -> tuple[float, float, float]:
    """east -> +X, up -> +Y, north -> -Z. See Slice2DToWorld in the C++ loader."""
    return (float(east), 0.0, -float(north))


def _as_point(value) -> tuple[float, float] | None:
    if not isinstance(value, list) or len(value) < 2:
        return None
    x, y = value[0], value[1]
    if isinstance(x, bool) or isinstance(y, bool):
        return None
    if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
        return None
    return (float(x), float(y))


def _read_vec2(points) -> list[tuple[float, float]] | None:
    if not isinstance(points, list):
        return None
    out: list[tuple[float, float]] = []
    for point in points:
        parsed = _as_point(point)
        if parsed is None:
            return None
        out.append(parsed)
    if len(out) < 2:
        return None
    return out


def _optional_string(value) -> str:
    return value if isinstance(value, str) else ""


def load_slice(path: Path = SLICE_PATH) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"{path} root is not an object")
    return data


def accepted_roads(data: dict) -> list[dict]:
    accepted = []
    for road in data.get("roads") or []:
        if not isinstance(road, dict):
            continue
        points = _read_vec2(road.get("points"))
        if not points:
            continue
        width = road.get("width", 7.0)
        if isinstance(width, bool) or not isinstance(width, (int, float)):
            width = 7.0
        accepted.append(
            {
                "name": _optional_string(road.get("name")),
                "highway": _optional_string(road.get("highway")),
                "width": float(width),
                "points": points,
            }
        )
    return accepted


def accepted_landmarks(data: dict) -> list[dict]:
    accepted = []
    for landmark in data.get("landmarks") or []:
        if not isinstance(landmark, dict):
            continue
        name = landmark.get("name")
        if not isinstance(name, str) or not name:
            continue
        pos = landmark.get("pos")
        location = None
        if isinstance(pos, list) and len(pos) >= 3 and all(
            isinstance(component, (int, float)) and not isinstance(component, bool) for component in pos[:3]
        ):
            # Already world space: stored as X, Y, Z. Not run through the 2D lift.
            location = (float(pos[0]), float(pos[1]), float(pos[2]))
        elif isinstance(pos, list) and len(pos) == 2:
            point = _as_point(pos)
            if point is not None:
                location = slice_2d_to_world(*point)
        hero = landmark.get("hero") is True
        accepted.append(
            {
                "name": name,
                "kind": _optional_string(landmark.get("kind")),
                "hero": hero,
                "location": location,
            }
        )
    return accepted


def accepted_water(data: dict) -> list[list[tuple[float, float]]]:
    rings = []
    for ring in data.get("water") or []:
        points = _read_vec2(ring)
        if points and len(points) >= 3:
            rings.append(points)
    return rings


def validate_committed_slice() -> dict:
    """Check the committed web slice and the mission file this track points at.

    Returns a small summary dict. Raises AssertionError when the contract breaks.
    """
    assert SLICE_PATH.is_file(), f"missing slice {SLICE_PATH}"
    data = load_slice()
    assert isinstance(data.get("name"), str) and data["name"], "slice name missing"
    origin = data.get("origin") or {}
    assert isinstance(origin.get("lat"), (int, float))
    assert isinstance(origin.get("lon"), (int, float))

    raw_roads = data.get("roads")
    assert isinstance(raw_roads, list) and raw_roads, "slice has no roads"
    roads = accepted_roads(data)
    assert len(roads) == len(raw_roads), "one or more roads would be dropped by the loader rules"
    unnamed = sum(1 for road in roads if road["name"] == "")
    assert unnamed > 0, "expected the committed slice to contain unnamed roads"

    raw_landmarks = data.get("landmarks")
    assert isinstance(raw_landmarks, list) and raw_landmarks
    landmarks = accepted_landmarks(data)
    assert len(landmarks) == len(raw_landmarks), "one or more landmarks would be dropped"
    assert all(len(item.get("pos") or []) == 2 for item in raw_landmarks), "committed landmarks are 2D"
    assert all(item["location"] is not None for item in landmarks)
    heroes = [item["name"] for item in landmarks if item["hero"]]
    assert BETHEL_NAME in heroes

    bethel = next(item for item in landmarks if item["name"] == BETHEL_NAME)
    assert slice_2d_to_world(*BETHEL_SLICE_POS) == BETHEL_WORLD
    for got, expected in zip(bethel["location"], BETHEL_WORLD):
        assert abs(got - expected) < 1e-3, (bethel["location"], BETHEL_WORLD)

    missions = json.loads(MISSION_PATH.read_text(encoding="utf-8"))
    first = missions["missions"][0]["steps"][0]
    assert first["text"].startswith("Meet Deacon Mealy")
    target = tuple(first["target"])
    for got, expected in zip(bethel["location"], target):
        assert abs(got - expected) < 1e-3, (bethel["location"], target)

    raw_water = data.get("water")
    assert isinstance(raw_water, list) and raw_water
    water = accepted_water(data)
    assert len(water) == len(raw_water), "one or more water rings would be dropped"

    assert data.get("buildings") == []
    assert data.get("buildingsStreamed") is True

    return {
        "slice": data["name"],
        "origin": (origin["lat"], origin["lon"]),
        "roads": len(roads),
        "unnamed_roads": unnamed,
        "landmarks": len(landmarks),
        "heroes": heroes,
        "water_rings": len(water),
        "buildings": 0,
        "buildings_streamed": True,
        "bethel_world": bethel["location"],
    }
