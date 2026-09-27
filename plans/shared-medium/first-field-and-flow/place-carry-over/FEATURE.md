# Feature: Place Carry-Over

## Summary

place-carry-over adds the medium's one carry-over operation, which a holder applies to its own places across a re-derivation, given the networks from before and after. A place on a carrier whose points are unchanged keeps its distance, so new nodes splitting its edge leave it where it was. A place on a carrier whose points changed moves to the point of the same carrier nearest its old ground position. A place on a carrier the new network lacks, or one that did not resolve before, carries over to no place. It also adds nearestPlaceOn, the nearest-place query restricted to one carrier, which the changed case uses. Both are pure functions of their arguments and live beside the network type in src/sim/medium.

## Acceptance criteria

1. carryOver gives no place when the place does not resolve in the network before, or when the network after has no carrier with its key.
2. When the place resolves in the network before and its carrier's points are equal in both networks, carryOver gives the place unchanged, whatever else differs between the networks. So a place held while a new node splits its edge names the same ground position afterwards.
3. When the place resolves in the network before and its carrier's points differ, carryOver gives nearestPlaceOn, in the network after, of its carrier and its ground point in the network before. The result resolves in the network after, lies on the same carrier, and no point of that carrier there is nearer the old ground position by more than 1e-9.
4. nearestPlaceOn gives no place for a carrier not in the network or a ground position that is not finite. Otherwise it gives a place on that carrier that resolves, no point of that carrier is nearer the position than its ground point by more than 1e-9, and among equally near places it gives the lowest distance. nearestPlace's result, when there is one, equals nearestPlaceOn of its own carrier and the same position.
5. carryOver depends only on the place and the two networks' contents: networks built from the same carriers and anchors given in different orders carry a place to the same result.

## Medium

This feature introduces no fields or flows. It provides the carry-over that believable-guests applies to each guest's place across a re-derivation, with navigable-networks keeping the previous networks available for it. Any later holder of places carries its own the same way.

## Principle checks

- Principle 2: criterion 1. A place whose carrier was removed, or that no longer resolved, carries over to no place, which is a legitimate state its holder handles, not an error.
- Principle 4: criteria 2 and 3. A kept distance is the producer's own, and the moved case uses the medium's one straight-line measure only to snap onto the place's own carrier, never to jump to another.
- Principle 10: criterion 5, and the tie rule of criterion 4.
- Principle 6: each holder carries its own places through carryOver, which reads the networks only through their public functions. The header shows this, and no test can check it without naming private members.

## Spec changes

src/sim/medium/SPEC.md: after the paragraph beginning "nearestPlace gives the place whose ground position is nearest a given one", add:

"nearestPlaceOn gives the place on one carrier whose ground position is nearest a given one, by the same projection, with ties going to the lower distance. It gives none when the carrier is not in the network or the position is not finite. nearestPlace's result is nearestPlaceOn of its own carrier."

Before the paragraph beginning "addNetworkComponent registers Network", add a section:

"## Carry-over

carryOver moves a place held across a re-derivation, given the network before and the network after. Each holder applies it to its own places. A carrier's geometry is its points, and its stops are not part of it, so adding, moving, or removing nodes along a carrier leaves its geometry unchanged.

- A place that does not resolve in the network before, or whose carrier the network after lacks, carries over to none. It is retired, a legitimate state its holder handles (principle 2).
- A place whose carrier's points are equal in both networks carries over unchanged, so a node that splits its edge leaves it at the same ground position.
- Otherwise its carrier's geometry changed, and it carries over to nearestPlaceOn, in the network after, of its carrier and its ground point in the network before. It stays on its own carrier and keeps its ground position as nearly as the new line allows."

## Files affected

- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/medium/network.h
- Modify: src/sim/medium/network.cpp
- Test pass: files under tests/, written by the test-writer agent: tests/sim/medium/carry_over_test.cpp, with additions to tests/sim/support/synthetic_network.h as needed and to tests/sim/CMakeLists.txt, whose tests join tpj_sim_tests.

## Dependencies

- networks-and-places: the network type, places, groundPoint, nearestPlace, and the synthetic network builder. Merged.

## Out of scope

- Deciding when carry-over runs, and keeping the previous networks: navigable-networks and believable-guests.
- Per-field relocation policies, such as keeping the distance on a changed carrier: a milestone deepening candidate.
- Carrying a place retired with its carrier onto another carrier.
- Carrying many places at once faster than one at a time: profiling can add it.

## Open questions

None.
