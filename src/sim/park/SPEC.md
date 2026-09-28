# park

Park intent: what the player authored, which the rest of the park is derived from (principle 1, decision 0025). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). The tools that change intent live outside tpj_sim, and change it only through commands.

## Intent

The park is a square PARK_SIZE, 256 m, on a side, centered on the origin of the ground, so every x and z inside it lies from -128 to 128 inclusive. A ParkPoint is an x and a z on the ground, in meters.

A path has a kind, PathKind guest or backstage, and its points, the positions the player clicked, in order. A box has a kind, BoxKind shop or depot, and a pose. An entrance has a pose. A Pose is a position, X and Z, and a facing, FacingX and FacingZ, a direction on the ground. The facing is held as given, not normalized; geometry normalizes it where it uses it. A default Pose is at the origin, facing -z.

Their component types are declared in internal/components.h, so they are private to this module (decision 0016). addParkIntent registers them as intent, in this order: entrance, whose save line holds x, z, facing-x, and facing-z; path, whose line holds kind and points, each point written as {x=... z=...}; and box, whose line holds kind, x, z, facing-x, and facing-z. The kinds are written guest, backstage, shop, and depot. makeParkSchema calls it after registering the medium's types.

Sizes are fixed. pathWidth is 3 m for a guest path and 2 m for a backstage path. A footprint's width lies across its facing and its depth along it. boxSize is 8 m wide by 6 m deep for a shop and 12 m by 8 m for a depot, and ENTRANCE_SIZE is 10 m by 3 m.

No other module names the components. parkEntrances, parkPaths, and parkBoxes give every entity holding each kind of intent, in ascending key order, as public values: ParkEntrance, a key and a pose; ParkPath, a key, a kind, and points; and ParkBox, a key, a kind, and a pose. Other modules read intent only through them, and change it only through commands.

## Ground lines

A path's ground line is its curve as a line of CarrierPoints (sim/medium/SPEC.md): positions on the ground, each with its distance along the line. It is a pure function of the path's points, and the renderer, the physical-validity check, and navigable-networks all use it, so what is drawn, refused, and walked is the same line.

groundLine is empty when any point has a coordinate that is not finite or outside the park's square. Otherwise it keeps the points in order, dropping each point closer to the last kept point than MIN_POINT_SPACING, 1 cm: that is, one for which dx * dx + dz * dz is below MIN_POINT_SPACING * MIN_POINT_SPACING. With fewer than two kept points the line is empty. keptPoints gives the points it keeps.

The curve through the kept points K0 to Kn-1 is the centripetal Catmull-Rom spline. Its segment from Ki to Ki+1 uses the four points Ki-1, Ki, Ki+1, and Ki+2, where the missing neighbor before K0 is the phantom 2 * K0 - K1, and the one after Kn-1 is 2 * Kn-1 - Kn-2, each coordinate computed in that form. The knots of the four points P0 to P3 are t0 = 0 and each next knot the previous plus the square root of the chord between the two points, where a chord is sqrt(dx * dx + dz * dz). The curve at t, from t1 to t2, is the Barry and Goldman pyramid:

    A1 = (t1 - t) / (t1 - t0) * P0 + (t - t0) / (t1 - t0) * P1
    A2 = (t2 - t) / (t2 - t1) * P1 + (t - t1) / (t2 - t1) * P2
    A3 = (t3 - t) / (t3 - t2) * P2 + (t - t2) / (t3 - t2) * P3
    B1 = (t2 - t) / (t2 - t0) * A1 + (t - t0) / (t2 - t0) * A2
    B2 = (t3 - t) / (t3 - t1) * A2 + (t - t1) / (t3 - t1) * A3
    C  = (t2 - t) / (t2 - t1) * B1 + (t - t1) / (t2 - t1) * B2

Each segment, with c the chord from Ki to Ki+1, contributes n = max(MIN_SEGMENT_SAMPLES, ceil(c / GROUND_LINE_SPACING)) points, with MIN_SEGMENT_SAMPLES 8 and GROUND_LINE_SPACING 1 m: Ki itself, exactly, and then the curve at t = t1 + (t2 - t1) * k / n for k from 1 to n - 1. The last kept point follows the last segment, exactly. So the line passes through every kept point bit for bit.

The first point's distance is 0, and each later point's is the previous distance plus sqrt(dx * dx + dz * dz) of the step from the previous point. Distances strictly increase. They measure the line, which the medium interpolates along, not the curve's exact arc length.

## Footprints

