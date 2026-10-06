# Feature: Edge Radii

## Summary

The player curves the inside and the outside of a bend separately. Today the bend's radius handle curves both together. After this feature, it curves only the inside edge and the line guests walk, and a new handle on the outside edge curves only the outside. Both start sharp, so a new bend is sharp inside and has a square outside corner. The player can curve either one and keep the other sharp, and the paving fills whatever lies between them. Each inside corner of a junction, and each outside turn at one, gets a handle of its own too, and starts sharp. A segment keeps the radius of the edge at each of its ends, so a curve stays with its segment when the segment splits, merges, or moves. An outside edge never curves in past the path's width where guests walk. Where two segments of different widths turn a corner, the edges curve from one width to the other, where today the wider end steps in.

## Acceptance criteria

1. In the app's new park, draw a guest path with a right-angled bend. The bend is sharp inside and has a square outside corner. Pick Edit path. The bend shows its radius handle, inside the bend, and one new handle on its outside corner. Drag the outside handle in toward the path: the outside corner curves, and the inside stays sharp. Release, and it commits as the ghost showed. With the Graph checkbox on, the guest network's line doesn't move. Then drag the radius handle: the inside and the line guests walk curve, and the outside keeps the curve it was given. Click a handle without dragging: nothing changes. (play)
2. Draw another bend and drag only its radius handle, to about 6 m. The inside curves and the outside stays square, with the paving filled in between. Drag the outside handle in as far as it goes: it stops where the outside lies half the width outside the line guests walk. Widen one of the two segments: the outside curves from one width to the other with no step. (play)
3. Draw a guest path that ends on the middle of another, making a T. Its two inside corners are sharp, and each shows a handle. Drag one: only that corner curves. Click the joined segment to insert a point on it, then delete that point: the corner keeps its curve through both edits. Save the park, open it again, and the corner still has that curve. (play)
4. Place a shop beside one of the T's inside corners, clear of the paving. Drag that corner's handle toward the shop until the curve would reach it: the ghost turns to the invalid tint, and releasing there changes nothing. (play)
5. Captures of tests/parks/fed.park and tests/parks/stress/full.park show every bend with a sharp inside and a square outside, and every junction's corners sharp. (capture)
6. The scenarios run their random edits, which now include SetEdgeRadius, without a crash: `build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null | grep -c "scenario park-edits"` prints `1001`. Every scenario except park-edits simulates as it did once the new intent fields were added, before the surface changed. (check, and `tpj_scenarios --compare`)
7. In the windows-release build, open tests/parks/stress/full.park and pick Edit path. The Debug panel's frame rate matches main's with Edit path picked. (play)
8. The park world checks pass with their new chosen edit, and the whole suite passes: `ctest.exe --preset windows-debug` reports 0 tests failed. (listed test)

## Tests

The feature adds a command, so the milestone asks for a chosen edit of it in the park world checks. Add it to tests/sim/park_world_checks_test.cpp next to the other chosen edits, and list it last in chosenEdits. It starts from the world warm.park reaches after one cycle, as the other chosen edits do. The existing checks then hold for it: its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name it for what it makes the capabilities do.

- Curving an inside corner of a junction that guests walk. Setup: find the first edge parkEdges gives whose point three or more guest segments use, where a guest stands on one of them (a guest record's place is on that segment's key), and whose MaxRadius is above 0. Require that one exists. Change: a SetEdgeRadius of that edge's segment and point to half its MaxRadius. Require that the world accepts it. Expected: the existing checks hold. No segment's ground line changes, so the guests stay where they stand, and the save holds the new edge radius.

## Medium

No fields or flows. Edge radii are park intent, which decision 0025 places outside the medium. sim/park publishes them through parkSegments, and the edges they make through parkEdges. The tools read parkEdges for the handles, and the renderer reads the surface as before. Routes reads ground lines, which edge radii don't change.

