# Implementation Plan: Day Parks

## Goal

Make day.park, no-toilet.park, noon.park, and evening.park with tpj_scenarios --slice-parks, cover them in the world checks, and show the slice's comparisons and a day's shape on them.

## Approach

A new src/scenarios/day_parks.cpp lays day.park out with the park's commands on the new-park template, as full_park.cpp lays out the full park, and refuses a layout with a starved shop or a facility off the guest network. It saves day.park, deletes the toilet block from a copy for no-toilet.park, and steps day.park to 12:00 and 20:30 for the other two. tpj_scenarios's --slice-parks writes them after the parks it writes today. Scratch programs run the comparisons and the earlier features' checks on the new parks.

## Placement

Decision 0027 places each behavior this feature adds:

- Laying out and stepping the four day parks: scenarios, a new src/scenarios/day_parks.h with day_parks.cpp. The scenarios module owns the park generators, and full_park.cpp and slice_parks.cpp each hold one family of parks, so the slice's four parks get a file of their own.
- Writing them: scenarios, src/scenarios/main.cpp's writeSliceParks. It already writes every slice park. The change is composition only: it calls makeDayParks and writes four more files.
- The noon.park edits in the world checks: tests/sim/park_world_checks_test.cpp, which holds every chosen edit.

## Tasks

### Task 1: Record the scenarios output before the change

Files: none in the repository.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios > /dev/null 2>&1; build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/predict-check/day_parks_before.txt 2>/dev/null; echo $?`
Expected: `0`

### Task 2: Update the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md:39,46`

Step 1: In line 39, change "tpj_scenarios --slice-parks fed warm cut main-street warm-main-street facilities stocked makes the boxes-and-tubes slice's parks from fed, the good-paths slice's warm park from main-street, and the day-at-the-park slice's stocked park from facilities:" to "tpj_scenarios --slice-parks fed warm cut main-street warm-main-street facilities stocked day no-toilet noon evening makes the boxes-and-tubes slice's parks from fed, the good-paths slice's warm park from main-street, the day-at-the-park slice's stocked park from facilities, and the day-at-the-park slice's four day parks:".

Step 2: Add these bullets after the stocked bullet:

    - day is a T of guest path from the entrance, with a food shop at one end of the T's arms and a drink shop at the other, a bench and a toilet block on the path in, and a depot on a backstage path behind both shops. Every facility is within about an hour's walk of the entrance. It has the default hours and starts at 9:00 with no guests. If a shop would be starved, or a shop, bench, or toilet block would have no guest connector, it fails.
    - no-toilet is day with its toilet block deleted. Every other entity keeps its key, so both parks admit the same guests with the same needs.
    - noon is day at 12:00, NOON_TICKS cycles later, with guests in the park.
    - evening is day at 20:30, EVENING_TICKS cycles later, so a run of tpj_scenarios's default 3,000 ticks crosses closing.

Step 3: In line 46, change "tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, and stocked.park are its output." to "tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, stocked.park, day.park, no-toilet.park, noon.park, and evening.park are its output."

### Task 3: Add the day parks' interface

Files:
- Create: `src/scenarios/day_parks.h`, `src/scenarios/day_parks.cpp`
- Modify: `src/scenarios/CMakeLists.txt:5-14`

Step 1: Create day_parks.h with include guard TPJ_SCENARIOS_DAY_PARKS_H. It declares, each with a comment:

- `inline constexpr uint64_t DAY_PARK_SEED = 1;` "The day parks' seed."
- `inline constexpr uint64_t NOON_TICKS = 5400;` "The cycles noon.park takes from day.park: 3 game hours, from 9:00 to 12:00."
- `inline constexpr uint64_t EVENING_TICKS = 20700;` "The cycles evening.park takes from day.park: 11.5 game hours, from 9:00 to 20:30, so a run of 3,000 ticks crosses closing at 21:00."
- `struct DayParks { std::string Day; std::string NoToilet; std::string Noon; std::string Evening; };` "The day-at-the-park slice's four day parks, as saves."
- `DayParks makeDayParks();` "Lays out day.park on the new-park template and saves it. Saves it with its toilet block deleted as NoToilet, and stepped NOON_TICKS and EVENING_TICKS cycles as Noon and Evening. Throws std::logic_error when the world refuses a command, a shop is starved, or a shop, bench, or toilet block has no guest connector."

Step 2: Create day_parks.cpp defining makeDayParks to return `DayParks{}`. Add `day_parks.cpp` to tpj_scenarios_lib's sources, before food_shop.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 4: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/scenarios/SPEC.md, src/sim/guests/SPEC.md, src/sim/park/SPEC.md, and src/sim/operations/SPEC.md, and the public headers src/scenarios/day_parks.h, src/sim/park/edits.h, and src/sim/park/intent.h. Its noon.park edits fail until Task 6 makes the park.

