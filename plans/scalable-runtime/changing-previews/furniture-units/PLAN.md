# Implementation Plan: Furniture Units

## Goal

Furniture lays again only the chains an edit reaches, reading only the obstacles near them, with the same pieces as laying every chain from nothing.

## Approach

Furniture keeps, in its laid-furniture component and outside the world's walk, each chain it laid with its order, members, pieces, and the 8 m cells its laying reaches, and the chains in each cell. A resolution marks dirty the chains whose members' segments changed and the chains in cells where an obstacle changed, then lays them in chain order. Each time a chain's pieces change, it marks dirty the later chains in the cells of its old and new pieces. A chain it lays reads its obstacles from park's and routes' cell indexes, and the pieces of earlier chains from its own cells. The seat places field keeps its entries and is edited only for the segments whose pieces or ground line changed.

## Placement

Decision 0027 places each behavior this feature adds:

- Looking up an index's entries outside a derivation: sim, src/sim/units.h, with the other unit-table reads (unitValue, changedUnits).
- Box and entrance cell indexes, the near and changed reads of park's obstacles, and the single segment and point reads: park, src/sim/park/unit_reads.h and unit_reads.cpp. Park owns boxes, entrances, segments, points, and the paved surface, and already gives other modules their units there.
- The frontage strip, its cell index, and the near and changed reads of walkways and strips: routes, src/sim/routes/connections.cpp and networks.h. Routes owns doors and their walkways.
- The segment-furnishings unit kind and changedSegments: park, src/sim/park/unit_reads.h and unit_reads.cpp. Park owns segments and their furnishing intent, and gives other modules what changed there.
- Keeping chains, finding the dirty ones, laying them, and editing the seat places: furniture, src/sim/furniture/furniture.cpp, which owns laying.

## Tasks

### Task 1: Write the specs and the milestone entry

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/sim/park/SPEC.md`
- Modify: `src/sim/routes/SPEC.md`
- Modify: `src/sim/furniture/SPEC.md`
- Modify: `plans/scalable-runtime/changing-previews/MILESTONE.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

Step 2: In MILESTONE.md criterion 6, after "The exception is a feature that carries out one of those changes. It reports its differences, and every later feature compares against its output.", add: "A feature may also take resolution bookkeeping off the world's walk, which changes every hash once and nothing else. It does that in a step of its own, shows by reading that step's diff that nothing the walk still covers changes, and records its outputs right after it to compare against." In criterion 5's check, replace "the candidate check's drags on warm.park, which derived-units adds" with "the drag world check on warm.park, which derived-units adds, and on furnished-paths.park, with a world committing each frame, which furniture-units adds".

Step 3: In MILESTONE.md's Features list, insert before thread-rules, renumbering it 9: "8. `furniture-units`: furniture lays again only the chains an edit reaches, and reads only the obstacles near them, instead of hashing every chain's surroundings on every resolution. Depends on: feature 7." In the whole-park work left, replace the furniture item with "Furniture: laid only where an edit reaches it (furniture-units)." and take "the ghost's other parts" item as it is.

### Task 2: Run the test pass

Files: none.

Step 1: Dispatch the test-writer agent with FEATURE.md, as implementing-features says, for the drag world check FEATURE.md's Tests section lists. It must build. Its new parts pass on main's furniture, since they check exactness, not the change.

### Task 3: Read an index's entries outside a derivation

Files:
- Modify: `src/sim/units.h`

Step 1: Add `template <typename I> std::span<const EntityKey> filedIn(const World &world, EntityKey key)`: the keys index I files under the key, ascending, from its IndexTable's Entries, or an empty span. Allocates nothing.

Step 2: Add `template <typename I> std::span<const EntityKey> filedUnder(const World &world, EntityKey key)`: the keys index I files the entity under, from its IndexTable's Filed, or an empty span. Allocates nothing.

### Task 4: Give park's obstacles near a place and the cells they changed

Files:
- Modify: `src/sim/park/unit_reads.h`
- Modify: `src/sim/park/unit_reads.cpp`

