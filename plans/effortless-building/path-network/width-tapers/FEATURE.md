# Feature: Width Tapers

## Summary

Where exactly two segments of different widths meet at a point, the paving's outline never steps straight across from one side to the other. On each side where it would step, the narrower segment's side curves out and runs smoothly into the wider segment's side, with no corner at either end. That happens on both sides where they meet straight on, and on the outside of a shallow bend. Sides that already join, such as the inside of a bend, don't change. Each path point holds a taper length, saved with it, and the new command SetTaperLength sets it. A length of 0, the default, gives each curve a natural length: twice how far it moves out. While Edit path is chosen, each such point shows a cyan taper handle on the narrower segment's line, and dragging it sets the length. The taper shares the narrower segment with the paving at its other end, as corners already do. The curved paving is what is drawn and what boxes are tested against.

## Acceptance criteria

1. The park world checks pass with their new chosen edit, and the whole suite passes, including food_loop_test: `ctest.exe --preset windows-debug` reports 0 tests failed. (listed test)
2. In a new park, draw a guest path of two segments by hand, about 20 m each, with a slight bend between them. Drag the second segment's side out to about 8 m. Neither side of the join steps: each side of the first segment curves out smoothly into the second's side, and a cyan handle shows on the first segment's line where the curves start. Drag the handle away from the point: the ghost shows longer, gentler curves, and releasing commits them as shown. Drag it toward the point: the curves get shorter, down to MIN_TAPER_LENGTH. Clicking the handle without dragging changes nothing. (play)
3. Draw a guest path with a bend of about 40° and widen the second segment to about 16 m. The inside of the bend curves as it did before this feature. The outside, which used to step, now curves from the narrow side straight into the wide side, with no knob, no second curve, and no sharp corner. Draw a right-angled bend with one segment widened: it has no step, so it shows no taper handle and looks as before. (play)
4. With a taper set, widen the narrower segment past the other with its side handle. The curves move to the other segment, over the same length. Draw a guest path that ends on the tapered point. The point becomes a junction and the curves go. Delete that path, and the curves are back. Save the park and open it again, and they're still there. (play)
5. Place a shop beside the narrower segment, clear of it but where a longer curve would reach. Drag the taper handle so the curve reaches the shop: the ghost shows in the invalid tint, and releasing changes nothing. (play)
6. The scenarios run their random edits, which now include SetTaperLength, without a crash: `build/windows-debug/tpj_scenarios.exe | grep -c "scenario park-edits tick"` prints `3001`. Every scenario except park-edits simulates as it did once the taper field was added, before tapers changed the surface. (check, and `tpj_scenarios --compare`)
7. In the windows-release build, open tests/parks/stress/full.park and pick Edit path. The Debug panel's frame time matches main's with Edit path picked. (play)

## Tests

The feature adds a command, so the milestone asks for a chosen edit of it in the park world checks. Add it to tests/sim/park_world_checks_test.cpp next to the other chosen edits, and list it last in chosenEdits. It starts from the world warm.park reaches after one cycle, as the other chosen edits do. The existing checks then hold for it: its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name it for what it makes the capabilities do.

- Setting a taper length on a junction point guests stand at. Setup: find the first path point, in the order parkPathPoints gives, that two or more guest segments use, where a guest stands on one of those segments (a guest record's place is on that segment's key). Require that one exists. Change: a SetTaperLength of that point to 4 m. Require that the world accepts it. Expected: the existing checks hold. warm.park has no point where two segments run straight on, so no ground line or paving changes. The save holds the new taper length.

## Medium

The feature adds no fields or flows. It adds a field to park intent and a command that sets it, which decision 0025 places outside the medium.

- Park intent: a path point gains a taper length, saved as `taper`. Every point a command adds takes DEFAULT_TAPER_LENGTH, which is 0, the natural length.
- Published tapers: sim/park publishes each taper through parkTapers. The tools read it for the taper handle.
- Paved surface: the curves replace the step. The validity check and the renderer read it as before, so what is drawn and what is refused stay one shape.
- Ground lines change only where a taper's length makes a corner at the narrower segment's other end smaller. Routes reads them as before.

