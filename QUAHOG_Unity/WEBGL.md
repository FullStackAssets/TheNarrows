# Unity WebGL path

The WebGL player is a manual GitHub Actions build of this Unity project. It is
not produced by `vite`, and it is not the Three.js game in `QUAHOG_Web/`.

Workflow: [`.github/workflows/deploy-webgl.yml`](../.github/workflows/deploy-webgl.yml).

- Trigger: `workflow_dispatch` only. A push does not build or deploy.
- Editor image: `6000.0.23f1`, the same version as
  `ProjectSettings/ProjectVersion.txt`.
- Project path: `QUAHOG_Unity`.
- Before the builder starts, the workflow exits if the license secrets are
  missing, or if **both** `UNITY_LICENSE` and `UNITY_SERIAL` are set. game-ci
  follows the serial whenever `UNITY_SERIAL` is non-empty, and a bad serial
  fails with licensing code `20110` even when the `.ulf` would have worked.

Publishing goes to the `gh-pages` branch. That step runs only after a person
dispatches the workflow and the Unity build succeeds. This sandbox did not
dispatch it, did not install the Unity editor, and did not upload a player.

## Secrets

Repository → Settings → Secrets and variables → Actions.

| Secret | Value |
| --- | --- |
| `UNITY_EMAIL` | Unity account email |
| `UNITY_PASSWORD` | Unity account password |
| `UNITY_LICENSE` | Full contents of the personal `.ulf` |
| `UNITY_SERIAL` | Pro/Plus serial, **instead of** `UNITY_LICENSE` |

Personal license file locations:

- Windows: `C:\ProgramData\Unity\Unity_lic.ulf`
- macOS: `/Library/Application Support/Unity/Unity_lic.ulf`
- Linux: `~/.local/share/unity3d/Unity/Unity_lic.ulf`

The helper workflow `.github/workflows/unity-activation.yml` only prints those
steps. It does not request an activation file. Unity no longer supports the
old GameCI `.alf` action.

## What the player build does that this sandbox cannot

`Assets/Editor/QuahogBootstrap.cs` is editor-only (`#if UNITY_EDITOR`). On a
WebGL build it sets Gzip compression and Decompression Fallback so a static
host can serve the player without a `Content-Encoding: gzip` header. The same
command is `QUAHOG/Configure WebGL Static Hosting` in the editor menu.

That script is not part of the headless C# compile. The shim never executes
it. Gzip fallback is therefore unverified until a real Unity WebGL build runs.

## Local editor

Unity Hub, editor `6000.0.23f1`, open the `QUAHOG_Unity` folder. The first
import writes `Library/`, `ProjectSettings` beyond the two committed files, and
`Packages/packages-lock.json`. Those stay untracked. Play Mode needs that
import; the sandbox tests do not replace it.
