using UnityEngine;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// A CharacterController-driven third-person avatar. Movement is camera-relative
    /// (read from the legacy Horizontal/Vertical axes), Left Shift breaks into a run,
    /// and gravity is integrated through the CharacterController so it respects the
    /// world's existing ground and colliders. The visible body is a primitive Capsule
    /// child whose own Collider is stripped — collision is the CharacterController's
    /// job, the mesh is purely cosmetic — and it yaws to face the travel direction.
    /// </summary>
    [RequireComponent(typeof(CharacterController))]
    public class PlayerCharacter : MonoBehaviour
    {
        // Tuning. Kept as fields (not consts) so they read like designer knobs even
        // though there's no inspector wiring in this code-driven project.
        private const float WalkSpeed   = 4f;
        private const float RunSpeed    = 8f;
        private const float Gravity     = -20f;
        private const float TurnSpeed   = 12f;   // how fast the body yaws toward travel
        private const float StickToGround = -2f; // small downward bias to hug slopes

        private CharacterController _controller;
        private Transform _body;        // cosmetic capsule mesh, rotated to face motion
        private bool _controlEnabled = true;
        private float _verticalVelocity;

        /// <summary>
        /// Builds the player GameObject (CharacterController + cosmetic capsule child)
        /// at the given position and returns the component. The world's ground and
        /// colliders are assumed to already exist at the spawn point.
        /// </summary>
        public static PlayerCharacter Spawn(Vector3 position)
        {
            var go = new GameObject("Player");
            go.transform.position = position;

            var controller = go.AddComponent<CharacterController>();
            // A roughly human-sized capsule, centred on its own height.
            controller.height = 2f;
            controller.radius = 0.4f;
            controller.center = new Vector3(0f, 1f, 0f);
            controller.stepOffset = 0.35f;
            controller.slopeLimit = 50f;

            var player = go.AddComponent<PlayerCharacter>();
            player.BuildBody();
            return player;
        }

        /// <summary>
        /// Enables or disables player control. While disabled (for example, when the
        /// player is riding in a vehicle) all input is ignored. Showing/hiding the
        /// avatar is the caller's responsibility.
        /// </summary>
        public void SetControlEnabled(bool enabled)
        {
            _controlEnabled = enabled;
        }

        private void Awake()
        {
            _controller = GetComponent<CharacterController>();
            // Spawn() builds the body, but guard for the case where this component is
            // added directly so we never run without a mesh child.
            if (_body == null) BuildBody();
        }

        /// <summary>Creates the cosmetic capsule child and removes its collider.</summary>
        private void BuildBody()
        {
            if (_body != null) return;

            var visual = GameObject.CreatePrimitive(PrimitiveType.Capsule);
            visual.name = "Body";

            // The CharacterController handles collision; drop the capsule's own collider.
            var col = visual.GetComponent<Collider>();
            if (col != null) Destroy(col);

            var t = visual.transform;
            t.SetParent(transform, false);
            // Capsule pivot is its centre; lift it so its feet sit at the controller base.
            t.localPosition = new Vector3(0f, 1f, 0f);
            _body = t;
        }

        private void Update()
        {
            if (_controller == null) return;

            // Camera-relative basis. Re-read Camera.main each frame because the follow
            // camera may be created/retargeted at runtime; fall back to world axes.
            Vector3 forward = Vector3.forward;
            Vector3 right = Vector3.right;

            var cam = Camera.main;
            if (cam != null)
            {
                forward = cam.transform.forward;
                right = cam.transform.right;
                // Flatten onto the ground plane so looking up/down doesn't scale speed.
                forward.y = 0f;
                right.y = 0f;
                // Looking straight up/down leaves a zero ground vector.
                if (forward.sqrMagnitude < 1e-6f) forward = Vector3.forward;
                else forward.Normalize();
                if (right.sqrMagnitude < 1e-6f) right = Vector3.right;
                else right.Normalize();
            }

            float h = 0f;
            float v = 0f;
            bool running = false;
            if (_controlEnabled)
            {
                DriveInput.ReadMove(out h, out v);
                running = Input.GetKey(KeyCode.LeftShift);
            }

            Vector3 wish = forward * v + right * h;
            // Clamp magnitude so diagonal input isn't faster than cardinal input.
            if (wish.sqrMagnitude > 1f) wish.Normalize();

            float speed = running ? RunSpeed : WalkSpeed;
            Vector3 horizontalMove = wish * speed;

            // Gravity / ground handling through the CharacterController.
            // Space hops while walking. Driving uses Space as the handbrake instead,
            // and control is disabled for the whole time the player is in the car.
            bool jump = _controlEnabled && _controller.isGrounded && Input.GetKeyDown(KeyCode.Space);
            if (jump)
            {
                _verticalVelocity = 6.5f;
            }
            else if (_controller.isGrounded && _verticalVelocity < 0f)
            {
                _verticalVelocity = StickToGround;
            }
            else
            {
                _verticalVelocity += Gravity * Time.deltaTime;
            }

            Vector3 motion = horizontalMove;
            motion.y = _verticalVelocity;
            _controller.Move(motion * Time.deltaTime);

            // Yaw the cosmetic body toward the travel direction (ignore vertical).
            if (_body != null && wish.sqrMagnitude > 0.0001f)
            {
                Quaternion target = Quaternion.LookRotation(wish, Vector3.up);
                _body.rotation = Quaternion.Slerp(_body.rotation, target, TurnSpeed * Time.deltaTime);
            }
        }
    }
}