The world checks cover the new field once the chosen edit's committed world holds it. Every checked-in park gains `taper=0` on each point line.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: "addParkEdits registers the twelve commands." Add parkTapers and the public value ParkTaper to the readers of the paved surface: "the paved surface through parkSurface, its edges through parkEdges, and its tapers through parkTapers". Add DEFAULT_TAPER_LENGTH, which is 0, to the public sizes.
- Intent, the path point bullet: "A path point has a ground position, a corner radius, and a taper length. The radius is for the corner the point makes when exactly two segments meet there. The taper length is for where the point is a taper (Paved surface)."
- Save lines: "a path point's x, z, radius, and taper".
- Ground lines, the corner paragraph: replace "The other end needs the farther of two lengths: how far the arc there would touch the segment with that point's own radius, and how far that point's inner paving edges touch it with the radii they hold." with "The other end needs the farthest of three lengths: how far the arc there would touch the segment with that point's own radius, how far that point's inner paving edges touch it with the radii they hold, and, where that point is a taper whose narrower segment this is and holds a taper length above 0, that length added to how far its inner paving edges touch the segment."
- Paved surface: replace the bullet "Where two segments at a point run along one line, the edge between them has no radius. The outline runs around the point from one side to the other at the narrower half width, and the wider segment's end steps in to it." with two bullets:
  - In the bullet "At a corner, the paving between the two segments' bodies belongs to the two segments, divided along the line into the corner. At any other point that two or more segments use, it belongs to the point.", make the last sentence "At any other point that two or more segments use, it belongs to the point."
  - "Where two segments at a point run along one line, the edge between them has no radius. The outline runs around the point from one side to the other at the narrower half width. The wider segment's end steps in to the narrower half width at the point, unless the point is a taper."
  - "A point is a taper when exactly two segments with ground lines and different widths use it, and on at least one side the outline would step straight across at the point from one segment's side to the other's. On each such side, it curves instead. The narrower segment's side stops its taper length farther from the point than it would stop otherwise, and a curve runs from there to where the wider segment's side begins. The curve is the cubic that leaves the narrower segment's side along it and meets the wider segment's side along it, each direction scaled by how far the curve's far end lies along it, so that it never doubles back. The taper length is the point's taper length, made smaller if needed so that the curve starts no farther from the point than the narrower segment's reach on that side. If the point's taper length is 0, not finite, or below zero, the taper length on each side is twice how far the wider segment's side, where it begins, lies out from the line of the narrower segment's side. Sides that don't step don't change."
- Paved surface, the reach bullet: "A segment's reach at one end, on one side, is its straight length less how far the paving at its other end would reach along it on that side if that reach were the whole length, or half its straight length if that is more. That paving reaches to where the edge on that side touches the segment, or, where that side curves at a taper, to where the curve starts. If no other segment uses the other end, the reach is the whole length."
- Paved surface, after the parkEdges paragraph: "parkTapers gives each taper, in ascending key order of its point. A ParkTaper has the point's key, the place on the narrower segment's line where its sides would stop without curves, the key of the narrower segment, the unit direction along it away from the point, the length of its longest curve, and the longest any of its curves can be."
- Commands: "Twelve commands change intent", with SetTaperLength last in the list. "SetTaperLength names a path point and a length." Acceptance: "For SetTaperLength, it means the key holds a path point and the length is finite and at least 0." Effects: "SetTaperLength sets the point's taper length, as given, and changes nothing else. A length longer than fits is kept, and each curve runs the longest that fits." Add, before the effect bullets: "Every point a command adds has DEFAULT_TAPER_LENGTH." In the MovePoint bullet, "keeps its key, radius, and taper length". "A ParkEdit holds any one of the twelve commands."

src/tools/SPEC.md:

- Handles: after the edge handle sentence, add "Each taper that parkTapers gives has a taper handle. It lies on the narrower segment's line, where its longest curve starts, but never nearer the place its sides would stop without curves than TAPER_HANDLE_DEPTH, or than the longest any of its curves can be if that is less." handleAt gives "the radius, edge, or taper handle nearest a ground point". The tie rule doesn't change.
- Edit path: "Under the pointer, a radius, edge, or taper handle comes first". editHandles gives "every path point's marker, then every radius handle, edge handle, and taper handle". hoveredHandle gives "the radius, edge, or taper handle". The tool marks the path point under the pointer "when no radius, edge, or taper handle is under the pointer".
- Add, after the edge handle drag paragraph: "Dragging a taper handle sets that point's taper length. Where the longest curve starts moves as far as the pointer moves along the narrower segment, away from the point, so the press itself changes nothing. The length stays between MIN_TAPER_LENGTH and the longest any of the point's curves can be."

src/render/SPEC.md:

- "appendHandle adds a radius, edge, or taper handle as a flat disc of RADIUS_HANDLE_SIZE". park_mesh.h gains TAPER_HANDLE_COLOR, a cyan, so taper handles stand apart from edge handles.

src/app/SPEC.md:

- "every edge handle in EDGE_HANDLE_COLOR and every taper handle in TAPER_HANDLE_COLOR".

## Files affected

- Modify: `src/sim/park/intent.h`, `src/sim/park/intent.cpp`, `src/sim/park/internal/components.h`, `src/sim/park/surface.h`, `src/sim/park/surface.cpp`, `src/sim/park/layout.cpp`, `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/park/SPEC.md`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/SPEC.md`
- Modify: `src/render/park_mesh.h`, `src/render/SPEC.md`, `src/app/scene/scene_uploads.cpp`, `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/SPEC.md`
- Modify: `src/scenarios/park_edits.cpp`
- Modify: every `.park` file in `tests/parks/` and `tests/parks/stress/`
- Modify: `tests/sim/park_world_checks_test.cpp`, in the test pass

The user's untracked parks in `parks/` gain `taper=0` too, so they keep loading. They stay unstaged.

## Dependencies

paved-surface and predictable-editing, both merged.

## Out of scope

- Inserting a point inside a taper's curve ends the curve at the new point, since its segment is now shorter. Recorded as a deepening candidate.
- A segment that tapers where three or more segments meet, where the edges already join the widths.
- Routes and guests don't see the curves. Until wide-routes, routes gives each carrier its kind's fixed width.

## Open questions

None.
