#!/usr/bin/env python3
"""
Import OSM graybox geometry and optional slice road splines into the vertical slice.

Run inside Unreal Editor 5.8 (Python Editor Script Plugin):
  Tools → Execute Python Script → select this file

Steps performed:
  1. Import ../quahog-project-files/mapdata/southcoast.obj → /Game/OSM/SM_SouthCoast_Blockout
  2. Place the mesh in the active level at the origin (meters → centimeters scale)
  3. Optionally spawn debug spline actors for roads from slice-newbedford.json
     (bounded by MAX_ROAD_SPLINES to keep editor responsive)

See Docs/OSM_TO_UNREAL.md for coordinate notes.
"""

from __future__ import annotations

import json
import os

try:
    import unreal
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "This script must run inside the Unreal Editor Python environment."
    ) from exc


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, ".."))
REPO_ROOT = os.path.normpath(os.path.join(PROJECT_ROOT, ".."))

OBJ_SOURCE = os.path.join(REPO_ROOT, "quahog-project-files", "mapdata", "southcoast.obj")
SLICE_JSON = os.path.join(REPO_ROOT, "QUAHOG_Web", "public", "slice-newbedford.json")

MESH_DEST_PATH = "/Game/OSM/SM_SouthCoast_Blockout"
OSM_FOLDER = "/Game/OSM"
MAP_PATH = "/Game/Maps/MH_VerticalSlice"

METERS_TO_CM = 100.0
MAX_ROAD_SPLINES = 120
SLICE_ROAD_BBOX_METERS = 2500.0  # fallback when Data/OSM/import_manifest.json is absent
MANIFEST_PATH = os.path.join(PROJECT_ROOT, "Data", "OSM", "import_manifest.json")


def log(message: str) -> None:
    unreal.log(f"[MountHope OSM] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[MountHope OSM] {message}")


def ensure_folders() -> None:
    for folder in (OSM_FOLDER, "/Game/Maps"):
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.make_directory(folder)


def import_obj_mesh() -> unreal.StaticMesh | None:
    if not os.path.isfile(OBJ_SOURCE):
        warn(f"OBJ not found: {OBJ_SOURCE}")
        return None

    if unreal.EditorAssetLibrary.does_asset_exist(MESH_DEST_PATH):
        mesh = unreal.EditorAssetLibrary.load_asset(MESH_DEST_PATH)
        log(f"Using existing mesh: {MESH_DEST_PATH}")
        return mesh

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", OBJ_SOURCE)
    task.set_editor_property("destination_path", OSM_FOLDER)
    task.set_editor_property("destination_name", "SM_SouthCoast_Blockout")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("combine_meshes", True)
    options.set_editor_property("auto_generate_collision", True)
    task.set_editor_property("options", options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    if unreal.EditorAssetLibrary.does_asset_exist(MESH_DEST_PATH):
        mesh = unreal.EditorAssetLibrary.load_asset(MESH_DEST_PATH)
        log(f"Imported {OBJ_SOURCE} → {MESH_DEST_PATH}")
        return mesh

    warn("OBJ import finished but mesh asset was not found — import manually via File → Import.")
    return None


def load_vertical_slice_map() -> None:
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        log(f"Loaded map {MAP_PATH}")
    else:
        warn(f"Map {MAP_PATH} missing — run editor_bootstrap_vertical_slice.py first.")


def place_blockout_mesh(mesh: unreal.StaticMesh) -> None:
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor):
            static_mesh = actor.static_mesh_component.get_static_mesh()
            if static_mesh == mesh:
                log("SouthCoast blockout mesh already placed")
                return

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    actor.set_actor_scale3d(unreal.Vector(METERS_TO_CM, METERS_TO_CM, METERS_TO_CM))
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_profile_name("BlockAll")
    log("Placed SouthCoast blockout at origin (scale 100 — verify in viewport)")


def slice_point_to_unreal(x_meters: float, y_north_meters: float, z_up_meters: float = 0.0) -> unreal.Vector:
    """Web slice 2D uses x=east, y=north; Unreal uses X=east, Y=-north, Z=up (cm)."""
    return unreal.Vector(
        x_meters * METERS_TO_CM,
        -y_north_meters * METERS_TO_CM,
        z_up_meters * METERS_TO_CM,
    )


