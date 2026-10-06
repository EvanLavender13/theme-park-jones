# Feature: Predictable Editing

## Summary

Every edit changes only what it names, and the player can see what a click will do. Joins and splits keep every curve the player set. A path that joins a rounded bend's point turns it into a junction, and the bend's curves stay as that junction's edges. A path that ends on a curve, crosses it, or has a point inserted on it splits the bend there into two bends of the same radius, so the curve doesn't move. A curve may use as much of its segment as the other end leaves free, so a split beside it no longer shrinks it. Delete removes only the segment or box under the pointer, and its ghost tints only what goes. Merging two segments into one becomes its own tool, Remove point. While Edit path or Remove point is chosen, every path point shows a marker.

## Acceptance criteria

1. The park world checks pass over every park, with their chosen edits now including RemovePoint in place of DeletePoint and InsertPoint naming its place. food_loop_test passes. (ctest)
2. Round a bend with its radius handle, and curve its outside with its edge handle. Draw a path that ends on the bend's point from outside the bend. The inside curve stays where it was, with an orange edge handle where the white one was, and the outside curve shows on both sides of the new path. Delete that path. The bend is back with the same inside and outside curves. Draw a path that ends on the bend's point from inside the bend instead. The inside curve shows on both sides of the new path. (play)
3. Round a bend to about half as far as its handle goes. Insert a point on the straight part of one of its segments, and draw a path that ends on the straight part of the other. The curve doesn't change. (play)
4. Round a bend. Draw a path that ends on the middle of the curve, and another that crosses the curve. Each joins the curve where it meets it, and the join mark shows there before commit. The curve's inside edge and its line in the Graph overlay don't move. A sharp outside corner is cut along the line that touches the curve at the join, and an outside edge curved to the bend's radius plus half the width doesn't move. Clicking the curve with Edit path inserts a point there the same way, and the bend then has a white handle on each side of that point. (play)
5. With Delete, hovering a segment tints only that segment's paving, and hovering a box tints only that box. Besides the candidate's walkways and starved marks, the ghost draws only the segments whose line guests walk the delete changes, as when a bend comes back. A refused delete shows its tint in the invalid tint. Clicking removes only that segment or box, and points no segment uses. Hovering near a point of a segment deletes the segment, never the point. (play)
6. With Remove point, every path point shows a marker. Clicking a point with two segments merges them into one straight segment, and the ghost shows that segment first. A point with one segment, or three or more, isn't marked under the pointer, and clicking it does nothing. Milestone criterion 4 holds: remove the point between two straight-on segments guests walk on, and no guest jumps or turns around. (play)
7. With Edit path, every path point shows a marker, under the radius and edge handles, and the point under the pointer is highlighted. (play)

## Tests

None. RemovePoint replaces DeletePoint's merge, and InsertPoint's place becomes a variant. The chosen edits in tests/sim/park_world_checks_test.cpp change only to build against them.

## Medium

The feature adds no fields or flows. It changes park intent's commands and the tools that issue them, which decision 0025 places outside the medium. Routes, the renderer, and the app read published segments, corners, and the surface as before. The save format doesn't change, and no new state is registered.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: add bendPlace and bendAlong to the public functions.
- Ground lines, the corner paragraph: replace "made smaller if needed so that the arc touches each segment no farther from the point than half that segment's straight length" with "made smaller if needed so that the arc touches each segment no farther from the point than that segment's straight length less what its other end needs, or half the straight length if that is more. The other end needs the farther of two lengths: how far the arc there would touch the segment with that point's own radius, and how far that point's inner paving edges touch it with the radii they hold. If no other segment uses the other end, the arc may use the whole segment."
- Ground lines: add "A corner's arc runs from where it touches its first segment, the lower-keyed, to where it touches the second. A place along it is given by a fraction: the place where the arc meets the line from its center through the point that fraction of the way from the first touch to the second. bendPlace gives the place for a fraction. bendAlong gives the fraction of the arc's place nearest a ground point, from 0 to 1."
- Paved surface: replace the reach bullet with "A segment's reach at one end, on one side, is its straight length less how far the edge on that side at its other end would touch it if that edge's reach were the whole length, or half its straight length if that is more. If no other segment uses the other end, the reach is the whole length." Delete "The edge radius held for a corner's inside edge is kept, and used if the point stops being a corner."
- Commands: rename DeletePoint to RemovePoint everywhere.
- Commands, stops: add "A stop can also be a place on the arc of a corner with a radius above 0 (OnBend), given by the corner's point and a fraction along the arc." InsertPoint names a place, either a place on a segment (OnSegment) or a place on an arc (OnBend). A MovePoint target can also be an OnBend.
- Commands, acceptance: AddPath's OnBend names a point that makes a corner whose segments both have the path's kind and whose radius is above 0, at a fraction strictly between 0 and 1. A MovePoint OnBend target names such a corner of the point's kind, neither of whose segments ends at the point. InsertPoint's OnBend names such a corner of either kind. For RemovePoint, it means exactly two segments use the point, their far ends are different points, and the segment it merges has a ground line, as does every part that segment splits into.
- Commands, effects: RemovePoint destroys the point and merges its two segments, as DeletePoint's merge does today. Drop DeletePoint's "Otherwise" sentence. AddPath adds a point at each OnBend stop too. Add the bullet: "A corner splits at places on its arc, given by OnBend stops, an OnBend target or insert, or where a segment meets the arc. A point is added at each place, with DEFAULT_CORNER_RADIUS. The corner's point moves to where the line touching the arc at the first place meets the first segment's line. A new point is added where the lines touching the arc at each two places in a row meet, and where the line touching it at the last place meets the second segment's line. Each of these corner points takes the radius the corner was rounded with. The second segment's end moves from the corner's point to the last of them. New segments run from the corner's point through the places and the points between them to that last point. The last new segment takes the second segment's width, and the rest the first's. Each new corner's outside edge takes the old corner's outside edge radius, and every other new end has DEFAULT_EDGE_RADIUS. The new corners' arcs together lie exactly on the old arc."
- Commands, joins: replace "Joins are found along the straight line between each segment's two points, not along its rounded ground line. Near a rounded corner, a path can cross or end on the arc without joining it." with "A segment the command leaves in place meets along the straight part of its ground line, between where the arcs at its ends touch it. A corner the command leaves in place, with both of its segments, meets along its arc, unless the command joins the corner's own point, as with an OnPoint stop or target. Where a segment the command adds, moves, or merges crosses an arc or ends on it, the corner splits there. If that place lies within MIN_SEGMENT_LENGTH of where the arc touches a segment, that segment splits where the arc touches it instead. A segment the command adds, moves, or merges meets others along the straight line between its points."
- Commands, add a paragraph on curves: "Joins and splits keep every curve. When an edit leaves a point that made a corner with three or more segments, the edge radius held for the corner's inside edge becomes the corner's radius less half the narrower width, or 0 if that is less, and the point keeps its radius. When an edit leaves a point that had two or more segments with one, its radius becomes DEFAULT_CORNER_RADIUS. When AddPath or MovePoint, or a split, adds segment ends at a point that already had two or more segments, each run of them lands inside one edge. The edges on either side of the run touch that edge's two segments where it did, so its curve stays where it was on both sides. Edges between the added ends take its radius, and a sharp edge stays sharp. When an edit removes segment ends from a point that keeps two or more and stays where it was, each edge that now spans a gap touches the segment it starts from where that segment's old edge did, so a curve a join cut in two comes back when the join goes. A merged segment's end keeps the edge radius described below. When an edit leaves a point that had three or more segments with exactly two that make a corner, the point's radius becomes its inside edge's radius plus half the narrower width. If that edge's radius is 0, the point's radius becomes the smaller of its own radius and half the narrower width."

