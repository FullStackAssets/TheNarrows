#!/usr/bin/env python3
"""Run the QUAHOG_Unreal JSON contract checks.

This validates committed data and the Python mirror of the loader rules.
It does not start Unreal, UnrealBuildTool, or a packaged game.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from slice_contract import validate_committed_slice  # noqa: E402


def main() -> int:
    summary = validate_committed_slice()
    print("QUAHOG_Unreal data contract passed.")
    print(
        "slice={slice!r} roads={roads} unnamed_roads={unnamed_roads} "
        "landmarks={landmarks} heroes={heroes_n} water_rings={water_rings} "
        "buildings={buildings} (buildingsStreamed={buildings_streamed})".format(
            heroes_n=len(summary["heroes"]),
            **summary,
        )
    )
    print("bethel_world=", summary["bethel_world"])
    print("NOTE: data check only. Unreal Editor, UBT, and PIE were not run.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
