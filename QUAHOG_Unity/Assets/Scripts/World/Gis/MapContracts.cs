using System;
using System.Collections.Generic;
using System.Globalization;

namespace Quahog.SouthCoast.Gis
{
    /// <summary>
    /// One local-metre sample. East and north are the shared map axes: the slice
    /// (`make_slice.py`) stores [east, north], and `southcoast-roads.json`
    /// (`bake_meshes.py`) stores the same pair as [x, z] with z = north.
    /// Unity places that at (x = east, y = up, z = north). Three.js
    /// (`QUAHOG_Web/src/slice.ts` toWorld) uses z = -north for the same point.
    /// </summary>
    public struct ProjectedPoint
    {
        public float East;
        public float North;

        public ProjectedPoint(float east, float north)
        {
            East = east;
            North = north;
        }
    }

    public sealed class ProjectedRoad
    {
        public string Name;
        public string Highway;
        public string City;
        public float Width = 6f;
        public bool Oneway;
        public bool Bridge;
        public int Layer;
        public int SourceIndex = -1;
        public readonly List<ProjectedPoint> Points = new List<ProjectedPoint>();
    }

    public sealed class ProjectedBuilding
    {
        public string Name;
        public float Height = 9f;
        public BuildingCategory Category = BuildingCategory.Default;
        public string SourceTile;
        public int SourceIndex = -1;
        public readonly List<ProjectedPoint> Footprint = new List<ProjectedPoint>();
        public readonly List<List<ProjectedPoint>> Holes = new List<List<ProjectedPoint>>();
    }

    public sealed class ProjectedRing
    {
        public int SourceIndex = -1;
        public readonly List<ProjectedPoint> Outer = new List<ProjectedPoint>();
        public readonly List<List<ProjectedPoint>> Holes = new List<List<ProjectedPoint>>();
    }

    public sealed class ProjectedLandmark
    {
        public string Name;
        public string Kind;
        public ProjectedPoint Pos;
        public bool Hero;
        public int SourceIndex = -1;
    }

    /// <summary>
    /// A map document in Unity metres, decoded from one of the shared contracts
    /// or from the Unity playable clip derived from them.
    /// </summary>
    public sealed class MapDocument
    {
        /// <summary>geojson | web-slice | southcoast-roads | unity-playable-clip</summary>
        public string Contract;
        public string Name;
        public string Attribution;
        public string Axes;
        public string Note;
        public double OriginLat;
        public double OriginLon;
        public double MetersPerDegreeLat = 111320.0;
        public double MetersPerDegreeLon;
        public bool BuildingsStreamed;
        public int DeclaredRoadCount = -1;
        public float ClipHalfExtent;
        public readonly List<int> WaterSourceIndex = new List<int>();
        public readonly List<ProjectedRoad> Roads = new List<ProjectedRoad>();
        public readonly List<ProjectedBuilding> Buildings = new List<ProjectedBuilding>();
        public readonly List<ProjectedRing> Water = new List<ProjectedRing>();
        public readonly List<ProjectedLandmark> Landmarks = new List<ProjectedLandmark>();
    }

    /// <summary>
    /// Reads the shared OSM products without fetching new data.
    /// <list type="bullet">
    /// <item><description>
    /// <c>web-slice</c> — <c>QUAHOG_Web/public/slice-newbedford.json</c>, written by
    /// <c>quahog-project-files/mapdata/make_slice.py</c>. Buildings are empty when
    /// <c>buildingsStreamed</c> is true; footprints live in <c>public/tiles</c>.
    /// </description></item>
    /// <item><description>
    /// <c>southcoast-roads</c> — <c>quahog-project-files/mapdata/southcoast-roads.json</c>,
    /// written by <c>bake_meshes.py</c>. Points are [east, north] (file comment: x, z).
    /// </description></item>
    /// <item><description>
    /// <c>unity-playable-clip</c> — <c>Assets/Resources/gis/waterfront.json</c>, a square
    /// clip of the slice plus building tiles. See <c>tools/extract_playable_slice.py</c>.
    /// </description></item>
    /// <item><description>
    /// <c>geojson</c> — FeatureCollection in lon/lat (the hand-authored
    /// <c>newbedford.json</c> fallback, or an Overpass GeoJSON export). Projected
    /// equirectangular about the file's own bounding-box centre.
    /// </description></item>
    /// </list>
    /// </summary>
    public static class MapContracts
    {
        public static UnityEngine.Vector3 ToUnity(ProjectedPoint p, float y)
        {
            return new UnityEngine.Vector3(p.East, y, p.North);
        }

