# Feature: Targeted Route Samples

## Summary

A walking guest stops sampling every source's route distance on every pass of its walk. The shared medium gains sourceSlot, one source's slot of a field as the layer rule chooses it, and sourceEntryAtNode, the first of one source's entries at a node, and the network gains firstAnchoredNode, the lowest node anchored to an entity. Navigable-networks gains routeEntryAt, one source's route entry at a place, by the node rule or route distance's own edge rule. None of them allocates. The guests module reads its target's and the entrances' entries through routeEntryAt and a shop's offer through sourceEntryAtNode, and samples every source only when a guest chooses. Every result equals what the full sample gave, so the simulation's output does not change. The feature ends with the runtime report's comparison against the milestone's before report.

## Acceptance criteria

1. sourceSlot<F>(world, source) is the slot the layer rule chooses for the source: its readable stepped slot when it has one, even an empty one, otherwise its resolved slot, and none when the world holds neither for it. (test)
2. sourceEntryAtNode<F>(world, network, node, source) equals the first entry with that source in sampleField<F>(world, network, network.nodePlace(node)), and none when that sample has none for the source. (test)
3. firstAnchoredNode(entity) equals the first node of anchoredNodes(entity), and none when the entity anchors no node. (test)
4. routeEntryAt(world, kind, network, place, source) equals the source's entry in sampleField of the kind's route distance field on the network at the place, bit for bit, and none when that sample gives the source none, including at a place that does not resolve. (test)
5. Every existing test passes unchanged, and tpj_scenarios's windows-debug output over the registered scenarios and tests/parks/*.park is identical, line for line, before and after the feature, by tpj_scenarios --compare (decision 0030). (check)
6. A Very Sleepy profile of tpj_bench stepping tests/parks/stress/full.park 600 ticks on windows-release puts under 5% of stepWorld's time in sampleField of guest route distance, which the milestone's profile put at 19.5 of 23.5 ms. (check)
7. scripts/runtime-report.sh's comparison of the milestone's before report with a report after the feature shows the full park's windows-release ticks stage faster, marked clear, with a median below 33,333 µs in the after report's show output, and the full park's windows-release app frames median below 33,333 µs there, so the app holds one tick per frame. (check)

## Medium

No field or flow changes what it carries. Guests read guest route distance, published by navigable-networks' route-distance resolver, through routeEntryAt while walking and through sampleField when choosing, and the food offer, published by plausible-operations' shops, through sourceEntryAtNode. The new medium queries read a field's slots for one source and the network's anchors, and are open to any module, as sampleField is.

## Principle checks

- Principle 10: the new queries give exactly what the full sample gives (criteria 2 and 4), so guests behave the same and runs stay identical (criterion 5).
- Principle 6: guests read route distance only through the routes module's routeEntryAt and the medium's public queries, never another module's components; routeEntryAt reads the field through the medium's sourceSlot and sourceEntryAtNode.
- Principle 4: inside an edge, routeEntryAt applies route distance's own edge rule, the better of the edge's two ends along its carrier, shared with sampleEdge rather than restated.

## Spec changes

src/sim/medium/SPEC.md, appended to the paragraph beginning "nodePlace gives a node's place":

"firstAnchoredNode gives an entity's lowest anchored node, the first that anchoredNodes gives, or none, without allocating."

src/sim/medium/SPEC.md, a new paragraph after the one beginning "sampleField gives a field's entries at a place":

"sourceSlot(world, source) gives one source's slot of a field as the layer rule chooses it: its readable stepped slot when it has one, even an empty one, and otherwise its resolved one, or none when the world holds neither for the source, or holds no entries for the field. sourceEntryAtNode(world, network, node, source) gives the first of the source's entries that sampleField gives at the node's nodePlace, or none, and throws std::out_of_range for a node not below the node count, whatever the world holds. Both read only that source's entries and allocate nothing, so a caller that needs one source pays for that source alone, not for every source the field holds."

src/sim/routes/SPEC.md, in the opening paragraph, "so other modules read it through sampleField (principle 6)" becomes "so other modules read it through sampleField and routeEntryAt (principle 6)", and a new paragraph after the one beginning "At a place strictly inside an edge, sampleEdge gives":

"routeEntryAt(world, kind, network, place, source) gives the source's entry in the kind's route distance field at the place on the network: exactly the entry sampleField of that field gives the source there, or none when it gives none, as at a place that does not resolve. It reads only the source's own entries, at a node through the medium's sourceEntryAtNode and inside an edge by sampleEdge's rule over the source's entries at the edge's ends, and allocates nothing, so a mover following one source pays for that source alone."

src/sim/guests/SPEC.md, in the opening paragraph, "guest route distance and the food offer through sampleField" becomes "guest route distance through sampleField and routeEntryAt, the food offer through sourceEntryAtNode", and appended to the paragraph beginning "So a guest decides at every node it leaves":

"Between choices a guest reads from R only its Target's entry or the entrances' entries, through routeEntryAt, and samples R whole only when it chooses."

## Files affected

- Modify: src/sim/medium/field.h, src/sim/medium/network.h, src/sim/medium/network.cpp, src/sim/medium/SPEC.md
- Modify: src/sim/routes/route_distance.h, src/sim/routes/route_distance.cpp, src/sim/routes/SPEC.md
- Modify: src/sim/guests/guests.cpp, src/sim/guests/SPEC.md
- Tests (test pass): tests/sim/medium/source_sample_test.cpp and tests/sim/routes/route_entry_test.cpp (new), registered in tests/sim/CMakeLists.txt

## Dependencies

- budgeted-report, for the report and the milestone's before report, build/runtime-report/indexed-sampling-before.txt.
- affordable-sampling's stop places.

## Out of scope

- Indexing slots by place, and reading route distance less often per walk: the milestone's candidates, gated on the profile after this feature.
- Allocations a guest makes at a node apart from route distance: the steps out of a node, a wander's weights, and a choice's options and sample, once per node a guest leaves.
- Shops' share of the tick, findEntity and parkBoxes: affordable for now, and measured by the report.

## Open questions

None.
