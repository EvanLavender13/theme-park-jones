# Implementation Plan: Arrival Curve

## Goal

Admit guests at each connected entrance by one keyed draw a tick at the arrival curve's rate, publish the curve through arrivalCurve, and remake the parks.

## Approach

guests.cpp's needCurve loop becomes a private curveAt over any table of CurvePoints, which needCurve and the new arrivalCurve share. arrivalCurve reads the park's hours and gives ARRIVAL_CURVE at the fraction of them that has passed, or 0 while closed. admitGuests turns that rate into a chance per tick and admits a guest at each connected entrance whose keyed draw falls below it.

## Placement

Decision 0027 places each behavior this feature adds:

- Arrivals by the curve, and arrivalCurve: guests, src/sim/guests/guests.cpp, published in src/sim/guests/guests.h. The guests module already admits guests in admitGuests, and only it creates them.
- The shared curve lookup: guests, a private function in src/sim/guests/guests.cpp. Only guests' curves use it.
- Warm-up lengths of the remade parks: scenarios, src/scenarios/slice_parks.h, which owns the slice parks.

No change touches src/app/main.cpp or src/scenarios/main.cpp.

## Tasks

### Task 1: Record the full park's baseline timings

Files: none in the repository.

Step 1: Build the whole windows-release preset of main as it stands (0cd57ce), and time the full park.

Run: `cmake.exe --build --preset windows-release > /dev/null 2>&1; build/windows-release/tpj_bench.exe tests/parks/stress/full.park > build/predict-check/arrival_curve_bench_before.txt; echo $?`
Expected: `0`. The file holds a park line and one stage line for each stage.

### Task 2: Update the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md:22,26`

Step 1: In line 22, change "(parkGuests, addGuest, guestRecord, guestOptions, and departureTally)" to "(parkGuests, addGuest, guestRecord, guestOptions, departureTally, and arrivalCurve)".

Step 2: In line 26, replace the first sentence, "Guests arrive at regular intervals at each entrance connected to the guest network, one per entrance at a time.", with:

    Guests arrive at random at each entrance connected to the guest network, at the rate the arrival curve gives for the time of day. In each tick, each such entrance admits at most one guest, by a draw keyed on the entrance and the tick, so the same world and seed always admit the same guests (principle 8).

Step 3: Add this paragraph after line 26:

    arrivalCurve(world, second) gives the arrival curve: the mean number of guests each connected entrance admits per game hour at that second of the day. It follows the park's opening hours. A few guests arrive at opening, the most come in the first third of the hours, fewer follow, and none arrive over the last part of the hours or while the park is closed. If the park is open all day, the curve runs over the whole day from the opening time. It depends only on the opening hours, so on a candidate with other hours it gives the curve those hours would admit guests by (decision 0025). It changes nothing and draws nothing.

### Task 3: Publish the arrival curve

Files:
- Modify: `src/sim/guests/guests.h:134-135,215-216`
- Modify: `src/sim/guests/guests.cpp:979-992`

Step 1: In guests.h, after the ARRIVAL_INTERVAL constant (Task 4 removes it), add:

    // The arrival curve: guests per game hour at each entrance, at fractions of the opening hours
    // that have passed. A few come at opening, the most from 0.15 to 0.3, and none from 0.75.
    inline constexpr std::array<CurvePoint, 6> ARRIVAL_CURVE{
        {{0.0, 5.0}, {0.15, 15.0}, {0.3, 15.0}, {0.5, 6.0}, {0.75, 0.0}, {1.0, 0.0}}};

Step 2: After departureTally's declaration, declare `double arrivalCurve(const World &world, uint64_t second);` with the comment: "The rate, in guests per game hour, at which each entrance connected to the guest network admits guests at the second of the day, below DAY_SECONDS, under the park's opening hours: ARRIVAL_CURVE at the fraction of the hours passed since opening, or 0 while the park is closed. If the park is open all day, the fraction runs over the whole day from the opening time. Reads only the hours, and draws nothing."

Step 3: In guests.cpp's anonymous namespace, add `double curveAt(std::span<const CurvePoint> curve, double x)`, commented "The authored curve's value at x, clamped to its first and last X, linear between its points.", holding needCurve's body with NEED_CURVE replaced by curve and the level by x. Make needCurve return `curveAt(NEED_CURVE, level)`. The operations stay the same, so needCurve's values don't change.

