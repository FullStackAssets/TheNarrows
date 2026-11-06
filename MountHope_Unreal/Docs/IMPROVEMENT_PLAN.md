# Mount Hope Unreal — Improvement Plan

This document captures the current state of `MountHope_Unreal/`, the gaps
identified during exploration, and the phased plan to reach a playable
vertical slice.

## Current state (before this pass)

`MountHope_Unreal/` was an early C++ scaffold (~500 lines) with:

- Thin stubs for player, vehicle, missions, economy, and world metadata
- No `Target.cs` files (first compile required editor project generation)
- Empty `Content/` and a default map (`MH_VerticalSlice`) that does not exist
- Legacy input bindings despite Enhanced Input being enabled
- No JSON bootstrap, save/load, or mission trigger loop
- No CI; only `Scripts/validate_scaffold.py` for structural checks

A parallel tree at `QUAHOG_Unreal/` had more complete framework code (JSON
loading, mission triggers, save game) but is not listed in `README.md` /
`ENGINES.md` as the canonical PC track.

## Gaps vs. vertical-slice acceptance criteria

From `Docs/VERTICAL_SLICE.md`:

| Criterion | Status |
| --- | --- |
| Graybox level with OSM roads/water | Needs editor assets |
| Walk third-person on roads | C++ ready; needs map + pawn mesh |
| Enter/exit Chaos vehicle | C++ possession loop implemented |
| Drive on roads | Needs vehicle mesh + wheel setup |
| Mission NPC + short crime mission | JSON campaign + trigger actors wired |
| Earn/spend money | `MHGameStateSubsystem` + businesses JSON |
| Dialogue exchanges | `UMHDialogueSubsystem` + `MHDialogueNpcActor` + JSON |
| OSM slice data loaded at runtime | `MHWorldSliceSubsystem` loads web slice JSON |
| Cinematic Lumen/Nanite lighting | Config defaults set; needs content |

## Improvements delivered in this pass

### Build & CI

- `Source/MountHope.Target.cs` and `Source/MountHopeEditor.Target.cs`
- `Scripts/build.sh` — wraps `RunUBT.sh` when `UE_ROOT` is set locally
- `.github/workflows/unreal-ci.yml` — runs `validate_scaffold.py` on PRs

### Framework (C++)

- `UMHGameInstance` — bootstraps slice, missions, economy, and save slot on startup
- `UMHWorldSliceSubsystem` — loads `slice-newbedford.json` at runtime
- `UMHMissionSubsystem` — JSON campaign with step advancement
- `UMHGameStateSubsystem` — cash, heat, weather, businesses, save/load
- `AMHMissionTriggerActor` — world-target objective volumes
- `AMHGameModeBase` — mission loop, heat decay, trigger refresh
- `AMHPlayerCharacter` — vehicle enter/exit + interact
- `UMHEconomySubsystem` — thin delegate to game state (backward compatible)

### Data

- `Data/Missions/vertical_slice.json`
- `Data/Economy/businesses.json`

### Validation

- Expanded `Scripts/validate_scaffold.py` to cover targets, JSON schemas,
  config wiring, mission/vehicle helpers, and deprecated-macro guards.

## Second pass — GTA-structure gameplay loop, canonical storyline, radio

This pass wired up systems that existed as orphaned scaffolding (never spawned
or read by anything) into real gameplay loops, and replaced the two-mission
placeholder campaign with the canonical Acts I–III + Gloria finale storyline
sourced from `QUAHOG_GODOT1/scripts/systems/story_mission.gd` (the most
complete story-mission implementation across the Godot/Web tracks — see
`ENGINES.md` and `QUAHOG_Unreal/TRACK.md` for track history). `QUAHOG_Unreal/`
(the earlier UE bootstrap, still its own parallel track) was checked for anything not already present here;
its data files are byte-identical to `MountHope_Unreal/`'s and its C++ is a
strict subset (no damage model, no `IMHInteractable`, weaker character), so
nothing further was ported from it.

### Police/wanted loop (previously orphaned)

- `UMHWantedSubsystem` (`WorldSubsystem`) now decays heat over time
  (`TickWantedDecay`, driven from `AMHGameModeBase::Tick`) and broadcasts
  `OnWantedLevelChanged`. `AMHVehiclePawn` reports `PropertyDamage` crimes on
  high-speed collisions with other pawns; mission steps can flag `"crime"`
  with a `"crimeSeverity"`; `AMHSafehouseActor` clears wanted state on save.
