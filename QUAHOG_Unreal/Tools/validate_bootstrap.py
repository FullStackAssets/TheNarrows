#!/usr/bin/env python3
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


EXPECTED_ENGINE = "5.8"


def validate_uproject():
    project = load_json(ROOT / "MountHope.uproject")
    module_names = {m["Name"] for m in project.get("Modules", [])}
    assert "MountHope" in module_names, "MountHope module missing in .uproject"
    plugins = {p["Name"] for p in project.get("Plugins", [])}
    assert "EnhancedInput" in plugins, "EnhancedInput plugin missing"
    assert "ChaosVehiclesPlugin" in plugins, "ChaosVehiclesPlugin missing"
    assert "WorldPartitionHLODUtilities" in plugins, "WorldPartitionHLODUtilities missing"
    assert project.get("EngineAssociation") == EXPECTED_ENGINE, (
        f"EngineAssociation must be {EXPECTED_ENGINE}, got "
        f"{project.get('EngineAssociation')!r}"
    )


def validate_target_rules():
    """The target rules must track the installed engine, not a stale pin."""
    for target in ("MountHope.Target.cs", "MountHopeEditor.Target.cs"):
        text = (ROOT / "Source" / target).read_text(encoding="utf-8")
        assert "EngineIncludeOrderVersion.Unreal5_4" not in text, (
            f"{target} still pins the legacy Unreal5_4 include order"
        )
        assert "DefaultBuildSettings" in text, f"{target} missing DefaultBuildSettings"
        assert "IncludeOrderVersion" in text, f"{target} missing IncludeOrderVersion"


def validate_no_deprecated_macros():
    """Guard against engine macros removed/deprecated on the way to 5.8."""
    src_root = ROOT / "Source"
    for path in src_root.rglob("*.cpp"):
        text = path.read_text(encoding="utf-8")
        for line in text.splitlines():
            # The bare KINDA_SMALL_NUMBER family is deprecated in favour of the
            # UE_-prefixed constants. Match the bare token, not UE_KINDA_...
            assert "UE_KINDA_SMALL_NUMBER" in line or "KINDA_SMALL_NUMBER" not in line, (
                f"{path.name}: use UE_KINDA_SMALL_NUMBER instead of KINDA_SMALL_NUMBER"
            )


def validate_expected_code_files():
    expected = [
        ROOT / "Source" / "MountHope" / "Public" / "MountHopeCharacter.h",
        ROOT / "Source" / "MountHope" / "Private" / "MountHopeCharacter.cpp",
        ROOT / "Source" / "MountHope" / "Public" / "MountHopeVehiclePawn.h",
        ROOT / "Source" / "MountHope" / "Private" / "MountHopeVehiclePawn.cpp",
        ROOT / "Source" / "MountHope" / "Public" / "MountHopeGameMode.h",
        ROOT / "Source" / "MountHope" / "Private" / "MountHopeGameMode.cpp",
        ROOT / "Source" / "MountHope" / "Public" / "MHMissionTriggerActor.h",
        ROOT / "Source" / "MountHope" / "Private" / "MHMissionTriggerActor.cpp",
        ROOT / "Source" / "MountHope" / "Public" / "MHSaveGame.h",
    ]
    missing = [str(path.relative_to(ROOT)) for path in expected if not path.exists()]
    assert not missing, f"Missing expected Unreal framework files: {missing}"


def validate_missions():
    data = load_json(ROOT / "Data" / "Missions" / "vertical_slice.json")
    missions = data.get("missions", [])
    assert missions, "No missions defined"
    for mission in missions:
        assert mission.get("title"), "Mission title is required"
        steps = mission.get("steps", [])
        assert steps, f"Mission {mission.get('title')} has no steps"
        for step in steps:
            assert "text" in step, "Mission step missing text"
            assert "needVehicle" in step, "Mission step missing needVehicle"
            assert "reward" in step, "Mission step missing reward"


