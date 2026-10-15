# Implementation Plan: Route Updates

## Goal

Route distance updates only the entries an edit changes, in whole millimetres, keeping each source's entries between resolutions.

## Approach

The medium lets a field keep its resolved entries and have its resolver edit sources in place. Route distance adds lengths in whole millimetres and keeps each source's entries ordered by place. On a resolution with a previous network, it compares the two networks once, matches nodes by place, and finds the touched nodes. Then, for each source, it runs LPA* without a heuristic from the touched nodes, which visits only nodes whose distance changes and their neighbours. A source whose anchored node is touched, or that is new, is searched in full.

## Placement

Decision 0027 places each behavior this feature adds:

- Keeping a field's resolved entries and editing them in place: medium, src/sim/medium/field.h. The medium owns both layers of every field and the rules for publishing into them.
- Whole-millimetre lengths, the network comparison, and the per-source update: routes, src/sim/routes/route_distance.cpp. Route distance owns its search and reads the network and the previous network that routes' own path-networks keeps.
- The decision record: docs/decisions/0042-whole-millimetre-routes.md, because it changes the simulation's results under the capability's exception rule.

## Tasks

### Task 1: Write the specs and the decision

Files:
- Modify: `src/sim/medium/SPEC.md`
- Modify: `src/sim/routes/SPEC.md`
- Create: `docs/decisions/0042-whole-millimetre-routes.md`
- Modify: `plans/scalable-runtime/CAPABILITY.md:14`
- Modify: `plans/scalable-runtime/changing-previews/MILESTONE.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

Step 2: Write decision 0042, "Route distance adds lengths in whole millimetres", Status Accepted with today's date, in 0041's shape:
- Context: an exact update must give the same distance however an edge is cut, and double sums don't. MILESTONE.md Decisions 1 gives the shop-drag example.
- Decision: each edge's length is its stops' distances, each rounded to whole millimetres, subtracted. Distances are their sums, published as metres. The user chose it on 2026-10-13.
- Consequences: distances change by under a millimetre per edge, and a tie between two equal routes may break the other way. tpj_scenarios output changes once, in this feature. A mover walking its next steps adds up the rounded lengths exactly.

Step 3: In MILESTONE.md's list of whole-park work left, add: "Route distance's slot moves: writing a source's changed entries moves the rest of its list, about 1.7 MB per path-drag frame on full.park, growing with sources times nodes."

Step 4: In CAPABILITY.md line 14, after "route distance adds edge lengths in whole millimetres", add " (decision 0042)".

### Task 2: Keep a field's resolved entries

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Add a concept KeepsResolvedEntries<F>: a FieldDefinition with `static constexpr bool KeepsResolved = true;`. Comment: a field whose resolver edits its resolved entries in place and keeps the rest from one resolution to the next.

Step 2: In emptyResolvedEntries, for such a field, create the entity and both components as now, but leave Slots as they are. Retire Previous and set HasPrevious to false.

Step 3: Add editResolved<F>(world, source), for such fields only. Refuse as publishResolved does outside a resolver, with no resolved entries, or for the null key, with the same messages. When the source has a readable stepped slot and Previous holds no slot for it, insert a copy of its current resolved slot into Previous in source order, or an empty slot when it has none, and set HasPrevious. Then return the source's slot's Entries for editing: the existing slot, or one taken from Retired, or a new one, inserted in source order. Set the slot's Ordered to false, since the caller changes the entries. Allocates nothing once Retired and Previous have grown.

Step 4: Add dropResolved<F>(world, source) with the same refusals and the same copy into Previous, then move the source's slot, if any, to Retired, emptied as retireSlots empties it.

Step 5: In settleSteppedEntries, for a field that keeps its entries, keep a readable stepped slot whose source Previous doesn't hold. Compare the others as now.

### Task 3: Add lengths in whole millimetres

Files:
- Modify: `src/sim/routes/route_distance.cpp`
- Modify: `src/sim/routes/route_distance.h`

Step 1: Give RouteDistance<Kind> `static constexpr bool KeepsResolved = true;`.

Step 2: Add `int64_t millimetresOf(double metres)`, std::llround of metres times 1000, and `double metresOf(int64_t millimetres)`, the value divided by 1000.0. A distance in millimetres is int64_t throughout the file. INT64_MAX means not reached.

Step 3: Leaving.Length becomes int64_t: millimetresOf(To's distance) minus millimetresOf(From's distance) of the edge.

Step 4: searchEntries keeps distances as int64_t in a kept list, and checks a next step with far distance plus length equal to the node's distance in integers. RouteEntry.Distance is metresOf of it. routeEntries gives the same.

Step 5: Add `void fullSearch(...)`: searchEntries for the source, then clear its slot's entries through editResolved and fill them with every reached node's entry at nodePlace, sorted by placeBefore. The from-nothing resolution and a source searched in full both use it.

### Task 4: Compare the networks once per resolution

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Add to RouteLists, for the resolution: Matched (a uint8_t per new node), Touched (new nodes, ascending, without repeats), and Removed (the old nodePlaces no new node holds, ascending by placeBefore).

Step 2: A new node is matched when the previous network resolves its nodePlace to a NodePosition whose nodePlace equals it. An old node is removed when the new network doesn't match it in the same way. Fill Matched and Removed this way.

Step 3: Walk the two networks' carriers together by key. A carrier changes when only one network holds it, or its stops' distances differ in count or value. Touched holds every new node a changed new carrier stops at, every new node matched to an old node a changed old carrier stops at, found by resolving that old stop's place in the new network, every unmatched new node and its neighbours, and the new nodes matched to each removed old node's neighbours in the previous network. A node outside Touched then keeps the old distances of all its neighbours, so its old distance is still the least over them.

### Task 5: Update a source in place

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Add to RouteLists kept per-source lists by new node: G and Rhs, int64_t; Loaded, a uint32_t generation; Queued; and Changed, the nodes whose G differs from what was loaded. Raise the generation once per source instead of clearing the lists.

Step 2: Loading a node v for the source, on first touch: G and Rhs are the old distance, millimetresOf the entry the source's slot holds at v's nodePlace when v is matched, found by binary search with placeBefore, and not reached otherwise.

Step 3: Rhs of a node is 0 when the source anchors it, and otherwise the least of each leaving step's far node's G plus its length, or not reached.

Step 4: For each touched node, set its Rhs, and queue it with key min(G, Rhs) when the two differ. Pop least key first, ties to the lower node. When G is above Rhs, set G to Rhs, and lower each neighbour's Rhs to this G plus the step's length when that is less, queuing it if it becomes inconsistent. When G is below Rhs, set G to not reached, and set the Rhs again of the node and of each neighbour, queuing each that is inconsistent. Stop when the queue is empty. Record in Changed each node whose final G differs from what was loaded.

Step 5: The nodes to write are Changed, every neighbour of a Changed node, and Touched. For each one the source doesn't anchor, its entry is none when G is not reached. Otherwise it is metresOf(G) with Next the preferred step, by isPreferredStep, among leaving steps whose far node's G plus length equals G.

Step 6: Write the source's changes in one pass. Collect each written node's new entry or its erasure, and an erasure at each Removed place the slot holds, sorted by placeBefore, in a kept list. Then merge them into the slot's entries through editResolved, moving each entry after the first change at most once.

### Task 6: Resolve route distance by updating

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: publishRouteDistance<Kind> gathers the sources, the anchored entities of the new network, as now. Without a previous network, or without the field's resolved entries, it drops every slot it holds and searches every source in full.

Step 2: With a previous network, it fills the leaving steps once and does Task 4. It drops each slot whose source the new network doesn't anchor. It searches in full each source with no slot, and each source with an anchored node in Touched. It updates every other source with Task 5. When no touched node has an old distance for the source, nothing the edit changed was reachable from it, so the update returns after loading them.

### Task 7: Watch route distance in the allocation check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Remove route-distance, guest-route-distance-field, and backstage-route-distance-field from EXCUSED_STEPS. The check fails while any of them still allocates, and also if one is left on the list after it stops.

### Task 8: Confirm the feature

Files: none.

watched-resolution changes only the allocation check, so unit-connections' outputs are still the before outputs: build/unit-connections-scenarios.txt, build/unit-connections-full.txt, and build/unit-connections.report.

Step 1: Build both presets, run the tests, tidy, and the scenarios.

Run: `cmake.exe --build --preset windows-debug && cmake.exe --build --preset windows-release && ctest.exe --preset windows-debug && scripts/tidy.sh && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/route-updates-scenarios.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/route-updates-full.txt; build/windows-debug/tpj_scenarios.exe --compare build/unit-connections-scenarios.txt build/route-updates-scenarios.txt; build/windows-debug/tpj_scenarios.exe --compare build/unit-connections-full.txt build/route-updates-full.txt`
Expected: no warning, every test passes, tidy prints "tidy: clean.", and both comparisons exit non-zero: every world with a guest path hashes differently. Read millimetresOf, the leaving steps' lengths, and searchEntries' sums for criterion 4.

Step 2: Dispatch the reviewer on the staged diff now, as Task 9 says, while the report runs.

Step 3: Write the after report and compare it.

Run: `scripts/runtime-report.sh build/route-updates.report && scripts/runtime-report.sh --compare build/unit-connections.report build/route-updates.report`
Expected: shop-drag and path-drag marked faster, each median at least 1 ms below the before report's. Stages marked result-changed are expected, since distances change.

Step 4: Profile the shop and path drags with Very Sleepy, with four runs of windows-release tpj_bench on full.park into build/perf/route-updates-1.sleepy to -4. Confirm route distance is under 5% of the shop drag's timed frame, and measure the share of the slot moves named in Out of scope.

Step 5: Read resolveRouteDistance and everything it calls. Confirm criterion 2, and that nothing allocates once its lists have grown.

### Task 9: Review and commit

Step 1: Stage only this feature's paths by pathspec, never with `git add -A`, and dispatch the reviewer on the staged diff through the reviewing skill. Present its findings verbatim.

Step 2: Commit with the commit-hygiene skill, subject `Sim: Update route distance in place in whole millimetres`. Check `git diff --stat` and amend with any files the hook formatted.
