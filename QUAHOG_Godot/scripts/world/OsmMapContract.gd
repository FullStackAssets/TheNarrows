## OsmMapContract.gd
## Runtime adapter for the shared OSM road graph and the web slice.
## Pure data helpers — no Overpass calls, no invented municipalities.
## Keep the geometry in sync with QUAHOG_Godot/tools/osm_contract.py.
## Mismatches are written up in res://OSM_CONTRACT.md.
## Project QUAHOG / Godot 4

extends RefCounted
class_name OsmMapContract

const CELL_M := 64.0
const TILE_M := 500.0
const CONTACT_DISTANCE_M := 40.0
const PROP_LATERAL_M := 1.5
const OFFROAD_M := 80.0

static func load_file(path: String) -> Dictionary:
	if not FileAccess.file_exists(path):
		push_error("[OsmMapContract] missing %s" % path)
		return {}
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if typeof(parsed) != TYPE_DICTIONARY:
		push_error("[OsmMapContract] not a JSON object: %s" % path)
		return {}
	return parsed

static func classify(doc: Dictionary) -> String:
	if doc.has("roads") and doc.has("road_count") and doc.has("axes") and not doc.has("buildings"):
		return "road_graph"
	if doc.has("roads") and doc.has("buildings") and doc.has("landmarks"):
		return "slice"
	return "unknown"

static func osm_text(value) -> String:
	if typeof(value) != TYPE_STRING:
		return ""
	return value.strip_edges()

static func full_width(road: Dictionary, fallback: float = 6.0) -> float:
	var width = road.get("width", null)
	if (typeof(width) == TYPE_FLOAT or typeof(width) == TYPE_INT) and float(width) > 0.0:
		return float(width)
	return fallback

static func road_graph_point_to_godot(east: float, north: float, y: float = 0.0) -> Vector3:
	return Vector3(east, y, north)

static func slice_point_to_godot(east: float, north: float, y: float = 0.0) -> Vector3:
	return Vector3(east, y, -north)

static func slice_to_wgs84(east: float, north: float, origin: Dictionary, meters_per_degree: Dictionary) -> Vector2:
	var lat := float(origin["lat"]) + north / float(meters_per_degree["lat"])
	var lon := float(origin["lon"]) + east / float(meters_per_degree["lon"])
	return Vector2(lat, lon)

static func wgs84_to_road_graph_xz(lat: float, lon: float, origin: Dictionary, meters_per_degree: Dictionary) -> Vector2:
	var x := (lon - float(origin["lon"])) * float(meters_per_degree["lon"])
	var z := (lat - float(origin["lat"])) * float(meters_per_degree["lat"])
	return Vector2(x, z)

static func building_tile_key(east: float, north: float, tile_m: float = TILE_M) -> String:
	# math.floor, not int() truncation. Negative tile keys depend on this.
	var gx := int(floor(east / tile_m))
	var gn := int(floor(north / tile_m))
	return "%d_%d" % [gx, gn]

static func banner_for_road(road: Dictionary) -> String:
	var parts: PackedStringArray = PackedStringArray()
	var city := osm_text(road.get("city"))
	var street := osm_text(road.get("name"))
	var highway := osm_text(road.get("highway"))
	if city != "":
		parts.append(city)
	if street != "":
		parts.append(street)
	elif highway != "":
		parts.append(highway)
	return " · ".join(parts)

static func pick_spawn_road(roads: Array) -> int:
	var fallback := -1
	for i in roads.size():
		var road = roads[i]
		if typeof(road) != TYPE_DICTIONARY:
			continue
		var pts: Array = road.get("points", [])
		if pts.size() < 2:
			continue
		if fallback < 0:
			fallback = i
		if osm_text(road.get("name")) != "":
			return i
	return fallback

