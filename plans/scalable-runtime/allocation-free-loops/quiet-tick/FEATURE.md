# Feature: Quiet Tick

## Summary

quiet-tick builds the allocation check of decision 0041 and makes the tick pass it. The check is an executable in tests/checks that counts every heap allocation and free. It steps tests/parks/stress/full.park through the app's own ParkSession, warms up, then watches a stretch of ticks. It fails if a heap block is both allocated and freed during the stretch, and names the ticks. To pass, a cycle with no commands must reuse all of its memory:

- Systems and swaps take a TickBuffers, the lists they keep from tick to tick, which the stepping caller owns and passes to stepWorld.
- The world keeps the nodes of the entities and components it removes, and reuses them for those it adds.
- Seat service holds its free seats in a fixed-capacity list.
- Stepped fields keep the slots each swap retires.
- The intent listings, anchors, and field samples the tick reads gain forms that fill a caller's lists.

The app, tpj_bench, tpj_scenarios, and the park generators each keep one TickBuffers across their ticks. The simulation's results don't change. This covers criteria 1 to 3 of the milestone.

## Acceptance criteria

1. tests/checks/allocations holds the allocation check, an executable of its own that replaces operator new and delete. It loads tests/parks/stress/full.park, resolves it, and steps it through ParkSession::step: first WARM_TICKS ticks, then WATCHED_TICKS ticks while it watches. WATCHED_TICKS is twice SEAT_TIME, so a seat service can start and end within it when its piece empties. Criterion 2's check shows that one does. If a block is allocated and freed during the watched ticks, it prints one line per block, `allocation check: tick freed a <n>-byte block at tick <f> that it allocated at tick <a>`, up to REPORTED_BLOCKS lines, and exits with status 1. Otherwise it prints `allocation check: <w> ticks of full.park freed no block they allocated` and exits with status 0. It runs in `ctest.exe --preset windows-debug` as the test "allocation check: ticks of full.park free no block they allocated". (check: inside stepGuests' loop over guests, in src/sim/guests/guests.cpp, add `const std::vector<EntityKey> held{key};` and `static_cast<void>(held);`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that name ticks; then revert)
2. Stepping full.park frees no block it allocated: the allocation check passes. That includes seat service starting and ending, offers published every tick, flows' packets arriving, and guests' choices. (ctest; check: make TypedStore's erase free its node instead of keeping it, build tpj_allocation_check, run it, and see it exit with status 1; then revert)
3. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, with its default ticks, and for full.park with 300 ticks. Saves keep their text, and hashes, equality, and the order of every walk are unchanged. (check: tpj_scenarios --compare, commands in PLAN.md task 21)
4. The app, tpj_bench, tpj_scenarios, and the park generators in src/scenarios each keep one TickBuffers for all the ticks of a run, so tpj_scenarios's comparison runs the path the app runs. (check: run `build/windows-debug/tpj_bench.exe --ticks 30 tests/parks/stress/winding-path.park` and see its stage lines; read ParkSession, startingPark, benchPark's ticks, runScenario, runSave, and the generators' loops for where each keeps its buffers)
5. The layer check places tests/checks above every layer, as it places tests/integration, and passes on the tree, which now holds the allocation check's files. (ctest: "layer check passes on the tree")
6. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: the three commands in PLAN.md task 22)

## Tests

None. The allocation check is a mechanical check that decision 0041 adds to tests/checks. It is written with the code and proved by the check in criterion 1.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows. The fields and flows keep the entries and units they hold, and sampling gives the entries it gave. Only where their memory comes from changes. A TickBuffers is never part of a world.

## Spec changes

src/sim/SPEC.md:

- Boundary: add sim/tick_buffers.h to the headers outside code reaches the world through.
- The tick cycle: replace "stepWorld(world) is the same cycle with no commands." with: "Systems and swaps take the world and a TickBuffers, which holds the lists they reuse from tick to tick. stepWorld(world, commands, buffers) passes the caller's buffers to every system and swap. stepWorld(world, buffers) is the same cycle with no commands. If you step a world repeatedly, keep one TickBuffers and pass it to every cycle. A TickBuffers holds one value of each type a module asks for, made on first use and kept. It is never part of a world, so walks, hashes, saves, and copies never see it. Each system and swap clears a list before it reads it, so a world steps to the same result whatever buffers it is given. stepWorld(world, commands) and stepWorld(world) step one cycle with buffers of their own."
- Entity keys: after the keysWith paragraph, add: "keysWith<T>(keys) puts the same keys in a list the caller keeps, clearing it first. forEachWith<T>(visit) calls visit(key, component) for each entity that holds T, in the same order, and visit must not add or remove a T. Neither allocates once the list has grown."
- A new section after Determinism, "Memory in play": "Once its buffers have grown, a cycle with no commands frees no memory it allocated (decision 0041). The world keeps the memory of the entities and components it removes and reuses it for those it adds. Once the park has held as many as it holds, guests arriving and leaving and seat service starting and ending allocate nothing. The allocation check in tests/checks holds a stretch of full.park's ticks to the first sentence."
- Saves: after the list of how values are written, add: "A component may hold a FixedList, a list with a fixed capacity, wherever it may hold a vector. Walks, saves, and loads treat it exactly as a vector. If a save holds more values than its capacity, loadWorld throws LoadError."

