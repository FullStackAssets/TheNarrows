## StreetContact.gd
## Interactable placed on a real road-graph polyline (see StreetLoop).
## The mesh is a gameplay marker, not an OSM feature.
## Project QUAHOG / Godot 4

extends StaticBody3D

signal contacted(player)

var street_name: String = ""

func interact(player) -> void:
	contacted.emit(player)
