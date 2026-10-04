# Feature: Park Routes Reshaped

## Summary

The park and routes specs drop their tuning numbers and the formulas copied from their code, keeping each constant's name and stating the property each formula computed. plans/focused-tests/RESEARCH.md lists 22 of their test cases as rewrite, and this feature adds 12 more whose setups were placed against the dropped numbers. The test-writer rewrites all 34 in rewrite mode, merged where the entries say, so each asserts its entry's property. edit_sequences_test.cpp goes once its one case is merged, the entries leave the inventory, and test support no remaining test uses is deleted.

## Acceptance criteria

1. The park's and routes' tests assert each of these properties once, in place of the cases the inventory lists for edit_sequences_test, edits_test, geometry_test, validity_test, connections_test, networks_test, and route_distance_test:
    - An accepted command leaves exactly the intent it describes. AddPath and AddBox each add one entity under the key counter's next key: a path of its kind through its kept points, or a box of its kind with its pose bit for bit. MoveBox replaces a box's pose bit for bit, keeping its key and kind. DeletePath and DeleteBox destroy the entity they name. Nothing else changes: other intent, the entities' keys, the seed, and the tick stay as they were, and the counter advances by one for an add only. Shown with an AddPath holding a repeated point and a point closer than MIN_POINT_SPACING to the last kept one, which are dropped, and with poses holding -0.0 and an unnormalized facing, which are kept bit for bit.
    - On a physically valid world, a command naming what it acts on is accepted exactly when the world it describes is physically valid, so every cycle leaves the park valid. An AddPath or AddBox whose kind is not a value of its enum is refused. Shown on a few commands, each chosen for a reason: one that fits; one that overlaps a box; a MoveBox onto part of its own old footprint, which it is judged without; and an AddBox whose kind is not an enum value, at a pose where a box of a real kind is accepted. Both outcomes occur.
    - groundLine is empty exactly when a point is not finite or lies outside the park's square, or fewer than two points are kept, and it never throws. Shown at the square's edge, which is inside, one representable step past it, a NaN, an infinity, and points all closer than MIN_POINT_SPACING to the first, with bounds taken from PARK_SIZE and MIN_POINT_SPACING by name.
    - groundLine depends only on keptPoints: the line of the points equals the line of their kept points. Each kept point lies at least MIN_POINT_SPACING from the last kept one. A point is dropped by its distance from the last kept point, not from the point before it, shown on points each closer than MIN_POINT_SPACING to the one before but drifting beyond it from the last kept one.
    - A ground line's distances start at 0 and strictly increase. Shown on a bend, a hairpin that doubles back, and two points MIN_POINT_SPACING apart.
    - footprintOf's corners are front left, front right, back right, and back left: the position plus half the depth along Forward at the front and minus it at the back, and plus half the width along Right on the right and minus it on the left, with Right (-Forward.Z, Forward.X). Shown on the default pose, which faces -z, and on one pose off the axes, with sizes from boxSize and ENTRANCE_SIZE by name.
    - Objects that only touch never conflict, and objects that interpenetrate by more than CONTACT_TOLERANCE always do. This covers solid against solid, line against solid, solid against the park's edge, and line against the edge, with offsets computed from boxSize, pathWidth, ENTRANCE_SIZE, PARK_SIZE, and CONTACT_TOLERANCE, never literals. Among the inputs are a diagonal facing, whose normalization rounds; a solid's corner nearest the middle of a segment whose own points lie farther off, since a line is judged by its segments, not its points; and a curved path whose line swings past its clicked points to the edge, since the edge judges the line, not the points.
    - A world is not physically valid when it holds intent with no footprint or no ground line, shown with an entrance and a box whose facings have zero length and a path whose ground line is empty, one of each.
    - An entity none of whose doors has a connector leaves the networks as they are without it. Shown on a door just beyond CONNECTION_REACH of the nearest line, a door within JUNCTION_TOLERANCE of a line, a door with no line of the kind it serves, an entity with no footprint, and a door whose connector key a path of its kind holds. Each door is placed from boxSize, CONNECTION_REACH, and JUNCTION_TOLERANCE by name.
    - Every stop of a path carrier other than its first and last lies within JUNCTION_TOLERANCE of a meeting's or a connection's distance on that carrier. Shown on parks of paths alone, a path crossing itself and a collinear overlap among them, and on parks with connectors.
    - Among steps achieving a node's route distance, Next is the least by carrier key, then toward lower distances before higher, then by lowest From. Shown on one network with a tie at every level: carriers whose least key walks toward higher distances, one carrier giving a node steps in both directions with the lower-direction one from a higher stop, and one carrier giving a node two lower-direction steps from different stops.
    - sampleRouteEdge gives one entry: the least of each end's entries with that end's offset added, stepping along the edge toward that end. Shown with each end winning once, several entries at one end, and entries at one end only.