src/sim/medium/SPEC.md:

- Networks: after "firstAnchoredNode allocates nothing, so callers on the tick path can use it.", add: "anchoredNodes can also put an entity's nodes in a list the caller keeps, clearing it first, and then allocates nothing once the list has grown."
- Fields: replace "Systems publish them with publishStepped while stepping." with "Systems publish them with publishStepped while stepping, which gives the source's entries for the tick as an empty list for the system to fill." After the paragraph on the two layers' readers, add: "A field keeps the memory of the stepped entries each swap replaces, and the next tick's publishes reuse it. Once a field has held as many entries as it holds, publishing every tick allocates nothing."
- Fields: after the sampleField paragraph, add: "sampleField can also fill a FieldSample that the caller keeps. The sample holds sampleField's working lists and the entries it gives, and it allocates nothing once those lists have grown. Kept fields have no such form."
- Flows: after the paragraph on the swap, add: "A swap allocates nothing once the lists it keeps have grown, apart from a ledger's own lists growing."

src/sim/park/SPEC.md:

- After "Other modules change intent only through commands." add: "parkEntrances and parkBoxes can also put their values in a list the caller keeps, clearing it first, and then allocate nothing once the list has grown. parkHours allocates nothing."

## Files affected

Create:

- src/sim/tick_buffers.h
- src/sim/fixed_list.h
- tests/checks/allocations/CMakeLists.txt
- tests/checks/allocations/watch.h
- tests/checks/allocations/watch.cpp
- tests/checks/allocations/allocation_check.cpp

Modify:

- src/sim/SPEC.md, src/sim/medium/SPEC.md, src/sim/park/SPEC.md
- src/sim/schema.h, src/sim/schema.cpp, src/sim/field_text.h, src/sim/world.h, src/sim/world.cpp, src/sim/cycle.cpp
- src/sim/medium/network.h, src/sim/medium/network.cpp, src/sim/medium/field.h, src/sim/medium/kept_field.h, src/sim/medium/ground_field.h, src/sim/medium/flow.h, src/sim/medium/flow.cpp
- src/sim/park/intent.h, src/sim/park/intent.cpp
- src/sim/operations/operations.cpp, src/sim/operations/internal/offer_entries.h, src/sim/operations/facilities.cpp, src/sim/operations/seats.cpp, src/sim/operations/internal/seat_service.h
- src/sim/furniture/furniture.h
- src/sim/guests/guests.h, src/sim/guests/guests.cpp, src/sim/guests/footfall.cpp
- src/app/session/park_session.h, src/app/session/park_session.cpp
- src/bench/stages.cpp
- src/scenarios/runner.cpp, src/scenarios/day_parks.cpp, src/scenarios/full_park.cpp, src/scenarios/slice_parks.cpp, src/scenarios/beacons.cpp, src/scenarios/food_shop.cpp, src/scenarios/stalls.cpp, src/scenarios/walkers.cpp
- tests/sim/cycle_test.cpp, tests/sim/save_test.cpp (their registered systems and swaps take the new argument, to fix the build)
- tests/checks/CMakeLists.txt, cmake/check_layers.cmake

## Dependencies

- Decision 0041 and the allocation-free-loops milestone.
- Decision 0039: the stores keep key order.
- tests/parks/stress/full.park.

## Out of scope

- Display work, the scene, the preview, the tools, and the panels. Later features of the milestone fix them, and their callers keep the allocating forms of the functions this feature gives fill forms to.
- Resolution and commands. A cycle that applies a command still allocates. The changing-previews milestone takes resolution.
- Tests keep stepping with stepWorld(world) and stepWorld(world, commands). They aren't play.
- stepShops finds each shop's nearest depot, and the delivery delay from it, by sampling backstage route distance at every tick. Route distance changes only at a resolution, so this repeats unchanged work, about two samples per shop per tick on full.park. This feature makes that work allocate nothing but doesn't stop repeating it. Deriving it once per resolution adds derived data, which changes hashes, so it goes to the capability's backlog.

## Open questions

- How long a warm-up the check needs, so that no kept list grows twice during the watched ticks. Start with WARM_TICKS of 300 and WATCHED_TICKS of 600. If a reported block comes from a kept list growing, lengthen WARM_TICKS until no list grows twice during the watched ticks, and record the length and the check's running time here.