static func point_along(points: Array, distance: float, frame: String = "road_graph") -> Vector3:
	if points.size() < 2:
		return Vector3.INF
	var prev := _point_to_godot(points[0], frame)
	var walked := 0.0
	for i in range(1, points.size()):
		var cur := _point_to_godot(points[i], frame)
		var step := Vector2(cur.x - prev.x, cur.z - prev.z).length()
		if step < 0.00000001:
			prev = cur
			continue
		if walked + step >= distance:
			var t := (distance - walked) / step
			return prev.lerp(cur, t)
		walked += step
		prev = cur
	return _point_to_godot(points[points.size() - 1], frame)

static func tangent_xz(points: Array, distance: float, frame: String = "road_graph") -> Vector2:
	if points.size() < 2:
		return Vector2(1.0, 0.0)
	var prev := _point_to_godot(points[0], frame)
	var walked := 0.0
	var last := Vector2(1.0, 0.0)
	for i in range(1, points.size()):
		var cur := _point_to_godot(points[i], frame)
		var delta := Vector2(cur.x - prev.x, cur.z - prev.z)
		var step := delta.length()
		if step < 0.00000001:
			prev = cur
			continue
		last = delta / step
		if walked + step >= distance:
			return last
		walked += step
		prev = cur
	return last

static func contact_pose(points: Array, distance: float = CONTACT_DISTANCE_M) -> Dictionary:
	var pos := point_along(points, distance, "road_graph")
	if pos == Vector3.INF:
		return {}
	var tangent := tangent_xz(points, distance, "road_graph")
	var lateral := Vector2(-tangent.y, tangent.x) * PROP_LATERAL_M
	return {
		"position": Vector3(pos.x + lateral.x, 0.0, pos.z + lateral.y),
		"on_road": pos,
	}

static func bounds_center(roads: Array) -> Vector2:
	var min_x := INF
	var min_z := INF
	var max_x := -INF
	var max_z := -INF
	var count := 0
	for road in roads:
		if typeof(road) != TYPE_DICTIONARY:
			continue
		for point in road.get("points", []):
			if typeof(point) != TYPE_ARRAY or point.size() < 2:
				continue
			var x := float(point[0])
			var z := float(point[1])
			min_x = minf(min_x, x)
			max_x = maxf(max_x, x)
			min_z = minf(min_z, z)
			max_z = maxf(max_z, z)
			count += 1
	if count == 0:
		return Vector2.ZERO
	return Vector2((min_x + max_x) * 0.5, (min_z + max_z) * 0.5)

static func build_cell_index(roads: Array, cell_m: float = CELL_M) -> Dictionary:
	var cells := {}
	for i in roads.size():
		var road = roads[i]
		if typeof(road) != TYPE_DICTIONARY:
			continue
		var seen := {}
		for sample in _sample_polyline(road.get("points", []), cell_m):
			var key: String = sample
			if seen.has(key):
				continue
			seen[key] = true
			if not cells.has(key):
				cells[key] = []
			(cells[key] as Array).append(i)
	return cells

static func nearest_road(roads: Array, cells: Dictionary, x: float, z: float, max_dist: float = OFFROAD_M, cell_m: float = CELL_M) -> Dictionary:
	var cx := int(floor(x / cell_m))
	var cz := int(floor(z / cell_m))
	var best_i := -1
	var best_d := max_dist
	for ix in range(cx - 1, cx + 2):
		for iz in range(cz - 1, cz + 2):
			var key := "%d,%d" % [ix, iz]
			if not cells.has(key):
				continue
			for i in cells[key]:
				var dist := road_distance(roads[i], x, z)
				if dist < best_d:
					best_d = dist
					best_i = i
	if best_i < 0:
		return {}
	return {"index": best_i, "distance": best_d, "road": roads[best_i]}