2. These cases, which the inventory did not list, keep their properties, with setups built from the constants by name rather than from the trimmed numbers: JUNCTION_TOLERANCE for meetings and doors placed within or beyond the tolerance, boxSize and ENTRANCE_SIZE for door positions, CONNECTION_REACH for lines within reach, and groundLine's own points for lines placed through another's line points. Positions and distances whose formula the spec no longer states, such as a door and a connector's reach, are compared within rounding.
    - networks_test's "Where two segments meet, each carrier has a stop within the tolerance of the meeting's distance on it, and the two stops are at one node".
    - networks_test's "Consecutive stops of a carrier lie more than the tolerance apart".
    - networks_test's "Nodes are numbered in order of first stop, walking carriers in key order and each carrier's stops in ascending distance".
    - connections_test's "Each kind's network has its paths as carriers and, besides them, exactly one carrier for each door serving that kind with a connector, keyed connectorKey of its entity and face". Its parks include a door flush against a line of a kind it serves, its face touching the line's edge, which has a connector, since CONNECTION_REACH exceeds half of every pathWidth.
    - connections_test's "A connector has two points, its door at distance 0 and its connection point at the reach, the straight distance between them, more than the tolerance and at most CONNECTION_REACH".
    - connections_test's "A connector's first stop is at a node no other stop of its network has".
    - connections_test's "A connector stops only at its door and its connection, whose node is its path's stop within the tolerance of the connection place".
    - connections_test's "A network's anchors are exactly its connectors' door nodes, each anchored to its door's entity".
    - previous_networks_test's "A finisher registered between addRoutes and addDropPreviousNetworks reads, in each resolution after a world's first, each kind's network as parkNetwork gave it before that resolution", whose shop is placed so that its doors reach both lines.
    - route_distance_test's "sampleField of a kind's route distance field at a node gives each source's routeEntries entry there, in source order, and none for a source that cannot reach it" and "Following Next from any place with a source's entry, sampling at each step, reaches a node the source anchors, and the lengths walked add up to the sampled distance exactly", whose separated park's lines are placed within CONNECTION_REACH of their doors.
    - route_distance_test's "Every world a random edit sequence reaches resolves route distance without throwing, including a lone path, an unconnected box, and an unreachable source", whose opening shop is placed within CONNECTION_REACH of its line.
3. tests/sim/park/edit_sequences_test.cpp is deleted, its one case merged into the acceptance property of criterion 1.
4. src/sim/park/SPEC.md and src/sim/routes/SPEC.md read as Spec changes gives. Each changed statement is asserted by a test:
    - the square's bounds and the drop rule by criterion 1's groundLine and keptPoints properties;
    - the sizes by kind, the tolerance, the overlap axes, and a line's distance to a rectangle by criterion 1's touching property;
    - that the line passes through every kept point bit for bit by geometry_test's "A ground line passes through every kept point bit for bit, in order, from distance 0 to its last point";
    - that a two-point line is straight, and that distances add each step's straight length, by criterion 2's meeting cases, whose expected meetings are ground distances along straight lines;
    - Forward by geometry_test's "footprintOf's forward is the unit vector in the facing's direction";
    - crossings, end meetings, and their distances by criterion 2's meeting cases;
    - the door, the reach, and CONNECTION_REACH exceeding half of every pathWidth by criterion 2's connector cases.
5. The 12 cases of criterion 2 are added to plans/focused-tests/RESEARCH.md before the test pass, and every sim/park and sim/routes entry, those 12 included, is removed from it after.
6. No definition in tests/sim/support/park_worlds.h, same_networks.h, or route_edits.h is left that no test uses, directly or through another definition in its header.
7. Nothing under src/ changes except src/sim/park/SPEC.md and src/sim/routes/SPEC.md, and tpj_scenarios's windows-debug output is identical before and after.

## Medium

None. The feature changes tests and two specs. It samples, emits, draws, and supplies no fields or flows. Each rewritten test reaches park and routes through their public headers. The world-as-value standards edit_sequences_test re-proved stay the sim root's, proven once by the shared world checks (rule 2).

## Spec changes

src/sim/park/SPEC.md:

