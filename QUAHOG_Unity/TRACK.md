# QUAHOG_Unity — active Unity track

Parallel Unity 6 track (`ProjectSettings/ProjectVersion.txt` reports
`6000.0.23f1`) for the same South Coast crime game, including the Unity-to-WebGL
vertical slice added in PR #18 (`Assets/Scripts/Player`, `Vehicles`, `World/Gis`,
`World/StreetLife`, `World/TownGreybox`). See [`../ENGINES.md`](../ENGINES.md).

This sandbox did not run the Unity Editor or Unity Build Automation, so an
editor or WebGL build is not claimed. The headless C# helper is
`tools/csharp/` (`compile-check.sh`, `run-tests.sh`). It was not run for the
note that moved this status forward.

## History worth keeping

The New Bedford GIS block in the early Unity slice was a placeholder grid.
Street-life pedestrians and arcade car-feel notes from this track were also
brought into `QUAHOG_Web/`. This project remains the Unity track: player,
vehicles, GIS, and street life still live under `Assets/Scripts`.

Build-automation notes: [`BUILD_AUTOMATION.md`](BUILD_AUTOMATION.md). The WebGL
workflow `.github/workflows/deploy-webgl.yml` needs Unity license secrets before
it can produce a player.

## Older path

[`LEGACY.md`](LEGACY.md) only redirects here so existing links keep working.
