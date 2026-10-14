# Implementation Plan: Lean Routes

## Goal

Route distance builds its step lists once per resolution and keeps its memory, resolvers take kept buffers, and tpj_bench times a path edit's preview.

## Approach

Resolvers and finishers become functions of the world and a TickBuffers, like systems, and every resolving entry point takes the caller's buffers or makes its own. The field's resolved layer retires emptied slots and hands them back through publishResolved, as the stepped layer does. Route distance keeps its step lists, search lists, and sources in a RouteLists from the buffers. The bench stages land first, so the report before the simulation changes holds them.

## Placement

Decision 0027 places each behavior this feature adds:

- Resolvers and finishers taking buffers: sim, src/sim/schema.h ResolveFunction. The schema owns the function types it registers.
- Resolving with the caller's buffers: sim, src/sim/world.h and src/sim/cycle.cpp, resolveWorld, makeCandidate, and commitCommands. The cycle owns resolution.
- The resolved layer keeping its memory: medium, src/sim/medium/field.h, ResolvedEntries, emptyResolvedEntries, settleSteppedEntries, and publishResolved. The medium owns a field's layers.
- Route distance's kept lists: routes, src/sim/routes/route_distance.cpp, RouteLists, private to the file. The resolver owns its working lists.
- The session passing its buffers when it applies edits while paused: app, src/app/session/park_session.cpp. Composition only.
- The new stages: bench, src/bench/stages.cpp. The runner owns the stages.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, build tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: exit status 0 and no warning.

Step 2: Record the scenarios.

Run: `mkdir -p build/lean-routes && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/lean-routes/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/lean-routes/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c lean-routes`.

### Task 2: Add the bench stages

Files:
- Modify: `src/bench/stages.h`
- Modify: `src/bench/stages.cpp`
- Modify: `src/bench/SPEC.md`

Step 1: Make src/bench/SPEC.md's edit from FEATURE.md's Spec changes.

Step 2: In src/bench/stages.h, add `inline constexpr double PATH_SPUR_LENGTH = 12.0;` with the comment "How far beyond a guest segment's side the path edit's stages draw their path, in metres." Rewrite benchPark's comment to list the stages in SPEC.md's order, ghost after preview and the three path stages after it, and to say it also throws std::runtime_error when the park has a guest segment with a ground line but the world accepts no path from any segment's side.

Step 3: Make the resolution stage resolve a park that is already resolved, as an edit's resolution does. Before it, copy the loaded world into a World named resolved and resolve it once, untimed. The stage then repeats `resolveWorld(resolved)` on that same world, with nothing to prepare, and its result is the hash of the world after. The later stages read resolved where they read the stage's last world before.

Step 4: In src/bench/stages.cpp, add a function in the anonymous namespace that gives the stages' path edit: `std::optional<AddPath> pathSpur(const World &park)`. For each segment parkSegments gives, in its order, skip any that is not PathKind::Guest or has fewer than 2 line points. Take its line's first and last points, the unit normal a quarter turn counter-clockwise from first to last, and the middle of the two, as tests/checks/allocations/allocation_check.cpp segmentPlaces does. For the side factors +1 and then -1, make `AddPath{PathKind::Guest, {OnSegment{segment.Key, 0.5}, NewPoint{middle + normal * side * (0.5 * segment.Width + PATH_SPUR_LENGTH)}}}`, and return it when `isAccepted(park, ParkEdit{path})`. If the park has such a segment but no path is accepted, throw std::runtime_error("no guest segment of the park accepts a path from its side"). If it has no such segment, return std::nullopt.

Step 5: After the preview stage, inside its `if`, add the stage ghost: timeRepeated of `buildGhostMesh(park, *edit, candidate)`, with result kind Vertices and the sum of the ghost's Tints, Lit, and Marks vertex counts.

Step 6: After the box stages, if pathSpur gives a path, add three stages. path-acceptance: timeRepeated of `isAccepted(park, pathEdit)`, result kind Values, result 1 when accepted. path-preview: timeRepeated of `previewEdit(park, pathEdit)`, result kind Hash of its candidate, throwing as the preview stage does if it has none. path-ghost: as ghost, with the path edit and the path preview's candidate.

