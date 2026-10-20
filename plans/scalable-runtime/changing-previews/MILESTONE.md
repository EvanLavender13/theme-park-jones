# Milestone: Changing Previews

Slice: none

Status: planned

## Summary

changing-previews makes dragging a shop and drawing a guest path on full.park smooth.

Today, each frame in which the tool's edit changes copies the whole world, applies the edit, runs every resolver over the whole park, and builds the ghost, which plans a path edit again. On full.park that is about 11 ms for a box edit and about 16 ms for a path edit with its ghost, on windows-release. In play, the Debug panel's frame rate dipped to about 100 while dragging a shop and to about 50 while drawing a path.

The cause is one design flaw, not a slow resolver (RESEARCH.md). Each resolver is a single task that derives its output for the whole park, so any edit makes every one of them run again. Build systems and incremental compilers solve this by deriving in small units and re-running only the units whose inputs changed. This milestone builds that mechanism into resolution and moves the resolvers onto it:

- Derived values are split into units keyed by what they derive from. Spatial values, such as paving, connections, furniture, and guest ground, are split by place. Route distance has one unit per source, and best seats one per seat.
- Each unit records what it read. When an edit or a tick changes something, only the units that read it run again. When a unit runs again and gives the same output, the units that read it don't run.
- The candidate is kept between frames. Each changed frame copies into it only what the world or the last edit changed, so the world is never copied whole.
- Route distance and best seats can't be split by place, because one edit can change a distance far away. Their units are updated by a dynamic shortest-path update, which touches only the nodes whose distance actually changes.
- Committing an edit, undo, and redo resolve the same way, so they get faster too.

The result must equal, exactly, a resolution from nothing (principle 8, decision 0025). Units are how resolution stores its work. A world's walk, hash, save, and comparison see the same values that a resolution from nothing gives.

## Acceptance criteria

1. Dragging is smooth on full.park. tpj_bench runs two scripted drags through the kept preview on full.park:
    - A shop moved 0.5 m a frame along its face, out to 2 m and back, so it stays clear of the paths beside it.
    - A new guest path drawn from an existing path, its end moving 0.5 m into open ground on each frame.

    One tick passes between every two frames. For each changed frame, the bench times the acceptance check, the candidate, the shop context, and the ghost together. On windows-release, the median frame must be under 1 ms for the shop drag and under 2 ms for the path drawing. Feature 2 measures both drags before anything else changes. (check: scripts/runtime-report.sh and its --compare)
