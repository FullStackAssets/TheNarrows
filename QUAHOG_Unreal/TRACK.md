# QUAHOG_Unreal — active Unreal track

Parallel Unreal Engine 5.8 project for the same South Coast crime game. It is
in active development alongside `MountHope_Unreal/`, `QUAHOG_Web/`,
`QUAHOG_Unity/`, `QUAHOG_Godot/`, and `QUAHOG_GODOT1/`. See [`../ENGINES.md`](../ENGINES.md).

This sandbox does not have Unreal Editor, so this tree was not compiled, cooked,
or played here. A green editor build is not claimed.

## What this tree is

An earlier UE 5.8 bootstrap (`MountHope.uproject`, `EngineAssociation` 5.8)
that first explored the JSON-driven mission, economy, and world-slice
framework. Those patterns also live in `MountHope_Unreal/`, which is the
PC/console vertical slice with the expanded campaign and gameplay subsystems.
Both trees stay available. History of the merge into the Mount Hope slice is
in `MountHope_Unreal/Docs/IMPROVEMENT_PLAN.md`.

Class names differ on purpose: this bootstrap used `MountHopeCharacter`; the
Mount Hope slice uses `MHPlayerCharacter`.

## Useful entry points

| Need | Where |
| --- | --- |
| This project | `MountHope.uproject` |
| Bootstrap check that does not need the editor | `python3 QUAHOG_Unreal/Tools/validate_bootstrap.py` |
| PC/console vertical slice | `../MountHope_Unreal/MountHope.uproject` |
| Slice checks without the editor | `python3 MountHope_Unreal/Scripts/validate_scaffold.py` |
| Browser track | `../QUAHOG_Web/` |

## Older path

[`LEGACY.md`](LEGACY.md) only redirects here so existing links keep working.
