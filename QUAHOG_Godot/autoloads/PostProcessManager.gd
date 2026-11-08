## PostProcessManager.gd
## res://autoloads/PostProcessManager.gd
## Autoload singleton — registered in project.godot as "PostProcessManager"
## Full post-processing stack control: glow (bloom), depth-of-field, colour
## adjustments (colour grading), and vignette-style darkening.
## Converted from PostProcessManager.cs (Unity C#, UnityEngine.Rendering.PostProcessing)
## for Project QUAHOG / Godot 4

extends Node

# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------

## In-game districts, each carrying its own colour-grade profile.
enum District {
	NEW_SEFTON,
	DIGHTON,
	TAUNTON_HILL,
	SAWYER
}

## Weather state that drives atmospheric overrides.
## Must match WeatherController.WeatherState ordinal values.
enum WeatherState {
	CLEAR,
	DENSE_FOG,
	COASTAL_RAIN,
	NOREASTER
}

# ---------------------------------------------------------------------------
# Exports — Environment reference
# ---------------------------------------------------------------------------

## The Environment resource from the scene's WorldEnvironment node.
## Assign this in the inspector of whatever scene owns the WorldEnvironment,
## or set it at runtime via set_environment().
@export var environment: Environment

# ---------------------------------------------------------------------------
# Exports — Transition
# ---------------------------------------------------------------------------

## Seconds to blend between district or weather profiles.
@export var blend_duration: float = 1.5

# ---------------------------------------------------------------------------
# Exports — Glow (bloom)
# ---------------------------------------------------------------------------

## Base glow intensity used during Clear weather at noon.
@export var glow_base_strength: float = 0.8
## Glow intensity multiplier during DenseFog.
@export var glow_fog_multiplier: float = 0.5
## Glow intensity multiplier during CoastalRain.
@export var glow_rain_multiplier: float = 0.7
## Glow intensity multiplier during Noreaster.
@export var glow_noreaster_multiplier: float = 0.3
## Glow intensity multiplier at night (hour 20–06).
@export var glow_night_multiplier: float = 1.8

# ---------------------------------------------------------------------------
# Exports — Depth of Field
# ---------------------------------------------------------------------------

## Default focal distance in metres.
@export var dof_focal_distance: float = 10.0
## Default lens f-stop (transition): lower = shallower DoF.
@export var dof_transition: float = 1.0

# ---------------------------------------------------------------------------
# Exports — Colour Grading (per-district brightness / saturation adjustments)
# ---------------------------------------------------------------------------

## New Sefton — harbour, cool blue-white neon.
@export var new_sefton_brightness: float = 1.0
@export var new_sefton_saturation: float = 1.1
@export var new_sefton_contrast:   float = 1.05

## Dighton — warm amber/copper strip.
@export var dighton_brightness: float = 1.05
@export var dighton_saturation: float = 1.2
@export var dighton_contrast:   float = 1.1

## Taunton Hill — cold cyan industrial glow.
@export var taunton_brightness: float = 0.95
@export var taunton_saturation: float = 0.9
@export var taunton_contrast:   float = 1.15

## Sawyer — deep violet underground neon.
@export var sawyer_brightness: float = 0.85
@export var sawyer_saturation: float = 1.25
@export var sawyer_contrast:   float = 1.2

# ---------------------------------------------------------------------------
# Exports — Vignette (approximated via canvas modulate on a full-screen dark overlay,
#           or via Environment sky_horizon_color darkening — we use fog_light_color)
# ---------------------------------------------------------------------------

## Base vignette strength (0.0–1.0). Applied as a fog sky contribution.
@export var vignette_base: float = 0.0
## Vignette strength during combat tension.
@export var vignette_combat: float = 0.18

# ---------------------------------------------------------------------------
# Signals
# ---------------------------------------------------------------------------
signal district_changed(district: int)
signal weather_changed(weather: int)

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
var _current_district: int = District.NEW_SEFTON
var _current_weather:  int = WeatherState.CLEAR
var _active_tween: Tween   = null

