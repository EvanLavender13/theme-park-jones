# Implementation Plan: Best Seats

## Goal

The guest network carries best rest seats and best meal seats fields, derived in resolution by one search per field from every piece of furniture at once.

## Approach

A new routes unit, best_places, runs a multi-source label-setting search. It takes labels in order of distance, then higher value, then lower key. At each node it keeps a seed while fewer than `keep` kept seeds there have at least its value, and it spreads only kept labels. Next steps are chosen afterwards from the kept labels of neighbouring nodes, as routeEntries chooses them. The same unit gives the edge rule, which merges the entries at an edge's ends and along it in place in the output list. A new module, sim/wayfinding, seeds the search from seat places and seat offers and publishes the two fields, each under its own field key.

## Placement

Decision 0027 places each behavior this feature adds:

- The best places search and its edge rule: sim/routes, a new unit src/sim/routes/best_places.h and best_places.cpp. routes owns the searches over networks, and the search reads only a network and seeds, so bins can use it later.
- The rule that picks a step among equals, shared by route distance and the search: sim/routes, a new private header src/sim/routes/internal/route_steps.h. Both routes units need the same rule, and no other module should.
- The best seats fields, their seeds, and their resolver: a new module, sim/wayfinding, in src/sim/wayfinding/best_seats.h and best_seats.cpp. They read seat offers, which sit in operations, so they can't sit in routes or furniture. guests will read them, so they sit below guests.
- Calling addBestSeats: sim/park_schema, in addPark in src/sim/park_schema.cpp, which composes the park's registrations in dependency order.
- The new layer: cmake/layers.txt, and the expected layers in tests/checks/layer_check_test.cmake.

## Tasks

### Task 1: Spec the change

Files:
- Modify: `src/sim/routes/SPEC.md`, `src/sim/SPEC.md`
- Create: `src/sim/wayfinding/SPEC.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

### Task 2: Share the step rule

Files:
- Create: `src/sim/routes/internal/route_steps.h`
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Move isPreferred and its comment from route_distance.cpp's anonymous namespace into the new header as `inline bool isPreferredStep(const RouteStep &step, const RouteStep &other)` in namespace tpj, with include guard `TPJ_SIM_ROUTES_INTERNAL_ROUTE_STEPS_H` and `#include "sim/routes/route_distance.h"`. Include the header in route_distance.cpp and call isPreferredStep where it called isPreferred.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 3: Declare best places

Files:
- Create: `src/sim/routes/best_places.h`, `src/sim/routes/best_places.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: Write best_places.h with the declarations FEATURE.md's Public interface gives for it. Include `sim/entity_key.h`, `sim/medium/field.h`, `sim/medium/network.h`, `sim/routes/route_distance.h`, `<span>`, `<stdint.h>`, and `<vector>`. Give BestPlace a visitFields that visits `key`, `owner`, `at`, `value`, `distance`, and `next`, in that order.

Step 2: Write best_places.cpp with stub definitions. bestPlaces returns `{}`, and sampleBestPlaceEdge does nothing. Leave the parameters unnamed so the stubs build without warnings.

Step 3: Add `routes/best_places.cpp` before `routes/networks.cpp` in src/sim/CMakeLists.txt.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 4: Declare the best seats fields and their layer

Files:
- Create: `src/sim/wayfinding/best_seats.h`, `src/sim/wayfinding/best_seats.cpp`
- Modify: `src/sim/CMakeLists.txt`, `src/sim/park_schema.cpp`, `cmake/layers.txt`, `tests/checks/layer_check_test.cmake`

Step 1: Write best_seats.h with the declarations FEATURE.md's Public interface gives for it, include guard `TPJ_SIM_WAYFINDING_BEST_SEATS_H`. BestMealSeats repeats BestRestSeats with its own name and comment. Include `sim/medium/field.h`, `sim/routes/best_places.h`, `<stdint.h>`, `<string_view>`, and `<vector>`, and forward-declare `class WorldSchema;`.

Step 2: Write best_seats.cpp. Define each field's sampleEdge as `sampleBestPlaceEdge(sample, SEAT_CHOICES, out);` and addBestSeats as an empty stub with its parameter unnamed.

Step 3: Add `wayfinding/best_seats.cpp` between `walk.cpp` and `world.cpp` in src/sim/CMakeLists.txt.

Step 4: In cmake/layers.txt, add the line `sim/wayfinding` between `sim/operations` and `sim/guests`. In tests/checks/layer_check_test.cmake, in the repository_layers case, add `sim/wayfinding` to the expected list between `sim/operations` and `sim/guests`.

Step 5: In src/sim/park_schema.cpp, include `sim/wayfinding/best_seats.h` and call `addBestSeats(schema);` after `addOperations(schema);`. In addPark's comment, change "then the operations that run its shops and depots, then the guests who visit them" to "then the operations that run its shops and depots, then the wayfinding that leads guests to the best of many places, then the guests who visit them".

Step 6: Build, and run the layer check.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug -R "layer"`
Expected: the build ends with no warnings and no errors, and ctest reports 100% tests passed.

### Task 5: Search from every seed

