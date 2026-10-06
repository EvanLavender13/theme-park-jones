# park

This module holds park intent: what the player authored, from which the rest of the park is derived (principle 1, decision 0025). It also makes the game's one refusal, physical validity (principle 5). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

addParkIntent registers the intent types entrance, path-point, path-segment, and box. Their components are private to this module (decision 0016). addParkEdits registers the eleven commands. makeParkSchema calls both after the medium's types.

Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, parkCorners, and parkBoxes, the paved surface through parkSurface, and its edges through parkEdges. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, ParkCorner, and ParkBox, and ParkEdge. Other modules change intent only through commands.

segmentLine, cornerRadiusFor, edgeRadiusFor, stopPosition, stopPositions, pathCrossings, editedSegments, editedSurface, footprintOf, and the sizes pathWidth, MIN_SEGMENT_LENGTH, ARC_TOLERANCE, DEFAULT_CORNER_RADIUS, DEFAULT_EDGE_RADIUS, which is 0, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so that the renderer, the tools, and routes measure the same shapes this module validates.

## Intent

The park is a square PARK_SIZE on a side, centered on the origin of the ground.

- A path point has a ground position and a corner radius. The radius is for the corner the point makes when exactly two segments meet there.
- A path segment has a kind, guest or backstage, its two path points, From and To, a width, and an edge radius at each end. Segments share a point where the player joined them, and where a path was drawn or moved across them. Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md).
- A box has a kind, shop or depot, and a pose.
- An entrance has a pose.

A Pose is a position and a facing on the ground, held as given. A default Pose is at the origin, facing -z.

The save lines hold:

- an entrance's x, z, facing-x, and facing-z;
- a path point's x, z, and radius;
- a path segment's kind, from, to, width, from-edge-radius, and to-edge-radius, from and to being its points' keys;
- a box's kind, x, z, facing-x, and facing-z.

Kinds are saved by name: guest, backstage, shop, and depot.

## Ground lines

A segment's ground line runs from its From point to its To point, as CarrierPoints (sim/medium/SPEC.md), the first at distance 0 and each at its distance along the line. It is straight, except where it rounds a corner at either end. parkSegments gives each segment with its ground line. segmentLine gives the straight ground line between any two positions. The renderer, the validity check, and routes all use a segment's ground line, so what is drawn, what is refused, and what is walked are the same line.

A segment's ground line is empty when segmentLine between its two points is empty, which it is when either position is not finite or lies outside the park, or the two lie closer together than MIN_SEGMENT_LENGTH. It is also empty when the segment names a key that holds no path point, names the same point twice, or has a width that is not finite and above zero.

A point makes a corner when exactly two segments use it, neither ground line is empty, and their far ends are different points at least MIN_SEGMENT_LENGTH apart. The point must also lie farther than MIN_SEGMENT_LENGTH from the line through the two far ends. Where the segments run straight on, or fold straight back, the point makes no corner. parkCorners gives each corner, in its point's key order. A ParkCorner has the point's key and position, the unit direction along each of its two segments, the lower-keyed segment's first, and the unit direction into the corner, halfway between those two. It also has the radius the corner is rounded with, the largest radius that fits, and where the arc's middle lies.

A corner is rounded with a circular arc that touches both of its segments. The arc's radius is the point's radius, made smaller if needed so that the arc touches each segment no farther from the point than half that segment's straight length. That smaller radius is the largest that fits. If the point's radius is not finite, or is below zero, the corner is sharp. Each of the two segments' ground lines runs straight from its far end to where the arc touches it, then along the arc to the arc's middle, where the two lines meet. Along the arc, the line is made of straight pieces, each within ARC_TOLERANCE of the arc. If the radius is 0, both lines end at the point. Where a point makes no corner, every segment at it ends at the point.

cornerRadiusFor gives the radius whose arc's middle lies a given distance from a corner's point, into the corner. It gives 0 for a distance of 0 or less.

## Paved surface