### Task 5: Lay out and make the day parks

Files:
- Modify: `src/scenarios/day_parks.cpp`

Step 1: In an anonymous namespace, add `applyAccepted`, `addPath`, and `addBox` as full_park.cpp has them, with messages starting "day park:", and `TEMPLATE_SEGMENT{4}`, the template's guest segment.

Step 2: Add `void layOut(World &world)`, commented with what it lays out. It deletes TEMPLATE_SEGMENT, then adds, in this order:

- the guest path from (0, 123) to (0, 100), and the guest path from (-50, 100) to (50, 100), which it joins at its middle;
- the backstage path from (-50, 90) to (50, 90);
- the food shop at Pose{-44.0, 94.5, 0.0, 1.0} and the drink shop at Pose{44.0, 94.5, 0.0, 1.0}. Each front door, at z = 97.5, is 1 m from the arms' paving, and each back door, at z = 91.5, is 0.5 m from the backstage paving;
- the depot at Pose{0.0, 84.0, 0.0, 1.0}, its front door at z = 88, 1 m from the backstage paving;
- the bench at Pose{3.0, 113.0, -1.0, 0.0} and the toilet block at Pose{-5.0, 113.0, 1.0, 0.0}, as in tests/parks/facilities.park.

It then sets the drink shop to ShopStock::Drink with SetShopStock. The food shop keeps the default, food.

Step 3: Add `void refuseUnservedFacilities(const World &world)`, like full_park.cpp's refuseStarvedShops: it throws naming the first shop with no nearestDepot, or the first shop, bench, or toilet block with no guest connector (`parkNetwork(world, PathKind::Guest).firstAnchoredNode(box.Key)`).

Step 4: Define makeDayParks. Make `World world = makeNewPark(DAY_PARK_SEED);`, lay it out, resolve it, and refuse unserved facilities. Save it as Day. Make a copy with copyWorld, delete its toilet block with applyAccepted and DeleteBox, resolve it, and save it as NoToilet. Step the world NOON_TICKS cycles and save it as Noon, then EVENING_TICKS less NOON_TICKS more and save it as Evening.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 6: Write the day parks

Files:
- Modify: `src/scenarios/main.cpp:6,118-185`
- Create: `tests/parks/day.park`, `no-toilet.park`, `noon.park`, `evening.park`

Step 1: In main.cpp, include scenarios/day_parks.h. In the usage comment at line 6, append " DAY NO_TOILET NOON EVENING". In writeSliceParks, require 12 arguments and say "--slice-parks takes eleven files". After making the stocked park, make the day parks with makeDayParks inside a try that catches std::logic_error, prints "tpj_scenarios: day parks: " and the message, and returns 1. After writing the stocked park, write Day, NoToilet, Noon, and Evening to arguments 8 to 11, each as the other files are written. Extend writeSliceParks's comment to say it makes the day parks and writes them to DAY, NO_TOILET, NOON, and EVENING.

Step 2: Make every slice park.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios > /dev/null 2>&1; build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park tests/parks/day.park tests/parks/no-toilet.park tests/parks/noon.park tests/parks/evening.park; echo $?; git status --short tests/parks`
Expected: `0`, then only the four new parks, untracked. The parks made before are rewritten byte for byte.

Step 3: Check what the new parks hold.

Run: `for p in day no-toilet noon evening; do printf "%s: tick %s, guests %s, toilet blocks %s\n" $p "$(grep '^tick' tests/parks/$p.park | cut -d' ' -f2)" "$(grep -c ' activity=' tests/parks/$p.park)" "$(grep -c 'kind=toilet-block' tests/parks/$p.park)"; done; grep -c "^\[facility-service\]" tests/parks/noon.park`
Expected: `day: tick 0, guests 0, toilet blocks 1`, `no-toilet: tick 0, guests 0, toilet blocks 0`, noon at tick 5400 with guests above 0 and 1 toilet block, evening at tick 20700 with 1 toilet block, then `1`.

Run: `grep -A3 '^\[facility-service\]' tests/parks/noon.park`
Expected: at least one line whose queue or free-at list is not empty, so a guest is seated or queued at a bench or toilet block at 12:00, as milestone criterion 7 asks. If every list is empty, that is a Major deviation: report it.

Step 4: Run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests > /dev/null 2>&1; build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]" | tail -2`
Expected: `All tests passed`. If a noon.park edit's REQUIRE fails because noon.park lacks what it needs, that is a Major deviation: report it.

### Task 7: Run the slice's comparisons and a day's shape

Files:
- Create: `build/predict-check/day_check.cpp` (scratch, not committed)

Step 1: Write a scratch program, in the shape of build/predict-check/arrival_check.cpp, with two modes.

