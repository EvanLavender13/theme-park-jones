# Feature: Benches and Toilets

## Summary

The player places benches and toilet blocks. Each is a new kind of box with its own footprint, height, and color. The Tools panel gains Place bench and Place toilet, which place them as Place shop places a shop. Near guest paving, each fronts it in the pose the frontage snap gives. Move box, Delete, undo, and redo work on them as on any box, and they save by kind name. Until navigable-networks' facility-doors gives them doors, they have no doors, so they join no network and change nothing guests or shops do. tests/parks/facilities.park holds one of each. This is the core feature of facilities-and-hours.

## Acceptance criteria

1. The Tools panel has Place bench and Place toilet, after Place depot. Place bench places a bench, a 2.0 by 1.0 m box 1.0 m high, and Place toilet places a toilet block, an 8.0 by 5.0 m box 3.5 m high. Each has its own color, unlike every other kind's. Pressing puts the box down, and dragging before release turns it, as with Place shop. (play)
2. Near guest paving, a bench or toilet block fronts it in the pose frontagePose gives, as a shop does: square to the edge line nearest the pointer, its front face FRONTAGE_SETBACK outside the paving, and moved the least distance that fits beside a junction or on the inside of a curve. Near backstage paving alone, it doesn't front. Holding Ctrl places it at the pointer instead. (play: on tests/parks/warm-main-street.park, place benches and toilet blocks freehand along the main street, along a curve, and beside a junction)
3. The ghost of a bench or toilet block shows the overlap check: it is refused only where it would overlap a solid or paving, or leave the park (principle 5). Move box moves either one, Delete removes it, and undo and redo restore each of these edits. (play)
4. A bench or toilet block has no doors. It gets no walkway, parkDoors gives no door for it, and placing one changes nothing guests or shops do. Every existing park simulates as before. (play: a bench placed beside a path shows no walkway; `tpj_scenarios --compare` on output from before and after the feature, status 0)
5. Benches and toilet blocks save by the kind names bench and toilet-block, and load back. tests/parks/facilities.park holds a bench and a toilet block beside a guest path, so the park world checks cover the new kinds. (ctest, and a capture of tests/parks/facilities.park that shows both beside the path in their colors, with no walkway at either)

## Tests

None. The park world checks cover the new kinds through tests/parks/facilities.park with no change to the tests.

## Medium

The feature adds no fields or flows. It adds two values to park intent's box kind, which decision 0025 places outside principle 3.

- Park intent: bench and toilet block boxes, read through parkBoxes. Routes, the tools, render, and the scene read them as they read every box.
- Routes reads them through parkBoxes and gives them no doors, so they anchor nothing and publish nothing. facility-doors gives them doors later.
- Operations reads only shop and depot boxes, so it is unchanged.

## Spec changes

src/sim/park/SPEC.md, Intent:

- "A box has a kind, shop or depot, and a pose." becomes "A box has a kind, shop, depot, bench, or toilet block, and a pose."
- "Kinds are saved by name: guest, backstage, shop, and depot." becomes "Kinds are saved by name: guest, backstage, shop, depot, bench, and toilet-block."

src/sim/routes/SPEC.md, Connectors, the list of doors gains a last item:

- "A bench and a toilet block have no doors."

src/tools/SPEC.md:

- Boundary: the ToolKind list becomes "None, GuestPath, BackstagePath, PlaceShop, PlaceDepot, PlaceBench, PlaceToilet, MoveBox, EditPath, RemovePoint, or Delete".
- Tools: "Place shop and Place depot place a box at the pointer." becomes "Place shop, Place depot, Place bench, and Place toilet place a box of their kind at the pointer: a shop, a depot, a bench, or a toilet block."
- Tools: "Near paving of the kind its front door serves, the box fronts that paving instead, in the pose frontagePose gives. A shop's front door serves guest paving, and a depot's serves backstage paving." becomes "Near paving of the kind its front faces, the box fronts that paving instead, in the pose frontagePose gives. A depot's front faces backstage paving, and the front of a shop, a bench, or a toilet block faces guest paving."
- Tools: "and its front door lies within CONNECTION_REACH of paving of the kind it serves, so it connects." becomes "and the middle of its front face lies within CONNECTION_REACH of paving of the kind it faces, so a door there connects."

src/render/SPEC.md, The park: "Guest paths, backstage paths, shops, depots, and the entrance each have their own opaque color." becomes "Guest paths, backstage paths, shops, depots, benches, toilet blocks, and the entrance each have their own opaque color."

src/app/SPEC.md, Tools and the ghost: the tool list becomes "Look, Guest path, Backstage path, Place shop, Place depot, Place bench, Place toilet, Move box, Edit path, Remove point, and Delete".

Public interface: BoxKind in src/sim/park/intent.h gains Bench and ToiletBlock after Depot, named bench and toilet-block. ToolKind in src/tools/tools.h gains PlaceBench and PlaceToilet after PlaceDepot. No function is added.

## Files affected

- Modify: src/sim/park/intent.h
- Modify: src/sim/park/SPEC.md
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/routes/SPEC.md
- Modify: src/tools/tools.h
- Modify: src/tools/tools.cpp
- Modify: src/tools/frontage.cpp
- Modify: src/tools/SPEC.md
- Modify: src/render/park_mesh.h
- Modify: src/render/SPEC.md
- Modify: src/app/ui/tool_panel.cpp
- Modify: src/app/SPEC.md
- Create: tests/parks/facilities.park

## Dependencies

- path-network's frontage snap, undo and redo, and RestoreIntent. Complete.

## Out of scope

- Doors for benches and toilet blocks, and their connectors: navigable-networks' facility-doors.
- Seats and stalls, and what guests do at a bench or toilet block: plausible-operations' served-needs.
- Inspecting a bench or toilet block. Clicking one with Look leaves the Inspector as it was, as clicking a depot does.
- Previewing what a bench or toilet block would serve: legible-simulation's explained-needs.
- A depot-like backstage door for toilet blocks, such as for cleaning staff. Nothing in the slice uses one.

## Open questions

- The footprints, heights, and colors are tuning values. This feature sets them from RESEARCH.md, and playing adjusts them.