def validate_economy():
    data = load_json(ROOT / "Data" / "Economy" / "businesses.json")
    businesses = data.get("businesses", [])
    assert businesses, "No businesses defined"
    seen = set()
    for b in businesses:
        bid = b.get("id")
        assert bid, "Business missing id"
        assert bid not in seen, f"Duplicate business id: {bid}"
        seen.add(bid)
        assert b.get("cost", 0) >= 0, f"Invalid cost for {bid}"
        assert b.get("dailyIncome", 0) >= 0, f"Invalid dailyIncome for {bid}"


def validate_default_config():
    default_game = (ROOT / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
    assert "GameInstanceClass=/Script/MountHope.MountHopeGameInstance" in default_game
    assert "GlobalDefaultGameMode=/Script/MountHope.MountHopeGameMode" in default_game
    assert "SaveSlotName=QuahogUnrealSlot" in default_game
    assert "SlicePath=../QUAHOG_Web/public/slice-newbedford.json" in default_game
    assert "[/Script/MountHope.MountHopeGameInstance]" in default_game


def validate_framework_wiring():
    char_cpp = (ROOT / "Source" / "MountHope" / "Private" / "MountHopeCharacter.cpp").read_text(encoding="utf-8")
    mode_cpp = (ROOT / "Source" / "MountHope" / "Private" / "MountHopeGameMode.cpp").read_text(encoding="utf-8")
    trigger_cpp = (ROOT / "Source" / "MountHope" / "Private" / "MHMissionTriggerActor.cpp").read_text(encoding="utf-8")

    assert "SetupPlayerInputComponent" in char_cpp, "Character input binding missing"
    assert "TryCompleteVehicleObjective" in mode_cpp, "Vehicle objective helper missing"
    assert "RefreshObjectiveTrigger" in mode_cpp, "Objective trigger refresh missing"
    assert "ResolveSaveSlotName" in mode_cpp, "Save slot must come from the game instance"
    assert "SetTriggerRadius" in trigger_cpp, "Trigger radius setter missing"
    assert "ResetConsumed" in trigger_cpp, "Trigger consumed reset missing"


def validate_slice_loader_source():
    """Source-text checks for the loader rules. This does not execute them."""
    loader = (ROOT / "Source" / "MountHope" / "Private" / "MHWorldSliceSubsystem.cpp").read_text(encoding="utf-8")
    assert "bool Slice2DToWorld" in loader, "Slice2DToWorld helper missing"
    assert "-NorthMeters" in loader, "2D north axis must map to -Z"
    assert "TryGetStringField" in loader, "null road names must not use GetStringField"
    assert "buildingsStreamed" in loader, "streamed-building note missing"
    assert "FMHWaterPolygon" in loader, "water rings are not loaded"

    slot = "QuahogUnrealSlot"
    other_slot = "MountHope" + "Slot"
    hits = []
    for path in (ROOT / "Source").rglob("*"):
        if path.suffix not in {".h", ".cpp", ".cs"}:
            continue
        text = path.read_text(encoding="utf-8")
        if other_slot in text:
            hits.append(str(path.relative_to(ROOT)))
    assert not hits, f"{other_slot} must not be the save slot in this track: {hits}"
    instance = (ROOT / "Source" / "MountHope" / "Public" / "MountHopeGameInstance.h").read_text(encoding="utf-8")
    game_state = (ROOT / "Source" / "MountHope" / "Public" / "MHGameStateSubsystem.h").read_text(encoding="utf-8")
    assert slot in instance and slot in game_state


def validate_data_contract():
    tools = Path(__file__).resolve().parent
    sys.path.insert(0, str(tools))
    from slice_contract import validate_committed_slice

    return validate_committed_slice()


if __name__ == "__main__":
    validate_uproject()
    validate_target_rules()
    validate_no_deprecated_macros()
    validate_expected_code_files()
    validate_missions()
    validate_economy()
    validate_default_config()
    validate_framework_wiring()
    validate_slice_loader_source()
    summary = validate_data_contract()
    print("QUAHOG_Unreal bootstrap validation passed (UE 5.8 target, source and data only).")
    print(
        f"data: roads={summary['roads']} landmarks={summary['landmarks']} "
        f"water_rings={summary['water_rings']} buildings={summary['buildings']}"
    )
    print(
        "NOTE: active development check. Unreal Editor, UnrealBuildTool, "
        "automation tests, PIE, and packaging were not run.",
        file=sys.stderr,
    )
