# Milestone: Affordable Sampling

Slice: none

## Summary

affordable-sampling makes sampling a field cheap enough that a park with guests steps quickly. A profile of tests/parks/supply.park stepped 3000 ticks shows the medium's sampling inside an edge taking 58% of the time, because it resolves every entry of every source on the network to find the few at the sampled edge. The network already knows where its carriers stop, so the milestone has it keep, for each node, the places where carriers stop there, and has sampling compare each entry's place with those and with the edge's bounds instead of resolving it. What a sample returns does not change. It comes now because believable-guests' slice-parks needs fed.park, warm.park, and cut.park stepped thousands of ticks in tests that must stay fast, and the boxes-and-tubes slice left its run lengths to this measurement.

## Acceptance criteria

1. A Network gives, for each node, the places where carriers stop at it, in ascending carrier key and then distance, and a place is among a node's stop places exactly when resolve gives that node for it.
2. sampleField, sampleResolvedField, and fieldValue give exactly what the medium's spec defines, for every field, network, and place. Every existing medium, routes, operations, and guests test passes unchanged, and the cross-build check's output is identical, line for line, to its output before the milestone.
3. A sample resolves only the sampled place, never its entries' places. In perf profiles of tests/parks/supply.park stepped 3000 ticks on linux-debug, Network::resolve's inclusive time, its share times the run's time, falls at least fivefold, and on windows-debug those ticks step at least 1.5 times as fast as before. The feature's report gives the windows-debug and linux-debug times before and after.
4. src/sim/medium/SPEC.md describes the stop places and how sampling uses them. linux-debug and windows-debug build without warnings, their tests pass, and scripts/tidy.sh is clean.

## Medium

The milestone changes how the medium answers samples, not what any field or flow carries. Every field's producers and samplers are unchanged: navigable-networks' route distance, plausible-operations' food offer, and believable-guests' hungry footfall are sampled exactly as before, and faster. The network type gains one query, its nodes' stop places, which any holder of a network may read.

## Dependencies

- first-field-and-flow: the network type and the field interface. Met.

## Core feature

stop-matched-sampling, the milestone's one feature. It removes the cost the profile found and changes no result, so it is useful as soon as it lands: every test and scenario with guests steps faster.

## Features

1. `stop-matched-sampling`: the network's stop places per node, and sampling at a node or inside an edge by comparing each entry's place with them and with the edge's bounds instead of resolving it. Depends on: none.

## Deepening candidates

Unordered pool this milestone draws later features from.

## Open questions

None.

## Research notes

- A profile of supply.park puts 58% of stepping in sampleSlotInEdge and 50% in Network::resolve, which it calls on every entry of every source for every guest's sample each tick.
- An entry resolves to a node exactly when its place equals one of the node's stop places, and inside an edge exactly when it is on the edge's carrier strictly between its stops, so sampling can compare places instead of resolving them.

Depth is in RESEARCH.md.
