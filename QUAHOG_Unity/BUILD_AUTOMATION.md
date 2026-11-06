# QUAHOG Unity — Build Automation Setup (Unity 6 + URP)

A minimal **buildable Unity 6 project** for
[Unity Build Automation](https://docs.unity.com/devops/en/manual/build-automation)
(UBA), so you can produce a downloadable build from your phone with no local editor.

> **Project is nested.** It lives in `QUAHOG_Unity/`, not the repo root (the repo
> also holds the Godot project and prototypes). In the UBA target, set the
> **project subdirectory** to `QUAHOG_Unity`.

## Layout

```
QUAHOG_Unity/
├── ProjectSettings/
│   ├── ProjectVersion.txt        # ★ Unity 6000.0.23f1 — match UBA's dropdown
│   └── EditorBuildSettings.asset  # ★ builds Assets/Scenes/Bootstrap.unity
├── Packages/manifest.json         # ★ lists URP + uGUI + core modules
├── Assets/
│   ├── Scenes/Bootstrap.unity(.meta)   # ★ proof-of-life scene; meta GUID is in the build list
│   ├── Editor/
│   │   └── QuahogBootstrap.cs(.meta)   # editor-only: configures URP + identity (see below)
│   └── Scripts/                        # each .cs has a committed .meta
│       ├── Core/Managers.cs            # Singleton<T> + core manager singletons
│       ├── Core/GameBootstrap.cs       # runtime entry point ([RuntimeInitializeOnLoadMethod])
│       ├── SceneBootstrap.cs           # validates singletons, sets prologue state
│       └── TestSceneBootstrap.cs       # builds the minimal HUD (cash + health bar)
└── .gitignore                     # ignores Library/ Temp/ Obj/ Logs/ Build/ *.csproj *.sln
```

## Render pipeline — built-in until URP is enabled in an editor

The committed project does not assign a render pipeline asset (Graphics and
Quality settings are generated on first import). URP is in the manifest but
**not assigned**. The playable slice — GIS city, on-foot player, car, street
life — asks for `Universal Render Pipeline/Lit` and falls back to `Standard`,
then `Sprites/Default`, so a built-in player still has materials. The HUD is
uGUI screen-overlay and does not depend on the pipeline.

`Assets/Editor/QuahogBootstrap.cs` is an **editor-only** script (`#if UNITY_EDITOR`,
under `Assets/Editor/`, so it never enters a player build). On load and as an
`IPreprocessBuildWithReport` pre-build step it stamps **product identity only**
(name / company / Android bundle id `com.rockwharf.quahog`) — no pipeline changes.

When you're ready to move to URP, open the project in a Unity 6 editor and run
**`QUAHOG ▸ Enable URP`**: it creates `Assets/Settings/URP-Quahog.asset` + its renderer
via URP's API and assigns them in Graphics/Quality settings. Then commit
`Assets/Settings/` and the updated `ProjectSettings/*.asset`.

## Runtime entry point

`GameBootstrap` runs via `[RuntimeInitializeOnLoadMethod]` (no scene wiring, no
`UnityEditor`), spawns the managers, runs `SceneBootstrap`, draws the HUD, seeds `$500`.
A green build logs:

```
[GameBootstrap] QUAHOG online — managers spawned, HUD up, $500 in the wallet.
[SceneBootstrap] All singletons validated successfully.
```

## Generated on first import (intentionally NOT committed)

- `Library/`, `Temp/`, `Obj/`, `*.csproj`, `*.sln` — gitignored (never commit `Library/`; it's multi-GB).
- `Packages/packages-lock.json` — let UBA resolve it on the first build, then commit the
  generated file for deterministic resolution (hand-guessing transitive versions is worse than none).
- Most of `ProjectSettings/*.asset` (Graphics, Quality, Player, Tags, Physics, …) — Unity
  writes defaults; `QuahogBootstrap` then overlays URP + identity.

## Connect Build Automation (web dashboard, phone-friendly)

1. Unity Cloud → **DevOps → Build Automation**.
2. **Link source:** connect this GitHub repo + branch.
3. **Build configuration:**
   - **Project subdirectory:** `QUAHOG_Unity` ← required (project is nested).
   - **Unity version:** match `ProjectVersion.txt` (`6000.0.23f1`) or change that file to a Unity 6 version UBA lists.
   - **Platform:** Android (`.apk`) is easiest to sideload; debug keystore is fine.
4. **Run the build**, read the log, download the artifact / share link.
5. **After the first green build:** pull the generated `Packages/packages-lock.json`
   (and, if you opened the editor, the `ProjectSettings/*.asset` + `Assets/Settings/URP-*`)
   and commit them for fully deterministic rebuilds.

## Scope note

This track is in active development beside the other engines. The playable
slice is C# under `Assets/Scripts/` (player, car, GIS city, street life, HUD,
managers). The district mesh comes from the shared OSM clip described in
[`OSM_CONTRACT.md`](OSM_CONTRACT.md).

Opening the project, pressing Play, and producing a WebGL or device build all
require a Unity 6000.0.23f1 editor (or a dispatched GameCI run). Those were not
executed in the sandbox that added the slice. The headless gate is
[`../tools/csharp/`](../tools/csharp/README.md): `compile-check.sh` and
`run-tests.sh`. A green result type-checks the scripts and checks map parsing,
triangulation, and the clip. It does not simulate physics, rendering, or input.

SQLite is **not** referenced by any committed gameplay script (the empire DB is
a stub), so no SQLite package is needed yet. The files under `external/` are
unused by the slice.
