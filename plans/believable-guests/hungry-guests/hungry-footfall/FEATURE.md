# Feature: Hungry Footfall

## Summary

Guests publish hungry footfall, a scalar field on the guest network's stretches, its edges. Each tick, after guests step, a guests system moves each stretch's value 1/FOOTFALL_TIME of the way toward the summed hunger of the guests standing on it, with no threshold, so a stretch's value counts guests weighted by hunger and settles over about ten seconds. The values are guests' private state, each held at its stretch's midpoint, and the system publishes them into the field as stepped entries, which a sampleEdge rule spreads over the whole stretch, with one entry at each node holding the mean of the stretches meeting it. A guests finisher carries the held midpoints across each resolution with carryOver, as guests' places are carried: an untouched stretch keeps its value, a split stretch's value goes to the half holding the old midpoint, joined stretches' values add, and a deleted path's values go with it. legible-simulation samples the field later; tests sample it now.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema that has stepped at least one cycle, the last applying no commands. For a world X, N(X) is parkNetwork(X, PathKind::Guest), a stretch of X is an edge of N(X), and v(X, E) is fieldValue of HungryFootfall on N(X) at a place strictly inside the stretch E. A place lies in a stretch as src/sim/guests/SPEC.md's Hungry footfall section defines, and a stretch D's midpoint is the Place of D's carrier at (D.FromDistance + D.ToDistance) / 2.

1. addGuests registers, after the component type guest, the component types that addField registers for HungryFootfall, named hungry-footfall, and then the state type footfall; after the system stepGuests, one system of hungry footfall; and after the finisher carryGuests, the finisher addField registers and then one finisher of hungry footfall. makeParkSchema's component types and systems end with those addGuests registers.
2. In W, fieldValue of HungryFootfall on N(W) is the same at every place strictly inside a stretch, and at a place that resolves to a node it is 0.0 with v(W, E) added for each end at that node of each stretch E, in N(W)'s edges() order and From before To, divided by the number of those ends as a double.
3. For any commands, let C be the candidate made from W with them, and X the world stepping C one cycle with no commands leaves. For every stretch E of X, v(X, E) is V + (S - V) / FOOTFALL_TIME, computed in doubles in that order with FOOTFALL_TIME converted to double, where S is 0.0 with the Hunger of each guest of X whose guestRecord's At lies in E added in ascending key order, and V is 0.0 with v(W, D) added for each stretch D of W, in N(W)'s edges() order, whose midpoint's carryOver from N(W) to N(C) lies in E.
4. In every cycle of randomized park edit sequences (tests/sim/support/route_edits.h) from a park with an entrance, shops, a depot, and guests walking, waiting, and eating, every value fieldValue of HungryFootfall gives inside a stretch is finite and not below 0.
5. The cross-build check passes, with the park-edits scenario's worlds now holding hungry footfall.

## Medium

- Hungry footfall (this feature produces): a scalar field, HungryFootfall, named hungry-footfall, on the guest network. Its one source is its own field entity, fieldKey("hungry-footfall"), and it publishes only stepped entries: one per stretch at the stretch's midpoint, then one per node at its nodePlace. legible-simulation samples it later for preview context, including where a shop ghost's connector would meet a path, which is often a node. Tests sample it here.
- Networks and carry-over (shared-medium, navigable-networks): the system reads parkNetwork(world, PathKind::Guest) and its edges(). The finisher reads previousNetwork and parkNetwork and carries the held midpoints with carryOver. Guests never clear routes' data.
- Guest places and hunger: state private to the guests module. The footfall system reads each guest's At and Hunger within the module. No other module reads them, and the footfall state is private to the module too.

## Principle checks

- Principle 1: the footfall is state, held once privately and once as the field's readable entries, and every world randomized edits with guests reach, now holding footfall, still loads back from its save and resolves equal to itself, as carried-guests' criterion 6 tests.
- Principle 2: under any edit, every value stays finite and not below 0, and a deleted path's values are dropped with it, a legitimate state (criteria 3 and 4).
- Principle 3: the footfall reaches other modules only as the published field (criteria 2 and 3).
- Principle 4: carrying uses carryOver alone, with no straight-line fallback for a retired midpoint (criterion 3).
- Principle 5: every guest's hunger counts toward its stretch, with no threshold (criterion 3).
- Principle 6: the footfall reads only the guests module's own state and the guest network through the medium's queries, and no other module reads the footfall state. Checked by review.
- Principle 8: a candidate's footfall is carried as committing its edit carries it (criterion 3 steps a candidate).
- Principle 10: guests are summed in ascending key order and held values in their held order, and the cross-build check passes (criteria 3 and 5).

## Spec changes

- src/sim/SPEC.md: addPark's order names the guests module's hungry-footfall field with its state, systems, and finishers.
- src/sim/guests/SPEC.md: the module paragraph adds that guests publish hungry footfall, and that a guest's hunger leaves the module only summed into it. Registration adds the field, the footfall state, the system stepFootfall, and the finisher carryFootfall. A new Hungry footfall section, before Inspection record, defines stretches, where a place lies, the average, the held midpoints, the node means, the sampleEdge rule, and carrying.

The exact text is in PLAN.md, Tasks 1 and 2.

## Files affected

- Modify: src/sim/SPEC.md
- Modify: src/sim/guests/SPEC.md
- Create: src/sim/guests/footfall.h
- Create: src/sim/guests/footfall.cpp
- Create: src/sim/guests/internal/footfall.h
- Modify: src/sim/guests/guests.h
- Modify: src/sim/guests/guests.cpp
- Modify: src/sim/CMakeLists.txt
- Tests, from the test pass: tests/sim/guests/

## Dependencies

- carried-guests, merged: previousNetwork, the guests finisher, and the routes finisher that drops the previous networks after every finisher that reads them.
- shared-medium's scalar fields, sampleEdge, publishStepped, and carryOver.
- navigable-networks' parkNetwork.

## Out of scope

- Splitting a stretch's value between the halves of a split stretch in proportion to length. The whole value goes to the half holding the old midpoint and the other starts at 0; both settle within a few FOOTFALL_TIMEs.
- Drawing hungry footfall, and the ghost's footfall context: legible-simulation.
- Crowding: navigable-networks' deepening candidate for edge costs rising with footfall.

## Superseded tests

One existing test asserts a registration this feature extends, and the test pass rewrites it to criterion 1:

- tests/sim/guests/arrivals_test.cpp, "makeParkSchema registers the state type guest and the guests' system after everything else addPark registers": addGuests now registers four component types and two systems, and makeParkSchema's lists end with footfall's.

## Test pass decisions

- Criterion 5 is checked by scripts/cross-build-check.sh, which runs the park-edits scenario with makeParkSchema, not by a Catch2 test.
- Principle 6, that no other module reads the footfall state, is checked by review and by the private header check on guests/internal/footfall.h.
- Two randomized tests are slimmed to stay well under 1 s: operations_edits_test's save round-trip with synthetic guests runs 20 edits instead of 60, still saving the tick-0 world and comparing through every warm-up cycle, and guest_edits_test's footfall bounds test runs 12 edits.
- Principles 1 and 8 need no new tests: the randomized guest edit tests of carried-guests' criterion 6 already compare saves and candidates of worlds made with makeParkSchema, which now hold footfall.

## Open questions

None.