- HUD shows a 5-star wanted readout (`UMHGameHudWidget::SetWantedText`).

### Reputation loop (previously orphaned)

- `UMHReputationSubsystem` is now read/written by mission steps
  (`"reputationFaction"` / `"reputationDelta"`) and by `AMHShopActor` (garage
  repair discount when reputation with `Faction.Business.Garage` is high
  enough). New faction tags for the expanded campaign: `Faction.Crew.FallRiver`,
  `Faction.Sports.Boxing`, `Faction.Elite.CapeCod`.

### Vehicle damage & repair

- `AMHVehiclePawn` gained `Health`/`MaxHealth`, `ApplyVehicleDamage`,
  `RepairVehicle`, `GetHealthPercent`, `IsWrecked`, and a `NotifyHit` override
  that converts impact impulse into damage and a wanted-heat report. Chaos
  vehicle `Mass`/`DragCoefficient` are tuned for a heavier, less toy-like feel.

### Safehouse, shops, and collectibles (previously missing)

- `AMHSafehouseActor` — interactable save point; persists a spawn location
  (`UMHGameStateSubsystem::SafehouseLocation`) that `AMHGameModeBase` restores
  the player to on the next session, and clears wanted state.
- `AMHShopActor` — `Garage` mode repairs the nearest damaged vehicle for cash;
  `GeneralStore` mode wraps `BuyBusiness` for a purchase interaction.
- `UMHCollectibleSubsystem` + `AMHCollectibleActor` — a JSON-driven "scrimshaw"
  side-content loop (`Data/Collectibles/vertical_slice.json`, 8 items across
  the map), saved/loaded via `UMHGameStateSubsystem`.

### Radio (new)

- `UMHRadioSubsystem` — a JSON-driven station roster
  (`Data/Radio/stations.json`) built from the real station/DJ/song content in
  `quahog-project-files/radio-scripts.md` and the rendered VO/music already in
  `QUAHOG_Web/public/music/`: **WHALE 92.1** (Sully, classic rock), **The
  Rage, 1480 AM** (Buddy Mello, talk), **The Anvil, WBOX** (Iron Mike
  Fontaine — its Brockton rock-anthem bed doubles as foreshadowing for the
  "The Undefeated" mission), **Maré Alta, 105.3** (Tia Conceição, Portuguese/
  Cape Verdean). `R` cycles stations while driving
  (`AMHPlayerCharacter::RequestRadioNextStation`); the HUD shows
  station/song. The subsystem only carries station/song *data* — binding
  actual `USoundWave` assets from the `voFolder`/`jingleFolder` manifest paths
  to a Blueprint audio player is an editor step (see
  `Scripts/editor_bootstrap_vertical_slice.py`'s next-steps output).

### Canonical campaign (replaces the 2-mission placeholder)

`Data/Missions/vertical_slice.json` now has 11 missions matching
`story_mission.gd`'s `MISSIONS` array 1:1 (title, steps, per-step reward,
mission-level `completionReward`, `act` label, `completionMessage`), with
world-space targets converted via the verified `(GodotX, 0, -GodotZ) ->
(UnrealX, 0, UnrealZ)` rule (cross-checked against 3 coordinates shared with
the pre-existing 2-mission data, which matched exactly):

1. **Off the Boat** (Prologue) — New Bedford waterfront opener.
2. **Auction Rules**, **The Linguiça Run**, **Harbor Heat** (Act I — The
   Narrows) — New Bedford crew jobs.
3. **Spindle City**, **Acquitted** (Act II — Spindle City) — Fall River /
   Lady Borden's crew, Battleship Cove and the Borden House.
4. **The Undefeated** (Act II — City of Champions) — Brockton / Iron Mike
   Fontaine at Champion City Gym.
5. **Heritage Marina**, **Compound Interest** (Act III — The Cape) — the Fake
   Kennedys' Hyannis compound.
6. **Gloria**, **Big Mamie** (Finale — Gloria) — the hurricane climax at the
   Hurricane Barrier and the USS Massachusetts at Battleship Cove.

Only New Bedford's core graybox is OSM-imported today; missions past "Harbor
Heat" reference Fall River/Brockton/Cape Cod coordinates that need their own
OSM import or hand-authored blockout before their trigger volumes are
reachable in PIE — see `Scripts/editor_bootstrap_vertical_slice.py`'s
next-steps output.

### Consequence loop, stamina, time-of-day, health pickups

