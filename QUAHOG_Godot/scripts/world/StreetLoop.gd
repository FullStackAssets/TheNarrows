## StreetLoop.gd
## On-foot slice loop for the Godot track:
##   walk the baked OSM road, read the real street name, meet one contact.
## District banners use the road-graph street name and highway tag.
## They do not invent a municipality when `city` is null.
## Parody mission titles (New Sefton and the rest) stay in MissionManager;
## they are in-world flavor, not OSM labels.
## Project QUAHOG / Godot 4

extends Node

const Contract := preload("res://scripts/world/OsmMapContract.gd")
const Neon := preload("res://scripts/world/HighwayNeon.gd")
const ContactScript := preload("res://scripts/world/StreetContact.gd")
const GameConstantsScript := preload("res://scripts/core/GameConstants.gd")

const INTRO_MISSION := "ns_01_intro"

var _player: Node3D
var _roads: Array = []
var _cells: Dictionary = {}
var _sun: DirectionalLight3D
var _baker: Node
var _vfx: Node
var _fog: Node
var _neon: Node
var _post: Node
var _weather_name := "CLEAR"
var _hour := 19
var _highway := ""
var _street_index := -1
var _poll := 0.0
var _lightning := 0.0
var _intro_done := false

var _cash_label: Label
var _status_label: Label
var _objective_label: Label
var _hint_label: Label
var _pause_label: Label

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_process(false)

func start(player: Node3D, graph: Dictionary, world: Dictionary) -> void:
	_player = player
	_roads = graph.get("roads", [])
	_cells = Contract.build_cell_index(_roads)
	_sun = world.get("sun")
	_baker = world.get("baker")
	_vfx = world.get("vfx")
	_fog = world.get("fog")
	_neon = world.get("neon")
	_build_hud()
	_grant_starting_cash()
	_bind_clock_and_weather()
	_place_contact()
	_begin_intro()
	_refresh_street(true)
	_apply_mood()
	set_process(true)
	if _player != null and not _player.interact_target_changed.is_connected(_on_interact_target):
		_player.interact_target_changed.connect(_on_interact_target)

func _unhandled_input(event: InputEvent) -> void:
	if _player == null:
		return
	if event.is_action_pressed("pause") and not event.is_echo():
		_toggle_pause()
		get_viewport().set_input_as_handled()

func _process(delta: float) -> void:
	if _player == null:
		return
	var game := get_node_or_null("/root/GameManager")
	if game != null and game.get_state() == game.GameState.PAUSED:
		return
	if _vfx != null:
		_vfx.global_position = _player.global_position + Vector3(0.0, 12.0, 0.0)
	_poll += delta
	if _poll >= 0.25:
		_poll = 0.0
		_refresh_street(false)
	if _weather_name == "NOREASTER":
		_lightning += delta
		if _lightning >= 7.0:
			_lightning = 0.0
			if _vfx != null and _vfx.has_method("trigger_lightning"):
				_vfx.trigger_lightning()
	else:
		_lightning = 0.0

func _toggle_pause() -> void:
	var game := get_node_or_null("/root/GameManager")
	if game == null or not game.has_method("set_state"):
		return
	if game.get_state() == game.GameState.PAUSED:
		game.set_state(game.GameState.PLAYING)
		if _player.has_method("capture_mouse"):
			_player.capture_mouse()
		if _pause_label != null:
			_pause_label.visible = false
	else:
		game.set_state(game.GameState.PAUSED)
		if _player.has_method("release_mouse"):
			_player.release_mouse()
		if _pause_label != null:
			_pause_label.visible = true

func _bind_clock_and_weather() -> void:
	var clock := get_node_or_null("/root/TimeOfDayClock")
	if clock != null:
		_hour = int(clock.current_time)
		if not clock.hour_changed.is_connected(_on_hour):
			clock.hour_changed.connect(_on_hour)
	var weather := get_node_or_null("/root/WeatherController")
	if weather != null:
		_weather_name = weather.get_state_name() if weather.has_method("get_state_name") else "CLEAR"
		if not weather.weather_changed.is_connected(_on_weather):
			weather.weather_changed.connect(_on_weather)
	_post = get_node_or_null("/root/PostProcessManager")

