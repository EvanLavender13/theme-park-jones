# Feature: Place-Indexed Entries

## Summary

Sampling a field stops scanning every entry of every source. Each field slot keeps an index of its entries ordered by place, made the first time the slot is sampled and kept beside the entries for as long as the slot lives, and a sample, and sourceEntryAtNode, find a source's entries at a node, at a place, or strictly inside an edge by binary searches in it. A sample reuses its working lists across its sources rather than making them for each. What a sample returns, and in what order, does not change. The index is never saved, hashed, or compared. The feature ends with the runtime report's comparison against a report made just before it, which should show the full park's food overlay build several times faster.

## Acceptance criteria

1. orderByPlace of a list of entries gives the positions of the entries whose distance is not NaN, each once, ordered by carrier key, then distance, then position. (test)
2. For every slot a world holds, entriesByPlace of the slot equals orderByPlace of its entries, whatever the world has been through since the slot was last sampled: resolutions after commands, steps, swaps, copies, and loads. (test)
3. sampleField, sampleResolvedField, fieldValue, and sourceEntryAtNode give what src/sim/medium/SPEC.md defines, in the same order, for entries at a node with several stop places, several entries of one source at one place, entries strictly inside an edge, entries at an edge's ends, and entries whose distance is NaN or a signed zero. (test)
4. A world whose slots have been sampled saves to the same text, hashes the same, and is worldsEqual to the same world never sampled. (test)
5. Every existing test passes unchanged, and tpj_scenarios's windows-debug output over the registered scenarios and tests/parks/*.park is identical, line for line, before and after the feature, by tpj_scenarios --compare (decision 0030). (check)
6. scripts/runtime-report.sh's comparison of a report made before the feature with one made after shows the full park's windows-release food-overlay stage faster, marked clear, and its ticks stage not slower, marked clear. (check)

## Medium

None changed. Route distance, the food offer, and hungry footfall are published and sampled exactly as before. The index is internal to the medium's field storage; orderByPlace and entriesByPlace are public only so its invariant can be tested, and no other module reads them.

## Principle checks

- Principle 1: the index is derived state with one owner, the slot that holds the entries it orders, and one way of being made, from those entries; criterion 2 checks the kept index equals a fresh one after every way a world changes.
- Principle 10: the index changes no result and no order (criterion 3), and nothing it holds reaches saves, hashes, or comparisons (criterion 4), so runs and builds stay identical (criterion 5).
- Principles 3 and 6: no field's producers or samplers change, and no module reads another's components.

## Spec changes

src/sim/medium/SPEC.md, replacing the paragraph beginning "A sample resolves only its own place.":

"A sample resolves only its own place, and finds each source's entries there through the source's slot's index by place, never by scanning them. orderByPlace(entries) gives the positions of the entries whose distance is not NaN, each once, ordered by carrier key, then distance, then position, with distances compared as numbers, so 0.0 and -0.0 tie. A slot keeps such an order of its entries, entriesByPlace(slot), made the first time the slot is sampled and kept for as long as the slot holds those entries. Publishing, swapping, settling, copying, and loading make or remove whole slots, and nothing changes a slot's entries in place, so a kept order always equals orderByPlace of the slot's entries. The order is derived: the walk does not visit it, so saves, hashes, and worldsEqual never see it. A sample at a node finds, for each of the node's stop places, the entries at exactly that place; inside an edge, a field with sampleEdge finds those on the edge's carrier strictly between its two stop distances, and those at each end's stop places, and a field without it those at the sampled place. The positions found are put in ascending order, once each, so the entries come out in the source's order. An entry whose distance is NaN is at no place and inside no edge, so it is never sampled, as resolving it would give. A sample therefore costs a few binary searches per source, not a pass over its entries, and it reuses its working lists across its sources. sourceEntryAtNode finds its entry through the same order."

src/sim/medium/SPEC.md, in the paragraph beginning "sourceSlot(world, source) gives", "Both read only that source's entries and allocate nothing" becomes "Both read only that source's entries and allocate nothing, except that sourceEntryAtNode makes the slot's order by place when it is the slot's first reader".

src/sim/routes/SPEC.md, in the paragraph beginning "routeEntryAt(world, kind, network, place, source) gives", "and allocates nothing" becomes "and allocates nothing, except the slot's order by place when it is the slot's first reader".

## Files affected

- Modify: src/sim/medium/field.h, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, src/sim/routes/route_distance.h
- Tests (test pass): tests/sim/medium/field_index_test.cpp (new), registered in tests/sim/CMakeLists.txt

## Dependencies

- targeted-route-samples, whose sourceEntryAtNode this feature makes find its entry through the index.
- budgeted-report, for the report.
- affordable-sampling's stop places.

## Out of scope

- routeEntryAt's lookup inside an edge, which scans one source's entries per walking guest per tick, about 1.3 ms of a full-park tick: a candidate, gated on the report.
- The vectors sampleEdge returns for each source, and the sample's own result: the capability's sampling-without-allocation candidate.
- Making the index off the sampling thread, or guarding it for concurrent readers: candidates off the frame's thread would need it.

## Open questions

None.
