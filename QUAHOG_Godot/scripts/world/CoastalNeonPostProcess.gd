## CoastalNeonPostProcess.gd
## res://scripts/world/CoastalNeonPostProcess.gd
## Applies per-district neon color grading by tweening the Environment's
## ambient_light_color.  Attach to a Node inside your post-process scene.
## Converted from CoastalNeonPostProcess.cs (Unity C#) for Project QUAHOG / Godot 4

extends Node

const HighwayNeonScript := preload("res://scripts/world/HighwayNeon.gd")

# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------
enum District {
	NEW_SEFTON,
	DIGHTON,
	TAUNTON_HILL,
	SAWYER
}

# ---------------------------------------------------------------------------
# Exports — Environment reference
# ---------------------------------------------------------------------------

## The Environment resource from the scene's WorldEnvironment node.
@export var environment: Environment

# ---------------------------------------------------------------------------
# Exports — Per-district ambient colors
# ---------------------------------------------------------------------------

## New Sefton — cool harbour blue-white neon.
@export var new_sefton_color: Color = Color(0.35, 0.45, 0.85, 1.0)

## Dighton — warm amber/copper neon strip.
@export var dighton_color: Color = Color(0.85, 0.50, 0.20, 1.0)

## Taunton Hill — cold cyan industrial glow.
@export var taunton_hill_color: Color = Color(0.20, 0.75, 0.80, 1.0)

## Sawyer — deep violet underground neon.
@export var sawyer_color: Color = Color(0.55, 0.20, 0.80, 1.0)

# ---------------------------------------------------------------------------
# Exports — Grade parameters
# ---------------------------------------------------------------------------

## Master intensity multiplier applied to all district colors.
@export var grade_intensity: float = 0.45

## Seconds for the ambient color to blend from one district to the next.
@export var blend_duration: float = 1.5

## When false, _ready does not snap the parody district grade. The street
## loop drives CLEAR-weather tint from the OSM highway tag instead.
@export var apply_initial_grade: bool = true

# ---------------------------------------------------------------------------
# Signals
# ---------------------------------------------------------------------------
signal district_grade_changed(grade: int)

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
var current_grade: int = District.NEW_SEFTON
var _active_tween: Tween = null

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------
func _ready() -> void:
	if environment == null:
		push_warning("CoastalNeonPostProcess: No Environment resource assigned.")
		return
	if apply_initial_grade:
		environment.ambient_light_color = _color_for_grade(current_grade)

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Transition the color grade to the given district enum value.
func set_district_grade(grade: int) -> void:
	if environment == null:
		push_warning("CoastalNeonPostProcess: Cannot set grade — Environment is null.")
		return

	if grade == current_grade:
		return

	current_grade = grade
	var target_color: Color = _color_for_grade(grade)

	if _active_tween != null and _active_tween.is_valid():
		_active_tween.kill()

	_active_tween = create_tween()
	_active_tween.set_ease(Tween.EASE_IN_OUT)
	_active_tween.set_trans(Tween.TRANS_SINE)
	_active_tween.tween_property(
		environment,
		"ambient_light_color",
		target_color,
		blend_duration
	)

	district_grade_changed.emit(grade)

var _tint_tween: Tween = null

## While the weather ambient is CLEAR, blend a little highway neon into it.
## Rain, fog, and nor'easter keep the WeatherController ambient. This does
## not switch the parody district enum (New Sefton / Dighton / …).
func apply_clear_highway_ambient(highway: String) -> void:
	if environment == null:
		return
	var target: Color = HighwayNeonScript.clear_ambient(highway)
	if _tint_tween != null and _tint_tween.is_valid():
		_tint_tween.kill()
	_tint_tween = create_tween()
	_tint_tween.set_ease(Tween.EASE_IN_OUT)
	_tint_tween.set_trans(Tween.TRANS_SINE)
	_tint_tween.tween_property(environment, "ambient_light_color", target, blend_duration)

## Stop a highway tint tween so a weather snap can own ambient_light_color.
func cancel_tint() -> void:
	if _tint_tween != null and _tint_tween.is_valid():
		_tint_tween.kill()
	_tint_tween = null

## Returns the name string for a given District enum value.
func get_district_name(grade: int) -> String:
	match grade:
		District.NEW_SEFTON:   return "New Sefton"
		District.DIGHTON:      return "Dighton"
		District.TAUNTON_HILL: return "Taunton Hill"
		District.SAWYER:       return "Sawyer"
	return "Unknown"

# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------
func _color_for_grade(grade: int) -> Color:
	var base_color: Color
	match grade:
		District.NEW_SEFTON:
			base_color = new_sefton_color
		District.DIGHTON:
			base_color = dighton_color
		District.TAUNTON_HILL:
			base_color = taunton_hill_color
		District.SAWYER:
			base_color = sawyer_color
		_:
			push_warning("CoastalNeonPostProcess: Unknown grade %d." % grade)
			base_color = new_sefton_color

	# Apply grade_intensity as a linear scale on the RGB channels.
	return Color(
		base_color.r * grade_intensity,
		base_color.g * grade_intensity,
		base_color.b * grade_intensity,
		base_color.a
	)
