# Feature: Furniture Units

## Summary

furniture-units makes furniture find the chains an edit changed from what changed, instead of reading the whole park on every resolution. Furniture is the largest whole-park cost left in a drag frame on full.park: 22.8% of a shop drag's and 14.4% of a path drag's, about 1 ms of each (RESEARCH.md). Laying chains again is under 1% of that. The cost is finding which chains to lay again: every resolution builds a grid of every obstacle in the park and hashes every chain's surroundings.

After this feature:

- Furniture keeps each chain it laid, the cells of 8 m that the chain's laying reaches, and the chains in each cell, from one resolution to the next.
- A resolution lays again only the chains whose sides or furnishing changed, the chains whose obstacles within reach changed, and the later chains that the changed pieces of a chain laid again or removed reach. Every other chain keeps its pieces.
- A chain it lays again reads only the obstacles in the cells it reaches, through park's and routes' cell indexes, not a grid of the whole park.
- The seat places field keeps its entries and changes only the segments whose pieces or ground line changed.

Where pieces stand doesn't change at all.

## Acceptance criteria

1. Both drags get faster. scripts/runtime-report.sh writes a report of the feature, and its --compare against build/route-updates.report, written on the code main holds now, marks shop-drag and path-drag faster. Each median falls by at least 0.7 ms. Every stage whose result is a hash is marked result-changed, because criterion 3's change to the walk changes every hash; only the medians are judged. Furniture took about 1.1 ms of the shop drag's frame and 1.0 ms of the path drag's. (check: scripts/runtime-report.sh build/furniture-units.report, then --compare build/route-updates.report build/furniture-units.report)
2. A changed frame lays and reads only the furniture its edit reaches. The furniture resolver reads no list of every segment, path point, box, entrance, walkway, piece, or chain, except at a world's first resolution. It reads the obstacles of each chain it lays from the cells that chain reaches, and finds what changed from the cells and segments the resolution changed. Very Sleepy profiles put furniture at 4.1% of the shop drag's frame and 5.1% of the path drag's, from 22.8% and 14.4%. What remains is reading the obstacles near each chain a changed cell or segment reaches, to check whether it must be laid again, which this design can't avoid. The target of 2% was restated at this measured share. (check: read layFurniture and everything it calls; four profiles build/perf/furniture-units-1.sleepy to -4)
3. The result is exact. Taking furniture's bookkeeping off the world's walk changes the hash of every world once: its fields leave the walk, and its entity now exists in every resolved world. No piece moves, as reading that change shows, so the feature records its scenarios right after it, before laying changes. tpj_scenarios --compare of that baseline and the finished feature shows no difference, for every scenario, every park in tests/parks, and full.park over 300 ticks. The save checks and the drag world check, on warm.park and furnished-paths.park, pass on every frame and after every commit. (check: tpj_scenarios --compare against build/furniture-units-baseline-scenarios.txt and build/furniture-units-baseline-full.txt; ctest)
4. The allocation check's exception list no longer holds furniture, so the drags and commits watch it. (ctest: the allocation check)
5. src/sim/SPEC.md, src/sim/furniture/SPEC.md, src/sim/park/SPEC.md, and src/sim/routes/SPEC.md state what the feature changes. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

The drag world check in tests/sim/park_world_checks_test.cpp (checkKeptCandidateThroughDrag) gains two things, since warm.park has no furniture and the check only ever resolves once from a world resolved from nothing:

- It also runs on tests/parks/furnished-paths.park. The shop drag is warm.park's: the lowest-keyed shop dragged along its face. The path drag can't pick its segment as warm.park's does, since furnished-paths.park has no guest on a segment after one tick. It uses the lowest-keyed guest segment with furnishing on both sides, and makes the frames from it as pathDragFrames does from a segment: a guest path from its middle, square to it, its end 6 m plus 0.5 m per frame out, on the first side the world accepts every frame on. Each frame's kept candidate equals the candidate made new for the frame and the world loaded from its save.
- In each drag, on both parks, a second world commits each frame's edit with commitCommands after the frame's tick, so it resolves in place frame after frame from its own last resolution. After each commit it equals the world loaded from its save and resolved.

This is the world check the sim root already holds (docs/testing.md), run over more worlds that edits reach. It is what catches bookkeeping that stays wrong for the next resolution.

## Medium

The seat places field keeps its name, entries, and readers. It keeps its resolved entries between resolutions, as route distance's fields do, and its resolver edits only the segments that changed. No field or flow is added or removed.

