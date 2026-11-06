// Runnable verification of the project's pure-logic code, executed against the
// UnityEngine shim. Run via tools/csharp/run-tests.sh (passes the sample GeoJSON
// path as arg 0). Exit code is non-zero if any assertion fails.
using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using Quahog.SouthCoast;
using Quahog.SouthCoast.Gis;

internal static class Program
{
    private static int _passed;
    private static int _failed;

    private static void Check(string name, bool cond)
    {
        if (cond) { _passed++; Console.WriteLine($"  PASS  {name}"); }
        else { _failed++; Console.WriteLine($"  FAIL  {name}"); }
    }

    private static int Main(string[] args)
    {
        Console.WriteLine("=== QUAHOG sandbox tests ===");

        TestMiniJsonAndGeoJson(args);
        TestEarClipper();
        TestHolesAndCategories();
        TestGisCityBuild(args);
        TestSharedMapContracts(args);
        TestPlayableClipFidelity(args);
        TestStreetRoutes();
        TestGisCityOnClip(args);

        Console.WriteLine($"\n=== {_passed} passed, {_failed} failed ===");
        return _failed == 0 ? 0 : 1;
    }

    // A fixed fixture so the parser assertions are stable regardless of whatever
    // real city data is currently baked into newbedford.json.
    private const string FixtureJson = @"{
      ""type"":""FeatureCollection"",""features"":[
        {""type"":""Feature"",""properties"":{""highway"":""primary"",""name"":""Main""},
         ""geometry"":{""type"":""LineString"",""coordinates"":[[-70.92,41.636],[-70.91,41.636]]}},
        {""type"":""Feature"",""properties"":{""highway"":""residential"",""name"":""Side""},
         ""geometry"":{""type"":""LineString"",""coordinates"":[[-70.92,41.637],[-70.91,41.637]]}},
        {""type"":""Feature"",""properties"":{""building"":""yes"",""height"":""30""},
         ""geometry"":{""type"":""Polygon"",""coordinates"":[[[-70.92,41.635],[-70.919,41.635],[-70.919,41.636],[-70.92,41.636],[-70.92,41.635]]]}},
        {""type"":""Feature"",""properties"":{""building"":""yes"",""building:levels"":""5""},
         ""geometry"":{""type"":""Polygon"",""coordinates"":[[[-70.918,41.635],[-70.917,41.635],[-70.917,41.636],[-70.918,41.636],[-70.918,41.635]]]}},
        {""type"":""Feature"",""properties"":{""natural"":""water""},
         ""geometry"":{""type"":""Polygon"",""coordinates"":[[[-70.915,41.635],[-70.914,41.635],[-70.914,41.638],[-70.915,41.638],[-70.915,41.635]]]}}
      ]}";

    // ---- GeoJSON parsing (against the stable fixture) --------------------
    private static void TestMiniJsonAndGeoJson(string[] args)
    {
        Console.WriteLine("\n[GeoJSON parse — fixture]");
        GeoData data = GeoJson.Parse(FixtureJson);
        Check($"roads == 2 (got {data.Roads.Count})", data.Roads.Count == 2);
        Check($"buildings == 2 (got {data.Buildings.Count})", data.Buildings.Count == 2);
        Check($"water == 1 (got {data.Water.Count})", data.Water.Count == 1);

        bool sawTall = false, sawLevels = false;
        foreach (var b in data.Buildings)
        {
            if (Math.Abs(b.Height - 30f) < 0.01f) sawTall = true;
            if (Math.Abs(b.Height - 16f) < 0.01f) sawLevels = true;  // 5 levels * 3.2
        }
        Check("explicit height=30 parsed", sawTall);
        Check("building:levels=5 -> 16m", sawLevels);

        float primary = 0, residential = 0;
        foreach (var r in data.Roads)
        {
            if (r.Name == "Main") primary = r.Width;
            if (r.Name == "Side") residential = r.Width;
        }
        Check($"primary wider than residential ({primary} > {residential})", primary > residential);

        // The live baked dataset (placeholder or real city) must be non-trivial.
        Console.WriteLine("[GeoJSON parse — live dataset]");
        string live = LoadSample(args);
        if (live == null) { Check("live GeoJSON found", false); return; }
        Check("live GeoJSON found", true);
        GeoData ld = GeoJson.Parse(live);
        Check($"live has roads (got {ld.Roads.Count})", ld.Roads.Count > 0);
        Check($"live has buildings (got {ld.Buildings.Count})", ld.Buildings.Count > 0);
    }

    // ---- Ear clipping ----------------------------------------------------
    private static void TestEarClipper()
    {
        Console.WriteLine("\n[Ear clipping]");

        // A convex square -> 2 triangles (6 indices).
        var square = new[]
        {
            new Vector2(0, 0), new Vector2(10, 0), new Vector2(10, 10), new Vector2(0, 10)
        };
        int[] sq = EarClipper.Triangulate(square);
        Check($"square -> 6 indices (got {sq.Length})", sq.Length == 6);
        Check("square indices in range", AllInRange(sq, square.Length));

        // A concave L-shape (6 vertices) -> 4 triangles (12 indices).
        var lshape = new[]
        {
            new Vector2(0, 0), new Vector2(20, 0), new Vector2(20, 10),
            new Vector2(10, 10), new Vector2(10, 20), new Vector2(0, 20)
        };
        int[] ls = EarClipper.Triangulate(lshape);
        Check($"L-shape -> 12 indices (got {ls.Length})", ls.Length == 12);
        Check("L-shape indices in range", AllInRange(ls, lshape.Length));

        // Degenerate (2 points) -> no triangles, no crash.
        int[] degen = EarClipper.Triangulate(new[] { new Vector2(0, 0), new Vector2(1, 1) });
        Check("degenerate -> 0 indices", degen.Length == 0);

        // Clockwise winding still triangulates (auto-orient).
        var cw = new[] { new Vector2(0, 0), new Vector2(0, 10), new Vector2(10, 10), new Vector2(10, 0) };
        Check("clockwise square -> 6 indices", EarClipper.Triangulate(cw).Length == 6);
    }

    // ---- Holes (courtyards/islands) + building categories ---------------
    private static void TestHolesAndCategories()
    {
        Console.WriteLine("\n[Holes + categories]");

        // A 10x10 square with a 4x4 hole -> triangulation should cover 100-16 = 84.
        var outer = new[]
        {
            new Vector2(0, 0), new Vector2(10, 0), new Vector2(10, 10), new Vector2(0, 10)
        };
        var hole = new Vector2[] // clockwise on purpose; EnsureWinding must fix it
        {
            new Vector2(3, 3), new Vector2(3, 7), new Vector2(7, 7), new Vector2(7, 3)
        };
        int[] idx = EarClipper.Triangulate(outer, new List<Vector2[]> { hole }, out Vector2[] merged);
        Check($"holed square merged has 10 verts (got {merged.Length})", merged.Length == 10);
        Check("holed square indices in range", AllInRange(idx, merged.Length));
        float area = TriArea(merged, idx);
        Check($"holed square area ~84 (got {area:0.0})", Math.Abs(area - 84f) < 0.5f);

        // No holes via the overload must match the plain triangulator.
        int[] plain = EarClipper.Triangulate(outer, new List<Vector2[]>(), out Vector2[] m2);
        Check($"no-hole overload -> 6 indices (got {plain.Length})", plain.Length == 6 && m2.Length == 4);

        // GeoJSON Polygon with an inner ring -> one building with one hole.
        const string holeJson = @"{""type"":""FeatureCollection"",""features"":[
          {""type"":""Feature"",""properties"":{""building"":""yes"",""height"":""12""},
           ""geometry"":{""type"":""Polygon"",""coordinates"":[
             [[-70.92,41.63],[-70.918,41.63],[-70.918,41.632],[-70.92,41.632],[-70.92,41.63]],
             [[-70.9195,41.6305],[-70.9185,41.6305],[-70.9185,41.6315],[-70.9195,41.6315],[-70.9195,41.6305]]
           ]}}]}";
        GeoData hd = GeoJson.Parse(holeJson);
        Check($"holed building parsed (got {hd.Buildings.Count})", hd.Buildings.Count == 1);
        Check($"building has 1 hole (got {(hd.Buildings.Count > 0 ? hd.Buildings[0].Holes.Count : -1)})",
              hd.Buildings.Count > 0 && hd.Buildings[0].Holes.Count == 1);

        // Categories from the building tag.
        Check("building=industrial -> Industrial", CatOf("industrial") == BuildingCategory.Industrial);
        Check("building=house -> Residential", CatOf("house") == BuildingCategory.Residential);
        Check("building=retail -> Commercial", CatOf("retail") == BuildingCategory.Commercial);
        Check("building=church -> Civic", CatOf("church") == BuildingCategory.Civic);
        Check("building=yes -> Default", CatOf("yes") == BuildingCategory.Default);

        // Untagged-height buildings of different categories get different defaults.
        float hRes = HeightOf("house"), hCivic = HeightOf("church");
        Check($"residential default < civic default ({hRes:0.0} < {hCivic:0.0})", hRes < hCivic);
    }

    private static BuildingCategory CatOf(string buildingTag)
    {
        string j = "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"properties\":{\"building\":\""
            + buildingTag + "\"},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[-70.92,41.63],[-70.919,41.63],[-70.919,41.631],[-70.92,41.631],[-70.92,41.63]]]}}]}";
        GeoData d = GeoJson.Parse(j);
        return d.Buildings.Count > 0 ? d.Buildings[0].Category : BuildingCategory.Default;
    }

    private static float HeightOf(string buildingTag)
    {
        string j = "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\",\"properties\":{\"building\":\""
            + buildingTag + "\"},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[-70.92,41.63],[-70.919,41.63],[-70.919,41.631],[-70.92,41.631],[-70.92,41.63]]]}}]}";
        GeoData d = GeoJson.Parse(j);
        return d.Buildings.Count > 0 ? d.Buildings[0].Height : 0f;
    }

    private static float TriArea(Vector2[] pts, int[] idx)
    {
        float sum = 0f;
        for (int t = 0; t + 2 < idx.Length; t += 3)
        {
            Vector2 a = pts[idx[t]], b = pts[idx[t + 1]], c = pts[idx[t + 2]];
            sum += Math.Abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
        }
        return sum;
    }

    // ---- Full GIS city build (smoke) ------------------------------------
    private static void TestGisCityBuild(string[] args)
    {
        Console.WriteLine("\n[GisCity build]");
        string json = LoadSample(args);
        if (json == null) { Check("sample for build", false); return; }

        GisCity city = null;
        Exception err = null;
        try { city = GisCity.Build(json); }
        catch (Exception e) { err = e; }

        Check("GisCity.Build did not throw", err == null);
        if (err != null) { Console.WriteLine("    " + err.Message); return; }
        Check("GisCity.Build returned a city", city != null);
        if (city == null) return;

        // Spawns must be finite and lifted off the ground.
        Check("PlayerSpawn finite", IsFinite(city.PlayerSpawn));
        Check("VehicleSpawn finite", IsFinite(city.VehicleSpawn));
        Check("PlayerSpawn ~1m up", Math.Abs(city.PlayerSpawn.y - 1f) < 0.001f);
        Check("VehicleSpawn ~0.5m up", Math.Abs(city.VehicleSpawn.y - 0.5f) < 0.001f);

        // Empty data should return null (fallback path), not throw.
        GisCity empty = GisCity.Build("{\"type\":\"FeatureCollection\",\"features\":[]}");
        Check("empty FeatureCollection -> null (greybox fallback)", empty == null);
    }

    // ---- Shared OSM contracts (real committed files, not fixtures) -------
    private static void TestSharedMapContracts(string[] args)
    {
        Console.WriteLine("\n[Shared map contracts]");
        string slicePath = Arg(args, 1);
        string roadsPath = Arg(args, 2);
        Check("slice file present", slicePath != null);
        Check("road-graph file present", roadsPath != null);
        if (slicePath == null || roadsPath == null) return;

        MapDocument slice = MapContracts.Parse(File.ReadAllText(slicePath));
        Check("slice parsed", slice != null);
        if (slice == null) return;
        Check("slice contract is web-slice", slice.Contract == "web-slice");
        Check("slice origin lat is make_slice 41.636", Math.Abs(slice.OriginLat - 41.636) < 1e-9);
        Check("slice origin lon is make_slice -70.9205", Math.Abs(slice.OriginLon - (-70.9205)) < 1e-9);
        Check("slice metres/deg lat is 111320", Math.Abs(slice.MetersPerDegreeLat - 111320.0) < 1e-6);
        Check($"slice metres/deg lon ~83198.429 (got {slice.MetersPerDegreeLon:0.000})",
              Math.Abs(slice.MetersPerDegreeLon - 83198.429) < 0.01);
        Check("slice attributes OpenStreetMap",
              slice.Attribution != null && slice.Attribution.Contains("OpenStreetMap"));
        Check("slice buildings are streamed (array empty)", slice.BuildingsStreamed && slice.Buildings.Count == 0);
        Check($"slice has a real road count (got {slice.Roads.Count})", slice.Roads.Count > 10000);
        Check($"slice has water (got {slice.Water.Count})", slice.Water.Count > 100);
        Check("slice heroes include Seamen's Bethel", HeroAt(slice, "Seamen's Bethel", -272.23f, -106.5f));
        Check("slice heroes include the Whaling Museum", HeroAt(slice, "New Bedford Whaling Museum", -219.18f, -106.72f));

        ProjectedPoint sample = slice.Roads[0].Points[0];
        Vector3 unity = MapContracts.ToUnity(sample, 0f);
        Check("Unity x = east", Math.Abs(unity.x - sample.East) < 1e-4f);
        Check("Unity z = north (not Three.js -north)", Math.Abs(unity.z - sample.North) < 1e-4f
              && Math.Abs(sample.North) > 1f
              && Math.Abs(unity.z + sample.North) > 1f);

        MapDocument graph = MapContracts.Parse(File.ReadAllText(roadsPath));
        Check("road graph parsed", graph != null);
        if (graph == null) return;
        Check("road graph contract is southcoast-roads", graph.Contract == "southcoast-roads");
        Check($"declared road_count matches array ({graph.DeclaredRoadCount} vs {graph.Roads.Count})",
              graph.DeclaredRoadCount == graph.Roads.Count && graph.Roads.Count > 10000);
        Check("road graph axes name z=north", graph.Axes != null && graph.Axes.Contains("z=north"));
        Check("road graph attributes OpenStreetMap",
              graph.Attribution != null && graph.Attribution.Contains("OpenStreetMap"));
        Check("road graph origin is the bake centre",
              Math.Abs(graph.OriginLat - 41.74524735) < 1e-6 && Math.Abs(graph.OriginLon - (-71.0503516)) < 1e-6);
        ProjectedRoad first = graph.Roads[0];
        Check("first baked road is Copicut Road", first.Name == "Copicut Road" && first.Highway == "residential");
        Check("first baked point is the published [east, north]",
              first.Points.Count > 2
              && Math.Abs(first.Points[0].East - 815.09f) < 0.02f
              && Math.Abs(first.Points[0].North - (-5638.73f)) < 0.02f);
        Check("baked width is metres from bake_meshes", Math.Abs(first.Width - 6f) < 0.01f);
    }

    private static void TestPlayableClipFidelity(string[] args)
    {
        Console.WriteLine("\n[Playable clip vs source]");
        string slicePath = Arg(args, 1);
        string clipPath = Arg(args, 3);
        string tilesDir = Arg(args, 4);
        Check("clip file present", clipPath != null && slicePath != null && tilesDir != null);
        if (slicePath == null || clipPath == null || tilesDir == null) return;

        MapDocument slice = MapContracts.Parse(File.ReadAllText(slicePath));
        MapDocument clip = MapContracts.Parse(File.ReadAllText(clipPath));
        Check("clip parsed as unity-playable-clip", clip != null && clip.Contract == "unity-playable-clip");
        if (slice == null || clip == null) return;
        Check("clip keeps the slice origin", Math.Abs(clip.OriginLat - slice.OriginLat) < 1e-9
              && Math.Abs(clip.OriginLon - slice.OriginLon) < 1e-9);
        Check("clip half-extent is 400 m", Math.Abs(clip.ClipHalfExtent - 400f) < 0.01f);
        Check("clip inlines buildings (not streamed)", !clip.BuildingsStreamed && clip.Buildings.Count > 40);
        Check($"clip roads are a district, not the whole coast (got {clip.Roads.Count})",
              clip.Roads.Count > 50 && clip.Roads.Count < slice.Roads.Count);
        Check("clip keeps the five hero landmarks",
              HeroAt(clip, "Seamen's Bethel", -272.23f, -106.5f)
              && HeroAt(clip, "Double Bank Building", -184.47f, -28.25f)
              && HeroAt(clip, "Rodman Candleworks", -186.74f, 1.72f)
              && HeroAt(clip, "Mariner's Home", -270.2f, -89.13f)
              && HeroAt(clip, "New Bedford Whaling Museum", -219.18f, -106.72f));

        int offRoad = 0, outside = 0;
        foreach (var road in clip.Roads)
        {
            if (road.SourceIndex < 0 || road.SourceIndex >= slice.Roads.Count) { offRoad++; continue; }
            var source = slice.Roads[road.SourceIndex];
            foreach (var p in road.Points)
            {
                if (Math.Abs(p.East) > clip.ClipHalfExtent + 0.05f || Math.Abs(p.North) > clip.ClipHalfExtent + 0.05f)
                    outside++;
                if (DistToPolyline(p, source.Points) > 0.08f) offRoad++;
            }
        }
        Check($"every clip road vertex lies on its source polyline (off={offRoad})", offRoad == 0);
        Check($"clip road vertices stay in the square (outside={outside})", outside == 0);

        int waterOff = 0;
        Check("clip has the harbour ring", clip.Water.Count == 1 && clip.Water[0].SourceIndex >= 0
              && clip.Water[0].SourceIndex < slice.Water.Count);
        if (clip.Water.Count == 1 && clip.Water[0].SourceIndex >= 0 && clip.Water[0].SourceIndex < slice.Water.Count)
        {
            var source = slice.Water[clip.Water[0].SourceIndex].Outer;
            float half = clip.ClipHalfExtent;
            foreach (var p in clip.Water[0].Outer)
            {
                if (DistToPolyline(p, source) <= 0.08f) continue;
                // A covered corner of the clip square is inserted by the polygon
                // clip. It is not an OSM vertex and does not lie on the ring.
                bool corner = Near(Math.Abs(p.East), half) && Near(Math.Abs(p.North), half);
                if (corner && PointInRing(p, source)) continue;
                waterOff++;
            }
        }
        Check($"harbour vertices are on the source ring or a covered clip corner (off={waterOff})", waterOff == 0);

        int lmOff = 0;
        foreach (var lm in clip.Landmarks)
        {
            if (lm.SourceIndex < 0 || lm.SourceIndex >= slice.Landmarks.Count) { lmOff++; continue; }
            var src = slice.Landmarks[lm.SourceIndex];
            if (src.Name != lm.Name || Math.Abs(src.Pos.East - lm.Pos.East) > 0.02f
                || Math.Abs(src.Pos.North - lm.Pos.North) > 0.02f)
                lmOff++;
        }
        Check("clip landmarks match the slice entries", lmOff == 0 && clip.Landmarks.Count >= 5);

        int bOff = 0, bChecked = 0;
        foreach (var b in clip.Buildings)
        {
            if (string.IsNullOrEmpty(b.SourceTile) || b.SourceIndex < 0) { bOff++; continue; }
            string tilePath = Path.Combine(tilesDir, "b_" + b.SourceTile + ".json");
            if (!File.Exists(tilePath)) { bOff++; continue; }
            var tile = MapContracts.ParseBuildingTile(File.ReadAllText(tilePath));
            if (b.SourceIndex >= tile.Count) { bOff++; continue; }
            var src = tile[b.SourceIndex];
            bChecked++;
            if (src.Footprint.Count != b.Footprint.Count || Math.Abs(src.Height - b.Height) > 0.01f)
            {
                bOff++;
                continue;
            }
            for (int i = 0; i < b.Footprint.Count; i++)
            {
                if (Math.Abs(src.Footprint[i].East - b.Footprint[i].East) > 0.02f
                    || Math.Abs(src.Footprint[i].North - b.Footprint[i].North) > 0.02f)
                {
                    bOff++;
                    break;
                }
            }
        }
        Check($"building footprints match tile files ({bChecked} checked, off={bOff})", bChecked > 40 && bOff == 0);
    }

    private static void TestStreetRoutes()
    {
        Console.WriteLine("\n[Street routes]");
        Check("residential is drivable", StreetRoutes.IsDrivable("residential"));
        Check("service is drivable", StreetRoutes.IsDrivable("service"));
        Check("footway is not a traffic route", !StreetRoutes.IsDrivable("footway"));

        var road = new ProjectedRoad { Highway = "residential", Width = 8f, Oneway = false };
        road.Points.Add(new ProjectedPoint(0f, 0f));
        road.Points.Add(new ProjectedPoint(10f, 0f));
        var routes = StreetRoutes.FromRoads(new List<ProjectedRoad> { road });
        Check("straight road becomes one route", routes.Count == 1 && Math.Abs(routes[0].Length - 10f) < 0.01f);
        routes[0].Sample(5f, true, out Vector3 pos, out Vector3 dir);
        Check("sample at 5 m is the midpoint, +X",
              Math.Abs(pos.x - 5f) < 0.01f && Math.Abs(pos.z) < 0.01f && Math.Abs(pos.y) < 0.01f
              && Math.Abs(dir.x - 1f) < 0.01f && Math.Abs(dir.z) < 0.01f);

        StreetRoutes.Advance(9f, 3f, 10f, false, true, out float bounced, out bool fwd);
        Check($"two-way advance ping-pongs (d={bounced:0.00}, fwd={fwd})",
              Math.Abs(bounced - 8f) < 0.01f && !fwd);
        StreetRoutes.Advance(9f, 3f, 10f, true, true, out float wrapped, out bool still);
        Check($"one-way advance wraps (d={wrapped:0.00})", Math.Abs(wrapped - 2f) < 0.01f && still);

        var walk = new ProjectedRoad { Highway = "footway", Width = 2f };
        walk.Points.Add(new ProjectedPoint(0f, 0f));
        walk.Points.Add(new ProjectedPoint(0f, 30f));
        Check("footway is dropped from traffic", StreetRoutes.FromRoads(new List<ProjectedRoad> { walk }).Count == 0);
    }

    private static void TestGisCityOnClip(string[] args)
    {
        Console.WriteLine("\n[GisCity on OSM clip]");
        string clipPath = Arg(args, 3);
        if (clipPath == null) { Check("clip for city build", false); return; }
        string json = File.ReadAllText(clipPath);
        GisCity city = null;
        Exception err = null;
        try { city = GisCity.Build(json); }
        catch (Exception e) { err = e; }
        Check("clip GisCity.Build did not throw", err == null);
        if (err != null) { Console.WriteLine("    " + err); return; }
        Check("clip build returned a city", city != null && city.Contract == "unity-playable-clip");
        if (city == null) return;
        Check("clip player/vehicle spawns finite", IsFinite(city.PlayerSpawn) && IsFinite(city.VehicleSpawn));
        Check("clip player is 1 m up", Math.Abs(city.PlayerSpawn.y - 1f) < 0.001f);
        Check("clip spawns sit inside the 400 m square",
              Math.Abs(city.PlayerSpawn.x) <= 420f && Math.Abs(city.PlayerSpawn.z) <= 420f
              && Math.Abs(city.VehicleSpawn.x) <= 420f && Math.Abs(city.VehicleSpawn.z) <= 420f);
        float dx = city.PlayerSpawn.x - (-272.23f);
        float dz = city.PlayerSpawn.z - (-106.5f);
        float dist = (float)Math.Sqrt(dx * dx + dz * dz);
        Check($"player spawn is near Seamen's Bethel (dist {dist:0.0} m)", dist < 40f);
        Check($"clip exposes drivable routes (got {city.TrafficRoutes.Count})", city.TrafficRoutes.Count > 10);

        // The hand-authored grid must stay distinguishable from the OSM clip.
        string grid = Arg(args, 0);
        if (grid != null)
        {
            MapDocument g = MapContracts.Parse(File.ReadAllText(grid));
            Check("fallback grid is geojson, not the slice",
                  g != null && g.Contract == "geojson" && g.Roads.Count < 20 && g.Roads.Count > 0);
        }
    }

    private static bool HeroAt(MapDocument doc, string name, float east, float north)
    {
        foreach (var lm in doc.Landmarks)
        {
            if (lm.Name == name && lm.Hero
                && Math.Abs(lm.Pos.East - east) < 0.02f
                && Math.Abs(lm.Pos.North - north) < 0.02f)
                return true;
        }
        return false;
    }

    private static bool Near(float a, float b) => Math.Abs(a - b) < 0.05f;

    private static bool PointInRing(ProjectedPoint p, List<ProjectedPoint> ring)
    {
        bool inside = false;
        for (int i = 0, j = ring.Count - 1; i < ring.Count; j = i++)
        {
            float yi = ring[i].North, yj = ring[j].North;
            if ((yi > p.North) == (yj > p.North)) continue;
            float xi = ring[i].East, xj = ring[j].East;
            float xCross = (xj - xi) * (p.North - yi) / (yj - yi) + xi;
            if (p.East < xCross) inside = !inside;
        }
        return inside;
    }

    private static float DistToPolyline(ProjectedPoint p, List<ProjectedPoint> line)
    {
        float best = float.MaxValue;
        for (int i = 0; i + 1 < line.Count; i++)
        {
            float d = DistToSeg(p, line[i], line[i + 1]);
            if (d < best) best = d;
        }
        return best;
    }

    private static float DistToSeg(ProjectedPoint p, ProjectedPoint a, ProjectedPoint b)
    {
        float abx = b.East - a.East, aby = b.North - a.North;
        float apx = p.East - a.East, apy = p.North - a.North;
        float ab2 = abx * abx + aby * aby;
        float t = ab2 < 1e-8f ? 0f : (apx * abx + apy * aby) / ab2;
        if (t < 0f) t = 0f;
        if (t > 1f) t = 1f;
        float dx = a.East + abx * t - p.East;
        float dy = a.North + aby * t - p.North;
        return (float)Math.Sqrt(dx * dx + dy * dy);
    }

    private static string Arg(string[] args, int index)
    {
        if (args != null && args.Length > index &&
            (File.Exists(args[index]) || Directory.Exists(args[index]))) return args[index];
        return null;
    }

    // ---- helpers ---------------------------------------------------------
    private static bool AllInRange(int[] idx, int n)
    {
        foreach (int i in idx) if (i < 0 || i >= n) return false;
        return true;
    }

    private static bool IsFinite(Vector3 v)
        => !(float.IsNaN(v.x) || float.IsNaN(v.y) || float.IsNaN(v.z)
             || float.IsInfinity(v.x) || float.IsInfinity(v.y) || float.IsInfinity(v.z));

    private static string LoadSample(string[] args)
    {
        if (args != null && args.Length > 0 && File.Exists(args[0])) return File.ReadAllText(args[0]);
        // Fall back to a path relative to the repo layout.
        string guess = Path.Combine(AppContext.BaseDirectory,
            "../../../../../QUAHOG_Unity/Assets/Resources/gis/newbedford.json");
        guess = Path.GetFullPath(guess);
        return File.Exists(guess) ? File.ReadAllText(guess) : null;
    }
}