- `UMHGameStateSubsystem::ApplyDamage` now triggers a "wasted" consequence at
  0 HP (cash penalty, full heal, wanted cleared, `OnPlayerWasted`); a sustained
  max wanted level (`AMHGameModeBase::MaxWantedBustedSeconds`, default 25s)
  triggers "busted" the same way via `TriggerBusted`/`OnPlayerBusted`.
  `AMHGameModeBase` binds both delegates and respawns at the safehouse.
- `AMHPlayerCharacter` has a sprint stamina meter (drain while sprinting,
  regen at rest, forces sprint off at 0); shown on the HUD status line.
- `UMHTimeOfDaySubsystem` — a minimal hour clock (`AdvanceTime`, default one
  game-day per 600 real seconds) that modulates `AMHWeatherDirectorActor`'s
  sun intensity day vs. night. Deliberately lightweight — no sky/moon material
  or full day-night blend; see the Godot tracks' `TimeOfDayClock.gd` if a
  fuller cycle is wanted later.
- `AMHHealthPickupActor` — walk-over heal pickup with a respawn cooldown,
  matching the Web track's "green cross" pickups.

### Ambient pedestrians (previously zero implementation)

- `AMHPedestrianCharacter` — an `AAIController`-driven wander/flee pedestrian
  using the `AIModule`/`NavigationSystem` dependencies the module already
  declared but never used. Wanders to random navmesh-reachable points near
  its spawn; flees on foot when a fast-moving `AMHVehiclePawn` gets close.
  `AMHVehiclePawn::NotifyHit` now reports `Assault` (not just
  `PropertyDamage`) when the actor hit is a pedestrian. Needs the navmesh
  baked (`Scripts/editor_setup_navmesh.py`) to path.
- This is the "first-slice" ambient NPC per `VERTICAL_SLICE.md`'s
  requirements table; Mass AI (already an enabled plugin) remains the planned
  upgrade path for denser crowds later — no ragdoll-on-death behavior yet.

### Third pass — minimap, mission-completion toast, denser crowds, basic combat

Closing the loop on the two items explicitly deferred in the previous pass:

- **Mission-completion toast (bug fix)** — `UMHMissionSubsystem::OnMissionCompleted`
  was being broadcast but nothing subscribed to it. `UMHGameHudWidget` now
  binds it and shows each mission's `CompletionMessage` ("Act I wrapped —
  Fall River's next.", etc.) as a centered toast for 5 seconds.
- **Minimap** — `AMHMinimapCaptureActor` (spawned by `AMHGameModeBase` like
  the weather director) is a top-down orthographic `USceneCaptureComponent2D`
  that follows the player's position and yaw (so the player always faces
  "up"), rendering into a runtime-created `UTextureRenderTarget2D`. The HUD
  displays it in a 220x220 `SizeBox` bottom-right. No blips for other actors
  yet (would need per-frame world-to-minimap projection); the exact "up"
  axis convention for a yaw-following top-down capture is a one-line offset
  to tune once seen in PIE.
- **Denser ambient crowds** — `AMHPedestrianSpawnerActor` (also spawned by
  `AMHGameModeBase`) is a pragmatic substitute for true Mass AI: it
  maintains up to `MaxActivePedestrians` `AMHPedestrianCharacter` instances
  in a radius around the player, using `UNavigationSystemV1::GetRandomReachablePointInRadius`
  to place them and despawning ones that fall far behind — the same
  "DynamicSpawner" pattern the Godot track used. This is *not* Mass AI; Mass
  (already an enabled plugin) remains the upgrade path if crowd density or
  per-agent LOD becomes a real perf concern.
- **Basic combat** — a single sidearm, kept intentionally minimal rather than
  a full weapon system: `AMHWeaponPickupActor` grants ammo via
  `AMHPlayerCharacter::PickUpPistol`; left-click (`FirePistol`) does an
  `ECC_Visibility` line trace from the camera, applies `ApplyVehicleDamage`
  to a hit `AMHVehiclePawn` or destroys a hit `AMHPedestrianCharacter`, and
  reports `Assault`/`PropertyDamage` to `UMHWantedSubsystem` accordingly. No
  reload, no weapon switching, no aim-down-sights, on-foot only (no
  drive-by) — deliberately scoped down from a full combat system; verify the
  `ECC_Visibility` trace channel against the project's actual collision
  presets once in PIE.

### Fourth pass — police pursuit