# Snapshot values for colour-grade blending.
var _from_brightness: float = 1.0
var _from_saturation: float = 1.0
var _from_contrast:   float = 1.0

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------
func _ready() -> void:
	if environment == null:
		push_warning("PostProcessManager: No Environment resource assigned. " +
			"Call set_environment() or assign via inspector.")
		return
	_apply_district(_current_district, true)
	_apply_weather(_current_weather)

# ---------------------------------------------------------------------------
# Public API — Environment
# ---------------------------------------------------------------------------

## Assign the Environment resource at runtime (e.g. after a scene load).
func set_environment(env: Environment) -> void:
	environment = env
	if environment != null:
		_apply_district(_current_district, true)
		_apply_weather(_current_weather)

# ---------------------------------------------------------------------------
# Public API — District
# ---------------------------------------------------------------------------

## Transition to a new district colour grade, blending over blend_duration seconds.
func set_district(district: int) -> void:
	if district == _current_district:
		return
	_snapshot_current_grade()
	_current_district = district
	_start_district_blend()
	district_changed.emit(district)

## Returns the currently active district enum value.
func get_district() -> int:
	return _current_district

## Saturation mood from an OSM highway tag.
## Does not select a parody district and does not invent a municipality.
func apply_osm_highway_grade(highway: String) -> void:
	if environment == null:
		return
	environment.adjustment_enabled = true
	var saturation := 1.0
	match highway:
		"motorway", "trunk", "motorway_link", "trunk_link":
			saturation = 1.15
		"primary", "primary_link", "secondary", "secondary_link", "tertiary", "tertiary_link":
			saturation = 1.08
		"residential", "living_street", "unclassified":
			saturation = 1.05
		"service":
			saturation = 0.95
		"footway", "path", "steps", "cycleway", "track", "pedestrian":
			saturation = 0.90
		_:
			saturation = 1.0
	environment.adjustment_saturation = saturation

## Returns the display name of a district enum value.
func get_district_name(district: int) -> String:
	match district:
		District.NEW_SEFTON:   return "New Sefton"
		District.DIGHTON:      return "Dighton"
		District.TAUNTON_HILL: return "Taunton Hill"
		District.SAWYER:       return "Sawyer"
	return "Unknown"

# ---------------------------------------------------------------------------
# Public API — Weather
# ---------------------------------------------------------------------------

## Apply weather-driven glow override immediately.
func set_weather(weather: int) -> void:
	_current_weather = weather
	_apply_weather(weather)
	weather_changed.emit(weather)

## Returns the currently active weather state enum value.
func get_weather() -> int:
	return _current_weather

# ---------------------------------------------------------------------------
# Public API — Time of Day
# ---------------------------------------------------------------------------

## Called by TimeOfDayClock's hour_changed signal.
## Adjusts glow strength for the day / night cycle.
func on_hour_changed(hour: int) -> void:
	if environment == null:
		return
	var is_night: bool = hour >= 20 or hour < 6
	var weather_mult: float = _weather_glow_multiplier(_current_weather)
	var target: float = glow_base_strength * weather_mult * (glow_night_multiplier if is_night else 1.0)
	environment.glow_strength = target

# ---------------------------------------------------------------------------
# Public API — Depth of Field
# ---------------------------------------------------------------------------

## Enable or disable depth-of-field (e.g. during an aim or cutscene).
## Pass focal_distance <= 0 to use the exported default.
func set_depth_of_field(enabled: bool, focal_distance: float = -1.0) -> void:
	if environment == null:
		return
	environment.dof_blur_far_enabled  = enabled
	environment.dof_blur_near_enabled = enabled
	if enabled:
		var fd: float = focal_distance if focal_distance > 0.0 else dof_focal_distance
		environment.dof_blur_far_distance  = fd
		environment.dof_blur_near_distance = fd * 0.5
		environment.dof_blur_far_transition  = dof_transition
		environment.dof_blur_near_transition = dof_transition