In mode `day`, it loads tests/parks/day.park and tests/parks/no-toilet.park, resolves each, and steps each one day, 43,200 ticks. Each tick, it remembers each guest's Arrived. When a guest is gone after a step, its stay is the tick of that step plus 1 less its Arrived. For each park it prints the departure tally by reason at the end of each game hour's last tick, the mean stay of the guests that left in game hours, the most guests inside at once and the time, and the time the last guest left. It then prints both parks' toilet departures and mean stays side by side, and `toilet comparison passed` if no-toilet.park's toilet departures are higher and its mean stay shorter. The run ends at the last tick before 9:00 of day 2, before the tally starts again, so the tally at the end holds the whole day.

In mode `noon`, it loads tests/parks/noon.park twice and resolves both. In one, it applies SetShopStock with ShopStock::Food to the shop that sells drink only, and resolves. It steps both to 21:00, 16,200 ticks, and at the tick the clock reaches 21:00 prints each park's guest count and mean thirst, then `thirst comparison passed` if the switched park's mean is higher.

Step 2: Build it against the release library and run both modes.

Run: `cmake.exe --build --preset windows-release > /dev/null 2>&1; cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O2 day_check.cpp ../windows-release/src/sim/libtpj_sim.a -o day_check.exe && ./day_check.exe day && ./day_check.exe noon`
Expected: the hourly tallies, then `toilet comparison passed` and `thirst comparison passed`. Report every printed number. If either comparison fails, or the last guest leaves after midnight, or the mean stay in day.park is under two game hours, stop and report the numbers to the user. They name any tuning values to change, and Task 10 sets them.

### Task 8: Run the earlier features' checks on day.park

Files: none in the repository.

Run: `cd build/predict-check && ./arrival_check.exe default ../../tests/parks/day.park | tail -3`
Expected: `arrival check passed` after the openings and arrivals line.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O2 going_home_check.cpp ../windows-release/src/sim/libtpj_sim.a -o going_home_check_release.exe && ./going_home_check_release.exe stocked ../../tests/parks/day.park | tail -3`
Expected: `going home check passed` last.

### Task 9: Run the full checks and play

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

Step 2: Show the simulation is unchanged on the parks that existed before.

Run: `build/windows-debug/tpj_scenarios.exe $(ls tests/parks/*.park | grep -v -e day.park -e no-toilet.park -e noon.park -e evening.park) > build/predict-check/day_parks_after.txt 2>/dev/null; build/windows-debug/tpj_scenarios.exe --compare build/predict-check/day_parks_before.txt build/predict-check/day_parks_after.txt; echo $?`
Expected: `0`

Step 3: Capture noon.park for milestone criterion 8.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/noon.park --capture build/predict-check/noon.bmp; echo $?`
Expected: `0`. Look at the capture: guests stand on the paving and at the facilities' doors, shaded from the sated color toward the distressed color by their most pressing need.

Step 4: Build the whole windows-release preset. Ask the user to open tests/parks/day.park in build/windows-release/ThemeParkJones.exe and watch a day at 4x: guests arrive through the morning, walk to the shops, the bench, and the toilet block, and head home through the afternoon and at closing. The Debug panel's guest count reaches 0 before midnight. Ask whether the day reads well, and which tuning values, if any, to change.

### Task 10: Set the tuning values the user names

Skip this task if the user names none.

Files:
- Modify: `src/sim/guests/guests.h` (the named constants and their comments)
- Modify: every park in tests/parks that holds guests, and `tests/parks/stress/full.park` (remade)

Step 1: Time the full park with the release build before the change: `build/windows-release/tpj_bench.exe tests/parks/stress/full.park > build/predict-check/day_parks_bench_before.txt`.

Step 2: Set the named values. Remake the slice parks as Task 6 Step 2 does, and the full park as arrival-curve's PLAN.md Task 5 Step 2 does. Run Task 6 Steps 3 and 4, and Tasks 7 and 8 again.

Step 3: Time the full park again into build/predict-check/day_parks_bench_after.txt and report each stage's median before and after, as arrival-curve's PLAN.md Task 7 does. Run Task 9 Step 1 again. Task 9 Step 2's comparison no longer applies, because the simulation changed on purpose.

### Task 11: Commit

Stage only this feature's paths: src/scenarios/SPEC.md, src/sim/guests/guests.h and the remade parks if Task 10 ran, CMakeLists.txt, main.cpp, day_parks.h, day_parks.cpp, the four new parks, tests/sim/park_world_checks_test.cpp, plans/believable-guests/a-day-out/MILESTONE.md, plans/slices/day-at-the-park/SLICE.md, and plans/believable-guests/a-day-out/day-parks/. Commit with the commit-hygiene skill, subject "Scenarios: Make the day-at-the-park slice's four day parks".
