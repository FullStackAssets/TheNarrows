# Mount Hope (Unreal) — Architecture

A map of the `MountHope` C++ runtime module: what each piece owns, how data
flows, and how the project is validated without an engine in CI. Read this
before extending a system so new code plugs into the existing seams instead of
growing a parallel one.

## Layers

```
              +---------------------------------------------------+
  Boot        |  UMHGameInstance::Init()                          |
              |    loads slice / missions / economy / dialogue /  |
              |    collectibles / radio JSON, then the save slot  |
              +------------------------+--------------------------+
                                       |
              +------------------------v--------------------------+
  Orchestrator|  AMHGameModeBase (ticks + wires everything)       |
              |    - spawns managers (weather, minimap, pedestrian|
              |      + police spawners)                           |
              |    - mission loop: complete / fail / restart      |
              |    - consequence loop: wasted / busted -> respawn |
              |    - drives heat decay, time-of-day, wanted decay |
              +----+-------------------+-------------------+------+
                   |                   |                   |
   Game-instance   |     World         |     Actors        |
   subsystems      |     subsystems    |     (spawned)     |
  +----------------v-+  +--------------v-+  +--------------v----------+
  | MissionSubsystem |  | WantedSubsystem|  | PlayerCharacter/Vehicle|
  | GameStateSubsys  |  | OpenWorldSubsys|  | Pedestrian(+Spawner)   |
  | DialogueSubsys   |  +----------------+  | PoliceUnit(+Spawner)   |
  | ReputationSubsys |                      | Mission/Health/Weapon/ |
  | CollectibleSubsys|                      | Collectible/Shop/Safe- |
  | RadioSubsys      |                      | house/Weather/Minimap  |
  | TimeOfDaySubsys  |                      +------------------------+
  | WorldSliceSubsys |
  +--------+---------+
           |  delegates (OnWeatherChanged, OnMissionCompleted/Failed,
           |             OnWantedLevelChanged, OnDialogueLineChanged,
           v             OnCollectibleFound, OnHourChanged, ...)
  +-------------------+
  | UMHGameHudWidget  |  (built at runtime by AMHPlayerController)
  +-------------------+
```

## Responsibilities

**Game-instance subsystems** (persist across level loads; own game state/data):

| Subsystem | Owns |
| --- | --- |
| `UMHMissionSubsystem` | Campaign data (stable `id`s), current mission/step, advance / **restart / fail** / completion, and which completion channel a step accepts (`Reach`/`Timed`/`Survive`/`Talk`/`Pay`/`Collect`). |
| `UMHGameStateSubsystem` | Cash, health, dual heat, weather, owned businesses + passive income, safehouse, save/load (the save path other systems write through). Wasted/busted consequences. |
| `UMHEconomySubsystem` | Offer catalog, discounted quotes, purchases, and a short ledger. Cash balance still lives on the game-state subsystem. |
| `UMHReputationSubsystem` | Per-faction standing (clamped ±100) + save snapshot. |
| `UMHDialogueSubsystem` | Conversations, per-line speakers, choice branches, active-line state, `OnDialogueLineChanged`, objective completion when a completing conversation ends. |
| `UMHCollectibleSubsystem` | Collectible roster, set progress, one-shot set bonuses, collected ids, `OnCollectibleFound`. |
| `UMHRadioSubsystem` | Station roster + tuning + `OnStationChanged` / `OnSongChanged`. |
| `UMHTimeOfDaySubsystem` | Game clock, `OnHourChanged`, sun-intensity curve. |
| `UMHWorldSliceSubsystem` | Loads the real New Bedford OSM slice JSON at runtime. |

**World subsystems** (per-world):

| Subsystem | Owns |
| --- | --- |
| `UMHWantedSubsystem` | Heat accumulation, fractional decay, 5-star wanted level, `OnWantedLevelChanged`. Drives police escalation: `AMHPoliceSpawnerActor` maps the star level to a tier (count / health / armed) and `AMHPoliceUnitPawn` runs a Pursue/Attack state machine (armed units fire with a line-of-sight check). The player's armor soaks damage before health, and the pistol can neutralize pursuers. |
| `UMHOpenWorldSubsystem` | Map-source profile metadata (kept in sync with the slice load path). |

**Actors** are the spawned, tickable things (player, vehicle, pedestrians and
their spawner, police units and their spawner, mission trigger volumes, pickups,
shops, safehouse, weather director, minimap capture). `AMHGameModeBase` spawns
the manager actors in `BeginPlay` and owns the top-level ticks.

## Cross-cutting patterns

- **Delegates over polling.** Subsystems broadcast; the HUD and game mode
  subscribe. New UI should bind an existing delegate, not poll each tick.
- **Data-driven content.** Missions, economy, dialogue, collectibles, and radio
  are JSON under `Data/` loaded at boot; JSON schemas are validated by
  `Scripts/validate_scaffold.py`. Prefer adding data over hard-coding.
- **Null-safe asset hooks.** `USoundBase*` cue fields are `EditAnywhere` and
  played through `PlaySound2D` (a no-op when unset), so gameplay works before
  audio is authored. Follow this for any new editor-authored asset.
- **One save path.** `UMHGameStateSubsystem::SaveToSlot/LoadFromSlot` (slot
  `UMHGameInstance::SaveSlotName`) is canonical and also snapshots the
  collectible + reputation subsystems. `UMHSaveSubsystem` is unwired scaffolding
  (documented in-file); do not route saves through it.

## Testing & validation (no engine in CI)

There is no Unreal install in the cloud sandbox, so nothing here compiles the
module. Three gates stand in, in increasing fidelity:

1. **`Scripts/validate_scaffold.py`** — project structure, plugin/module wiring,
   JSON schema, and required-symbol presence.
2. **`Scripts/check_cpp.py`** — C++ *structural* static checks (delimiter
   balance, `.generated.h` ordering, `GENERATED_BODY`, `Build.cs` module deps,
   dynamic-delegate `UFUNCTION`s). Not a compiler — see its header.
3. **UE automation tests** (`Source/MountHope/Tests/*.cpp`, guarded by
   `WITH_DEV_AUTOMATION_TESTS`) — real unit tests of the deterministic subsystem
   logic (wanted decay, mission channels, dialogue choices, offer quotes,
   collectible sets, reputation, clock). The automation tests are compiled by
   Unreal; this sandbox does not run them.
   These require the engine, so they run **locally / on a self-hosted UE runner**,
   not in the Python CI. Run them from the editor's **Session Frontend →
   Automation** (filter `MountHope`) or headlessly:

   ```bash
   "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" \
     "$PWD/MountHope.uproject" \
     -ExecCmds="Automation RunTests MountHope; Quit" -unattended -nop4 -nosplash
   ```

The definitive gate is still a real UE 5.8 compile + PIE playtest; the above
catch structural and logic regressions early, they do not replace it.

## Deployment reality

Packaging a UE title needs a licensed engine and a GPU to cook content, which
GitHub's hosted runners don't provide. `Scripts/build.sh` / `build.ps1` (compile)
and `Scripts/package.sh` / `package.ps1` (cook + stage) are the local pipeline;
wiring them into CI requires a **self-hosted runner with UE 5.8 installed** — the
Python gates in `.github/workflows/unreal-ci.yml` are the sandbox-safe stand-in
until then. See `Docs/BUILD_WINDOWS.md`.
