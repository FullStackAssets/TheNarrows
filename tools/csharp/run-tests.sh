#!/usr/bin/env bash
# Build + run the pure-logic test harness (GeoJSON parsing, ear-clipping, GIS city
# build). Exit code is non-zero if any assertion fails.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$DIR/../.." && pwd)"

"$DIR/setup.sh"

JSON="$ROOT/QUAHOG_Unity/Assets/Resources/gis/newbedford.json"
SLICE="$ROOT/QUAHOG_Web/public/slice-newbedford.json"
ROADS="$ROOT/quahog-project-files/mapdata/southcoast-roads.json"
CLIP="$ROOT/QUAHOG_Unity/Assets/Resources/gis/waterfront.json"
TILES="$ROOT/QUAHOG_Web/public/tiles"
echo "=== Running QUAHOG sandbox tests ==="
dotnet run --project "$DIR/tests/Tests.csproj" -- "$JSON" "$SLICE" "$ROADS" "$CLIP" "$TILES"
