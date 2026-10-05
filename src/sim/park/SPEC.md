# park

This module holds park intent: what the player authored, from which the rest of the park is derived (principle 1, decision 0025). It also makes the game's one refusal, physical validity (principle 5). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

addParkIntent registers the intent types entrance, path-point, path-segment, and box. Their components are private to this module (decision 0016). addParkEdits registers the ten commands. makeParkSchema calls both after the medium's types.

Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, parkCorners, and parkBoxes, and the paved surface through parkSurface. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, ParkCorner, and ParkBox. Other modules change intent only through commands.

segmentLine, cornerRadiusFor, stopPosition, stopPositions, pathCrossings, editedSegments, editedSurface, footprintOf, and the sizes pathWidth, MIN_SEGMENT_LENGTH, ARC_TOLERANCE, DEFAULT_CORNER_RADIUS, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so that the renderer, the tools, and routes measure the same shapes this module validates.

## Intent

The park is a square PARK_SIZE on a side, centered on the origin of the ground.

- A path point has a ground position and a corner radius. The radius is for the corner the point makes when exactly two segments meet there.
- A path segment has a kind, guest or backstage, its two path points, From and To, and a width. Segments share a point where the player joined them, and where a path was drawn or moved across them. Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md).
- A box has a kind, shop or depot, and a pose.
- An entrance has a pose.

A Pose is a position and a facing on the ground, held as given. A default Pose is at the origin, facing -z.

The save lines hold:

- an entrance's x, z, facing-x, and facing-z;
- a path point's x, z, and radius;
- a path segment's kind, from, to, and width, from and to being its points' keys;
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
- A corner's rounding belongs half to each of its two segments, split at the arc's middle. Around the arc, the paving takes the narrower of the two half widths, and the wider segment's end steps in to it. Its outer edge follows the arc that far outside it. If the radius is at least that half width, its inner edge follows the arc that far inside it. Otherwise the corner is sharp on the inside: the two inner edges run straight on until they meet. If one segment's inner edge would first meet the other's square end at the point, it runs to that end instead. If neither fits within the reach of each segment, the inner edge steps straight across at the point, from one segment's inner edge to the other's.
- Any other point that segments use is a junction, which belongs to the point. Between each segment and the next one around the point, the inside corner is rounded with an arc that touches both segments' facing edges, with a radius of half the narrower width. Where the two segments are less than a quarter turn apart, the radius is made smaller, so that the arc reaches no farther past where the edges meet than its radius. Each segment stops where the farther of its two arcs touches it, within its reach. If an arc doesn't fit, it is made smaller. If no arc fits, the outline steps straight across from one edge to the other at the point. If the next segment lies half a turn or more around, the outline runs around the point from one edge to the other instead, at the narrower half width, and the wider segment's end steps in to it.
- A segment's reach at one end is its whole straight length if no other segment uses its other end, and half of it otherwise.
- Along curves, the outline is made of straight pieces, each within ARC_TOLERANCE of its curve.

Edge lines run along each side of each segment, across each square end, along a corner's inner and outer edges, and around a junction from one segment to the next. They belong to the point they run around. No edge line runs where a body meets a corner's half or a junction. Segments are joined when a chain of shared points links them. Where an edge line lies inside the paving of segments joined to its own, that part of it is hidden. Where segments that aren't joined overlap, each one's edge lines run across the other.

## Footprints

footprintOf returns the rectangle a pose covers for a size: its width across the facing and its depth along it. It returns none when the pose is not finite or its facing has zero length. Its corners run front left, front right, back right, back left: the front face runs from the first corner to the second, and the back face from the third to the fourth.

## Physical validity

A world is physically valid when no object leaves the park, no two solids overlap, and no segment meets a solid. Solids are the footprints of the entrance and the boxes. Paths occupy the paved surface. A footprint meets a path when it overlaps one of the surface's pieces by more than CONTACT_TOLERANCE. A segment leaves the park when its ground line comes nearer the park's edge than half its width.

If objects only touch, within CONTACT_TOLERANCE, they never conflict, whatever rounding their facings bring. Segments never conflict with each other. Where they cross, routes derives a junction. If an entrance or box has no footprint, or a segment has an empty ground line, the world is invalid. A path point that no segment uses doesn't affect validity. Only a hand-written save can hold such an object. isPhysicallyValid returns whether a world is valid.

A segment's ground line can change when an edit changes a corner at its end, even when the edit leaves the segment's own points in place. A deletion or an insertion can make a corner, remove one, or change how far an arc may reach. The check covers every ground line an edit changes.

## Commands

Ten commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, DeletePoint, SetCornerRadius, and SetSegmentWidth. No command moves or deletes the entrance. Tools submit commands to the command queue, so they apply between ticks.

AddPath draws a path of one kind through a list of stops, in order. A stop is a new point at a ground position (NewPoint), an existing path point (OnPoint), a place on an existing segment (OnSegment), given as the fraction of the way from the segment's From point to its To point, the point an earlier stop of the same path makes (RepeatStop), as when a path closes a loop on its own start, or a place on one of the path's own earlier segments (OnDrawnSegment), given as the fraction of the way between that segment's two stops. stopPositions gives each stop's ground position, and stopPosition gives that of a NewPoint, an OnPoint, or an OnSegment.

MovePoint moves a path point to a target: a new position (NewPoint), another path point (OnPoint), or a place on a segment (OnSegment), given as for AddPath's stops. InsertPoint names a segment and a fraction of the way from its From point to its To point. DeletePoint names a path point. A segment's far end from a point is its other end.

SetCornerRadius names a path point and a radius. SetSegmentWidth names a segment and a width.

