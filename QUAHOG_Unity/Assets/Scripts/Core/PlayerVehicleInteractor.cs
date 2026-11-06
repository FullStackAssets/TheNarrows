using UnityEngine;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// Glue between the on-foot player, the follow camera, and a car. Walk near the
    /// car and press <c>F</c> to get in; press <c>F</c> again to get out. Entering
    /// disables and hides the avatar, hands driving control to the car, and points
    /// the camera at it; exiting reverses all of that and drops the player at the
    /// car's exit point. Enter and exit are E (same key as the web slice) or F.
    /// Kept deliberately small — it owns the cross-cutting handoff so the player,
    /// camera, and car components stay independent of one another.
    /// </summary>
    public sealed class PlayerVehicleInteractor : MonoBehaviour
    {
        private const float EnterRange = 3.5f;        // metres from the car to allow entry

        private PlayerCharacter _player;
        private ThirdPersonCamera _camera;
        private CarController _car;
        private bool _driving;

        /// <summary>Wires the interactor to the three slice actors.</summary>
        public void Init(PlayerCharacter player, ThirdPersonCamera camera, CarController car)
        {
            _player = player;
            _camera = camera;
            _car = car;
        }

        private void Update()
        {
            if (_player == null || _car == null) return;

            if (!Input.GetKeyDown(KeyCode.E) && !Input.GetKeyDown(KeyCode.F)) return;

            if (_driving) ExitCar();
            else TryEnterCar();
        }

        private void TryEnterCar()
        {
            float distance = Vector3.Distance(_player.transform.position, _car.transform.position);
            if (distance > EnterRange) return;

            _driving = true;
            _player.SetControlEnabled(false);
            _player.gameObject.SetActive(false);     // hide the avatar while riding
            _car.SetDriven(true);
            if (_camera != null) _camera.SetTarget(_car.transform);
        }

        private void ExitCar()
        {
            _driving = false;
            _car.SetDriven(false);

            // Drop the player at the car's exit point (fall back to beside the car).
            Vector3 exit = _car.ExitPoint != null
                ? _car.ExitPoint.position
                : _car.transform.position - _car.transform.right * 2f;

            _player.transform.position = exit;
            _player.gameObject.SetActive(true);
            _player.SetControlEnabled(true);
            if (_camera != null) _camera.SetTarget(_player.transform);
        }

        // A tiny on-screen prompt so the control is discoverable in the demo build.
        private void OnGUI()
        {
            if (_player == null || _car == null) return;

            string hint = null;
            if (_driving)
            {
                hint = "Press E or F to exit the car";
            }
            else if (_player.gameObject.activeSelf &&
                     Vector3.Distance(_player.transform.position, _car.transform.position) <= EnterRange)
            {
                hint = "Press E or F to enter the car";
            }

            if (hint != null)
            {
                GUI.Label(new Rect(20f, Screen.height - 40f, 320f, 24f), hint);
            }
        }
    }
}
