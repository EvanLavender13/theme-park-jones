# park

This module holds park intent: what the player authored, from which the rest of the park is derived (principle 1, decision 0025). It also makes the game's one refusal, physical validity (principle 5). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

addParkIntent registers the intent types entrance, path-point, path-segment, and box. Their components are private to this module (decision 0016). addParkEdits registers the ten commands. makeParkSchema calls both after the medium's types.

Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, parkCorners, and parkBoxes. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, ParkCorner, and ParkBox. Other modules change intent only through commands.

segmentLine, cornerRadiusFor, stopPosition, stopPositions, pathCrossings, editedSegments, footprintOf, and the sizes pathWidth, MIN_SEGMENT_LENGTH, ARC_TOLERANCE, DEFAULT_CORNER_RADIUS, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so that the renderer, the tools, and routes measure the same shapes this module validates.

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

## Footprints

footprintOf returns the rectangle a pose covers for a size: its width across the facing and its depth along it. It returns none when the pose is not finite or its facing has zero length. Its corners run front left, front right, back right, back left: the front face runs from the first corner to the second, and the back face from the third to the fourth.

## Physical validity

A world is physically valid when no object leaves the park, no two solids overlap, and no segment meets a solid. Solids are the footprints of the entrance and the boxes. A segment occupies its ground line widened by half its width, with round ends.

If objects only touch, within CONTACT_TOLERANCE, they never conflict, whatever rounding their facings bring. Segments never conflict with each other. Where they cross, routes derives a junction. If an entrance or box has no footprint, or a segment has an empty ground line, the world is invalid. A path point that no segment uses doesn't affect validity. Only a hand-written save can hold such an object. isPhysicallyValid returns whether a world is valid.

A segment's ground line can change when an edit changes a corner at its end, even when the edit leaves the segment's own points in place. A deletion or an insertion can make a corner, remove one, or change how far an arc may reach. The check covers every ground line an edit changes.

## Commands

Ten commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, DeletePoint, SetCornerRadius, and SetSegmentWidth. No command moves or deletes the entrance. Tools submit commands to the command queue, so they apply between ticks.

AddPath draws a path of one kind through a list of stops, in order. A stop is a new point at a ground position (NewPoint), an existing path point (OnPoint), a place on an existing segment (OnSegment), given as the fraction of the way from the segment's From point to its To point, the point an earlier stop of the same path makes (RepeatStop), as when a path closes a loop on its own start, or a place on one of the path's own earlier segments (OnDrawnSegment), given as the fraction of the way between that segment's two stops. stopPositions gives each stop's ground position, and stopPosition gives that of a NewPoint, an OnPoint, or an OnSegment.

MovePoint moves a path point to a target: a new position (NewPoint), another path point (OnPoint), or a place on a segment (OnSegment), given as for AddPath's stops. InsertPoint names a segment and a fraction of the way from its From point to its To point. DeletePoint names a path point. A segment's far end from a point is its other end.

SetCornerRadius names a path point and a radius. SetSegmentWidth names a segment and a width.

On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. For AddPath, that means it has at least two stops, each OnPoint names a path point that at least one segment uses and every segment at it has the path's kind, each OnSegment names a segment of the path's kind at a fraction strictly between 0 and 1, each RepeatStop names an earlier stop, each OnDrawnSegment names a segment between two earlier stops at a fraction strictly between 0 and 1, and every segment it adds or splits has a ground line. For MovePoint, it means at least one segment uses the point and all of them have one kind, an OnPoint target names another path point of that kind, an OnSegment target names a segment of that kind that doesn't end at the point, at a fraction strictly between 0 and 1, and every segment it moves, splits, or adds has a ground line. For InsertPoint, it means the segment has a ground line, the fraction lies strictly between 0 and 1, and both parts have ground lines. For DeletePoint, it means the key holds a path point, and a segment it merges has a ground line, as does every part that segment splits into. For SetCornerRadius, it means the key holds a path point and the radius is finite and at least 0. For SetSegmentWidth, it means the key holds a segment with a ground line and the width is finite and above zero. For every command, the world it describes is physically valid when every ground line the command adds or changes, including one whose corner it changes, is not empty, stays in the park, and meets no solid. So a DeleteSegment, DeletePoint, or InsertPoint is refused if a corner it changes would round into a solid. isAccepted returns whether a command would be accepted. If the command is not accepted, applyCommand changes nothing. A ghost shown as valid always matches what its commit does.

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

Everything a command adds takes a key from the key counter. A pose or position is kept exactly as given.

A ParkEdit holds any one of the ten commands. It is the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on its command. queueEdit pushes the command, so a cycle applies it as if it were pushed directly.

## The new park

makeNewPark(schema, seed) returns a physically valid world at tick 0, pending resolution. The world holds an entrance with its back on the park's edge, facing in, and a short guest path in front of it: two path points and the segment between them. tests/parks/new.park is the save of makeNewPark(seed) with seed 1.
