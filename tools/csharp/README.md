# C# / Unity compile + test toolchain (sandbox)

This directory lets us **compile-check and unit-test the Unity gameplay scripts
inside the Claude Code sandbox**, where Unity itself isn't available. It is never
used by the real Unity/UBA build — it lives outside `QUAHOG_Unity/Assets`, so
Unity never sees it.

## Why it exists
The gameplay scripts reference `UnityEngine`, `UnityEngine.UI`, and
`UnityEngine.Rendering`, which we don't have here. So we:
1. Install the **.NET SDK** (Roslyn `csc`, C# 9 — matching Unity 6).
2. Compile the scripts against a hand-written **`UnityEngine` shim**
   (`shim/UnityEngineShim.cs`) that declares the exact API surface the project
   uses (types, members, signatures), with no-op bodies.
3. **Run** the pure-logic code (JSON parsing, ear-clipping, GIS projection)
   against the shim to verify behaviour, not just types.

This catches syntax/type/signature errors and logic regressions before a change
ever reaches a real Unity build.

## Usage
```bash
# Type-check all gameplay scripts (installs dotnet on first run):
tools/csharp/compile-check.sh

# Run the logic test harness (GeoJSON / triangulation / GIS build /
# shared slice + southcoast-roads contracts / playable-clip fidelity):
tools/csharp/run-tests.sh
```
Both scripts call `setup.sh`, which installs `dotnet-sdk-8.0` if missing. The
sandbox is ephemeral, so the first run in a new session reinstalls it
(~1–2 min); subsequent runs are instant.

## Layout
| Path | What |
|---|---|
| `setup.sh` | Idempotent .NET SDK installer |
| `compile-check.sh` | Type-checks all scripts via `CompileCheck.csproj` |
| `run-tests.sh` | Builds + runs `tests/` via `Tests.csproj` |
| `CompileCheck.csproj` | Compiles `shim/` + all `Assets/Scripts` as a library |
| `shim/UnityEngineShim.cs` | The UnityEngine compile-only stand-in |
| `tests/Program.cs` | Assertions over the pure-logic code paths |

## Limitations (read before trusting a green result)
- The shim has **no runtime behaviour** — method bodies return defaults. It
  proves code *compiles and links* and that *pure-logic* (math, parsing) is
  correct. It does **not** simulate physics, rendering, input, or the Unity
  lifecycle.
- It covers only the API surface the project currently uses. When new
  `UnityEngine` members are referenced, the compiler will report them as missing
  — **add them to `shim/UnityEngineShim.cs`** (a one-line stub each) and re-run.
- C# language version is pinned to **9** to match Unity 6; raising it could mask
  errors Unity would reject.