The park world checks cover the new intent once a park file holds it. Every checked-in park holds edge radii of 0, and the chosen edit above saves a curved one.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: "addParkEdits registers the ten commands" becomes "the eleven commands". Add parkEdges to the public queries, as giving the public value ParkEdge, and add edgeRadiusFor and DEFAULT_EDGE_RADIUS, which is 0, to the public functions and sizes.
- Intent, the segment bullet, becomes: "A path segment has a kind, guest or backstage, its two path points, From and To, a width, and an edge radius at each end." The save lines' segment entry becomes "a path segment's kind, from, to, width, from-edge-radius, and to-edge-radius, from and to being its points' keys".
- Paved surface: replace the corner bullet and the junction bullet with these:

> - At any other point, the outline turns from each segment's left side, seen walking away from the point, to the right side of the next segment counter-clockwise around the point. Each such turn is an edge. If the next segment lies less than half a turn around, the edge is an inner edge: an arc that touches both sides from outside the paving. If it lies more than half a turn around, the edge is an outer edge: an arc that touches both sides from inside the paving. An edge of radius 0 is sharp: the two sides run straight on until they meet.
> - A segment's start at a point is where its ground line's arc touches it, at a corner, and the point itself elsewhere. An inner edge touches each segment between its start and its reach. An outer edge touches each segment no farther from the point than its start, and no farther behind the point than the wider of the two widths. The radius is the one asked for, made larger or smaller as little as needed to fit. If no radius fits, the outline steps straight across at the point, from one side to the other.
> - At a corner, the inside edge's radius is the corner's radius less half the narrower width, or 0 if that is less. Every other edge's radius is the edge radius held by the segment it starts from, at that end, or 0 if that is below zero or not finite. A corner's outside edge is one of these, so it curves apart from the corner's radius. The edge radius held for a corner's inside edge is kept, and used if the point stops being a corner.
> - Where two segments at a point run along one line, the edge between them has no radius. The outline runs around the point from one side to the other at the narrower half width, and the wider segment's end steps in to it.
> - Each side of a segment stops where the edge on that side touches it, or at the point if the edge touches behind the point or steps across it.
> - At a corner, the paving between the two segments' bodies belongs to the two segments, divided along the line into the corner. At any other point that two or more segments use, it belongs to the point.

- Paved surface, edge lines: "along a corner's inner and outer edges, and around a junction from one segment to the next" becomes "and around each point from each segment to the next".
- Paved surface, a new paragraph at the end:

> parkEdges gives each edge that has a radius, other than a corner's inside edge, in ascending key order of its point, then of the segment whose left side it starts from. A ParkEdge has the point's key, that segment's key, the unit direction along each of the two segments away from the point, that segment's first, whether it is an inner edge, the radius it is rounded with, and the smallest and largest radii that fit. It also has where its curve's middle lies, where the two sides meet, and the unit direction from there in which the middle moves as the radius grows. edgeRadiusFor gives the radius whose middle lies a given distance from where the sides meet, in that direction. It gives 0 for a distance of 0 or less.

- Physical validity: "A segment leaves the park when its ground line comes nearer the park's edge than half its width." gains ", or when a piece of the surface it or its points own reaches past the park's edge by more than CONTACT_TOLERANCE".
- Commands: "Ten commands" becomes "Eleven commands", and the list adds SetEdgeRadius. After "SetCornerRadius names a path point and a radius.", add "SetEdgeRadius names a segment, one of its two points, and a radius." In the acceptance paragraph, add "For SetEdgeRadius, it means the key holds a segment with a ground line, the point is one of its two points, and the radius is finite and at least 0." In the same paragraph, "no piece of the surface the command adds or changes meets a solid" becomes "no piece of the surface the command adds or changes meets a solid or leaves the park".
- Commands, the effects list:
  - AddPath: "of the path's kind and pathWidth" gains ", with DEFAULT_EDGE_RADIUS at both ends". "The segment keeps its key, kind, and width, and now ends at its first split point." becomes "The segment keeps its key, kind, width, and edge radius at its From end, and now ends at its first split point, with DEFAULT_EDGE_RADIUS there." "the last ends at the segment's old To point" gains ", with the segment's old edge radius there". Add: "Each other end of a new part has DEFAULT_EDGE_RADIUS."
  - MovePoint: each "keeps its key, kind, and width" becomes "keeps its key, kind, width, and edge radii".
  - DeletePoint: "Its end at the point moves to the other segment's far end" gains ", with the other segment's edge radius there".
  - Add: "SetEdgeRadius sets the segment's edge radius at the point, as given, and changes nothing else. A radius that doesn't fit is kept, and the edge is rounded with the nearest that fits."
