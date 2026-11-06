#!/usr/bin/env python3
"""Validate the Unreal scaffold without requiring Unreal Engine."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = PROJECT_ROOT.parent


REQUIRED_FILES = [
    "MountHope.uproject",
    "Config/DefaultEngine.ini",
    "Config/DefaultGame.ini",
    "Config/DefaultInput.ini",
    "Source/MountHope.Target.cs",
    "Source/MountHopeEditor.Target.cs",
    "Source/MountHope/MountHope.Build.cs",
    "Source/MountHope/MountHope.cpp",
    "Source/MountHope/MHGameInstance.h",
    "Source/MountHope/MHGameInstance.cpp",
    "Source/MountHope/MHGameModeBase.h",
    "Source/MountHope/MHGameModeBase.cpp",
    "Source/MountHope/MHPlayerCharacter.h",
    "Source/MountHope/MHPlayerCharacter.cpp",
    "Source/MountHope/MHVehiclePawn.h",
    "Source/MountHope/MHVehiclePawn.cpp",
    "Source/MountHope/MHInteractable.h",
    "Source/MountHope/MHEconomySubsystem.h",
    "Source/MountHope/MHEconomySubsystem.cpp",
    "Source/MountHope/MHMissionSubsystem.h",
    "Source/MountHope/MHMissionSubsystem.cpp",
    "Source/MountHope/MHOpenWorldSubsystem.h",
    "Source/MountHope/MHOpenWorldSubsystem.cpp",
    "Source/MountHope/MHWorldSliceSubsystem.h",
    "Source/MountHope/MHWorldSliceSubsystem.cpp",
    "Source/MountHope/MHWorldSliceTypes.h",
    "Source/MountHope/MHGameStateSubsystem.h",
    "Source/MountHope/MHGameStateSubsystem.cpp",
    "Source/MountHope/MHSaveGame.h",
    "Source/MountHope/MHMissionTriggerActor.h",
    "Source/MountHope/MHMissionTriggerActor.cpp",
    "Source/MountHope/MHDialogueTypes.h",
    "Source/MountHope/MHDialogueSubsystem.h",
    "Source/MountHope/MHDialogueSubsystem.cpp",
    "Source/MountHope/MHDialogueNpcActor.h",
    "Source/MountHope/MHDialogueNpcActor.cpp",
    "Source/MountHope/MHGameHudWidget.h",
    "Source/MountHope/MHGameHudWidget.cpp",
    "Source/MountHope/MHPlayerController.h",
    "Source/MountHope/MHPlayerController.cpp",
    "Source/MountHope/MHWeatherDirectorActor.h",
    "Source/MountHope/MHWeatherDirectorActor.cpp",
    "Source/MountHope/MHWantedSubsystem.h",
    "Source/MountHope/MHWantedSubsystem.cpp",
    "Source/MountHope/MHReputationSubsystem.h",
    "Source/MountHope/MHReputationSubsystem.cpp",
    "Source/MountHope/MHSafehouseActor.h",
    "Source/MountHope/MHSafehouseActor.cpp",
    "Source/MountHope/MHShopActor.h",
    "Source/MountHope/MHShopActor.cpp",
    "Source/MountHope/MHCollectibleSubsystem.h",
    "Source/MountHope/MHCollectibleSubsystem.cpp",
    "Source/MountHope/MHCollectibleActor.h",
    "Source/MountHope/MHCollectibleActor.cpp",
    "Source/MountHope/MHRadioSubsystem.h",
    "Source/MountHope/MHRadioSubsystem.cpp",
    "Source/MountHope/MHTimeOfDaySubsystem.h",
    "Source/MountHope/MHTimeOfDaySubsystem.cpp",
    "Source/MountHope/MHHealthPickupActor.h",
    "Source/MountHope/MHHealthPickupActor.cpp",
    "Source/MountHope/MHPedestrianCharacter.h",
    "Source/MountHope/MHPedestrianCharacter.cpp",
    "Source/MountHope/MHMinimapCaptureActor.h",
    "Source/MountHope/MHMinimapCaptureActor.cpp",
    "Source/MountHope/MHPedestrianSpawnerActor.h",
    "Source/MountHope/MHPedestrianSpawnerActor.cpp",
    "Source/MountHope/MHWeaponPickupActor.h",
    "Source/MountHope/MHWeaponPickupActor.cpp",
    "Source/MountHope/MHPoliceUnitPawn.h",
    "Source/MountHope/MHPoliceUnitPawn.cpp",
    "Source/MountHope/MHPoliceSpawnerActor.h",
    "Source/MountHope/MHPoliceSpawnerActor.cpp",
    "Data/Missions/vertical_slice.json",
    "Data/Economy/businesses.json",
    "Data/Dialogue/vertical_slice.json",
    "Data/Collectibles/vertical_slice.json",
    "Data/Radio/stations.json",
    "Data/OSM/import_manifest.json",
    "Docs/VERTICAL_SLICE.md",
    "Docs/OSM_TO_UNREAL.md",
    "Docs/BUILD_WINDOWS.md",
    "Docs/EDITOR_SETUP.md",
    "Docs/IMPROVEMENT_PLAN.md",
    "Docs/ARCHITECTURE.md",
    "Docs/CI_AND_BUILD.md",
    "Content/README.md",
    "Source/MountHope/Tests/MHWantedSubsystemTest.cpp",
    "Source/MountHope/Tests/MHMissionSubsystemTest.cpp",
    "Source/MountHope/Tests/MHGameStateSubsystemTest.cpp",
    "Source/MountHope/Tests/MHTimeOfDaySubsystemTest.cpp",
    "Source/MountHope/Tests/MHReputationSubsystemTest.cpp",
    "Source/MountHope/Tests/MHPoliceTest.cpp",
    "Source/MountHope/Tests/MHDialogueSubsystemTest.cpp",
    "Source/MountHope/Tests/MHEconomySubsystemTest.cpp",
    "Source/MountHope/Tests/MHCollectibleSubsystemTest.cpp",
    "Scripts/build.sh",
    "Scripts/build.ps1",
    "Scripts/build.bat",
    "Scripts/package.sh",
    "Scripts/package.ps1",
    "Scripts/run_tests.sh",
    "Scripts/editor_bootstrap_vertical_slice.py",
    "Scripts/editor_create_enhanced_input.py",
    "Scripts/editor_import_osm.py",
    "Scripts/prepare_osm_import.py",
    "Scripts/editor_import_southcoast_roads.py",
    "Scripts/editor_setup_navmesh.py",
    "Scripts/editor_create_hud_widget.py",
]

EXPECTED_PLUGINS = {
    "EnhancedInput",
    "ChaosVehiclesPlugin",
    "MassEntity",
    "MassAI",
    "Water",
}

EXPECTED_EXISTING_DATA = [
    "quahog-project-files/mapdata/southcoast-roads.json",
    "quahog-project-files/mapdata/southcoast.obj",
    "QUAHOG_Web/public/slice-newbedford.json",
]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def read_text(relative_path: str) -> str:
    path = PROJECT_ROOT / relative_path
    if not path.exists():
        fail(f"missing required file: {relative_path}")
    return path.read_text(encoding="utf-8")


def load_json(relative_path: str) -> dict:
    try:
        return json.loads(read_text(relative_path))
    except json.JSONDecodeError as exc:
        fail(f"{relative_path} is invalid JSON: {exc}")


EXPECTED_ENGINE = "5.8"


def validate_uproject() -> None:
    descriptor = load_json("MountHope.uproject")

    modules = descriptor.get("Modules", [])
    if not any(module.get("Name") == "MountHope" for module in modules):
        fail("MountHope.uproject does not declare the MountHope runtime module")

    enabled_plugins = {
        plugin.get("Name")
        for plugin in descriptor.get("Plugins", [])
        if plugin.get("Enabled") is True
    }
    missing_plugins = EXPECTED_PLUGINS - enabled_plugins
    if missing_plugins:
        fail(f"MountHope.uproject is missing plugins: {sorted(missing_plugins)}")

    if descriptor.get("EngineAssociation") != EXPECTED_ENGINE:
        fail(
            f"MountHope.uproject EngineAssociation must be {EXPECTED_ENGINE}, "
            f"got {descriptor.get('EngineAssociation')!r}"
        )


def validate_required_files() -> None:
    for relative_path in REQUIRED_FILES:
        read_text(relative_path)


def validate_target_rules() -> None:
    for target in ("MountHope.Target.cs", "MountHopeEditor.Target.cs"):
        text = read_text(f"Source/{target}")
        if "EngineIncludeOrderVersion.Unreal5_4" in text:
            fail(f"{target} still pins the legacy Unreal5_4 include order")
        if "DefaultBuildSettings" not in text:
            fail(f"{target} missing DefaultBuildSettings")
        if "IncludeOrderVersion" not in text:
            fail(f"{target} missing IncludeOrderVersion")


def validate_no_deprecated_macros() -> None:
    src_root = PROJECT_ROOT / "Source" / "MountHope"
    for path in src_root.rglob("*.cpp"):
        text = path.read_text(encoding="utf-8")
        for line in text.splitlines():
            if "UE_KINDA_SMALL_NUMBER" in line or "KINDA_SMALL_NUMBER" not in line:
                continue
            fail(f"{path.name}: use UE_KINDA_SMALL_NUMBER instead of KINDA_SMALL_NUMBER")


def validate_source_contract() -> None:
    module_source = read_text("Source/MountHope/MountHope.cpp")
    if 'IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MountHope, "MountHope")' not in module_source:
        fail("MountHope.cpp does not implement the primary game module")

    game_mode_source = read_text("Source/MountHope/MHGameModeBase.cpp")
    if "AMHPlayerCharacter::StaticClass()" not in game_mode_source:
        fail("MHGameModeBase.cpp does not set MHPlayerCharacter as the default pawn")
    if "AMHPlayerController::StaticClass()" not in game_mode_source:
        fail("MHGameModeBase.cpp does not set MHPlayerController")
    if "AMHWeatherDirectorActor" not in game_mode_source:
        fail("MHGameModeBase.cpp does not spawn weather director")
    for symbol in (
        "CompleteCurrentObjective",
        "TryCompleteVehicleObjective",
        "RefreshObjectiveTrigger",
    ):
        if symbol not in game_mode_source:
            fail(f"MHGameModeBase.cpp is missing mission helper: {symbol}")

    for symbol in ("FailCurrentMission", "ApplyObjectiveCompletion", "TickStepTimer"):
        if symbol not in game_mode_source:
            fail(f"MHGameModeBase.cpp is missing mission-failure/timed-objective support: {symbol}")
    for symbol in ("CompleteCurrentObjectiveFrom", "TryPayCurrentObjective", "NotifyCollectibleProgress"):
        if symbol not in game_mode_source:
            fail(f"MHGameModeBase.cpp is missing typed-objective support: {symbol}")

    mission_source = read_text("Source/MountHope/MHMissionSubsystem.cpp")
    for symbol in ("RestartCurrentMission", "IsMissionInProgress", "CanAcceptCompletion", "CanAcceptCollection"):
        if symbol not in mission_source:
            fail(f"MHMissionSubsystem.cpp is missing: {symbol}")

    game_instance = read_text("Source/MountHope/MHGameInstance.cpp")
    if "LoadDialogueFromJson" not in game_instance:
        fail("MHGameInstance.cpp does not bootstrap dialogue JSON")
    if "LoadOffersFromJson" not in game_instance:
        fail("MHGameInstance.cpp does not bootstrap economy offers")

    dialogue_source = read_text("Source/MountHope/MHDialogueSubsystem.cpp")
    for symbol in ("LoadDialogueFromJson", "StartConversation", "AdvanceConversation", "SelectChoice"):
        if symbol not in dialogue_source:
            fail(f"MHDialogueSubsystem.cpp is missing: {symbol}")

    npc_source = read_text("Source/MountHope/MHDialogueNpcActor.cpp")
    if "Interact_Implementation" not in npc_source:
        fail("MHDialogueNpcActor.cpp does not implement interaction")

    hud_source = read_text("Source/MountHope/MHGameHudWidget.cpp")
    for symbol in ("SetDialogueLine", "RefreshHud", "HandleDialogueLineChanged"):
        if symbol not in hud_source:
            fail(f"MHGameHudWidget.cpp is missing: {symbol}")

    controller_source = read_text("Source/MountHope/MHPlayerController.cpp")
    if "UMHGameHudWidget" not in controller_source:
        fail("MHPlayerController.cpp does not create HUD widget")

    weather_source = read_text("Source/MountHope/MHWeatherDirectorActor.cpp")
    if "ApplyWeather" not in weather_source:
        fail("MHWeatherDirectorActor.cpp is missing ApplyWeather")

    game_state_header = read_text("Source/MountHope/MHGameStateSubsystem.h")
    if "OnWeatherChanged" not in game_state_header:
        fail("MHGameStateSubsystem.h missing OnWeatherChanged delegate")

    player_source = read_text("Source/MountHope/MHPlayerCharacter.cpp")
    for symbol in (
        "UEnhancedInputComponent",
        "BindEnhancedInput",
        "BindLegacyInput",
        "InputMove",
        "EnterVehicle",
        "ExitVehicle",
        "FindNearestVehicle",
    ):
        if symbol not in player_source:
            fail(f"MHPlayerCharacter.cpp is missing: {symbol}")

    default_input = read_text("Config/DefaultInput.ini")
    if "EnhancedInput.EnhancedPlayerInput" not in default_input:
        fail("DefaultInput.ini does not set EnhancedPlayerInput as default")

    trigger_source = read_text("Source/MountHope/MHMissionTriggerActor.cpp")
    for symbol in ("SetTriggerRadius", "ResetConsumed"):
        if symbol not in trigger_source:
            fail(f"MHMissionTriggerActor.cpp is missing: {symbol}")

    build_source = read_text("Source/MountHope/MountHope.Build.cs")
    for dependency in ("EnhancedInput", "GameplayTags", "ChaosVehicles", "Json"):
        if dependency not in build_source:
            fail(f"MountHope.Build.cs is missing dependency: {dependency}")

    vehicle_source = read_text("Source/MountHope/MHVehiclePawn.cpp")
    for symbol in ("ApplyVehicleDamage", "RepairVehicle", "GetHealthPercent", "NotifyHit"):
        if symbol not in vehicle_source:
            fail(f"MHVehiclePawn.cpp is missing damage/repair support: {symbol}")

    wanted_source = read_text("Source/MountHope/MHWantedSubsystem.cpp")
    if "TickWantedDecay" not in wanted_source:
        fail("MHWantedSubsystem.cpp is missing TickWantedDecay")

    if "MHWantedSubsystem" not in game_mode_source or "TickWantedDecay" not in game_mode_source:
        fail("MHGameModeBase.cpp does not tick the wanted subsystem")
    if "MHReputationSubsystem" not in game_mode_source:
        fail("MHGameModeBase.cpp does not apply mission reputation")
    if "RespawnAtSafehouseIfAvailable" not in game_mode_source:
        fail("MHGameModeBase.cpp does not respawn at the saved safehouse")

    safehouse_source = read_text("Source/MountHope/MHSafehouseActor.cpp")
    if "Interact_Implementation" not in safehouse_source or "SetSafehouseLocation" not in safehouse_source:
        fail("MHSafehouseActor.cpp does not save/set the safehouse location")

    shop_source = read_text("Source/MountHope/MHShopActor.cpp")
    for symbol in ("HandleGarageInteraction", "HandleGeneralStoreInteraction", "RepairVehicle"):
        if symbol not in shop_source:
            fail(f"MHShopActor.cpp is missing: {symbol}")

    collectible_source = read_text("Source/MountHope/MHCollectibleActor.cpp")
    if "CollectItem" not in collectible_source:
        fail("MHCollectibleActor.cpp does not call CollectItem")
    if "NotifyCollectibleProgress" not in collectible_source or "ConsumePendingSetBonus" not in collectible_source:
        fail("MHCollectibleActor.cpp does not apply set bonuses or collect objectives")

    economy_source = read_text("Source/MountHope/MHEconomySubsystem.cpp")
    for symbol in ("LoadOffersFromJson", "QuoteOfferPrice", "TryPurchaseOffer"):
        if symbol not in economy_source:
            fail(f"MHEconomySubsystem.cpp is missing: {symbol}")

    collectible_subsystem = read_text("Source/MountHope/MHCollectibleSubsystem.cpp")
    if "GetSetProgress" not in collectible_subsystem or "EvaluateSetBonuses" not in collectible_subsystem:
        fail("MHCollectibleSubsystem.cpp is missing collectible-set support")

    radio_source = read_text("Source/MountHope/MHRadioSubsystem.cpp")
    for symbol in ("TuneToStation", "NextStation", "LoadStationsFromJson"):
        if symbol not in radio_source:
            fail(f"MHRadioSubsystem.cpp is missing: {symbol}")

    if "RequestRadioNextStation" not in player_source:
        fail("MHPlayerCharacter.cpp does not wire radio station cycling")

    if "HandleStationChanged" not in hud_source or "SetWantedText" not in hud_source:
        fail("MHGameHudWidget.cpp does not surface wanted level / radio state")

    if "TickStamina" not in player_source or "bSprintRequested" not in player_source:
        fail("MHPlayerCharacter.cpp does not implement the stamina system")

    game_state_source = read_text("Source/MountHope/MHGameStateSubsystem.cpp")
    for symbol in ("OnPlayerWasted", "OnPlayerBusted", "TriggerBusted", "Heal"):
        if symbol not in game_state_source:
            fail(f"MHGameStateSubsystem.cpp is missing consequence-loop support: {symbol}")

    if "TickBustedTimer" not in game_mode_source or "HandlePlayerWasted" not in game_mode_source:
        fail("MHGameModeBase.cpp does not wire the busted/wasted consequence loop")

    time_of_day_source = read_text("Source/MountHope/MHTimeOfDaySubsystem.cpp")
    if "AdvanceTime" not in time_of_day_source:
        fail("MHTimeOfDaySubsystem.cpp is missing AdvanceTime")
    if "MHTimeOfDaySubsystem" not in game_mode_source:
        fail("MHGameModeBase.cpp does not tick the time-of-day subsystem")

    pickup_source = read_text("Source/MountHope/MHHealthPickupActor.cpp")
    if "OnPickupOverlap" not in pickup_source or "Heal" not in pickup_source:
        fail("MHHealthPickupActor.cpp does not heal the player on overlap")

    pedestrian_source = read_text("Source/MountHope/MHPedestrianCharacter.cpp")
    for symbol in ("PickNewWanderTarget", "FindNearestThreat", "FleeFromThreat"):
        if symbol not in pedestrian_source:
            fail(f"MHPedestrianCharacter.cpp is missing: {symbol}")

    if "MHPedestrianCharacter" not in vehicle_source:
        fail("MHVehiclePawn.cpp does not distinguish pedestrian hits (Assault)")

    for dependency in ("AIModule", "NavigationSystem"):
        if dependency not in build_source:
            fail(f"MountHope.Build.cs is missing dependency: {dependency}")

    if "MissionCompleteSound" not in game_mode_source or "PlaySound2D" not in game_mode_source:
        fail("MHGameModeBase.cpp does not wire mission/consequence audio cues")
    if "PickupSound" not in collectible_source:
        fail("MHCollectibleActor.cpp does not wire a pickup sound cue")
    if "TransactionSuccessSound" not in shop_source or "TransactionDeniedSound" not in shop_source:
        fail("MHShopActor.cpp does not wire transaction sound cues")
    if "HealSound" not in pickup_source:
        fail("MHHealthPickupActor.cpp does not wire a heal sound cue")
    if "WantedIncreaseSound" not in wanted_source:
        fail("MHWantedSubsystem.cpp does not wire a wanted-increase sound cue")

    if "HandleMissionCompleted" not in hud_source or "ToastTextBlock" not in hud_source:
        fail("MHGameHudWidget.cpp does not display mission-completion toasts")
    if "HandleMissionFailed" not in hud_source or "OnMissionFailed" not in hud_source:
        fail("MHGameHudWidget.cpp does not surface mission-failure toasts")
    if "RefreshMinimap" not in hud_source or "MinimapImage" not in hud_source:
        fail("MHGameHudWidget.cpp does not wire the minimap")

    minimap_source = read_text("Source/MountHope/MHMinimapCaptureActor.cpp")
    if "TextureTarget" not in minimap_source or "USceneCaptureComponent2D" not in minimap_source:
        fail("MHMinimapCaptureActor.cpp does not set up a scene capture render target")

    if "AMHMinimapCaptureActor" not in game_mode_source:
        fail("MHGameModeBase.cpp does not spawn the minimap capture actor")
    if "AMHPedestrianSpawnerActor" not in game_mode_source:
        fail("MHGameModeBase.cpp does not spawn the pedestrian spawner")

    spawner_source = read_text("Source/MountHope/MHPedestrianSpawnerActor.cpp")
    for symbol in ("SpawnIfNeeded", "DespawnFarPedestrians", "GetRandomReachablePointInRadius"):
        if symbol not in spawner_source:
            fail(f"MHPedestrianSpawnerActor.cpp is missing: {symbol}")

    weapon_pickup_source = read_text("Source/MountHope/MHWeaponPickupActor.cpp")
    if "PickUpPistol" not in weapon_pickup_source:
        fail("MHWeaponPickupActor.cpp does not grant the pistol on interact")

    if "FirePistol" not in player_source or "PickUpPistol" not in player_source:
        fail("MHPlayerCharacter.cpp does not implement the pistol combat loop")
    if "LineTraceSingleByChannel" not in player_source:
        fail("MHPlayerCharacter.cpp does not raycast for pistol hit detection")
    if "AMHPoliceUnitPawn" not in player_source:
        fail("MHPlayerCharacter.cpp pistol does not damage police units")

    if "AMHPoliceSpawnerActor" not in game_mode_source:
        fail("MHGameModeBase.cpp does not spawn the police pursuit spawner")

    police_spawner_source = read_text("Source/MountHope/MHPoliceSpawnerActor.cpp")
    if "GetDesiredUnitCount" not in police_spawner_source or "GetWantedLevel" not in police_spawner_source:
        fail("MHPoliceSpawnerActor.cpp does not scale pursuit units with wanted level")
    for symbol in ("GetTierForWantedLevel", "ConfigureForTier"):
        if symbol not in police_spawner_source:
            fail(f"MHPoliceSpawnerActor.cpp is missing wanted-tier escalation: {symbol}")

    police_unit_source = read_text("Source/MountHope/MHPoliceUnitPawn.cpp")
    if "TryCatchPlayer" not in police_unit_source or "ApplyDamage" not in police_unit_source:
        fail("MHPoliceUnitPawn.cpp does not chase and apply catch pressure")
    for symbol in ("TryShootPlayer", "ConfigureForTier", "IsDead", "EMHPoliceState"):
        if symbol not in police_unit_source:
            fail(f"MHPoliceUnitPawn.cpp is missing combat-AI support: {symbol}")

    if "AddArmor" not in game_state_source or "Armor" not in game_state_source:
        fail("MHGameStateSubsystem.cpp is missing the armor damage-soak system")


def validate_default_config() -> None:
    default_game = read_text("Config/DefaultGame.ini")
    if "GameInstanceClass=/Script/MountHope.MHGameInstance" not in default_game:
        fail("DefaultGame.ini does not wire MHGameInstance")
    if "GlobalDefaultGameMode=/Script/MountHope.MHGameModeBase" not in default_game:
        fail("DefaultGame.ini does not wire MHGameModeBase")


def validate_missions() -> None:
    data = load_json("Data/Missions/vertical_slice.json")
    missions = data.get("missions", [])
    if not missions:
        fail("No missions defined in vertical_slice.json")

    titles = []
    for mission in missions:
        if not mission.get("title"):
            fail("Mission title is required")
        titles.append(mission["title"])
        steps = mission.get("steps", [])
        if not steps:
            fail(f"Mission {mission.get('title')} has no steps")
        for step in steps:
            for field in ("text", "needVehicle", "reward"):
                if field not in step:
                    fail(f"Mission step missing field: {field}")
            if step.get("crime") and "crimeSeverity" not in step:
                fail(f"Mission step '{step.get('text')}' flags crime without crimeSeverity")
            if step.get("weatherOnStart") not in (None, "Clear", "DenseFog", "CoastalRain", "Noreaster"):
                fail(f"Mission step '{step.get('text')}' has an unknown weatherOnStart value")
            objective_type = step.get("objectiveType")
            if objective_type not in (None, "reach", "timed", "survive", "talk", "pay", "collect"):
                fail(f"Mission step '{step.get('text')}' has an unknown objectiveType: {objective_type}")
            if objective_type in ("timed", "survive") and not step.get("timeLimitSeconds", 0) > 0:
                fail(f"Mission step '{step.get('text')}' is {objective_type} but has no positive timeLimitSeconds")
            if objective_type == "talk" and not step.get("conversationId"):
                fail(f"Talk step '{step.get('text')}' is missing conversationId")
            if objective_type == "pay" and (not step.get("payOfferId") or not step.get("payAmount", 0) > 0):
                fail(f"Pay step '{step.get('text')}' needs payOfferId and a positive payAmount")
            if objective_type == "collect":
                has_set = bool(step.get("collectSet"))
                has_item = bool(step.get("collectItem"))
                if has_set == has_item:
                    fail(f"Collect step '{step.get('text')}' needs exactly one of collectSet or collectItem")
                if not step.get("collectCount", 0) > 0 and has_set:
                    fail(f"Collect step '{step.get('text')}' needs a positive collectCount")

    seen_ids: set[str] = set()
    for mission in missions:
        mission_id = mission.get("id")
        if not mission_id:
            fail(f"Mission '{mission.get('title')}' is missing id")
        elif mission_id in seen_ids:
            fail(f"Duplicate mission id: {mission_id}")
        else:
            seen_ids.add(mission_id)

    for required_title in ("Off the Boat", "Gloria", "Big Mamie"):
        if required_title not in titles:
            fail(f"Canonical campaign is missing story mission: {required_title}")

    if len(missions) < 10:
        fail("Canonical campaign should span the full Acts I-III + Gloria finale (10+ missions)")


def validate_collectibles() -> None:
    data = load_json("Data/Collectibles/vertical_slice.json")
    collectibles = data.get("collectibles", [])
    if not collectibles:
        fail("No collectibles defined in Data/Collectibles/vertical_slice.json")
    seen: set[str] = set()
    for item in collectibles:
        item_id = item.get("id")
        if not item_id:
            fail("Collectible missing id")
        if item_id in seen:
            fail(f"Duplicate collectible id: {item_id}")
        seen.add(item_id)
        if not item.get("name"):
            fail(f"Collectible {item_id} missing name")
        if item.get("cashReward", 0) < 0:
            fail(f"Invalid cashReward for {item_id}")

    sets = data.get("sets", [])
    if not sets:
        fail("No collectible sets defined in Data/Collectibles/vertical_slice.json")
    seen_sets: set[str] = set()
    for group in sets:
        set_id = group.get("id")
        if not set_id:
            fail("Collectible set missing id")
        if set_id in seen_sets:
            fail(f"Duplicate collectible set id: {set_id}")
        seen_sets.add(set_id)
        if not group.get("name"):
            fail(f"Collectible set {set_id} missing name")
        members = group.get("members") or []
        if not members:
            fail(f"Collectible set {set_id} has no members")
        if len(members) != len(set(members)):
            fail(f"Collectible set {set_id} has duplicate members")
        if group.get("bonusCash", 0) < 0:
            fail(f"Collectible set {set_id} has a negative bonus")
        for member in members:
            if member not in seen:
                fail(f"Collectible set {set_id} references unknown item {member}")


def validate_radio_stations() -> None:
    data = load_json("Data/Radio/stations.json")
    stations = data.get("stations", [])
    if not stations:
        fail("No stations defined in Data/Radio/stations.json")
    seen: set[str] = set()
    for station in stations:
        station_id = station.get("id")
        if not station_id:
            fail("Radio station missing id")
        if station_id in seen:
            fail(f"Duplicate radio station id: {station_id}")
        seen.add(station_id)
        if not station.get("name"):
            fail(f"Radio station {station_id} missing name")
        if not station.get("dj"):
            fail(f"Radio station {station_id} missing dj")


def validate_gameplay_tags() -> None:
    tags_text = read_text("Config/DefaultGameplayTags.ini")
    for required_tag in (
        "Faction.Crew.Dockside",
        "Faction.Crew.FallRiver",
        "Faction.Sports.Boxing",
        "Faction.Elite.CapeCod",
        "Faction.Business.Garage",
    ):
        if required_tag not in tags_text:
            fail(f"DefaultGameplayTags.ini is missing required faction tag: {required_tag}")


def validate_dialogue() -> None:
    data = load_json("Data/Dialogue/vertical_slice.json")
    conversations = data.get("conversations", [])
    if not conversations:
        fail("No conversations defined in dialogue vertical_slice.json")
    total_lines = 0
    for conversation in conversations:
        if not conversation.get("id"):
            fail("Dialogue conversation missing id")
        if not conversation.get("speaker"):
            fail("Dialogue conversation missing speaker")
        lines = conversation.get("lines", [])
        if not lines:
            fail(f"Dialogue {conversation.get('id')} has no lines")
        total_lines += len(lines)
        for line in lines:
            if isinstance(line, str):
                continue
            if not isinstance(line, dict) or not line.get("text"):
                fail(f"Dialogue {conversation.get('id')} has a line without text")
            choices = line.get("choices") or []
            if choices and line.get("endsConversation"):
                fail(f"Dialogue {conversation.get('id')} line both offers choices and ends the conversation")
            for choice in choices:
                if not choice.get("text"):
                    fail(f"Dialogue {conversation.get('id')} has a choice without text")
                if choice.get("end"):
                    continue
                next_line = choice.get("nextLine")
                if not isinstance(next_line, int) or next_line < 0 or next_line >= len(lines):
                    fail(f"Dialogue {conversation.get('id')} choice '{choice.get('text')}' has nextLine out of range")
    if total_lines < 3:
        fail("Vertical slice requires at least three authored dialogue lines")


def validate_economy() -> None:
    data = load_json("Data/Economy/businesses.json")
    businesses = data.get("businesses", [])
    if not businesses:
        fail("No businesses defined in businesses.json")
    seen: set[str] = set()
    for business in businesses:
        bid = business.get("id")
        if not bid:
            fail("Business missing id")
        if bid in seen:
            fail(f"Duplicate business id: {bid}")
        seen.add(bid)
        if business.get("cost", 0) < 0:
            fail(f"Invalid cost for {bid}")
        if business.get("dailyIncome", 0) < 0:
            fail(f"Invalid dailyIncome for {bid}")

    offers = data.get("offers", [])
    if not offers:
        fail("No offers defined in businesses.json")
    seen_offers: set[str] = set()
    for offer in offers:
        offer_id = offer.get("id")
        if not offer_id:
            fail("Economy offer missing id")
        if offer_id in seen_offers:
            fail(f"Duplicate economy offer id: {offer_id}")
        seen_offers.add(offer_id)
        if not offer.get("name"):
            fail(f"Economy offer {offer_id} missing name")
        if offer.get("price", -1) < 0:
            fail(f"Economy offer {offer_id} has an invalid price")
        if not offer.get("category"):
            fail(f"Economy offer {offer_id} missing category")


AUTOMATION_TESTS = [
    "Source/MountHope/Tests/MHWantedSubsystemTest.cpp",
    "Source/MountHope/Tests/MHMissionSubsystemTest.cpp",
    "Source/MountHope/Tests/MHGameStateSubsystemTest.cpp",
    "Source/MountHope/Tests/MHTimeOfDaySubsystemTest.cpp",
    "Source/MountHope/Tests/MHReputationSubsystemTest.cpp",
    "Source/MountHope/Tests/MHPoliceTest.cpp",
    "Source/MountHope/Tests/MHDialogueSubsystemTest.cpp",
    "Source/MountHope/Tests/MHEconomySubsystemTest.cpp",
    "Source/MountHope/Tests/MHCollectibleSubsystemTest.cpp",
]


def validate_automation_tests() -> None:
    for relative_path in AUTOMATION_TESTS:
        text = read_text(relative_path)
        if "IMPLEMENT_SIMPLE_AUTOMATION_TEST" not in text:
            fail(f"{relative_path} declares no automation test")
        if "WITH_DEV_AUTOMATION_TESTS" not in text:
            fail(f"{relative_path} is not guarded by WITH_DEV_AUTOMATION_TESTS")
        if "::RunTest" not in text:
            fail(f"{relative_path} has no RunTest body")


def validate_existing_data_links() -> None:
    for relative_path in EXPECTED_EXISTING_DATA:
        if not (REPO_ROOT / relative_path).exists():
            fail(f"referenced existing data is missing: {relative_path}")


def validate_content_references() -> None:
    """Cross-check mission objectives against dialogue, offers, and collectible sets."""
    missions = load_json("Data/Missions/vertical_slice.json").get("missions", [])
    conversations = {
        conversation["id"]: conversation
        for conversation in load_json("Data/Dialogue/vertical_slice.json").get("conversations", [])
    }
    offers = {
        offer["id"]: offer
        for offer in load_json("Data/Economy/businesses.json").get("offers", [])
    }
    sets = {
        group["id"]: group
        for group in load_json("Data/Collectibles/vertical_slice.json").get("sets", [])
    }
    items = {
        item["id"]
        for item in load_json("Data/Collectibles/vertical_slice.json").get("collectibles", [])
    }

    for mission in missions:
        for step in mission.get("steps", []):
            kind = step.get("objectiveType")
            label = step.get("text")
            if kind == "talk":
                conversation = conversations.get(step.get("conversationId"))
                if conversation is None:
                    fail(f"Talk step '{label}' references unknown conversation '{step.get('conversationId')}'")
                elif not conversation.get("onComplete", {}).get("completeObjective"):
                    fail(f"Talk step '{label}' conversation must set onComplete.completeObjective")
            elif kind == "pay":
                offer = offers.get(step.get("payOfferId"))
                if offer is None:
                    fail(f"Pay step '{label}' references unknown offer '{step.get('payOfferId')}'")
                elif offer.get("price") != step.get("payAmount"):
                    fail(f"Pay step '{label}' payAmount does not match offer price")
            elif kind == "collect":
                if step.get("collectItem") and step["collectItem"] not in items:
                    fail(f"Collect step '{label}' references unknown item '{step['collectItem']}'")
                set_id = step.get("collectSet")
                if set_id:
                    group = sets.get(set_id)
                    if group is None:
                        fail(f"Collect step '{label}' references unknown set '{set_id}'")
                    elif step.get("collectCount", 0) > len(group.get("members") or []):
                        fail(f"Collect step '{label}' collectCount exceeds set size")


def validate_osm_manifest() -> None:
    script = PROJECT_ROOT / "Scripts" / "prepare_osm_import.py"
    completed = subprocess.run(
        [sys.executable, str(script), "--check"],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "OSM manifest check failed").strip()
        fail(detail)


def main() -> None:
    validate_uproject()
    validate_required_files()
    validate_target_rules()
    validate_no_deprecated_macros()
    validate_source_contract()
    validate_default_config()
    validate_missions()
    validate_dialogue()
    validate_economy()
    validate_collectibles()
    validate_radio_stations()
    validate_gameplay_tags()
    validate_automation_tests()
    validate_existing_data_links()
    validate_content_references()
    validate_osm_manifest()
    print("MountHope_Unreal scaffold validation passed")


if __name__ == "__main__":
    main()
