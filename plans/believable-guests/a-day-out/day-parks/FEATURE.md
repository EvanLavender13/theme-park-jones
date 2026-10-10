# Feature: Day Parks

## Summary

tpj_scenarios --slice-parks also makes the day-at-the-park slice's four parks: day.park, no-toilet.park, noon.park, and evening.park. day.park is a compact T of guest path with a food shop at one end, a drink shop at the other, a bench and a toilet block on the way in, and a depot behind on a backstage path. Every facility is within about an hour's walk of the entrance. no-toilet.park is day.park with its toilet block deleted. noon.park and evening.park are day.park run to 12:00 and 20:30. The park world checks then cover benches and toilet blocks in use, and gain three edits of noon.park. A scratch program runs the slice's comparisons on the made parks, and the user watches day.park through a day. If watching the day shows a tuning value wrong, the user names the new values, and this feature sets them and remakes every park that holds guests.

## Acceptance criteria

1. tpj_scenarios --slice-parks writes day.park, no-toilet.park, noon.park, and evening.park after the parks it writes today. day.park holds an entrance, a guest path, a food shop, a drink shop, a bench, a toilet block, a depot, and a backstage path, with the default hours, at 9:00 with no guests. no-toilet.park is day.park without the toilet block, with every other key the same. noon.park is day.park at 12:00 with guests in the park. evening.park is day.park at 20:30, so the cross-build check's run of 3,000 ticks crosses closing. (check: PLAN.md Task 6)
2. Every shop in day.park is supplied, and every shop, bench, and toilet block has a guest connector. Each shop is under an hour's walk from the entrance, about 70 m. (check: makeDayParks throws otherwise; play: open day.park and see the layout)
3. The park world checks cover a bench's or toilet block's service state, and noon.park's three chosen edits: a second toilet block, the drink shop switched to stock both goods, and new opening hours. Each edit's candidate equals the world that commits it. (listed tests; ctest)
4. Over one day from 9:00, more guests leave no-toilet.park with the toilet as their reason than leave day.park for it, and the mean stay in no-toilet.park is shorter. Switching noon.park's drink shop to food only leaves its guests' mean thirst at closing higher than in noon.park unchanged. (check: the scratch program of PLAN.md Task 7 prints both sides of each)
5. Going home and arrivals hold on day.park. The going-home check's closing, tally, and reason rules pass on it, and the arrival check's hourly counts match the curve on it. (check: PLAN.md Task 8)
6. A day at day.park reads as the milestone's open question asks: guests stay for hours, no-toilet.park loses them earlier, and the park is empty well before midnight. (check: the scratch program prints the mean stay, the peak count and its time, and when the last guest leaves; play: the user watches day.park through a day at 4x in the release app)
7. src/scenarios/SPEC.md describes the four parks. The windows-debug build has no warnings, ctest passes, and tidy is clean. If no tuning value changed, a tpj_scenarios --compare over the existing parks shows the simulation unchanged. If one changed, every park that holds guests is remade and full.park's release stage medians are reported before and after. (check: PLAN.md Tasks 9 and 10)
8. Guests are shaded by their most pressing need in noon.park, as milestone criterion 8 asks. (capture: PLAN.md Task 9)

## Tests

In tests/sim/park_world_checks_test.cpp:

- Remove facility-service from UNHELD_TYPES. noon.park holds a bench's or toilet block's service state once guests have used one, so a checked world holds it.
- Add three chosen edits of tests/parks/noon.park, made from noon.park opened and stepped one cycle, as warm.park's edits are. Like those, each edit's candidate must equal the world that queuing it for that cycle gives and the world that committing it while paused gives. Both are among the checked worlds, and each edit is among the two-runs checks.
  - Placing a second toilet block with AddBox at Pose{5.5, 106.0, -1.0, 0.0}, facing the guest path in from the entrance. Require that the world accepts it and that the candidate's new toilet block has a guest connector, from either face (spec decision from the test pass: the edit only needs the block on the guest network).
  - Setting the shop that sells drink only to stock both goods with SetShopStock, while guests are in the park. Require that the candidate's shop stocks both.
  - Setting the park's opening hours to 8:00 to 20:00 (28800 and 72000) with SetOpeningHours, while guests are in the park. Require that the candidate's hours are those.

## Medium

None new. The parks are made with the park's commands and stepped with the simulation as it is. The scratch program reads guestRecord and departureTally (decision 0025).

The parks hold state that is already registered: the departure tally, guests' needs, shops' stock per good, and benches' and toilet blocks' service state. The world checks cover it once noon.park holds it.

## Spec changes

src/scenarios/SPEC.md, Slice parks: the command takes eleven files, ending with day, no-toilet, noon, and evening. A new paragraph describes the four parks (PLAN.md Task 1).

The public interface adds src/scenarios/day_parks.h:

- DAY_PARK_SEED, NOON_TICKS (5400, to 12:00), and EVENING_TICKS (20700, to 20:30).
- `struct DayParks { std::string Day; std::string NoToilet; std::string Noon; std::string Evening; };`
- `DayParks makeDayParks();`

## Files affected

- Modify: src/scenarios/SPEC.md, src/scenarios/CMakeLists.txt, src/scenarios/main.cpp (composition only: make the day parks and write four more files)
- Create: src/scenarios/day_parks.h, src/scenarios/day_parks.cpp
- Create: tests/parks/day.park, no-toilet.park, noon.park, evening.park
- Modify: tests/sim/park_world_checks_test.cpp (the test pass)
- Modify, only if the user names tuning values: src/sim/guests/guests.h, every park in tests/parks that holds guests, and tests/parks/stress/full.park
- Modify: plans/believable-guests/a-day-out/MILESTONE.md and plans/slices/day-at-the-park/SLICE.md (the compact layout; done while planning)

## Dependencies

- arrival-curve, going-home, and four-needs: merged.
- plausible-operations/served-needs: benches, toilet blocks, and drink shops serving. Complete.

## Out of scope

- Each need overlay's discount curve in game time, so the shops' overlays separate in a compact park: explained-needs, as the slice now says.
- The integration tests for the slice's criteria 1 to 3: the slice's closing.
- The slice's captures of noon.park and day.park in daylight, at night, and with overlays: stylized-presentation and explained-needs.
- Choosing tuning values by search. If a comparison fails or the day reads wrong, the numbers go to the user, who names the values to change.

## Open questions

- Whether a day at day.park reads well, which resolves the milestone's open question. The user decides by watching it, and names any tuning values to change.