- "The park is a square PARK_SIZE, 256 m, on a side, centered on the origin of the ground, so every x and z inside it lies from -128 to 128 inclusive." becomes "The park is a square PARK_SIZE on a side, centered on the origin of the ground, so every x and z inside it lies from -PARK_SIZE / 2 to PARK_SIZE / 2 inclusive."
- "Sizes are fixed. pathWidth is 3 m for a guest path and 2 m for a backstage path. A footprint's width lies across its facing and its depth along it. boxSize is 8 m wide by 6 m deep for a shop and 12 m by 8 m for a depot, and ENTRANCE_SIZE is 10 m by 3 m." becomes "Sizes are fixed by kind: pathWidth gives a path's width, boxSize a box's width and depth, and ENTRANCE_SIZE the entrance's. A footprint's width lies across its facing and its depth along it."
- "dropping each point closer to the last kept point than MIN_POINT_SPACING, 1 cm: that is, one for which dx * dx + dz * dz is below MIN_POINT_SPACING * MIN_POINT_SPACING." becomes "dropping each point closer to the last kept point than MIN_POINT_SPACING."
- The paragraph beginning "The curve through the kept points K0 to Kn-1", the six-line pyramid block after it, and the paragraph beginning "Each segment, with c the chord" become one paragraph: "The curve through the kept points is the centripetal Catmull-Rom spline. Beyond each end it uses a phantom point that continues the end's chord straight on, so a path of two kept points is a straight line. Each segment between consecutive kept points contributes the first of them, exactly, and then points on the curve between them. The last kept point follows the last segment, exactly. So the line passes through every kept point bit for bit."
- "each later point's is the previous distance plus sqrt(dx * dx + dz * dz) of the step from the previous point." becomes "each later point's is the previous distance plus the straight length of the step from the previous point."
- "Forward is the facing normalized: both components divided by the larger of their magnitudes, and then by the square root of the sum of their squares." becomes "Forward is the unit vector in the facing's direction, for any finite facing of nonzero length, however large or small its components."
- "Every comparison allows CONTACT_TOLERANCE, 1 mm, so" becomes "Every comparison allows CONTACT_TOLERANCE, so".
- " A corner projects onto an axis as x * axis.X + z * axis.Z." is deleted.
- "The distance is measured in the rectangle's own frame, along Forward and Right from the pose's position: it is zero when the segment enters the rectangle, and otherwise the least of each end's distance to the rectangle and each corner's distance to the segment." becomes "The distance is the least between any point of the segment and any point of the rectangle, zero when the segment enters it."
- "so its back lies on the park's edge at z = 128 and" becomes "so its back lies on the park's edge and".

src/sim/routes/SPEC.md:

- " The side of a point p from the segment from a to b is (b.X - a.X) * (p.Z - a.Z) - (b.Z - a.Z) * (p.X - a.X)." is deleted.
- The paragraph beginning "Two segments, from a to b and from c to d, cross when" becomes "Two segments cross when the ends of each lie strictly on opposite sides of the line through the other. Crossing segments meet at their crossing point, at the distance on each that is the same fraction of the way between its two points' distances as the crossing point is along it, never outside them."
- In the paragraph beginning "Whether or not they cross", the text from its start through "at most JUNCTION_TOLERANCE squared." becomes "Whether or not they cross, two segments also meet at each of their four ends that lies within JUNCTION_TOLERANCE of its nearest point on the other segment. The meeting's distances are the end's own distance, and on the other segment the distance of that nearest point, the same fraction of the way between the segment's two distances as the point is along it. Where the nearest point is an end of the other segment, it is exactly that end, with its distance." The rest of the paragraph stays.
- ", each coordinate (first + second) / 2." becomes "."
- "and the reach is the straight distance from the door to it, sqrt(dx * dx + dz * dz)." becomes "and the reach is the straight distance from the door to it."
- "When the reach is at most CONNECTION_REACH, 4 m, and more than" becomes "When the reach is at most CONNECTION_REACH and more than".
- After "Otherwise the door has no connector, and nothing else about its entity changes (principle 2).", add "CONNECTION_REACH exceeds half of every pathWidth, so a door flush against a line of a kind it serves, its face touching the line's edge, has a connector."

## Files affected

- Modify: src/sim/park/SPEC.md, src/sim/routes/SPEC.md
- Modify: tests/sim/park/edits_test.cpp, geometry_test.cpp, validity_test.cpp; tests/sim/routes/connections_test.cpp, networks_test.cpp, previous_networks_test.cpp, route_distance_test.cpp (the test-writer)
- Delete: tests/sim/park/edit_sequences_test.cpp; Modify: tests/sim/CMakeLists.txt, to drop it
- Modify: tests/sim/support/park_worlds.h, same_networks.h, route_edits.h, for definitions no remaining test uses
- Modify: plans/focused-tests/RESEARCH.md

## Dependencies

medium-reshaped, merged: the medium's tests are in their final shape, so a routes rewrite that leans on carriers, places, and fields finds them settled.

## Out of scope

- The save lines, kind names, registration orders, networkKey and connectorKey, the corner order, the new park's coordinates, and route distance's statements, which stay as RESEARCH.md says.
- Unlisted cases in park and routes that place inputs loosely, far inside any tolerance or reach, which no change of a constant's value within reason breaks.
- The park files in tests/parks/, such as routes.park, which are content laid out for the current sizes and reach, not test setups.
- The touching property's segment-versus-points input needs ground line segments longer than a bound computed from pathWidth, CONTACT_TOLERANCE, and its inset, about 0.11 m today. No placement of a corner removes that, so the case REQUIREs the bound rather than passing silently if a retuning ever broke it.
- Tests in other modules that place boxes beside paths at literal gaps, which belong to those modules' features.

## Open questions

None.
