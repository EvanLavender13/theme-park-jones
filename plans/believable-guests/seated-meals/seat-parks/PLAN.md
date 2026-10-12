# Implementation Plan: Seat Parks

## Goal

tpj_scenarios --slice-parks makes the day parks with furniture and without a bench box, adds far-tables.park and unfurnished.park, and the furnished parks pass the milestone's comparisons and the earlier features' checks.

## Approach

makeDayParks lays day.park as today without its bench box, then sets furnishing on two guest segments it finds by their points' positions. It makes far-tables.park and unfurnished.park from copies of day.park by setting furnishing again, as it makes no-toilet.park by deleting the toilet block. Scratch programs adapted from carried-meals' mealcheck and seat-rests' seatcheck run the comparisons and the checks. Tuning values change only if the user names them after watching noon.park.

## Placement

Decision 0027 places each behavior this feature adds:

- Laying day.park's furniture and making far-tables.park and unfurnished.park: scenarios, src/scenarios/day_parks.cpp behind day_parks.h, which already makes every day park.
- Writing the two new parks: scenarios, src/scenarios/main.cpp's writeSliceParks. This is composition only: it passes two more of makeDayParks's saves to the files it already writes.
- The world checks' new edit: tests/sim/park_world_checks_test.cpp, which holds every chosen edit.

## Tasks

### Task 1: Spec the parks

Files:
- Modify: `src/scenarios/SPEC.md`

Step 1: Make the changes FEATURE.md's Spec changes gives for src/scenarios/SPEC.md, word for word. In the paragraph after the bullets, add far-tables.park and unfurnished.park to the list of output files, after evening.park.

### Task 2: Declare the interface

Files:
- Modify: `src/scenarios/day_parks.h`

Step 1: Add `inline constexpr double DAY_TABLE_SPACING = 6.0;` and `inline constexpr double DAY_BENCH_SPACING = 10.0;` after EVENING_TICKS, each with a one-line comment: the meters between day.park's tables, and between its benches. Add `std::string FarTables;` and `std::string Unfurnished;` to DayParks after Evening. Change the struct's comment to "The day-at-the-park slice's four day parks and the somewhere-to-sit slice's two, as saves." Change makeDayParks's comment to:

"Lays out day.park on the new-park template, furnishes it, and saves it. Saves it with its toilet block deleted as NoToilet, with its tables moved to the arm toward the drink shop as FarTables, and with no furniture as Unfurnished. Saves it stepped NOON_TICKS and EVENING_TICKS cycles as Noon and Evening. Throws std::logic_error when the world refuses a command, a shop is starved, or a shop or toilet block has no guest connector."

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c -e warning -e error`
Expected: `0`

### Task 3: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/scenarios/SPEC.md, src/sim/guests/SPEC.md, src/sim/furniture/SPEC.md, and src/sim/park/SPEC.md as specs, and src/sim/park/edits.h, src/sim/furniture/furniture.h, and src/scenarios/day_parks.h as headers. Its changes to the world checks fail until Task 6 remakes noon.park: seat-service is held by no checked world, and the new edit finds no benches to displace.

### Task 4: Furnish day.park

Files:
- Modify: `src/scenarios/day_parks.cpp`

Step 1: In layOut, delete the AddBox of the bench at (3, 113). Change layOut's comment: the toilet block, not "the bench and the toilet block", faces the path in.

Step 2: Add a helper `EntityKey guestSegmentJoining(const World &world, ParkPoint a, ParkPoint b)`. It returns the key of the guest segment in parkSegments whose From and To points, positioned by parkPathPoints, are at a and b in either order, and throws std::logic_error naming both points when there is none. day.park's points are exact, so compare positions with ==.

Step 3: Add a helper `FurnishedSide sideAwayFrom(const World &world, EntityKey segment, ParkPoint away)`. With d the segment's To position less its From position, and t the position `away` less its From position, `away` lies on the segment's left when d.Z × t.X − d.X × t.Z is greater than 0, since a direction's left is (d.Z, −d.X) (src/sim/park/surface.cpp, leftOf). Return Right when it lies on the left, and Left otherwise.

Step 4: Add `void furnish(World &world, EntityKey segment, Furnishing to)` that applies SetFurnishing{segment, to} with applyAccepted, naming the segment's key.

Step 5: Name the two arms and the path in by their points: the path in joins (0, 123) and (0, 100), the arm toward the food shop joins (−50, 100) and (0, 100), and the arm toward the drink shop joins (0, 100) and (50, 100). Give them constants beside TEMPLATE_SEGMENT's, as ParkPoint pairs. At the end of layOut, set the food arm's furnishing to tables on both sides, `SideFurnishing{FurnitureKind::Table, DAY_TABLE_SPACING}`, and the path in's furnishing to benches, `SideFurnishing{FurnitureKind::Bench, DAY_BENCH_SPACING}`, on the side sideAwayFrom gives for the toilet block's position (−5, 113) and none on the other.

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c -e warning -e error`
Expected: `0`