- editedSurface: "It gives none when editedSegments gives none." becomes "It gives none when the edit doesn't name what it acts on, when a segment the edit adds or reshapes has no ground line, and for edits of boxes."
- ParkEdit: "any one of the ten commands" becomes "any one of the eleven commands".

src/tools/SPEC.md:

- Boundary: "radiusHandleAt" becomes "handleAt", and "radiusHandles and hoveredHandle" becomes "editHandles and hoveredHandle".
- Picking, the radius handle paragraph: after the radius handle's two sentences, add "Each edge that parkEdges gives has an edge handle at its curve's middle." Replace the radiusHandleAt sentence with "handleAt gives the radius handle or edge handle nearest a ground point, within HANDLE_REACH."
- Tools, Edit path: "a radius handle comes first" becomes "a radius or edge handle comes first". "radiusHandles gives where every corner's radius handle lies" becomes "editHandles gives every radius handle and edge handle". "hoveredHandle gives the radius handle or the segment's sides" becomes "hoveredHandle gives the radius or edge handle, or the segment's sides". "when no radius handle is under the pointer" becomes "when no radius or edge handle is under the pointer". After the radius handle's drag paragraph, add "Dragging an edge handle sets that edge's radius. The edge's middle moves as far as the pointer moves along the direction it moves in as the radius grows, so the press itself changes nothing. The radius stays between the smallest and largest that fit." "If the player clicks a point, a radius handle, or a side" becomes "If the player clicks a point, a handle, or a side".

src/render/SPEC.md:

- The boundary list: "appendRadiusHandle" becomes "appendHandle", and HANDLE_COLOR gains EDGE_HANDLE_COLOR.
- "appendRadiusHandle adds a radius handle as a flat disc of RADIUS_HANDLE_SIZE" becomes "appendHandle adds a radius or edge handle as a flat disc of RADIUS_HANDLE_SIZE".

src/app/SPEC.md:

- "it shows every corner's radius handle in HANDLE_COLOR, and highlights the radius handle or the segment's sides under the pointer" becomes "it shows every corner's radius handle in HANDLE_COLOR and every edge handle in EDGE_HANDLE_COLOR, and highlights the handle or the segment's sides under the pointer".

## Files affected

- Modify: src/sim/park/SPEC.md, src/sim/park/intent.h, src/sim/park/internal/components.h, src/sim/park/internal/layout.h, src/sim/park/layout.cpp, src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/park/edits.h, src/sim/park/edits.cpp
- Modify: src/tools/SPEC.md, src/tools/tools.h, src/tools/tools.cpp
- Modify: src/render/SPEC.md, src/render/park_mesh.h, src/render/park_mesh.cpp
- Modify: src/app/SPEC.md, src/app/application.cpp, src/app/input/interaction.h, src/app/input/interaction.cpp, src/app/scene/scene_sync.h, src/app/scene/scene_uploads.cpp
- Modify: src/scenarios/park_edits.cpp
- Modify: every park in tests/parks/ and tests/parks/stress/
- Modify: tests/sim/park_world_checks_test.cpp (test pass)
- Modify: plans/effortless-building/path-network/MILESTONE.md

## Dependencies

paved-surface, for the surface the edges shape. Merged.

## Out of scope

- A handle for a corner's inside edge apart from the radius handle. The radius handle sets both the inside edge and the line guests walk.
- Handles on edges between segments that run along one line. Those edges keep the narrower half width around the point, and the wider end steps in. width-tapers smooths the straight-on case.
- Bevels. A sharp outer edge that would reach farther behind the point than the wider width is rounded instead.
- Showing handles only near the pointer. Every handle shows while Edit path is chosen.

## Open questions

None.

## Tuning

These values bound what the player can set. Each is a fit limit, not a style choice.

- An outer edge reaches no farther behind its point than the wider of its two segments' widths. Past that, its radius grows.
- Edges never cut inside a segment's start, so the paving always covers the path's width where guests walk.