The last item flagged as "still needed" for the wanted loop: actual
cop-car pursuit AI/spawning.

- `AMHPoliceUnitPawn` — a deliberately simple pursuer: moves directly toward
  the player pawn each tick (no steering/torque/road-following) and applies
  small "catch pressure" damage (`ApplyDamage`, on a 2s cooldown) when within
  `CatchRadius`. It does **not** trigger Busted directly — that stays owned
  by `AMHGameModeBase`'s existing sustained-max-wanted timer
  (`TickBustedTimer`), so proximity pressure and the actual arrest transition
  don't fight each other.
- `AMHPoliceSpawnerActor` (spawned by `AMHGameModeBase` alongside the other
  managers) scales the active pursuer count to `UMHWantedSubsystem::GetWantedLevel()`
  (capped at 5, matching the star cap), spawning/despawning in a ring around
  the player every 2s.
- Deliberately **not** real vehicle-driving AI (steering toward a nav-mesh
  path while obeying Chaos vehicle physics is a much harder, much riskier
  problem to write blind without a compiler) — `AMHPoliceUnitPawn` is a bare
  `APawn` with a mesh, no `AMHVehiclePawn` inheritance. Upgrading pursuers to
  actually drive cars along roads is the natural next step once this is
  running in the real editor and the OSM road network is imported.

### Fifth pass — code audit and bug fixes

A systematic read-only audit of every `Source/MountHope/` class (split by
subsystem, cross-checked against `VERTICAL_SLICE.md`/this document's stated
behavior) turned up several real bugs that had been sitting unnoticed because
this codebase has never had a compiler run against it. All were fixed in this
pass; no new features, so `Scripts/validate_scaffold.py`'s existing checks
still cover the wired-up state.

**High severity (broke a core loop entirely):**
- `UMHWantedSubsystem::TickWantedDecay` rounded `DecayPerSecond * DeltaSeconds`
  to the nearest int *every frame*, which truncates to 0 at any framerate
  above ~6 FPS — wanted heat never actually decayed in normal play. Fixed by
  accumulating the fractional remainder across ticks (`PendingDecay`) instead
  of discarding it each frame.
- Sprint was unreachable through the legacy input fallback: `DefaultInput.ini`
  had no `Sprint` action mapping and `AMHPlayerCharacter::BindLegacyInput`
  never bound one, so without a Blueprint-authored Enhanced Input mapping
  context, stamina/sprint never triggered. Added the mapping (`LeftShift`)
  and `LegacySprintStart`/`LegacySprintStop` bindings that route through the
  same `SetSprintRequested` helper Enhanced Input uses.

**Medium severity:**
- `TickStamina` could strand stamina at 0 while sprint was still held (regen
  only ran when `!bSprintRequested`, but the stuck state is
  `bSprintRequested && Stamina == 0`, which matched neither branch). Regen now
  runs whenever not actively draining.
- `AMHPoliceSpawnerActor` placed pursuers via a flat XY ring offset with no
  ground/navmesh check, risking spawns underground/in buildings/floating on
  uneven terrain. Now projects onto the navmesh via `ProjectPointToNavigation`
  (falls back to the flat offset if none is found).
- `AMHPoliceUnitPawn::Tick` moved with `SetActorLocation(..., bSweep=false)`
  in full 3D, so pursuers tunneled through walls and flew vertically to match
  a player's height. Now sweeps the move and chases along XY only.
- `AMHGameModeBase::RespawnAtSafehouseIfAvailable` silently no-op'd for a
  player who hadn't claimed a safehouse yet, so an early death/bust had no
  positional consequence. Now falls back to the level's `APlayerStart`.
- Reputation (`UMHReputationSubsystem`) was read/written by missions and
  shops but never persisted — `SaveToSlot`/`LoadFromSlot` didn't touch it.
  Added a string-keyed snapshot (`GetReputationSnapshot`/
  `RestoreReputationSnapshot`) and a `ReputationByFaction` field on
  `UMHSaveGame`.
- `UMHGameStateSubsystem::GetPassiveDailyIncome()` was computed but never
  called anywhere, so owning a business had no ongoing return. Now applied
  once per in-game day via `UMHTimeOfDaySubsystem::OnHourChanged`
  (`AMHGameModeBase::HandleHourChanged`, gated on the midnight rollover).
- `AMHPedestrianCharacter::FindNearestThreat` returned the *first* qualifying
  vehicle found, not the nearest — a pedestrian could flee a far vehicle while
  ignoring a closer, faster one. Now tracks the closest match.