Files:
- Modify: `src/sim/routes/best_places.cpp`

Step 1: Write the search as its own function in the anonymous namespace. It gives, for each node, the kept labels in the order they were kept: each with the seed's index into the resolved seeds and its distance.

- Throw std::invalid_argument naming the key when two seeds share a key. Find repeats on a sorted copy of the keys.
- Resolve each seed's place with `network.resolve`. Leave out seeds that don't resolve. A seed at a node gives the label (0.0, seed) at that node. A seed strictly inside an edge gives the labels (FromOffset, seed) at the edge's From node and (ToOffset, seed) at its To node, using EdgePosition's offsets.
- Keep a min priority queue of labels ordered by distance, then higher value, then lower key, then lower node.
- Pop a label. Skip it if its node already keeps its seed. Count the labels the node keeps whose seed's value is at least the label's. Skip it if that count is at least `keep`. Otherwise keep it, and for each of the node's edgeEnds, push the label at the edge's other end with the distance plus the edge's length.

Step 2: Name the stub bestPlaces' parameters, and have it call the search and still return `{}`, so the search has a caller and the build has no unused-function warning. Task 6 replaces the return.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 6: Choose next steps and publish entries

Files:
- Modify: `src/sim/routes/best_places.cpp`

Step 1: Write bestPlaces with the search of Task 5. For each node in order, and each label it keeps in order, choose the next step:

- If the seed's place resolves to this node and the distance is 0.0, the step is `RouteStep{}`, to show arrival.
- Otherwise, collect the steps that achieve the distance. For each of the node's edgeEnds, the far node is the edge's other end. If the far node keeps the seed and its distance plus the edge's length equals this distance, the step runs from this end's stop distance to the far end's. If the seed's place lies strictly inside that edge and the place's offset from this end equals this distance, the step runs from this end's stop distance to the seed's place's distance.
- Take the first by isPreferredStep.

Give each kept label a PlacedEntry at `network.nodePlace(node)` with the seed's key, owner, place, and value, the distance, and the step. After every node, give each resolved seed strictly inside an edge, in key order, an entry at its own place with distance 0.0 and `RouteStep{}`.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 7: The rule inside an edge

Files:
- Modify: `src/sim/routes/best_places.cpp`

Step 1: Write sampleBestPlaceEdge, working in place at the end of out.

- Note out's size as the start.
- For each Along entry, append a copy with distance `|entry.FromOffset - sample.FromOffset|`. If that is 0.0, its step is `RouteStep{}`. Otherwise, it runs from the edge's FromDistance to the entry's place distance when the entry lies toward the To end, and from the edge's ToDistance to it when it lies toward the From end.
- For each AtFrom entry, append a copy with distance `sample.FromOffset + entry.Distance` and step `{edge.Carrier, edge.ToDistance, edge.FromDistance}`. For each AtTo entry, append a copy with distance `sample.ToOffset + entry.Distance` and step `{edge.Carrier, edge.FromDistance, edge.ToDistance}`.
- Sort the appended entries by key, then distance, then arrival first, then isPreferredStep. Keep the first of each key, with std::unique on key, and erase the rest.
- Sort the remaining appended entries by distance, then higher value, then lower key. Move each one forward when fewer than `keep` entries already moved forward have a value at least its own, and erase the rest.

Use std::sort, not std::stable_sort, which may allocate.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 8: Seed and publish the best seats

Files:
- Modify: `src/sim/wayfinding/best_seats.cpp`

Step 1: Write the resolver best-seats. It reads `parkNetwork(world, PathKind::Guest)`. For each slot of `resolvedSlots<SeatPlaces>(world)` and each entry, it finds the piece's slot in `resolvedSlots<SeatOffers>(world)` by lower_bound on Source. It skips the piece if there is no slot or the slot has no entries. With the slot's first entry's SeatOffer, the piece's seed has key the piece, owner the entry's Segment, and place the entry's place. It joins the rest seeds with value RestRelief when RestRelief is above 0.0, and the meal seeds with MealRelief when MealRelief is above 0.0. For each field, if bestPlaces gives any entries, publish them with `publishResolved<F>(world, fieldKey(F::Name), ...)`.