func _on_hour(hour: int) -> void:
	_hour = hour
	_apply_mood()

func _on_weather(state: String) -> void:
	_weather_name = state
	_apply_mood()

func _apply_mood() -> void:
	_apply_sun(_hour)
	if _baker != null and _baker.has_method("set_neon_energy"):
		_baker.set_neon_energy(Neon.emission_energy(_weather_name, _hour))
	if _baker != null and _baker.has_method("set_road_wet"):
		_baker.set_road_wet(_weather_name == "COASTAL_RAIN" or _weather_name == "NOREASTER")
	if _vfx != null and _vfx.has_method("apply_state_name"):
		_vfx.apply_state_name(_weather_name)
	if _fog != null and _fog.has_method("apply_named_state"):
		_fog.apply_named_state(_weather_name, 1.2)
	if _post != null and _post.has_method("set_weather"):
		_post.set_weather(_post_weather(_weather_name))
	if _post != null and _post.has_method("apply_osm_highway_grade"):
		_post.apply_osm_highway_grade(_highway)
	if _post != null and _post.has_method("on_hour_changed"):
		_post.on_hour_changed(_hour)
	if _neon != null:
		if _weather_name == "CLEAR" and _neon.has_method("apply_clear_highway_ambient"):
			_neon.apply_clear_highway_ambient(_highway)
		elif _neon.has_method("cancel_tint"):
			_neon.cancel_tint()
	_refresh_status()

func _apply_sun(hour: int) -> void:
	if _sun == null:
		return
	# Art-directed coastal light. Not a solar model and not a measured reading.
	if Neon.is_night(hour):
		_sun.light_energy = 0.08
		_sun.light_color = Color(0.45, 0.52, 0.75)
	elif hour >= 17:
		_sun.light_energy = 0.55
		_sun.light_color = Color(1.0, 0.62, 0.42)
	else:
		_sun.light_energy = 1.15
		_sun.light_color = Color(1.0, 0.96, 0.90)

func _post_weather(state: String) -> int:
	if _post == null:
		return 0
	match state:
		"DENSE_FOG":
			return _post.WeatherState.DENSE_FOG
		"COASTAL_RAIN":
			return _post.WeatherState.COASTAL_RAIN
		"NOREASTER":
			return _post.WeatherState.NOREASTER
		_:
			return _post.WeatherState.CLEAR

func _refresh_street(force: bool) -> void:
	if _player == null or _roads.is_empty():
		return
	var hit: Dictionary = Contract.nearest_road(_roads, _cells, _player.global_position.x, _player.global_position.z)
	if hit.is_empty():
		if force or _street_index != -2:
			_street_index = -2
			_highway = ""
			_show_banner("")
		return
	var index := int(hit["index"])
	if not force and index == _street_index:
		return
	_street_index = index
	var road: Dictionary = hit["road"]
	_highway = Contract.osm_text(road.get("highway"))
	_show_banner(Contract.banner_for_road(road))
	if _weather_name == "CLEAR" and _neon != null and _neon.has_method("apply_clear_highway_ambient"):
		_neon.apply_clear_highway_ambient(_highway)
	if _post != null and _post.has_method("apply_osm_highway_grade"):
		_post.apply_osm_highway_grade(_highway)

func _show_banner(text: String) -> void:
	if text == "":
		return
	var hud := get_node_or_null("/root/HUDManager")
	if hud != null and hud.has_method("show_district"):
		hud.show_district(text)

