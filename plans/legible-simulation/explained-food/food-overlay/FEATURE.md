# Feature: Food Overlay

## Summary

A new library, tpj_legible, in src/legible, computes food availability at any place of a world's guest network. It is how well fed a guest standing there could be: the sum, over the reachable shops whose offers say meals are available, of each shop's relief discounted by the effective time to a meal there, its route distance at REFERENCE_SPEED plus its offer's wait. Each shop's term is its contribution, and the value is exactly their sum. The renderer builds the food overlay on the CPU, shaded by a value function the app makes from it, so render never depends on tpj_legible: a band OVERLAY_BAND wide either side of each guest path's line, sampled along the line and colored through a viridis ramp, with zero a distinct gray. The band is shaped as a low tent, so where bands overlap, the nearest line's shows. The app draws it over the terrain and under the park while the Debug panel's new Food overlay checkbox is checked, rebuilding it every frame with no cache, and --overlay food checks the box at start for scripted captures. Integration tests check slice criterion 4 on the slice's parks, and captures of warm.park and cut.park show slice criteria 8 and 9.

## Acceptance criteria

Throughout, N is parkNetwork(world, PathKind::Guest), R is the entries sampleField of guest route distance on N gives at a place, and a source's offer is the first of its own entries that sampleField of food-offer on N gives at the nodePlace of its lowest anchored node on N, or none.

1. foodAvailability(world, place) lists one contribution for each source of R whose offer exists and has Supplied true, in ascending source order, and no other. Each holds the source as Shop, the offer's Relief, the Distance of the source's first entry in R, Wait as the offer's Wait times SIM_TICK_SECONDS, Time as Distance / REFERENCE_SPEED + Wait, and Term as Relief times foodDiscount(Time).
2. foodAvailability's Value is 0.0 with each contribution's Term added in order, bit for bit. A place with no contributions, including one that does not resolve on N and any place in a world with no guest network, has the Value 0.0.
3. foodDiscount(t) is the piecewise-linear curve through FOOD_DISCOUNT_CURVE's points, with the value a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X) strictly between consecutive points a and b. A time below the first point's X gives 1, one above the last point's X gives 0, and a NaN gives 0.
4. foodColor(value) is OVERLAY_ZERO_COLOR for a value not above 0, including a NaN, and otherwise the ramp color src/render/SPEC.md defines from OVERLAY_RAMP and OVERLAY_FULL, which is the last stop for every value at or above OVERLAY_FULL. No positive value's color equals OVERLAY_ZERO_COLOR in red, green, and blue.
5. buildFoodOverlay(world, value) holds, for each carrier of N keyed by a guest path of parkPaths, in ascending key order, one row for each of the carrier's sample distances and then an end cone at each end, as src/render/SPEC.md defines them. Each row's center vertex lies at its sample place's groundPoint and OVERLAY_TOP_LIFT, and its edge vertices OVERLAY_BAND from the center at OVERLAY_EDGE_LIFT. Every vertex of a row or cone has foodColor of the value function's value at its sample place and an upward normal. Every cone triangle faces up, and so does every triangle joining two rows of which neither is at a point strictly between the line's ends. Connectors and backstage paths have no band. Building it on a slice park leaves hashWorld unchanged.
6. Between two consecutive rows of one segment of a line, neither at a point strictly between the line's ends, the overlay's surface over a ground point at distance d from the segment, within OVERLAY_BAND, lies at OVERLAY_TOP_LIFT - (OVERLAY_TOP_LIFT - OVERLAY_EDGE_LIFT) * d / OVERLAY_BAND. So where two lines' bands overlap there, the nearer line's surface is the higher.
7. ThemeParkJones given --overlay with any value but food, or with no value, or --overlay food together with --hash, prints its usage and exits with a nonzero status.
8. Opened tests/parks/fed.park, warm.park, and cut.park, each stepped 300 ticks: in every cycle, at the nodePlace of every node and at the midpoint of every edge of N, foodAvailability's Value is the in-order sum of its terms (slice criterion 4). Every such place in cut.park has the Value 0.0, and some place in warm.park has a positive Value. foodAvailability at those places leaves hashWorld unchanged.
9. Captures of warm.park and cut.park after 3000 ticks with --overlay food show the paths, boxes, and guests with the band on the ground under the paths. In cut.park the starved shop is marked and the band is the zero color, where warm.park's shades toward its shop (slice criteria 8 and 9).

