## FogController.gd
## res://scripts/world/FogController.gd
## Manages fog density on a Godot Environment resource with Tween-based smooth
## interpolation. Attach to a Node inside the WorldEnvironment scene.
## Converted from FogController.cs (Unity C#) for Project QUAHOG / Godot 4

extends Node

# ---------------------------------------------------------------------------
# Exports
# ---------------------------------------------------------------------------

## The Environment resource belonging to the scene's WorldEnvironment node.
@export var environment: Environment

## Fog density preset — no weather (blue sky).
@export var clear_density: float = 0.0

## Fog density preset — thick harbour fog.
@export var dense_fog_density: float = 0.08

## Fog density preset — coastal rain.
@export var rain_fog_density: float = 0.03

## Fog density preset — nor'easter blizzard.
@export var noreaster_fog_density: float = 0.12

# ---------------------------------------------------------------------------
# Private state
# ---------------------------------------------------------------------------
var _active_tween: Tween = null

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------
func _ready() -> void:
	if environment == null:
		push_warning("FogController: No Environment resource assigned.")

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Smoothly animate fog density to `target` over `duration` seconds.
## Any in-flight tween is killed before starting the new one.
func set_target_density(target: float, duration: float) -> void:
	if environment == null:
		push_warning("FogController: Cannot set density — Environment is null.")
		return

	if _active_tween != null and _active_tween.is_valid():
		_active_tween.kill()

	_active_tween = create_tween()
	_active_tween.set_ease(Tween.EASE_IN_OUT)
	_active_tween.set_trans(Tween.TRANS_SINE)

	# Tween both the volumetric and legacy density properties for compatibility.
	_active_tween.tween_property(
		environment,
		"volumetric_fog_density",
		target,
		duration
	)
	# Parallel: also animate the non-volumetric fog_density
	_active_tween.parallel().tween_property(
		environment,
		"fog_density",
		target,
		duration
	)

	# Enable/disable fog based on whether target density is non-zero.
	if target > 0.0:
		environment.fog_enabled = true
	else:
		# Disable fog once the tween completes.
		_active_tween.tween_callback(func() -> void:
			environment.fog_enabled = false
		)

## Tween (or snap) to a WeatherController state name.
## Densities are this node's exported presets, which match WeatherController.
func apply_named_state(state_name: String, duration: float = 1.5) -> void:
	if duration <= 0.0:
		snap_to_preset(state_name)
		return
	set_target_density(_density_for_preset(state_name), duration)

## Convenience: snap to a named preset density without animation.
func snap_to_preset(preset_name: String) -> void:
	var density: float = _density_for_preset(preset_name)
	if _active_tween != null and _active_tween.is_valid():
		_active_tween.kill()
	environment.fog_density = density
	environment.volumetric_fog_density = density
	environment.fog_enabled = density > 0.0

## Returns current fog density from the environment.
func get_current_density() -> float:
	if environment == null:
		return 0.0
	return environment.fog_density

# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------
func _density_for_preset(preset_name: String) -> float:
	match preset_name.to_upper():
		"CLEAR":        return clear_density
		"DENSE_FOG":    return dense_fog_density
		"COASTAL_RAIN", "RAIN": return rain_fog_density
		"NOREASTER":    return noreaster_fog_density
	push_warning("FogController: Unknown preset '%s' — using 0.0." % preset_name)
	return 0.0