        public static MapDocument Parse(string text)
        {
            if (string.IsNullOrEmpty(text)) return null;
            object root;
            try { root = MiniJson.Parse(text); }
            catch (FormatException) { return null; }
            var obj = root as Dictionary<string, object>;
            if (obj == null) return null;

            string type = AsString(Get(obj, "type"));
            if (type == "FeatureCollection" || obj.ContainsKey("features"))
                return FromGeoJson(text);

            if (!obj.ContainsKey("roads")) return null;

            string contract = AsString(Get(obj, "contract"));
            bool sliceLike = contract != null
                || obj.ContainsKey("landmarks")
                || obj.ContainsKey("buildings")
                || obj.ContainsKey("buildingsStreamed")
                || obj.ContainsKey("water");
            if (sliceLike)
                return FromPlanar(obj, string.IsNullOrEmpty(contract) ? "web-slice" : contract);

            if (obj.ContainsKey("road_count") || obj.ContainsKey("origin"))
                return FromRoadGraph(obj);
            return null;
        }

        /// <summary>A building-tile file: a JSON array of {footprint, height, name?}.</summary>
        public static List<ProjectedBuilding> ParseBuildingTile(string text)
        {
            var list = new List<ProjectedBuilding>();
            if (string.IsNullOrEmpty(text)) return list;
            object root;
            try { root = MiniJson.Parse(text); }
            catch (FormatException) { return list; }
            var arr = root as List<object>;
            if (arr == null) return list;
            for (int i = 0; i < arr.Count; i++)
            {
                var b = ReadBuilding(arr[i] as Dictionary<string, object>);
                if (b == null) continue;
                if (b.SourceIndex < 0) b.SourceIndex = i;
                list.Add(b);
            }
            return list;
        }

        // -----------------------------------------------------------------
        // Contracts
        // -----------------------------------------------------------------

        private static MapDocument FromGeoJson(string text)
        {
            GeoData data = GeoJson.Parse(text);
            var doc = new MapDocument
            {
                Contract = "geojson",
                Axes = "equirectangular metres; x=east, z=north; origin is this file's lon/lat bbox centre",
                MetersPerDegreeLat = 111320.0
            };

            double minLon = double.MaxValue, maxLon = double.MinValue;
            double minLat = double.MaxValue, maxLat = double.MinValue;
            void Acc(LonLat p)
            {
                if (p.Lon < minLon) minLon = p.Lon;
                if (p.Lon > maxLon) maxLon = p.Lon;
                if (p.Lat < minLat) minLat = p.Lat;
                if (p.Lat > maxLat) maxLat = p.Lat;
            }
            foreach (var r in data.Roads) foreach (var p in r.Line) Acc(p);
            foreach (var b in data.Buildings) foreach (var p in b.Ring) Acc(p);
            foreach (var w in data.Water) foreach (var p in w.Ring) Acc(p);

            bool any = minLon <= maxLon;
            double lon0 = any ? (minLon + maxLon) * 0.5 : 0;
            double lat0 = any ? (minLat + maxLat) * 0.5 : 0;
            doc.OriginLon = lon0;
            doc.OriginLat = lat0;
            doc.MetersPerDegreeLon = 111320.0 * Math.Cos(lat0 * Math.PI / 180.0);

            foreach (var r in data.Roads)
            {
                var road = new ProjectedRoad
                {
                    Name = r.Name,
                    Highway = r.Highway,
                    Width = r.Width,
                    SourceIndex = -1
                };
                foreach (var p in r.Line) road.Points.Add(Project(p, doc));
                doc.Roads.Add(road);
            }

            foreach (var b in data.Buildings)
            {
                var building = new ProjectedBuilding
                {
                    Height = b.Height,
                    Category = b.Category
                };
                foreach (var p in b.Ring) building.Footprint.Add(Project(p, doc));
                foreach (var hole in b.Holes)
                {
                    var h = new List<ProjectedPoint>(hole.Count);
                    foreach (var p in hole) h.Add(Project(p, doc));
                    building.Holes.Add(h);
                }
                doc.Buildings.Add(building);
            }
            foreach (var w in data.Water)
            {
                var ring = new ProjectedRing();
                foreach (var p in w.Ring) ring.Outer.Add(Project(p, doc));
                foreach (var hole in w.Holes)
                {
                    var h = new List<ProjectedPoint>(hole.Count);
                    foreach (var p in hole) h.Add(Project(p, doc));
                    ring.Holes.Add(h);
                }
                doc.Water.Add(ring);
            }
            return doc;
        }

