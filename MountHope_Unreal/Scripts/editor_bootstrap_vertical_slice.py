#!/usr/bin/env python3
"""
Mount Hope — vertical-slice editor bootstrap.

Run inside Unreal Editor 5.8 with the Python Editor Script Plugin enabled:
  Tools → Execute Python Script → select this file

Creates /Game/Maps/MH_VerticalSlice, content folders, a player start, and a
placed MHVehiclePawn instance. Blueprint subclasses and Chaos wheel tuning
still require manual editor steps (see Docs/EDITOR_SETUP.md).
"""

from __future__ import annotations

try:
    import unreal
except ImportError as exc:  # pragma: no cover - only runs inside UE
    raise SystemExit(
        "This script must run inside the Unreal Editor Python environment."
    ) from exc


MAP_PATH = "/Game/Maps/MH_VerticalSlice"
PLAYER_START_LOCATION = unreal.Vector(-27200.0, 0.0, 200.0)
PLAYER_START_ROTATION = unreal.Rotator(0.0, 0.0, 0.0)
VEHICLE_LOCATION = unreal.Vector(-27000.0, 500.0, 200.0)
VEHICLE_ROTATION = unreal.Rotator(0.0, 90.0, 0.0)
NPC_LOCATION = unreal.Vector(-27223.0, 0.0, 10650.0)
NPC_CONVERSATION_ID = "deacon_mealy_harbor_errand"
NPC_SPEAKER = "Deacon Mealy"

# Derived from Data/Missions + Data/Economy JSON targets, scaled x100 to match
# NPC_LOCATION's established (JSON X*100, 0, JSON Z*100) convention above.
# Verify against final OSM-imported terrain height in editor before shipping.
SAFEHOUSE_LOCATION = unreal.Vector(-18800.0, 0.0, 4000.0)
GARAGE_LOCATION = unreal.Vector(-32000.0, 0.0, 6000.0)
GENERAL_STORE_LOCATION = unreal.Vector(-24000.0, 0.0, 12200.0)

COLLECTIBLE_LOCATIONS = (
    ("scrimshaw_bethel_pew", unreal.Vector(-26800.0, 0.0, 9800.0)),
    ("scrimshaw_anvil_garage", unreal.Vector(-32200.0, 0.0, 5800.0)),
)

PEDESTRIAN_LOCATIONS = (
    unreal.Vector(-27100.0, 200.0, 9700.0),
    unreal.Vector(-26900.0, -300.0, 10600.0),
    unreal.Vector(-32100.0, 150.0, 6000.0),
)

WEAPON_PICKUP_LOCATION = unreal.Vector(-27050.0, -200.0, 9750.0)

CONTENT_FOLDERS = (
    "/Game/Maps",
    "/Game/Blueprints",
    "/Game/Input",
    "/Game/Materials",
    "/Game/OSM",
)


def log(message: str) -> None:
    unreal.log(f"[MountHope Bootstrap] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[MountHope Bootstrap] {message}")


def ensure_content_folders() -> None:
    for folder in CONTENT_FOLDERS:
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.make_directory(folder)
            log(f"Created content folder: {folder}")


def save_map_if_needed() -> None:
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        log(f"Map already exists: {MAP_PATH}")
        unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        return

    log("Creating new Open World map...")
    unreal.EditorLoadingAndSavingUtils.new_blank_map(save_existing_packages=False)

    saved = unreal.EditorLoadingAndSavingUtils.save_map(
        unreal.EditorLevelLibrary.get_editor_world(),
        MAP_PATH,
    )
    if not saved:
        warn(f"Failed to save map to {MAP_PATH}")
    else:
        log(f"Saved map: {MAP_PATH}")


def spawn_or_find_player_start() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(actor, unreal.PlayerStart):
            actor.set_actor_location(PLAYER_START_LOCATION, False, True)
            actor.set_actor_rotation(PLAYER_START_ROTATION, False)
            log("Repositioned existing PlayerStart")
            return

    unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        PLAYER_START_LOCATION,
        PLAYER_START_ROTATION,
    )
    log("Spawned PlayerStart")


def spawn_vehicle_pawn() -> None:
    vehicle_class = unreal.load_class(None, "/Script/MountHope.MHVehiclePawn")
    if vehicle_class is None:
        warn("Could not load MHVehiclePawn; compile C++ first.")
        return

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class() == vehicle_class:
            log("MHVehiclePawn already placed in level")
            return

    unreal.EditorLevelLibrary.spawn_actor_from_class(
        vehicle_class,
        VEHICLE_LOCATION,
        VEHICLE_ROTATION,
    )
    log("Spawned MHVehiclePawn (assign mesh + Chaos wheels in editor)")


def spawn_mission_npc() -> None:
    npc_class = unreal.load_class(None, "/Script/MountHope.MHDialogueNpcActor")
    if npc_class is None:
        warn("Could not load MHDialogueNpcActor; compile C++ first.")
        return

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class() == npc_class:
            log("MHDialogueNpcActor already placed")
            return

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        npc_class,
        NPC_LOCATION,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    actor.set_actor_label("BP_DeaconMealy_NPC")
    try:
        actor.set_editor_property("conversation_id", unreal.Name(NPC_CONVERSATION_ID))
        actor.set_editor_property("speaker_display_name", NPC_SPEAKER)
        actor.set_editor_property("interaction_prompt_override", f"Talk to {NPC_SPEAKER}")
    except Exception:
        try:
            actor.set_editor_property("ConversationId", unreal.Name(NPC_CONVERSATION_ID))
            actor.set_editor_property("SpeakerDisplayName", NPC_SPEAKER)
        except Exception:
            warn("Set ConversationId on Deacon Mealy NPC manually in Details panel.")

    log("Spawned Deacon Mealy dialogue NPC")


