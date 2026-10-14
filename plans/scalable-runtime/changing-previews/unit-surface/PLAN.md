# Implementation Plan: Unit Surface

## Goal

The paved surface derives in units per point and per segment, hides edge lines with only nearby pieces of the same group, and is stored per owner, so a changed frame derives paving only where its edit reaches, with every world hash unchanged.

## Approach

First the surface's storage changes to per-owner runs with a walk that emits today's words, and every reader moves to the runs. A scenarios comparison then proves the words are unchanged before any derivation changes. Next, surface.cpp's whole-layout passes become per-point and per-segment functions that buildSurface calls for hypothetical layouts and the units call for the world, so both compute alike. Last, the unit mechanism gains indexes filed from unit values, a kind that updates its own units, and the list of changed units. The surface's units and the paved-surface resolver use them, and the allocation check's drags and commits watch them.

## Placement

Decision 0027 places each behavior this feature adds:

- KeptMap, copyable and iterable: sim, src/sim/kept_map.h, in the sim root. It is storage both the unit space and the park's surface component use, so it must sit below both (cmake/layers.txt's sim/*).
- Indexes over unit values, kinds that update their own units, and changedUnits: sim, src/sim/units.h, with their registration in src/sim/schema.h and src/sim/schema.cpp. The unit mechanism owns every kind of unit table, and the schema owns what modules register.
- The per-owner surface, its walk, and pavingOf, edgePoints, and sidePoints: park, src/sim/park/surface.h and src/sim/park/surface.cpp. The park module owns the surface.
- The per-point and per-segment paving functions and per-owner hiding: park, src/sim/park/internal/paving.h and src/sim/park/paving.cpp, private to the module. They are split out of surface.cpp, which is 1,600 lines, because both buildSurface and the units call them.
- The surface's units, the group labels, the cell indexes, and the paved-surface resolver: park, src/sim/park/surface_units.cpp, registered by addSurfaceUnits, which addPavedSurface in surface.cpp calls. The park module owns the surface and its derivation, as ground_units.cpp holds the ground's.
- The ground's unit kinds, which the surface's units read: park, src/sim/park/internal/ground_units.h, moved out of ground_units.cpp's anonymous namespace so the module's other files can name them.
- The walked surface refilled in place: routes, src/sim/routes/networks.cpp, resolveWalkedSurface. It owns the walked surface.
- Reading the runs: each reader's own module, in the files that read the surface today. No reader gains a new concern.
- The steps the allocation check watches: tests/checks/allocations/allocation_check.cpp. The check owns what it watches.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, build tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: exit status 0 and no warning.

Step 2: Record the scenarios.

Run: `mkdir -p build/unit-surface && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/unit-surface/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/unit-surface/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c unit-surface`.

The runtime baseline is build/flat-ground.report. Main hasn't changed since it was written, so no before report is made.

### Task 2: Write the specs

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/sim/park/SPEC.md`

Step 1: Add FEATURE.md's two Units paragraphs to src/sim/SPEC.md after the Units section's second paragraph, and its changedUnits paragraph after them. Add its KeptMap paragraph to Memory in play, after the first paragraph.

Step 2: In src/sim/park/SPEC.md, add the addPavedSurface sentence to the Boundary after the sentence on addPathGround. In Paved surface, replace the first sentence, the sidePoints sentence, and add the walk sentence after the one on resolvedSurface before a world first resolves, all as FEATURE.md gives them.

### Task 3: Make KeptMap copyable and iterable

Files:
- Create: `src/sim/kept_map.h`
- Modify: `src/sim/unit_space.h`
- Modify: `src/sim/unit_space.cpp`

Step 1: Move KeptMap from unit_space.h into kept_map.h unchanged, and include kept_map.h from unit_space.h.

Step 2: Add the members FEATURE.md's interface gives:
- The copy constructor copies Items only, as clone does. Drop clone, and make its four callers use the copy: KindTable::clone in unit_space.h, IndexTable::clone's two in unit_space.cpp (lines 16 and 17), and UnitSpace::clone's (line 52).
- Copy assignment calls assignFrom.
- The move constructor and move assignment are defaulted, so moving a surface moves its nodes instead of copying them.
- size, empty, begin, and end read Items. begin and end return Items' const iterators.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 4: Store the surface per owner

Files:
- Modify: `src/sim/park/surface.h`
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/sim/park/internal/layout.h`
- Modify: `src/sim/park/SPEC.md`
- Modify: `src/sim/routes/networks.cpp`

Step 1: In surface.h, add OwnerPaving and replace PavedSurface with the per-owner form FEATURE.md gives, including kept_map.h. Change EdgeLine's and SegmentSide's comments to say First counts from the start of the owner's positions. Replace edgePoints and sidePoints with the OwnerPaving forms, and add pavingOf.

Step 2: Write PavedSurface's visitFields. It emits, in order:
- "pieces": the total piece count as one field, then each owner's pieces in owner order, each as a field.
- "edges": the total count, then each owner's edge lines, each emitted with First plus the count of edge positions in the owners before it.
- "edge-points": the total count, then every owner's positions in order.
- "sides" and "side-points" the same way.

emitField writes a list as its size and then its elements, so this gives the words the flat lists gave. When the visitor is a FieldReader, throw std::logic_error("a paved surface is derived and is never read"). Derived components never load (save.cpp), so no load reaches it.

Step 3: Make the surface's writers refill a surface they are given, keeping its memory. Add two private helpers:
- One starts a refill: it clears every list of every owner the surface holds, keeping their memory.
- One ends it: it erases each owner whose run stayed empty, with KeptMap's erase, which keeps the node and its lists for a later add.

Between them, Output, the Visible writer, addWalkway, and buildSurface write each piece, edge line, and side into its owner's OwnerPaving, taken with getOrAdd. Offsets count within the owner. Drop the stable sorts, since each owner's run is written in the order the sorts kept.

Step 4: kept-checks made buildSurface, surfaceOf, surfaceAround, and hideJoined refill a surface they are given, with a PavingLists from the caller's TickBuffers. walkedSurfaceOf takes the surface to refill too. PavingLists gains the raw surface they build before hiding. hideJoined reads every owner's pieces in the raw surface as its hiders, and refills the given surface with the hidden edge lines, the pieces, and the sides. The raw surface is refilled the same way, so once both have grown, building a surface allocates nothing.

Step 5: resolveWalkedSurface in src/sim/routes/networks.cpp takes its component with getOrEmplace and passes its Held surface, with a PavingLists from its buffers, to a new parkSurface form that refills it: `void parkSurface(const World &world, std::span<const Walkway> walkways, PavedSurface &into, TickBuffers &buffers)`. Declare it in surface.h with the comment "As the walked form, refilling into and keeping the lists it works with in the buffers. Allocates nothing once into and the lists have grown." Add FEATURE.md's sentence on it to src/sim/park/SPEC.md's Boundary, after the sentence on parkSurface taking TickBuffers. The surfaces on the acceptance check's, a command's, and the ghost's paths are the ones kept-checks keeps, and Step 3's refill keeps their owners' memory. Only the returning parkSurface forms, for callers outside frames, still build a new surface.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the park module builds. Readers in other files fail to build until Tasks 5 to 7.

### Task 5: Read the runs in the park and its resolvers

Files:
- Modify: `src/sim/park/edits.cpp`
- Modify: `src/sim/routes/networks.cpp`
- Modify: `src/sim/guests/ground.cpp`
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: In edits.cpp:
- changedOwners (line 1376) compares each owner's pieces and edge lines with its run in before, and refills the kept surface it is given with the runs that differ, with their positions, as Task 4's refill does.
- The checks at lines 1521, 1955, and 2275 test every piece of every owner, in owner order.
- piecesNear (line 1738) gathers every owner's pieces, in owner order, into the kept list it fills.
- Each surface these build is one kept-checks keeps in the caller's buffers.

Step 2: In networks.cpp, deriveNetwork takes the PavedSurface instead of a list of pieces. ownerDistances and rayEntry, through findConnections and connectorRoute, read every owner's pieces in owner order. resolvePathNetworks passes resolvedSurface(world).

Step 3: In ground.cpp, the walk over the walked surface's pieces and edge lines (lines 279 to 311) reads each owner's run in owner order, taking edgePoints from that owner's paving.

Step 4: In furniture.cpp, sideOf (line 860) takes the segment's sides from pavingOf(surface, segment). Where a side's points are read (lines 475 and 942), read them from that owner's paving, which the code that found the side passes along with it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 6: Read the runs in render

Files:
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`
- Modify: `src/render/paving_overlay.cpp`

Step 1: In park_mesh.cpp:
- appendSurface (line 254) appends each owner's pieces and edge lines in owner order.
- The loop at line 592 reads every owner's pieces.
- ownedBy (line 613) copies the runs of the owners asked for, with getOrAdd into own, which it empties first by erasing every owner it holds, so its nodes are kept.
- ownersOf (line 637) lists the owners with a run.
- appendCandidate's reads at lines 718 to 724 test whether the edited surface holds any owner.

GhostLists.Owned in park_mesh.h stays a PavedSurface.

Step 2: In paving_overlay.cpp, the loop at line 274 reads every owner's pieces.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: exit status 0 and no warning.

### Task 7: Read the runs in tools and the crossing test

Files:
- Modify: `src/tools/frontage.cpp`
- Modify: `src/tools/internal/frontage.h`
- Modify: `src/tools/tools.cpp`
- Modify: `tests/integration/crossing_split_test.cpp`

Step 1: piecesNear (frontage.cpp:195), edgeSteps (line 388), and the paving test at line 425 read every owner's run in owner order. So does the paving test at tools.cpp:271.

Step 2: onGuestPaving in crossing_split_test.cpp (line 64) tests every owner's pieces. It asserts what it asserted. Only the way it reaches the pieces changes.

Step 3: Check that the walk and every reader see what they saw.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/unit-surface/scenarios-runs.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/unit-surface/full-runs.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/scenarios-before.txt build/unit-surface/scenarios-runs.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/full-before.txt build/unit-surface/full-runs.txt`
Expected: no warning, and both comparisons exit with status 0. Every hash is the same, so both surfaces walk the words they walked.

### Task 8: Split the paving into per-point and per-segment functions

Files:
- Create: `src/sim/park/internal/paving.h`
- Create: `src/sim/park/paving.cpp`
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: Move the derivation out of surface.cpp, unchanged except for where it lives. Each item goes to exactly one of three places:
- paving.h, in namespace tpj, as inline definitions: the constants, the ParkPoint operators and the vector functions (dot, ccw, lengthOf, unit, leftOf, rightOf, samePoint, sagittaOf), since Shape's members and edgeRadiusFor use them.
- paving.h, in namespace tpj, as type definitions: Side, Shape, End, Pair, TaperFit, TurnSpan, Scratch, Bounds, Joins, EdgeLists, and the PavingLists of Task 4. Declare there too, with external linkage, the functions surface.cpp and surface_units.cpp call: askAt, fitAt, paveAt, paveSegment, hideOwner (Step 3), the fill forms of buildSurface and walkedSurfaceOf, hideJoined, fitEdges, taperCandidates, and fitTapers.
- paving.cpp, in its anonymous namespace: every other helper, among them Output, Arm, Visible, insideOf, writeStep, appendTurn, appendCurve, halfway, filletOf, outlinePair, appendHalves, sideAt, triangulate, simplify, earOf, turnAt, pairOf, askedOf, fitPoint, trimPoint, buildPoint, buildFreeEnd, addEdges, findPoint, heldTaper, touches, focusOf, shapesOf, endsOf, forEachPoint, farEndsOf, askPoints, PointReaches, reachOf, pointsNear, and addWalkway.

paving.cpp also defines, outside its anonymous namespace, the functions Step 1 declares in paving.h, and surfaceOf, surfaceAround, pointsAffected, and edgesOf, which internal/layout.h still declares, and walkwayPiece, pieceCorners, pieceDistance, edgePoints, and sidePoints, which surface.h declares.

surface.cpp keeps pavingOf, the parkSurface forms, resolvedSurface, addPavedSurface, parkEdges, parkTapers, edgeRadiusFor, and the walk of Task 4, and includes paving.h.

Step 2: Declare in paving.h, in namespace tpj, the inputs and outputs of one point's and one segment's paving:
- `PavingEnd`: a segment end at a point. It holds the segment's ground line as a ParkSegment, the positions of its From and To points, which end is at the point, and, at the segment's far end, the asks and the trims, each a pair of doubles, left and right as seen from that end.
- `PointInput`: the point's key, position, and taper; its corner, if it has one; and its ends, ascending by segment key and then by end, with only segments whose ground line isn't empty.
- `PointAsks`: for each end at the point, its segment, which end, and what it asks for on each side. Zeros when the point has one end.
- `PointTrims`: the same, with the trims.
- `PointPaving`: the pieces the point owns, the corner pieces it gives its segments, in the order buildPoint writes them, its edge lines before hiding with their positions, and, for each end and side that has one, its turn half: its positions and the side it meets.
- `SegmentInput`: the segment's ground line, its points' positions, the trims at both ends, the turn halves at both ends, and the corner pieces both points give it, the lower-keyed point's first.
- `SegmentPaving`: the segment's From point, its pieces, the index of its body among them or none, its sides, and their positions.

Give each output a visitFields that lists every field, so units compare them by words.

Step 3: Declare and define in paving.h and paving.cpp, each marked [[gnu::noinline]] so a release profile shows it by name:
- `void askAt(const PointInput &, PavingScratch &, PointAsks &)`: makes the ends' shapes as shapesOf makes them, with each far end's asks, and runs trimPoint with asking true when there is more than one end.
- `void fitAt(const PointInput &, PavingScratch &, PointTrims &)`: the same with asking false.
- `void paveAt(const PointInput &, PavingScratch &, PointPaving &)`: runs trimPoint with asking false, sets each shape's trims at its far end from the input, then runs buildPoint, or buildFreeEnd for one end, into the output.
- `void paveSegment(const SegmentInput &, PavingScratch &, SegmentPaving &)`: writes the corner pieces, then the body and both sides, as buildSurface's last loop does.
- `void hideOwner(const OwnerPaving &raw, std::span<const SurfacePiece> hiders, PavingScratch &, OwnerPaving &out)`: hides the raw edge lines as hideJoined does for one owner. For each edge line, it keeps the hiders whose bounds overlap the line's, then steps the line. The caller gives only the hiders of the owner's group.

PavingScratch holds Scratch and the lists these reuse, cleared before each use.

Step 4: Rebuild buildSurface, surfaceOf, surfaceAround, and hideJoined on these functions, keeping their signatures and their subsets of points and owners:
- askAt for every point that is asked.
- fitAt for every point that is built.
- paveAt for every built point, with the far trims from the fits. Where a far end wasn't fitted, pass zeros. Only owners outside the wanted set read them.
- paveSegment for every wanted segment.
- hideOwner for each point's run, with the hiders of its group that Joins gives.

Move fitEdges, fitTapers, and the whole-layout helpers they share with buildSurface (shapesOf, endsOf, forEachPoint, askPoints, farEndsOf, addEdges) into paving.cpp unchanged, and declare in paving.h the entry points surface.cpp calls: edgesOf's and parkTapers' fitting, buildSurface, and hideJoined.

Step 5: Add paving.cpp to tpj_sim's sources.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/unit-surface/scenarios-split.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/unit-surface/full-split.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/scenarios-before.txt build/unit-surface/scenarios-split.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/full-before.txt build/unit-surface/full-split.txt && build/windows-debug/tpj_sim_tests.exe "[#park_world_checks_test]"`
Expected: no warning, both comparisons exit with status 0, and the world checks pass.

### Task 9: Add indexes over unit values, kinds that update themselves, and changedUnits

Files:
- Modify: `src/sim/units.h`
- Modify: `src/sim/unit_space.h`
- Modify: `src/sim/schema.h`
- Modify: `src/sim/schema.cpp`

Step 1: Add the UnitValueIndex concept and its addUnitIndex overload. Its update is updateIndex's, except that its dirty keys, after the first update, are the keys of Of's units on the change list, and at the first update every key Of holds a unit for. It reads each unit's value through Of's table and files it with keysOf. It is registered over Of's Over component, so the schema's Over and Reads checks hold.

Step 2: Add UnitUpdate and the SelfUpdatingKind concept, and an addUnits overload that registers a KindTable<Value> whose update makes a UnitUpdate and calls K::update. set compares the held words with sameWords, using lists kept in the buffers, and pushes the unit on the change list when it adds the unit or the words differ. remove pushes it when it held one. changed scans the change list for the table's entries and appends their keys, sorted and unique. unit and filed read as UnitReads does, without recording reads. Afterwards, set the table's Counter as updateUnits does.

Step 3: Add changedUnits, which appends the keys of the kind's table on the change list, sorted and unique.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 10: Label the groups

Files:
- Create: `src/sim/park/internal/surface_units.h`
- Create: `src/sim/park/surface_units.cpp`
- Modify: `src/sim/park/internal/ground_units.h`
- Modify: `src/sim/park/ground_units.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: Move SegmentEnds, StraightLine, StraightLines, ArmInputs, PointArmsUnits, Corner, Corners, GroundLine, and GroundLines, with their visitFields, out of ground_units.cpp's anonymous namespace into internal/ground_units.h, in namespace tpj, unchanged. ground_units.cpp keeps addPathGround and worldGround.

Step 2: In surface_units.cpp, add the self-updating kind GroupLabels, named group-labels, over PathPointIntent, whose Value holds one EntityKey, the label. Its update:
- At its first update, takes every point. After that, it takes the points whose segment-ends entry changed and the From and To points of every ground-line unit that changed.
- For each point it took that no earlier fill this update reached, it flood fills the point's group. Two points are linked when a segment filed under one ends at the other and its ground line isn't empty. The label of every point in the group is its lowest key. A point with no link has no label, so remove its unit.
- Keeps the fill's lists in its scratch.

Step 3: Declare in surface_units.h `void addSurfaceUnits(WorldSchema &schema)`, which registers group-labels and, from Tasks 11 and 12, the rest and the paved-surface resolver. Declare there too `EntityKey pavedSurfaceKey()`, the derived key of the paved surface's entity, which resolvedSurface in surface.cpp reads in place of SURFACE_PURPOSE. Add surface_units.cpp to tpj_sim's sources.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 11: Derive the paving in units

Files:
- Modify: `src/sim/park/surface_units.cpp`

Step 1: Add a helper that fills a PointInput for a point through UnitReads: the point's intent; its segments from filed<SegmentEnds>; each segment's ground line from unit<GroundLines>; each far point's position through component<PathPointIntent>; and its corner from unit<Corners>. Add a flag for which far data it reads: the far asks from unit<PointAsksUnits>, or also the far trims from unit<PointTrimsUnits>.

Step 2: Add the unit kinds, each over PathPointIntent unless named otherwise, with Reads naming PathPointIntent:
- point-asks: askAt.
- point-fits: fitAt, with the far asks.
- point-paving: paveAt, with the far asks and trims.
- segment-paving, over PathSegmentIntent: paveSegment, with its ground line, its points' positions and trims, and its points' paving's turn halves and corner pieces for it.

Step 3: Add the two cell indexes. A cell is SURFACE_CELL, 8 m, square. Its key packs floor(x / SURFACE_CELL) and floor(z / SURFACE_CELL), each offset by 2^31, into the high and low 32 bits, so no cell's key is NULL_KEY. A box covers every cell from the floor of its least corner to the floor of its greatest.
- point-cells, over point-paving: the cells each of the point's pieces and corner pieces covers.
- segment-cells, over segment-paving: the cells its body covers.

Step 4: Add hidden-edges, over PathPointIntent. It reads the point's paving and label. For each raw edge line, it looks up every cell the line's bounds cover in both indexes. It keeps:
- the pieces and corner pieces of each point filed there whose label equals the point's;
- the body of each segment filed there whose From point's label equals the point's.

It drops repeats, then calls hideOwner with them. A point with no label has no paving, so it gives an empty run.

Step 5: Register them in addSurfaceUnits in this order: group-labels, point-asks, point-fits, point-paving, segment-paving, point-cells, segment-cells, hidden-edges.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 12: Keep the paved surface from the units

Files:
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/sim/park/surface_units.cpp`

Step 1: Move resolvePavedSurface from surface.cpp into surface_units.cpp, and register it at the end of addSurfaceUnits under the name paved-surface. addPavedSurface registers the component and calls addSurfaceUnits. resolvePavedSurface takes the surface with getOrEmplace on its derived entity. It gathers the keys changedUnits gives for point-paving, segment-paving, and hidden-edges, in a list kept in the buffers, sorted and unique. For each key:
- A point's run is its paving's pieces and its hidden edge lines with their positions.
- A segment's run is its paving's pieces and sides with their positions.
- When the run is empty, erase the owner. Otherwise take it with getOrAdd and copy into it with copyGrowing.

A loaded world and a new world hold no units, so their first resolution puts every unit on the change list and fills every owner. resolvePavedSurface no longer calls parkSurface, worldGround, layoutOf, or parkSegments.

Step 2: Check the simulation is unchanged and the world checks pass.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/unit-surface/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/unit-surface/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/scenarios-before.txt build/unit-surface/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/full-before.txt build/unit-surface/full-after.txt && ctest.exe --preset windows-debug`
Expected: no warning, both comparisons exit with status 0, and every test passes, the save checks and the candidate check's drags among them.

### Task 13: Watch the surface's steps in the allocation check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Add the steps this feature moves to the names WatchedSteps holds: group-labels, point-asks, point-fits, point-paving, segment-paving, point-cells, segment-cells, hidden-edges, paved-surface, and walked-surface. Update the comment that lists them.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, and the message names the watched drags and commits. If watched work frees a block, fix the code that allocates it, then run again.

### Task 14: Confirm the feature

Files: none.

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warning, every test passes, and tidy prints no finding.

Step 2: Write the after report and compare it with the baseline.

Run: `cmake.exe --build --preset windows-release && scripts/runtime-report.sh build/unit-surface.report && scripts/runtime-report.sh --compare build/flat-ground.report build/unit-surface.report`
Expected: no stage is marked result-changed. On full.park, shop-drag and path-drag are marked faster. shop-drag's median is at least 0.3 ms below 9.48 ms, and path-drag's at least 0.5 ms below 13.33 ms. No other stage is marked slower.

Step 3: Profile the drags. Run this eight times, with NAME from unit-surface-1 to unit-surface-8:

    W=$(wslpath -w .)
    "/mnt/c/Program Files/Very Sleepy/sleepy.exe" "/r:$W\build\windows-release\tpj_bench.exe --ticks 1 $W\tests\parks\stress\full.park" "/o:$W\build\perf\NAME.sleepy"

Over the eight profiles, for each drag, sum the samples whose stacks hold the drag's timing function and resolveWorld. Among them, sum those that hold askAt, fitAt, paveAt, paveSegment, or hideOwner and don't hold walkedSurfaceOf. Separately, sum those that hold buildSurface or hideJoined and don't hold walkedSurfaceOf.
Expected:
- Each first sum is above 0.
- The buildSurface and hideJoined sum is 0 for both drags.
- Under timeShopDrag, the paving units' sum is 0.
- Under timePathDrag, the paving units' sum is under 1.5% of the first sum. In the derived-units profiles, the paved surface was about 7.7% of these samples.

Step 4: Read each reader Tasks 5 to 7 changed, and resolvePavedSurface and every function it and the surface's units call. Confirm criterion 5, and that nothing they run allocates once its lists have grown.

### Task 15: Commit

Stage only this feature's paths. Commit with the commit-hygiene skill, subject `Sim: Derive the paved surface in units per point and segment`. Then check `git diff --stat` and amend if the hook formatted files.
