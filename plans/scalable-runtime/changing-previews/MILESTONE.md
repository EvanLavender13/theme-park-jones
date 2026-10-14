# Milestone: Changing Previews

Slice: none

Status: planned

## Summary

changing-previews makes dragging a shop and drawing a guest path on full.park smooth.

Today, each frame in which the tool's edit changes copies the whole world, applies the edit, runs every resolver over the whole park, and builds the ghost, which plans a path edit again. On full.park that is about 11 ms for a box edit and about 16 ms for a path edit with its ghost, on windows-release. In play, the Debug panel's frame rate dipped to about 100 while dragging a shop and to about 50 while drawing a path.

The cause is one design flaw, not a slow resolver (RESEARCH.md). Each resolver is a single task that derives its output for the whole park, so any edit makes every one of them run again. Build systems and incremental compilers solve this by deriving in small units and re-running only the units whose inputs changed. This milestone builds that mechanism into resolution and moves the resolvers onto it:

- Derived values are split into units keyed by what they derive from. Spatial values, such as paving, connections, furniture, and guest ground, are split by place. Route distance has one unit per source, and best seats one per seat.
- Each unit records what it read. When an edit or a tick changes something, only the units that read it run again. When a unit runs again and gives the same output, the units that read it don't run.
- The candidate is a layer over the world. It holds only what the edit changed and reads everything else from the world, so the world is never copied.
- Route distance and best seats can't be split by place, because one edit can change a distance far away. Their units are updated by a dynamic shortest-path update, which touches only the nodes whose distance actually changes.
- Committing an edit, undo, and redo resolve the same way, so they get faster too.

The result must equal, exactly, a resolution from nothing (principle 8, decision 0025). Units are how resolution stores its work. A world's walk, hash, save, and comparison see the same values that a resolution from nothing gives.

## Acceptance criteria

1. Dragging is smooth on full.park. tpj_bench runs two scripted drags through the kept preview on full.park:
    - A shop moved 0.5 m along a guest path on each frame.
    - A new guest path drawn from an existing path, its end moving 0.5 m into open ground on each frame.

    One tick passes between every two frames. For each changed frame, the bench times the acceptance check, the candidate, the shop context, and the ghost together. On windows-release, the median frame must be under 1 ms for the shop drag and under 2 ms for the path drawing. Feature 2 measures both drags before anything else changes. (check: scripts/runtime-report.sh and its --compare)
