## WeatherController.gd
## Autoload singleton — registered in project.godot as "WeatherController"
## 4-state weather FSM: CLEAR, DENSE_FOG, COASTAL_RAIN, NOREASTER
## Converted from WeatherController.cs (Unity C#) for Project QUAHOG / Godot 4

extends Node

# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------
enum WeatherState {
	CLEAR,
	DENSE_FOG,
	COASTAL_RAIN,
	NOREASTER
}

# ---------------------------------------------------------------------------
# Exports
# ---------------------------------------------------------------------------
@export var environment: Environment
@export var blend_duration: float = 6.0
@export var state_check_interval: float = 60.0
@export var state_weights: Array[float] = [0.45, 0.25, 0.20, 0.10]

# ---------------------------------------------------------------------------
# Signals
# ---------------------------------------------------------------------------
signal weather_changed(state: String)
signal blend_progress(t: float)

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
var current_state: WeatherState = WeatherState.CLEAR
var _is_transitioning: bool = false

# Internal timer node for periodic state checks
var _state_timer: Timer

# ---------------------------------------------------------------------------
# Weather visual parameter tables
# [fog_enabled, fog_density, ambient_color]
# ---------------------------------------------------------------------------
const _WEATHER_PARAMS: Dictionary = {
	WeatherState.CLEAR: {
		"fog_enabled": false,
		"fog_density": 0.0,
		"ambient_color": Color(0.55, 0.55, 0.6, 1.0)
	},
	WeatherState.DENSE_FOG: {
		"fog_enabled": true,
		"fog_density": 0.08,
		"ambient_color": Color(0.6, 0.62, 0.65, 1.0)
	},
	WeatherState.COASTAL_RAIN: {
		"fog_enabled": true,
		"fog_density": 0.03,
		"ambient_color": Color(0.35, 0.38, 0.42, 1.0)
	},
	WeatherState.NOREASTER: {
		"fog_enabled": true,
		"fog_density": 0.12,
		"ambient_color": Color(0.25, 0.28, 0.32, 1.0)
	}
}

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------
func _ready() -> void:
	_state_timer = Timer.new()
	_state_timer.wait_time = state_check_interval
	_state_timer.autostart = true
	_state_timer.one_shot = false
	_state_timer.timeout.connect(_on_state_timer_timeout)
	add_child(_state_timer)

	# Apply initial visual state immediately (no transition)
	_apply_weather_visuals(current_state)

# ---------------------------------------------------------------------------
# Timer callback — roll for a new weather state
# ---------------------------------------------------------------------------
func _on_state_timer_timeout() -> void:
	if _is_transitioning:
		return
	var next_state: WeatherState = _pick_weighted_state()
	if next_state != current_state:
		_begin_transition(next_state)

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Bind the world Environment and apply the current state immediately.
## Use this when the scene creates the Environment after the autoload _ready.
func bind_environment(env: Environment) -> void:
	environment = env
	_apply_weather_visuals(current_state)

## Name of the current state. Matches weather_changed and HighwayNeon keys.
func get_state_name() -> String:
	return _state_name(current_state)

## Force the controller into a specific state immediately (or start transition).
func force_state(state: WeatherState) -> void:
	if _is_transitioning:
		return
	_begin_transition(state)

## Replace the state-probability weights at runtime.
## weights must have exactly 4 elements summing to > 0.
func set_state_weights(weights: Array[float]) -> void:
	if weights.size() != 4:
		push_warning("WeatherController: set_state_weights requires exactly 4 elements.")
		return
	state_weights = weights

## Called by the game's phase/chapter system to push a thematic weather state.
func apply_phase_weather(phase: int) -> void:
	match phase:
		0:
			force_state(WeatherState.CLEAR)
		1:
			force_state(WeatherState.COASTAL_RAIN)
		2:
			force_state(WeatherState.DENSE_FOG)
		3:
			force_state(WeatherState.NOREASTER)
		_:
			push_warning("WeatherController: Unknown phase %d — defaulting to CLEAR." % phase)
			force_state(WeatherState.CLEAR)

# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

func _pick_weighted_state() -> WeatherState:
	var total: float = 0.0
	for w in state_weights:
		total += w
	var roll: float = randf() * total
	var cumulative: float = 0.0
	for i in range(state_weights.size()):
		cumulative += state_weights[i]
		if roll < cumulative:
			return i as WeatherState
	return WeatherState.CLEAR

func _begin_transition(target_state: WeatherState) -> void:
	_is_transitioning = true
	_transition_async(target_state)

## Async coroutine-style transition: interpolates blend_progress over blend_duration,
## then snaps to the new state visuals.
func _transition_async(target_state: WeatherState) -> void:
	var elapsed: float = 0.0
	while elapsed < blend_duration:
		await get_tree().process_frame
		elapsed += get_process_delta_time()
		var t: float = clampf(elapsed / blend_duration, 0.0, 1.0)
		blend_progress.emit(t)

	current_state = target_state
	_apply_weather_visuals(current_state)
	weather_changed.emit(_state_name(current_state))
	_is_transitioning = false

func _apply_weather_visuals(state: WeatherState) -> void:
	if environment == null:
		push_warning("WeatherController: No Environment resource assigned.")
		return
	var params: Dictionary = _WEATHER_PARAMS[state]
	environment.fog_enabled = params["fog_enabled"]
	environment.volumetric_fog_density = params["fog_density"]
	environment.fog_density = params["fog_density"]
	environment.ambient_light_color = params["ambient_color"]

func _state_name(state: WeatherState) -> String:
	match state:
		WeatherState.CLEAR:        return "CLEAR"
		WeatherState.DENSE_FOG:    return "DENSE_FOG"
		WeatherState.COASTAL_RAIN: return "COASTAL_RAIN"
		WeatherState.NOREASTER:    return "NOREASTER"
	return "CLEAR"