def _spawn_singleton(class_path: str, location: "unreal.Vector", label: str) -> None:
    actor_class = unreal.load_class(None, class_path)
    if actor_class is None:
        warn(f"Could not load {class_path}; compile C++ first.")
        return

    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class() == actor_class and actor.get_actor_label() == label:
            log(f"{label} already placed")
            return

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if actor:
        actor.set_actor_label(label)
        log(f"Spawned {label}")


def spawn_safehouse() -> None:
    _spawn_singleton("/Script/MountHope.MHSafehouseActor", SAFEHOUSE_LOCATION, "BP_MHSafehouse_Narrows")


def spawn_garage_shop() -> None:
    _spawn_singleton("/Script/MountHope.MHShopActor", GARAGE_LOCATION, "BP_MHShop_AnvilGarage")


def spawn_general_store_shop() -> None:
    _spawn_singleton("/Script/MountHope.MHShopActor", GENERAL_STORE_LOCATION, "BP_MHShop_QuohogRepublic")


def spawn_collectibles() -> None:
    collectible_class = unreal.load_class(None, "/Script/MountHope.MHCollectibleActor")
    if collectible_class is None:
        warn("Could not load MHCollectibleActor; compile C++ first.")
        return

    placed_labels = {
        actor.get_actor_label()
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if actor.get_class() == collectible_class
    }

    for item_id, location in COLLECTIBLE_LOCATIONS:
        label = f"BP_MHCollectible_{item_id}"
        if label in placed_labels:
            log(f"{label} already placed")
            continue

        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            collectible_class,
            location,
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if actor:
            actor.set_actor_label(label)
            try:
                actor.set_editor_property("item_id", unreal.Name(item_id))
            except Exception:
                warn(f"Set ItemId={item_id} on {label} manually in Details panel.")
            log(f"Spawned {label}")


def spawn_pedestrians() -> None:
    pedestrian_class = unreal.load_class(None, "/Script/MountHope.MHPedestrianCharacter")
    if pedestrian_class is None:
        warn("Could not load MHPedestrianCharacter; compile C++ first.")
        return

    placed = sum(
        1
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if actor.get_class() == pedestrian_class
    )
    if placed >= len(PEDESTRIAN_LOCATIONS):
        log("Pedestrians already placed")
        return

    for index, location in enumerate(PEDESTRIAN_LOCATIONS[placed:], start=placed):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            pedestrian_class,
            location,
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if actor:
            actor.set_actor_label(f"BP_MHPedestrian_{index}")
            log(f"Spawned BP_MHPedestrian_{index}")


def spawn_weapon_pickup() -> None:
    _spawn_singleton("/Script/MountHope.MHWeaponPickupActor", WEAPON_PICKUP_LOCATION, "BP_MHWeaponPickup_Pistol")


def save_dirty_packages() -> None:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


def print_next_steps() -> None:
    warn(
        "Run next:\n"
        "  1. Scripts/editor_create_enhanced_input.py\n"
        "  2. Scripts/editor_import_osm.py\n"
        "  3. Scripts/editor_import_southcoast_roads.py (optional, denser roads)\n"
        "  4. Scripts/editor_setup_navmesh.py\n"
        "  5. Create BP_MHVehicle (mesh + Chaos wheels)\n"
        "  6. Set ShopType=Garage on BP_MHShop_AnvilGarage. Set ShopType=GeneralStore "
        "on BP_MHShop_QuohogRepublic (LinkedBusinessId=quohog, or LinkedOfferId=medkit). "
        "A general store also pays the active Pay objective (dock_bribe during Auction Rules)\n"
        "  7. Import radio VO/music from Data/Radio/stations.json's voFolder/jingleFolder "
        "paths as USoundWave assets and wire them to a Blueprint radio player bound to "
        "UMHRadioSubsystem::OnStationChanged / OnSongChanged\n"
        "  8. The campaign in Data/Missions/vertical_slice.json now spans New Bedford, "
        "Fall River (Battleship Cove/Borden House), Brockton (Champion City Gym), and "
        "Cape Cod (Hyannis) — missions past 'Harbor Heat' need those areas' OSM data "
        "imported/authored before their trigger volumes are reachable\n"
        "  9. Press Play — talk to Deacon Mealy with E. When a line lists choices, "
        "press 1 or 2 (DialogueChoice1/DialogueChoice2). R cycles the radio while driving\n"
        " 10. BP_MHPedestrian_* wander/flee via AAIController + the navmesh from step 4 — "
        "bake the navmesh before expecting them to path (the pedestrian spawner also needs "
        "the navmesh to place new pedestrians around the player)\n"
        " 11. Left mouse fires the pistol once BP_MHWeaponPickup_Pistol is picked up — "
        "hits register on AMHVehiclePawn and AMHPedestrianCharacter via an ECC_Visibility "
        "trace; adjust the trace channel if your collision presets differ\n"
        " 12. The minimap (bottom-right) and mission-complete toasts are spawned/wired "
        "automatically at runtime — no editor placement needed, just compile"
    )


def main() -> None:
    log("Starting vertical-slice bootstrap...")
    ensure_content_folders()
    save_map_if_needed()
    spawn_or_find_player_start()
    spawn_vehicle_pawn()
    spawn_mission_npc()
    spawn_safehouse()
    spawn_garage_shop()
    spawn_general_store_shop()
    spawn_collectibles()
    spawn_pedestrians()
    spawn_weapon_pickup()
    save_dirty_packages()
    print_next_steps()
    log("Bootstrap complete.")


if __name__ == "__main__":
    main()