_PLAYABLE_RADIUS_M: float | None = None


def playable_radius_meters() -> float:
    """Prefer the prepared manifest radius. Fall back to the script constant.

    The manifest is a read-only contract (see Scripts/prepare_osm_import.py).
    Missing or unreadable data does not change the OBJ import. The radius is a
    Euclidean distance from the slice origin, matching the manifest's
    ``sliceRoadsNearOrigin400m`` count.
    """
    global _PLAYABLE_RADIUS_M
    if _PLAYABLE_RADIUS_M is not None:
        return _PLAYABLE_RADIUS_M

    radius = SLICE_ROAD_BBOX_METERS
    if os.path.isfile(MANIFEST_PATH):
        try:
            with open(MANIFEST_PATH, encoding="utf-8") as handle:
                manifest = json.load(handle)
            loaded = float(manifest["playableDistrict"]["radiusMeters"])
            if loaded > 0.0:
                radius = loaded
        except (OSError, ValueError, KeyError, TypeError):
            warn("OSM import manifest unreadable; using the script bbox default.")
    _PLAYABLE_RADIUS_M = radius
    return radius


def road_in_slice_bbox(road: dict) -> bool:
    radius = playable_radius_meters()
    radius_sq = radius * radius
    for point in road.get("points", []):
        if len(point) < 2:
            continue
        east = point[0]
        north = point[1]
        if (east * east) + (north * north) <= radius_sq:
            return True
    return False


def spawn_road_spline(road: dict, index: int) -> None:
    points = road.get("points", [])
    if len(points) < 2:
        return

    location = slice_point_to_unreal(points[0][0], points[0][1])
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.Actor,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        return
    actor.set_actor_label(f"MH_RoadSpline_{index:03d}_{road.get('name', 'road')[:24]}")

    # These debug road splines are optional visualization; the OBJ blockout import is
    # the primary geometry. Instance-component creation via Python varies across engine
    # builds, so degrade gracefully rather than aborting the whole import.
    add_component = getattr(actor, "add_component_by_class", None)
    if add_component is None:
        warn("Actor.add_component_by_class unavailable in this build; skipping debug "
             "road spline (OBJ blockout import is unaffected).")
        return
    try:
        spline = add_component(unreal.SplineComponent, False, unreal.Transform(), False)
    except Exception as exc:  # pragma: no cover - editor-only
        warn(f"Could not add spline component ({exc}); skipping debug road spline.")
        return
    if not spline:
        return

    actor_transform = actor.get_actor_transform()
    for i, point in enumerate(points):
        if len(point) < 2:
            continue
        world_point = slice_point_to_unreal(point[0], point[1])
        local_point = actor_transform.inverse_transform_position(world_point)
        spline.add_spline_point(local_point, unreal.SplineCoordinateSpace.LOCAL, False)

    spline.set_closed_loop(False, False)
    spline.set_editor_property("mobility", unreal.ComponentMobility.STATIC)


def import_slice_road_splines() -> None:
    if not os.path.isfile(SLICE_JSON):
        warn(f"Slice JSON not found: {SLICE_JSON}")
        return

    with open(SLICE_JSON, encoding="utf-8") as handle:
        data = json.load(handle)

    roads = [road for road in data.get("roads", []) if road_in_slice_bbox(road)]
    roads = roads[:MAX_ROAD_SPLINES]

    if not roads:
        warn("No slice roads matched the vertical-slice bounding box.")
        return

    if not unreal.EditorLevelLibrary.get_editor_world():
        warn("No editor world loaded.")
        return

    for index, road in enumerate(roads):
        spawn_road_spline(road, index)

    log(f"Spawned {len(roads)} debug road splines (max {MAX_ROAD_SPLINES})")


def save_all() -> None:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


def main() -> None:
    log("Starting OSM import...")
    ensure_folders()
    load_vertical_slice_map()

    mesh = import_obj_mesh()
    if mesh:
        place_blockout_mesh(mesh)

    import_slice_road_splines()
    save_all()

    warn(
        "Verify mesh scale in viewport (expect ~100x if OBJ is in meters).\n"
        "Tune SLICE_ROAD_BBOX_METERS / MAX_ROAD_SPLINES at top of script if needed."
    )
    log("OSM import pass complete.")


if __name__ == "__main__":
    main()
