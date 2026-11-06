#!/usr/bin/env python3
"""Clip the shared OSM playable district into a Unity Resources file.

Reads the committed web slice and the streamed building tiles. Does not call
Overpass and does not invent coordinates.

Producers (already in the repo):
  quahog-project-files/mapdata/make_slice.py  -> QUAHOG_Web/public/slice-newbedford.json
  QUAHOG_Web/public/tiles/manifest.json       -> b_<east>_<north>.json footprints

2D points in those files are [east, north] meters about the slice origin
(lat 41.636, lon -70.9205). This clip keeps that convention.

Roads and the harbour ring are cut to an axis-aligned square. A vertex copied
from the source stays as published. A vertex that appears only because an edge
crosses x = ±half or north = ±half is the intersection of that source edge with
the square, rounded to centimeters. The harbour ring can also gain a corner of
the square when the source polygon covers that corner. That corner is not an
OSM node.
"""
import json
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SLICE = os.path.join(ROOT, "QUAHOG_Web", "public", "slice-newbedford.json")
MANIFEST = os.path.join(ROOT, "QUAHOG_Web", "public", "tiles", "manifest.json")
OUT = os.path.join(ROOT, "QUAHOG_Unity", "Assets", "Resources", "gis", "waterfront.json")

HALF = 400.0  # metres, square centered on the slice origin


