# tools

The park's editing tools: what the player's pointer does to the park. A tool never changes the world. It reads intent through sim/park's public queries and geometry (sim/park/SPEC.md). It gives edits, as ParkEdit values, which the app queues for the next cycle. The library tpj_tools links only tpj_sim, so it runs without a window (principle 10).

## Boundary

A ToolState holds one tool, of a ToolKind: None, GuestPath, BackstagePath, PlaceShop, PlaceDepot, MoveBox, EditPath, or Delete. The app drives it with four calls: selectTool, movePointer, pressPointer, and releasePointer. The app reads tentativeEdit, the edit the ghost shows, and highlightedEntity, the entity the tool marks. boxAt, segmentAt, pointAt, radiusHandleAt, sideAt, snapStop, and snapPoint are the picking queries the tools use. radiusHandles and hoveredHandle give the handles Edit path shows.

## Input

selectTool changes the tool and drops anything held or drawn without committing it. movePointer records where the pointer meets the ground, or that it meets none, as when it is over a panel. pressPointer and releasePointer are the primary button going down and up.

When the button is released, the tool commits exactly the edit tentativeEdit showed just before, so what the player sees in the ghost is what gets committed. A tool commits its edit whether or not the simulation will accept it, and only the simulation refuses edits (principle 5). A path tool asks the simulation before it adds a stop, as described under Tools. Neither tentativeEdit nor highlightedEntity changes anything.

## Picking

boxAt gives the box whose footprint holds a ground point. segmentAt gives the segment whose width covers the point. If several do, it gives the first in key order. Neither gives the entrance. pointAt gives the path point nearest a ground point, within POINT_REACH. If two are equally near, it gives the one with the lower key.

Each corner (sim/park/SPEC.md, Ground lines) has a radius handle. It lies on the line from the corner's point into the corner, at the arc's middle, but never nearer the point than RADIUS_HANDLE_DEPTH. radiusHandleAt gives the corner whose radius handle is nearest a ground point, within HANDLE_REACH, by its point's key. A segment's sides are the two lines half its width from its ground line. sideAt gives the segment whose side lies nearest a ground point, within SIDE_REACH. If two are equally near, each gives the one with the lower key.

snapStop gives the stop a path tool's click makes. If a path point of the tool's kind, or a point an earlier stop of the path being drawn makes, lies within SNAP_REACH, the stop is the nearest such point. The path's last stop is left out, because a click there finishes the path. Otherwise, if a segment of the kind, or a segment between two stops of the path being drawn, passes within SNAP_REACH, the stop is the place on the nearest such segment nearest the click, or the segment's end point when that place lies within MIN_SEGMENT_LENGTH of it. Otherwise the stop is a new point at the click. A point is a kind's when at least one segment uses it and every segment at it has that kind, so a path never snaps onto a point or segment of the other kind. Distances to a segment are measured to the straight line between its two points, not to its rounded ground line.

snapPoint gives the target a dragged path point moves to. It follows snapStop's rules for a path of the point's kind with no stops drawn, but it leaves out the dragged point and every segment at it. A point's kind is the kind of the segments at it. If no segment uses the point, the target is a new point at the place.

## Tools

Look (ToolKind None) edits nothing.

Guest path and Backstage path draw a path of their kind. Each click adds the stop snapStop gives. If the world would refuse the path through the stops so far and that stop, as when it runs through a box, the click adds nothing, and the ghost stays in the invalid tint until the pointer moves somewhere valid. The first click always adds its stop. A click near a point or segment of the same kind joins the path there, and a segment it joins splits there when the path commits. Where the path crosses a segment of its kind, or itself, both split and share a point there when it commits. Clicking the last stop again finishes and commits the path. The ghost shows the path through the stops and the pointer, and marks each stop that joins a point or segment, and each place where the path crosses one. Before the first click, pathStart gives where that click would start the path and whether it would join a point or segment there, so the ghost can show it. If the drawn path has fewer than two stops, finishing it commits nothing. While a press would finish a path of at least two stops, pathFinish gives its last stop, so the ghost can show that a click there finishes the path.

Place shop and Place depot place a box at the pointer. Pressing puts the box down, and dragging before release turns it to face the drag. The next box faces the same way as the last.

Move box marks the box under the pointer, and dragging moves it, keeping its facing. If the player clicks without dragging, nothing is committed.

Edit path reshapes paths of both kinds. Under the pointer, a radius handle comes first, then a path point, then a segment's side, then the segment itself. radiusHandles gives where every corner's radius handle lies while Edit path is chosen and nothing is held, so the ghost can show them. hoveredHandle gives the radius handle or the segment's sides under the pointer, when a press would take hold of one. The tool marks the path point under the pointer, as pointAt gives it, when no radius handle is under the pointer.

Dragging a point moves it, and where it lands snaps as snapPoint gives, so a point dropped on a point or segment of its kind joins it there. The ghost shows every segment the move reshapes, and marks a join with a point or segment, and each place where a moved segment crosses one.

Dragging a radius handle sets the corner's radius. The arc's middle moves as far into the corner as the pointer moves along the corner's halfway line, so the press itself changes nothing. The radius stays between 0 and the largest that fits.

Dragging a segment's side sets its width: the width at the press, plus twice how far the pointer has moved away from the ground line. The width stays between MIN_PATH_WIDTH and MAX_PATH_WIDTH.

The ghost shows the reshaped segments. If the player clicks a point, a radius handle, or a side without dragging, nothing is committed. Away from all of these, the ghost marks the place nearest the pointer on the segment under it, and a click inserts a point there.

Delete removes the box under the pointer. If there is no box, it deletes the path point under the pointer, as pointAt gives it. Otherwise it removes the segment under the pointer, and any point no other segment uses. The ghost shows what it removes, and the segment that a point's deletion would merge its two segments into.