Step 7: Build and run.

Run: `cmake.exe --build --preset windows-release --target tpj_bench && build/windows-release/tpj_bench.exe --ticks 1 tests/parks/stress/full.park`
Expected: exit status 0, with lines starting `stage ghost`, `stage path-acceptance`, `stage path-preview`, and `stage path-ghost` after `stage preview`.

Run: `build/windows-release/tpj_bench.exe --ticks 1 tests/parks/stress/winding-path.park`
Expected: exit status 0, with the same four lines.

### Task 3: Measure before the simulation changes

Files: none.

Step 1: Run the runtime report. It builds both presets' runners and launches them hidden.

Run: `scripts/runtime-report.sh build/lean-routes-before.report > build/lean-routes-before.log 2>&1`
Expected: exit status 0, and the log ends with `runtime-report: wrote` and the report's path.

Step 2: Profile the bench four times with Very Sleepy, on the windows-release tpj_bench task 2 built.

Run: `W=$(wslpath -w .) && for i in 1 2 3 4; do "/mnt/c/Program Files/Very Sleepy/sleepy.exe" "/r:$W\build\windows-release\tpj_bench.exe --ticks 1 $W\tests\parks\stress\full.park" "/o:$W\build\perf\lean-routes-before-$i.sleepy"; done`
Expected: each run exits with status 0.

Step 3: From the profiles' Callstacks.txt and Symbols.txt, sum the resolution stage's samples: those whose stacks hold tpj::resolveWorld and not tpj::makeCandidate. Of those, sum the ones that also hold resolveRouteDistance, and of those, the samples whose first frames are heap allocation or freeing (RtlAllocateHeap, RtlFreeHeap, operator new, operator delete, malloc, free). Write down route distance's share of resolution and its heap share.

Step 4: Build the whole windows-release preset: `cmake.exe --build --preset windows-release`. Expected: exit status 0 and no warning. Ask the user to open tests/parks/stress/full.park in build/windows-release/ThemeParkJones.exe, drag a shop around and then draw a guest path, and say what the Debug panel's frame rate shows during each. Write the numbers down for the milestone's close.

### Task 4: Update the sim and medium specs

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/sim/medium/SPEC.md`
- Modify: `src/sim/tick_buffers.h`

Step 1: Make the two SPEC.md edits from FEATURE.md's Spec changes.

Step 2: In src/sim/tick_buffers.h, change TickBuffers's comment to say it holds the lists systems, swaps, resolvers, and finishers reuse from one tick or resolution to the next, owned by whoever steps or resolves the world.

### Task 5: Give resolvers and finishers the caller's buffers

Files:
- Modify: `src/sim/schema.h`, `src/sim/schema.cpp`, `src/sim/world.h`, `src/sim/cycle.cpp`
- Modify: every resolver and finisher: `src/sim/medium/field.h` (emptyResolvedEntries, settleSteppedEntries), `src/sim/medium/kept_field.h` (emptyKeptEntries), `src/sim/medium/ground_field.h` (emptyGroundEntries), `src/sim/medium/flow.h` (ensureLedger), `src/sim/routes/route_distance.cpp` (resolveRouteDistance), `src/sim/routes/networks.cpp` (resolvePathNetworks, resolveWalkedSurface, dropPreviousNetworks), `src/sim/park/surface.cpp` (resolvePavedSurface), `src/sim/furniture/furniture.cpp` (layFurniture), `src/sim/guests/footfall.cpp` (carryFootfall), `src/sim/guests/ground.cpp` (resolveGuestGround), `src/sim/guests/guests.cpp` (ensureDepartures, carryGuests), `src/sim/operations/operations.cpp` (resolveOffers), `src/sim/operations/facilities.cpp` (resolveFacilityOffers), `src/sim/operations/seats.cpp` (resolveSeatOffers), `src/sim/wayfinding/best_seats.cpp` (resolveBestSeats), `src/scenarios/beacons.cpp` (resolveGlows), `src/scenarios/stalls.cpp` (resolveTrack, resolveStallEntries)
- Modify: `tests/sim/cycle_test.cpp`, `tests/sim/save_test.cpp`, `tests/sim/schema_test.cpp`

Step 1: In src/sim/schema.h, replace WorldFunction with `using ResolveFunction = void (*)(World &world, TickBuffers &buffers);`. Its comment says a resolver or finisher takes the world and the resolving caller's buffers, holds no state outside the world, and clears a buffer before reading it, so its result depends on the world alone (principle 10). ResolverType's Resolve, addResolver, addFinisher, finishers(), and Finishers use it. Make the same change in src/sim/schema.cpp.

Step 2: In src/sim/world.h, declare `void resolveWorld(World &world, TickBuffers &buffers);`, `World makeCandidate(const World &world, const CommandQueue &commands, TickBuffers &buffers);`, and `void commitCommands(World &world, CommandQueue &commands, TickBuffers &buffers);`, each with the comment of its present form and "passing the buffers to every resolver and finisher". Keep the present three forms, and say each resolves with buffers of its own. The friend declarations name the buffer forms.

Step 3: In src/sim/cycle.cpp, give resolveWorld the buffers and pass them to each resolver and finisher. stepWorld(world, commands, buffers) passes its buffers to both resolveWorld calls. The buffer forms of makeCandidate and commitCommands pass theirs. The forms without buffers make a local TickBuffers and call the buffer form.

Step 4: Give each resolver and finisher listed above a second parameter `TickBuffers & /*buffers*/`, unnamed in a comment as applyKeptChanges does. In the three tests, give each registered lambda and function the same parameter. Nothing else in the tests changes.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug`
Expected: exit status 0 and no warning.

