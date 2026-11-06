using System.Collections.Generic;
using UnityEngine;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// Adds lightweight "street life" to the town greybox: a handful of wandering
    /// pedestrians and a few decorative AI cars that patrol the streets. Everything
    /// is built at runtime from primitives (no prefabs, no packages) and lives under
    /// one "StreetLife" root so the integration layer can move or destroy it as a unit.
    ///
    /// Design constraints (important):
    ///   * Everything here is KINEMATIC. NPCs and cars move purely by transform
    ///     translation/rotation each frame — no Rigidbody, no physics forces. This
    ///     guarantees they can never collide with or shove the player's physics-based
    ///     car. The primitive colliders that ship with CreatePrimitive are switched to
    ///     trigger-only (isTrigger = true) so the player's Rigidbody never reacts to them.
    ///   * Only built-in UnityEngine APIs are used so this compiles in a headless,
    ///     package-free build.
    ///
    /// Coordinate convention matches TownGreybox: X = east/west, Z = north/south,
    /// Y = up, ground top at y = 0. Water runs along the +Z (north) edge, so
    /// pedestrians are clamped to z &lt; 26 and never walk into it.
    /// </summary>
    public class StreetLife : MonoBehaviour
    {
        // ---- Tunables (kept as constants so behaviour is reproducible). ----

        private const float GroundTopY = 0f;   // matches TownGreybox ground top

        private const int   PedestrianCount = 10;
        private const float PedHeight       = 1.8f;   // capsule total height (m)
        private const float PedSpeed        = 1.5f;   // walk speed (m/s)
        private const float PedTurnSpeed    = 6f;     // how fast they face travel dir
        private const float PedArriveDist   = 0.6f;   // "close enough" to a destination
        private const float PedRepathTime   = 12f;    // pick a new goal after this long

        // Wander area: a 40x40 box centred on the origin, but the north edge is
        // clamped so pedestrians stay out of the water (z &lt; 26).
        private const float WanderHalf      = 20f;    // half-extent of the wander box
        private const float WaterKeepoutZ   = 26f;    // pedestrians never go to z >= this

        private const float CarSpeed        = 8f;     // AI car cruise speed (m/s)
        private const float CarTurnSpeed    = 8f;     // how fast cars face travel dir
        private const float CarRideHeight   = 0.5f;   // body centre height above ground

        // Shared materials (created once, reused to keep material/draw count low).
        private Material[] _pedMats;
        private Material[] _carMats;
        private Material   _carCabinMat;

        // Live agents updated each frame.
        private Pedestrian[] _peds;
        private CarPatrol[]  _cars;

        // Real OSM centre-lines when the GIS city supplied them. Null keeps the
        // greybox wander box (z keep-out at the painted waterfront).
        private IReadOnlyList<TrafficRoute> _routes;

        /// <summary>
        /// Build all pedestrians and AI cars under a fresh "StreetLife" root and
        /// return the component. Safe to call once; the integration layer owns the
        /// returned object's lifetime. With no routes, agents use the greybox box.
        /// </summary>
        public static StreetLife Spawn() => Spawn(null);

        /// <summary>
        /// Same as <see cref="Spawn()"/>, but pedestrians and patrol cars follow
        /// <paramref name="routes"/> (Unity x = east, z = north).
        /// </summary>
        public static StreetLife Spawn(IReadOnlyList<TrafficRoute> routes)
        {
            var root = new GameObject("StreetLife");
            var life = root.AddComponent<StreetLife>();
            life._routes = routes;
            life.BuildInternal();
            return life;
        }

        private bool HasRoutes() => _routes != null && _routes.Count > 0;

        private void BuildInternal()
        {
            CreateMaterials();
            BuildPedestrians();
            BuildCars();
        }

        // -----------------------------------------------------------------
        // Materials
        // -----------------------------------------------------------------

        /// <summary>
        /// Creates a coloured material that works across render pipelines. URP/Lit
        /// reads "_BaseColor"; the legacy Standard shader reads ".color". We set both
        /// so the same material looks right regardless of which shader was found.
        /// (Same approach as TownGreybox.)
        /// </summary>
        private static Material MakeColorMaterial(Color c)
        {
            Shader s = Shader.Find("Universal Render Pipeline/Lit");
            if (s == null) s = Shader.Find("Standard");
            if (s == null) s = Shader.Find("Sprites/Default");

            if (s == null)
            {
                Debug.LogWarning("[StreetLife] No suitable shader found; using fallback.");
                var fallback = new Material(Shader.Find("Hidden/InternalErrorShader"));
                return fallback;
            }

            var m = new Material(s);
            if (m.HasProperty("_BaseColor")) m.SetColor("_BaseColor", c);
            m.color = c;
            return m;
        }

        private void CreateMaterials()
        {
            // Muted, "everyday clothing" palette for the pedestrians.
            _pedMats = new[]
            {
                MakeColorMaterial(new Color(0.55f, 0.30f, 0.28f)), // rust
                MakeColorMaterial(new Color(0.30f, 0.36f, 0.46f)), // slate blue
                MakeColorMaterial(new Color(0.40f, 0.44f, 0.34f)), // olive
                MakeColorMaterial(new Color(0.62f, 0.58f, 0.48f)), // tan
                MakeColorMaterial(new Color(0.42f, 0.42f, 0.46f)), // grey
                MakeColorMaterial(new Color(0.50f, 0.40f, 0.52f)), // muted plum
            };

            // A couple of simple car-body colours.
            _carMats = new[]
            {
                MakeColorMaterial(new Color(0.65f, 0.20f, 0.18f)), // faded red
                MakeColorMaterial(new Color(0.20f, 0.28f, 0.40f)), // navy
                MakeColorMaterial(new Color(0.80f, 0.78f, 0.72f)), // off-white
            };

            _carCabinMat = MakeColorMaterial(new Color(0.15f, 0.18f, 0.22f)); // dark glass/cabin
        }

        // -----------------------------------------------------------------
        // Primitive helper
        // -----------------------------------------------------------------

        /// <summary>
        /// Spawns a primitive parented under the given transform, positions/scales it,
        /// applies a material, and makes any collider trigger-only so it never disturbs
        /// the player's physics car. Pass <paramref name="parent"/> = null to parent
        /// under the StreetLife root.
        /// </summary>
        private GameObject SpawnPrimitive(PrimitiveType type, string name, Transform parent,
                                          Vector3 localPos, Vector3 localScale, Material mat)
        {
            var go = GameObject.CreatePrimitive(type);
            go.name = name;
            go.transform.SetParent(parent != null ? parent : transform, false);
            go.transform.localPosition = localPos;
            go.transform.localScale = localScale;

            var renderer = go.GetComponent<Renderer>();
            if (renderer != null && mat != null) renderer.sharedMaterial = mat;

            // Make the auto-created collider a trigger so the kinematic agents can
            // pass through the world (and the player) without exerting any force.
            var col = go.GetComponent<Collider>();
            if (col != null) col.isTrigger = true;

            return go;
        }

        // -----------------------------------------------------------------
        // Pedestrians
        // -----------------------------------------------------------------

        private void BuildPedestrians()
        {
            if (HasRoutes())
            {
                BuildPedestriansOnRoutes();
                return;
            }

            _peds = new Pedestrian[PedestrianCount];

            for (int i = 0; i < PedestrianCount; i++)
            {
                Material mat = _pedMats[i % _pedMats.Length];

                // Unity's default Capsule is 2m tall at scale 1. Scale uniformly so the
                // total height is PedHeight, and lift the centre by half-height so the
                // feet rest on the ground (y = 0).
                float scaleFactor = PedHeight / 2f;
                float centreY = GroundTopY + PedHeight * 0.5f;

                Vector3 start = RandomWanderPoint();
                var go = SpawnPrimitive(PrimitiveType.Capsule, "Pedestrian",
                    transform,
                    new Vector3(start.x, centreY, start.z),
                    new Vector3(scaleFactor, scaleFactor, scaleFactor),
                    mat);

                var ped = new Pedestrian
                {
                    Tr = go.transform,
                    CentreY = centreY,
                    Destination = RandomWanderPoint(),
                    RepathTimer = Random.Range(0f, PedRepathTime),
                };
                _peds[i] = ped;
            }
        }

        /// <summary>
        /// Picks a random point on the ground plane within the wander box, clamped so
        /// it never lands in (or near) the water along the north edge.
        /// </summary>
        private static Vector3 RandomWanderPoint()
        {
            float x = Random.Range(-WanderHalf, WanderHalf);
            float z = Random.Range(-WanderHalf, WanderHalf);
            if (z > WaterKeepoutZ) z = WaterKeepoutZ; // never walk into the water
            return new Vector3(x, GroundTopY, z);
        }

        // -----------------------------------------------------------------
        // AI cars
        // -----------------------------------------------------------------

        private void BuildPedestriansOnRoutes()
        {
            _peds = new Pedestrian[PedestrianCount];
            for (int i = 0; i < PedestrianCount; i++)
            {
                Material mat = _pedMats[i % _pedMats.Length];
                float scaleFactor = PedHeight / 2f;
                float centreY = GroundTopY + PedHeight * 0.5f;

                int routeIndex = i % _routes.Count;
                TrafficRoute route = _routes[routeIndex];
                float distance = route.Length * ((i + 0.5f) / PedestrianCount);
                if (distance > route.Length) distance = route.Length * 0.5f;
                route.Sample(distance, true, out Vector3 pos, out Vector3 dir);
                float lateral = route.Width * 0.5f + 0.6f;
                if ((i & 1) == 1) lateral = -lateral;
                Vector3 side = new Vector3(-dir.z, 0f, dir.x);
                pos += side * lateral;
                pos.y = centreY;

                var go = SpawnPrimitive(PrimitiveType.Capsule, "Pedestrian",
                    transform, pos, new Vector3(scaleFactor, scaleFactor, scaleFactor), mat);

                _peds[i] = new Pedestrian
                {
                    Tr = go.transform,
                    CentreY = centreY,
                    Destination = pos,
                    RepathTimer = PedRepathTime,
                    OnRoute = true,
                    RouteIndex = routeIndex,
                    Distance = distance,
                    Forward = true,
                    Side = lateral
                };
            }
        }

        private void BuildCars()
        {
            if (HasRoutes())
            {
                BuildCarsOnRoutes();
                return;
            }

            // Three decorative patrol cars: two on the east-west lane (z = 0), one on
            // the north-south lane (x = 0). They reverse at the lane ends.
            //   forward = unit travel direction at spawn
            //   min/max = signed extent of the patrol along that axis
            var specs = new[]
            {
                // East-west lane, driving east first, slightly offset south of centre.
                new CarSpec(new Vector3(-28f, CarRideHeight, -1.4f), Vector3.right, -28f, 28f, Axis.X, 0),
                // East-west lane, driving west first, offset north of centre.
                new CarSpec(new Vector3( 28f, CarRideHeight,  1.4f), Vector3.left,  -28f, 28f, Axis.X, 2),
                // North-south lane, driving north first, offset east of centre.
                new CarSpec(new Vector3( 1.4f, CarRideHeight, -23f), Vector3.forward, -23f, 23f, Axis.Z, 1),
            };

            _cars = new CarPatrol[specs.Length];
            for (int i = 0; i < specs.Length; i++)
            {
                CarSpec s = specs[i];
                Material bodyMat = _carMats[s.MatIndex % _carMats.Length];
                Transform body = BuildCarBody(bodyMat);
                body.localPosition = s.Start;
                body.localRotation = Quaternion.LookRotation(s.Forward, Vector3.up);

                _cars[i] = new CarPatrol
                {
                    Tr = body,
                    Direction = s.Forward,
                    PatrolAxis = s.PatrolAxis,
                    Min = s.Min,
                    Max = s.Max,
                };
            }
        }

        /// <summary>
        /// Builds one car as a small hierarchy: a scaled box body with a smaller cabin
        /// box on top, parented to an empty "Car" transform we steer. Returns the
        /// steerable parent transform.
        /// </summary>
        private Transform BuildCarBody(Material bodyMat)
        {
            var carRoot = new GameObject("Car");
            carRoot.transform.SetParent(transform, false);

            // Body: ~4m long (local +Z is forward), 1.8m wide, 1.2m tall.
            SpawnPrimitive(PrimitiveType.Cube, "Body", carRoot.transform,
                new Vector3(0f, 0f, 0f),
                new Vector3(1.8f, 1.2f, 4f),
                bodyMat);

            // Cabin: a smaller box sitting on top, set back a touch.
            SpawnPrimitive(PrimitiveType.Cube, "Cabin", carRoot.transform,
                new Vector3(0f, 0.85f, -0.2f),
                new Vector3(1.5f, 0.9f, 2f),
                _carCabinMat);

            return carRoot.transform;
        }

        private void BuildCarsOnRoutes()
        {
            int count = _routes.Count < 6 ? _routes.Count : 6;
            _cars = new CarPatrol[count];
            for (int i = 0; i < count; i++)
            {
                int routeIndex = (i * 3) % _routes.Count;
                TrafficRoute route = _routes[routeIndex];
                float distance = route.Length * ((i + 1f) / (count + 1f));
                route.Sample(distance, true, out Vector3 pos, out Vector3 dir);
                pos.y = CarRideHeight;

                Material bodyMat = _carMats[i % _carMats.Length];
                Transform body = BuildCarBody(bodyMat);
                body.localPosition = pos;
                if (dir.sqrMagnitude > 0.0001f)
                    body.localRotation = Quaternion.LookRotation(dir, Vector3.up);

                _cars[i] = new CarPatrol
                {
                    Tr = body,
                    Direction = dir,
                    PatrolAxis = Axis.X,
                    Min = 0f,
                    Max = route.Length,
                    OnRoute = true,
                    RouteIndex = routeIndex,
                    Distance = distance,
                    Forward = true
                };
            }
        }

        // -----------------------------------------------------------------
        // Per-frame movement (kinematic — transform only)
        // -----------------------------------------------------------------

        private void Update()
        {
            float dt = Time.deltaTime;
            UpdatePedestrians(dt);
            UpdateCars(dt);
        }

        private void UpdatePedestrians(float dt)
        {
            if (_peds == null) return;

            for (int i = 0; i < _peds.Length; i++)
            {
                Pedestrian p = _peds[i];
                if (p.Tr == null) continue; // robust to externally destroyed children

                if (p.OnRoute)
                {
                    UpdatePedestrianOnRoute(ref p, dt);
                    _peds[i] = p;
                    continue;
                }

                Vector3 pos = p.Tr.localPosition;

                // Flat (XZ) vector toward the current destination.
                Vector3 toDest = p.Destination - pos;
                toDest.y = 0f;
                float dist = toDest.magnitude;

                // Re-target on arrival or after the timeout, so nobody gets stuck.
                p.RepathTimer -= dt;
                if (dist <= PedArriveDist || p.RepathTimer <= 0f)
                {
                    p.Destination = RandomWanderPoint();
                    p.RepathTimer = PedRepathTime;
                    // Recompute for this frame so motion is continuous.
                    toDest = p.Destination - pos;
                    toDest.y = 0f;
                    dist = toDest.magnitude;
                }

                if (dist > 0.0001f)
                {
                    Vector3 dir = toDest / dist; // normalised travel direction

                    // Smoothly face the travel direction (yaw only; capsule stays upright).
                    Quaternion want = Quaternion.LookRotation(dir, Vector3.up);
                    p.Tr.localRotation = Quaternion.Slerp(p.Tr.localRotation, want, PedTurnSpeed * dt);

                    // Step forward, but never overshoot the destination this frame.
                    float step = Mathf.Min(PedSpeed * dt, dist);
                    pos += dir * step;
                }

                // Keep feet on the ground; the capsule centre sits at CentreY.
                pos.y = p.CentreY;
                p.Tr.localPosition = pos;

                _peds[i] = p;
            }
        }

        private void UpdatePedestrianOnRoute(ref Pedestrian p, float dt)
        {
            if (_routes == null || p.RouteIndex < 0 || p.RouteIndex >= _routes.Count)
                return;
            TrafficRoute route = _routes[p.RouteIndex];
            StreetRoutes.Advance(p.Distance, PedSpeed * dt, route.Length, false, p.Forward,
                out float next, out bool forward);
            p.Distance = next;
            p.Forward = forward;
            route.Sample(p.Distance, p.Forward, out Vector3 pos, out Vector3 dir);
            Vector3 side = new Vector3(-dir.z, 0f, dir.x);
            pos += side * p.Side;
            pos.y = p.CentreY;
            p.Tr.localPosition = pos;
            if (dir.sqrMagnitude > 0.0001f)
            {
                Quaternion want = Quaternion.LookRotation(dir, Vector3.up);
                p.Tr.localRotation = Quaternion.Slerp(p.Tr.localRotation, want, PedTurnSpeed * dt);
            }
        }

        private void UpdateCars(float dt)
        {
            if (_cars == null) return;

            for (int i = 0; i < _cars.Length; i++)
            {
                CarPatrol c = _cars[i];
                if (c.Tr == null) continue;

                if (c.OnRoute)
                {
                    UpdateCarOnRoute(ref c, dt);
                    _cars[i] = c;
                    continue;
                }

                Vector3 pos = c.Tr.localPosition;

                // Advance along the current travel direction.
                pos += c.Direction * (CarSpeed * dt);

                // Reverse at the patrolled lane's ends. We only patrol one axis, so we
                // test that axis's coordinate against the signed min/max.
                float coord = (c.PatrolAxis == Axis.X) ? pos.x : pos.z;
                if (coord >= c.Max)
                {
                    // Clamp to the end and flip direction.
                    if (c.PatrolAxis == Axis.X) pos.x = c.Max; else pos.z = c.Max;
                    c.Direction = -c.Direction;
                }
                else if (coord <= c.Min)
                {
                    if (c.PatrolAxis == Axis.X) pos.x = c.Min; else pos.z = c.Min;
                    c.Direction = -c.Direction;
                }

                // Hold a believable ride height.
                pos.y = CarRideHeight;
                c.Tr.localPosition = pos;

                // Always face the current travel direction (smoothly through the flip).
                if (c.Direction.sqrMagnitude > 0.0001f)
                {
                    Quaternion want = Quaternion.LookRotation(c.Direction, Vector3.up);
                    c.Tr.localRotation = Quaternion.Slerp(c.Tr.localRotation, want, CarTurnSpeed * dt);
                }

                _cars[i] = c;
            }
        }

        private void UpdateCarOnRoute(ref CarPatrol c, float dt)
        {
            if (_routes == null || c.RouteIndex < 0 || c.RouteIndex >= _routes.Count)
                return;
            TrafficRoute route = _routes[c.RouteIndex];
            StreetRoutes.Advance(c.Distance, CarSpeed * dt, route.Length, route.Oneway, c.Forward,
                out float next, out bool forward);
            c.Distance = next;
            c.Forward = forward;
            route.Sample(c.Distance, c.Forward, out Vector3 pos, out Vector3 dir);
            pos.y = CarRideHeight;
            c.Tr.localPosition = pos;
            c.Direction = dir;
            if (dir.sqrMagnitude > 0.0001f)
            {
                Quaternion want = Quaternion.LookRotation(dir, Vector3.up);
                c.Tr.localRotation = Quaternion.Slerp(c.Tr.localRotation, want, CarTurnSpeed * dt);
            }
        }

        // -----------------------------------------------------------------
        // Plain data holders
        // -----------------------------------------------------------------

        /// <summary>Which world axis a car patrols along.</summary>
        private enum Axis { X, Z }

        /// <summary>Mutable runtime state for one wandering pedestrian.</summary>
        private struct Pedestrian
        {
            public Transform Tr;          // capsule transform we move
            public float     CentreY;     // y of the capsule centre (feet on ground)
            public Vector3   Destination; // current wander goal (on the ground plane)
            public float     RepathTimer; // seconds until a forced re-target
            public bool      OnRoute;
            public int       RouteIndex;
            public float     Distance;
            public bool      Forward;
            public float     Side;
        }

        /// <summary>Mutable runtime state for one patrolling AI car.</summary>
        private struct CarPatrol
        {
            public Transform Tr;         // steerable car-root transform
            public Vector3   Direction;  // unit travel direction (flips at lane ends)
            public Axis      PatrolAxis; // which axis Min/Max apply to
            public float     Min;        // signed lane-end (low)
            public float     Max;        // signed lane-end (high)
            public bool      OnRoute;
            public int       RouteIndex;
            public float     Distance;
            public bool      Forward;
        }

        /// <summary>Immutable description used to lay out one car at build time.</summary>
        private struct CarSpec
        {
            public readonly Vector3 Start;
            public readonly Vector3 Forward;
            public readonly float   Min;
            public readonly float   Max;
            public readonly Axis    PatrolAxis;
            public readonly int     MatIndex; // index into _carMats, resolved by StreetLife

            public CarSpec(Vector3 start, Vector3 forward, float min, float max, Axis axis, int matIndex)
            {
                Start = start;
                Forward = forward;
                Min = min;
                Max = max;
                PatrolAxis = axis;
                MatIndex = matIndex;
            }
        }
    }
}