2. In play on full.park in the windows-release app, the Debug panel's frame rate holds at the display's refresh rate while dragging a shop and while drawing a guest path. The app caps the frame rate at the refresh rate, which is 165 on the user's display. Before the milestone, it dipped to about 100 and about 50. (play)
3. A changed frame does no whole-park work. In particular:
    - The world is never copied.
    - A path edit is planned once per changed frame. The acceptance check, applying the edit, and the ghost all use that one plan.
    - Each function that criterion 1 times reads only what the edit, and the ticks since the last frame, reach. That covers every resolver and finisher, and also the acceptance check, the shop context, and the ghost.
    - Only these may grow with the park's size:
        - Renumbering network nodes.
        - Visiting each route-distance source once, to update the nodes the change reaches.

    (check: read each function criterion 1 times; criterion 1's report)
4. The functions criterion 1 times allocate nothing once their lists have grown, as docs/conventions.md requires of per-frame work. That covers the units' recorded reads, the candidate's layer, and the lists lean-routes left to this milestone: the other resolvers' readers, such as parkSegments and layoutOf, and the comparison that settles the stepped entries. The watched-previews milestone extends the allocation check to them. Until then, this milestone reads that code before it closes, as decision 0041 requires. (check: read each function criterion 1 times)
5. The result is exact. The reference is a resolution from nothing: every unit derived again, with no unit kept. In each of these cases, the result equals that reference by worldsEqual:
    - The kept candidate, after every frame of both scripted drags, with a tick between frames. The reference is the same edit on the ticked world.
    - A world that commits each chosen edit of the save checks.
    - A world that commits a moved box, which the save checks add.

    (ctest: a world check over the scripted drags on full.park and the park files in tests/parks; the save checks)
6. The simulation is unchanged, except for the two changes the user chose (Decisions below). tpj_scenarios --compare, run on windows-debug output from before and after each feature, shows no difference for:
    - its scenarios,
    - every park in tests/parks,
    - full.park over 300 ticks.

    The exception is a feature that carries out one of those changes. It reports its differences, and every later feature compares against its output. (check: tpj_scenarios --compare; the report's --compare)
7. A decision record states the rules simulation work must follow to run on more than one thread:
    - The results are identical to one thread's, whatever the number of threads and the order they finish in.
    - Each worker writes only to what it alone owns.
    - Only one thread changes the world, in a fixed order.
    - No worker reads the clock.

    The record names the check that the first milestone to run simulation work on threads must add: a tpj_scenarios --compare between one worker and several. (check: read the record)
8. Every SPEC.md whose behavior or public functions change says so. Decision 0025 says that a candidate may be a layer over the world instead of a copy. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Medium

None. The milestone adds, samples, and publishes no fields or flows. Units, their recorded reads, the candidate's layer, and kept buffers never appear in a world's walk, hash, save, or comparison.

## Dependencies

- allocation-free-loops: the kept preview. Complete.
- Decision 0025: a candidate is the derivation of tentative intent over the world. The milestone keeps that result exactly and changes only how it is stored and resolved.
- measured-runtime: tpj_bench, the runtime report, and full.park. Complete.

## Core feature

derived-units. It builds the mechanism: units, recorded reads, re-running only the units whose reads changed, and early cutoff. It also adds the reference resolution from nothing, the world check of criterion 5, and the bench's two drags. The paved surface moves onto it first. That surface's reach is already computed by surfaceAround and pointsAffected, and a shop drag leaves it unchanged. Every later feature moves more resolvers onto the same mechanism, so if the core is wrong, everything after it is wrong.

## Features

1. `lean-routes`: route distance builds each network's steps once per resolution and keeps its lists, and resolvers take the caller's kept buffers. Merged.
2. `derived-units`: the mechanism, as the core feature describes, with the paved surface as its first unit kind. It also adds the bench's two scripted drags and their before measurements, the reference resolution from nothing, and the world check of criterion 5. Its units reuse their storage, and early cutoff compares a unit's output with its last one in kept lists, which replaces the settling comparison's new lists. For criteria 1, 3, 4, and 5. Depends on: feature 1.
3. `layered-candidate`: the candidate as a layer over the world, holding only what the edit changed, with no copy. The world records the tick of each component's last change, so after a tick the units whose reads changed run again. A path edit is planned once per changed frame. The acceptance check and the shop context read only what the edit reaches, in kept lists instead of new copies of parkSegments, layoutOf, and the solids. Depends on: feature 2.
4. `spatial-units`: the path networks' connections, the walked surface, furniture, guest ground, and the seat, facility, food, and drink offers move onto units split by place. Guest ground's groups are numbered so that a join or split elsewhere leaves them unchanged, and the walk still sees the numbering a resolution from nothing gives. The ghost reads the edited units. Each resolver it moves stops making new lists of the park's segments and layout. Depends on: feature 3.
5. `graph-updates`: route distance and best seats keep one unit per source or seat. Each unit is updated by a dynamic shortest-path update from the edges that changed, with edge lengths added in whole millimetres (Decisions, 1). Each source's entries and each node's best seats are kept in an order that renumbering nodes doesn't disturb, such as by place. The walk still emits them in node order, as a resolution from nothing does, so no frame rebuilds a source's whole list. Depends on: feature 4.
6. `local-carrying`: after an edit, only the guests and footfall on carriers or ground that the edit changed are carried and re-aimed (Decisions, 2). Depends on: feature 4.
7. `thread-rules`: the decision record of criterion 7. Depends on: none.

## Deepening candidates

Unordered pool this milestone draws later features from.

- None yet.

## Decisions

The user chose both on 2026-10-13. Each feature that carries one out writes its decision record, and CAPABILITY.md lists it as an exception to the rule that a speedup never changes the answer.

1. Route distance adds edge lengths in whole millimetres (feature 5). An exact dynamic update must give the same distances however an edge is cut, and floating-point sums don't do that. When a shop's connection cuts a path in two, the sum over the halves can differ in its last bit from the old edge's length. That difference reaches every node beyond the cut, for every source whose routes cross it, on every frame of a shop drag. Whole millimetres make the halves sum to exactly the old length. Distances change by under a millimetre per edge, and a tie between two equal routes may break the other way once.
2. An edit carries and re-aims only the guests whose carrier or ground it changed (feature 6). Today every resolution re-checks every guest against the new ground and resets every guest's aim, so any edit re-aims all 2,000 guests on full.park. A guest standing where it doesn't fit, such as at a door's end, can also be moved by an edit far away. Carrying every guest would keep about 1 ms of work in every changed frame.

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

- The design follows build systems (minimality and early cutoff), Salsa (recorded reads per keyed call), Recast (rebuilding only the tiles a change overlaps), and Ramalingam–Reps and D* Lite (updating only the distances that change). Furniture's chain hash is already a small case of recorded reads.
- The projection is an estimate, not a measurement. After features 2 to 6, a changed frame's work covers:
    - what the edit and the last tick reach,
    - one cheap visit per route-distance source,
    - a renumbering of nodes.

    That puts a shop drag frame at about 0.5 to 1 ms, and a path drawing frame at about 1 to 2 ms. Criterion 1 is reached only when features 2 to 6 have all landed. A path that joins two parts of the network, and shortens routes across the park, costs in proportion to the distances it really changes.

The details are in RESEARCH.md.