func _place_contact() -> void:
	var index := Contract.pick_spawn_road(_roads)
	if index < 0:
		push_warning("[StreetLoop] no road available for the contact.")
		return
	var road: Dictionary = _roads[index]
	var pose: Dictionary = Contract.contact_pose(road.get("points", []))
	if pose.is_empty():
		return
	var body := StaticBody3D.new()
	body.set_script(ContactScript)
	body.name = "StreetContact"
	body.street_name = Contract.osm_text(road.get("name"))
	body.position = pose["position"]
	body.collision_layer = 0b0000_0001
	body.collision_mask = 0

	var shape := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.35
	capsule.height = 2.2
	shape.shape = capsule
	shape.position = Vector3(0.0, 1.1, 0.0)
	body.add_child(shape)

	var mesh := MeshInstance3D.new()
	var cylinder := CylinderMesh.new()
	cylinder.top_radius = 0.18
	cylinder.bottom_radius = 0.28
	cylinder.height = 2.2
	mesh.mesh = cylinder
	mesh.position = Vector3(0.0, 1.1, 0.0)
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Neon.highway_color(Contract.osm_text(road.get("highway")))
	mat.emission_enabled = true
	mat.emission = mat.albedo_color
	mat.emission_energy_multiplier = 1.4
	mesh.material_override = mat
	body.add_child(mesh)

	var label := Label3D.new()
	label.text = "Contact" if body.street_name == "" else body.street_name
	label.position = Vector3(0.0, 2.5, 0.0)
	label.font_size = 48
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.modulate = Color(1.0, 0.85, 0.7)
	label.outline_modulate = Color(0.05, 0.02, 0.04)
	label.outline_size = 8
	body.add_child(label)

	body.contacted.connect(_on_contact)
	add_child(body)

func _on_contact(_player_node) -> void:
	_progress_intro()

func _on_interact_target(target) -> void:
	if _hint_label == null:
		return
	if target != null and target.has_method("interact"):
		_hint_label.text = "E  —  meet contact"
	else:
		_hint_label.text = ""

func _begin_intro() -> void:
	var missions := get_node_or_null("/root/MissionManager")
	if missions == null:
		_set_objective("Walk the street.")
		return
	var mission = missions.get_mission(INTRO_MISSION)
	if mission == null:
		_set_objective("Walk the street.")
		return
	if mission.state == missions.MissionState.AVAILABLE and missions.can_start(INTRO_MISSION):
		missions.start_mission(INTRO_MISSION)
	_set_objective("%s\nMeet the contact on %s." % [mission.display_name, _spawn_street_label()])

func _progress_intro() -> void:
	var missions := get_node_or_null("/root/MissionManager")
	if missions == null:
		return
	var mission = missions.get_mission(INTRO_MISSION)
	if mission == null:
		return
	if mission.state == missions.MissionState.AVAILABLE and missions.can_start(INTRO_MISSION):
		missions.start_mission(INTRO_MISSION)
	if mission.state == missions.MissionState.ACTIVE:
		missions.complete_mission(INTRO_MISSION)
		_intro_done = true
		var next_text := "Paid."
		if mission.unlocks_mission_id != "":
			var nxt = missions.get_mission(mission.unlocks_mission_id)
			if nxt != null:
				next_text = "Paid. Next available: %s." % nxt.display_name
		_set_objective(next_text)
		var hud := get_node_or_null("/root/HUDManager")
		if hud != null and hud.has_method("show_mission_prompt"):
			hud.show_mission_prompt(next_text, 4.0)
	elif mission.state == missions.MissionState.COMPLETED:
		_set_objective("Contact already met.")

func _spawn_street_label() -> String:
	var index := Contract.pick_spawn_road(_roads)
	if index < 0:
		return "this road"
	var label := Contract.banner_for_road(_roads[index])
	return label if label != "" else "this road"

func _grant_starting_cash() -> void:
	var wallet := get_node_or_null("/root/PlayerWallet")
	if wallet == null:
		return
	if wallet.get_balance() <= 0.0:
		wallet.add(GameConstantsScript.STARTING_BALANCE, "starting cash")