def dist(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5


def r2(p):
    return [round(p[0], 2), round(p[1], 2)]


def inside_point(p):
    return -HALF <= p[0] <= HALF and -HALF <= p[1] <= HALF


def liang_barsky(p0, p1):
    x0, y0 = p0
    x1, y1 = p1
    dx, dy = x1 - x0, y1 - y0
    p = (-dx, dx, -dy, dy)
    q = (x0 - (-HALF), HALF - x0, y0 - (-HALF), HALF - y0)
    u1, u2 = 0.0, 1.0
    for pi, qi in zip(p, q):
        if abs(pi) < 1e-12:
            if qi < 0:
                return None
            continue
        t = qi / pi
        if pi < 0:
            if t > u2:
                return None
            if t > u1:
                u1 = t
        else:
            if t < u1:
                return None
            if t < u2:
                u2 = t
    return (x0 + u1 * dx, y0 + u1 * dy), (x0 + u2 * dx, y0 + u2 * dy)


def clip_polyline(pts):
    chains = []
    cur = []

    def push(pt):
        q = r2(pt)
        if not cur or dist(cur[-1], q) > 0.02:
            cur.append(q)

    for i in range(len(pts) - 1):
        seg = liang_barsky(pts[i], pts[i + 1])
        if seg is None:
            if len(cur) >= 2:
                chains.append(cur)
            cur = []
            continue
        a, b = seg
        if cur and dist(cur[-1], r2(a)) > 0.05:
            if len(cur) >= 2:
                chains.append(cur)
            cur = []
        push(a)
        push(b)
    if len(cur) >= 2:
        chains.append(cur)
    return chains


def clip_polygon(poly):
    def clip_edge(pts, inside, intersect):
        if not pts:
            return []
        out = []
        prev = pts[-1]
        prev_in = inside(prev)
        for cur in pts:
            cur_in = inside(cur)
            if cur_in:
                if not prev_in:
                    out.append(intersect(prev, cur))
                out.append(cur)
            elif prev_in:
                out.append(intersect(prev, cur))
            prev, prev_in = cur, cur_in
        return out

    def ix(a, b, x):
        dx = b[0] - a[0]
        t = 0.0 if abs(dx) < 1e-12 else (x - a[0]) / dx
        return [x, a[1] + t * (b[1] - a[1])]

    def iy(a, b, y):
        dy = b[1] - a[1]
        t = 0.0 if abs(dy) < 1e-12 else (y - a[1]) / dy
        return [a[0] + t * (b[0] - a[0]), y]

    pts = [list(p) for p in poly]
    if len(pts) >= 2 and dist(pts[0], pts[-1]) < 1e-6:
        pts = pts[:-1]
    pts = clip_edge(pts, lambda p: p[0] >= -HALF, lambda a, b: ix(a, b, -HALF))
    pts = clip_edge(pts, lambda p: p[0] <= HALF, lambda a, b: ix(a, b, HALF))
    pts = clip_edge(pts, lambda p: p[1] >= -HALF, lambda a, b: iy(a, b, -HALF))
    pts = clip_edge(pts, lambda p: p[1] <= HALF, lambda a, b: iy(a, b, HALF))
    out = []
    for p in pts:
        q = r2(p)
        if not out or dist(out[-1], q) > 0.02:
            out.append(q)
    if len(out) >= 2 and dist(out[0], out[-1]) <= 0.02:
        out = out[:-1]
    return out if len(out) >= 3 else []


def centroid(ring):
    n = len(ring)
    return sum(p[0] for p in ring) / n, sum(p[1] for p in ring) / n


def main():
    slice_doc = json.load(open(SLICE, encoding="utf-8"))
    manifest = json.load(open(MANIFEST, encoding="utf-8"))
    tile_m = manifest["tile"]
    # Tiles whose squares can overlap the clip. floor division matches
    # QUAHOG_Web/src/world/StreamingBuildings.tsx (gx = floor(east / tile)).
    keys = set(manifest["keys"])
    g0 = int(-HALF // tile_m) - 1
    g1 = int(HALF // tile_m) + 1
    tile_keys = []
    for gx in range(g0, g1 + 1):
        for gn in range(g0, g1 + 1):
            k = f"{gx}_{gn}"
            if k in keys:
                tile_keys.append(k)

    roads = []
    for i, road in enumerate(slice_doc["roads"]):
        for chain in clip_polyline(road["points"]):
            if len(chain) < 2:
                continue
            item = {
                "highway": road.get("highway"),
                "width": road.get("width"),
                "name": road.get("name"),
                "points": chain,
                "sourceIndex": i,
            }
            if road.get("bridge"):
                item["bridge"] = True
                if "layer" in road:
                    item["layer"] = road["layer"]
            roads.append(item)

    buildings = []
    for key in tile_keys:
        path = os.path.join(ROOT, "QUAHOG_Web", "public", "tiles", f"b_{key}.json")
        tile = json.load(open(path, encoding="utf-8"))
        for idx, b in enumerate(tile):
            fp = b.get("footprint") or []
            if len(fp) < 3:
                continue
            cx, cy = centroid(fp)
            if not inside_point((cx, cy)):
                continue
            item = {
                "footprint": fp,
                "height": b.get("height"),
                "sourceTile": key,
                "sourceIndex": idx,
            }
            if b.get("name"):
                item["name"] = b["name"]
            buildings.append(item)

    water = []
    water_source = []
    for i, ring in enumerate(slice_doc["water"]):
        clipped = clip_polygon(ring)
        if len(clipped) >= 3:
            water.append(clipped)
            water_source.append(i)

    landmarks = []
    for i, lm in enumerate(slice_doc["landmarks"]):
        pos = lm["pos"]
        if not inside_point(pos):
            continue
        item = {
            "name": lm["name"],
            "kind": lm.get("kind"),
            "pos": pos,
            "sourceIndex": i,
        }
        if lm.get("hero"):
            item["hero"] = True
        landmarks.append(item)

    doc = {
        "contract": "unity-playable-clip",
        "name": slice_doc.get("name"),
        "origin": slice_doc["origin"],
        "meters_per_degree": slice_doc["meters_per_degree"],
        "axes": "x=east(m), y=up, z=north(m). 2D points are [east, north].",
            "note": (
            "Unity keeps north as +Z. The Three.js slice consumer "
            "(QUAHOG_Web/src/slice.ts toWorld) stores the same 2D point as z=-north. "
            "Road and water vertices on the square boundary are edge intersections, "
            "not additional OSM nodes. A harbour vertex may also be a corner of the "
            "clip square when the source polygon covers that corner."
        ),
        "attribution": slice_doc.get("attribution"),
        "buildingsStreamed": False,
        "provenance": {
            "slice": "QUAHOG_Web/public/slice-newbedford.json",
            "buildingManifest": "QUAHOG_Web/public/tiles/manifest.json",
            "buildingTiles": [f"QUAHOG_Web/public/tiles/b_{k}.json" for k in tile_keys],
            "tileMeters": tile_m,
            "clipHalfExtentMeters": HALF,
            "clipCenterEastNorth": [0, 0],
            "generator": "QUAHOG_Unity/tools/extract_playable_slice.py",
            "producers": [
                "quahog-project-files/mapdata/make_slice.py",
                "QUAHOG_Web/public/tiles",
            ],
            "waterSourceIndex": water_source,
            "overpassFetched": False,
        },
        "buildings": buildings,
        "roads": roads,
        "water": water,
        "landmarks": landmarks,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
        f.write("\n")
    print(
        f"wrote {OUT}: roads={len(roads)} buildings={len(buildings)} "
        f"water={len(water)} landmarks={len(landmarks)} "
        f"bytes={os.path.getsize(OUT)}"
    )


if __name__ == "__main__":
    main()
