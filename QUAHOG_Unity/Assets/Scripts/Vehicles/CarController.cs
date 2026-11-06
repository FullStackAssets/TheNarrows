using UnityEngine;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// A simple arcade drivable car built entirely from runtime primitives — no
    /// prefabs, no scene wiring, no WheelColliders. The root carries a single
    /// Rigidbody plus a BoxCollider sized to the body, and the wheels are purely
    /// visual cylinders. Driving is velocity-based arcade physics in FixedUpdate
    /// (legacy Input axes), tuned to feel responsive yet stay stable headless.
    ///
    /// Spawn it with <see cref="Spawn"/>, then toggle player control with
    /// <see cref="SetDriven"/>; when not driven the car reads no input and rests.
    /// </summary>
    public class CarController : MonoBehaviour
    {
        // --- Tuning (metres / seconds / Newtons; arcade, not realistic) ---
        private const float MaxSpeed       = 20f;    // top forward speed (m/s)
        private const float MaxReverseSpeed = 8f;    // capped reverse speed (m/s)
        private const float Acceleration   = 12f;    // forward accel force scale
        private const float BrakeForce     = 24f;    // active braking force scale
        private const float TurnSpeed      = 90f;    // degrees/sec at full steer
        private const float GripFactor     = 6f;     // lateral grip (kills sliding)

        private Rigidbody _rb;
        private Transform _exitPoint;
        private bool _driven;
        private readonly System.Collections.Generic.List<Transform> _wheels =
            new System.Collections.Generic.List<Transform>();
        private float _wheelSpin;
        private float _speedMps;

        /// <summary>Signed forward speed in metres per second (positive = nose-first).</summary>
        public float SpeedMps { get { return _speedMps; } }

        /// <summary>True while the player is actively driving this car.</summary>
        public bool IsOccupied { get { return _driven; } }

        /// <summary>
        /// A point beside the driver door where the player should be placed when
        /// they exit the vehicle.
        /// </summary>
        public Transform ExitPoint { get { return _exitPoint; } }

        /// <summary>
        /// Build a complete primitive car at <paramref name="position"/> and return
        /// its controller. The car starts un-driven (at rest).
        /// </summary>
        public static CarController Spawn(Vector3 position)
        {
            // --- Root + Rigidbody + body collider ---
            var root = new GameObject("Car");
            root.transform.position = position;

            var car = root.AddComponent<CarController>();

            var rb = root.AddComponent<Rigidbody>();
            rb.mass = 1200f;
            rb.linearDamping = 0.5f;          // forward/lateral damping (a.k.a. drag)
            rb.angularDamping = 3f;           // resists spinning so it tracks straight
            rb.interpolation = RigidbodyInterpolation.Interpolate;
            rb.collisionDetectionMode = CollisionDetectionMode.ContinuousDynamic;
            // Lower the centre of mass well below the body so the car resists tipping.
            rb.centerOfMass = new Vector3(0f, -0.5f, 0f);
            car._rb = rb;

            // Body dimensions (x = width, y = height, z = length).
            var bodySize = new Vector3(2f, 0.6f, 4f);

            var box = root.AddComponent<BoxCollider>();
            box.center = new Vector3(0f, bodySize.y * 0.5f, 0f);
            box.size = bodySize;

            // --- Visual body (scaled cube) ---
            var body = GameObject.CreatePrimitive(PrimitiveType.Cube);
            body.name = "Body";
            DestroyImmediate(body.GetComponent<Collider>()); // root BoxCollider owns physics
            body.transform.SetParent(root.transform, false);
            body.transform.localPosition = new Vector3(0f, bodySize.y * 0.5f, 0f);
            body.transform.localScale = bodySize;

            // --- Cabin (smaller cube on top, set back toward the rear) ---
            var cabin = GameObject.CreatePrimitive(PrimitiveType.Cube);
            cabin.name = "Cabin";
            DestroyImmediate(cabin.GetComponent<Collider>());
            cabin.transform.SetParent(root.transform, false);
            cabin.transform.localPosition = new Vector3(0f, bodySize.y + 0.35f, -0.3f);
            cabin.transform.localScale = new Vector3(1.6f, 0.7f, 2f);

            // --- Four visual wheels (cylinders rotated 90° on Z so they roll) ---
            const float wheelRadius = 0.5f;
            const float wheelWidth  = 0.3f;
            float halfW = bodySize.x * 0.5f;             // place wheels at body edges
            float wheelZ = bodySize.z * 0.5f - wheelRadius; // inset from the ends
            float wheelY = wheelRadius;                  // hubs at ground-contact height

            car.CreateWheel(root.transform, "Wheel_FL", new Vector3(-halfW, wheelY,  wheelZ), wheelRadius, wheelWidth);
            car.CreateWheel(root.transform, "Wheel_FR", new Vector3( halfW, wheelY,  wheelZ), wheelRadius, wheelWidth);
            car.CreateWheel(root.transform, "Wheel_RL", new Vector3(-halfW, wheelY, -wheelZ), wheelRadius, wheelWidth);
            car.CreateWheel(root.transform, "Wheel_RR", new Vector3( halfW, wheelY, -wheelZ), wheelRadius, wheelWidth);

            // --- Exit point beside the driver (left) door ---
            var exit = new GameObject("ExitPoint");
            exit.transform.SetParent(root.transform, false);
            exit.transform.localPosition = new Vector3(-(halfW + 1f), 0f, 0.5f);
            car._exitPoint = exit.transform;

            return car;
        }

        /// <summary>
        /// Create one visual-only wheel: a cylinder rotated so its round face points
        /// sideways. Unity cylinders are 2 units tall along local Y, so a unit-1
        /// scale gives a radius of ~0.5; we scale to the requested radius/width.
        /// </summary>
        private void CreateWheel(Transform parent, string wheelName, Vector3 localPos, float radius, float width)
        {
            var wheel = GameObject.CreatePrimitive(PrimitiveType.Cylinder);
            wheel.name = wheelName;
            // Wheels are decorative only — strip the collider so physics stays simple.
            DestroyImmediate(wheel.GetComponent<Collider>());
            wheel.transform.SetParent(parent, false);
            wheel.transform.localPosition = localPos;
            // Rotate 90° about Z so the cylinder's axis runs left-right (a wheel).
            wheel.transform.localRotation = Quaternion.Euler(0f, 0f, 90f);
            // Cylinder default: diameter 1 (X/Z), height 2 (Y). Scale to radius/width.
            wheel.transform.localScale = new Vector3(radius * 2f, width * 0.5f, radius * 2f);
            _wheels.Add(wheel.transform);
        }

        /// <summary>
        /// Enable or disable player driving control. When enabling, the Rigidbody is
        /// woken so it responds immediately; when disabling, the car coasts to rest.
        /// </summary>
        public void SetDriven(bool driven)
        {
            _driven = driven;
            if (_rb == null) return;

            if (driven)
            {
                _rb.WakeUp();
            }
        }

        private void FixedUpdate()
        {
            // Inactive when not driven: read no input and let damping settle the car.
            if (!_driven || _rb == null) return;

            DriveInput.ReadMove(out float steer, out float throttle);
            bool handbrake = Input.GetKey(KeyCode.Space);

            // Signed forward speed along the car's nose.
            Vector3 velocity = _rb.linearVelocity;
            float forwardSpeed = Vector3.Dot(velocity, transform.forward);
            _speedMps = forwardSpeed;
            SpinWheels(forwardSpeed);

            ApplyDrive(throttle, forwardSpeed, handbrake);
            ApplySteering(steer, forwardSpeed);
            ApplyGrip(velocity, handbrake);
            ClampSpeed();
        }

        /// <summary>
        /// Spin the visual cylinders about their axle. The 90° Z rotation lays the
        /// cylinder's Y axis across the car, so roll is an extra Y rotation.
        /// </summary>
        private void SpinWheels(float forwardSpeed)
        {
            const float radius = 0.5f;
            _wheelSpin += (forwardSpeed / radius) * Mathf.Rad2Deg * Time.fixedDeltaTime;
            Quaternion spin = Quaternion.Euler(0f, _wheelSpin, 90f);
            for (int i = 0; i < _wheels.Count; i++)
            {
                if (_wheels[i] != null) _wheels[i].localRotation = spin;
            }
        }

        /// <summary>Accelerate, reverse, or brake along the car's forward axis.</summary>
        private void ApplyDrive(float throttle, float forwardSpeed, bool handbrake)
        {
            if (handbrake && Mathf.Abs(forwardSpeed) > 0.2f)
            {
                float sign = Mathf.Sign(forwardSpeed);
                _rb.AddForce(transform.forward * (-sign * BrakeForce * Time.fixedDeltaTime),
                    ForceMode.VelocityChange);
            }

            if (throttle > 0.01f)
            {
                // Pressing forward: accelerate (or brake if we were reversing).
                // VelocityChange is mass-independent, so this is a direct m/s delta.
                float force = forwardSpeed < -0.1f ? BrakeForce : Acceleration;
                _rb.AddForce(transform.forward * (throttle * force * Time.fixedDeltaTime),
                    ForceMode.VelocityChange);
            }
            else if (throttle < -0.01f)
            {
                // Pressing back: brake if moving forward, else reverse.
                float force = forwardSpeed > 0.1f ? BrakeForce : Acceleration;
                _rb.AddForce(transform.forward * (throttle * force * Time.fixedDeltaTime),
                    ForceMode.VelocityChange);
            }
        }

        /// <summary>
        /// Steer by rotating the body. Steering scales with forward speed and
        /// reverses when driving backwards. Below a crawl there is nothing to yaw.
        /// </summary>
        private void ApplySteering(float steer, float forwardSpeed)
        {
            if (Mathf.Abs(forwardSpeed) < 0.15f || Mathf.Abs(steer) < 0.01f)
                return;

            // Keep a floor so the car can turn as soon as it is rolling.
            // Still scales up toward top speed.
            float speedFactor = Mathf.Clamp(Mathf.Abs(forwardSpeed) / MaxSpeed, 0.35f, 1f);
            float direction   = Mathf.Sign(forwardSpeed); // flip steering in reverse
            float yaw = steer * direction * TurnSpeed * speedFactor * Time.fixedDeltaTime;

            Quaternion turn = Quaternion.Euler(0f, yaw, 0f);
            _rb.MoveRotation(_rb.rotation * turn);
        }

        /// <summary>
        /// Apply mild lateral grip: bleed off sideways velocity so the car doesn't
        /// slide forever, while leaving forward/back motion untouched.
        /// </summary>
        private void ApplyGrip(Vector3 velocity, bool handbrake)
        {
            float grip = handbrake ? GripFactor * 0.25f : GripFactor;
            float lateralSpeed = Vector3.Dot(velocity, transform.right);
            Vector3 lateralCorrection = -transform.right * lateralSpeed *
                Mathf.Clamp01(grip * Time.fixedDeltaTime);
            _rb.AddForce(lateralCorrection, ForceMode.VelocityChange);
        }

        /// <summary>Clamp forward speed (and a tighter reverse cap) for arcade feel.</summary>
        private void ClampSpeed()
        {
            Vector3 velocity = _rb.linearVelocity;
            float forwardSpeed = Vector3.Dot(velocity, transform.forward);
            float clamped = Mathf.Clamp(forwardSpeed, -MaxReverseSpeed, MaxSpeed);

            if (!Mathf.Approximately(clamped, forwardSpeed))
            {
                // Replace the forward component with the clamped value, keep the rest.
                Vector3 forwardComponent = transform.forward * forwardSpeed;
                Vector3 otherComponents = velocity - forwardComponent;
                _rb.linearVelocity = otherComponents + transform.forward * clamped;
            }
        }
    }
}
