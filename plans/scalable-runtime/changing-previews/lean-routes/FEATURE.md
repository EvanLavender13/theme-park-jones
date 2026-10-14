# Feature: Lean Routes

## Summary

lean-routes makes the route-distance resolver do its work once and keep its memory, and it measures previews of a path edit so the milestone can report against them. Today route distance builds the network's list of steps leaving each node again for every source. It also searches each source with new lists, and publishes a new list per source that the next resolution frees. Route distance is 31% of a preview on full.park, the largest share of any resolver, and 42% of its time is heap work. After this feature, resolvers and finishers take the caller's kept buffers, as systems and swaps do, and stepWorld, commitCommands, resolveWorld, and makeCandidate pass them. The medium's resolved entries keep their memory from one resolution to the next, as its stepped entries already do. Route distance builds each network's step lists once per resolution and searches and publishes in lists it keeps. tpj_bench gains stages for a guest path edit's acceptance check, its preview, and its ghost, and for the ghost of the existing preview. What the player sees doesn't change. This covers milestone criteria 1 and 2 and route distance's part of criterion 3.

## Acceptance criteria

1. tpj_bench times, after the preview of the box edit, its ghost, and then a guest path edit's acceptance check, preview, and ghost, as the stages ghost, path-acceptance, path-preview, and path-ghost. The path edit is a 12 m path from the middle of a guest segment out into the park, on the first guest segment and side the world accepts one on. The resolution stage resolves a park that is already resolved, as an edit's resolution does, and keeps its buffers across repetitions, as the app keeps its own. (check: `build/windows-release/tpj_bench.exe --ticks 1 tests/parks/stress/full.park` prints the four new stage lines)
2. The runtime report from before the feature's simulation changes, with the new stages, and the report after it are compared. Each stage's change is reported, and no stage is marked result-changed. Very Sleepy profiles from before and after give route distance's share of resolution and its share of heap work. (check: the commands in PLAN.md tasks 3 and 10)
3. The Debug panel's frame rate while dragging a shop and while drawing a guest path on full.park in the windows-release app is read before the feature changes the simulation, for milestone criterion 2's comparison at the milestone's close. (play)
4. Resolvers and finishers take the world and the caller's TickBuffers. stepWorld passes its buffers to the resolutions it runs, and resolveWorld, commitCommands, and makeCandidate take the caller's buffers. Their forms without buffers use buffers of their own. The app's session passes its kept buffers when it applies edits while paused. A world resolves to the same result whatever buffers it is given. (check: tpj_scenarios --compare; ctest)
5. Route distance builds each network's step lists once per resolution, not once per source. With kept buffers, its search and its publishing reuse their memory, and the field's resolved entries keep the memory of each resolution's entries for the next one's publishes. Publishing makes no string unless it refuses. (check: read route_distance.cpp and field.h; the profile after in criterion 2 shows route distance's heap share near zero in the resolution stage, whose stacks hold resolveWorld and not makeCandidate)
6. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. A candidate still equals the world that commits the same commands. (check: tpj_scenarios --compare, commands in PLAN.md tasks 1 and 10; ctest)
7. src/sim/SPEC.md says resolvers and finishers take the caller's buffers and which functions pass them. src/sim/medium/SPEC.md says publishResolved gives a list to fill and the field keeps its memory. src/bench/SPEC.md lists the new stages. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: read the three files; the commands in PLAN.md task 10)

## Tests

None. The world checks in tests/sim cover candidates and resolution, and the scenarios comparison shows the simulation unchanged. Milestone feature 5's allocation check watches the whole preview once every resolver keeps its memory.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows. Route distance still publishes the same entries into the same two fields. Kept buffers and a field's retired slots are never part of a world's walk, hash, save, or comparison.

## Spec changes

src/sim/SPEC.md, replace the paragraph that begins "Systems and swaps take the world and a TickBuffers" with:

"Systems, swaps, resolvers, and finishers take the world and a TickBuffers, which holds the lists they reuse from one tick or resolution to the next. stepWorld(world, commands, buffers) passes the caller's buffers to every system and swap, and to every resolver and finisher of the resolutions it runs. resolveWorld(world, buffers), commitCommands(world, commands, buffers), and makeCandidate(world, commands, buffers) pass them to every resolver and finisher. stepWorld(world, buffers) is the same cycle with no commands. If you step or resolve a world repeatedly, keep one TickBuffers and pass it every time. A TickBuffers holds one value of each type a module asks for, made on first use and kept. It is never part of a world, so walks, hashes, saves, and copies never see it. Each system, swap, resolver, and finisher clears a list before it reads it, so a world steps and resolves to the same result whatever buffers it is given. stepWorld(world, commands), stepWorld(world), resolveWorld(world), commitCommands(world, commands), and makeCandidate(world, commands) use buffers of their own."

src/sim/medium/SPEC.md, replace the bullet "Resolved entries are derived data (principle 1). Resolvers publish them with publishResolved, and they are readable at once. Every resolution replaces every source's entries in full." with:

"- Resolved entries are derived data (principle 1). Resolvers publish them with publishResolved, which gives the source's entries for the resolution as an empty list for the resolver to fill. The list is readable at once and stays valid until the next publish into the field. Every resolution replaces every source's entries in full. The field keeps the memory of a resolution's entries for the next resolution's publishes."

src/bench/SPEC.md, replace the Stages list and the sentence after it with:

"- resolution of a park already resolved, as an edit's resolution is, with buffers kept across repetitions, as the app keeps them
- a preview of an edit that leaves the park as it is, and its ghost (only when the park has a box)
- a guest path edit's acceptance check, its preview, and its ghost (only when the park has a guest segment with a ground line). The edit is a path from the middle of a guest segment to a place PATH_SPUR_LENGTH beyond the segment's side. It uses the first such path the world accepts, trying segments in ascending key order and, on each, first the side a quarter turn counter-clockwise from the segment's direction and then the other.
- the food overlay's shape
- the food overlay's shades
- the park mesh
- the guest mesh
- a run of ticks with no commands

It leaves the loaded world unchanged. Each stage except ticks is warmed up and then repeated. Ticks times each of its calls. Each stage gives the median, least, and greatest of its times in nanoseconds. It also gives a result computed from its work: a hash, a vertex count, or a count of values. The acceptance check's count is 1. The result keeps the work from being optimized away. If a build did different work, its result is different. If the park has a guest segment with a ground line but the world accepts no such path on any, benchPark throws."

## Files affected

- Modify: `src/sim/schema.h`
- Modify: `src/sim/schema.cpp`
- Modify: `src/sim/world.h`
- Modify: `src/sim/cycle.cpp`
- Modify: `src/sim/tick_buffers.h`
- Modify: `src/sim/medium/field.h`
- Modify: `src/sim/medium/kept_field.h`
- Modify: `src/sim/medium/ground_field.h`
- Modify: `src/sim/medium/flow.h`
- Modify: `src/sim/routes/route_distance.cpp`
- Modify: `src/sim/routes/networks.cpp`
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/sim/furniture/furniture.cpp`
- Modify: `src/sim/guests/footfall.cpp`
- Modify: `src/sim/guests/ground.cpp`
- Modify: `src/sim/guests/guests.cpp`
- Modify: `src/sim/operations/operations.cpp`
- Modify: `src/sim/operations/facilities.cpp`
- Modify: `src/sim/operations/seats.cpp`
- Modify: `src/sim/wayfinding/best_seats.cpp`
- Modify: `src/scenarios/beacons.cpp`
- Modify: `src/scenarios/stalls.cpp`
- Modify: `src/app/session/park_session.cpp`
- Modify: `src/bench/stages.cpp`
- Modify: `src/bench/stages.h`
- Modify: `tests/sim/cycle_test.cpp`
- Modify: `tests/sim/save_test.cpp`
- Modify: `tests/sim/schema_test.cpp`
- Modify: `src/sim/SPEC.md`
- Modify: `src/sim/medium/SPEC.md`
- Modify: `src/bench/SPEC.md`

## Dependencies

- allocation-free-loops: TickBuffers, the kept-list helpers in sim/tick_buffers.h, and the stepped layer's retired slots, which the resolved layer follows.
- measured-runtime: tpj_bench and the runtime report.

## Out of scope

- The other resolvers' own lists and returning readers, such as parkSegments and layoutOf. Features 2 to 4 of the milestone take them. They gain the buffers parameter here and ignore it.
- settleSteppedEntries' comparison of each source's entries by their words, which makes new lists. quiet-placement, feature 4, takes it with the other finishers.
- The preview's own candidate and buffers. previewEdit and buildGhostMesh still make a candidate with buffers of their own until kept-candidate, feature 5.

## Open questions

None.
