# Implementation Plan: Carried Meals

## Goal

Served guests carry their meal and choose between eating now and the best meal seats, and eat at a seat with its reliefs or now with STANDING_MEAL_RELIEF.

## Approach

A served guest keeps its meal unit in its own ledger stock instead of consuming it, and that stock is what carrying means. While it carries food, scoring gives eating now and the meal seats from best meal seats, reusing seat-rests' seat options with the use and need as parameters. The seat a guest heads to takes its field from the need it chose it for. One eatMeal helper consumes the meal and records it, called by eating now in choose and by a meal seat's returned rest in awaitSeat.

## Placement

Decision 0027 places each behavior this feature adds:

- Carrying a meal, scoring eating now and meal seats, eating now, and eating at a seat: sim/guests, src/sim/guests/guests.cpp, with the seat's rest relief saved in src/sim/guests/internal/guest.h. A guest's choices, its visits, and its needs are the guests module's.
- Whether a guest is carrying food and whether its last meal was seated, in the record: sim/guests, guestRecord in src/sim/guests/guests.h, which publishes everything about a guest.
- The "Carrying" row and the choice rows while carrying: legible, src/legible/inspect.cpp, which builds every inspector row.

## Tasks

### Task 1: Spec the change

Files:
- Modify: `src/sim/guests/SPEC.md`, `src/legible/SPEC.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

### Task 2: Declare the interface

Files:
- Modify: `src/sim/guests/guests.h`

Step 1: Add the declarations FEATURE.md's Public interface gives. Append EatNow after Seat, and "eat-now" after "seat" in its names, so saved names keep their meaning. Visit `seated` after `after` in GuestMeal's visitFields.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 3: Save the meal seat's rest relief

Files:
- Modify: `src/sim/guests/internal/guest.h`

Step 1: Add `double SeatRestRelief = 0.0;` to Guest after Full. Update the struct's comment to end "..., the pieces it remembers as full, and the rest relief of the piece it heads to or sits at to eat."

Step 2: Visit it last in visitFields, as `seat-rest-relief`, after the full pieces.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 4: Keep a served meal

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: In takeVisit, wrap the block that takes servings in `if constexpr (N != GuestNeed::Hunger)`, and remove its meal branch, so a returned meal stays in the guest's stock. Its comment becomes "Takes back the guest's visit for the need when it holds it: it finishes the visit, and takes any drink, rest, or toilet use that came with it, which lowers the need by the offer's relief and fills the bladder by the need's fill. A meal stays in its stock, and it carries the meal. Returns whether the visit was back."

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 5: Choose at once when served a meal

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Rename SeatWait to VisitWait, with values Waiting, Back, and Choose. Its comment: "What awaiting a visit found: the guest still waits, its visit came back, or its visit came back and it must choose now."

Step 2: awaitVisit returns VisitWait. It returns Waiting while the visit isn't back. When it is back, it returns Choose if `unitsHeld<Meals>(world, key, key) > 0`, and otherwise Back. awaitSeat returns Choose where it returned Back.

Step 3: In stepGuests, replace seatBack with `bool mustChoose`. Each of awaitSeat and awaitVisit continues the loop on Waiting, and sets mustChoose on Choose. Pass mustChoose to walk.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 6: Score the options of a guest carrying food

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Add, after rememberFull, `void eatMeal(World &world, EntityKey key, Guest &guest, double relief, bool seated)`. Its comment: "The guest eats the meal it carries: it consumes the meal with EATEN_CAUSE, its hunger falls by the relief, not below 0, its bladder fills by BLADDER_PER_MEAL, and it records the meal." It takes `meals = unitsHeld<Meals>(world, key, key)`, consumes them with EATEN_CAUSE, keeps `before = guest.Hunger`, sets Hunger to `std::max(0.0, before - relief)`, sets Bladder to `std::min(1.0, guest.Bladder + (BLADDER_PER_MEAL * static_cast<double>(meals)))`, increments MealsEaten, and sets LastMeal to `{world.Tick, before, guest.Hunger, seated}`.

Step 2: Give appendSeatOptions a `SeatUse use` parameter. It samples that use's field. Its need is Hunger for SeatUse::Meal and Tiredness for SeatUse::Rest, and its Relief term uses needCurve of that need's level. Update its comment to name both fields.

Step 3: Give scoreOptions a `bool carrying` parameter. When carrying, it calls appendSeatOptions with SeatUse::Meal, then pushes eating now, and returns. Eating now has Kind EatNow, Need Hunger, Relief `RELIEF_WEIGHT * needCurve(guest.Hunger) * STANDING_MEAL_RELIEF`, and Score equal to Relief. Push STANDING_MEAL_RELIEF to Reliefs and NULL_KEY to Owners with it. Otherwise it scores as now, with appendSeatOptions given SeatUse::Rest.

Step 4: choose takes `World &world`. It reads `carrying = unitsHeld<Meals>(world, key, key) > 0` and passes it to scoreOptions. guestOptions passes `unitsHeld<Meals>(world, guest, guest) > 0`.

Step 5: In choose, when the picked option is a seat for hunger, set SeatRestRelief to the RestRelief of the first entry of `sourceSlot<SeatOffers>(world, option.Facility)`, or 0 when there is no slot or no entry. When the picked option is EatNow, call `eatMeal(world, key, guest, STANDING_MEAL_RELIEF, false)` and set the activity to Wandering.

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 7: Head to a meal seat and eat there

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Add `SeatUse seatUseOf(const Guest &guest)`: SeatUse::Meal when TargetNeed is Hunger, and SeatUse::Rest otherwise. headingOf and traceGuestRoute use it in place of SeatUse::Rest. Update both comments to say the piece's entry in the field it chose the piece from.

Step 2: In awaitSeat, when rests come back: if TargetNeed is Hunger, call `eatMeal(world, key, guest, guest.ServingRelief, true)` and lower tiredness by SeatRestRelief, not below 0. Otherwise lower tiredness by ServingRelief as now. Update its comment.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 8: Publish carrying

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: In guestRecord, set Carrying to `unitsHeld<Meals>(world, guest, guest) > 0`.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 9: Explain carrying in the Inspector

Files:
- Modify: `src/legible/inspect.cpp`

Step 1: guestRows adds a "Carrying" row after "Target": "food" when the record is carrying, and otherwise "nothing".

Step 2: choiceRows takes `bool carrying`, and inspectSubject passes `record->Carrying`. Each need's row counts options of kind EatNow with those of kind Offer and Seat. When the best of them is EatNow, its Target is "eat now". When carrying, every row other than hunger's has Target "carrying food" and no other cell. Update its comment.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug`

