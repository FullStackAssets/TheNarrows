# Project QUAHOG — Godot 4 Setup Guide

**Status:** active GDScript track for The Narrows. **Not a production build.**
`LEGACY.md` in this folder is updated by the MountHope agent; it may still say legacy until that lands. Track status for this directory is [`README.md`](README.md).

**Engine:** Godot 4.2+ (Forward+ renderer)
**Language:** GDScript
**Era:** 2026. Coastal neon is a lighting mood, not a 1986 period lock.
**Export and map contract:** [`EXPORT.md`](EXPORT.md), [`OSM_CONTRACT.md`](OSM_CONTRACT.md)

---

## Quick Start

### 1. Install Godot
Download **Godot 4.2 or later** from [godotengine.org](https://godotengine.org/download).
- Use the **standard build** (not .NET/C# version — this project is pure GDScript)

### 2. Open the project
1. Launch Godot → **Import**
2. Point it at the `QUAHOG_Godot/` folder (where `project.godot` lives)
3. Let the engine import and index all scripts (takes ~10s on first open)

### 3. Press Play — the playable slice
The main scene is now `res://scenes/world/Game.tscn`. It holds a single `Node3D`
running `res://scripts/world/GameWorld.gd`, which assembles the whole playable
world **in code** on `_ready()`:

- a `WorldEnvironment` (procedural sky + filmic tonemap), wired into `WeatherController`
- a directional **sun** with shadows
- an **infinite ground collider** + a large ground plane (so you can walk)
- the **real South Coast road mesh** baked from `data/southcoast-roads.json` via `MapMeshBaker` (byte copy of the shared bake; axis `z = north`)
- a spawned **PlayerController** (capsule + first-person camera) on the first **named** road in that file
- weather particles, highway neon, a street-name banner, and one contact mission (`StreetLoop.gd`)

Hit **F5** (or the Play button). In the **Output** panel the scripts print autoload
boot lines plus a map bake, a warning that road-graph `city` is null, and:
```
[GameWorld] Ready — player spawned at (…)
```
A headless Godot 4.2.2 dummy-renderer boot printed that line (see [`EXPORT.md`](EXPORT.md)).
A windowed editor pass and a device launch were not done.

The mouse is captured — **WASD** to walk, **mouse** to look, **Shift** to sprint,
**Space** to jump, **Q** to take cover, **E** on the contact to finish the opener,
**Esc** to pause. The banner is the OSM street name (and `highway` if the road is
unnamed). It does not invent a municipality. Dusk is the start hour so the neon
read is visible; the sun curve is art direction, not a solar model.

> **Note on the old bootstrap:** `scenes/bootstrap/Bootstrap.tscn` (singleton-validation
> + prologue fog) is still in the repo as the headless boot harness, but it is no longer
> the main scene. `GameWorld.gd` performs the equivalent autoload wiring itself.

> **Camera feel:** if look up/down is inverted for you, set `orbit_sensitivity_y` negative
> on the Player, or tweak the first-person rig in `GameWorld._spawn_player()`.

---

## Project Structure

```
QUAHOG_Godot/
│
├── project.godot              ← Godot project config, input map, autoloads
│
├── autoloads/                 ← 13 global singletons (auto-initialized)
│   ├── GameManager.gd         ← State machine: Boot/MainMenu/Playing/Paused/Cutscene
│   ├── PlayerWallet.gd        ← Balance, deduct, add — fires balance_changed signal
│   ├── SaveLoadManager.gd     ← 5-slot JSON saves with SHA-256 checksum
│   ├── InputManager.gd        ← Unified input, dead-zone, KB+mouse↔gamepad detection
│   ├── TimeOfDayClock.gd      ← 24h cycle, hour/midnight/day signals
│   ├── WeatherController.gd   ← 4-state FSM: CLEAR/DENSE_FOG/COASTAL_RAIN/NOREASTER
│   ├── HeatManager.gd         ← Wanted level (0-5) + per-faction aggro (0-100)
│   ├── SafehouseManager.gd    ← 4 safehouses, sleep/save/heat-clear flow
│   ├── RadioManager.gd        ← 5 stations (WMILL/RADIO_ATLANTICO/WRGE/WYAC/OFF)
│   ├── RevenueManager.gd      ← Daily property yields at midnight
│   ├── MissionManager.gd      ← 22 missions across 7 strands, prerequisite graph
│   ├── GloriaEventDirector.gd ← 5-phase hurricane event sequence
│   └── HUDManager.gd          ← CanvasLayer HUD coordinator
│
├── scripts/
│   ├── core/
│   │   ├── GameConstants.gd   ← All district/faction/mission/safehouse ID strings
│   │   ├── GameDataStructures.gd ← SaveData, PropertySaveData, MissionSaveData
│   │   ├── GameInterfaces.gd  ← IInteractable, IDamageable base classes
│   │   └── ServiceLocator.gd  ← Typed service registry (String key → Object)
│   │
│   ├── player/
│   │   └── PlayerController.gd ← CharacterBody3D: move, camera orbit, ADS, interact, cover
│   │
│   ├── world/
│   │   ├── WeatherVisualEffects.gd ← GPUParticles3D rain/snow, lightning OmniLight3D
│   │   ├── FogController.gd        ← Tween-based fog density blending
│   │   ├── CoastalNeonPostProcess.gd ← Parody-district grades, plus a CLEAR-weather highway tint
│   │   ├── HighwayNeon.gd          ← OSM highway-tag neon colors (not municipalities)
│   │   ├── OsmMapContract.gd       ← Road-graph / slice adapter
│   │   ├── MapMeshBaker.gd         ← Road ribbons from the shared bake
│   │   ├── GameWorld.gd            ← In-code slice bootstrap
│   │   ├── StreetLoop.gd           ← Walk, banner, weather/neon, one contact
│   │   ├── StreetContact.gd        ← Interactable prop on the spawn road
│   │   └── LighthouseBeam.gd       ← Rotating beacon + player detection raycast
│   │
│   ├── systems/
│   │   ├── assets/
│   │   │   ├── AssetPropertyData.gd ← Resource: property data model
│   │   │   ├── AssetProperty.gd     ← Node3D + IInteractable: buy/yield flow
│   │   │   └── AcquisitionEngine.gd ← Purchase validation + mission unlock
│   │   ├── combat/
│   │   │   ├── CombatController.gd  ← 5-weapon system, raycast hit, async reload
│   │   │   └── FactionHostilityManager.gd ← Territory detection, hit squad spawning
│   │   ├── audio/
│   │   │   ├── AudioBarkManager.gd  ← NPC contextual voice lines by region
│   │   │   └── DJChatterSystem.gd   ← Context-aware DJ commentary + radio ducking
│   │   ├── ai/
│   │   │   ├── PedestrianAI.gd     ← 5-state FSM + NavigationAgent3D
│   │   │   ├── DynamicSpawner.gd   ← Spawn/despawn by region density
│   │   │   └── RegionProfile.gd    ← Resource: per-district demographic config
│   │   └── traffic/
│   │       ├── VehicleAI.gd          ← 4-state FSM + NavigationAgent3D
│   │       ├── GridlockZone.gd       ← Area3D canal bridge gridlock + honking
│   │       └── RegionalVehiclePool.gd ← Weighted vehicle type selection
│   │
│   ├── events/
│   │   ├── FloodSystem.gd           ← Dynamic water plane rise/recede
│   │   ├── BoatSinkSystem.gd        ← 3-stage boat sinking sequence
│   │   └── EmergencyBroadcastSystem.gd ← EAS radio override
│   │
│   ├── controllers/
│   │   └── VehicleController.gd    ← VehicleBody3D: motor/steer/brake, weather friction
│   │
│   ├── vehicle/
│   │   └── BuoyancyEffector.gd     ← Multi-point hull buoyancy physics
│   │
│   └── ui/
│       ├── MinimapController.gd    ← SubViewportContainer + icon projection
│       ├── WantedLevelDisplay.gd   ← 5-star TextureRect array
│       ├── CashCounter.gd          ← Animated tween number count-up
│       ├── HealthArmorDisplay.gd   ← Dual ProgressBar with color threshold
│       ├── DistrictNameDisplay.gd  ← Fade-in/out Label banner
│       ├── FactionHostilityDisplay.gd ← Per-faction aggro bars
│       ├── RadioStationWidget.gd   ← Station + song label display
│       └── RadarBlipSystem.gd      ← Player-relative blip projection
│
└── scenes/
    └── bootstrap/
        └── Bootstrap.tscn          ← Create this (see Quick Start step 3)
```

---

## Autoload Signal Map

The event-driven architecture is preserved exactly from Unity. Key cross-system wires:

| Signal | Emitter | Connected To |
|--------|---------|-------------|
| `TimeOfDayClock.midnight` | TimeOfDayClock | RevenueManager.process_daily_yield |
| `WeatherController.weather_changed` | WeatherController | VehicleController, WeatherVisualEffects, FogController, PedestrianAI |
| `PlayerWallet.balance_changed` | PlayerWallet | CashCounter |
| `HeatManager.wanted_level_changed` | HeatManager | WantedLevelDisplay, PedestrianAI, AudioBarkManager, DJChatterSystem |
| `HeatManager.heat_cleared` | HeatManager | SafehouseManager |
| `MissionManager.mission_completed` | MissionManager | AcquisitionEngine.unlock_linked_mission, DJChatterSystem |
| `SafehouseManager.player_slept` | SafehouseManager | TimeOfDayClock.advance_hours(6), SaveLoadManager, HeatManager.clear_heat |
| `GloriaEventDirector.gloria_phase_changed` | GloriaEventDirector | WeatherController, VehicleController, FloodSystem, EmergencyBroadcastSystem |

---

## Input Actions (all pre-mapped in project.godot)

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| move_forward / back / left / right | WASD | Left stick |
| sprint | Left Shift | R3 |
| jump | Space | A (Xbox) / Cross (PS) |
| interact | E | X (Xbox) / Square (PS) |
| fire | LMB | RT/R2 |
| aim | RMB | LT/L2 |
| reload | R | Y/Triangle |
| weapon_next / prev | Scroll up/down | RB/LB |
| pause | Escape | Start |
| map | M | Select/Back |
| radio | N | D-pad up |
| crouch | Left Ctrl | L3 |
| enter_vehicle | F | X/Square |

---

## What's Not in This Port

A few Unity-specific things were left as integration points rather than direct ports:

| Unity System | Godot Equivalent Needed |
|---|---|
| `EmpireDatabaseManager` (SQLite) | Replace with Godot's built-in JSON save (SaveLoadManager already handles this) or add [godot-sqlite](https://github.com/2shady4u/godot-sqlite) plugin |
| `NavMesh baking` (runtime) | Use Godot's `NavigationRegion3D` in scenes; bake in editor or use `NavigationServer3D.bake_from_source_geometry_data()` |
| `WheelCollider` physics | Replaced with `VehicleBody3D` + `VehicleWheel3D` — configure wheel nodes in scene editor |
| Post-processing (URP Volume) | Godot uses `WorldEnvironment` + `Environment` resource — CoastalNeonPostProcess.gd uses `ambient_light_color` |
| `ParticleSystem` particle configs | Replace Unity `.asset` particle configs with Godot `GPUParticles3D` resources in the scene editor |
| `DebugOverlay` (IMGUI) | Port to Godot's built-in debug overlay or use a `CanvasLayer` with `Label` nodes |

---

## First Systems to Test After Boot

Run these in order — each one validates a dependency layer:

1. **PlayerWallet** — call `PlayerWallet.add(1000, "test")` from GDScript console
2. **TimeOfDayClock** — watch Output for hour/midnight signals firing
3. **WeatherController** — call `WeatherController.force_state(WeatherController.WeatherState.NOREASTER)`
4. **HeatManager** — call `HeatManager.add_wanted_level(3)`, watch WantedLevelDisplay update
5. **MissionManager** — call `MissionManager.start_mission("ns_01_intro")`
6. **SafehouseManager** — call `SafehouseManager.enter_safehouse("flour_mill")`, then `SafehouseManager.sleep()`

---

*Project QUAHOG — Godot 4 track, active and not production-ready.*
*Contract checks: `python3 -m unittest QUAHOG_Godot.tests.test_osm_contract` from the repo root.*