### Task 5: Make far-tables.park and unfurnished.park

Files:
- Modify: `src/scenarios/day_parks.cpp`

Step 1: In makeDayParks, after making NoToilet, make FarTables from `copyWorld(world)`: furnish the food arm with `Furnishing{}` and the drink arm with tables on both sides at DAY_TABLE_SPACING, resolve it, and save it. Then make Unfurnished from another copy: furnish the food arm and the path in with `Furnishing{}`, resolve it, and save it. Both come before the stepping to noon, so both start at 9:00 with no guests.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c -e warning -e error`
Expected: `0`

### Task 6: Write the parks and remake them

Files:
- Modify: `src/scenarios/main.cpp`
- Modify: `tests/parks/day.park`, `no-toilet.park`, `noon.park`, `evening.park`
- Create: `tests/parks/far-tables.park`, `tests/parks/unfurnished.park`

Step 1: In main.cpp, add FAR_TABLES and UNFURNISHED to the usage comment at the top and to writeSliceParks's comment. Change the argument count check to 14 and its message to "takes thirteen files". Add `{12, &dayParks.FarTables}` and `{13, &dayParks.Unfurnished}` to the written array, whose size becomes 11.

Step 2: Remake the parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios > /dev/null 2>&1; build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park tests/parks/day.park tests/parks/no-toilet.park tests/parks/noon.park tests/parks/evening.park tests/parks/far-tables.park tests/parks/unfurnished.park; echo $?; git status --short tests/parks`
Expected: `0`, then ` M` for day.park, no-toilet.park, noon.park, and evening.park, and `??` for far-tables.park and unfurnished.park. The other parks are rewritten byte for byte.

Step 3: Run the park world checks and the integration tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests > /dev/null 2>&1; build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]" | tail -2; build/windows-debug/tpj_integration_tests.exe | tail -2`
Expected: `All tests passed` from each. If a noon.park edit's REQUIRE fails, or seat-service is held by no checked world, noon.park lacks what criterion 2 needs. That is a Major deviation: report it. If an integration test over the day parks fails, report its numbers as a Major deviation.

### Task 7: Check what the parks hold

Files:
- Create: `build/seat-parks/build.sh`, `build/seat-parks/parkcheck.cpp` (scratch, not committed)

Step 1: Copy build/carried-meals/build.sh to build/seat-parks/build.sh. Write parkcheck.cpp. For each of day.park, far-tables.park, unfurnished.park, and noon.park in tests/parks, loaded and resolved, it prints:

- the tick and the guest count;
- each box by key and kind, and each entrance and path segment by key;
- for each segment with furniture, its key, and each side's kind and piece count from parkFurniture;
- for day.park and far-tables.park, `benches away from the toilet block` if every bench's pose from parkFurniture lies at X > 0, the side away from the toilet block at X = −5, and otherwise each bench's pose;
- for noon.park only, how many guests are seated (Activity Seated with a Seat), how many are carrying food (Carrying), and how many remember a full piece (some entry of FullPieces has a Piece other than NULL_KEY). FullPieces is a fixed-size array, so count its entries, not the array.

It ends with `keys match` if far-tables.park and unfurnished.park list the same boxes, entrances, and segments by key as day.park.

Step 2: Build it and run it.

Run: `cmake.exe --build --preset windows-release > /dev/null 2>&1; build/seat-parks/build.sh parkcheck && build/seat-parks/parkcheck.exe`
Expected: day.park at tick 0 with 0 guests and no bench box, its food arm with tables on both sides, and its path in with benches on one side only. The benches' poses from parkFurniture lie at X > 0, away from the toilet block. far-tables.park with tables on the drink arm instead and the same benches. unfurnished.park with no furnished segment. noon.park at tick 5400, with at least one guest seated, one carrying, and one remembering a full piece. Then `keys match`. If noon.park lacks a seated, carrying, or remembering guest, report it as a Major deviation.

### Task 8: Run the comparisons and the earlier checks

Files:
- Create: `build/seat-parks/daycheck.cpp` (scratch, not committed)

Step 1: Write daycheck.cpp from build/carried-meals/mealcheck.cpp and build/seat-rests/seatcheck.cpp. It does five things, printing each line named here.

