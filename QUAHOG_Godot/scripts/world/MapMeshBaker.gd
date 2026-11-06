## MapMeshBaker.gd
## res://scripts/world/MapMeshBaker.gd
## Bakes the real-world South Coast street grid (New Bedford + Fall River) into
## an ArrayMesh at runtime from the OpenStreetMap-derived road graph.
## Reads res://data/southcoast-roads.json (origin-relative meters, x=east /
## z=north / y=up) and extrudes each road centerline into a flat ribbon whose
## width is the full width stored on the feature (not re-derived).
## Highway class only picks the neon color.
## Map data © OpenStreetMap contributors, ODbL.
## Project QUAHOG / Godot 4

@tool
extends Node3D

# ---------------------------------------------------------------------------
# Exports
# ---------------------------------------------------------------------------

## Road graph produced by quahog-project-files/mapdata/bake_meshes.py.
@export_file("*.json") var road_graph_path: String = "res://data/southcoast-roads.json"

## Material applied to the baked road mesh. If null, a neon-tinted default is used.
@export var road_material: Material

## Build the mesh as soon as the node enters the tree.
@export var bake_on_ready: bool = true

## Re-bake now (toggle in the inspector while running with @tool).
@export var bake_now: bool = false:
	set(value):
		if value:
			bake()

## Parsed road-graph or slice document. When set, bake() does not read the file.
var graph_override: Dictionary = {}

const Contract := preload("res://scripts/world/OsmMapContract.gd")
const Neon := preload("res://scripts/world/HighwayNeon.gd")

# ---------------------------------------------------------------------------
# Internals
# ---------------------------------------------------------------------------

var _mesh_instance: MeshInstance3D
var _baked_material: StandardMaterial3D
var _logged_bridge := false

func _ready() -> void:
	if bake_on_ready:
		bake()

## Loads the road graph and (re)builds the road ribbon mesh.
func bake() -> void:
	var data := graph_override if not graph_override.is_empty() else _load_graph()
	if data.is_empty():
		return
	Contract.warn_loaded(data)
	var frame := Contract.classify(data)
	if frame == "unknown":
		push_error("[MapMeshBaker] unrecognized map document at %s" % road_graph_path)
		return

	var st := SurfaceTool.new()
	st.begin(Mesh.PrimitiveType.PRIMITIVE_TRIANGLES)

	var roads: Array = data.get("roads", [])
	for road in roads:
		if typeof(road) == TYPE_DICTIONARY:
			_add_ribbon(st, road, frame)

	st.generate_normals()
	var mesh := st.commit()

	if _mesh_instance == null:
		_mesh_instance = MeshInstance3D.new()
		_mesh_instance.name = "BakedRoads"
		add_child(_mesh_instance)
		if Engine.is_editor_hint() and get_tree() != null:
			_mesh_instance.owner = get_tree().edited_scene_root
	_mesh_instance.mesh = mesh
	if road_material != null:
		_mesh_instance.material_override = road_material
		_baked_material = road_material as StandardMaterial3D
	else:
		_baked_material = _default_material()
		_mesh_instance.material_override = _baked_material

	var verts := 0
	if mesh.get_surface_count() > 0:
		verts = mesh.surface_get_array_len(0)
	print("[MapMeshBaker] baked %d roads (%d vertices) frame=%s" % [roads.size(), verts, frame])

## Highway neon strength. Weather and time of day drive this from StreetLoop.
func set_neon_energy(energy: float) -> void:
	if _baked_material != null:
		_baked_material.emission_enabled = true
		_baked_material.emission_energy_multiplier = energy

## Wet asphalt read for rain and nor'easter. Not a physical water layer.
func set_road_wet(wet: bool) -> void:
	if _baked_material != null:
		_baked_material.roughness = 0.28 if wet else 0.72

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

func _load_graph() -> Dictionary:
	if not FileAccess.file_exists(road_graph_path):
		push_error("[MapMeshBaker] road graph not found: %s" % road_graph_path)
		return {}
	var text := FileAccess.get_file_as_string(road_graph_path)
	var parsed: Variant = JSON.parse_string(text)
	if typeof(parsed) != TYPE_DICTIONARY:
		push_error("[MapMeshBaker] could not parse road graph JSON")
		return {}
	return parsed

## Extrudes one road's centerline into per-segment quads (two triangles each).
## Width is the stored full width. Points follow the document frame:
## road graph z = north, slice z = -north. Bridge deck height is not in
## either contract, so bridge roads stay on y = 0.
func _add_ribbon(st: SurfaceTool, road: Dictionary, frame: String) -> void:
	var pts: Array = road.get("points", [])
	if pts.size() < 2:
		return
	if frame == "slice" and bool(road.get("bridge", false)) and not _logged_bridge:
		_logged_bridge = true
		push_warning("[MapMeshBaker] slice bridge/layer has no deck height in the shared contract; ribbons stay at y=0.")
	var half: float = Contract.full_width(road) * 0.5
	var color := Neon.highway_color(Contract.osm_text(road.get("highway")))

	for i in range(pts.size() - 1):
		if typeof(pts[i]) != TYPE_ARRAY or typeof(pts[i + 1]) != TYPE_ARRAY:
			continue
		if pts[i].size() < 2 or pts[i + 1].size() < 2:
			continue
		var a := _frame_point(pts[i], frame)
		var b := _frame_point(pts[i + 1], frame)
		var dir := b - a
		var length := dir.length()
		if length < 0.001:
			continue
		dir /= length
		var n := Vector3(-dir.z, 0.0, dir.x) * half
		var p1 := a + n
		var p2 := b + n
		var p3 := b - n
		var p4 := a - n
		_tri(st, p1, p2, p3, color)
		_tri(st, p1, p3, p4, color)

func _frame_point(point: Array, frame: String) -> Vector3:
	if frame == "slice":
		return Contract.slice_point_to_godot(float(point[0]), float(point[1]), 0.0)
	return Contract.road_graph_point_to_godot(float(point[0]), float(point[1]), 0.0)

func _tri(st: SurfaceTool, x: Vector3, y: Vector3, z: Vector3, color: Color) -> void:
	st.set_color(color)
	st.add_vertex(x)
	st.set_color(color)
	st.add_vertex(y)
	st.set_color(color)
	st.add_vertex(z)

func _default_material() -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(1, 1, 1)
	m.vertex_color_use_as_albedo = true
	m.emission_enabled = true
	m.emission = Color(0.85, 0.88, 0.95)
	m.emission_energy_multiplier = 0.35
	m.roughness = 0.72
	return m
