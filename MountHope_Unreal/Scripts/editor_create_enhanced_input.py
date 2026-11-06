#!/usr/bin/env python3
"""
Create Enhanced Input assets for Mount Hope.

Run inside Unreal Editor 5.8 (Python Editor Script Plugin):
  Tools → Execute Python Script → select this file

Creates /Game/Input/IMC_Default plus IA_Move, IA_Look, IA_Sprint, IA_Interact,
IA_EnterExitVehicle, IA_DialogueChoice1, and IA_DialogueChoice2 with WASD,
mouse, E/F, and 1/2 bindings.

Assign IMC_Default and the Input Actions on BP_MHPlayerCharacter (or the C++
defaults will pick them up if set on the class defaults in editor).
"""

from __future__ import annotations

try:
    import unreal
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "This script must run inside the Unreal Editor Python environment."
    ) from exc


INPUT_DIR = "/Game/Input"
IMC_PATH = f"{INPUT_DIR}/IMC_Default"
ACTION_SPECS = (
    ("IA_Move", unreal.InputActionValueType.AXIS2D),
    ("IA_Look", unreal.InputActionValueType.AXIS2D),
    ("IA_Sprint", unreal.InputActionValueType.BOOLEAN),
    ("IA_Interact", unreal.InputActionValueType.BOOLEAN),
    ("IA_EnterExitVehicle", unreal.InputActionValueType.BOOLEAN),
    ("IA_DialogueChoice1", unreal.InputActionValueType.BOOLEAN),
    ("IA_DialogueChoice2", unreal.InputActionValueType.BOOLEAN),
)


def log(message: str) -> None:
    unreal.log(f"[MountHope Input] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[MountHope Input] {message}")


def ensure_input_folder() -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(INPUT_DIR):
        unreal.EditorAssetLibrary.make_directory(INPUT_DIR)
        log(f"Created {INPUT_DIR}")


def load_or_create_action(name: str, value_type: unreal.InputActionValueType) -> unreal.InputAction:
    path = f"{INPUT_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        action = unreal.EditorAssetLibrary.load_asset(path)
        log(f"Loaded existing {name}")
        return action

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    # The Python-exposed factory is named "InputAction_Factory" (underscore); fall
    # back to a null factory (works for these simple data assets) if it is not
    # present in this engine build.
    factory_cls = getattr(unreal, "InputAction_Factory", None)
    factory = factory_cls() if factory_cls is not None else None
    action = asset_tools.create_asset(name, INPUT_DIR, unreal.InputAction, factory)
    if action is None:
        raise RuntimeError(f"Failed to create Input Action: {name}")

    action.set_editor_property("value_type", value_type)
    unreal.EditorAssetLibrary.save_loaded_asset(action)
    log(f"Created {name}")
    return action


def load_or_create_imc() -> unreal.InputMappingContext:
    if unreal.EditorAssetLibrary.does_asset_exist(IMC_PATH):
        imc = unreal.EditorAssetLibrary.load_asset(IMC_PATH)
        log("Loaded existing IMC_Default")
        return imc

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory_cls = getattr(unreal, "InputMappingContext_Factory", None)
    factory = factory_cls() if factory_cls is not None else None
    imc = asset_tools.create_asset("IMC_Default", INPUT_DIR, unreal.InputMappingContext, factory)
    if imc is None:
        raise RuntimeError("Failed to create Input Mapping Context")

    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    log("Created IMC_Default")
    return imc


def _key(name: str) -> unreal.Key:
    return unreal.Key(name)



def configure_mappings(
    imc: unreal.InputMappingContext,
    actions: dict[str, unreal.InputAction],
) -> None:
    move = actions["IA_Move"]
    look = actions["IA_Look"]
    sprint = actions["IA_Sprint"]
    interact = actions["IA_Interact"]
    vehicle = actions["IA_EnterExitVehicle"]
    choice1 = actions["IA_DialogueChoice1"]
    choice2 = actions["IA_DialogueChoice2"]

    imc.map_key(move, _key("W"))
    imc.map_key(move, _key("S"), [unreal.InputModifierNegate()])
    imc.map_key(move, _key("D"))
    imc.map_key(move, _key("A"), [unreal.InputModifierNegate()])

    imc.map_key(look, _key("Mouse2D"))

    imc.map_key(sprint, _key("LeftShift"))
    imc.map_key(interact, _key("E"))
    imc.map_key(vehicle, _key("F"))
    imc.map_key(choice1, _key("One"))
    imc.map_key(choice2, _key("Two"))

    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    log("Configured key mappings on IMC_Default")


def assign_to_character_defaults(actions: dict[str, unreal.InputAction], imc: unreal.InputMappingContext) -> None:
    character_class = unreal.load_class(None, "/Script/MountHope.MHPlayerCharacter")
    if character_class is None:
        warn("MHPlayerCharacter not loaded — compile C++ first, then re-run.")
        return

    cdo = unreal.get_default_object(character_class)
    property_map = {
        "DefaultMappingContext": imc,
        "IA_Move": actions["IA_Move"],
        "IA_Look": actions["IA_Look"],
        "IA_Sprint": actions["IA_Sprint"],
        "IA_Interact": actions["IA_Interact"],
        "IA_EnterExitVehicle": actions["IA_EnterExitVehicle"],
        "IA_DialogueChoice1": actions["IA_DialogueChoice1"],
        "IA_DialogueChoice2": actions["IA_DialogueChoice2"],
    }
    for prop_name, value in property_map.items():
        try:
            cdo.set_editor_property(prop_name, value)
        except Exception:
            warn(f"Could not set {prop_name} on MHPlayerCharacter CDO — assign manually on Blueprint.")

    log("Assigned input assets to MHPlayerCharacter class defaults where supported")


def main() -> None:
    log("Creating Enhanced Input assets...")
    ensure_input_folder()

    actions: dict[str, unreal.InputAction] = {}
    for name, value_type in ACTION_SPECS:
        actions[name] = load_or_create_action(name, value_type)

    imc = load_or_create_imc()
    configure_mappings(imc, actions)
    assign_to_character_defaults(actions, imc)

    unreal.EditorAssetLibrary.save_directory(INPUT_DIR)
    warn(
        "If duplicate mappings appear after re-runs, delete IMC_Default and re-run.\n"
        "Assign IMC_Default + Input Actions on BP_MHPlayerCharacter if using a Blueprint child."
    )
    log("Enhanced Input setup complete.")


if __name__ == "__main__":
    main()
