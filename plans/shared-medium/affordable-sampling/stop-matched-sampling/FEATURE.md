# Feature: Stop-Matched Sampling

## Summary

A Network gives each node's stop places, the places where carriers stop at it, built once by its constructor. Sampling a field at a node, or strictly inside an edge for a field with sampleEdge, then finds each entry's position by comparing its place with the stop places of the node or of the edge's two nodes, and with the edge's carrier and stop distances, instead of resolving it. An entry resolves to a node exactly when its place is one of the node's stop places, and strictly inside an edge exactly when it is on the edge's carrier strictly between its stops, so every sample gives what it gave before, in the same order, and only the sampled place is resolved. A park with guests, whose every walking guest samples route distance every tick, steps much faster, and every hash stays the same.

## Acceptance criteria

Throughout, a network is any Network the constructor accepts, including the random synthetic networks the medium's tests build.

1. For every node n of a network, stopPlaces(n) lists places in ascending carrier key and then distance, with no repeats, and a place is among them exactly when resolve gives NodePosition n for it. stopPlaces throws std::out_of_range for a node not below nodeCount().
2. For every node n of a network, nodePlace(n) equals the first of stopPlaces(n).
3. For any network, field entries, and place, sampleField, sampleResolvedField, and fieldValue give exactly what src/sim/medium/SPEC.md's Fields section defines through resolve, for fields with and without sampleEdge, including entries at nodes, strictly inside edges, on carriers absent from the network, and at distances that resolve nowhere.
4. Every existing test passes unchanged, and the cross-build check's linux-debug output, build/cross-build-check/linux-debug.txt, is identical, byte for byte, to its output before the feature.
5. In a perf profile of tests/parks/supply.park stepped 3000 ticks on linux-debug, Network::resolve's inclusive share of the time is under 5%, and on windows-debug those ticks step at least 1.5 times as fast as before the feature, with the same final hash. The feature's report gives the windows-debug and linux-debug times before and after.

## Medium

The feature changes how the medium answers samples, not what any field or flow carries. Route distance (navigable-networks), food offer (plausible-operations), and hungry footfall (believable-guests) are published and sampled exactly as before. The Network type gains one public query, stopPlaces, which any holder of a network may read.

## Principle checks

- Principle 2: an entry whose place does not resolve, on an absent carrier or at a distance outside its carrier, is still never sampled (criterion 3).
- Principle 3 and 8: every sampled entry keeps its source, in ascending source order and each source's order (criterion 3).
- Principle 4: sampling compares places along carriers only; nothing new measures a straight line (criterion 3).
- Principle 10: every scenario's and park's hashes are unchanged on both builds (criterion 4).

## Spec changes

- src/sim/medium/SPEC.md, Networks: after the paragraph on nodePlace, nodeAnchor, and anchoredNodes, a paragraph defining stopPlaces: each stop's place at a node, in ascending carrier key and then distance, which are exactly the places resolve gives the node for, with nodePlace the first of them, and std::out_of_range for a node not below the node count.
- src/sim/medium/SPEC.md, Fields: after the paragraph on sampleField, a sentence saying that a sample finds its entries by comparing their places with the stop places of the node or of the edge's two nodes and with the edge's carrier and stop distances, so a sample resolves only its own place, and its cost grows with the entries it scans, not with resolving them.

The exact text is in PLAN.md, Task 2.

## Files affected

- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/medium/network.h
- Modify: src/sim/medium/network.cpp
- Modify: src/sim/medium/field.h
- Tests, from the test pass: tests/sim/medium/

## Dependencies

- first-field-and-flow, merged: the Network type, fields, and the sampling rules.

## Out of scope

- Entries indexed by place, so a sample visits only the entries at its node or edge: the milestone's deepening candidate.
- Listing shops and depots once per cycle instead of rebuilding the park's boxes for every shop: supplied-food-shop's deepening candidate.
- A check that fails when stepping slows past a budget: deterministic-simulation's deepening candidate.

## Open questions

None.
