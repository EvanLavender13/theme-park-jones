# Feature: Footfall on Kept Entries

## Summary

Hungry footfall moves from the stepped layer to the kept layer kept-entries added, so a tick's footfall work grows with the guests rather than the guest network (decision 0031). HungryFootfall becomes a kept field. Its readKept decays a value by value * simExp(k * simLog(1 - 1/FOOTFALL_TIME)) after k ticks, and its sampleNode gives a node the mean of the stretches meeting it, worked out when read. Each tick, stepFootfall takes each guest once, sums the hunger on each stretch some guest stands in, and changes only those stretches' kept entries, each at its midpoint, by today's step from its value read at the world's tick. It visits no other stretch and publishes nothing. carryFootfall carries the kept entries across a resolution by carryOver. A stretch one entry is carried into keeps that entry's value and tick at its midpoint. A stretch several are carried into holds their values, read at the world's tick, added. The private Footfall state goes. fed.park is edited to the new saved form by hand. warm.park and cut.park are remade by tpj_scenarios --slice-parks, and full.park by tpj_bench --full-park. winding-path.park is converted by a one-off edit that keeps its values. The runtime report is compared before and after. Values differ from today's only after a stretch sits empty, and then only in their last bits. Nothing in the simulation reads footfall, so no guest or shop acts differently.

## Acceptance criteria

1. After a cycle with no commands, the stretches whose kept entry has the world's tick are exactly the stretches some guest's place lies in. (test)
2. Hungry footfall holds at most one kept entry in each stretch of the guest network, at the stretch's midpoint. (test)
3. A stretch some guest lies in after a cycle holds V + (S - V) / FOOTFALL_TIME, where V is its value read before the cycle and S is the summed hunger of its guests. (test)
4. A kept entry of hungry footfall reads, k ticks after its tick, as its value times simExp(k * simLog(1 - 1/FOOTFALL_TIME)). (test)
5. fieldValue at a node is the mean, over the node's edge ends, of the values of the stretches those ends belong to. (test)
6. After a resolution, each stretch's value is the sum of the values, read at the world's tick, of the entries carryOver brings into it from the network before. (test)
7. A stretch that exactly one entry is carried into holds that entry's value and tick. (test)
8. The remade warm.park, cut.park, and full.park differ from the ones before only in hungry footfall's sections, and the converted winding-path.park loads and holds every stretch value it held before. Run the commands in PLAN.md's Tasks 10 to 12. Each filtered diff prints 0. (check)
9. On every stress park, the after runtime report's ticks median is not slower than the before report's, beyond the spread both reports measured, as `scripts/runtime-report.sh --compare` shows. (check)
10. windows-debug builds without warnings, ctest.exe --preset windows-debug passes, and scripts/tidy.sh is clean. Every existing test passes unchanged except tests/sim/guests/footfall_test.cpp and the registration test in tests/sim/guests/arrivals_test.cpp, which the test pass rewrites from the new spec. (check)

## Medium

Hungry footfall, the field named hungry-footfall on the guest network, keeps its name, its meaning, and its one reader, legible's shopContext, which calls fieldValue as before. The field's entity, fieldKey("hungry-footfall"), is still its source. It now holds kept entries, which stepFootfall changes with keepEntry and carryFootfall replaces with replaceKeptEntries (src/sim/medium/SPEC.md, Kept fields). Route distance, the food offer, and every flow are untouched. Guests still read their own state and the medium, and no other module reads a guest's state.

## Principle checks