func _build_hud() -> void:
	var hud := get_node_or_null("/root/HUDManager")
	if hud == null:
		return

	_cash_label = Label.new()
	_cash_label.name = "CashLabel"
	_cash_label.position = Vector2(24, 18)
	_cash_label.add_theme_font_size_override("font_size", 28)
	_cash_label.add_theme_color_override("font_color", Color(0.55, 0.95, 0.65))
	_cash_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(_cash_label)
	var wallet := get_node_or_null("/root/PlayerWallet")
	if wallet != null:
		var refresh := func(balance: float) -> void:
			if is_instance_valid(_cash_label):
				_cash_label.text = "$%d" % int(balance)
		refresh.call(wallet.get_balance())
		if not wallet.balance_changed.is_connected(refresh):
			wallet.balance_changed.connect(refresh)

	_status_label = Label.new()
	_status_label.position = Vector2(24, 52)
	_status_label.add_theme_font_size_override("font_size", 18)
	_status_label.add_theme_color_override("font_color", Color(0.85, 0.88, 0.95))
	_status_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(_status_label)

	var banner := Control.new()
	banner.set_script(load("res://scripts/ui/DistrictNameDisplay.gd"))
	banner.name = "StreetBanner"
	banner.anchor_left = 0.5
	banner.anchor_right = 0.5
	banner.offset_left = -420.0
	banner.offset_right = 420.0
	banner.offset_top = 28.0
	banner.offset_bottom = 96.0
	banner.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var banner_label := Label.new()
	banner_label.set_anchors_preset(Control.PRESET_FULL_RECT)
	banner_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	banner_label.add_theme_font_size_override("font_size", 32)
	banner_label.add_theme_color_override("font_color", Color(1.0, 0.55, 0.78))
	banner_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	banner.add_child(banner_label)
	banner.label = banner_label
	hud.add_child(banner)
	hud.district_display = banner

	var prompt := PanelContainer.new()
	prompt.name = "MissionPrompt"
	prompt.anchor_left = 0.5
	prompt.anchor_right = 0.5
	prompt.anchor_top = 1.0
	prompt.anchor_bottom = 1.0
	prompt.offset_left = -280.0
	prompt.offset_right = 280.0
	prompt.offset_top = -150.0
	prompt.offset_bottom = -100.0
	prompt.visible = false
	prompt.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var prompt_label := Label.new()
	prompt_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	prompt_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	prompt.add_child(prompt_label)
	hud.add_child(prompt)
	hud.mission_prompt_panel = prompt
	hud.mission_prompt_label = prompt_label

	_objective_label = Label.new()
	_objective_label.anchor_left = 0.5
	_objective_label.anchor_right = 0.5
	_objective_label.anchor_top = 1.0
	_objective_label.anchor_bottom = 1.0
	_objective_label.offset_left = -360.0
	_objective_label.offset_right = 360.0
	_objective_label.offset_top = -96.0
	_objective_label.offset_bottom = -36.0
	_objective_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_objective_label.add_theme_font_size_override("font_size", 20)
	_objective_label.add_theme_color_override("font_color", Color(0.95, 0.93, 0.88))
	_objective_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(_objective_label)

	_hint_label = Label.new()
	_hint_label.anchor_left = 0.5
	_hint_label.anchor_right = 0.5
	_hint_label.anchor_top = 1.0
	_hint_label.anchor_bottom = 1.0
	_hint_label.offset_left = -200.0
	_hint_label.offset_right = 200.0
	_hint_label.offset_top = -36.0
	_hint_label.offset_bottom = -8.0
	_hint_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hint_label.add_theme_font_size_override("font_size", 18)
	_hint_label.add_theme_color_override("font_color", Color(1.0, 0.85, 0.45))
	_hint_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(_hint_label)

	_pause_label = Label.new()
	_pause_label.text = "Paused"
	_pause_label.anchor_left = 0.5
	_pause_label.anchor_right = 0.5
	_pause_label.anchor_top = 0.5
	_pause_label.anchor_bottom = 0.5
	_pause_label.offset_left = -80.0
	_pause_label.offset_right = 80.0
	_pause_label.offset_top = -20.0
	_pause_label.offset_bottom = 20.0
	_pause_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_pause_label.add_theme_font_size_override("font_size", 36)
	_pause_label.visible = false
	_pause_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hud.add_child(_pause_label)

func _set_objective(text: String) -> void:
	if _objective_label != null:
		_objective_label.text = text

func _refresh_status() -> void:
	if _status_label == null:
		return
	var clock := get_node_or_null("/root/TimeOfDayClock")
	var time_text := "%02d:00" % _hour
	if clock != null and clock.has_method("get_time_string"):
		time_text = clock.get_time_string()
	_status_label.text = "%s   %s" % [time_text, _weather_name]
