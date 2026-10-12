# Feature: Seat Parks

## Summary

tpj_scenarios --slice-parks remakes day.park with furniture and without its bench box. The arm toward the food shop gets tables on both sides every 6 m, and the path in gets benches every 10 m on the side away from the toilet block. noon.park, evening.park, and no-toilet.park follow from it. Two new parks join them: far-tables.park moves the tables to the arm toward the drink shop, and unfurnished.park has no furniture. noon.park then holds guests seated at furniture, carrying food, and remembering a full piece, so the park world checks cover that state, and seat-service leaves their unheld types. The checks gain an edit of noon.park that places a shop facing the benches. A scratch program runs the milestone's two comparisons and repeats the earlier features' checks on the furnished parks. The user watches noon.park, and if a tuning value reads wrong, the user names the new value and this feature sets it.

## Acceptance criteria

1. tpj_scenarios --slice-parks takes thirteen files and writes far-tables.park and unfurnished.park after the parks it writes today. day.park holds no bench box. Its arm toward the food shop has tables on both sides, every 6 m, and its path in has benches every 10 m on the side away from the toilet block. far-tables.park is day.park with that arm unfurnished and the arm toward the drink shop holding tables on both sides, every 6 m. unfurnished.park is day.park with no furniture. Both start at 9:00 with no guests, and every box, path, and entrance keeps its key from day.park. (check: PLAN.md Task 7 prints each park's furniture by segment, kind, and side; play: open day.park and see it)
2. noon.park holds a guest seated at furniture, a guest carrying food, and a guest that remembers a full piece. (check: PLAN.md Task 7 prints how many of each noon.park holds)
3. The park world checks cover seat service with seat-service removed from their unheld types. They check a new edit of noon.park against its commit: a shop placed facing the path in from the entrance, on the benched side, which displaces benches. The integration tests over the remade day parks keep passing. (listed tests; ctest)
4. Over one day from 9:00, a smaller share of meals is eaten seated in far-tables.park than in day.park. More guests leave unfurnished.park with tiredness as their reason than leave day.park for it. These are the milestone's criterion 8 and the slice's criteria 1 and 2. (check: PLAN.md Task 8 prints both sides of each)
5. The milestone's criteria 1, 4, and 5 hold on the furnished day parks. Every seat option of every guest in noon.park has terms that add up to its score. Over a day of day.park, every meal eaten, seated or standing, has the relief its criterion gives, meals consumed as eaten equal the meals guests record eating, and the ledger's identity holds. Every guest turned away from a piece remembers at most SEAT_CHOICES pieces and forgets each one FULL_MEMORY_TIME after it found it full. (check: PLAN.md Task 8 prints each count with how many were wrong)
6. In noon.park, guests sit at the tables beside the food shop and on the benches, as the milestone's criterion 3 and the slice's criterion 4 ask. (capture: PLAN.md Task 9)
7. The user watches noon.park in the release app with the guest Inspector, which shows the milestone's criteria 2, 3, and 6 as played. The scratch program also prints how often guests with tiredness under 0.1 pick a rest seat (RESEARCH.md). If a tuning value reads wrong, the user names the new value. This feature then sets it, remakes every park that holds guests, and compares the runtime report on full.park before and after. (play: PLAN.md Task 10)
8. src/scenarios/SPEC.md describes the new parks, and src/sim/guests/SPEC.md and the other specs seated-meals touched describe guests as built. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Task 11 reads each spec beside its code)

## Tests

In tests/sim/park_world_checks_test.cpp:

- Remove seat-service from UNHELD_TYPES. noon.park holds a guest seated at furniture, so a checked world holds a piece's seat service.
- Add a chosen edit of tests/parks/noon.park to noonChosenEdits, made from noon.park opened and stepped one cycle, as the other noon.park edits are. It places a shop with AddBox at Pose{6.5, 106.8, -1.0, 0.0}, facing the guest path in from the entrance from the side its benches stand on. Require that the world accepts it. Require that the pieces of furniture on that guest segment differ between the world before and the candidate, by key or pose, so the edit displaces benches. Like the other chosen edits, its candidate must equal the world that queuing it for that cycle gives and the world that committing it while paused gives, and it joins the two-runs checks.
- Keep furnishingLongestGuestSegmentUnderGuests. noon.park now holds furniture of its own, so reword its comment: the edit lays furniture again under guests in the park, some of them seated or heading to a piece.