(Deviation, minor: the app has no target named ThemeParkJones, so this step builds the whole preset.)
Expected: the build ends with no warnings and no errors.

### Task 10: Remake the parks

Files:
- Modify: `tests/parks/cut.park`, `evening.park`, `noon.park`, `warm.park`, `warm-main-street.park`, `tests/parks/stress/full.park`, `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

Step 1: Remake the slice parks.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park tests/parks/day.park tests/parks/no-toilet.park tests/parks/noon.park tests/parks/evening.park; echo $?; git status --short tests/parks`
Expected: `0`, then ` M` lines for cut.park, evening.park, noon.park, warm-main-street.park, and warm.park, and no others.

(Deviation, minor: after this remake, warm.park held only one guest heading to the shop, on a segment that refuses a path crossing its middle, so three park world checks failed. Guests eat more often now, because a meal eaten standing relieves less. src/scenarios/slice_parks.h's WARM_TICKS goes from 4600 to 4320, inside the range from 4213 to 4434 where warm.park holds a guest heading to the shop on a crossable segment, a guest waiting at the shop, and supplies in transit, as build/carried-meals/warmticks.cpp found. Then this step runs again. The supply-route integration test also failed with the 4600 warm-up, cut's mean hunger 0.284 against warm's 0.292, and passes with 4320.)

Step 2: Remake full.park.

Run: `cmake.exe --build --preset windows-release --target tpj_bench > /dev/null 2>&1; build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

Step 3: Add the new fields to the play parks' guest lines.

Run: `sed -i -E 's/(last-meal=\{[^}]*)\}/\1 seated=false}/; s/(full-3=\{[^}]*\})$/\1 seat-rest-relief=0/' parks/busted-path.park parks/jank.park parks/radii.park; grep -c "seat-rest-relief=0$" parks/busted-path.park parks/jank.park parks/radii.park`
Expected: `parks/busted-path.park:35`, `parks/jank.park:37`, `parks/radii.park:63`

Step 4: Load them.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 1 parks/busted-path.park parks/jank.park parks/radii.park > /dev/null; echo $?`
Expected: `0`

If the saved form of a field differs from the sed's text, copy it from a guest line of the remade noon.park and run step 3 again on the files from git.

### Task 11: Check the meals

Files:
- Create: `build/carried-meals/build.sh` (copied from build/seat-rests/build.sh), `build/carried-meals/mealcheck.cpp`

Step 1: Write mealcheck.cpp. It runs two worlds.

The first is a copy of tests/parks/stress/full.park with SetFurnishing of no furnishing on every guest segment except the five with the lowest keys, resolved, then stepped 1800 ticks. At each tick it does four things:

- For each guest, it calls guestRecord and guestOptions. If the record is carrying, every option must be eating now or a seat for hunger. Eating now's Relief and Score must equal `RELIEF_WEIGHT * needCurve(hunger) * STANDING_MEAL_RELIEF`, with every other term 0. Each seat option must name a piece that `bestSeatsAt(..., SeatUse::Meal, ...)` gives at the guest's place, not in the record's FullPieces, with Relief `RELIEF_WEIGHT * needCurve(hunger) * Value`, Distance `DISTANCE_WEIGHT * Distance`, and Commitment COMMITMENT_BONUS exactly when the guest heads to that piece. If the record isn't carrying, no option may be eating now or a seat for hunger. It counts `meal options` checked and wrong, and `carrying` guest ticks.
- It keeps each guest's last record. When a guest's MealsEaten rises by one with LastMeal.Seated false, the meal counts as standing. It is right when After is `max(0, Before - STANDING_MEAL_RELIEF)`, bladder is `min(1, b + BLADDER_PER_MEAL)` where b is the bladder in the tick before, the record no longer carries, and the guest was carrying or waiting at a shop for hunger in the tick before. When it rises with LastMeal.Seated true, the meal counts as seated. It is right when the guest was Seated in the tick before at a piece whose kind gives meal relief m and rest relief r by seatOffer, After is `max(0, Before - m)`, bladder is `min(1, b + BLADDER_PER_MEAL)` as for a standing meal, and tiredness is at least `max(0, t - r)` and at most 0.01 above it, where t is the tiredness in the tick before. The guest walks after it eats in that tick, and the 0.01 allows for that walk, as build/seat-rests/seatcheck.cpp does. If the seat record was gone in the tick before, it takes the kind from the last tick that gave one.
- When a guest waiting or seated at a piece for hunger leaves Waiting with that piece in its FullPieces and its record still carrying, it counts as a meal turned away, right when the tick is its visit's send tick plus 2.
- It checks the meal ledger: meals consumed with EATEN_CAUSE rose by the sum of the rise in MealsEaten over guests in both ticks, and `unitsCreated == unitsHeld + unitsInTransit + unitsConsumed` for meals.

The second is tests/parks/day.park stepped from 9:00 for 43200 ticks, a full day at GAME_SECONDS_PER_TICK 2, with the same ledger check.

It prints `meal options: N checked, 0 wrong`, `carrying: N`, `standing meals: N wrong: 0`, `seated meals: N wrong: 0`, `meal turned away: N wrong: 0`, `full.park: meals eaten N, ledger 0 wrong`, and `day.park: meals eaten N, ledger 0 wrong`.

Step 2: Build and run it.

Run: `sh build/carried-meals/build.sh mealcheck && build/carried-meals/mealcheck.exe`
Expected: the seven lines above, each N above 0 and each wrong count 0.

### Task 12: Play and measure

Step 1: Rebuild windows-release in full.

Run: `cmake.exe --build --preset windows-release`
Expected: the build ends with no warnings and no errors.

Step 2: Play tests/parks/stress/full.park in build/windows-release/ThemeParkJones.exe. Do the play checks of criteria 3 and 5.

Step 3: Make the runtime report and compare it with build/seat-rests/after.report.

Run: `scripts/runtime-report.sh build/carried-meals/after.report > build/carried-meals/report.log 2>&1; tail -1 build/carried-meals/report.log; scripts/runtime-report.sh --compare build/seat-rests/after.report build/carried-meals/after.report | tail -2`
Expected: `over-budget 0 of 25` in the report, and the comparison's lines.

### Task 13: Confirm the criteria

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports nothing.

### Task 14: Commit

Step 1: Commit the feature with commit-hygiene. Stage only this feature's paths, by pathspec.
