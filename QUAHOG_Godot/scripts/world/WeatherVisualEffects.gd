## WeatherVisualEffects.gd
## res://scripts/world/WeatherVisualEffects.gd
## Drives weather particle systems, lightning flashes, and wet-surface
## shader parameters in response to WeatherController state changes.
## Attach to a Node that lives inside the active level scene.
## Converted from WeatherVisualEffects.cs (Unity C#) for Project QUAHOG / Godot 4

extends Node3D

# ---------------------------------------------------------------------------
# Exports — Scene references
# ---------------------------------------------------------------------------

## GPU particle system for rain.
@export var rain_particles: GPUParticles3D

## GPU particle system for snow/blizzard.
@export var snow_particles: GPUParticles3D

## Omni light used for lightning flash effect.
@export var lightning_light: OmniLight3D

## ShaderMaterial applied to wet road/pavement surfaces.
## Must expose a "wetness" float shader parameter.
@export var wet_road_material: ShaderMaterial

# ---------------------------------------------------------------------------
# Exports — Tuning
# ---------------------------------------------------------------------------

## Duration of the lightning flash peak (seconds).
@export var flash_duration: float = 0.15

## Peak energy of the lightning OmniLight3D during a flash.
@export var flash_intensity: float = 2.5

## Duration for the light to fade back to zero after the flash peak (seconds).
@export var fade_duration: float = 0.3

# ---------------------------------------------------------------------------
# Private state
# ---------------------------------------------------------------------------
var _lightning_tween: Tween = null

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------
func _ready() -> void:
	# Ensure lightning light starts off.
	if lightning_light != null:
		lightning_light.light_energy = 0.0
		lightning_light.visible = false

	# Particles off by default.
	if rain_particles != null:
		rain_particles.emitting = false
	if snow_particles != null:
		snow_particles.emitting = false

# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

## Set rain particle emission intensity.
## intensity: 0.0 = off, 1.0 = full export amount override.
func set_rain_intensity(intensity: float) -> void:
	if rain_particles == null:
		push_warning("WeatherVisualEffects: rain_particles is not assigned.")
		return
	var clamped: float = clampf(intensity, 0.0, 1.0)
	if clamped <= 0.0:
		rain_particles.emitting = false
	else:
		rain_particles.emitting = true
		# Scale the amount relative to the exported amount value.
		rain_particles.amount_ratio = clamped

## Set snow particle emission intensity (mirrors set_rain_intensity for snow).
func set_intensity(intensity: float) -> void:
	if snow_particles == null:
		push_warning("WeatherVisualEffects: snow_particles is not assigned.")
		return
	var clamped: float = clampf(intensity, 0.0, 1.0)
	if clamped <= 0.0:
		snow_particles.emitting = false
	else:
		snow_particles.emitting = true
		snow_particles.amount_ratio = clamped

## Trigger a lightning flash: hard cut to peak energy, then fade out.
## Safe to call while a previous flash is still in progress — the old
## tween is killed and a new one starts immediately.
func trigger_lightning() -> void:
	if lightning_light == null:
		push_warning("WeatherVisualEffects: lightning_light is not assigned.")
		return

	if _lightning_tween != null and _lightning_tween.is_valid():
		_lightning_tween.kill()

	_lightning_flash_async()

## Apply a WeatherController state name to the particle nodes.
## Snow uses the existing set_intensity() path. Missing nodes are skipped
## so a half-built scene does not spam warnings every weather tick.
func apply_state_name(state: String) -> void:
	var rain := 0.0
	var snow := 0.0
	match state:
		"COASTAL_RAIN":
			rain = 0.7
		"NOREASTER":
			rain = 1.0
			snow = 0.85
		_:
			pass
	if rain_particles != null:
		set_rain_intensity(rain)
	if snow_particles != null:
		set_intensity(snow)
	if wet_road_material != null:
		set_wet_surfaces(rain > 0.0 or snow > 0.0)

## Apply or remove wet-surface wetness via shader parameter.
## wet=true  → wetness 0.85
## wet=false → wetness 0.0
func set_wet_surfaces(wet: bool) -> void:
	if wet_road_material == null:
		push_warning("WeatherVisualEffects: wet_road_material is not assigned.")
		return
	var target_wetness: float = 0.85 if wet else 0.0
	wet_road_material.set_shader_parameter("wetness", target_wetness)

# ---------------------------------------------------------------------------
# Internal async helpers
# ---------------------------------------------------------------------------

## Async lightning flash sequence:
##   1. Snap light on at peak intensity.
##   2. Hold for flash_duration.
##   3. Tween energy to 0 over fade_duration.
func _lightning_flash_async() -> void:
	lightning_light.light_energy = flash_intensity
	lightning_light.visible = true

	# Hold the flash.
	await get_tree().create_timer(flash_duration).timeout

	# Fade out.
	_lightning_tween = create_tween()
	_lightning_tween.set_ease(Tween.EASE_IN)
	_lightning_tween.set_trans(Tween.TRANS_EXPO)
	_lightning_tween.tween_property(
		lightning_light,
		"light_energy",
		0.0,
		fade_duration
	)
	await _lightning_tween.finished

	lightning_light.visible = false
	_lightning_tween = null