parkSurface gives the paved surface the park's segments make. It is a list of pieces and a list of edge lines, each belonging to a segment or a point, in ascending key order of what it belongs to. Each piece is a convex polygon on the ground, with its corners counter-clockwise seen from above. Each edge line runs along the surface's outline, with the paving on its left seen from above. A segment with an empty ground line adds nothing.

- A segment's body is its width wide and runs along the straight part of its ground line. At each end it stops where the surface at that point begins.
- At a point that only one segment uses, the body ends square at the point.
- At any other point, the outline turns from each segment's left side, seen walking away from the point, to the right side of the next segment counter-clockwise around the point. Each such turn is an edge. If the next segment lies less than half a turn around, the edge is an inner edge: an arc that touches both sides from outside the paving. If it lies more than half a turn around, the edge is an outer edge: an arc that touches both sides from inside the paving. An edge of radius 0 is sharp: the two sides run straight on until they meet.
- A segment's start at a point is where its ground line's arc touches it, at a corner, and the point itself elsewhere. An inner edge touches each segment between its start and its reach. An outer edge touches each segment no farther from the point than its start, and no farther behind the point than the wider of the two widths. The radius is the one asked for, made larger or smaller as little as needed to fit. If no radius fits, the outline steps straight across at the point, from one side to the other.
- At a corner, the inside edge's radius is the corner's radius less half the narrower width, or 0 if that is less. Every other edge's radius is the edge radius held by the segment it starts from, at that end, or 0 if that is below zero or not finite. A corner's outside edge is one of these, so it curves apart from the corner's radius. The edge radius held for a corner's inside edge is kept, and used if the point stops being a corner.
- Where two segments at a point run along one line, the edge between them has no radius. The outline runs around the point from one side to the other at the narrower half width, and the wider segment's end steps in to it.
- Each side of a segment stops where the edge on that side touches it, or at the point if the edge touches behind the point or steps across it.
- At a corner, the paving between the two segments' bodies belongs to the two segments, divided along the line into the corner. At any other point that two or more segments use, it belongs to the point.
- A segment's reach at one end is its whole straight length if no other segment uses its other end, and half of it otherwise.
- Along curves, the outline is made of straight pieces, each within ARC_TOLERANCE of its curve.

Edge lines run along each side of each segment, across each square end, and around each point from each segment to the next. They belong to the point they run around. No edge line runs where a body meets a corner's half or a junction. Segments are joined when a chain of shared points links them. Where an edge line lies inside the paving of segments joined to its own, that part of it is hidden. Where segments that aren't joined overlap, each one's edge lines run across the other.

parkEdges gives each edge that has a radius, other than a corner's inside edge, in ascending key order of its point, then of the segment whose left side it starts from. A ParkEdge has the point's key, that segment's key, the unit direction along each of the two segments away from the point, that segment's first, whether it is an inner edge, the radius it is rounded with, and the smallest and largest radii that fit. It also has where its curve's middle lies, where the two sides meet, and the unit direction from there in which the middle moves as the radius grows. edgeRadiusFor gives the radius whose middle lies a given distance from where the sides meet, in that direction. It gives 0 for a distance of 0 or less.

## Footprints

footprintOf returns the rectangle a pose covers for a size: its width across the facing and its depth along it. It returns none when the pose is not finite or its facing has zero length. Its corners run front left, front right, back right, back left: the front face runs from the first corner to the second, and the back face from the third to the fourth.

## Physical validity

A world is physically valid when no object leaves the park, no two solids overlap, and no segment meets a solid. Solids are the footprints of the entrance and the boxes. Paths occupy the paved surface. A footprint meets a path when it overlaps one of the surface's pieces by more than CONTACT_TOLERANCE. A segment leaves the park when its ground line comes nearer the park's edge than half its width, or when a piece of the surface it or its points own reaches past the park's edge by more than CONTACT_TOLERANCE.

If objects only touch, within CONTACT_TOLERANCE, they never conflict, whatever rounding their facings bring. Segments never conflict with each other. Where they cross, routes derives a junction. If an entrance or box has no footprint, or a segment has an empty ground line, the world is invalid. A path point that no segment uses doesn't affect validity. Only a hand-written save can hold such an object. isPhysicallyValid returns whether a world is valid.

