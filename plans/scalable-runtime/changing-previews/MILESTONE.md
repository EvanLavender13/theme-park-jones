# Milestone: Changing Previews

Slice: none

Status: planned

## Summary

changing-previews makes editing on full.park cheaper, and makes the previews of a changing edit follow docs/conventions.md's rule against allocating in per-frame work. Each frame the tool's edit changes, the app makes the preview again. It checks that the world accepts the edit, copies the world, applies the edit, and resolves the whole copy. Applying the edit checks acceptance again, and the ghost checks it a third time and derives the edited surface again. On full.park a preview costs about 13 ms on windows-release, and the app also makes it again at every tick while an overlay is shown and the preview holds a candidate. A profile (RESEARCH.md) found that no single resolver dominates. Three resolvers that hold work repeating with no new answer take about 55% of a preview: route distance rebuilds the network's step lists for every source, the connections compare every door with every surface piece, and the walked surface derives the paved surface again. The profile can't separate the repeated part from the rest of each resolver. Heap work is about a fifth. This milestone measures what a drag costs, removes the repeated work, and makes resolution, the candidate, the ghost, and the acceptance check reuse their memory. While the edit is held still and only ticks pass, the kept candidate follows the ticks by re-running only the resolvers that read the park's running state, since route distance, the networks, the surfaces, and furniture derive from intent alone and about 70% of a preview gives the same answer at each tick. The allocation check then watches previews of a changing edit, the preview made again at each tick, and applying a command, as decision 0041 leaves to this milestone. The faster resolution also speeds every committed edit, undo, and redo. The milestone also records the rules that simulation work must follow to run on several threads, so later milestones can resolve across cores or make previews off the frame's thread.

## Acceptance criteria

1. tpj_bench times, on full.park, the preview of a guest path edit that the world accepts, that edit's acceptance check, and the ghost of each previewed edit, next to the existing preview stage. The runtime report before and after the milestone compares them. (check: scripts/runtime-report.sh and its --compare)
2. The Debug panel's frame rate while dragging a shop and while drawing a guest path on full.park in the windows-release app is read before the milestone and after it. (play)
3. A resolution does once what repeats today. Route distance builds each network's steps from each node once per resolution, not once per source. Each door's connection is found from the pieces and paths near the door, so that work grows with the doors and not with doors times pieces. The paved surface and the walked surface share one derivation of the layout and its ground, and the walked surface doesn't build the paved surface's pieces a second time. In a frame where the edit changed, the preview and the ghost share one acceptance check and one derivation of the edited surface. Applying the edit to the candidate keeps its own check, so the candidate still equals the world that commits the edit. (check: read each; criterion 1's report)
4. The allocation check watches previews of a changing edit. It alternates the tool's edit between two edits for each tool, so each watched frame makes the preview, its candidate, its shop context, and its ghost again, and fails if they free a block they allocated. While an overlay is shown and the preview holds a candidate, it watches the preview made again at each tick. It also stops pausing around applying a command in its undo and redo rounds, and around a path tool's press after its first stop. (ctest; check: inside one resolver, add a temporary std::vector, confirm that the check fails and names the preview, and revert)
5. The simulation and the previews are unchanged. tpj_scenarios --compare of windows-debug output from before and after each feature shows no difference, for its scenarios, every park in tests/parks, and full.park over 300 ticks. A candidate still equals, by worldsEqual, the world that commits the same edit. The runtime report marks no stage result-changed. (check: tpj_scenarios --compare; ctest; the report's --compare)
6. A decision record states the rules that simulation work must follow to run on more than one thread. Its results must be identical to one thread's whatever the number of threads and the order they finish in. Each worker writes only to what it alone owns, and only one thread changes the world, in a fixed order. No worker reads the clock. The record names the check the first milestone that runs simulation work on threads must add, a tpj_scenarios --compare between one worker and several. It leaves to that milestone whether a preview may trail the pointer (principle 8). (check: read the record)
7. While an overlay is shown, the edit is unchanged, and only ticks pass, the kept candidate follows each tick without resolving it whole. It takes the world's running state and re-runs only the resolvers and finishers that read running state. Each resolver and finisher declares, where the schema registers it, whether it reads running state. The followed candidate equals, by worldsEqual, makeCandidate of the same edit on the ticked world. (ctest: the world check that a candidate equals the world that commits the same commands, extended to a candidate followed over ticks, on the park files in tests/parks; criterion 1's report)
8. Every SPEC.md whose public functions now take kept buffers or a kept world says so. docs/testing.md, docs/conventions.md, and decision 0041 say the check watches previews and applying a command. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Medium

None. The milestone adds, samples, and publishes no fields or flows, and changes no answer the simulation or the display gives. Kept buffers and a kept candidate's storage are never part of a world's walk, hash, save, or comparison.

## Dependencies

- allocation-free-loops: the allocation check, its rounds of undos and redos, and the kept preview. Complete.
- Decision 0041, which leaves previews of a changing edit, the preview made again at each tick, and applying a command to this milestone.
- Decision 0025: a candidate is a whole copy of the world, resolved. The milestone keeps that, and reuses the copy's storage.
- measured-runtime: tpj_bench, the runtime report, and full.park. Complete.

## Core feature

lean-routes. Route distance and the path networks are about 46% of a preview, the largest share, and most of their time goes to work that repeats or grows with doors times pieces. The feature also adds the bench stages and takes the before measurements, so every later feature reports against them.

## Features

1. `lean-routes`: tpj_bench's path edit preview, acceptance, and ghost stages and the before measurements; route distance building each network's steps once per resolution; each door's connection found from nearby pieces and paths; both resolvers reusing their storage. For criteria 1, 2, and part of 3. Depends on: none.
2. `lean-surfaces`: the two surfaces sharing one derivation, guest ground, and the acceptance check and edited surface made once per changed frame and shared by the preview and the ghost, all reusing their storage. For part of criterion 3. Depends on: feature 1.
3. `quiet-placement`: furniture, best seats, the food, drink, facility, and seat offers, the departure tally, carrying guests onto the new ground, and the finishers reusing their storage, so criterion 4's watch of the preview can pass. Depends on: feature 1.
4. `kept-candidate`: the candidate copied into a world the preview keeps, reusing its storage, the ghost built into kept lists, and the allocation check watching previews, the preview at each tick, applying a command, and a path tool's press, for criterion 4; and the edits to docs/testing.md, docs/conventions.md, and decision 0041 in criterion 8. Depends on: features 1 to 3.
5. `thread-rules`: the decision record of criterion 6. Depends on: none.
6. `ticking-candidate`: each resolver and finisher declaring whether it reads running state, and the kept candidate following ticks by re-running only those, for criterion 7. Depends on: feature 4.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Ghost from what the edit changes: build the ghost's surface from the edited segments' pieces alone instead of reading the candidate's whole walked surface. Gated on: criterion 1's ghost stage showing the ghost as a real share of a changed frame.

## Open questions

None.

## Research notes

- A preview on full.park is about 13 ms on windows-release. Route distance takes 31%, the path networks 15%, the walked surface 9%, furniture 9.5%, best seats 8%, carrying guests 7%, copying the world 7%, and the paved surface 5%. Heap work is 19%.
- Reusing storage alone saves at most about a fifth. The resolvers that hold repeated work, route distance, the connections, and the walked surface, are about 55%, and removing the repeated part saves some share of that.
- Resolving only what the edit touches and resolving off the frame's thread stay deepening candidates of the capability, measured against the leaner resolution.

The details are in RESEARCH.md.