Step 4: Define arrivalCurve after departureTally. With `hours = parkHours(world)`: return 0.0 when `!isOpenAt(hours, second)`. Otherwise `span` is DAY_SECONDS when hours.Opening equals hours.Closing, and `(hours.Closing + DAY_SECONDS - hours.Opening) % DAY_SECONDS` otherwise; `passed` is `(second + DAY_SECONDS - hours.Opening) % DAY_SECONDS`; return `curveAt(ARRIVAL_CURVE, static_cast<double>(passed) / static_cast<double>(span))`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 4: Admit guests by the curve

Files:
- Modify: `src/sim/guests/guests.h:134-135`
- Modify: `src/sim/guests/guests.cpp:50-51,646-659`
- Modify: `src/scenarios/slice_parks.h:13-16`

Step 1: In guests.h, remove ARRIVAL_INTERVAL and its comment.

Step 2: In guests.cpp's anonymous namespace, after DEPARTURES_PURPOSE, add `constexpr uint64_t ARRIVAL_PURPOSE = hashName("guest-arrival");`, commented "The purpose an entrance's arrival draw is keyed with.", and `constexpr double HOUR_SECONDS = 3600.0;`, commented "The game seconds in a game hour."

Step 3: Rewrite admitGuests. With `rate = arrivalCurve(world, parkTime(world.Tick).Second)`, return when rate is not above 0. Otherwise `chance = rate * static_cast<double>(GAME_SECONDS_PER_TICK) / HOUR_SECONDS`. For each entrance with anchored nodes on the network, as today, call `addGuest(world, network.nodePlace(anchored.front()))` when `drawUniform(drawKey(world, entrance.Key, ARRIVAL_PURPOSE, 0)) < chance`. Change its comment to "Each entrance with a guest connector admits a guest through addGuest when its draw for the tick falls below the chance of an arrival in a tick at arrivalCurve's rate."

Step 4: In slice_parks.h, remove the sentence of MAIN_STREET_WARM_TICKS's comment that starts "It isn't a multiple of ARRIVAL_INTERVAL". Arrivals are now drawn, so the last cycle may admit a guest.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 5: Remake the parks

Files:
- Modify: `tests/parks/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`, `stocked.park`, `stress/full.park`
- Modify: `src/scenarios/slice_parks.h` and `src/scenarios/SPEC.md:43`, only as Steps 3 and 4 say

Step 1: Remake the slice parks.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park; echo $?`
Expected: `0`. stocked.park's hours start at 10:00 and it is made before then, so it now holds no guests, and its departure tally is 0 for every reason.

Step 2: Build the whole windows-release preset and remake the full park, which takes about 18 s.

Run: `cmake.exe --build --preset windows-release > /dev/null 2>&1; build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

Step 3: Run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests > /dev/null 2>&1; build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]" | tail -2`
Expected: `All tests passed`. Fewer arrivals can leave warm.park with no guest waiting at its shop or heading to it, and then the chosen edits' REQUIREs fail. If so, raise WARM_TICKS in src/scenarios/slice_parks.h by 300, update its comment's seconds (ticks divided by 30), and repeat Steps 1 and 3 until they pass. (Deviation, minor: WARM_TICKS became 4600. Between 2500 and 4300 the checks failed either for want of those guests or because no shipment was in transit, which warm.park also needs.)

Step 4: Count warm-main-street's guests by activity.

Run: `grep -o "activity=[a-z-]*" tests/parks/warm-main-street.park | sort | uniq -c`
Expected: a count for each of heading-home, heading-to-facility, waiting, and wandering. If one is missing, raise MAIN_STREET_WARM_TICKS by 900, update its comment's seconds (ticks divided by 30), and repeat Steps 1, 3, and 4. src/scenarios/SPEC.md line 43 then stays as it is.

### Task 6: Check arrivals on main-street.park and the full park

Files:
- Create: `build/predict-check/arrival_check.cpp` (scratch, not committed)

Step 1: Write a scratch program, in the shape of build/predict-check/going_home_check.cpp, that takes a mode and a park path.