On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. For AddPath, that means it has at least two stops, each OnPoint names a path point that at least one segment uses and every segment at it has the path's kind, each OnSegment names a segment of the path's kind at a fraction strictly between 0 and 1, each RepeatStop names an earlier stop, each OnDrawnSegment names a segment between two earlier stops at a fraction strictly between 0 and 1, and every segment it adds or splits has a ground line. For MovePoint, it means at least one segment uses the point and all of them have one kind, an OnPoint target names another path point of that kind, an OnSegment target names a segment of that kind that doesn't end at the point, at a fraction strictly between 0 and 1, and every segment it moves, splits, or adds has a ground line. For InsertPoint, it means the segment has a ground line, the fraction lies strictly between 0 and 1, and both parts have ground lines. For DeletePoint, it means the key holds a path point, and a segment it merges has a ground line, as does every part that segment splits into. For SetCornerRadius, it means the key holds a path point and the radius is finite and at least 0. For SetSegmentWidth, it means the key holds a segment with a ground line and the width is finite and above zero. For every command, the world it describes is physically valid when every ground line the command adds or changes, including one whose corner it changes, is not empty and stays in the park, and no piece of the surface the command adds or changes meets a solid. So a DeleteSegment, DeletePoint, or InsertPoint is refused if a corner it changes would round into a solid. isAccepted returns whether a command would be accepted. If the command is not accepted, applyCommand changes nothing. A ghost shown as valid always matches what its commit does.

If a command is accepted, it leaves exactly the intent it describes and changes nothing else:

- AddPath adds a point at each NewPoint, OnSegment, and OnDrawnSegment stop, with DEFAULT_CORNER_RADIUS. A RepeatStop uses its earlier stop's point. It adds a segment from each stop's point to the next, of the path's kind and pathWidth. Each segment that OnSegment stops name splits at their points, in order of fraction. The segment keeps its key, kind, and width, and now ends at its first split point. A new segment of the same kind and width runs on from each split point to the next, and the last ends at the segment's old To point. Each of the path's own segments that OnDrawnSegment stops name runs through their points in order of fraction, as a run of segments.
- AddBox adds one box.
- MoveBox replaces a box's pose and keeps its key and kind.
- DeleteSegment destroys the segment, and each of its points that no other segment uses.
- DeleteBox destroys the box.
- MovePoint to a NewPoint or OnSegment target moves the point there and keeps its key and radius. Every segment at it follows and keeps its key, kind, and width. An OnSegment target's segment splits at the point, as for an OnSegment stop.
- MovePoint to an OnPoint target destroys the moved point. Every segment at it ends at the target point instead and keeps its key, kind, and width. A segment between the two points is destroyed. If no segment then uses the target point, it is destroyed too.
- InsertPoint adds a point at the place, with DEFAULT_CORNER_RADIUS, and the segment splits there, as for an OnSegment stop.
- DeletePoint destroys the point. If exactly two segments use it, and their far ends are different points, it merges them. The one with the lower key keeps its key, kind, and far end, and takes the wider of the two widths. Its end at the point moves to the other segment's far end, and the other segment is destroyed. Otherwise, DeletePoint destroys every segment at the point, and each of their far ends that no other segment uses.
- SetCornerRadius sets the point's radius, as given, and changes nothing else. A radius larger than fits is kept, and the corner is rounded with the largest that fits.
- SetSegmentWidth sets the segment's width, as given, and changes nothing else.

Where a segment that AddPath adds, MovePoint moves, or DeletePoint merges meets another segment of its kind, the two join. The other segment can be one the command leaves in place, or another one the command adds or moves.

- If each end of each segment lies within MIN_SEGMENT_LENGTH of the line through the other, the segments lie along one line. They don't meet, and neither splits.
- Otherwise, if an end of one segment lies within MIN_SEGMENT_LENGTH of the other segment, and farther than MIN_SEGMENT_LENGTH from both of the other's ends, the other segment splits at that end's point.
- Otherwise, if the segments cross, the command adds a point at the crossing, with DEFAULT_CORNER_RADIUS, and both split there. If the crossing lies within MIN_SEGMENT_LENGTH of a point the command already places, as where a path doubles back across a segment, both split at that point instead.

If they meet within MIN_SEGMENT_LENGTH of an end of each, nothing splits. A segment that meets several others splits at each of their points, in order along it, together with any OnSegment or OnDrawnSegment stops or OnSegment target on it, as described above. pathCrossings gives the ground position of each point where AddPath, MovePoint, or DeletePoint splits a segment this way, other than at an OnSegment or OnDrawnSegment stop or a MovePoint's OnSegment target. It gives none if the world refuses the command.

Joins are found along the straight line between each segment's two points, not along its rounded ground line. Near a rounded corner, a path can cross or end on the arc without joining it.

editedSegments gives each segment an edit adds, or whose ground line or width it changes, as it would lie after the edit, in ascending key order. A new segment has the key the commit would give it. The parts of a segment the edit splits are among them, and so is a segment whose corner the edit changes. It gives them whether or not the world refuses the edit for meeting a solid. It gives none when the edit doesn't name what it acts on, when a segment the edit adds or reshapes has no ground line, and for edits of boxes.

editedSurface gives the pieces and edge lines of each segment and point whose part of the surface an edit changes, as they would lie after the edit, in ascending key order of the segment or point they belong to. It gives none when editedSegments gives none.

Everything a command adds takes a key from the key counter. A pose or position is kept exactly as given.

A ParkEdit holds any one of the ten commands. It is the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on its command. queueEdit pushes the command, so a cycle applies it as if it were pushed directly.

## The new park

makeNewPark(schema, seed) returns a physically valid world at tick 0, pending resolution. The world holds an entrance with its back on the park's edge, facing in, and a short guest path in front of it: two path points and the segment between them. tests/parks/new.park is the save of makeNewPark(seed) with seed 1.
