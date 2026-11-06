# The Narrows

An original 3D open-world game set on the **real Massachusetts South Coast**
(New Bedford, Fall River, Brockton, Cape Cod) — walk real streets and real
landmarks, populated by original characters, with GTA-style walk/drive
mechanics. *GTA: Vice City is only a tonal reference; this is original work.*

**The Narrows** — subtitle *South Coast · Now* (2026). Named for the Taunton River
narrows between Fall River and Somerset; **Mount Hope Bay** remains in-world geography.
*(Repo/dir names still use the legacy "QUAHOG" working title; see `plans/the-narrows.md`.)*

## Engines — parallel tracks

The game is developed on several **active parallel tracks**. See
[`ENGINES.md`](ENGINES.md). The browser project is the one this environment can
run. Unreal, Unity, and Godot need their editors, which were not run here, so
this README does not claim those builds are green.

### `QUAHOG_Web/` — browser
**Three.js / React Three Fiber** + Rapier. Third-person walk, car enter/drive,
collision, on the **real New Bedford waterfront** street grid + OSM building
blockout, with ambient pedestrians and traffic.
**Recorded deploy:** https://projectsouthcoast.vercel.app · setup: [`QUAHOG_Web/README.md`](QUAHOG_Web/README.md)

### `MountHope_Unreal/` — PC/console vertical slice
Unreal Engine 5.8 C++ slice: missions, NPCs, economy, dialogue, collectibles,
and an OSM import contract. Python checks run without the editor
(`MountHope_Unreal/Scripts/validate_scaffold.py`, `check_cpp.py`). Compiling
and playing need a local Unreal 5.8 install.

### Map data — `quahog-project-files/mapdata/`
The canonical OpenStreetMap pipeline feeding the web game: real road/water/
building geometry for New Bedford & Fall River → GeoJSON / PMTiles / OBJ and the
slice blockout JSON. See [`quahog-project-files/mapdata/README.md`](quahog-project-files/mapdata/README.md).

### `quahog-project-files/`
Design and pitch materials: the Game Design Document, Master Plan, pitch
one-pager, and system-design PDFs.

## More engine tracks

- `QUAHOG_Unreal/` — parallel UE 5.8 project; the earlier bootstrap whose patterns also live in `MountHope_Unreal/`. See [`QUAHOG_Unreal/TRACK.md`](QUAHOG_Unreal/TRACK.md).
- `QUAHOG_Godot/` — parallel Godot 4 (GDScript) port. See [`QUAHOG_Godot/TRACK.md`](QUAHOG_Godot/TRACK.md).
- `QUAHOG_GODOT1/` — parallel Godot 4.6 project ("The Narrows").
- `QUAHOG_Unity/` — parallel Unity 6 project, including the Unity→WebGL slice. See [`QUAHOG_Unity/TRACK.md`](QUAHOG_Unity/TRACK.md).
- `tools/csharp/`, `tools/mapgen/` — headless C# compile/test, and a Unity-oriented OSM fetcher. The shared web/Unreal map artifacts come from `quahog-project-files/mapdata/`.
- `prototypes/Quahog3D.html` — standalone offline Three.js prototype.

---

**Unofficial fan parody / original work.** Not affiliated with, endorsed by, or
connected to Rockstar Games or Take-Two Interactive. Map data ©
OpenStreetMap contributors, ODbL.