## Medium

The feature produces nothing the simulation consumes. It samples:

- Guest route distance (navigable-networks), at each sampled place, for each source's Distance.
- Food offer (plausible-operations), at each source's lowest anchored node, for Relief, Wait, and Supplied.
- The guest network (navigable-networks, through shared-medium's Network queries): carriers, points, stops, groundPoint, anchoredNodes, and nodePlace.
- Park intent (effortless-building): parkPaths, to tell guest paths from connectors.

## Principle checks

- Principle 1 and 10: foodAvailability and buildFoodOverlay leave hashWorld unchanged (criteria 5 and 8), and tpj_legible links tpj_sim alone, so availability is never part of a world's state, hash, or save, and is computed without a window.
- Principle 2: a world with no guest network, a place that does not resolve, and a park with no supplied shop give the value 0.0 and an overlay with no band or a zero-colored band, never an error.
- Principle 4: each contribution's Distance is the source's guest route distance at the place (criterion 1), and the band takes its values from places on the paths (criterion 5).
- Principle 5: every reachable supplied shop contributes, however far, with a term that only falls as its effective time grows (criteria 1 and 3).
- Principle 8: the terms reconstruct the value exactly at every place (criteria 2 and 8), and a starved shop's area reads as zero (criterion 8).

## Spec changes

- src/legible/SPEC.md, new: the module's contract, reading only fields, networks, and intent and changing nothing, and a Food availability section defining foodAvailability, its contributions and value, REFERENCE_SPEED, FOOD_DISCOUNT_CURVE, and foodDiscount.
- src/render/SPEC.md: a paragraph in Contract for setOverlayMesh and where drawFrame draws the overlay, and a Food overlay section defining foodColor, the ramp and zero color, the band's lines, samples, rows, and end cones, the tent's lifts, and what the tent's shape gives where bands overlap.
- src/app/SPEC.md: the Food overlay checkbox and the overlay's rebuilding in Tooling UI, and --overlay food and its refusals in Command line.

The exact text is in PLAN.md, Tasks 1 to 3.

## Files affected

- Create: src/legible/SPEC.md, src/legible/CMakeLists.txt, src/legible/food.h, src/legible/food.cpp
- Modify: CMakeLists.txt
- Create: src/render/food_overlay.h, src/render/food_overlay.cpp
- Modify: src/render/CMakeLists.txt, src/render/renderer.h, src/render/renderer.cpp, src/render/SPEC.md
- Modify: src/app/main.cpp, src/app/CMakeLists.txt, src/app/debug_panel.h, src/app/debug_panel.cpp, src/app/SPEC.md
- Tests, from the test pass: tests/legible/, tests/render/, tests/app/, tests/integration/, and tests/CMakeLists.txt

## Dependencies

- hungry-guests' slice-parks, merged: tests/parks/fed.park, warm.park, and cut.park.
- The food offer, guest route distance, and the offer lookup guests use (sim/guests/SPEC.md, Choice).

## Out of scope

- Hover attribution: overlay-attribution, the milestone's next feature.
- The overlay on a candidate world: candidate-previews.
- Shading exact near a line's own bends, and a band along connectors.
- An overlay for any other field: the capability's deepening candidate.

## Test pass decisions

- The usage text is not part of the contract, so criterion 7's usage is checked as output on standard output or standard error.
- At and above OVERLAY_FULL, foodColor is render/SPEC.md's formula at f = 1, which in float arithmetic can differ from the last stop by rounding. Criterion 4's "the last stop" holds to within float rounding, and the test compares within 1e-6.
- Criterion 7's refusals without --hash are checked with --frames 1, so an implementation that wrongly accepted them would open a window for a frame. That needs a machine that can open one, as every app test run does.
- The --overlay food being accepted, the checkbox, rebuilding every frame, and the drawing order have no window-free test. Criterion 9's captures check them.
- The render tests find rows and cones by the vertex order render/SPEC.md gives, and triangles by the vertices they use, not by the order of the index list.
- The hash half of criterion 8 is checked on each park's opened world and after its last cycle, not in every cycle. Computing availability either changes the hash or does not, and hashing every cycle would make the test too slow.

## Open questions

None.