## Medium

None new. The parks are made with the park's commands and stepped with the simulation as it is. The scratch program reads guestRecord, guestOptions, departureTally, parkFurniture, the ledger's counts, and best rest seats (decision 0025).

The parks hold state that is already registered: furnishing intent, furniture pieces, seat service, guests' carried meals in the ledger, their remembered full pieces, and the seat reliefs they chose with. The world checks cover it once noon.park holds it.

## Spec changes

src/scenarios/SPEC.md, Slice parks. The command line becomes:

"tpj_scenarios --slice-parks fed warm cut main-street warm-main-street facilities stocked day no-toilet noon evening far-tables unfurnished makes the boxes-and-tubes slice's parks from fed, the good-paths slice's warm park from main-street, the day-at-the-park slice's stocked park from facilities, the day-at-the-park slice's four day parks, and the somewhere-to-sit slice's two parks that move or remove day's furniture:"

The day bullet becomes:

"- day is a T of guest path from the entrance, with a food shop at one end of the T's arms and a drink shop at the other, a toilet block on the path in, and a depot on a backstage path behind both shops. The arm toward the food shop has tables on both sides, every 6 m, and the path in has benches every 10 m on the side away from the toilet block. Every facility is within about an hour's walk of the entrance. It has the default hours and starts at 9:00 with no guests. If a shop would be starved, or a shop or toilet block would have no guest connector, it fails."

Two bullets follow the evening bullet:

"- far-tables is day with the arm toward the food shop unfurnished and the arm toward the drink shop holding tables on both sides, every 6 m. The benches stay.
- unfurnished is day with no furniture.

In no-toilet, far-tables, and unfurnished, every box, path, and entrance keeps its key from day, so all four parks admit the same guests with the same needs."

The no-toilet bullet loses its own sentence about keys, which the new paragraph covers. The list of output files gains far-tables.park and unfurnished.park.

The public interface changes src/scenarios/day_parks.h:

- `struct DayParks` gains `std::string FarTables;` and `std::string Unfurnished;` after Evening.
- `DAY_TABLE_SPACING` (6.0) and `DAY_BENCH_SPACING` (10.0), in meters.
- makeDayParks's comment names the two new parks and drops the bench from its failure cases.

If the user names a tuning value, its constant's comment in its header changes with it, and no spec sentence does, because the specs name the constants, not their values.

## Files affected

- Modify: src/scenarios/SPEC.md, src/scenarios/day_parks.h, src/scenarios/day_parks.cpp
- Modify: src/scenarios/main.cpp (composition only: write two more files)
- Modify: tests/parks/day.park, no-toilet.park, noon.park, evening.park (remade)
- Create: tests/parks/far-tables.park, tests/parks/unfurnished.park
- Modify: tests/sim/park_world_checks_test.cpp (the test pass)
- Modify, only if the user names tuning values: the header that holds the value, every park in tests/parks that holds guests, and tests/parks/stress/full.park
- Modify, only where Task 11 finds a spec that doesn't match the code: that SPEC.md
- Create, outside the repository's tracked files: build/seat-parks/ scratch programs

## Dependencies

- seat-rests and carried-meals: merged.
- effortless-building/furnished-paths: SetFurnishing and the furniture it lays. Complete.

## Out of scope

- Retiring the bench box, its tool, and its inspector: the slice's last step, after explained-seating.
- The integration tests for the slice's criteria 1 and 2: the slice's closing.
- The eating-seat overlay and the Inspector's seated or standing meal line: explained-seating.
- Changing decision 0019's softmax pick, such as picking among the best few options (RESEARCH.md). If the user wants that, it needs a decision record and its own feature.
- Choosing tuning values by search. If a comparison fails or noon.park reads wrong, the numbers go to the user, who names the values to change.

## Open questions

- Whether noon.park reads well, including how often guests who aren't tired sit down. The user decides by watching it, and names any tuning values to change.