A segment's ground line can change when an edit changes a corner at its end, even when the edit leaves the segment's own points in place. A deletion or an insertion can make a corner, remove one, or change how far an arc may reach. The check covers every ground line an edit changes.

## Commands

Eleven commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, DeletePoint, SetCornerRadius, SetSegmentWidth, and SetEdgeRadius. No command moves or deletes the entrance. Tools submit commands to the command queue, so they apply between ticks.

AddPath draws a path of one kind through a list of stops, in order. A stop is a new point at a ground position (NewPoint), an existing path point (OnPoint), a place on an existing segment (OnSegment), given as the fraction of the way from the segment's From point to its To point, the point an earlier stop of the same path makes (RepeatStop), as when a path closes a loop on its own start, or a place on one of the path's own earlier segments (OnDrawnSegment), given as the fraction of the way between that segment's two stops. stopPositions gives each stop's ground position, and stopPosition gives that of a NewPoint, an OnPoint, or an OnSegment.

MovePoint moves a path point to a target: a new position (NewPoint), another path point (OnPoint), or a place on a segment (OnSegment), given as for AddPath's stops. InsertPoint names a segment and a fraction of the way from its From point to its To point. DeletePoint names a path point. A segment's far end from a point is its other end.

SetCornerRadius names a path point and a radius. SetSegmentWidth names a segment and a width. SetEdgeRadius names a segment, one of its two points, and a radius.

On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. For AddPath, that means it has at least two stops, each OnPoint names a path point that at least one segment uses and every segment at it has the path's kind, each OnSegment names a segment of the path's kind at a fraction strictly between 0 and 1, each RepeatStop names an earlier stop, each OnDrawnSegment names a segment between two earlier stops at a fraction strictly between 0 and 1, and every segment it adds or splits has a ground line. For MovePoint, it means at least one segment uses the point and all of them have one kind, an OnPoint target names another path point of that kind, an OnSegment target names a segment of that kind that doesn't end at the point, at a fraction strictly between 0 and 1, and every segment it moves, splits, or adds has a ground line. For InsertPoint, it means the segment has a ground line, the fraction lies strictly between 0 and 1, and both parts have ground lines. For DeletePoint, it means the key holds a path point, and a segment it merges has a ground line, as does every part that segment splits into. For SetCornerRadius, it means the key holds a path point and the radius is finite and at least 0. For SetSegmentWidth, it means the key holds a segment with a ground line and the width is finite and above zero. For SetEdgeRadius, it means the key holds a segment with a ground line, the point is one of its two points, and the radius is finite and at least 0. For every command, the world it describes is physically valid when every ground line the command adds or changes, including one whose corner it changes, is not empty and stays in the park, and no piece of the surface the command adds or changes meets a solid or leaves the park. So a DeleteSegment, DeletePoint, or InsertPoint is refused if a corner it changes would round into a solid. isAccepted returns whether a command would be accepted. If the command is not accepted, applyCommand changes nothing. A ghost shown as valid always matches what its commit does.

If a command is accepted, it leaves exactly the intent it describes and changes nothing else:

- AddPath adds a point at each NewPoint, OnSegment, and OnDrawnSegment stop, with DEFAULT_CORNER_RADIUS. A RepeatStop uses its earlier stop's point. It adds a segment from each stop's point to the next, of the path's kind and pathWidth, with DEFAULT_EDGE_RADIUS at both ends. Each segment that OnSegment stops name splits at their points, in order of fraction. The segment keeps its key, kind, width, and edge radius at its From end, and now ends at its first split point, with DEFAULT_EDGE_RADIUS there. A new segment of the same kind and width runs on from each split point to the next, and the last ends at the segment's old To point, with the segment's old edge radius there. Each other end of a new part has DEFAULT_EDGE_RADIUS. Each of the path's own segments that OnDrawnSegment stops name runs through their points in order of fraction, as a run of segments.
- AddBox adds one box.
- MoveBox replaces a box's pose and keeps its key and kind.
- DeleteSegment destroys the segment, and each of its points that no other segment uses.
- DeleteBox destroys the box.
- MovePoint to a NewPoint or OnSegment target moves the point there and keeps its key and radius. Every segment at it follows and keeps its key, kind, width, and edge radii. An OnSegment target's segment splits at the point, as for an OnSegment stop.
- MovePoint to an OnPoint target destroys the moved point. Every segment at it ends at the target point instead and keeps its key, kind, width, and edge radii. A segment between the two points is destroyed. If no segment then uses the target point, it is destroyed too.
- InsertPoint adds a point at the place, with DEFAULT_CORNER_RADIUS, and the segment splits there, as for an OnSegment stop.
- DeletePoint destroys the point. If exactly two segments use it, and their far ends are different points, it merges them. The one with the lower key keeps its key, kind, and far end, and takes the wider of the two widths. Its end at the point moves to the other segment's far end, with the other segment's edge radius there, and the other segment is destroyed. Otherwise, DeletePoint destroys every segment at the point, and each of their far ends that no other segment uses.
- SetCornerRadius sets the point's radius, as given, and changes nothing else. A radius larger than fits is kept, and the corner is rounded with the largest that fits.
- SetSegmentWidth sets the segment's width, as given, and changes nothing else.
- SetEdgeRadius sets the segment's edge radius at the point, as given, and changes nothing else. A radius that doesn't fit is kept, and the edge is rounded with the nearest that fits.

Where a segment that AddPath adds, MovePoint moves, or DeletePoint merges meets another segment of its kind, the two join. The other segment can be one the command leaves in place, or another one the command adds or moves.

- If each end of each segment lies within MIN_SEGMENT_LENGTH of the line through the other, the segments lie along one line. They don't meet, and neither splits.
- Otherwise, if an end of one segment lies within MIN_SEGMENT_LENGTH of the other segment, and farther than MIN_SEGMENT_LENGTH from both of the other's ends, the other segment splits at that end's point.
- Otherwise, if the segments cross, the command adds a point at the crossing, with DEFAULT_CORNER_RADIUS, and both split there. If the crossing lies within MIN_SEGMENT_LENGTH of a point the command already places, as where a path doubles back across a segment, both split at that point instead.

If they meet within MIN_SEGMENT_LENGTH of an end of each, nothing splits. A segment that meets several others splits at each of their points, in order along it, together with any OnSegment or OnDrawnSegment stops or OnSegment target on it, as described above. pathCrossings gives the ground position of each point where AddPath, MovePoint, or DeletePoint splits a segment this way, other than at an OnSegment or OnDrawnSegment stop or a MovePoint's OnSegment target. It gives none if the world refuses the command.

Joins are found along the straight line between each segment's two points, not along its rounded ground line. Near a rounded corner, a path can cross or end on the arc without joining it.

editedSegments gives each segment an edit adds, or whose ground line or width it changes, as it would lie after the edit, in ascending key order. A new segment has the key the commit would give it. The parts of a segment the edit splits are among them, and so is a segment whose corner the edit changes. It gives them whether or not the world refuses the edit for meeting a solid. It gives none when the edit doesn't name what it acts on, when a segment the edit adds or reshapes has no ground line, and for edits of boxes.

editedSurface gives the pieces and edge lines of each segment and point whose part of the surface an edit changes, as they would lie after the edit, in ascending key order of the segment or point they belong to. It gives none when the edit doesn't name what it acts on, when a segment the edit adds or reshapes has no ground line, and for edits of boxes.

Everything a command adds takes a key from the key counter. A pose or position is kept exactly as given.

A ParkEdit holds any one of the eleven commands. It is the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on its command. queueEdit pushes the command, so a cycle applies it as if it were pushed directly.

## The new park

makeNewPark(schema, seed) returns a physically valid world at tick 0, pending resolution. The world holds an entrance with its back on the park's edge, facing in, and a short guest path in front of it: two path points and the segment between them. tests/parks/new.park is the save of makeNewPark(seed) with seed 1.