**Low severity / cleanup:**
- `UMHOpenWorldSubsystem::GetDefaultMapSource()` was unwired and pointed at a
  stale path (`quahog-project-files/mapdata/southcoast-roads.json`) that
  isn't the JSON actually loaded at runtime. Path corrected to match
  `UMHGameInstance::SlicePath`, and `AMHGameModeBase::BeginPlay` now logs the
  active profile so the function isn't dead code.
- `UMHSaveSubsystem` is a second, entirely unwired save system with a
  different default slot name (`MountHope_Autosave` vs. the real
  `MountHopeSlot`) — a footgun if anything is ever pointed at it by mistake.
  Left in place (not referenced by `validate_scaffold.py` or anything else,
  so removing it is safe whenever someone wants to; not done here since
  deleting pre-existing files without being asked is out of scope for an
  audit pass) but documented as unwired and its slot name aligned to the real
  one so it can't silently diverge if it is ever wired up.
- `UMHCollectibleSubsystem::OnCollectibleFound` broadcast on every pickup but
  had no HUD listener — cash was still awarded, but the player got zero
  on-screen feedback. `UMHGameHudWidget` now shows a "found (N/Total)" toast,
  mirroring the existing mission-completion toast.
- Mission/dialogue/collectible JSON loaders used `GetStringField` (silently
  substitutes an empty string and logs a warning on a missing/malformed
  required field) for `title`/`text`/`id`/`name` instead of
  `TryGetStringField` + skip. A future content edit that drops one of these
  fields would previously load a blank-prompt entry instead of failing
  loudly; now the entry is skipped with a warning, matching the pattern
  already used for `id`/`text` elsewhere in the same files.
- `UMHGameHudWidget::SetDialogueLine` ran the speaker name through
  `FText::Format(FText::FromString("{0}"), Speaker)` instead of using it
  directly — wasteful, and a latent risk if a speaker name ever contained
  literal `{}`.
- `AMHMinimapCaptureActor::Tick` re-applied the capture component's constant
  `-90` pitch every frame; it's set once in the constructor and never
  changes, so the per-tick call was redundant work.

### Sixth pass — mission depth (typed/timed objectives, failure & restart)

The mission layer was a flat "reach the next target" chain with no way to lose.
This pass adds stakes and objective variety — all deterministic gameplay logic,
the category most safely written without a live compiler, and covered by the
`validate_scaffold.py` + `check_cpp.py` gates.

- **Typed objectives** — `FMHMissionStep` gained `EMHObjectiveType`
  (`Reach` / `Timed` / `Survive`) and `TimeLimitSeconds`. `Reach` is the
  unchanged legacy behavior. `Timed` must be completed before the countdown
  expires or the mission fails. `Survive` is a hold-out beat that auto-completes
  when the countdown ends (no world target needed). All optional/back-compatible
  in JSON — untyped steps stay `Reach`.
- **Mission failure + checkpoint restart** — `UMHMissionSubsystem` gained
  `OnMissionFailed`, `RestartCurrentMission()` (reset to the mission's first
  step), and `IsMissionInProgress()`. `AMHGameModeBase::FailCurrentMission()`
  restarts the mission and broadcasts; it fires from a `Timed` timeout and from
  the wasted/busted consequence handlers (dying mid-mission now fails it,
  GTA-style). The HUD shows a "Mission failed: <reason>" toast.
- **Live countdown HUD** — a `Timed`/`Survive` step's remaining seconds render
  in the objective line (`AMHGameModeBase::IsCurrentStepTimed()` /
  `GetCurrentStepTimeRemaining()`).
- **Refactor** — the post-gate completion logic (rewards, crime, reputation,
  advance, mission-complete, save) was extracted into
  `AMHGameModeBase::ApplyObjectiveCompletion()`, shared by the normal completion
  path (after its vehicle/heat gates) and the `Survive` auto-complete.
- **Data** — `Data/Missions/vertical_slice.json` now exercises both new types:
  "Off the Boat"'s ambush escape is `Timed` (45s), and "Harbor Heat" gained a
  `Survive` hold-out beat plus a `Timed` getaway. A new `MissionFailedSound`
  hook on the game mode follows the existing null-safe audio-cue pattern.

Still not proven by a compile — a real UE 5.8 build is required, and the
per-type *feel* (timer lengths, what counts as "in combat" for a survive beat)
needs PIE tuning.