Run: `build/windows-debug/tpj_sim_tests.exe "a cycle*,a queue*,a candidate*"`
Expected: exit status 0, `All tests passed (11 assertions in 4 test cases)`.

### Task 6: Pass kept buffers where a caller keeps them

Files:
- Modify: `src/app/session/park_session.cpp`
- Modify: `src/bench/stages.cpp`

Step 1: In ParkSession::applyWithoutTick, pass Buffers to both commitCommands calls.

Step 2: In benchPark, keep a TickBuffers named resolveBuffers before the resolution stage, and resolve with it both in the untimed first resolution and in the stage's repetitions, so they share it. The ticks stage keeps its own buffers.

Step 3: Build the two targets.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core tpj_bench`
Expected: exit status 0 and no warning.

### Task 7: Keep the resolved layer's memory

Files:
- Modify: `src/sim/medium/field.h`
- Modify: `src/sim/furniture/furniture.cpp:995`
- Modify: `src/sim/operations/facilities.cpp:252`
- Modify: `src/sim/operations/operations.cpp:531`
- Modify: `src/sim/operations/seats.cpp:55`
- Modify: `src/sim/wayfinding/best_seats.cpp:40`
- Modify: `src/scenarios/stalls.cpp:159-160`

Step 1: Give ResolvedEntries a `std::vector<FieldSlot<typename F::Entry>> Retired;` after Previous, with the comment that SteppedEntries's Retired has: slots taken out of the layer, their lists emptied, kept so later publishes reuse their memory, and never visited. visitFields stays as it is.

Step 2: Add a function in field.h that retires a list of slots into Retired: for each slot, clear Entries and ByPlace, set Ordered to false and Source to NULL_KEY, and move it to Retired; then clear the list.

Step 3: Change emptyResolvedEntries. When the field's entity holds ResolvedEntries, retire its Previous, swap Slots with Previous, and set HasPrevious to true, changing the component in place. Otherwise emplace an empty ResolvedEntries. Then get or emplace the stepped entries as it does now. The world after is the same as before: Slots empty, Previous holding the last resolution's slots, HasPrevious true when there was one.

Step 4: Change settleSteppedEntries to retire Previous instead of clearing it.

Step 5: Change publishResolved to `template <FieldDefinition F> std::vector<PlacedEntry<typename F::Entry>> &publishResolved(World &world, EntityKey source);`. It keeps every check it makes now, but builds the field's name as a std::string only on the paths that throw: the route-distance fields' names are longer than a string's own buffer, so building the name at every call allocates at every publish. Make the same change in publishStepped. It takes a slot from Retired when there is one, as publishStepped does, sets its Source, inserts it in order, and returns its Entries, which are empty. Its comment says the list stays valid until the next publish into the field.

Step 6: Change the five other publishers in src/sim to fill the returned list from the list they make now, with `.assign(entries.begin(), entries.end())` or the same for the call's result held in a local. Their own lists stay until quiet-placement. In src/scenarios/stalls.cpp resolveStallEntries, push each single entry onto the list its publishResolved gives.

Step 7: Build and run the sim tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios && build/windows-debug/tpj_sim_tests.exe`
Expected: exit status 0, ending with `All tests passed`.

