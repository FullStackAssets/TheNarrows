## HighwayNeon.gd
## Neon lighting mood keyed by the OSM `highway` tag on a road feature.
## Colors are art direction. They are not municipalities and not a claim that
## a tag was measured in the field.
## Keep the tables in sync with QUAHOG_Godot/tools/osm_contract.py.
## Project QUAHOG / Godot 4

extends RefCounted
class_name HighwayNeon

const HIGHWAY_RGB := {
	"motorway": [1.00, 0.24, 0.66],
	"trunk": [1.00, 0.24, 0.66],
	"primary": [1.00, 0.45, 0.72],
	"secondary": [0.25, 0.85, 0.90],
	"tertiary": [0.35, 0.75, 0.85],
	"residential": [0.95, 0.62, 0.28],
	"unclassified": [0.90, 0.55, 0.25],
	"living_street": [0.95, 0.70, 0.40],
	"service": [0.55, 0.30, 0.85],
	"pedestrian": [0.95, 0.75, 0.45],
	"footway": [0.35, 0.45, 0.75],
	"path": [0.35, 0.45, 0.75],
	"cycleway": [0.30, 0.70, 0.55],
	"steps": [0.35, 0.45, 0.75],
	"track": [0.45, 0.40, 0.30],
}
const DEFAULT_RGB := [0.70, 0.70, 0.75]

const WEATHER_EMISSION := {
	"CLEAR": 0.35,
	"DENSE_FOG": 0.20,
	"COASTAL_RAIN": 0.55,
	"NOREASTER": 0.15,
}
const NIGHT_EMISSION_MULTIPLIER := 1.80
const HIGHWAY_AMBIENT_MIX := 0.18
const CLEAR_AMBIENT := [0.55, 0.55, 0.60]

static func highway_color(highway: String) -> Color:
	var key := highway
	if not HIGHWAY_RGB.has(key) and key.ends_with("_link"):
		key = key.trim_suffix("_link")
	var rgb: Array = HIGHWAY_RGB.get(key, DEFAULT_RGB)
	return Color(float(rgb[0]), float(rgb[1]), float(rgb[2]))

static func is_night(hour: int) -> bool:
	return hour >= 20 or hour < 6

static func emission_energy(weather_state: String, hour: int) -> float:
	var base: float = float(WEATHER_EMISSION.get(weather_state, WEATHER_EMISSION["CLEAR"]))
	if is_night(hour):
		return base * NIGHT_EMISSION_MULTIPLIER
	return base

static func clear_ambient(highway: String) -> Color:
	var neon := highway_color(highway)
	var mix := HIGHWAY_AMBIENT_MIX
	return Color(
		CLEAR_AMBIENT[0] * (1.0 - mix) + neon.r * mix,
		CLEAR_AMBIENT[1] * (1.0 - mix) + neon.g * mix,
		CLEAR_AMBIENT[2] * (1.0 - mix) + neon.b * mix,
		1.0
	)