Step 2: Write addBestSeats: `addField<BestRestSeats>`, `addField<BestMealSeats>`, then `schema.addResolver("best-seats", &resolveBestSeats, {"seat-offers", "best-rest-seats-field", "best-meal-seats-field"})`. Include `sim/furniture/furniture.h`, `sim/operations/seats.h`, `sim/park/intent.h`, `sim/routes/networks.h`, `sim/schema.h`, and `sim/world.h`.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim && cmake.exe --build --preset windows-release`
Expected: both builds end with no warnings and no errors.

### Task 9: Check the fields against brute force

Files:
- Create, scratch: `build/best-seats/build.sh`, `build/best-seats/bandcheck.cpp`

Step 1: Copy build/seat-service/build.sh to build/best-seats/build.sh.

Step 2: Write bandcheck.cpp. It loads the park file given as its argument with makeParkSchema, resolves it, and does the following for each field, best-rest-seats and then best-meal-seats.

- Make the seeds as Task 8 does, from the two fields through resolvedSlots. Print the count.
- For each seed, run its own Dijkstra over the guest network's edges, seeded as Task 5 seeds one seed, giving its distance at every node.
- At each node, sort the seeds that reach it by distance, then higher value, then lower key. Keep each seed that fewer than SEAT_CHOICES seeds before it, kept or not, have a value at least its own. Sample the field at the node's nodePlace. The node matches if the sample's keys and distances, in order, are exactly the kept seeds' keys and distances.
- Do the same at each edge's midpoint, `Place{edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2}`, skipping one that resolves to a node, and at each seed's place strictly inside an edge. A seed's distance there is the least of FromOffset plus its From node distance, ToOffset plus its To node distance, and, if its place lies strictly inside the same edge, `|its offset from the From end - the place's FromOffset|`. Use resolve's offsets for both.
- From each node, for each sampled entry, walk. At each step, if Next's carrier is NULL_KEY, the walk has arrived when the place equals the seed's place or both resolve to the same node. Otherwise the step's length is `|Next.To - Next.From|` from a node and `|Next.To - place.Distance|` inside an edge. Sample at `Place{Next.Carrier, Next.To}`, find the same key, and check that its distance plus the length equals the distance before, exactly. Stop after 100,000 steps.
- From each edge midpoint, walk the same way, and check that the lengths add up to the first distance within 1e-9.

Print one line per field: `<name> seeds S nodes N matching M places P matching Q walks W arrived A exact E midwalks V arrived B close C`.

Step 3: Run it on both parks.

Run: `cmake.exe --build --preset windows-release --target tpj_sim && build/best-seats/build.sh bandcheck && build/best-seats/bandcheck.exe tests/parks/furnished-paths.park && build/best-seats/bandcheck.exe tests/parks/stress/full.park`
Expected: four lines. On furnished-paths.park S is 85 for each field, and on full.park 644. In every line, M equals N, Q equals P, A and E equal W, and B and C equal V. If a count falls short, it is a deviation: find the first mismatch before going on.

### Task 10: Confirm the slice parks are otherwise unchanged

Files:
- Create, scratch: `build/best-seats/after/`

Step 1: Make the slice parks and diff them with the checked-in files.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && A=build/best-seats/after && mkdir -p $A && cp tests/parks/fed.park $A/fed.park && build/windows-debug/tpj_scenarios.exe --slice-parks $A/fed.park $A/warm.park $A/cut.park tests/parks/main-street.park $A/warm-main-street.park tests/parks/facilities.park $A/stocked.park $A/day.park $A/no-toilet.park $A/noon.park $A/evening.park && for f in fed warm cut warm-main-street stocked day no-toilet noon evening; do diff tests/parks/$f.park $A/$f.park; done | grep '^[<>]' | sed 's/^> [0-9][0-9]* /> KEY /' | sort | uniq -c`
Expected: eight lines, in some order, and none starting with `<`: `45 > ` (the blank line after each section), `9 > [seat-places-stepped]`, `9 > [seat-visits-ledger]`, `9 > [seat-offers-stepped]`, `9 > [best-rest-seats-stepped]`, `9 > [best-meal-seats-stepped]`, `36 > KEY readable=[] pending=[]`, and `9 > KEY packets=[] stocks=[] created=0 consumed=[]`.

Step 2: Check that debug and release simulate the scenarios alike. The food-shop scenario's furnished path resolves both fields.

Run: `cmake.exe --build --preset windows-release --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe > build/best-seats/debug.txt && build/windows-release/tpj_scenarios.exe > build/best-seats/release.txt && build/windows-debug/tpj_scenarios.exe --compare build/best-seats/debug.txt build/best-seats/release.txt; echo $?`
Expected: `0`.

### Task 11: Compare the runtime report

Files:
- Create, scratch: `build/best-seats/after.report`

Step 1: Make the report on this feature's build and compare it with the report from seat-service. Main has changed only in plans since that report, so it stands as the baseline.

Run: `scripts/runtime-report.sh build/best-seats/after.report && scripts/runtime-report.sh --compare build/seat-service/after.report build/best-seats/after.report`
Expected: the tick stages within their noise (`unclear`), and full.park's resolution median up by less than 1,413 µs from 14,132.9 µs. `result-changed` marks every hash stage, since new registered types change every world's hash. Report the numbers.

### Task 12: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 13: Commit, merge, and close the milestone

Files:
- Modify: `plans/navigable-networks/best-seats/MILESTONE.md`

Step 1: Stage only this feature's paths: src/sim/routes/, src/sim/wayfinding/, src/sim/SPEC.md, src/sim/CMakeLists.txt, src/sim/park_schema.cpp, cmake/layers.txt, tests/checks/layer_check_test.cmake, and plans/navigable-networks/best-seats/best-seats/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Lead guests to the best seats`. Merge into main as implementing-features says, and rebuild windows-release.

Step 2: Close the milestone as planning-milestones' Closing a milestone says. Check each of MILESTONE.md's criteria by its means and report what was seen, set its Status to complete, and commit MILESTONE.md by pathspec with subject `Plans: Close the best-seats milestone`.