### Seventh pass — automated tests, architecture docs, maintainability

Turned the "expand + harden" directive into the parts that actually apply to a
single-player UE game (this is not a scaling web backend, and there's still no
engine in the sandbox to compile against), focusing on the biggest real gap:
the C++ logic had no automated tests.

- **UE automation test suite** (`Source/MountHope/Tests/*.cpp`, guarded by
  `WITH_DEV_AUTOMATION_TESTS`) — real unit tests of the deterministic subsystem
  logic, each driving a `NewObject` subsystem directly (no world needed):
  - `MHWantedSubsystemTest` — includes a **regression test for the wanted-decay
    bug** (heat must actually drop at 60 FPS) and the 5-star clamp.
  - `MHMissionSubsystemTest` — step advance, campaign-complete boundary, and the
    new restart-to-first-step behavior.
  - `MHGameStateSubsystemTest` — cash clamp, buy-business + passive income, and
    the lethal-damage "wasted" consequence.
  - `MHTimeOfDaySubsystemTest` — midnight rollover and the day/night sun curve.
  - `MHReputationSubsystemTest` — ±100 clamp and save-snapshot roundtrip.
  They run in-editor / on a self-hosted UE runner (see `README.md` and
  `ARCHITECTURE.md`), not in the Python CI, since they need the engine.
- **`Docs/ARCHITECTURE.md`** — module map, subsystem responsibilities, the
  delegate/data-driven/one-save-path patterns, the three-tier
  validation-vs-compile model, and the deployment reality (real packaging needs a
  self-hosted UE runner).
- **Gate wiring** — `validate_scaffold.py` now requires the test files and
  checks each declares an automation test guarded by `WITH_DEV_AUTOMATION_TESTS`;
  `check_cpp.py` covers the new files structurally (73 files, still green).

Deliberately *not* done: fabricated "microservices / horizontal-scaling /
exponential-growth" scaffolding (doesn't map to a single-player game) or large
new gameplay modules written blind (too risky without a compiler). Those wait
for the editor loop.

### Eighth pass — combat & police-AI depth

Pushed on the combat/police track (chosen over vehicle/traffic because its logic
is deterministic and unit-testable without an engine; ambient-traffic pathing is
mostly physics/feel that needs the editor). The risky "feel" bits (movement,
line-of-sight timing) are kept simple and flagged for PIE tuning.

- **Player armor** — `UMHGameStateSubsystem` gains `Armor`/`MaxArmor`,
  `AddArmor()`, and `ApplyDamage` now soaks damage through armor before health
  (classic GTA). Armor is saved/loaded and cleared on wasted/busted. The HUD
  status line shows HP and (when non-zero) Armor.
- **Police pursuit state machine** — `AMHPoliceUnitPawn` now has an
  `EMHPoliceState` (`Pursue` / `Attack`): unarmed units close in and apply
  contact "catch" pressure (as before); armed units hold at `AttackRange` and
  fire on a cadence, gated by a `HasLineOfSightTo` trace, dealing `ShotDamage`
  through the armor-aware `ApplyDamage`.
- **Cop health + neutralization** — units have `Health`/`MaxHealth`,
  `ApplyDamage`, and `IsDead`; the player's pistol now damages cops
  (`PistolCopDamage`) and a killed pursuer is destroyed (still a crime — reports
  `Assault`). This ties the combat loop back into the wanted loop.
- **Wanted-tier escalation** — `AMHPoliceSpawnerActor::GetTierForWantedLevel`
  (a pure, testable map) scales unit **count**, **health**, and **whether they
  are armed** with the star level (firearms appear at 3★); spawned units are
  configured via `ConfigureForTier`.
- **Tests** — `MHPoliceTest` (unit death + tier escalation) and an armor-soak
  test added to `MHGameStateSubsystemTest`. Gates: `validate_scaffold.py` checks
  the new symbols + test file; `check_cpp.py` covers it (74 files, green).

Known editor-only follow-ups: the `ECC_Visibility` LOS/pistol trace channel
against the project's collision presets; a Search/give-up state and cop
ragdoll/animation; and tuning ranges/damage/fire-rate in PIE.

### Audio cue hook points

Mirroring the radio subsystem's "data ready, asset bound in editor" pattern:
`AMHGameModeBase` (mission complete / next objective / busted-wasted),
`AMHCollectibleActor` (pickup), `AMHShopActor` (transaction success/denied),
`AMHHealthPickupActor` (heal), and `UMHWantedSubsystem` (wanted level
increase) all expose `EditAnywhere`/`EditDefaultsOnly` `USoundBase*`
properties and call `UGameplayStatics::PlaySound2D` — safe to leave unset
(null-safe), ready for `USoundWave`/`SoundCue` assets to be assigned in the
editor.