# ---------------------------------------------------------------------------
# Public API — Vignette / Combat
# ---------------------------------------------------------------------------

## Raise vignette to signal combat tension, or restore base level.
func set_combat_vignette(active: bool) -> void:
	if environment == null:
		return
	# Godot has no dedicated vignette parameter; we approximate it via
	# ambient_light_energy dimming on top of the existing colour grade.
	var target: float = vignette_combat if active else vignette_base
	# Blend ambient down to simulate edge darkening.
	var tween: Tween = create_tween()
	tween.set_ease(Tween.EASE_IN_OUT)
	tween.set_trans(Tween.TRANS_SINE)
	tween.tween_property(environment, "ambient_light_energy", 1.0 - target, 0.3)

# ---------------------------------------------------------------------------
# Internal — district application
# ---------------------------------------------------------------------------

func _apply_district(district: int, instant: bool = false) -> void:
	if environment == null:
		return
	var grade: Dictionary = _get_district_grade(district)
	if instant:
		environment.adjustment_brightness = grade.brightness
		environment.adjustment_saturation = grade.saturation
		environment.adjustment_contrast   = grade.contrast
	else:
		_blend_grade(grade.brightness, grade.saturation, grade.contrast)

func _start_district_blend() -> void:
	if environment == null:
		return
	var grade: Dictionary = _get_district_grade(_current_district)
	_blend_grade(grade.brightness, grade.saturation, grade.contrast)

func _blend_grade(to_b: float, to_s: float, to_c: float) -> void:
	if environment == null:
		return
	if _active_tween != null and _active_tween.is_valid():
		_active_tween.kill()

	_active_tween = create_tween()
	_active_tween.set_ease(Tween.EASE_IN_OUT)
	_active_tween.set_trans(Tween.TRANS_SINE)
	_active_tween.set_parallel(true)
	_active_tween.tween_property(environment, "adjustment_brightness", to_b, blend_duration)
	_active_tween.tween_property(environment, "adjustment_saturation", to_s, blend_duration)
	_active_tween.tween_property(environment, "adjustment_contrast",   to_c, blend_duration)

func _snapshot_current_grade() -> void:
	if environment != null:
		_from_brightness = environment.adjustment_brightness
		_from_saturation = environment.adjustment_saturation
		_from_contrast   = environment.adjustment_contrast
	else:
		var grade: Dictionary = _get_district_grade(_current_district)
		_from_brightness = grade.brightness
		_from_saturation = grade.saturation
		_from_contrast   = grade.contrast

## Returns a Dictionary with keys "brightness", "saturation", "contrast"
## for the given district enum value.
func _get_district_grade(district: int) -> Dictionary:
	match district:
		District.DIGHTON:
			return { "brightness": dighton_brightness, "saturation": dighton_saturation, "contrast": dighton_contrast }
		District.TAUNTON_HILL:
			return { "brightness": taunton_brightness, "saturation": taunton_saturation, "contrast": taunton_contrast }
		District.SAWYER:
			return { "brightness": sawyer_brightness,  "saturation": sawyer_saturation,  "contrast": sawyer_contrast  }
		_: # NEW_SEFTON default
			return { "brightness": new_sefton_brightness, "saturation": new_sefton_saturation, "contrast": new_sefton_contrast }

# ---------------------------------------------------------------------------
# Internal — weather glow
# ---------------------------------------------------------------------------

func _apply_weather(weather: int) -> void:
	if environment == null:
		return
	var target_strength: float = glow_base_strength * _weather_glow_multiplier(weather)
	environment.glow_enabled  = true
	environment.glow_strength  = target_strength

func _weather_glow_multiplier(weather: int) -> float:
	match weather:
		WeatherState.DENSE_FOG:    return glow_fog_multiplier
		WeatherState.COASTAL_RAIN: return glow_rain_multiplier
		WeatherState.NOREASTER:    return glow_noreaster_multiplier
	return 1.0
