# The Narrows (QUAHOG)

**The Narrows** — open-world crime game on the real Massachusetts South Coast (2026).
See `README.md` and `plans/the-narrows.md`. Shippable product: `QUAHOG_GODOT1/`.

## Cursor Cloud specific instructions

### Engine tracks

All engine trees are active parallel tracks. See `ENGINES.md`. Do not describe
them as legacy, reference-only, or not actively developed.

- **`QUAHOG_Web/`** (Three.js / React Three Fiber + Rapier, Vite + TypeScript)
  is the track this sandbox can run end to end.
- **`MountHope_Unreal/`** and **`QUAHOG_Unreal/`** are UE 5.8 tracks. They are
  not runnable here without Unreal Editor. Validate `MountHope_Unreal/` with
  `Scripts/validate_scaffold.py` and `Scripts/check_cpp.py`. Do not claim a
  UBT compile or a PIE playthrough unless one actually ran.
- **`QUAHOG_Unity/`** (Unity 6 C#) and **`QUAHOG_Godot/`** / **`QUAHOG_GODOT1/`**
  (Godot GDScript) need the Unity or Godot editors and cannot be built or
  launched in this sandbox. The Unity WebGL CI workflow
  (`.github/workflows/deploy-webgl.yml`) needs Unity license secrets before it
  can produce a player. `tools/csharp/` can headless-compile Unity gameplay
  scripts; say so only after it has been run.

### Running / building the web game (commands live in `QUAHOG_Web/README.md`)

- All web commands run from `QUAHOG_Web/`: `npm run dev` (Vite dev server on
  http://localhost:5173), `npm run build`, `npm run preview`.
- **There is no separate lint script.** The type-check IS `tsc`, which runs as
  the first half of `npm run build` (`tsc && vite build`). Use `npm run build`
  to lint/type-check.
- `predev`/`prebuild` automatically run `node scripts/gen_manifests.mjs` (music
  manifests). It is zero-dependency and safe.

### Non-obvious gameplay/runtime notes

- The game starts on a title screen — you must click **NEW GAME**, then click
  the 3D canvas, before keyboard input (WASD move, `E` enter car, etc.) is
  captured. Without canvas focus, movement keys do nothing.
- The world loads from the committed `QUAHOG_Web/public/slice-newbedford.json`
  (real OSM New Bedford waterfront). **No map-data regeneration is needed to
  run the game.** Regen pipelines under `quahog-project-files/mapdata/` and
  `tools/mapgen/` are optional and require network egress to `overpass-api.de`.
- In local `vite dev` the ground often renders as a bright/over-exposed white
  plane. This is expected: the Google Static Maps satellite drape is served by
  a Vercel function (`api/staticmap.ts`) that does NOT run under `vite dev`, so
  it falls back to the procedural ground under dusk lighting + bloom. It is not
  a bug or a crash.
- The Vercel serverless functions in `QUAHOG_Web/api/` (satellite, ElevenLabs
  TTS/VO, music) only activate on a Vercel deploy with the keys in
  `QUAHOG_Web/ENV.md`. All of them degrade gracefully when keys are absent, so
  local dev needs no secrets.

### Legacy C# tooling (optional)

- `tools/csharp/` headless-compiles/tests the legacy Unity C# without Unity. Its
  scripts (`compile-check.sh`, `run-tests.sh`) self-install the .NET 8 SDK on
  first run via `tools/csharp/setup.sh`. Only relevant when touching
  `QUAHOG_Unity/Assets/Scripts` (see the `compile-check` skill).