### Ninth pass — talk / pay / collect, choices, catalog, OSM contract

Expanded the four content systems with logic that stays data-driven and
unit-testable, and prepared the OSM import without touching the shared map
pipeline.

- **Missions** gained stable `id`s and three objective types: `Talk` (a named
  conversation), `Pay` (buy a catalog offer), and `Collect` (a set count or one
  item). `CanAcceptCompletion` / `CanAcceptCollection` decide which caller may
  finish the step. World triggers still complete `Reach`/`Timed` only.
  `Data/Missions/vertical_slice.json` keeps the 11-mission South Coast arc and
  adds a Bethel scrimshaw pickup to "Off the Boat" and a $200 dock bribe to
  "Auction Rules".
- **Dialogue** lines can name a speaker, offer choices (`nextLine` / `end`), and
  mark a branch ending with `endsConversation` so the other branch is not played
  next. `1` and `2` select the first two choices. Deacon Mealy's opener and the
  Republic runner both branch. The HUD appends the choices under the line.
- **Economy** loads an `offers` catalog from `Data/Economy/businesses.json`
  (bribe, medkit, vest, patch job). Quotes clamp discounts to 0–75%. A general
  store pays the active Pay step through `TryPayCurrentObjective`, or sells
  `LinkedOfferId` when no pay step is active. Successful cash changes append a
  short ledger.
- **Collectibles** gained sets. `narrows_scrimshaw` pays $400 and
  `south_coast_scrimshaw` pays $800, each once. The pickup actor grants the
  bonus and notifies the collect objective.
- **OSM** — `Scripts/prepare_osm_import.py` writes `Data/OSM/import_manifest.json`
  from the committed slice and southcoast roads (counts, class histogram,
  400 m playable roads, nearby/hero landmarks, both coordinate contracts).
  `editor_import_osm.py` uses the manifest radius for debug splines. The OBJ
  import and the shared map pipeline are unchanged.
- **Tests** — automation coverage for objective channels, dialogue choices,
  offer quotes, and set bonuses. They need Unreal Editor; they were not executed
  here. `validate_scaffold.py` cross-checks the JSON references and the manifest.

## Next phases (editor workstation required)

### Phase 1 — First playable in editor

1. Open `MountHope.uproject` in UE 5.8 and compile (`Scripts/build.sh` or editor).
2. Create `/Game/Maps/MH_VerticalSlice` as an Open World map.
3. Add Blueprint children for `MHPlayerCharacter` and `MHVehiclePawn` with
   placeholder meshes and a Chaos vehicle setup.
4. Place at least one `MHVehiclePawn` in the slice spawn area.
5. PIE: walk, enter vehicle, complete first mission step via trigger volume.

### Phase 2 — World geometry

1. Run `Scripts/editor_import_osm.py` (imports OBJ + slice road splines).
2. Verify scale/collision in viewport; tune bbox caps in the script if needed.
3. Nav mesh bake on imported roads.

### Phase 3 — Enhanced Input migration

Done in C++ (`MHPlayerCharacter` supports Enhanced Input with legacy fallback).
Run `Scripts/editor_create_enhanced_input.py` in the editor to create
`IMC_Default` and Input Action assets.

### Phase 4 — Content & polish

**Implemented in C++:**
- `UMHGameHudWidget` — objective, cash/weather status, dialogue subtitles (binds to `OnDialogueLineChanged`)
- `AMHPlayerController` — creates HUD at runtime; **.** cycles weather (debug)
- `AMHWeatherDirectorActor` — blends fog/sun per `EMHWeatherState`

**Still editor-authored:**
- MetaHuman mesh on `MHDialogueNpcActor`
- Optional `WBP_MHGameHud` via `Scripts/editor_create_hud_widget.py`
- Niagara rain particles for CoastalRain / Nor'easter
- Package Development build via `Scripts/package.sh`
- Meshes for `AMHSafehouseActor` / `AMHShopActor` / `AMHCollectibleActor`,
  `ShopType` + `LinkedBusinessId` on the two placed shops
- Import `USoundWave` assets from `Data/Radio/stations.json`'s
  `voFolder`/`jingleFolder` paths and wire a Blueprint radio player to
  `UMHRadioSubsystem::OnStationChanged` / `OnSongChanged`