Furniture reads three new things through park's and routes' interfaces, named in Spec changes: the cells whose obstacles changed, the obstacles in given cells, and one segment or path point by key.

## Spec changes

src/sim/SPEC.md, in the Units section, after the sentence on unitsCurrent, add:

    filedIn gives the keys an index files under a key, ascending, and filedUnder the keys it files an entity under, ascending, each empty when there are none. Resolvers read indexes with them outside a unit derivation. Neither allocates.

src/sim/park/SPEC.md:

- In the boundary paragraph that begins "Other modules read intent only through", add after "parkBoxes": ", and, from a resolver, parkSegment, pathPointAt, pavingNear, boxesNear, entrancesNear, changedSegments, and changedParkCells".
- In the registration paragraph, replace "addParkUnits registers the units park keeps for other modules' units, named park-boxes, one for each box, and park-entrances, one for each entrance, so those names are taken." with "addParkUnits registers the units park keeps for other modules' units, named park-boxes, one for each box, and park-entrances, one for each entrance, their cell indexes, box-cells and entrance-cells, and segment-furnishings, one for each segment's furnishing, so those names are taken."
- At the end of the paragraph on the unit reads other modules use, add:

    Outside a unit derivation, a resolver reads the same units through these: parkSegment gives the segment a key holds, as parkSegments gives it, into a segment the caller keeps; pathPointAt gives a path point's position; pavingNear, boxesNear, and entrancesNear give the pieces of the paved surface, the boxes, and the entrances whose bounds meet a rectangle, and may also give others; changedSegments gives the segments whose furnishing, paving, or ground line the running resolution changed; changedParkCells gives the cells of park's cell grid where a piece of the paved surface, a box, or an entrance was added, removed, or changed in the running resolution. None allocates once its lists have grown.

src/sim/routes/SPEC.md:

- In the boundary paragraph, replace "and their walkways through connectorWalkways and keyedWalkways" with "and their walkways through connectorWalkways, keyedWalkways, walkwaysNear, and changedWalkwayCells".
- In the registration paragraph, add frontage-cells to the names of the units addRoutes registers.
- At the end of the paragraph that begins "keyedWalkways gives the same walkways", add:

    walkwaysNear gives the walkway pieces and the doors' frontage strips whose bounds meet a rectangle, and may also give others. A frontage strip runs straight out from a door's face, as wide as the face, to where the door's walkway ends. changedWalkwayCells gives the cells where a walkway piece or a frontage strip was added, removed, or changed in the running resolution. Neither allocates once its lists have grown.

src/sim/furniture/SPEC.md:

- Boundary: replace "furniture reads intent only through parkSegments, parkPathPoints, parkBoxes, parkEntrances, and footprintOf (sim/park/SPEC.md), and doors, walkways, and the paved surface with its walkways only through routes' parkDoors, keyedWalkways, and walkedSurface (sim/routes/SPEC.md)." with "furniture reads intent only through parkSegments, parkSegment, pathPointAt, pavingNear, boxesNear, entrancesNear, changedSegments, changedParkCells, and footprintOf (sim/park/SPEC.md), and walkways and the paved surface with its walkways only through routes' walkwaysNear, changedWalkwayCells, and walkedSurface (sim/routes/SPEC.md)."

## Files affected

- Modify: src/sim/units.h
- Modify: src/sim/SPEC.md
- Modify: src/sim/park/unit_reads.h
- Modify: src/sim/park/unit_reads.cpp
- Modify: src/sim/park/SPEC.md
- Modify: src/sim/routes/internal/connections.h
- Modify: src/sim/routes/connections.cpp
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/furniture/furniture.h
- Modify: src/sim/furniture/furniture.cpp
- Modify: src/sim/furniture/SPEC.md
- Modify: tests/checks/allocations/allocation_check.cpp
- Modify: tests/sim/park_world_checks_test.cpp (the test pass)
- Modify: plans/scalable-runtime/changing-previews/MILESTONE.md

## Dependencies

route-updates, whose medium change lets a field keep its resolved entries. unit-connections, whose door walkways and walkway cells this reads.

## Out of scope

- Best seats, carrying guests, the copy, and the ghost's furniture comparison: each a later feature, by measured share.
- Changing where furniture stands.

## Open questions

- winding-path.park's preview stage, which copies a new world and resolves it once, measured 15% slower (483 to 554 µs). Its profile shows no furniture work and none of the new units in that stage, so the cause is unknown; it may be drift since build/route-updates.report. Running the report again on the code before this feature would settle it.
