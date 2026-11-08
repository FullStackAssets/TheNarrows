using UnityEngine;

namespace Quahog.SouthCoast
{
    /// <summary>
    /// Code-driven entry point. Runs automatically in every build and in play
    /// mode via [RuntimeInitializeOnLoadMethod], so it needs no scene wiring —
    /// the build scene can be empty. It spawns the manager singletons, runs the
    /// ported SceneBootstrap validation/init pass, and stands up a minimal HUD
    /// (cash counter + health bar) as a visible proof-of-life.
    /// </summary>
    public static class GameBootstrap
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void Boot()
        {
            var root = new GameObject("QUAHOG_Managers");
            Object.DontDestroyOnLoad(root);

            // Spawn the core singletons (their Awake registers Instance).
            root.AddComponent<GameManager>();
            root.AddComponent<PlayerWallet>();
            root.AddComponent<TimeOfDayClock>();
            root.AddComponent<WeatherController>();
            root.AddComponent<HeatManager>();
            root.AddComponent<RevenueManager>();
            root.AddComponent<AudioBarkManager>();
            root.AddComponent<RadioManager>();
            root.AddComponent<MissionManager>();
            root.AddComponent<HUDManager>();
            root.AddComponent<EmpireDatabaseManager>();
            root.AddComponent<SceneObjectRegistry>();
            root.AddComponent<PostProcessManager>();

            // Demo-friendly cadence so the living-world systems are visibly
            // moving in a build (the GDScript defaults are tuned for real play).
            TimeOfDayClock.Instance.SecondsPerGameHour = 3f;      // a full day ~72s
            WeatherController.Instance.StateCheckInterval = 12f;  // roll weather often
            WeatherController.Instance.BlendDuration = 1.5f;      // snappy transitions

            // Run the ported bootstrap (validates singletons, sets prologue state).
            root.AddComponent<SceneBootstrap>();

            // Visible proof-of-life HUD, and some starting cash to show it ticking.
            root.AddComponent<TestSceneBootstrap>().InitTestScene();
            PlayerWallet.Instance.SetBalance(500f);

            // Backdrop camera: an atmospheric sky that reacts to time + weather
            // (so the empty play area isn't a black void), and a Camera to build on.
            root.AddComponent<WorldBackdrop>();

            // Living-world HUD: day/time clock, weather, and wanted-level stars.
            root.AddComponent<WorldHud>();

            // ----- Playable slice. Prefer the OSM clip (Resources/gis/waterfront),
            // which is cut from the shared web slice + building tiles. The hand-authored
            // newbedford grid is only a fallback. Greybox is last so a build is never empty.
            Vector3 playerSpawn, vehicleSpawn;
            var gis = Quahog.SouthCoast.Gis.GisCity.TryBuildFromResources("gis/waterfront");
            if (gis == null)
                gis = Quahog.SouthCoast.Gis.GisCity.TryBuildFromResources("gis/newbedford");
            if (gis != null)
            {
                playerSpawn = gis.PlayerSpawn;
                vehicleSpawn = gis.VehicleSpawn;
            }
            else
            {
                var town = TownGreybox.Build();
                playerSpawn = town.PlayerSpawn;
                vehicleSpawn = town.VehicleSpawn;
            }

            var player = PlayerCharacter.Spawn(playerSpawn);
            var followCam = ThirdPersonCamera.Attach(player.transform);
            var car = CarController.Spawn(vehicleSpawn);
            root.AddComponent<PlayerVehicleInteractor>().Init(player, followCam, car);

            // Street life stays kinematic. On the OSM clip it follows real centre-lines;
            // the greybox keeps the old wander box.
            if (gis != null && gis.TrafficRoutes != null && gis.TrafficRoutes.Count > 0)
                StreetLife.Spawn(gis.TrafficRoutes);
            else
                StreetLife.Spawn();

            Debug.Log("[GameBootstrap] QUAHOG online — managers spawned, HUD up, $500 in the wallet.");
        }
    }
}