2. In play on full.park in the windows-release app, the Debug panel's frame rate holds at the display's refresh rate while dragging a shop and while drawing a guest path. The app caps the frame rate at the refresh rate, which is 165 on the user's display. Before the milestone, it dipped to about 100 and about 50. (play)
3. A changed frame does no whole-park work. In particular:
    - The world is never copied whole.
    - A path edit is planned once per changed frame. The acceptance check, applying the edit, and the ghost all use that one plan.
    - Each function that criterion 1 times reads only what the edit, and the ticks since the last frame, reach. That covers every resolver and finisher, and also the acceptance check, the shop context, and the ghost.
    - Only these may grow with the park's size:
        - Renumbering network nodes.
        - Finding which paths join which, once per edit, as Parkitect's flood fill does.
        - Visiting each route-distance source once, to update the nodes the change reaches.

    (check: read each function criterion 1 times; criterion 1's report)
4. The functions criterion 1 times allocate nothing once their lists have grown, as docs/conventions.md requires of per-frame work. That covers the units' recorded reads, the candidate's change records, and the lists lean-routes left to this milestone: the other resolvers' readers, such as parkSegments and layoutOf, and the comparison that settles the stepped entries. The allocation check watches every resolution step but those on its exception list (watched-resolution). Until then, this milestone reads that code before it closes, as decision 0041 requires. (check: read each function criterion 1 times)
5. The result is exact. The reference is a resolution from nothing: every unit derived again, with no unit kept. In each of these cases, the result equals that reference by worldsEqual:
    - The kept candidate, after every frame of the candidate check's two drags on warm.park, a moved shop and a drawn path, with a tick between frames. The reference is the same edit on the ticked world, saved, loaded, and resolved.
    - A world that commits each chosen edit of the save checks.
    - A world that commits a moved box, which the save checks add.

    (ctest: the drag world check on warm.park, which derived-units adds, and on furnished-paths.park, with a world committing each frame, which furniture-units adds; the save checks)
6. The simulation is unchanged, except for the two changes the user chose (Decisions below). tpj_scenarios --compare, run on windows-debug output from before and after each feature, shows no difference for:
    - its scenarios,
    - every park in tests/parks,
    - full.park over 300 ticks.

    The exception is a feature that carries out one of those changes. It reports its differences, and every later feature compares against its output. A feature may also take resolution bookkeeping off the world's walk, which changes every hash once and nothing else. It does that in a step of its own, shows by reading that step's diff that nothing the walk still covers changes, and records its outputs right after it to compare against. (check: tpj_scenarios --compare; the report's --compare)
7. A decision record states the rules simulation work must follow to run on more than one thread:
    - The results are identical to one thread's, whatever the number of threads and the order they finish in.
    - Each worker writes only to what it alone owns.
    - Only one thread changes the world, in a fixed order.
    - No worker reads the clock.

    The record names the check that the first milestone to run simulation work on threads must add: a tpj_scenarios --compare between one worker and several. (check: read the record)
8. Every SPEC.md whose behavior or public functions change says so. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Medium

None. The milestone adds, samples, and publishes no fields or flows. Units, their recorded reads, the change records, and kept buffers never appear in a world's walk, hash, save, or comparison.

## Dependencies

- allocation-free-loops: the kept preview. Complete.
- Decision 0025: a candidate is the derivation of tentative intent over the world. The milestone keeps that result exactly and changes only how it is stored and resolved.
- measured-runtime: tpj_bench, the runtime report, and full.park. Complete.

## Core feature

derived-units. It builds the mechanism: units, the change records they start from, recorded reads, re-running only the units whose reads changed, and early cutoff. It also adds the bench's two drags, measured before anything else changes, the world checks' moved box, which compares an incremental resolution with a load's resolution from nothing, and the world check over the scripted drags, run on a candidate the preview's way, refilled frame after frame with a tick between frames. The path ground, the ground lines and corners that every surface, network, and furniture resolver derives again today, moves onto it first, because every later unit kind reads it. Every later feature moves more resolvers onto the same mechanism, so if the core is wrong, everything after it is wrong.

## Features

1. `lean-routes`: route distance builds each network's steps once per resolution and keeps its lists, and resolvers take the caller's kept buffers. Merged.
2. `derived-units`: the mechanism, as the core feature describes, with the path ground as its first unit kinds. It also adds the bench's two scripted drags and their before measurements, the preview's candidate refilled in place, and the world check over the scripted drags, which each later feature keeps passing. Units reuse their storage, and early cutoff compares a unit's output with its last one. For criteria 1, 3, 4, and 5. Merged. Depends on: feature 1.
3. `kept-checks`: a path edit is planned once per changed frame, into lists the caller keeps. The acceptance check, applying the edit to the candidate, the shop context, and the ghost use that one plan, and a cycle applies commands with its buffers. None of them allocates once its lists have grown, and every surface they build refills storage the caller keeps. Resolution tells a trace each step, so the allocation check watches both drags and a round of commits, pausing only inside the resolvers no feature has moved yet. Merged. Depends on: feature 2.
4. `unit-surface`: the paved surface moves onto units, one per point or segment that owns paving. Hiding reads only nearby pieces of the same group, through a grid filed from the pieces. Groups are labelled by their lowest point key, so a path drawn from an existing one leaves every label as it was. Both surfaces store their runs per owner and walk the words they walk today, and their readers read the runs. The paved-surface resolver stops making new lists of the park's segments and layout. The allocation check's drags and commits watch its units and the two surface resolvers too. Merged. Depends on: feature 3.
5. `unit-connections`: each door's connection and walkway, and the walked surface, move onto units. A connection finds nearby paving through unit-surface's grid, and the path networks read the connections from the units instead of measuring every door against every piece. The walked surface shares the paved surface's units and hides again only near walkways. The walked-surface resolver stops making new lists of the park's segments and layout. Merged. Depends on: feature 4.
6. `watched-resolution`: the allocation check's drags and commits watch every resolution step except a named exception list of the steps that still allocate, each naming the feature that removes it, and the check fails when an excused step stops allocating. Depends on: feature 5.
7. `route-updates`: route distance adds lengths in whole millimetres and updates only the entries an edit changes, instead of searching from every source again. Depends on: feature 6.
8. `furniture-units`: furniture lays again only the chains an edit reaches, and reads only the obstacles near them, instead of hashing every chain's surroundings on every resolution. Depends on: feature 7.
9. `thread-rules`: the decision record of criterion 7. Depends on: none.

Later features are planned one at a time, from the code, after the last one merges. Each takes the largest whole-park cost still measured in a drag frame (Research notes). After each one, the frame is measured again and played on full.park. Once dragging holds the display's refresh rate in play, the user decides whether to go on to criterion 1.

The allocation check's exception list names the resolution steps that still allocate. If the milestone stops before its last feature, fix each remaining entry or record it in docs/exceptions.md with its reason. Don't leave it only in the test file.

The whole-park work still left after route-updates, with what each part needs from the others:

- Furniture: laid only where an edit reaches it (furniture-units).
- Best seats: one search over the whole network into one slot. It needs to know which seats changed, which the seat places and seat offers don't record yet.
- Carrying guests and footfall: every guest is carried. Carrying only some needs to know which guest ground changed, which needs guest ground on units first.
- Copying the world into the candidate.
- Planning a path edit, its acceptance check, and the ghost's edited surface: they copy the layout and derive the whole park's ground (worldGround, keptCurves, surfaceAround, pointsAffected).
- The ghost's other parts: it compares every furniture piece of both worlds, draws every walkway, and checks every box.
- Guest ground, the path networks' carriers, and the offers: each rebuilt for the whole park.
- Route distance's slot moves: writing a source's changed entries moves the rest of its list, about 1.7 MB per path-drag frame on full.park, growing with sources times nodes.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Resolve the candidate when the cursor rests: while the cursor moves, each drag frame builds only the ghost and the accept check, about 2.7 ms of a path drag's frame on full.park, and the full candidate resolves when the cursor rests or the edit is placed. It changes what the preview shows during a fast drag, so it's gameplay. The user chose on 2026-10-14 to keep the planned features first and consider this as a final feature.

## Decisions

The user chose both on 2026-10-13. Each feature that carries one out writes its decision record, and CAPABILITY.md lists it as an exception to the rule that a speedup never changes the answer.

1. Route distance adds edge lengths in whole millimetres (route-updates, decision 0042). An exact dynamic update must give the same distances however an edge is cut, and floating-point sums don't do that. When a shop's connection cuts a path in two, the sum over the halves can differ in its last bit from the old edge's length. That difference reaches every node beyond the cut, for every source whose routes cross it, on every frame of a shop drag. Whole millimetres make the halves sum to exactly the old length. Distances change by under a millimetre per edge, and a tie between two equal routes may break the other way once.
2. An edit carries and re-aims only the guests whose carrier or ground it changed (a later feature). Today every resolution re-checks every guest against the new ground and resets every guest's aim, so any edit re-aims all 2,000 guests on full.park. A guest standing where it doesn't fit, such as at a door's end, can also be moved by an edit far away. Carrying every guest would keep about 1 ms of work in every changed frame.

## Open questions

None.

## Research notes

- Before the milestone, on windows-release with full.park, the Debug panel's frame rate dipped to about 100 while dragging a shop and to about 50 while drawing a guest path (2026-10-13). After lean-routes, tpj_bench puts these at:

    | Stage | Time |
    |---|---|
    | Resolution | 9.3 ms |
    | Box preview | 11.3 ms |
    | Path preview | 12.1 ms |
    | Path acceptance check | 0.4 ms |
    | Path ghost | 3.3 ms |

- After unit-connections, the runtime report puts the shop-drag frame at 6.3 ms and the path-drag frame at 8.2 ms on full.park. Four Very Sleepy profiles of windows-release tpj_bench (build/perf/layered-1.sleepy to layered-4.sleepy) split the timed part of each frame:

    | Part | Shop drag | Path drag |
    |---|---|---|
    | Route distance | 25% | 21% |
    | Copying the world | 15% | 12% |
    | Furniture | 16% | 11% |
    | Best seats | 14% | 11% |
    | Carrying guests and footfall | 12% | 11% |
    | The ghost's edited surface | under 1% | 16% |
    | Guest ground | 5% | 4% |
    | Path networks | 3% | 2% |
    | Offers | 3% | 3% |
    | Planning the edit | 2% | 3% |

- After furniture-units, the shop-drag frame is 3.85 ms and the path-drag frame 5.97 ms. The largest cost in the path drag's frame is the ghost's edited surface, 22% (about 1.3 ms). The largest in the shop drag's is copying the world, 23%, which is 14% of the path drag's. Best seats is 20% and 14% (candidate-ghost/RESEARCH.md, build/perf/next-1.sleepy to -4).
- The design follows build systems (minimality and early cutoff), Salsa (recorded reads per keyed call), Recast (rebuilding only the tiles a change overlaps), and Ramalingam–Reps and D* Lite (updating only the distances that change). Furniture's chain hash is already a small case of recorded reads.
- The projection is an estimate, not a measurement. After the remaining features, a changed frame's work covers:
    - what the edit and the last tick reach,
    - one cheap visit per route-distance source,
    - a renumbering of nodes.

    That puts a shop drag frame at about 0.5 to 1 ms, and a path drawing frame at about 1 to 2 ms. Criterion 1 is reached only when all the whole-park work above is gone. A path that joins two parts of the network, and shortens routes across the park, costs in proportion to the distances it really changes.

The details are in RESEARCH.md.