Step 1: Add BoxCells, a UnitValueIndex of BoxUnits named box-cells, filing each box under the cells (cellsOf) of its footprint's bounds from footprintOf(box.At, boxSize(box.Kind)), or none when footprintOf gives none. Add EntranceCells, of EntranceUnits, named entrance-cells, the same with ENTRANCE_SIZE. Register both in addParkUnits after the two kinds.

Step 2: Add, each into lists the caller keeps and clears first, reading units with unitValue and indexes with filedIn:
- `void pavingNear(const World &world, ParkPoint least, ParkPoint most, std::vector<const SurfacePiece *> &pieces)`: the same pieces as the UnitReads form.
- `void boxesNear(const World &world, ParkPoint least, ParkPoint most, std::vector<const ParkBox *> &boxes)`: the boxes filed in the rectangle's cells, ascending by key, once each.
- `void entrancesNear(const World &world, ParkPoint least, ParkPoint most, std::vector<const ParkEntrance *> &entrances)`: the same for entrances.
Give them a NearLists in a TickBuffers argument, as the other world forms take one.

Step 3: Add `void changedParkCells(const World &world, std::vector<EntityKey> &cells, TickBuffers &buffers)`, keeping the list ascending without repeats: the changed entries of point-cells, segment-cells, box-cells, and entrance-cells (changedUnits on each index), and filedUnder of each of those indexes for each key changedUnits gives of PointPavingUnits, SegmentPavingUnits, BoxUnits, and EntranceUnits. changedUnits sorts the whole list it appends to, so the unit keys go into a list of their own, ChangedLists.UnitKeys, kept in the buffers, before their cells go into cells.

Step 4: Add SegmentFurnishings, a unit kind named segment-furnishings over PathSegmentIntent with Reads `std::tuple<LeftFurnishingIntent, RightFurnishingIntent>`. Its value is a SegmentFurnishingUnit: HasLeft and Left, HasRight and Right, each side's SideFurnishing or a default one when the side has none, read through reads.component. Its visitFields walks has-left, left-kind, left-spacing, has-right, right-kind, and right-spacing, since the walk takes no std::optional. Naming the two stores in Reads makes the schema log them (schema.cpp registers a unit table's Over and Reads stores as logged), so a command that changes only furnishing re-derives the unit. Register it in addParkUnits.

Step 5: Add `void changedSegments(const World &world, std::vector<EntityKey> &segments)`, ascending without repeats: the changed units of SegmentFurnishings, SegmentPavingUnits, and GroundLines.

Step 6: Add `bool parkSegment(const World &world, EntityKey key, ParkSegment &segment)`: fills the segment as parkSegments gives it, its ground line from the GroundLines unit and its furnishing from the SegmentFurnishings unit, and gives false when the key holds no segment. Add `std::optional<ParkPoint> pathPointAt(const World &world, EntityKey point)`: the point's position from its intent.

### Task 5: Give routes' walkways and frontage strips near a place

Files:
- Modify: `src/sim/routes/internal/connections.h`
- Modify: `src/sim/routes/connections.cpp`
- Modify: `src/sim/routes/networks.h`
- Modify: `src/sim/routes/networks.cpp`

Step 1: Give DoorWalkway a `double DoorWidth`, the connection's door's width, set in DoorWalkways::derive and visited as "door-width". Add `bool frontageStrip(const DoorWalkway &walkway, std::array<ParkPoint, 4> &corners)`: the strip from the door's face, as wide as DoorWidth, out to Meets, with the corners furniture's surroundingsOf gives today; false when its depth along Out is not above 0.

Step 2: Add FrontageCells, a UnitValueIndex of DoorWalkways named frontage-cells, filing each entity under the cells of each of its strips' bounds. Register it in addConnectionUnits after WalkwayCells.