src/tools/SPEC.md:

- Boundary: ToolKind gains RemovePoint, between EditPath and Delete.
- Picking: replace "Distances to a segment are measured to the straight line between its two points, not to its rounded ground line." with "Distances to a segment are measured to the straight part of its ground line. A corner of the kind with a radius above 0 is a place to snap to as well, measured to its arc, and a place on it gives an OnBend stop. If an arc and a segment are equally near, the segment wins." snapPoint also leaves out every corner with a segment at the dragged point.
- Edit path: the ghost marks the place nearest the pointer on the ground line of the segment under it, and a click inserts a point there, as an OnBend when the place lies on an arc. editHandles gives every path point's marker first, then the radius and edge handles, while Edit path is chosen and nothing is held, and only the markers while Remove point is chosen.
- Add: "Remove point marks the path point under the pointer, as pointAt gives it, when exactly two segments use it, and a click removes it with RemovePoint, merging its two segments. If the player clicks away from every point, nothing is committed."
- Delete: "Delete removes the box under the pointer. If there is no box, it removes the segment under the pointer, and any point no other segment uses. The ghost shows what it removes."

src/render/SPEC.md:

- Ghosts: the first bullet applies to every edit but DeleteSegment. The delete bullet reads "the box or segment an accepted delete would remove, or both segments at a point an accepted RemovePoint merges, in a delete tint, under the segments the edit reshapes. A DeleteSegment's ghost is this tint, in the invalid tint when the world refuses it, the paving of each segment whose ground line the delete changes, as editedSegments gives them, and the candidate's walkways and starved marks."
- Add: "appendPointMark adds a path point's marker as a flat disc of POINT_MARK_SIZE, in a given color, under handles and above join marks."

src/app/SPEC.md:

- The tools list gains Remove point, before Delete.
- While Edit path or Remove point is chosen and nothing is held, the ghost shows every path point's marker in POINT_MARK_COLOR.

## Files affected

- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/park/geometry.h`, `src/sim/park/geometry.cpp`, `src/sim/park/layout.cpp`, `src/sim/park/surface.cpp`, `src/sim/park/SPEC.md`
- Create: `src/sim/park/curves.cpp`, `src/sim/park/internal/curves.h`, `src/sim/park/internal/around.h`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/SPEC.md`
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`, `src/render/SPEC.md`
- Modify: `src/app/ui/tool_panel.cpp`, `src/app/scene/scene_uploads.cpp`, `src/app/SPEC.md`
- Modify: `src/scenarios/park_edits.cpp`
- Modify: `tests/sim/park_world_checks_test.cpp`, to build against the renamed command and InsertPoint's place

## Dependencies

edge-radii, which is merged.

## Out of scope

- A segment the command adds or moves meets others along the straight line between its points, not along its own arcs. Dragging a rounded bend across a path can join it off the curve. It is a deepening candidate in MILESTONE.md.
- When a dragged bend is dropped on a point that already has two or more segments, the target's edges win over the bend's curves.
- Deleting a segment tints the segment's own paving, not the junction paving at its points that also goes.
- A point placed anywhere on a segment's paving joins it, and snap guides. These stay in snaps-and-guides.

## Open questions

None.
