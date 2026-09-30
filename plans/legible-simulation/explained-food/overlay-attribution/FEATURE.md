# Feature: Overlay Attribution

## Summary

Hovering the food overlay explains it. tpj_legible gains nearestGuestPathPlace, which finds the place on a guest path nearest a ground point and how far the point lies from it, and foodNear, the food availability there when the point lies within a reach of it. While the overlay is shown, the app takes the ground under the cursor. When foodNear with the reach OVERLAY_BAND gives an availability, a tooltip shows it: the value, and a table with one row per contributing shop giving its relief, route distance, expected wait, effective time, and term. The terms are exactly the ones the value sums, in its order. The tooltip is a new app component; main.cpp calls it once from the frame's UI.

## Acceptance criteria

Throughout, N is parkNetwork(world, PathKind::Guest), and a guest path's carrier is the carrier of N keyed by a guest path of parkPaths.

1. For a finite ground point in a world whose N has a guest path's carrier, nearestGuestPathPlace gives a place on some guest path's carrier, and a Distance equal to the straight distance, sqrt(dx * dx + dz * dz), from the point to the place's groundPoint. No guest path's carrier has a place, by nearestPlaceOn, strictly nearer the point, and among guest paths whose nearestPlaceOn is equally near, the place is on the lowest key's.
2. nearestGuestPathPlace gives none for a point that is not finite and for any point in a world whose N has no guest path's carrier, such as a park with only backstage paths, or a world not yet resolved. A connector's place is never given.
3. foodNear(world, point, reach) is foodAvailability at nearestGuestPathPlace's place when that gives one whose Distance is at most the reach, and none otherwise.
4. nearestGuestPathPlace and foodNear change nothing: hashWorld is the same before and after.
5. In the running app, while the Food overlay is shown, hovering the ground within OVERLAY_BAND of a guest path shows a tooltip. It gives foodAvailability's Value at the cursor's nearest guest path place and a row for each of its contributions, in order, with the shop's key, relief, route distance, wait, effective time, and term. Hovering beyond the band, over a panel, or with the overlay hidden shows none. This is checked by hand, as slice criterion 10 is.

## Medium

The feature produces nothing the simulation consumes. It reads:

- The guest network (navigable-networks, through shared-medium's Network queries): nearestPlaceOn and groundPoint.
- Park intent (effortless-building): parkPaths, to take guest paths alone.
- Food availability (food-overlay): foodAvailability, which samples guest route distance and the food offer.

## Principle checks

- Principle 1 and 10: nearestGuestPathPlace and foodNear leave hashWorld unchanged (criterion 4).
- Principle 2: a world with no guest path and a point that is not finite give none, never an error (criterion 2).
- Principle 4: the straight distance only locates the cursor on a path; the attribution shown is foodAvailability's, measured along routes (criteria 1, 3, and 5).
- Principle 8: the tooltip lists the terms the value is the sum of (criteria 3 and 5, with food-overlay's criterion 2).

## Spec changes

- src/legible/SPEC.md: a Path places section defining PathPlace, nearestGuestPathPlace, and foodNear, and saying the straight distance only locates a point on the paths.
- src/app/SPEC.md, Tooling UI: the food tooltip, when it shows, and what it lists.

The exact text is in PLAN.md, Tasks 1 and 2.

## Files affected

- Modify: src/legible/SPEC.md, src/legible/CMakeLists.txt, src/legible/food.h, src/legible/food.cpp
- Create: src/legible/path_place.h, src/legible/path_place.cpp
- Modify: src/app/SPEC.md, src/app/CMakeLists.txt, src/app/main.cpp
- Create: src/app/food_tooltip.h, src/app/food_tooltip.cpp
- Tests, from the test pass: tests/legible/

## Dependencies

- food-overlay, merged: foodAvailability, the overlay, OVERLAY_BAND, and the Food overlay checkbox.

## Out of scope

- The tooltip on a candidate world while a ghost is shown: candidate-previews.
- Marking the ghost's own contribution in the tooltip: the milestone's deepening candidate.
- A pinned attribution panel.

## Open questions

None.