- Comparisons: it steps day.park, far-tables.park, and unfurnished.park one day each, 43,200 ticks from 9:00. For each it counts the meals eaten seated and standing, from each guest's MealsEaten rising and its LastMeal's Seated, and reads departureTally's tiredness count at the end. It prints each park's seated, standing, seated share, and tiredness departures. Then it prints `meal comparison passed` if far-tables.park's seated share is lower than day.park's, and `tiredness comparison passed` if unfurnished.park's tiredness departures are higher than day.park's.
- Meals: over the day.park run, it checks each meal with mealcheck's checkMeal, and the ledger each tick as mealcheck's run does. It prints `meals: <n> standing, <n> seated, <n> wrong` and `ledger: <n> wrong`.
- Full pieces: over the day.park run, each tick, it reads each guest's FullPieces, counting only entries whose Piece isn't NULL_KEY. guestRecord already leaves out a piece once FULL_MEMORY_TIME has passed, so the record alone can't show a piece kept too long. Check what the memory does instead:
  - While a piece is in the record, it is never among the guest's guestOptions. Count each option that names a remembered piece as wrong.
  - When a piece leaves the record before FULL_MEMORY_TIME has passed since it was found, count it as forgotten early. Count it wrong unless the guest found a new piece full in that tick and the piece it forgot was the one it found first.
  - When a piece leaves the record at the tick FULL_MEMORY_TIME has passed, check that the guest can be offered it again: in a later tick where the field the guest scores seats from, best meal seats while it carries food and best rest seats otherwise, gives the piece at its place, guestOptions gives the piece as an option. Count each tick it doesn't as wrong.
  It prints `full pieces: <n> turned away, <n> forgotten early, <n> expired, <n> wrong`.
- Options: it steps noon.park 600 ticks. Each tick, for every guest, it reads guestOptions and checks every seat option as seatcheck and mealcheck do: its terms add up to its score, and its relief and distance match the field where the guest stands. It prints `seat options: <n> checked, <n> wrong`.
- Idle seats: over the day.park run, it counts each time a guest starts heading to a rest seat (its Activity becomes HeadingToSeat with TargetNeed Tiredness), and how many of those started with tiredness under 0.1. It prints `rest seat picks: <n>, under 0.1 tiredness: <n>`.

Step 2: Build it and run it.

Run: `build/seat-parks/build.sh daycheck && build/seat-parks/daycheck.exe`
Expected: `meal comparison passed`, `tiredness comparison passed`, and 0 wrong on every line. Report every printed number. If a comparison fails, stop and give the numbers to the user, who names any tuning values to change for Task 10. If a check prints a wrong count, find the cause before going on: a scratch program's mistake is fixed in the program, and a simulation bug is fixed in the code now.

### Task 9: Capture noon.park

Files: none in the repository.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/noon.park --capture build/seat-parks/noon.bmp; echo $?`
Expected: `0`. Look at the capture: tables along both sides of the arm toward the food shop and benches along one side of the path in, with guests sitting at the tables beside the food shop and on the benches. Report what it shows.

### Task 10: Play noon.park and set any tuning values the user names

Files:
- Modify, only if the user names values: the header that holds each one, every park in tests/parks that holds guests, and `tests/parks/stress/full.park`

Step 1: Build the whole windows-release preset. Open tests/parks/noon.park once in build/windows-release/ThemeParkJones.exe for the user. Give the user Task 8's numbers, including the idle seat picks, and ask them to watch noon.park at 1x and 4x with the guest Inspector. They should see what the milestone's criteria 2, 3, and 6 describe: guests walking to a piece and sitting, a guest turned away choosing again, a carrying guest's options, and the full pieces it remembers. Then ask them to make two edits, each with a guest selected in the Inspector:

- Set the furnishing of the arm toward the food shop to benches while a guest is heading to a table there. The guest drops its target and chooses again at once, and its Inspector shows the new choice.
- Make the same kind of edit while a guest sits at a table. The guest is drawn standing where it was until its visit comes back, then takes its rest, and eats if it carried food, and walks on. Ask whether noon.park reads well, and which tuning values, if any, to change.

Step 2: If the user names none, skip to Task 11.

Step 3: Run scripts/runtime-report.sh for a before report on main's full.park, unless the last after report in build/carried-meals/after.report still applies because main is unchanged since. Set the named values. Remake the slice parks as Task 6 Step 2 does, and the full park with `build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park` after building tpj_bench with windows-release. Run Task 6 Step 3, Task 7, and Task 8 again, and an after runtime report. Report each comparison's numbers and the report's changed stages to the user.

### Task 11: Confirm the specs and the full checks

Files:
- Modify, only where the reading finds a mismatch: that SPEC.md

Step 1: Read src/sim/guests/SPEC.md beside src/sim/guests/guests.cpp and guests.h, and src/legible/SPEC.md's guest Inspector section beside src/legible/inspect.cpp. Read src/scenarios/SPEC.md beside day_parks.cpp and main.cpp. Read the sections seat-rests and carried-meals changed in src/sim/operations/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, src/sim/furniture/SPEC.md, and src/sim/wayfinding/SPEC.md beside their code. `git show f81366f 63ba53a -- '*SPEC.md'` shows which sections those are. Fix each sentence that doesn't describe the code, and list each fix in the report. This is milestone criterion 9.

Step 2: Run the full checks.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

### Task 12: Commit

Stage only this feature's paths by pathspec: the plan directory, src/scenarios, tests/sim/park_world_checks_test.cpp, tests/parks, and any spec or header Tasks 10 and 11 changed. Commit with the commit-hygiene skill, subsystem `Scenarios:`.
