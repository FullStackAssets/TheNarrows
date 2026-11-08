## GameWorld.gd
## Playable vertical-slice bootstrap, assembled entirely in code so it needs no
## hand-authored node graph. On _ready it builds:
##   - a WorldEnvironment (sky + filmic tonemap), wired into WeatherController
##   - a directional sun with shadows
##   - an infinite ground collider + a ground plane centered on the road graph
##   - the OSM road mesh (via MapMeshBaker)
##   - a PlayerController, weather particles, and StreetLoop (walk, street
##     banner, weather/neon, one contact mission)
## Map data © OpenStreetMap contributors, ODbL.
## Active Godot track — not a production build. See res://EXPORT.md.
## Project QUAHOG / Godot 4

extends Node3D

const ROAD_DATA: String = "res://data/southcoast-roads.json"
const Contract := preload("res://scripts/world/OsmMapContract.gd")

func _ready() -> void:
	var graph := Contract.load_file(ROAD_DATA)
	var env := _setup_environment()
	var sun := _setup_sun()
	_setup_ground(graph)
	var baker := _setup_roads(graph)
	var vfx := _setup_weather_vfx()
	var fog := _setup_fog(env)
	var neon := _setup_neon(env)
	var spawn := _find_spawn(graph)
	var player := _spawn_player(spawn)
	# Set dusk before the street loop reads the clock. set_time emits
	# hour_changed; the loop connects after this, then reads current_time.
	var clock := get_node_or_null("/root/TimeOfDayClock")
	if clock != null and clock.has_method("set_time"):
		clock.set_time(19)
	_start_street_loop(player, graph, sun, baker, vfx, fog, neon)
	_init_world_state()
	print("[GameWorld] Ready — player spawned at %s" % spawn)

func _setup_environment() -> Environment:
	var we := WorldEnvironment.new()
	we.name = "WorldEnvironment"
	var env := Environment.new()
	env.background_mode = Environment.BG_SKY
	var sky := Sky.new()
	sky.sky_material = ProceduralSkyMaterial.new()
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.65
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.glow_enabled = true
	env.adjustment_enabled = true
	we.environment = env
	add_child(we)

	var weather := get_node_or_null("/root/WeatherController")
	if weather != null and weather.has_method("bind_environment"):
		weather.bind_environment(env)
	var post := get_node_or_null("/root/PostProcessManager")
	if post != null and post.has_method("set_environment"):
		post.set_environment(env)
	return env

func _setup_sun() -> DirectionalLight3D:
	var sun := DirectionalLight3D.new()
	sun.name = "Sun"
	sun.rotation_degrees = Vector3(-18.0, -40.0, 0.0)
	sun.light_energy = 0.55
	sun.light_color = Color(1.0, 0.62, 0.42)
	sun.shadow_enabled = true
	add_child(sun)
	return sun

func _setup_ground(graph: Dictionary) -> void:
	var body := StaticBody3D.new()
	body.name = "Ground"
	body.collision_layer = 0b0000_0001
	var col := CollisionShape3D.new()
	col.shape = WorldBoundaryShape3D.new()
	body.add_child(col)
	add_child(body)

	var center := Contract.bounds_center(graph.get("roads", []))
	var mi := MeshInstance3D.new()
	mi.name = "GroundVisual"
	var pm := PlaneMesh.new()
	pm.size = Vector2(30000.0, 30000.0)
	mi.mesh = pm
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.07, 0.07, 0.09)
	mat.roughness = 1.0
	mi.material_override = mat
	mi.position = Vector3(center.x, -0.05, center.y)
	add_child(mi)

func _setup_roads(graph: Dictionary) -> Node:
	var baker := Node3D.new()
	baker.set_script(load("res://scripts/world/MapMeshBaker.gd"))
	baker.name = "MapMeshBaker"
	baker.graph_override = graph
	add_child(baker)
	return baker

func _setup_weather_vfx() -> Node:
	var vfx := Node3D.new()
	vfx.set_script(load("res://scripts/world/WeatherVisualEffects.gd"))
	vfx.name = "WeatherVisualEffects"

	var rain := _make_precip(1600, Color(0.72, 0.8, 0.9, 0.45), Vector2(0.04, 0.55), 22.0)
	rain.name = "Rain"
	vfx.add_child(rain)
	vfx.rain_particles = rain

	var snow := _make_precip(700, Color(0.9, 0.92, 0.95, 0.7), Vector2(0.08, 0.08), 6.0)
	snow.name = "Snow"
	vfx.add_child(snow)
	vfx.snow_particles = snow

	var flash := OmniLight3D.new()
	flash.name = "Lightning"
	flash.light_color = Color(0.85, 0.9, 1.0)
	flash.omni_range = 80.0
	flash.position = Vector3(0.0, 20.0, 0.0)
	vfx.add_child(flash)
	vfx.lightning_light = flash

	add_child(vfx)
	return vfx