        private static ProjectedPoint Project(LonLat p, MapDocument doc)
        {
            float east = (float)((p.Lon - doc.OriginLon) * doc.MetersPerDegreeLon);
            float north = (float)((p.Lat - doc.OriginLat) * doc.MetersPerDegreeLat);
            return new ProjectedPoint(east, north);
        }

        private static MapDocument FromRoadGraph(Dictionary<string, object> obj)
        {
            var doc = new MapDocument { Contract = "southcoast-roads" };
            ReadFrame(obj, doc);
            if (string.IsNullOrEmpty(doc.Axes))
                doc.Axes = "x=east(m), z=north(m), y=up";
            doc.DeclaredRoadCount = AsInt(Get(obj, "road_count"), -1);
            ReadRoadList(AsList(Get(obj, "roads")), doc);
            return doc;
        }

        private static MapDocument FromPlanar(Dictionary<string, object> obj, string contract)
        {
            var doc = new MapDocument { Contract = contract };
            ReadFrame(obj, doc);
            doc.BuildingsStreamed = AsBool(Get(obj, "buildingsStreamed"));
            var prov = Get(obj, "provenance") as Dictionary<string, object>;
            if (prov != null)
            {
                doc.ClipHalfExtent = (float)AsDouble(Get(prov, "clipHalfExtentMeters"), 0);
                var wsrc = AsList(Get(prov, "waterSourceIndex"));
                if (wsrc != null)
                    foreach (var s in wsrc) doc.WaterSourceIndex.Add(AsInt(s, -1));
            }

            ReadRoadList(AsList(Get(obj, "roads")), doc);

            var buildings = AsList(Get(obj, "buildings"));
            if (buildings != null)
            {
                foreach (var item in buildings)
                {
                    var b = ReadBuilding(item as Dictionary<string, object>);
                    if (b != null) doc.Buildings.Add(b);
                }
            }

            var water = AsList(Get(obj, "water"));
            if (water != null)
            {
                for (int i = 0; i < water.Count; i++)
                {
                    var ring = ReadWater(water[i]);
                    if (ring == null) continue;
                    if (ring.SourceIndex < 0 && i < doc.WaterSourceIndex.Count)
                        ring.SourceIndex = doc.WaterSourceIndex[i];
                    doc.Water.Add(ring);
                }
            }

            var landmarks = AsList(Get(obj, "landmarks"));
            if (landmarks != null)
            {
                foreach (var item in landmarks)
                {
                    var lm = ReadLandmark(item as Dictionary<string, object>);
                    if (lm != null) doc.Landmarks.Add(lm);
                }
            }
            return doc;
        }

        private static void ReadFrame(Dictionary<string, object> obj, MapDocument doc)
        {
            doc.Name = AsString(Get(obj, "name"));
            doc.Attribution = AsString(Get(obj, "attribution"));
            doc.Axes = AsString(Get(obj, "axes"));
            doc.Note = AsString(Get(obj, "note"));
            var origin = Get(obj, "origin") as Dictionary<string, object>;
            if (origin != null)
            {
                doc.OriginLat = AsDouble(Get(origin, "lat"), 0);
                doc.OriginLon = AsDouble(Get(origin, "lon"), 0);
            }
            var mpd = Get(obj, "meters_per_degree") as Dictionary<string, object>;
            if (mpd != null)
            {
                doc.MetersPerDegreeLat = AsDouble(Get(mpd, "lat"), 111320.0);
                doc.MetersPerDegreeLon = AsDouble(Get(mpd, "lon"), 0);
            }
        }