- Principle 1: footfall's kept entries are state, so a world's save, loaded and resolved, equals the world. The existing test "Every world randomized park edits with guests walking, waiting, and eating reach equals its save loaded and resolved" covers it.
- Principle 2: an edit carries footfall's places by carryOver, so a deleted path's values go and an untouched stretch keeps its entry (criteria 6 and 7).
- Principle 5: every guest's hunger counts, with no threshold (criterion 3).
- Principle 8: a candidate's footfall is carried as committing its edit carries it. The existing test "A candidate made with an edit from a world of randomized park edits ..., once it has stepped a cycle, equals the world that queues the edit for that cycle" covers it.
- Principle 10: the decay uses simExp and simLog, the same on every build (criterion 4). stepFootfall reads only values made readable before its tick, so system order changes nothing (kept-entries' criterion 2).

## Spec changes

src/sim/guests/SPEC.md, in the opening paragraph, replacing "and no other module reads a guest's or the module's footfall state, so a guest's hunger leaves the module only summed into the hungry-footfall field it publishes (principles 3 and 6)." with:

"and no other module reads a guest's state, so a guest's hunger leaves the module only summed into the hungry-footfall field it keeps (principles 3 and 6)."

src/sim/guests/SPEC.md, Registration, replacing "the field HungryFootfall by addField, the state component type Footfall, named footfall, the system stepFootfall, and the finisher carryFootfall. So guests step before their footfall is averaged, and are carried before it. Footfall is private to the module, declared in guests/internal/footfall.h." with:

"the kept field HungryFootfall by addKeptField, the system stepFootfall, and the finisher carryFootfall. So guests step before their footfall is averaged, and are carried before it."

src/sim/guests/SPEC.md, Hungry footfall, replacing the section's first sentence, "Guests publish hungry footfall, a scalar field named hungry-footfall on N, defined by HungryFootfall in guests/footfall.h.", with:

"Guests keep hungry footfall, a kept scalar field (sim/medium/SPEC.md, Kept fields) named hungry-footfall on N, defined by HungryFootfall in guests/footfall.h."

and appending to that first paragraph:

"A stretch's midpoint is the place of its carrier at (FromDistance + ToDistance) / 2, which is strictly inside it."

src/sim/guests/SPEC.md, Hungry footfall, replacing the paragraphs beginning "Each stretch's value is an exponential moving average", "HungryFootfall's sampleEdge gives", and "A resolution re-derives N under the held entries" with:

"Each stretch's value is an exponential moving average, with time constant FOOTFALL_TIME, 300 ticks, of the summed hunger of the guests on it. The field's entity, fieldKey("hungry-footfall"), is the source of its kept entries and holds at most one in each stretch, at its midpoint. A stretch with none has the value 0.0, and a stretch's value is its entry's read value. Each cycle, after stepGuests, stepFootfall takes each guest in ascending key order and finds the stretch its At lies in. Then, for each stretch some guest lies in, in edges() order, its hunger S is 0.0 with the Hunger of each such guest added in ascending key order. Its held value V is keptValue of the source at its midpoint, read at the world's tick, or 0.0 when it has none. Its new value is V + (S - V) / FOOTFALL_TIME, computed in that order with FOOTFALL_TIME converted to double, and stepFootfall keeps it at the midpoint with keepEntry. It visits no stretch no guest lies in and publishes nothing, so its work grows with the guests, not with N (decision 0031). Every guest's hunger counts, with no threshold (principle 5), so a stretch's value approaches the summed hunger of guests who stay on it, and ten hungry guests passing weigh ten times one.

A stretch no guest lies in keeps its entry unchanged, and its value decays as it is read. HungryFootfall's readKept(value, k) is value * simExp(k * simLog(1 - 1 / FOOTFALL_TIME)), with k and FOOTFALL_TIME converted to double and 1 / FOOTFALL_TIME computed first. Each step of V + (0 - V) / FOOTFALL_TIME multiplies V by 1 - 1 / FOOTFALL_TIME up to rounding, so the rule is k such steps up to rounding. It is the rule decision 0031 lets the field state in place of decaying every tick, and it uses only the simulation's own math, so it is the same on every build (decision 0022). readKept(value, 0) is value, as the formula gives, so a stretch with guests on it every tick takes exactly one step of the rule above each tick.

HungryFootfall's sampleEdge gives the read values of the source's entries strictly inside the sampled edge, in order, and ignores those at its nodes. Its sampleNode gives one value, the mean of the stretches meeting the node. It starts from 0.0, adds, for each of the sample's ends in order, that end's stretch value, and divides by the number of ends as a double. An end's stretch value is 0.0 with the read value of each of its entries strictly inside the edge added in order. It ignores entries at the node's stop places, which footfall never holds. Every node is the end of some stretch, so the mean is defined. So fieldValue anywhere strictly inside a stretch is its value, and at a node the mean of the stretches meeting it, with each end of a stretch at the node counted, so a path's end reads its one stretch and a junction the mean of those meeting it. A node's mean is worked out when read and never held.

A resolution re-derives N under the kept entries. The finisher carryFootfall takes the source's kept entries in held order and carries each to the place carryOver gives from previousNetwork(world, PathKind::Guest) to N. It drops an entry when that gives none, or a place in no stretch. It then replaces the source's kept entries, with replaceKeptEntries, by one entry for each stretch some carried entry lies in, in edges() order, at the stretch's midpoint. A stretch that one entry is carried into holds that entry's value and tick. A stretch that several are carried into holds, with the world's tick, 0.0 with the value of each read at the world's tick added in held order. So an untouched stretch keeps its entry exactly, a split stretch's value goes to the half holding the old midpoint while the other half starts at 0, joined stretches' values add, and a deleted path's values are dropped (principle 2). This work is over the source's entries, which decision 0031 allows in resolution and never in a tick. Carrying uses carryOver alone (principle 4), and a candidate's footfall is carried as committing its edit carries it (principle 8). A world's first resolution has no network before, so carrying changes nothing in it."

src/legible/SPEC.md, in the paragraph beginning "shopContext(world, candidate, edit) tells what a shop ghost would find", replacing "which a candidate, never stepped, holds only where the last step published it" with:

"which a candidate, never stepped, holds only as its resolution carried it"

## Files affected

- Modify: src/sim/guests/SPEC.md, src/legible/SPEC.md, src/sim/guests/footfall.h, src/sim/guests/footfall.cpp, src/sim/guests/internal/footfall.h
- Modify (data): tests/parks/fed.park, tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park, tests/parks/stress/winding-path.park
- Test pass: tests/sim/guests/footfall_test.cpp (rewrite), tests/sim/guests/arrivals_test.cpp (its registration test)

## Dependencies

- kept-entries: addKeptField, keepEntry, keptValue, keptEntries, replaceKeptEntries, readKeptEntry, NodeSample, and edgeEnds' order. Met (6429bff).
- simExp and simLog in sim/sim_math.h. Met.
- tpj_scenarios --slice-parks and tpj_bench --full-park, which remake the parks. Met.

## Out of scope

- Dropping entries that have decayed to nothing: at most one entry per stretch ever stood on, and an idle entry costs no tick work.
- Footfall for needs beyond hunger: plans/BACKLOG.md holds it.
- The food overlay's frame cost: a separate milestone, affordable-overlay, not yet planned.
- tpj_scenarios's output staying identical: footfall is in every hash, and the milestone states this exception.

## Spec decisions

- The decay is the simExp and simLog closed form, not repeated squaring (RESEARCH.md).
- A stretch one entry is carried into keeps that entry's tick, so an edit changes footfall's bits only where stretches join (RESEARCH.md).
- sampleNode ignores entries at a node's stop places, since footfall's entries are all at midpoints strictly inside stretches.
- The registration, the order of guests and footfall among systems and finishers, and the saved section's name are spec rules. The existing registration test is updated to them, and no new test lists them.

## Open questions

None.