func _make_precip(amount: int, color: Color, size: Vector2, speed: float) -> GPUParticles3D:
	var particles := GPUParticles3D.new()
	particles.amount = amount
	particles.lifetime = 1.4
	particles.visibility_aabb = AABB(Vector3(-40, -30, -40), Vector3(80, 60, 80))
	particles.emitting = false
	var process := ParticleProcessMaterial.new()
	process.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	process.emission_box_extents = Vector3(24, 1, 24)
	process.direction = Vector3(0, -1, 0)
	process.spread = 8.0
	process.initial_velocity_min = speed * 0.8
	process.initial_velocity_max = speed
	process.gravity = Vector3(0, -4, 0)
	particles.process_material = process
	var quad := QuadMesh.new()
	quad.size = size
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = color
	mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	quad.material = mat
	particles.draw_pass_1 = quad
	return particles

func _setup_fog(env: Environment) -> Node:
	var fog := Node.new()
	fog.set_script(load("res://scripts/world/FogController.gd"))
	fog.name = "FogController"
	fog.environment = env
	add_child(fog)
	return fog

func _setup_neon(env: Environment) -> Node:
	var neon := Node.new()
	neon.set_script(load("res://scripts/world/CoastalNeonPostProcess.gd"))
	neon.name = "CoastalNeon"
	neon.apply_initial_grade = false
	neon.environment = env
	add_child(neon)
	return neon

func _find_spawn(graph: Dictionary) -> Vector3:
	var roads: Array = graph.get("roads", [])
	var index := Contract.pick_spawn_road(roads)
	if index < 0:
		return Vector3(0.0, 3.0, 0.0)
	var pts: Array = roads[index].get("points", [])
	if pts.is_empty():
		return Vector3(0.0, 3.0, 0.0)
	var origin := Contract.road_graph_point_to_godot(float(pts[0][0]), float(pts[0][1]), 3.0)
	return origin

func _spawn_player(spawn: Vector3) -> CharacterBody3D:
	var player := CharacterBody3D.new()
	player.set_script(load("res://scripts/player/PlayerController.gd"))
	player.name = "Player"
	player.collision_layer = 0b0000_0010
	player.collision_mask = 0b0000_0001

	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.4
	cap.height = 1.8
	col.shape = cap
	col.position = Vector3(0.0, 0.9, 0.0)
	player.add_child(col)

	var mesh := MeshInstance3D.new()
	var cm := CapsuleMesh.new()
	cm.radius = 0.4
	cm.height = 1.8
	mesh.mesh = cm
	mesh.position = Vector3(0.0, 0.9, 0.0)
	player.add_child(mesh)

	var rig := Node3D.new()
	rig.name = "CameraRig"
	rig.position = Vector3(0.0, 1.7, 0.0)
	player.add_child(rig)

	var cam := Camera3D.new()
	cam.name = "Camera3D"
	cam.rotation = Vector3(0.0, PI, 0.0)
	cam.current = true
	rig.add_child(cam)

	player.set("camera_rig", rig)
	player.set("camera_node", cam)
	player.position = spawn
	add_child(player)
	return player

func _start_street_loop(player: Node3D, graph: Dictionary, sun: DirectionalLight3D, baker: Node, vfx: Node, fog: Node, neon: Node) -> void:
	var loop := Node.new()
	loop.set_script(load("res://scripts/world/StreetLoop.gd"))
	loop.name = "StreetLoop"
	add_child(loop)
	loop.start(player, graph, {
		"sun": sun,
		"baker": baker,
		"vfx": vfx,
		"fog": fog,
		"neon": neon,
	})

func _init_world_state() -> void:
	var game := get_node_or_null("/root/GameManager")
	if game != null and game.has_method("set_state") and "GameState" in game:
		game.set_state(game.GameState.PLAYING)

	var hud := get_node_or_null("/root/HUDManager")
	if hud != null and hud.has_method("show_hud"):
		hud.show_hud()
