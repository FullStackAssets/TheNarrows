#if UNITY_EDITOR
using System;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Quahog.SouthCoast.EditorTools
{
    /// <summary>
    /// Editor-only project config (wrapped in UNITY_EDITOR, under Assets/Editor/,
    /// so it never enters a player build).
    ///
        /// The project ships on the BUILT-IN render pipeline until someone runs
        /// QUAHOG/Enable URP in the editor. URP is in the manifest but not assigned.
        /// The playable slice (GIS city, player, car, street life) asks for
        /// URP/Lit and falls back to Standard, then Sprites/Default, so a built-in
        /// player still has materials. The HUD is uGUI and does not depend on that.
    ///
    /// A pre-build step stamps product identity (name / company / Android bundle
    /// id) so the player has a proper identity. URP is a deliberate opt-in via the
    /// "QUAHOG/Enable URP" menu command — run it from a Unity editor when you're
    /// ready, then commit Assets/Settings/ + the updated ProjectSettings.
    /// </summary>
    [InitializeOnLoad]
    public static class QuahogBootstrap
    {
        const string SettingsDir  = "Assets/Settings";
        const string UrpAssetPath = SettingsDir + "/URP-Quahog.asset";
        const string RendererPath = SettingsDir + "/URP-Quahog-Renderer.asset";

        static QuahogBootstrap()
        {
            // Identity only — no render-pipeline changes on load.
            EditorApplication.delayCall += EnsureIdentity;
        }

        /// <summary>Stamp product identity. Safe and render-pipeline-independent.</summary>
        [MenuItem("QUAHOG/Set Product Identity")]
        public static void EnsureIdentity()
        {
            try
            {
                if (string.IsNullOrEmpty(PlayerSettings.productName) ||
                    PlayerSettings.productName == "QUAHOG_Unity")
                    PlayerSettings.productName = "Project QUAHOG";

                if (string.IsNullOrEmpty(PlayerSettings.companyName) ||
                    PlayerSettings.companyName == "DefaultCompany")
                    PlayerSettings.companyName = "Rockwharf Games";

                PlayerSettings.SetApplicationIdentifier(NamedBuildTarget.Android, "com.rockwharf.quahog");
                AssetDatabase.SaveAssets();
            }
            catch (Exception e)
            {
                Debug.LogWarning("[QuahogBootstrap] Identity stamp skipped: " + e.Message);
            }
        }

        /// <summary>
        /// Opt-in: create the URP pipeline + renderer via URP's own API and assign
        /// them in Graphics/Quality settings. Run when you're ready to move off the
        /// built-in pipeline (needs a Unity editor), then commit the results.
        /// </summary>
        [MenuItem("QUAHOG/Enable URP")]
        public static void EnableUrp()
        {
            try
            {
                if (GraphicsSettings.defaultRenderPipeline is UniversalRenderPipelineAsset)
                {
                    Debug.Log("[QuahogBootstrap] URP already assigned.");
                    return;
                }

                if (!AssetDatabase.IsValidFolder(SettingsDir))
                    AssetDatabase.CreateFolder("Assets", "Settings");

                var urp = AssetDatabase.LoadAssetAtPath<UniversalRenderPipelineAsset>(UrpAssetPath);
                if (urp == null)
                {
                    var rendererData = ScriptableObject.CreateInstance<UniversalRendererData>();
                    AssetDatabase.CreateAsset(rendererData, RendererPath);

                    urp = UniversalRenderPipelineAsset.Create(rendererData);
                    AssetDatabase.CreateAsset(urp, UrpAssetPath);
                    AssetDatabase.SaveAssets();
                }

                GraphicsSettings.defaultRenderPipeline = urp;
                QualitySettings.renderPipeline = urp;
                AssetDatabase.SaveAssets();
                Debug.Log("[QuahogBootstrap] URP created and assigned. Commit Assets/Settings/ + ProjectSettings.");
            }
            catch (Exception e)
            {
                Debug.LogWarning("[QuahogBootstrap] Enable URP failed: " + e.Message);
            }
        }

        /// <summary>
        /// Configure WebGL so the player loads from any static host (GitHub Pages,
        /// itch.io, plain file servers) that can't send a "Content-Encoding: gzip"
        /// header. Decompression Fallback makes the loader decompress in JS, so the
        /// gzip-compressed build no longer depends on server headers.
        /// </summary>
        [MenuItem("QUAHOG/Configure WebGL Static Hosting")]
        public static void ConfigureWebGLForStaticHosting()
        {
            try
            {
                PlayerSettings.WebGL.compressionFormat = WebGLCompressionFormat.Gzip;
                PlayerSettings.WebGL.decompressionFallback = true;
                Debug.Log("[QuahogBootstrap] WebGL set to Gzip + Decompression Fallback (static-host friendly).");
            }
            catch (Exception e)
            {
                Debug.LogWarning("[QuahogBootstrap] WebGL config skipped: " + e.Message);
            }
        }

        /// <summary>Pre-build (incl. Build Automation): stamp identity, and make
        /// WebGL builds static-host friendly. No render-pipeline changes.</summary>
        private class BuildHook : IPreprocessBuildWithReport
        {
            public int callbackOrder => 0;
            public void OnPreprocessBuild(BuildReport report)
            {
                EnsureIdentity();
                if (report.summary.platform == BuildTarget.WebGL)
                    ConfigureWebGLForStaticHosting();
            }
        }
    }
}
#endif
