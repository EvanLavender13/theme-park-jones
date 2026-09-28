# Feature: Park Intent

## Summary

park-intent gives the park its first content. It adds the module src/sim/park to tpj_sim, with the three kinds of intent the player authors: paths by kind through clicked points, shop and depot boxes with a pose, and the entrance. It registers them as intent in makeParkSchema, keeps the component types private to the module (decision 0016), gives other modules key-ordered queries that return public values, and adds the geometry every later feature and capability reads. A path's ground line is its centripetal Catmull-Rom curve sampled by a fixed rule, and a box's footprint is a rectangle with a front and a back. makeNewPark builds the fixed template, a park with an entrance and a short guest path, and tests/parks/new.park is its save, which the cross-build check runs. Nothing changes intent yet: park-edits adds the commands and the physical-validity check.

## Acceptance criteria

1. addParkIntent registers entrance, path, and box, in that order, as intent, and makeParkSchema registers them after the network. A save holding any finite intent of every kind, in the form src/sim/park/SPEC.md gives, loads with makeParkSchema, saves again to identical text, and loads again to an equal world (principle 1).
2. parkEntrances, parkPaths, and parkBoxes give every entity holding that kind of intent, in ascending key order, each with its key and its values exactly as loaded or created.
3. groundLine gives an empty line exactly when the path's points include a coordinate that is not finite or lies outside the park's square, or fewer than two points are kept. It never throws, whatever the points.
4. A non-empty ground line holds every kept point, bit for bit and in order, the first at distance 0 and the last as the line's final point. Between two consecutive kept points it holds max(8, ceil(c / 1 m)) points counting the first and not the last, where c is the chord between them, and every point it holds strictly between them lies on the centripetal Catmull-Rom curve with reflected phantom ends, within 1e-9 m, at the parameter the spec gives.
5. Each distance of a ground line after the first is the previous distance plus sqrt(dx * dx + dz * dz) of the step from the previous point, computed in that order, so distances strictly increase.
6. footprintOf gives no footprint exactly when the pose has a coordinate or facing component that is not finite, or a facing of zero length. Otherwise its forward is a unit vector in the facing's direction, within 1e-12. For a position inside the park's square, its corners are the rectangle the spec gives, within 1e-9 m, and a pose whose facing is scaled by a positive factor that leaves its components finite and each nonzero one nonzero gives the same footprint, within 1e-12 m per coordinate.
7. makeNewPark gives exactly the template the spec gives, with the seed given and resolution pending. The template's entrance footprint and its path's ground line, widened by half the path's width, lie inside the park's square, and the ground line comes no nearer than half the path's width to the footprint. tests/parks/new.park is saveWorld's text for makeNewPark(1).

## Medium

None. Park intent sits outside principle 3 (decision 0025). navigable-networks will derive networks from the ground lines and connect boxes and the entrance by their faces. plausible-operations will derive shops and the depot from boxes, and believable-guests will take the entrance's key as where guests arrive. This feature reads nothing from any of them.

## Principle checks

- Principle 1: intent holds only clicked points, poses, and kinds. The three component types are registered as intent, and nothing this feature derives (ground lines, footprints) is a registered component. A save of intent loads back equal and saves again identically (criterion 1).
- Principle 2: every input gives a ground line or an empty one, and a footprint or none, and never throws, including paths with repeated, NaN, infinite, or out-of-square points and poses with zero, NaN, or infinite facings (criteria 3 and 6). A world holding finite but degenerate intent, such as a path of one point, repeated points, or points outside the square, or a box with a zero facing, still copies, hashes, saves, and loads. NaN is excluded, since the walk refuses it (src/sim/SPEC.md).
- Principle 10: the geometry uses only basic operations, sqrt, and ceil, so the symbol check keeps passing on tpj_sim, and a ground line or footprint computed twice from the same input is identical bit for bit.

## Spec changes

src/sim/SPEC.md: in the paragraph on registration, replace "It registers the medium's types first (sim/medium/SPEC.md), and so far nothing else." with "It registers the medium's types first (sim/medium/SPEC.md), then park intent (sim/park/SPEC.md)."

Create src/sim/park/SPEC.md:

```markdown
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

groundLine is empty when any point has a coordinate that is not finite or outside the park's square. Otherwise it keeps the points in order, dropping each point closer to the last kept point than MIN_POINT_SPACING, 1 cm: that is, one for which dx * dx + dz * dz is below MIN_POINT_SPACING * MIN_POINT_SPACING. With fewer than two kept points the line is empty.

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

## The new park

makeNewPark(seed) gives a world with makeParkSchema's schema, the seed, and resolution pending, holding exactly two entities. Entity 1 is an entrance at (0, 126.5) facing (0, -1), so its back lies on the park's edge at z = 128 and its front faces into the park. Entity 2 is a guest path through (0, 123) and (0, 103), which starts 2 m in front of the entrance and runs 20 m into the park. tests/parks/new.park is its save with seed 1.
```

## Files affected

- Create: src/sim/park/SPEC.md
- Create: src/sim/park/intent.h, src/sim/park/intent.cpp, src/sim/park/internal/components.h
- Create: src/sim/park/geometry.h, src/sim/park/geometry.cpp
- Create: tests/parks/new.park
- Modify: src/sim/SPEC.md, src/sim/park_schema.cpp, src/sim/CMakeLists.txt
- Modify: tests/scenarios/main_test.cpp, a comment that says makeParkSchema registers only the network type
- Create, by the test pass: tests/sim/park/ test files
- Modify, by the test pass: tests/sim/CMakeLists.txt

## Dependencies

deterministic-simulation's world-as-value (registration, saves, the walk) and shared-medium's CarrierPoint. No sibling feature.

## Out of scope

- Commands that change intent, and the physical-validity check: park-edits.
- Drawing ground lines and footprints: park-view.
- Carriers, networks, and connections built from the ground lines: navigable-networks.
- Adding a ground line or footprint line to tpj_scenarios' output. The first resolver that calls the geometry, navigable-networks', puts it under the cross-build check.

## Open questions

None.