In modes `default` and `late`, it loads the park and resolves it. In mode `late`, it then applies SetOpeningHours of 13:00 to 1:00 (46800 and 3600) and resolves. It first checks arrivalCurve, comparing exactly:

- `default`: 5 at 9:00, 15 at 12:00, 6 at 15:00, 0 at 18:00, 0 at 21:00, and 0 at 3:00.
- `late`: 5 at 13:00, 15 at 16:00, 6 at 19:00, 0 at 22:00, 0 at 0:30, and 0 at 9:00.
- In mode `default` only, it makes a candidate with makeCandidate and SetOpeningHours of 13:00 to 1:00, and checks that the candidate gives the `late` values (Deviation, minor: World is move-only, and a candidate is what the Hours panel will read) and the world still gives the `default` values.

It then steps three days (129,600 ticks). Before each step, at tick t, it adds `arrivalCurve(world, parkTime(t).Second) * 2.0 / 3600.0` to the prediction for that clock hour, `parkTime(t).Second / 3600`. If t is above 0 and the park is open at t but wasn't at t - 1, it checks that the park holds no guests. After each step, it counts the guests whose Arrived is t, adds them to that hour's count, and fails if any arrived while arrivalCurve was 0. At the end, for each clock hour, it prints the count and the prediction, and fails if they differ by more than 4 times the square root of the prediction, plus 1.

In mode `full`, it loads the park, resolves it, and counts its guests. It steps 3,600 ticks, DEFAULT_TICKS, counting the guests whose Arrived is the tick before each step. It prints the guests at the start, the arrivals, the guests at the end, and the departure tally.

Step 2: Build it against the release library, because debug can't step three days or the full park in reasonable time, and release simulates identically. Run each mode.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O2 arrival_check.cpp ../windows-release/src/sim/libtpj_sim.a -o arrival_check.exe && ./arrival_check.exe default ../../tests/parks/main-street.park && ./arrival_check.exe late ../../tests/parks/main-street.park`
Expected: each run prints 24 hours with their counts and predictions, about 79 arrivals a day in all, with 0 in every hour the park is closed or in its last quarter, then `arrival check passed`.

Run: `cd build/predict-check && ./arrival_check.exe full ../../tests/parks/stress/full.park`
Expected: arrivals above 0, and guests at the end at least 1,900 of the 2,000 or so at the start.

### Task 7: Time the full park after the change

Files: none in the repository.

Run: `build/windows-release/tpj_bench.exe tests/parks/stress/full.park > build/predict-check/arrival_curve_bench_after.txt; echo $?; paste <(awk '/^stage/{print $2, $6}' build/predict-check/arrival_curve_bench_before.txt) <(awk '/^stage/{print $6}' build/predict-check/arrival_curve_bench_after.txt)`
Expected: `0`, then each stage's name with its median in nanoseconds before and after. Every stage's median stays below one tick, 33,333,333 ns. Report the medians in the feature's report.

### Task 8: Run the full checks and play

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

Step 2: Play. Open tests/parks/warm-main-street.park in build/windows-release/ThemeParkJones.exe. Its clock reads about 11:30, in the morning rush, and the Debug panel's guest count rises. In the Hours panel, set Opens to 17:00 and Closes to 12:00 and apply, then run at 4x. Guests walk about 78 m a game hour, so five closed hours let the farthest of them reach the entrance. After 12:00, watch the count fall to 0 as every guest walks out, and stay at 0 until 17:00. After 17:00, watch guests arrive again.

Step 3: Add to the Deepening candidates of plans/scalable-runtime/CAPABILITY.md: "- full.park holds its crowd through hours of play: its 30 counters serve about 600 meals and drinks a game hour, well below what its 2,000 guests want, so thirst sends them home within a few game hours. Give it enough counters for its guests. Gated on: a run long enough to show it, such as play or a longer runtime report."

### Task 9: Commit

Stage only this feature's paths: the files of Tasks 2 to 5, plans/scalable-runtime/CAPABILITY.md, and plans/believable-guests/a-day-out/arrival-curve/. Commit with the commit-hygiene skill, subject "Sim: Admit guests by the arrival curve over the opening hours".
