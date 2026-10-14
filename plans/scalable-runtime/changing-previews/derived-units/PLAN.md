# Implementation Plan: Derived Units

## Goal

Resolution keeps derived values in keyed units, re-derives only those whose recorded reads changed, and derives the path ground that way. tpj_bench times a scripted shop drag and path drawing.

## Approach

The world logs each intent key that a non-const accessor touches, on the stores units read. A unit space, held by the world outside its stores, keeps each unit kind's values, what each value's derivation read, and, for each thing read, its readers. Each kind and index runs as a resolver step in registration order. A step derives again the units whose entity changed or whose reads changed, in ascending key order, and records a change only when a value comes out different.

The ground's four passes in layout.cpp become per-point and per-segment functions. groundOf calls them for a whole layout, and five unit steps call them for one point or segment, so both paths compute alike. The bench stages land first, so the before report holds them.

## Placement

Decision 0027 places each behavior this feature adds:

- Units, indexes, recorded reads, and their update: sim, src/sim/units.h, src/sim/unit_space.h, and src/sim/unit_space.cpp, in the sim root next to the schema and the cycle. It is resolution machinery that every module registers with, and world.cpp, walk.cpp, and cycle.cpp include it, so it must sit in the sim root's layer (cmake/layers.txt's sim/*). units.h is what modules use.
- Registering unit kinds and indexes, and marking the stores units read: sim, src/sim/schema.h, WorldSchema. The schema owns what modules register.
- Logging changes and holding the unit space: sim, src/sim/world.h and src/sim/world.cpp, World. The world owns its storage and its accessors.
- Carrying units in a copy, and refilling a kept world from another: sim, src/sim/walk.cpp, copyWorld, and src/sim/component_store.h and src/sim/schema.h, ComponentStore and TypedStore. They own copying and the stores.
- Making a candidate into a kept world: sim, src/sim/cycle.cpp, makeCandidate. The cycle owns candidates.
- The preview keeping its candidate's storage and buffers between frames: legible, src/legible/preview.cpp, KeptPreview. It owns the kept preview.
- Ending a resolution's change records: sim, src/sim/cycle.cpp, resolveWorld. The cycle owns resolution.
- The ground's per-point and per-segment functions: park, src/sim/park/layout.cpp, private to the module. It already owns the ground.
- The ground units and the shared world ground: park, src/sim/park/ground_units.cpp, with worldGround in src/sim/park/internal/ground_units.h and addPathGround in the public src/sim/park/surface.h, as addPavedSurface is. The park module owns the ground and its readers.
- Registering the ground units: sim, src/sim/park_schema.cpp, addPark, through sim/park/surface.h. Composition only.
- The drag stages: bench, src/bench/stages.cpp. The runner owns the stages.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, build tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: exit status 0 and no warning.

Step 2: Record the scenarios.

Run: `mkdir -p build/derived-units && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/derived-units/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/derived-units/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c derived-units`.

### Task 2: Write the specs

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/bench/SPEC.md`
- Modify: `src/sim/park/SPEC.md`

Step 1: Add the Units section from FEATURE.md's Spec changes to src/sim/SPEC.md, after The tick cycle.

Step 2: Add the two stages from FEATURE.md's Spec changes to src/bench/SPEC.md's list, after the path edit's stages, and its sentences on frames, results, and throwing to the paragraph after the list.

Step 3: Add the addPathGround sentence from FEATURE.md's Spec changes to src/sim/park/SPEC.md's Boundary, after the sentence on addParkEdits.

### Task 3: Add the units interface

Files:
- Create: `src/sim/units.h`
- Create: `src/sim/unit_space.h`
- Create: `src/sim/unit_space.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/world.h`
- Modify: `src/sim/walk.cpp`
- Modify: `src/sim/cycle.cpp`

Step 1: Declare in units.h, in namespace tpj, the UnitReads class, the UnitKind and UnitIndex concepts, addUnitIndex, addUnits, unitValue, and unitsCurrent, as FEATURE.md's public interface gives them, each with the comment given there. The concepts require:
- a `static constexpr std::string_view Name`;
- an `Over` type;
- for a kind, a default-constructible `Value` with a visitFields function, a `Reads` that is a std::tuple of the intent types it reads, and `static void derive(UnitReads &, EntityKey, Value &)`;
- for an index, `static void keysOf(const Over &, std::vector<EntityKey> &)`.

Give each template a stub body that compiles and returns null, an empty span, or false.

Step 2: Declare in src/sim/world.h, after copyWorld, `void copyWorld(const World &world, World &into);`, with the comment Task 11 gives. Declare after the returning makeCandidate `void makeCandidate(const World &world, const CommandQueue &commands, TickBuffers &buffers, World &into);`, with the comment Task 12 gives. Give each a stub body in walk.cpp and cycle.cpp that does what the returning form does and moves the result into into, so the test pass can build against them.

Step 3: Add unit_space.cpp to tpj_sim's sources in src/sim/CMakeLists.txt.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 4: Run the test pass

Dispatch the test-writer agent as implementing-features describes, with FEATURE.md, src/sim/SPEC.md, src/sim/park/SPEC.md, src/sim/units.h, and src/sim/world.h.

### Task 5: Add the drag stages

Files:
- Modify: `src/bench/stages.h`
- Modify: `src/bench/stages.cpp`

Step 1: In stages.h, add `inline constexpr double DRAG_STEP = 0.5;` with the comment "How far the drag stages move their edit each frame, in metres." Add `inline constexpr double DRAG_SPAN = 2.0;` with the comment "The farthest the shop drag moves its shop from where it stands, in metres." Add `inline constexpr double DRAG_START_REACH = 4.0;` with the comment "How far beyond its segment's side the path drag's end starts, in metres." Add shop-drag and path-drag to benchPark's comment, after path-ghost, with the throws.

Step 2: In stages.cpp, split pathSpur so the segment it chose and the side it accepted come out with the path. Add `std::optional<SpurChoice> spurChoice(const World &park)`, where SpurChoice holds the segment's key, its line's middle, its unit normal on the accepted side, and its half width. pathSpur keeps its behavior on top of it.

Step 3: Add a function, in the anonymous namespace, that times a drag:

    template <typename EditAt>
    StageResult timeDrag(std::string name, const World &park, EditAt editAt, const std::string &refused)

Its steps:
- Copy park into a World named stepped. Keep one TickBuffers and one KeptPreview.
- For each frame from 0 to WARM_UPS + REPETITIONS - 1:
    - Untimed: `stepWorld(stepped, buffers)`. Make the frame's edit with `editAt(frame)`. If `isAccepted(stepped, edit)` is false, throw std::runtime_error(refused + " at frame " + the frame).
    - Timed: `keepPreview(kept, stepped, edit, false)`, then, if kept.Made.Candidate holds a world, `buildGhostMesh(stepped, edit, kept.Made.Candidate)`.
- Keep the times of the frames from WARM_UPS on. Return a StageResult of kind Hash, with the hash of the last frame's candidate.

Step 4: Add two functions, each marked [[gnu::noinline]] so that a release profile keeps their frames and can tell the drags apart.
- `timeShopDrag(const World &park, const ParkBox &shop)` calls timeDrag("shop-drag", ...). Frame k's edit is `MoveBox{shop.Key, pose}`, where pose is shop.At moved along its face, its facing kept. The direction is (-FacingZ, FacingX) divided by its length, since a pose's facing is held as given and need not be a unit vector. The distance goes out and back within DRAG_SPAN. With n = DRAG_SPAN / DRAG_STEP steps out, which is 4, it is `DRAG_STEP * (n - |n - (k + 1) % (2n)|)`. That gives 0.5, 1.0, 1.5, 2.0, 1.5, 1.0, 0.5, 0, then repeats. So no two frames in a row share an edit, and the shop never moves more than 2 m. A shop on full.park has 4.5 m of room on each side between its grid's paths.
- `timePathDrag(const World &park, const SpurChoice &spur)` calls timeDrag("path-drag", ...). Frame k's edit is `AddPath{PathKind::Guest, {OnSegment{spur.Segment, 0.5}, NewPoint{middle + normal * (half width + DRAG_START_REACH + DRAG_STEP * k)}}}`.

Step 5: In benchPark, after the path stages, add shop-drag when parkBoxes(park) holds a box of kind Shop, using the lowest-keyed one, and path-drag when spurChoice gives a segment.

Run: `cmake.exe --build --preset windows-release --target tpj_bench && build/windows-release/tpj_bench.exe --ticks 1 tests/parks/stress/full.park && build/windows-release/tpj_bench.exe --ticks 1 tests/parks/stress/winding-path.park`
Expected: exit status 0 for both. Each output holds `stage shop-drag count 11` and `stage path-drag count 11` lines after `stage path-ghost`, so every frame's edit was accepted on both stress parks the runtime report times.

### Task 6: Record the runtime before the mechanism

Files: none.

Step 1: Build the whole windows-release preset, then write the report.

Run: `cmake.exe --build --preset windows-release && scripts/runtime-report.sh build/derived-units-before.report`
Expected: exit status 0. The report holds shop-drag and path-drag for full.park.

### Task 7: Register unit kinds and indexes

Files:
- Modify: `src/sim/schema.h`
- Modify: `src/sim/schema.cpp`
- Modify: `src/sim/units.h`

Step 1: Give WorldSchema a list of unit tables: each with a name, whether it is a kind or an index, the store index of its Over, and a function that makes its empty table. Add `std::vector<bool> Logged` by store index, with a reader.

Step 2: addUnitIndex<I> and addUnits<K> throw std::invalid_argument when Over isn't a registered component, or when its DataKind isn't Intent. Otherwise they add the table and mark Over's store logged. They then call addResolver with the table's name and the step function updateIndex<I> or updateUnits<K>, declared in units.h with empty bodies until Task 9 fills them, so the step runs in resolver order and its name can't repeat a resolver's.

Step 3: A kind names the intent types its derivation reads, as `using Reads = std::tuple<...>`. addUnits marks each of their stores logged, and throws std::invalid_argument when one isn't an intent component.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 8: Hold the unit space in the world

Files:
- Modify: `src/sim/unit_space.h`
- Modify: `src/sim/unit_space.cpp`
- Modify: `src/sim/world.h`
- Modify: `src/sim/world.cpp`
- Modify: `src/sim/walk.cpp`

Step 1: In unit_space.h, declare class UnitSpace. It holds:
- for each component store index, whether it is logged, and its log of touched keys;
- a counter of changes, raised by each logged touch;
- for each unit kind and index, its table: values or filed keys by key, each unit's recorded reads, and the counter value its last update ran at, or none before its first;
- for each table that is read (stores, indexes, and kinds), a map from key to that key's readers;
- the list of what this resolution changed: a table and a key for each. Each logged store's log goes onto it, as changes to that store, when the resolution starts.

Removed map nodes go to spare lists for later inserts, as TypedStore's Spare does, and lists are emptied with clear(), so updates allocate nothing once grown. UnitSpace has a clone function that copies everything but the spares.

Step 2: World holds `std::unique_ptr<UnitSpace> Units`, made in the constructor from the schema's unit kinds, indexes, and logged stores. Define World's destructor and move operations in world.cpp, since UnitSpace is incomplete in world.h.

Step 3: Give World a private `noteChange(size_t store, EntityKey key)` that, if the store is logged, appends the key to its log and raises the counter. Call it from the non-const get, tryGet (when it finds the component), emplace, emplaceOrReplace, getOrEmplace, and remove, through a findStore that also gives the store's index. Call it from destroyEntity for each logged store that holds the key.

Step 4: In copyWorld, set `copy.Units = world.Units->clone()`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 9: Update units and indexes

Files:
- Modify: `src/sim/unit_space.h`
- Modify: `src/sim/unit_space.cpp`
- Modify: `src/sim/units.h`

Step 1: updateIndex<I>(World &, TickBuffers &):
- Before the index's first update, take every key of the Over store.
- Otherwise, take the keys in Over's log, sorted and unique.
- For each, compute keysOf of its Over, or no keys when it no longer holds Over. Compare them with the keys it was filed under. Remove it from each old entry and add it to each new one, keeping each entry ascending.
- Add each entry that changed to the resolution's change list.
- Set the index's counter to the space's counter.

Step 2: updateUnits<K>(World &, TickBuffers &) gathers its dirty keys in a kept list:
- Before its first update, every key of the Over store.
- Otherwise, the keys in Over's log, and every reader of kind K of each entry on the resolution's change list. Logged stores' changes are on that list, so a segment's straight line derives again when one of its points moves, though only the point's key was logged.

Sort and dedupe them. For each key, ascending:
- If the entity no longer holds Over, erase its unit and its reads, and put it on the change list when it had a unit.
- Otherwise, derive into a kept scratch value, through a UnitReads that records into a kept list.
- If the unit is new or its value differs, swap the scratch into the unit and put the key on the change list. Values are compared by the words their visitFields emits, the walk's own equality, into two kept word lists. operator== is not used, since it calls +0.0 and -0.0 equal and the walk doesn't.
- Replace the unit's recorded reads, updating the readers of what it read before and of what it reads now.

Set the kind's counter to the space's counter.

Step 3: UnitReads records each read as its table and key, then reads:
- component<T> through the world's const tryGet. In debug builds, it throws std::logic_error when T isn't one of the kind's Reads.
- unit<K> from K's table. In debug builds, it throws std::logic_error when K isn't registered before the kind being derived.
- filed<I> from I's entry.

Step 4: unitValue<K> reads K's table without recording. unitsCurrent<K> is true when K's counter equals the space's counter.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 10: End a resolution's changes

Files:
- Modify: `src/sim/cycle.cpp`

Step 1: At the start of resolveWorld, before the first resolver, put each logged store's log onto the resolution's change list. At its end, after the finishers, clear every store's log and the change list. They stay through the whole resolution, so later steps see earlier steps' changes.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 11: Refill a kept world

Files:
- Modify: `src/sim/component_store.h`
- Modify: `src/sim/schema.h`
- Modify: `src/sim/world.h`
- Modify: `src/sim/walk.cpp`
- Modify: `src/sim/unit_space.h`
- Modify: `src/sim/unit_space.cpp`

Step 1: Give ComponentStore `virtual void assignFrom(const ComponentStore &other) = 0`, which makes the store hold exactly the other's components. TypedStore walks both maps in key order:
- For a key both hold, it assigns the value in place, so a component's lists keep their memory.
- For a key only it holds, it erases the key into Spare.
- For a key only the other holds, it adds the key, taking a node from Spare.

Step 2: Give UnitSpace an assignFrom that does the same for every table, every unit's recorded reads, every map of readers, the logs, the change list, and the counters.

Step 3: Give copyWorld(world, into), declared in Task 3, the comment "Makes into equal to the world, its units included, reusing into's memory. Throws std::invalid_argument when into doesn't share the world's schema. Allocates nothing once into has held a world as large." Replace its stub in walk.cpp: it throws std::invalid_argument when into's schema pointer differs from the world's. It copies Tick, Seed, NextKey, and ResolvePending. It assigns ByKey the same way, reusing SpareRecords. It calls assignFrom on every store and on the unit space.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 12: Keep the preview's candidate between frames

Files:
- Modify: `src/sim/world.h`
- Modify: `src/sim/cycle.cpp`
- Modify: `src/legible/preview.h`
- Modify: `src/legible/preview.cpp`

Step 1: Give makeCandidate(world, commands, buffers, into), declared in Task 3, the comment "Makes into the candidate makeCandidate gives, refilling it with copyWorld(world, into) instead of making a new world." Replace its stub in cycle.cpp, and have the returning form call it on copyWorld(world).

Step 2: KeptPreview gains `std::optional<World> Spare`, `TickBuffers Buffers`, and `CommandQueue Queue`. Making the preview again clears Queue and queues the edit in it, instead of the local CommandQueue at preview.cpp:81. When keepPreview makes the preview again, it moves the kept candidate, if any, into Spare. If Spare holds no world, or holds one whose schema is a different object from the world's, as after a caller replaces the world and clears only the kept tick, it takes copyWorld(world) instead, since World offers no other way to make a world on another world's schema. So clearing the tick stays enough, as src/legible/SPEC.md says. It makes the new candidate into Spare with KeptPreview's Buffers, then moves it into Made.Candidate. previewEdit keeps its returning form, built on the same steps with a world and buffers of its own. Update preview.h's comment on KeptPreview to name the three new members.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/tpj_sim_tests.exe && ctest.exe --preset windows-debug -R allocation`
Expected: no warning, and all tests pass. The allocation check watches the kept preview while its edit holds still.

### Task 13: Split the ground into per-point and per-segment functions

Files:
- Modify: `src/sim/park/layout.cpp`
- Modify: `src/sim/park/internal/layout.h`

Step 1: Rounding keeps its own two arc halves as lists, instead of indices into the shared arc buffer. Bend and Rounding name their segments by key instead of by layout index. roundedLine takes the halves. The kept lists keep each rounding's halves with resizeKeeping.

Step 2: Make these functions, declared in internal/layout.h so ground_units.cpp can call them, each reading only what it is given:
- `straightLineOf(const PathSegmentIntent &, const PathPointIntent *from, const PathPointIntent *to, std::vector<CarrierPoint> &line)`.
- `armsAt(EntityKey point, const PathPointIntent &, std::span<const ArmInput> ends, PointArms &out)`. ArmInput is one segment end at the point: its segment's key and intent, whether its straight line is empty, and its far point's intent. A segment with both ends at the point gives two. PointArms is the bend, or none, and the reach along each segment, by segment key. The bend needs exactly two ends with straight lines, as roundingsOf counts them.
- `cornerAt(const PointArms &arms, NeededAt needed, Rounding &out)`. needed(far point, segment key) is the larger of the far point's bend's tangent for its asked radius and its reach along the segment, as roundingsOf's needed gives.
- `groundLineOf(EntityKey key, const PathSegmentIntent &, std::span<const CarrierPoint> straight, const Rounding *start, const Rounding *end, ParkSegment &out)`.

Step 3: Mark straightLineOf, armsAt, cornerAt, and groundLineOf [[gnu::noinline]], so a release profile shows each derivation of the ground by name, from groundOf and from the units alike. Rebuild fillGround on these functions, over the layout's points and segments in key order, so groundOf gives what it gave.

Step 4: Check the simulation is unchanged.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios && build/windows-debug/tpj_sim_tests.exe && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/derived-units/scenarios-split.txt && build/windows-debug/tpj_scenarios.exe --compare build/derived-units/scenarios-before.txt build/derived-units/scenarios-split.txt`
Expected: all tests pass, and the comparison exits with status 0.

### Task 14: Add the ground units

Files:
- Create: `src/sim/park/internal/ground_units.h`
- Create: `src/sim/park/ground_units.cpp`
- Modify: `src/sim/park/surface.h`
- Modify: `src/sim/park_schema.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: In ground_units.cpp, define one index and four kinds:
- Index SegmentEnds, "segment-ends", over PathSegmentIntent: keysOf gives From and To, once each.
- Kind StraightLines, "straight-lines", over PathSegmentIntent. Its Value is the straight line. Its derivation reads the segment and its two points, and calls straightLineOf.
- Kind PointArmsUnits, "point-arms", over PathPointIntent. Its Value is PointArms. Its derivation reads:
    - the point;
    - the segments filed under it;
    - for each segment, its intent, its StraightLines unit, and its far point.

    It builds the ArmInput list in segment key order, and calls armsAt.
- Kind Corners, "corners", over PathPointIntent. Its Value is the Rounding, or none. Its derivation reads the point's PointArmsUnits unit. When the point has a bend, it reads the PointArmsUnits unit at each of the bend's far points, and calls cornerAt.
- Kind GroundLines, "ground-lines", over PathSegmentIntent. Its Value is the ParkSegment with no furnishing. Its derivation reads the segment, its StraightLines unit, and the Corners units at its two points. As fillGround does, it passes no corners when the straight line is empty, and calls groundLineOf.

Each Value gets a visitFields function listing every field it holds, so early cutoff compares it word for word. emitField has no std::optional, so a value that may be absent holds a bool saying whether it is present, and default fields when it isn't: PointArms' bend and the Corners value do this.

Step 2: Declare in src/sim/park/surface.h, next to addPavedSurface, `void addPathGround(WorldSchema &schema)`, with the comment "Registers the units the path ground derives in: an index of segments by their points, then each segment's straight line, each point's arms, each point's corner, and each segment's ground line." It registers them in that order. Declare in internal/ground_units.h `void worldGround(const World &world, GroundLists &lists)`. When unitsCurrent holds for GroundLines and Corners, it fills lists.Ground from the units: the segments in ascending key order, and the corners of the points that have one, in ascending key order. Otherwise it runs layoutOf and groundOf, as today. Fill lists.Layout either way.

Step 3: In park_schema.cpp, call addPathGround(schema) before addPavedSurface(schema). Add ground_units.cpp to tpj_sim's sources.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 15: Read the world's ground from the units

Files:
- Modify: `src/sim/park/intent.cpp`
- Modify: `src/sim/park/surface.h`
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/sim/park/edits.cpp`
- Modify: `src/sim/park/SPEC.md`
- Modify: `src/sim/routes/networks.cpp`
- Modify: `src/sim/guests/ground.cpp`
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: Make every reader of the world's ground call worldGround with GroundLists from its buffers, or from a local GroundLists where it has none:
- both forms of parkSegments and parkCorners;
- both forms of parkSurface;
- parkEdges and parkTapers;
- the box acceptance checks at edits.cpp:2021 and 2030. piecesNear (edits.cpp:1737) takes the layout and its ground instead of deriving the ground itself. These two calls pass worldGround's, and the call at edits.cpp:1962, on a hypothetical layout, passes groundOf of it.

Leave the edits on hypothetical layouts calling groundOf.

Step 2: Add buffered forms `PavedSurface parkSurface(const World &world, TickBuffers &buffers)` and `PavedSurface parkSurface(const World &world, std::span<const Walkway> walkways, TickBuffers &buffers)` to surface.h. They take the ground from worldGround with the GroundLists in the buffers. The surface they build still allocates until unit-surface. Make the five resolvers that call returning readers today pass their buffers instead:
- resolvePavedSurface (surface.cpp:1399) and resolveWalkedSurface (networks.cpp:554) call the buffered parkSurface.
- resolvePathNetworks (networks.cpp:521), resolveGuestGround (ground.cpp:265), and layFurniture (furniture.cpp:1015) call the buffered parkSegments into a list each keeps in a struct of its own in the buffers.

The returning forms, which callers outside resolution use, keep a GroundLists of their own until the features that own those callers.

Step 3: Add the parkSurface sentence from FEATURE.md's Spec changes to src/sim/park/SPEC.md's Boundary, after the sentence on parkSegments, parkCorners, parkEdges, and parkTapers keeping their lists in TickBuffers.

Step 4: Check the simulation is unchanged.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios && build/windows-debug/tpj_sim_tests.exe && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/derived-units/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/derived-units/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/derived-units/scenarios-before.txt build/derived-units/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/derived-units/full-before.txt build/derived-units/full-after.txt`
Expected: all tests pass, and both comparisons exit with status 0.

### Task 16: Confirm the feature

Files: none.

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warning, every test passes, and tidy prints no finding.

Step 2: Write the after report and compare.

Run: `cmake.exe --build --preset windows-release && scripts/runtime-report.sh build/derived-units-after.report && scripts/runtime-report.sh --compare build/derived-units-before.report build/derived-units-after.report`
Expected: no stage is marked result-changed.

Step 3: Profile the drags. Run this eight times, with NAME from derived-units-1 to derived-units-8:

    W=$(wslpath -w .)
    "/mnt/c/Program Files/Very Sleepy/sleepy.exe" "/r:$W\build\windows-release\tpj_bench.exe --ticks 1 $W\tests\parks\stress\full.park" "/o:$W\build\perf\NAME.sleepy"

Over the eight profiles, sum the samples whose stacks hold both timePathDrag and resolveWorld. Among them, sum those that also hold straightLineOf, armsAt, cornerAt, or groundLineOf. Do the same for timeShopDrag.
Expected:
- Each first sum is above 0, so both wrappers kept their frames.
- Under timePathDrag, the second sum is under 0.15% of the first. Deriving the whole layout's ground once is about 0.6% of a resolution, so a frame that derived every ground unit would fail.
- Under timeShopDrag, the second sum is 0.

Step 4: Read, for an allocation after their lists have grown, exactly what criterion 5 names: updateIndex, updateUnits, and what they call; the ground units' derivations; worldGround; copyWorld(world, into) and assignFrom; and keepPreview's refill of its candidate and its queue. Fix any you find. The resolvers' own reader lists, the acceptance check's, and the shop context's are unit-surface's, layered-candidate's, and spatial-units' (MILESTONE.md), so leave them.

### Task 17: Commit

Stage only this feature's paths. Commit with the commit-hygiene skill, subject `Sim: Derive the path ground in units that record their reads`. Then check `git diff --stat` and amend if the hook formatted files.