footprintOf gives a pose's footprint for a size: the rectangle the box or entrance covers. It gives none when the pose's position or facing has a component that is not finite, or its facing has zero length. Forward is the facing normalized: both components divided by the larger of their magnitudes, and then by the square root of the sum of their squares. Right is (-Forward.Z, Forward.X). The corners are, in order, front left, front right, back right, and back left: the position plus or minus Forward times half the depth, front plus and back minus, and plus or minus Right times half the width, right plus and left minus. The front face runs from the first corner to the second, and the back face from the third to the fourth.

## Physical validity

Physical validity is the only refusal (principle 5). The check compares solids, the footprint of each entrance and each box, from its pose and ENTRANCE_SIZE or its kind's boxSize, and lines, the ground line of each path with half its kind's pathWidth. Every comparison allows CONTACT_TOLERANCE, 1 mm, so objects that only touch never conflict, whatever rounding their facings bring.

- A solid leaves the park when a corner has an x or z of magnitude above PARK_SIZE / 2 + CONTACT_TOLERANCE.
- A line leaves the park when one of its points has an x or z of magnitude above (PARK_SIZE / 2 - its half width) + CONTACT_TOLERANCE. The line is straight between its points and the square is convex, so this is the line widened by its half width leaving the square.
- Two solids overlap when, on each of four axes, the Forward and Right of each, the intervals their corners project onto share more than CONTACT_TOLERANCE: the lesser of the two maxima minus the greater of the two minima exceeds it. A corner projects onto an axis as x * axis.X + z * axis.Z.
- A line meets a solid when some segment between consecutive points of it lies nearer the solid's rectangle than its half width less CONTACT_TOLERANCE. The distance is measured in the rectangle's own frame, along Forward and Right from the pose's position: it is zero when the segment enters the rectangle, and otherwise the least of each end's distance to the rectangle and each corner's distance to the segment.

Paths never conflict with each other: where they cross, navigable-networks derives a junction. A world is physically valid when every entrance and box has a footprint and every path a non-empty ground line, no solid or line leaves the park, no two solids overlap, and no line meets a solid. isPhysicallyValid gives whether it is.

## Commands

Five commands change intent: AddPath, a kind and points; AddBox, a kind and a pose; MoveBox, a box's key and a pose; DeletePath, a path's key; and DeleteBox, a box's key. addParkEdits registers them in that order, and makeParkSchema calls it after addParkIntent. Tools submit them to the command queue, so they apply between ticks (sim/SPEC.md).

isAccepted gives whether a command would be applied to a world. applyCommand calls it on the world it is given and changes nothing when it is false, so what a ghost shows as valid is what its commit does, as long as nothing else changes intent first. It changes only intent and the key counter: marking resolution pending is the cycle's and makeCandidate's (sim/SPEC.md).

- AddPath is accepted when the ground line of its kept points is not empty, does not leave the park, and meets no solid. It creates an entity from the key counter holding a path of its kind through its kept points. A path with a point that is not finite or outside the square, or fewer than two kept points, is refused.
- AddBox is accepted when its pose has a footprint for its kind's size, and that footprint does not leave the park, overlaps no solid, and meets no line. It creates an entity from the key counter holding a box of its kind with its pose exactly as given.
- MoveBox is accepted when its key holds a box, and the pose, with that box's kind, would be accepted as an AddBox by the world without that box. It replaces the box's pose with the one given, exactly, keeping its key and kind.
- DeletePath is accepted when its key holds a path, and DeleteBox when its key holds a box. Each destroys that entity. No command deletes or moves the entrance.

An AddPath or AddBox whose kind is not a value of its enum describes nothing, and is refused. A box or entrance with no footprint, or a path with an empty ground line, which only a hand-written save can hold, is no solid or line, so it blocks nothing. For a physically valid world, a command is therefore accepted exactly when it names what it acts on and the world it describes is physically valid.

Commands compare equal when their fields do. A ParkEdit is any one of the five commands, the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on the command it holds, and queueEdit pushes that command to a CommandQueue as its own type, so a cycle applies it exactly as if it had been pushed directly.

## The new park

makeNewPark(seed) gives a world with makeParkSchema's schema, the seed, tick 0, next key 3, and resolution pending, holding exactly two entities. It is physically valid. Entity 1 is an entrance at (0, 126.5) facing (0, -1), so its back lies on the park's edge at z = 128 and its front faces into the park. Entity 2 is a guest path through (0, 123) and (0, 103), which starts 2 m in front of the entrance and runs 20 m into the park. tests/parks/new.park is its save with seed 1.
