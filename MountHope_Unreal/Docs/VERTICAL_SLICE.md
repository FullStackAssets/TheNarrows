# Mount Hope Unreal Vertical Slice

## Creative target

Build a premium PC/console vertical slice that feels like a grounded,
photoreal, Massachusetts South Coast crime game with sharp satire and a small
thread of local Gothic unease. The tone reference is GTA-like in structure and
freedom, but the world, characters, place names, missions, and story material
must stay original.

## Map direction

Use real OSM data as the base truth, then allow controlled fictionalization:

- preserve recognizable coastline, arterial roads, neighborhoods, and civic
  landmarks where legally and creatively useful;
- compress distances when mission pacing needs it;
- rename businesses, interiors, gangs, and story-specific locations;
- author hero blocks by hand after the OSM graybox establishes scale;
- add Brockton after the first Unreal import path is proven on existing South
  Coast data.

The current repo already contains New Bedford and Fall River city data plus a
combined South Coast road mesh. Brockton should be treated as the next data
pull, not a blocker for the first Unreal slice.

## Required gameplay systems

| System | First-slice behavior | Scaffold hook |
| --- | --- | --- |
| On-foot traversal | Third-person walking, camera-relative movement, interaction input. | `MHPlayerCharacter` |
| Driving | One drivable heavy-feeling car with chase camera and enter/exit flow. | `MHVehiclePawn`, `MHPlayerCharacter::RequestEnterExitVehicle` |
| NPCs | Ambient wandering/fleeing pedestrians (AIModule/NavigationSystem) and interactable mission givers; Mass AI can expand crowd density later. | `MHPedestrianCharacter`, `MHInteractable`, Mass plugins |
| Missions | Authored campaign with reach, timed, survive, talk, pay, and collect steps, plus fail/restart. | `MHMissionSubsystem` |
| Economy | Cash reward/cost loop, business ownership, and a quoted offer catalog (bribe, medkit, vest, repair). | `MHEconomySubsystem` |
| Dialogue | Short mission setup and world flavor, including two-choice branches. | `MHDialogueSubsystem`, `MHInteractable` |
| OSM world | Import roads/water/building blockout in Unreal centimeters. | `MHOpenWorldSubsystem` |
| Police/wanted | Crime reporting, heat decay, wanted stars, scaling chase pursuers. | `MHWantedSubsystem`, `MHPoliceUnitPawn`, `MHPoliceSpawnerActor` |
| Reputation | Faction standing gated by mission/shop outcomes. | `MHReputationSubsystem` |
| Radio | Station roster with real DJ/song content, cycled while driving. | `MHRadioSubsystem` |

## Campaign (canonical)

The playable campaign is `Data/Missions/vertical_slice.json` — 11 missions
across a prologue, three acts, and a hurricane finale, ported 1:1 from
`QUAHOG_GODOT1/scripts/systems/story_mission.gd` (the most complete story
implementation across the Godot/Web tracks; see `IMPROVEMENT_PLAN.md` for the
coordinate-conversion notes):

1. **Off the Boat** (Prologue) — New Bedford waterfront opener: meet Deacon
   Mealy at Seamen's Bethel, get ambushed at the fish pier, steal a car, reach
   the safehouse.
2. **Auction Rules / The Linguiça Run / Harbor Heat** (Act I — The Narrows) —
   New Bedford crew jobs for Sully's operation.
3. **Spindle City / Acquitted** (Act II — Spindle City) — Fall River, Lady
   Borden's crew, Battleship Cove and the Borden House.
4. **The Undefeated** (Act II — City of Champions) — Brockton, Iron Mike
   Fontaine and Champion City Gym.
5. **Heritage Marina / Compound Interest** (Act III — The Cape) — the Fake
   Kennedys' Hyannis compound.
6. **Gloria / Big Mamie** (Finale — Gloria) — Hurricane Gloria bears down;
   the climax plays out at the Hurricane Barrier and the USS Massachusetts.

Satire comes from characters, local institutions, radio, signage, and mission
dialogue; a stinger of older local violence / Lizzie Borden-era folklore can
still be layered into the Fall River act without leaning on parody of any
protected franchise. Only the New Bedford graybox is OSM-imported today —
Fall River, Brockton, and Cape Cod need their own import or hand-authored
blockout before those missions' trigger volumes are reachable in PIE.

## Acceptance criteria

- The level opens in Unreal and streams a playable city graybox.
- Player can walk, enter a car, drive, exit, and re-enter.
- At least one NPC starts a mission through an interaction prompt.
- Mission state changes from inactive to active to complete, including a talk step, a collect step, and a pay step.
- Economy balance changes after mission completion and after buying a catalog offer.
- Dialogue displays at least three authored lines, and one exchange offers two choices (`1` / `2`).
- Roads, water, and at least one landmark/blockout zone come from OSM-derived
  source data.
- Lighting, post-process, and materials target cinematic realism, even if final
  assets are placeholders.
- Runtime HUD shows objectives, dialogue, cash, and weather (`UMHGameHudWidget`).

## HUD & weather

`AMHPlayerController` spawns the game HUD at play start. Dialogue lines render
as bottom subtitles; objectives, cash/weather, wanted stars, and the current
radio station/song display in the corners. Press `.` in PIE to cycle
`EMHWeatherState` (fog/sun adjusted by `AMHWeatherDirectorActor`); press `R`
while driving to cycle radio stations (`AMHPlayerCharacter::RequestRadioNextStation`).

- Use Lumen, Nanite-ready assets, virtual shadow maps, and cinematic exposure.
- Favor overcast coastal realism, sodium vapor street lighting, wet asphalt,
  weathered brick, triple-decker houses, mills, diners, waterfront industry,
  pawn shops, municipal buildings, and used sedans/SUVs.
- Satire should come from characters, local institutions, radio, signage, and
  mission dialogue; visuals should stay grounded and photoreal.