### Task 8: Keep route distance's lists

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: In the anonymous namespace, add `struct RouteLists` with: `std::vector<std::vector<Leaving>> Steps` and `std::vector<std::vector<Leaving>> SpareSteps`, the steps leaving each node and the inner lists not in use; `std::vector<std::optional<RouteEntry>> Entries`; `std::vector<std::pair<double, uint32_t>> Queue`; `std::vector<uint8_t> Settled`; `std::vector<uint32_t> Anchored`; and `std::vector<EntityKey> Sources`. Its comment says these are the lists the route-distance resolver keeps in the caller's buffers.

Step 2: Replace leavingSteps with `void fillLeavingSteps(const Network &network, RouteLists &lists)`. It sizes Steps to the network's nodes with resizeKeeping from sim/tick_buffers.h, clears each inner list, and pushes each edge's two steps as leavingSteps does now.

Step 3: Add `void searchEntries(const Network &network, EntityKey source, RouteLists &lists)`, routeEntries's body on the kept lists. It reads lists.Steps, which the caller has filled for the network. It fills Entries with fillGrowing to the node count of nullopt, Settled to 0, clears Queue, and takes the anchored nodes with the fill form `network.anchoredNodes(source, lists.Anchored)`. Queue is a heap: push with push_back then std::push_heap, and pop with std::pop_heap then pop_back, both with std::greater<>, which gives std::priority_queue's order. The next-step pass is unchanged.

Step 4: routeEntries keeps its signature and result. It makes a local RouteLists, calls fillLeavingSteps and searchEntries, and returns a copy of Entries.

Step 5: publishRouteDistance<Kind> takes the RouteLists. It gathers the anchors into Sources, cleared first, then sorts and dedupes them as now. It calls fillLeavingSteps once. For each source it calls searchEntries, then fills the list publishResolved<RouteDistance<Kind>>(world, source) gives with the reachable nodes' places and entries, in node order.

Step 6: resolveRouteDistance names its buffers parameter, takes `RouteLists &lists = buffers.get<RouteLists>()`, and passes the lists to both kinds.

Step 7: Build and run the sim and integration tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests && build/windows-debug/tpj_sim_tests.exe && build/windows-debug/tpj_integration_tests.exe`
Expected: exit status 0, each ending with `All tests passed`.

### Task 9: Compare the simulation

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/lean-routes/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/lean-routes/scenarios-before.txt build/lean-routes/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/lean-routes/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/lean-routes/full-before.txt build/lean-routes/full-after.txt`
Expected: exit status 0.

### Task 10: Confirm the criteria

Files: none.

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug`
Expected: exit status 0 and no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed.

Run: `scripts/tidy.sh`
Expected: exit status 0 and no warning.

Step 2: Compare the runtime report.

Run: `scripts/runtime-report.sh build/lean-routes-after.report > build/lean-routes-after.log 2>&1 && scripts/runtime-report.sh --compare build/lean-routes-before.report build/lean-routes-after.report`
Expected: exit status 0, and no stage marked result-changed. Report each stage's change.

Step 3: Profile again as task 3 step 2 does, writing build/perf/lean-routes-after-$i.sleepy, and sum the samples as task 3 step 3 does. Report route distance's share of resolution and its heap share, before and after.

Step 4: Build the whole windows-release preset: `cmake.exe --build --preset windows-release`. Expected: exit status 0 and no warning.

### Task 11: Review and commit

Step 1: Stage the feature's paths by name, never with `git add -A`. Dispatch the reviewer on the staged diff with FEATURE.md, src/sim/SPEC.md, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, src/bench/SPEC.md, and docs/principles.md.

Step 2: Commit once with the commit-hygiene skill, subject `Sim: Keep route distance's lists across resolutions`.