        private static void ReadRoadList(List<object> roads, MapDocument doc)
        {
            if (roads == null) return;
            foreach (var item in roads)
            {
                var o = item as Dictionary<string, object>;
                if (o == null) continue;
                var pts = ReadPoints(Get(o, "points"));
                if (pts.Count < 2) continue;
                var road = new ProjectedRoad
                {
                    Name = AsString(Get(o, "name")),
                    Highway = AsString(Get(o, "highway")),
                    City = AsString(Get(o, "city")),
                    Width = (float)AsDouble(Get(o, "width"), 6),
                    Oneway = AsBool(Get(o, "oneway")),
                    Bridge = AsBool(Get(o, "bridge")),
                    Layer = AsInt(Get(o, "layer"), 0),
                    SourceIndex = AsInt(Get(o, "sourceIndex"), -1)
                };
                if (road.Width <= 0f) road.Width = 6f;
                road.Points.AddRange(pts);
                doc.Roads.Add(road);
            }
        }

        private static ProjectedBuilding ReadBuilding(Dictionary<string, object> o)
        {
            if (o == null) return null;
            var fp = ReadPoints(Get(o, "footprint"));
            if (fp.Count < 3) return null;
            var b = new ProjectedBuilding
            {
                Name = AsString(Get(o, "name")),
                Height = (float)AsDouble(Get(o, "height"), 8),
                SourceTile = AsString(Get(o, "sourceTile")),
                SourceIndex = AsInt(Get(o, "sourceIndex"), -1),
                Category = BuildingCategory.Default
            };
            if (b.Height <= 0f) b.Height = 8f;
            b.Footprint.AddRange(fp);
            return b;
        }

        private static ProjectedRing ReadWater(object item)
        {
            if (item is List<object> ringPts)
            {
                var ring = new ProjectedRing();
                ring.Outer.AddRange(ReadPoints(ringPts));
                return ring.Outer.Count >= 3 ? ring : null;
            }
            var o = item as Dictionary<string, object>;
            if (o == null) return null;
            var wrapped = new ProjectedRing
            {
                SourceIndex = AsInt(Get(o, "sourceIndex"), -1)
            };
            wrapped.Outer.AddRange(ReadPoints(Get(o, "ring")));
            return wrapped.Outer.Count >= 3 ? wrapped : null;
        }

        private static ProjectedLandmark ReadLandmark(Dictionary<string, object> o)
        {
            if (o == null) return null;
            var pair = Get(o, "pos") as List<object>;
            if (pair == null || pair.Count < 2 || pair[0] is List<object>) return null;
            ProjectedPoint p = new ProjectedPoint((float)AsDouble(pair[0], 0), (float)AsDouble(pair[1], 0));

            return new ProjectedLandmark
            {
                Name = AsString(Get(o, "name")),
                Kind = AsString(Get(o, "kind")),
                Pos = p,
                Hero = AsBool(Get(o, "hero")),
                SourceIndex = AsInt(Get(o, "sourceIndex"), -1)
            };
        }

        private static List<ProjectedPoint> ReadPoints(object coords)
        {
            var result = new List<ProjectedPoint>();
            var pts = coords as List<object>;
            if (pts == null) return result;
            foreach (var item in pts)
            {
                var pair = item as List<object>;
                if (pair == null || pair.Count < 2) continue;
                if (pair[0] is List<object>) continue; // nested ring, not a point
                result.Add(new ProjectedPoint((float)AsDouble(pair[0], 0), (float)AsDouble(pair[1], 0)));
            }
            return result;
        }

        // -----------------------------------------------------------------
        // Dynamic-tree helpers (MiniJson output)
        // -----------------------------------------------------------------

        private static object Get(Dictionary<string, object> obj, string key)
        {
            if (obj != null && obj.TryGetValue(key, out object v)) return v;
            return null;
        }

        private static string AsString(object v)
        {
            if (v == null) return null;
            return v as string ?? Convert.ToString(v, CultureInfo.InvariantCulture);
        }

        private static bool AsBool(object v)
        {
            if (v is bool b) return b;
            if (v is string s) return s == "yes" || s == "true";
            return false;
        }

        private static double AsDouble(object v, double fallback)
        {
            if (v is double d) return d;
            if (v is int i) return i;
            if (v is string s && double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out double parsed))
                return parsed;
            return fallback;
        }

        private static int AsInt(object v, int fallback)
        {
            double d = AsDouble(v, double.NaN);
            if (double.IsNaN(d)) return fallback;
            return (int)d;
        }

        private static List<object> AsList(object o) => o as List<object>;
    }
}
