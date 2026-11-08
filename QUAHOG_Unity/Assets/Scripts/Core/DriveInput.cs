using UnityEngine;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// Shared walk/drive axes. The legacy Horizontal/Vertical axes win when the
    /// Input Manager is supplying them. WASD and the arrow keys fill in when those
    /// axes read as zero, which is what a WebGL player sees if the default input
    /// asset was never committed (this project only commits ProjectVersion and
    /// EditorBuildSettings).
    /// </summary>
    public static class DriveInput
    {
        public static void ReadMove(out float horizontal, out float vertical)
        {
            horizontal = Input.GetAxis("Horizontal");
            vertical = Input.GetAxis("Vertical");

            if (Mathf.Abs(horizontal) < 0.01f)
            {
                if (Input.GetKey(KeyCode.A) || Input.GetKey(KeyCode.LeftArrow)) horizontal -= 1f;
                if (Input.GetKey(KeyCode.D) || Input.GetKey(KeyCode.RightArrow)) horizontal += 1f;
            }
            if (Mathf.Abs(vertical) < 0.01f)
            {
                if (Input.GetKey(KeyCode.S) || Input.GetKey(KeyCode.DownArrow)) vertical -= 1f;
                if (Input.GetKey(KeyCode.W) || Input.GetKey(KeyCode.UpArrow)) vertical += 1f;
            }

            horizontal = Mathf.Clamp(horizontal, -1f, 1f);
            vertical = Mathf.Clamp(vertical, -1f, 1f);
        }
    }
}