### Phase 5 — Parallel Unreal tracks

`QUAHOG_Unreal/` is an active parallel UE track. Its earlier bootstrap is where
the JSON mission/economy/world-slice patterns were first written; those patterns
also live in `MountHope_Unreal/`. Status and history: `QUAHOG_Unreal/TRACK.md`
(the old `LEGACY.md` path only redirects). Mount Hope work stays in this tree.
Neither tree has been compiled by Unreal in this sandbox.

## Local compile & package

```bash
export UE_ROOT="/path/to/UE_5.8"
./MountHope_Unreal/Scripts/build.sh

# Package (after editor content exists):
./MountHope_Unreal/Scripts/package.sh
```

See `Docs/EDITOR_SETUP.md` for the Phase 1 editor checklist and
`Scripts/editor_bootstrap_vertical_slice.py` for in-editor automation.

## CI (cloud sandbox)

```bash
python3 MountHope_Unreal/Scripts/validate_scaffold.py   # structure / wiring / JSON
python3 MountHope_Unreal/Scripts/check_cpp.py           # C++ structural static checks
```

Full C++ compile cannot run in the cloud sandbox without a local Unreal install.

### C++ static gate (`Scripts/check_cpp.py`)

The fifth-pass audit surfaced ~14 bugs in C++ that had never seen a compiler,
so this pass added a headless static gate to catch the *structural* subset of
build-breakers before the editor ever opens. It deliberately is **not** a
`UnrealEngine` shim / fake compiler: UnrealHeaderTool code-gen (`UCLASS`/
`GENERATED_BODY`/`UPROPERTY` -> `*.generated.h`) means a shim would have to
`#define` UHT away and would then pass code UBT rejects — false confidence,
worse than no gate. Instead it checks, per file, without an engine:

1. `{}` / `()` / `[]` balance (after stripping comments and string/char
   literals), catching truncation/copy-paste breaks.
2. `.generated.h` include present, **last**, and matching the file's basename.
3. `GENERATED_BODY()` present for every `UCLASS`/`USTRUCT`/`UINTERFACE`.
4. Every `X.cpp` includes its own `X.h`.
5. Every non-transitive module a source file uses (`EnhancedInput`,
   `GameplayTags`, `AIModule`, `NavigationSystem`, `UMG`, `Json`,
   `ChaosVehicles`) is declared in `MountHope.Build.cs`.
6. Every `AddDynamic`/`RemoveDynamic` target method is declared `UFUNCTION()`
   (binding a non-`UFUNCTION` to a dynamic multicast delegate is a hard error).

Each check is conservative (skips rather than guesses when unsure), so a green
result carries no false positives and a red result is a genuine problem. It is
verified against injected breakages for all six classes. It still does not
prove type/semantic correctness — a real UE 5.8 compile remains required.

## Deployment pipeline (Tier 3)

The static gates above are Tiers 1–2 of a three-tier validation model now fully
documented in `Docs/CI_AND_BUILD.md`. Tier 3 is the real thing — a UBT compile,
headless automation tests, and an optional cook/package — which the sandbox
cannot run because Unreal Engine is not installable here. This pass closed the
gap between "we have build scripts" and "we have a pipeline" by adding:

- `Scripts/run_tests.sh` — a shared headless automation-test runner
  (`UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MountHope" -nullrhi`)
  that parses the exported report and exits non-zero on any failure. It is the
  single entry point used both locally and by CI, mirroring `build.sh`'s
  `UE_ROOT` interface.
- `.github/workflows/unreal-build.yml` — a self-hosted (`[self-hosted, unreal]`),
  `workflow_dispatch`-only job that runs the Tier 1–2 gates, then `build.sh`,
  `run_tests.sh`, and optionally `package.sh`, uploading the automation report
  and packaged build as artifacts. It is **dormant** until a runner with a local
  UE 5.8 install is registered and the `UE_ROOT` repo variable is set — kept
  manual and separate from `unreal-ci.yml` so the always-on PR gate stays green
  (GitHub-hosted runners cannot install the engine, so a compile bolted onto the
  PR workflow would leave every PR with a permanently pending check).

The result: the moment a UE 5.8 runner exists, `compile → test → package` is one
click, and until then nothing regresses. Local developers get the same three
commands (`build.sh` / `run_tests.sh` / `package.sh`) documented in one place.
