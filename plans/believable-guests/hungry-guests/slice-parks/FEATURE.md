# Feature: Slice Parks

## Summary

The boxes-and-tubes slice's three park files are checked in: tests/parks/fed.park, an entrance, guest paths, one shop, one depot, and the backstage path between them, with no guests yet; warm.park, fed.park after a warm-up of WARM_TICKS; and cut.park, warm.park one cycle later with its backstage path deleted. The scenarios library's makeSliceParks makes all three from fed.park, and tpj_scenarios --slice-parks writes them, so they regenerate whenever the simulation's rules change. cut.park is made by a real DeletePath in a cycle, not by editing text, so the resolution clears the shop's stepped offer and the saved park reports no meals at once. Integration tests in tests/integration/ check the slice's criteria 1, 2, 3, 6, and 7 on the parks, each over the shortest run that shows it, and the cross-build check runs the parks 3000 ticks on both builds. SLICE.md's run lengths change to match, closing its open question.

## Acceptance criteria

Throughout, F, W, and C are the texts of tests/parks/fed.park, warm.park, and cut.park, and to open a text is to load it with makeParkSchema and resolveWorld it. A park's shop is its one shop box, and the shop's offer is the entries of source the shop that sampleField of FoodOffer gives on parkNetwork(world, PathKind::Guest) at the nodePlace of the shop's guest anchor. A guest waits at the shop when its guestRecord's Activity is Waiting and its Target is the shop.

1. makeSliceParks(text, warmTicks) gives three saves: Fed, saveWorld of the opened text; Warm, saveWorld of that world after warmTicks stepWorld cycles with no commands; and Cut, saveWorld of that world after one more cycle with a DeletePath queued for each backstage path parkPaths gives, in ascending key order. It throws LoadError for a text loadWorld refuses.
2. tpj_scenarios --slice-parks FED WARM CUT writes the Fed, Warm, and Cut of makeSliceParks, given FED's text and WARM_TICKS, to the files FED, WARM, and CUT, byte for byte with line feeds, and exits with status 0. It exits with a nonzero status and a message on standard error, writing no file, when it is not given three files or cannot read or load FED, and with a nonzero status and a message naming the file when it cannot write one.
3. F, W, and C are the Fed, Warm, and Cut of makeSliceParks(F, WARM_TICKS). Opened, F is at tick 0 with one entrance, one shop, one depot, one backstage path, at least one guest path, and no guests; and C has no backstage path, at least one guest waiting at the shop, and at least one supplies packet in transit.
4. In every cycle of opened F stepped 1800 ticks, every guest whose record has a LastMeal has its After below its Before, and by the end at least one meal has been consumed as eaten.
5. In every cycle of each of opened F, W, and C stepped 300 ticks, the supplies and meals kinds each have units created equal to units in transit plus units held plus units consumed.
6. Opened C and opened W each stepped 600 ticks: C's shop's offer has Supplied false in the opened world and after every cycle; no guest's LastChoice made after C's starting tick picks the shop's offer; every guest waiting at the shop in opened C has, after the last cycle, left the park or stopped waiting at it; and the mean Hunger of C's guests after the last cycle is higher than W's.
7. For each of F, W, and C, saveWorld of the opened text is the text, and opening that save gives a world equal to the opened text.
8. Two runs of opened F stepped 600 ticks have equal hashWorld after every cycle, and the cross-build check passes with the three parks in tests/parks/.

## Medium

The feature adds no field or flow. Its tests read, through the public interface, what the slice's members already publish:

- Food offer (plausible-operations): the shop's offer, sampled at its guest anchor.
- Supplies and meals (plausible-operations and believable-guests): the ledgers' totals and packets, through the medium's flow queries.
- Inspection records (believable-guests): each guest's Activity, Target, Hunger, LastMeal, and LastChoice, through parkGuests and guestRecord.
- Park intent and edits (effortless-building): parkPaths and parkBoxes, and DeletePath, which makeSliceParks queues as the player's delete tool would.

## Principle checks

- Principle 1: each park's save loads back to the world saved, and saving again gives the same file, shipments in transit included (criteria 3 and 7).
- Principle 2: cutting the supply route leaves a legitimate park that keeps running: the shop reports no meals, and its queue empties (criterion 6).
- Principle 3: supplies and meals are conserved in every cycle of every park (criterion 5).
- Principle 5: hunger rises without a gate where no meals are offered, so cut.park's guests end hungrier (criterion 6).
- Principle 8: the cut reaches guests through the offer at once, from cut.park's first tick (criterion 6).
- Principle 10: the parks step identically in two runs and on both builds (criterion 8).

## Spec changes

- src/scenarios/SPEC.md: a paragraph after the tpj_scenarios --compare paragraph defining makeSliceParks, WARM_TICKS, and tpj_scenarios --slice-parks. The cross-build paragraph is unchanged, since it already runs every tests/parks/*.park file.
- plans/slices/boxes-and-tubes/SLICE.md: criteria 1, 2, 3, and 7 state the run lengths of criteria 4, 5, 6, and 8 above, and criterion 7 names the cross-build check's 3000-tick runs. The park files' paragraph names tpj_scenarios --slice-parks, which regenerates them. The open question on run lengths is removed, resolved by this feature's research.

The exact text is in PLAN.md, Tasks 1 and 2.

## Files affected

- Modify: src/scenarios/SPEC.md
- Modify: plans/slices/boxes-and-tubes/SLICE.md
- Create: src/scenarios/slice_parks.h
- Create: src/scenarios/slice_parks.cpp
- Modify: src/scenarios/CMakeLists.txt
- Modify: src/scenarios/main.cpp
- Create: tests/parks/fed.park, tests/parks/warm.park, tests/parks/cut.park
- Tests, from the test pass: tests/integration/ and tests/scenarios/

## Dependencies

- eating-guests, carried-guests, and hungry-footfall, merged: guests that eat, carry across edits, and publish footfall.
- stop-matched-sampling, merged: stepping fast enough for these runs.
- plausible-operations' supplied-food-shop and effortless-building's DeletePath.

## Out of scope

- Slice criteria 4 and 5, food availability and the placement preview: legible-simulation's explained-food, and the slice's closing.
- Slice criteria 8 and 9, the captures with the food overlay: legible-simulation adds --overlay.
- A check that CI steps as fast as this machine: deterministic-simulation's stepping budget candidate.

## Open questions

None.
