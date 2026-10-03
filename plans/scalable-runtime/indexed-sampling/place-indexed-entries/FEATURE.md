# Feature: Place-Indexed Entries

## Summary

Sampling a field stops scanning every entry of every source. Each field slot keeps an index of its entries ordered by place, built the first time the slot is sampled and kept beside the entries for as long as the slot lives, and a sample finds a source's entries at a node, at a place, or strictly inside an edge by binary searches in it. What a sample returns, and in what order, does not change. The index is never saved, hashed, or compared. The feature ends with the runtime report's comparison against the milestone's before report, which should show the full park's tick falling below 33 ms on windows-release.

## Acceptance criteria

1. orderByPlace of a list of entries gives the positions of the entries whose distance is not NaN, each once, ordered by carrier key, then distance, then position. (test)
2. For every slot a world holds, entriesByPlace of the slot equals orderByPlace of its entries, whatever the world has been through since the slot was last sampled: resolutions after commands, steps, swaps, copies, and loads. (test)
3. sampleField, sampleResolvedField, and fieldValue give what src/sim/medium/SPEC.md defines, in the same order, for entries at a node with several stop places, several entries of one source at one place, entries strictly inside an edge, entries at an edge's ends, and entries whose distance is NaN or a signed zero. (test)
4. A world whose slots have been sampled saves to the same text, hashes the same, and is worldsEqual to the same world never sampled. (test)
5. Every existing test passes unchanged, and tpj_scenarios's windows-debug output over the registered scenarios and tests/parks/*.park is identical, line for line, before and after the feature, by tpj_scenarios --compare (decision 0030). (check)
6. scripts/runtime-report.sh's comparison of the milestone's before report with a report after the feature shows the full park's windows-release ticks stage faster, marked clear, with a median below 33,333 µs in the after report's show output. (check)

## Medium

None changed. Route distance, the food offer, and hungry footfall are published and sampled exactly as before. The index is internal to the medium's field storage; orderByPlace and entriesByPlace are public only so its invariant can be tested, and no other module reads them.

## Principle checks

- Principle 1: the index is derived state with one owner, the slot that holds the entries it orders, and one way of being made, from those entries; criterion 2 checks the kept index equals a fresh one after every way a world changes.
- Principle 10: the index changes no result and no order (criterion 3), and nothing it holds reaches saves, hashes, or comparisons (criterion 4), so runs and builds stay identical (criterion 5).
- Principles 3 and 6: no field's producers or samplers change, and no module reads another's components.

## Spec changes

src/sim/medium/SPEC.md, replacing the paragraph beginning "A sample resolves only its own place.":

"A sample resolves only its own place, and finds each source's entries there through the source's slot's index by place, never by scanning them. orderByPlace(entries) gives the positions of the entries whose distance is not NaN, each once, ordered by carrier key, then distance, then position. A slot keeps such an order of its entries, entriesByPlace(slot), made the first time the slot is sampled and kept for as long as the slot holds those entries. Publishing, swapping, settling, copying, and loading make or remove whole slots, and nothing changes a slot's entries in place, so a kept order always equals orderByPlace of the slot's entries. The order is derived: the walk does not visit it, so saves, hashes, and worldsEqual never see it. A sample at a node finds, for each of the node's stop places, the entries at exactly that place; inside an edge, a field with sampleEdge finds those on the edge's carrier strictly between its two stop distances, and those at each end's stop places, and a field without it those at the sampled place. The positions found are sorted and made unique, so the entries come out in the source's order, once each. An entry whose distance is NaN is at no place and inside no edge, so it is never sampled, as resolving it would give. A sample therefore costs a few binary searches per source, and not a pass over its entries."

## Files affected

- Modify: src/sim/medium/field.h, src/sim/medium/SPEC.md
- Tests (test pass): tests/sim/medium/field_index_test.cpp (new), registered in tests/sim/CMakeLists.txt or the medium's test list

## Dependencies

- budgeted-report, for the report and the milestone's before report, build/runtime-report/indexed-sampling-before.txt.
- affordable-sampling's stop places.

## Out of scope

- Sampling only a guest's target between nodes, and sampling less often per walk: the milestone's candidates, gated on the after report.
- Removing sampling's allocations: scalable-runtime's candidate.
- Building the index off the sampling thread, or guarding it for concurrent readers: candidates off the frame's thread would need it.

## Open questions

None.