Step 3: In networks.h, add, into lists the caller keeps and clears first:
- `void walkwaysNear(const World &world, ParkPoint least, ParkPoint most, std::vector<SurfacePiece> &pieces, std::vector<std::array<ParkPoint, 4>> &strips, TickBuffers &buffers)`: the walkway pieces of the entities filed in the rectangle's cells of walkway-cells, and the frontage strips of those in its cells of frontage-cells, each entity once, in key order.
- `void changedWalkwayCells(const World &world, std::vector<EntityKey> &cells, TickBuffers &buffers)`: as changedParkCells, for walkway-cells and frontage-cells over DoorWalkways. Its unit keys go into a ChangedLists of routes' own in the buffers.

### Task 6: Take furniture's bookkeeping off the walk, and record the baseline

Files:
- Modify: `src/sim/furniture/furniture.cpp`

laid-furniture is a derived component, so the walk, the hash, and worldsEqual see its fields today: each chain's Read, ReadCheck, and Pieces. Its bookkeeping is derived from the pieces and changes with how furniture finds dirty chains, so it must not be walked, as ResolvedEntries' Retired and FieldSlot's ByPlace aren't. Taking it off the walk changes every furnished world's hash once, with no piece moved. This task makes that change alone, before laying changes, so Task 12 can compare against outputs that differ only by it.

Step 1: LaidChains' visitFields visits nothing; its members are still copied with the world. layFurniture no longer destroys the laid-furniture entity when no chain remains: every resolution creates it, so a world in place and its saved, loaded, and resolved copy both hold it. A world with no LaidChains component, a new world or a loaded save, is at its first resolution.