static func road_distance(road: Dictionary, x: float, z: float) -> float:
	var pts: Array = road.get("points", [])
	if pts.size() == 1 and typeof(pts[0]) == TYPE_ARRAY and pts[0].size() >= 2:
		return Vector2(x - float(pts[0][0]), z - float(pts[0][1])).length()
	var best := INF
	for i in range(pts.size() - 1):
		if typeof(pts[i]) != TYPE_ARRAY or typeof(pts[i + 1]) != TYPE_ARRAY:
			continue
		if pts[i].size() < 2 or pts[i + 1].size() < 2:
			continue
		best = minf(best, _segment_distance(
			x, z,
			float(pts[i][0]), float(pts[i][1]),
			float(pts[i + 1][0]), float(pts[i + 1][1])
		))
	return best

static func warn_loaded(doc: Dictionary) -> void:
	var kind := classify(doc)
	if kind == "road_graph":
		var roads: Array = doc.get("roads", [])
		var named_city := 0
		for road in roads:
			if typeof(road) != TYPE_DICTIONARY:
				continue
			if osm_text(road.get("city")) != "":
				named_city += 1
		if named_city == 0:
			push_warning("[OsmMapContract] road-graph city is null or empty on every road (%d). Banners use the OSM street name and highway tag. Municipality is not inferred." % roads.size())
		var axes := str(doc.get("axes", ""))
		if not axes.contains("z=north"):
			push_warning("[OsmMapContract] unexpected road-graph axes: %s" % axes)
	elif kind == "slice":
		push_warning("[OsmMapContract] slice 3D frame is x=east, z=-north. Convert through WGS84 before mixing with the road graph.")
		var buildings: Array = doc.get("buildings", [])
		if bool(doc.get("buildingsStreamed", false)) and buildings.is_empty():
			push_warning("[OsmMapContract] slice buildings are streamed and this array is empty. Building tiles are not in the Godot project.")
	else:
		push_warning("[OsmMapContract] unrecognized map document.")

static func _point_to_godot(point, frame: String) -> Vector3:
	if typeof(point) != TYPE_ARRAY or point.size() < 2:
		return Vector3.ZERO
	if frame == "slice":
		return slice_point_to_godot(float(point[0]), float(point[1]), 0.0)
	return road_graph_point_to_godot(float(point[0]), float(point[1]), 0.0)

static func _remember_cell(seen: Dictionary, samples: Array, east: float, north: float, spacing: float) -> void:
	var key := "%d,%d" % [int(floor(east / spacing)), int(floor(north / spacing))]
	if seen.has(key):
		return
	seen[key] = true
	samples.append(key)

static func _sample_polyline(points: Array, spacing: float) -> Array:
	var samples: Array = []
	if points.is_empty():
		return samples
	var seen := {}
	if typeof(points[0]) == TYPE_ARRAY and points[0].size() >= 2:
		_remember_cell(seen, samples, float(points[0][0]), float(points[0][1]), spacing)
	for i in range(points.size() - 1):
		if typeof(points[i]) != TYPE_ARRAY or typeof(points[i + 1]) != TYPE_ARRAY:
			continue
		if points[i].size() < 2 or points[i + 1].size() < 2:
			continue
		var x1 := float(points[i][0])
		var z1 := float(points[i][1])
		var x2 := float(points[i + 1][0])
		var z2 := float(points[i + 1][1])
		var dist := Vector2(x2 - x1, z2 - z1).length()
		if dist < 0.00000001:
			continue
		var steps := int(floor(dist / spacing))
		for s in range(1, steps + 1):
			var t := (float(s) * spacing) / dist
			if t >= 1.0:
				break
			_remember_cell(seen, samples, x1 + (x2 - x1) * t, z1 + (z2 - z1) * t, spacing)
		_remember_cell(seen, samples, x2, z2, spacing)
	return samples

static func _segment_distance(px: float, pz: float, ax: float, az: float, bx: float, bz: float) -> float:
	var abx := bx - ax
	var abz := bz - az
	var length_sq := abx * abx + abz * abz
	if length_sq < 0.00000001:
		return Vector2(px - ax, pz - az).length()
	var t := clampf(((px - ax) * abx + (pz - az) * abz) / length_sq, 0.0, 1.0)
	return Vector2(px - (ax + abx * t), pz - (az + abz * t)).length()
