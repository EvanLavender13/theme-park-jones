# park

This module holds park intent: what the player authored, from which the rest of the park is derived (principle 1, decision 0025). It also makes the game's one refusal, physical validity (principle 5). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

addParkIntent registers the intent types entrance, path-point, path-segment, and box. Their components are private to this module (decision 0016). addParkEdits registers the five commands. makeParkSchema calls both after the medium's types.

Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, and parkBoxes. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, and ParkBox. Other modules change intent only through commands.

segmentLine, stopPosition, stopPositions, pathCrossings, footprintOf, and the sizes pathWidth, MIN_SEGMENT_LENGTH, DEFAULT_CORNER_RADIUS, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so that the renderer, the tools, and routes measure the same shapes this module validates.

## Intent

The park is a square PARK_SIZE on a side, centered on the origin of the ground.

- A path point has a ground position and a corner radius. The radius is for the corner the point makes when exactly two segments meet there. Nothing uses it yet.
- A path segment has a kind, guest or backstage, its two path points, From and To, and a width. Segments share a point where the player joined them, and where a path was drawn across them. Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md).
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

A segment's ground line is the straight line from its From point to its To point, as two CarrierPoints (sim/medium/SPEC.md), the first at distance 0 and the second at the segment's length. parkSegments gives each segment with its ground line, and segmentLine gives the ground line between any two positions. The renderer, the validity check, and routes all use it, so what is drawn, what is refused, and what is walked are the same line.

A ground line is empty when either position is not finite or lies outside the park, or the two lie closer together than MIN_SEGMENT_LENGTH. A segment's ground line is also empty when it names a key that holds no path point, names the same point twice, or has a width that is not finite and above zero.

## Footprints

footprintOf returns the rectangle a pose covers for a size: its width across the facing and its depth along it. It returns none when the pose is not finite or its facing has zero length. Its corners run front left, front right, back right, back left: the front face runs from the first corner to the second, and the back face from the third to the fourth.

## Physical validity

A world is physically valid when no object leaves the park, no two solids overlap, and no segment meets a solid. Solids are the footprints of the entrance and the boxes. A segment occupies its ground line widened by half its width, with round ends.

If objects only touch, within CONTACT_TOLERANCE, they never conflict, whatever rounding their facings bring. Segments never conflict with each other. Where they cross, routes derives a junction. If an entrance or box has no footprint, or a segment has an empty ground line, the world is invalid. A path point that no segment uses doesn't affect validity. Only a hand-written save can hold such an object. isPhysicallyValid returns whether a world is valid.

## Commands

Five commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, and DeleteBox. No command moves or deletes the entrance. Tools submit commands to the command queue, so they apply between ticks.

AddPath draws a path of one kind through a list of stops, in order. A stop is a new point at a ground position (NewPoint), an existing path point (OnPoint), a place on an existing segment (OnSegment), given as the fraction of the way from the segment's From point to its To point, the point an earlier stop of the same path makes (RepeatStop), as when a path closes a loop on its own start, or a place on one of the path's own earlier segments (OnDrawnSegment), given as the fraction of the way between that segment's two stops. stopPositions gives each stop's ground position, and stopPosition gives that of a NewPoint, an OnPoint, or an OnSegment.

On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. For AddPath, that means it has at least two stops, each OnPoint names a path point that at least one segment uses and every segment at it has the path's kind, each OnSegment names a segment of the path's kind at a fraction strictly between 0 and 1, each RepeatStop names an earlier stop, each OnDrawnSegment names a segment between two earlier stops at a fraction strictly between 0 and 1, and every segment it adds or splits has a ground line. isAccepted returns whether a command would be accepted. If the command is not accepted, applyCommand changes nothing. A ghost shown as valid always matches what its commit does.

If a command is accepted, it leaves exactly the intent it describes and changes nothing else:

- AddPath adds a point at each NewPoint, OnSegment, and OnDrawnSegment stop, with DEFAULT_CORNER_RADIUS. A RepeatStop uses its earlier stop's point. It adds a segment from each stop's point to the next, of the path's kind and pathWidth. Each segment that OnSegment stops name splits at their points, in order of fraction. The segment keeps its key, kind, and width, and now ends at its first split point. A new segment of the same kind and width runs on from each split point to the next, and the last ends at the segment's old To point. Each of the path's own segments that OnDrawnSegment stops name runs through their points in order of fraction, as a run of segments.
- AddBox adds one box.
- MoveBox replaces a box's pose and keeps its key and kind.
- DeleteSegment destroys the segment, and each of its points that no other segment uses.
- DeleteBox destroys the box.

Where a segment AddPath adds meets another segment of its kind, the two join. The other segment can be one already in the park or another one the path adds.

- If each end of each segment lies within MIN_SEGMENT_LENGTH of the line through the other, the segments lie along one line. They don't meet, and neither splits.
- Otherwise, if an end of one segment lies within MIN_SEGMENT_LENGTH of the other segment, and farther than MIN_SEGMENT_LENGTH from both of the other's ends, the other segment splits at that end's point.
- Otherwise, if the segments cross, AddPath adds a point at the crossing, with DEFAULT_CORNER_RADIUS, and both split there.

If they meet within MIN_SEGMENT_LENGTH of an end of each, nothing splits. A segment that meets several others splits at each of their points, in order along it, together with any OnSegment or OnDrawnSegment stops on it, as described above. pathCrossings gives the ground position of each point where AddPath splits a segment this way, other than at an OnSegment or OnDrawnSegment stop. It gives none if the world refuses the path.

Everything a command adds takes a key from the key counter. A pose or position is kept exactly as given.

A ParkEdit holds any one of the five commands. It is the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on its command. queueEdit pushes the command, so a cycle applies it as if it were pushed directly.

## The new park

makeNewPark(schema, seed) returns a physically valid world at tick 0, pending resolution. The world holds an entrance with its back on the park's edge, facing in, and a short guest path in front of it: two path points and the segment between them. tests/parks/new.park is the save of makeNewPark(seed) with seed 1.
