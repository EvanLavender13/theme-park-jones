# Feature: Candidate Ghost

## Summary

candidate-ghost makes the ghost of an accepted path edit read the surface the edit changes from the candidate, instead of deriving that paving again. The ghost's edited surface is now the largest single cost in a drag frame on full.park: 22% of the path drag's frame, about 1.3 ms of 5.97 ms (RESEARCH.md). To find which segments and points the edit changes, the ghost compares every segment before and after the edit, widens what changed by testing every segment's reach, and derives the paving around it twice, before and after. It then draws only those owners, from the candidate's surface.

The candidate has already resolved the edit. Its resolution derived every paving unit and ground line the edit can change, and it lists the units it derived until it resolves again. After this feature, when the ghost has a candidate, it reads that list. It compares each listed point's and segment's paving and ground line in the candidate with the world's. The ghost shows exactly what it shows today. A refused edit has no candidate, so its ghost still derives the paving around it.

## Acceptance criteria

1. The path drag gets faster. scripts/runtime-report.sh writes a report of the feature, and its --compare against build/furniture-units.report, written on the code main holds now, marks path-drag faster, its median at least 1 ms below 5.97 ms. (check: scripts/runtime-report.sh build/candidate-ghost.report, then --compare build/furniture-units.report build/candidate-ghost.report)
2. The ghost of an accepted edit reads only what the edit changed. With a candidate, the ghost's edited surface reads no list of every segment, point, or owner of the park. It reads the units the candidate's resolution derived, and the world's and the candidate's paving and ground lines for those units. Very Sleepy profiles put the ghost's edited surface under 1% of the path drag's frame, down from 22.3%. (check: read the candidate form of editedParts and everything it calls; four profiles build/perf/candidate-ghost-1.sleepy to -4)
3. The ghost is unchanged. The criterion 1 comparison marks no stage result-changed, including the ghost and path-ghost vertex counts on each stress park. In play on full.park in the windows-release app, a guest path drawn from an existing path shows the same ghost as before: its paving, its joins, and its walkways. A path drawn into a box still shows its refused ghost in the invalid tint. (check: the criterion 1 comparison; play)
4. The simulation is unchanged. tpj_scenarios --compare shows no difference for every scenario, every park in tests/parks, and full.park over 300 ticks, against outputs recorded before the change. (check: tpj_scenarios --compare)
5. The ghost allocates nothing once its lists have grown. The allocation check watches both drags, ghost included. (ctest: the allocation check)
6. src/sim/park/SPEC.md and src/render/SPEC.md state what the feature changes. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

None. The feature changes no simulation state and registers no unit kind.

## Medium

None. The ghost reads the candidate's paved surface and ground-line units, which the paved-surface resolver and the ground units already derive. No field or flow is added, removed, or read in a new way.

## Spec changes

src/sim/park/SPEC.md, in the paragraph that begins "planEdit plans an edit on a world", after "Neither allocates once the lists have grown.", add:

    editedParts can also take the edit's candidate: a copy of the world, which is resolved, with the edit applied and then resolved once, as makeCandidate and the preview make it. The candidate must not have resolved again since. Then editedParts gives the same parts and derives no paving, and its work grows with what the edit changes, not with the park.

src/render/SPEC.md, in the Ghosts and highlights section:

- Replace "If a caller already holds the candidate, the caller passes it instead." with "If a caller already holds the candidate makeCandidate gave for the edit on this world, and the candidate hasn't resolved again since, the caller passes it instead."
- After the sentence "buildGhostMesh(world, edit, planned, candidate, lists) builds the same ghost from the edit's plan, as planEdit gives it on the world, in GhostLists the caller keeps, clearing them first.", add:

    With the candidate, finding the surface the edit changes derives no paving, and that work grows with what the edit changes, not with the park.

## Files affected

- Modify: src/sim/park/edits.h
- Modify: src/sim/park/edits.cpp
- Modify: src/sim/park/SPEC.md
- Modify: src/render/park_mesh.cpp
- Modify: src/render/SPEC.md
- Modify: plans/scalable-runtime/changing-previews/MILESTONE.md

## Dependencies

- furniture-units, merged as defac57. build/furniture-units.report is the before report.
- The derived-units list and derivedUnits (derived-units), and the paving and ground-line units (unit-surface and derived-units).

## Out of scope

- Planning a path edit and its acceptance check. They still copy the layout, derive the whole park's ground, compare every segment before and after, and derive the paving around the edit, before and after, without hiding its edge lines: 5.8% of the path drag's frame. A later feature takes them.
- The ghost of a refused edit. With no candidate, it still derives the paving around the edit.
- The ghost's other parts: comparing every furniture piece, drawing every walkway, and checking every box, 2.2% of the path drag's frame and 3.7% of the shop drag's. They stay on the milestone's list of whole-park work.

## Open questions

None.