Step 2: Build windows-debug and record the baseline.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/furniture-units-baseline-scenarios.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/furniture-units-baseline-full.txt`
Expected: both files written. Every world's hash differs from build/route-updates-scenarios.txt and build/route-updates-full.txt, since laid-furniture's fields left the walk and its entity now exists in every resolved world.

Step 3: Read this task's diff and confirm it changes only LaidChains' visitFields and the destroy of the laid-furniture entity: no line that membersOf, chainsOf, readBy, keptChain, layChain, or publishSeatPlaces reads. That is what shows no piece moved.

### Task 7: Keep the chains furniture laid

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: A LaidChain becomes: Order, the lowest member's segment key and whether it is the left side; Members, each member's segment key and side; Pieces, the keys it laid; and Cells, the cells (pavingCells) of the reach rectangles readBy builds today. LaidChains becomes a KeptMap of LaidChain by chain key, the derived key of its lowest member, and a KeptMap from each cell to the chain keys filed there, ascending, and one from each segment to the chain keys holding its sides. None is visited. Remove Read and ReadCheck, readBy, keptChain, and Obstacles::hashNear.

Step 2: Keep in a TickBuffers struct, FurnitureLists, every list laying uses, cleared and refilled rather than rebuilt:
- the members, the walk's visited members and stack, and each formed chain's Steps;
- the chains to lay as a min-heap by Order, and the piece keys laid this resolution as a sorted vector, in place of the std::set;
- one Obstacles whose clear() empties only the cells it filled, and the near lists;
- ChainLine's raw points, raw spans, steps, kept indices, Points, Lengths, and Spans. ChainLine becomes a refill(members, chain, surface, distance) on these kept lists in place of its constructor;
- placesOf's places, runsOf's free flags and runs, and layChain's placed pieces, each passed in instead of returned;
- the seat-place entries a segment publishes;
- the changed segments and cells, the dirty chain keys, the queued, laid, and formed chain keys as sorted vectors, and the segments to republish.
Nothing in Tasks 8 to 10 allocates once these have grown.

### Task 8: Find the dirty chains

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: With no LaidChains, every furnished side of parkSegments is dirty, as today.

Step 2: Otherwise, dirty members are both sides of each segment changedSegments gives. The dirty chains are the chains the segment map files those segments under, and the chains the cell map files under each cell changedParkCells and changedWalkwayCells give.

Step 3: Form the chains to lay: from each dirty member that is furnished and has a side in walkedSurface, and from each member of each dirty chain, walk its linked sides both ways as linkedMember links them, building each member with parkSegment and pathPointAt. Take the component's lowest member and form its chain from it exactly as chainsOf does. Each chain is formed once. Each chain the segment map files a member of a formed chain under is dirty too, and its members are walked the same way, until no new chain is dirty.

Step 4: Before laying anything, remove each dirty chain whose key no formed chain has: queue every chain with a higher Order than the removed chain's that the cell map files in the cells of its pieces' grown footprints, then destroy its pieces and drop its map entries. Collect every such chain first, and never queue one of them: a chain removed in this resolution is not laid. A chain with the same key has the same Order, so only a removed chain can leave pieces behind chains it no longer precedes.

### Task 9: Lay the dirty chains in order

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: Pop chains by Order. A chain queued by Task 8 Step 4 or by Step 2 below has no formed chain yet. Form it first by walking from its LaidChain's lowest member as Task 8 Step 3 does, with parkSegment and pathPointAt. For each, fill the kept Obstacles for its reach cells' rectangles: pavingNear, the walkway pieces and strips from walkwaysNear, boxesNear and entrancesNear as surroundingsOf adds them, and the grown footprints of every piece of each chain with a lower Order that the cell map files in those cells. Chains are popped in Order and each updates the maps when laid, so those chains are already current. Lay the chain with layChain against them.

Step 2: Destroy each of the chain's old pieces that it didn't lay again and that no chain laid this resolution holds, since a side can move to an earlier chain. Replace its LaidChain and its entries in both maps. For each piece whose key or pose differs from before, laid now or laid before and not now, push each chain with a higher Order that the cell map files in the cells of that piece's grown footprint, unless it is queued or laid. Pieces that kept their key and pose are the same obstacles, so they push nothing.

Step 3: Remove surroundingsOf and the whole-park loops of layFurniture. The first resolution goes through the same steps with every chain dirty.

### Task 10: Edit the seat places

Files:
- Modify: `src/sim/furniture/furniture.h`
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: Give SeatPlaces `static constexpr bool KeepsResolved = true;`.

Step 2: Republish only the segments that are members of a chain laid or removed this resolution, before or after, and the segments changedSegments gives: editResolved with the segment's pieces' entries in key order, as publishSeatPlaces makes them, or dropResolved when it has none.

### Task 11: Watch furniture in the allocation check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Remove furniture from EXCUSED_STEPS. The new indexes are watched as every step is.

### Task 12: Confirm the feature

Files: none.

Task 6's outputs are the before outputs for the scenarios: build/furniture-units-baseline-scenarios.txt and build/furniture-units-baseline-full.txt. build/route-updates.report is the before report.

Step 1: Build both presets, run the tests, tidy, and the scenarios.

Run: `cmake.exe --build --preset windows-debug && cmake.exe --build --preset windows-release && ctest.exe --preset windows-debug && scripts/tidy.sh && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/furniture-units-scenarios.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/furniture-units-full.txt && build/windows-debug/tpj_scenarios.exe --compare build/furniture-units-baseline-scenarios.txt build/furniture-units-scenarios.txt && build/windows-debug/tpj_scenarios.exe --compare build/furniture-units-baseline-full.txt build/furniture-units-full.txt`
Expected: no warning, every test passes, tidy prints "tidy: clean.", and both comparisons exit 0.

Step 2: As soon as the build passes, stage only this feature's paths by pathspec, never with `git add -A`, and dispatch the reviewer on the staged diff through the reviewing skill, in parallel with the checks. Present its findings verbatim.

Step 3: Run `scripts/runtime-report.sh build/furniture-units.report && scripts/runtime-report.sh --compare build/route-updates.report build/furniture-units.report`. Expected: shop-drag and path-drag faster, each median at least 0.7 ms below. Every stage whose result is a hash is marked result-changed, because Task 6 changed the walk; only the medians are judged.

Step 4: Profile with Very Sleepy four times into build/perf/furniture-units-1.sleepy to -4, as the milestone's research notes do, and confirm furniture is under 2% of the shop drag's timed frame.

Step 5: Read layFurniture and everything it calls for criterion 2, and that nothing allocates once its lists have grown.

### Task 13: Review and commit

Step 1: Commit with the commit-hygiene skill, subject `Sim: Lay furniture only where an edit reaches`. Check `git diff --stat` and amend with any files the hook formatted.
