using System.Collections.Generic;
using UnityEngine;
using Quahog.SouthCoast.Gis;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// One driveable centreline in Unity metres (x = east, z = north), sampled by
    /// distance. Built from a <see cref="ProjectedRoad"/> that came from the shared
    /// slice or road graph — not from a hand-placed grid.
    /// </summary>
    public sealed class TrafficRoute
    {
        public Vector3[] Points;
        public float[] Cumulative;
        public float Length;
        public bool Oneway;
        public float Width;

        public void Sample(float distance, bool forward, out Vector3 pos, out Vector3 dir)
        {
            float d = Mathf.Clamp(distance, 0f, Length);
            int i = 0;
            int last = Points.Length - 2;
            while (i < last && Cumulative[i + 1] < d) i++;
            float seg = Cumulative[i + 1] - Cumulative[i];
            float t = seg < 1e-4f ? 0f : (d - Cumulative[i]) / seg;
            pos = Vector3.Lerp(Points[i], Points[i + 1], t);
            dir = Points[i + 1] - Points[i];
            dir.y = 0f;
            if (dir.sqrMagnitude < 1e-8f) dir = Vector3.forward;
            else dir.Normalize();
            if (!forward) dir = -dir;
        }
    }

    /// <summary>
    /// Pure helpers for kinematic street life: which OSM classes are drivable, and
    /// how a car advances along a polyline (wrap on one-way, ping-pong otherwise).
    /// The slice contract does not carry <c>oneway</c> (see <c>make_slice.py</c>);
    /// <c>southcoast-roads.json</c> does.
    /// </summary>
    public static class StreetRoutes
    {
        public static bool IsDrivable(string highway)
        {
            switch (highway)
            {
                case "motorway": case "trunk": case "primary": case "secondary": case "tertiary":
                case "residential": case "unclassified": case "living_street": case "service":
                case "motorway_link": case "trunk_link": case "primary_link":
                case "secondary_link": case "tertiary_link":
                    return true;
                default:
                    return false;
            }
        }

        public static List<TrafficRoute> FromRoads(List<ProjectedRoad> roads)
        {
            var list = new List<TrafficRoute>();
            if (roads == null) return list;
            foreach (var road in roads)
            {
                if (road == null || !IsDrivable(road.Highway) || road.Points.Count < 2) continue;
                var pts = new List<Vector3>(road.Points.Count);
                for (int i = 0; i < road.Points.Count; i++)
                {
                    Vector3 v = MapContracts.ToUnity(road.Points[i], 0f);
                    if (pts.Count == 0 || (pts[pts.Count - 1] - v).sqrMagnitude > 0.01f)
                        pts.Add(v);
                }
                if (pts.Count < 2) continue;

                var cum = new float[pts.Count];
                float len = 0f;
                for (int i = 1; i < pts.Count; i++)
                {
                    len += Vector3.Distance(pts[i - 1], pts[i]);
                    cum[i] = len;
                }
                if (len < 6f) continue;

                list.Add(new TrafficRoute
                {
                    Points = pts.ToArray(),
                    Cumulative = cum,
                    Length = len,
                    Oneway = road.Oneway,
                    Width = road.Width > 0f ? road.Width : 6f
                });
            }
            return list;
        }

        /// <summary>
        /// Advance <paramref name="distance"/> by <paramref name="delta"/> metres.
        /// One-way routes wrap. Two-way routes reverse at the ends.
        /// </summary>
        public static void Advance(float distance, float delta, float length, bool oneway,
                                   bool goingForward, out float newDistance, out bool newForward)
        {
            if (length < 0.01f)
            {
                newDistance = 0f;
                newForward = true;
                return;
            }

            if (oneway)
            {
                newForward = true;
                float d = distance + (goingForward ? delta : -delta);
                while (d >= length) d -= length;
                while (d < 0f) d += length;
                newDistance = d;
                return;
            }

            float pos = goingForward ? distance + delta : distance - delta;
            bool fwd = goingForward;
            for (int n = 0; n < 8 && (pos < 0f || pos > length); n++)
            {
                if (pos > length)
                {
                    pos = length - (pos - length);
                    fwd = false;
                }
                else if (pos < 0f)
                {
                    pos = -pos;
                    fwd = true;
                }
            }
            newDistance = Mathf.Clamp(pos, 0f, length);
            newForward = fwd;
        }
    }
}
