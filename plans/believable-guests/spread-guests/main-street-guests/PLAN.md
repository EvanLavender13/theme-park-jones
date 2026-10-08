# Implementation Plan: Main Street Guests

## Goal

--slice-parks makes tests/parks/warm-main-street.park, full.park is made again, the slice's capture of guests on the main street is taken, and the runtime report after it is kept as the good-paths baseline.

## Approach

slice_parks gains MAIN_STREET_WARM_TICKS and a function that loads a park, resolves it, steps it that many cycles with no commands, and saves it. tpj_scenarios' --slice-parks takes main-street and warm-main-street after its three files and writes warm-main-street with it. The parks are then made with the tools as they are, and checked with the scratch checks from sidestep and paving-walk. This feature's RESEARCH.md gives the reasons for the warm-up and the command's form.

## Placement

Decision 0027 places each behavior this feature adds:

- Warming main-street.park and MAIN_STREET_WARM_TICKS: scenarios, src/scenarios/slice_parks.h with slice_parks.cpp, which already makes the slice parks from fed.park.
- --slice-parks taking main-street and warm-main-street: src/scenarios/main.cpp, which already reads --slice-parks' files and writes what makeSliceParks gives. It adds two more paths to that command and nothing else, so it is composition only.
- Making full.park again: tpj_bench --full-park, run as it is.
- The presence, stays, and cost checks: scratch programs in build/sidestep/ and build/paving-walk/, never committed.

## Tasks

### Task 1: Describe warm-main-street.park in the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md` (Slice parks)

Step 1: Make the three edits FEATURE.md's Spec changes gives.

### Task 2: Warm main-street.park

Files:
- Modify: `src/scenarios/slice_parks.h`
- Modify: `src/scenarios/slice_parks.cpp`

Step 1: In slice_parks.h, after WARM_TICKS, add MAIN_STREET_WARM_TICKS = 4530, with the comment: "The warm-up tests/parks/warm-main-street.park takes from main-street.park: 151 s of game time, long enough that guests arrive and leave about equally often, so they fill the park in every activity. It isn't a multiple of ARRIVAL_INTERVAL, so no guest arrives on the last cycle, and every guest that isn't waiting holds its guest presence entry."

Step 2: Declare `std::string makeWarmPark(std::string_view text, uint64_t warmTicks);` with the comment: "Loads the text with makeParkSchema, resolves it, steps it warmTicks cycles with no commands, and gives its save. Throws LoadError as loadWorld does."

Step 3: Define it in slice_parks.cpp with the same calls makeSliceParks makes for Warm.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 3: Make --slice-parks write warm-main-street

Files:
- Modify: `src/scenarios/main.cpp` (the usage comment at the top, and writeSliceParks)

Step 1: In the usage comment, change the --slice-parks line to `tpj_scenarios --slice-parks FED WARM CUT MAIN_STREET WARM_MAIN_STREET`.

Step 2: In writeSliceParks, require six arguments, and change the message to `--slice-parks takes five files`. Read MAIN_STREET, arguments[4], as FED is read, with the same message if it can't be read. Make the warm park with `tpj::makeWarmPark(*mainStreet, tpj::MAIN_STREET_WARM_TICKS)` inside the same try as makeSliceParks, reporting a LoadError with MAIN_STREET's path. Write it to WARM_MAIN_STREET, arguments[5], after the other three, with the same message if it can't be written. Never write MAIN_STREET. Update writeSliceParks' comment to name the new files.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 4: Make the slice parks

Files:
- Create: `tests/parks/warm-main-street.park`

Step 1: Run the command on tests/parks.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park; echo "status $?"; git status --short tests/parks`
Expected: `status 0`, and the only line for tests/parks is `?? tests/parks/warm-main-street.park`. The simulation hasn't changed, so the other parks come out the same.

Step 2: Check that it holds guest presence, and its guests' activities.

Run: `build/sidestep/build.sh held && build/sidestep/held.exe tests/parks/warm-main-street.park && grep -o 'activity=[a-z-]*' tests/parks/warm-main-street.park | sort | uniq -c`
Expected: the presence line reports entries above 0, equal to the guests that aren't waiting. The counts list wandering, heading-to-shop, waiting, and heading-home guests, about 50 to 60 in all. If one activity is missing, report it before going on: the warm-up is too short or too long, and MAIN_STREET_WARM_TICKS needs another value.

Deviation, minor: the park holds 64 guests, a few above the 50 to 60 expected: 16 wandering, 22 heading to a shop, 10 waiting, and 16 heading home, with 54 presence entries for the 54 that aren't waiting. Every activity is there, so MAIN_STREET_WARM_TICKS stays 4530.

Step 3: Check that its guests stay on the paving.

Run: `build/paving-walk/build.sh stays && build/paving-walk/stays.exe tests/parks/warm-main-street.park`
Expected: the line ends in `0 guests off the paving`.

### Task 5: Capture warm-main-street.park

Files:
- Create, scratch: `build/main-street-guests/warm-main-street.bmp`, `build/main-street-guests/warm-main-street.png`

Step 1: Capture it with the windows-debug app.

Run: `mkdir -p build/main-street-guests && cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/warm-main-street.park --capture build/main-street-guests/warm-main-street.bmp; echo "status $?"`
Expected: `status 0`, and the BMP exists.

Step 2: Convert it to PNG to look at it.

Run: `python3 -c "from PIL import Image; Image.open('build/main-street-guests/warm-main-street.bmp').save('build/main-street-guests/warm-main-street.png')"`
Expected: no output.

Step 3: Look at the PNG against criterion 3: one paved surface with rounded inside corners, edge lines only along the outline, square free ends, shops in rows facing the street, and guests spread across the main street. The app frames the whole park when it opens one and has no camera option, so judge criterion 3 on that framing. Task 4's stays check shows every guest is on the paving. If the guests are too small to see at that framing, say so in the report, so the user can judge it in the app at the slice's close.

### Task 6: Make full.park again

Files:
- Modify: `tests/parks/stress/full.park`

Step 1: Remake it.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench && build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park; echo "status $?"`
Expected: `status 0`.

Step 2: Check that it holds guest presence.

Run: `build/sidestep/build.sh held && build/sidestep/held.exe tests/parks/stress/full.park`
Expected: entries above 0, and no more than the guests that aren't waiting, of about 2,000 guests.

### Task 7: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 8: Commit and merge

Files: none.

Step 1: Stage only this feature's paths: src/scenarios/, tests/parks/warm-main-street.park, tests/parks/stress/full.park, and plans/believable-guests/spread-guests/main-street-guests/. Commit them by pathspec with the commit-hygiene skill, subject `Scenarios: Make warm-main-street.park with the slice parks`. Merge into main as implementing-features says, and rebuild windows-release.

### Task 9: Take the good-paths baseline

Files:
- Modify: `plans/believable-guests/spread-guests/main-street-guests/PLAN.md` (this task's record)

Step 1: Make the runtime report on main.

Run: `scripts/runtime-report.sh --launches 3 build/good-paths-baseline.report`
Expected: the script prints its report, with `over-budget 0 of` and the run count. Keep the file as the baseline.

Step 2: Run the cost check on the new full.park, built against the windows-release library as in sidestep.

Run: `R='C:/Users/EvanUhhh/source/repos/ThemeParkJones'; (cd build/sidestep && /mnt/c/msys64/ucrt64/bin/g++.exe -I$R/src -isystem $R/.cpm-cache/entt/1d2c/src -std=c++20 -ffp-contract=off -O2 -DNDEBUG cost.cpp "$R/build/windows-release/src/tools/libtpj_tools.a" "$R/build/windows-release/src/sim/libtpj_sim.a" -o cost.exe) && build/sidestep/cost.exe`
Expected: two lines. The first gives the sampling cost with the lookup already built. The second gives the tick, the sampling cost, the guests, and the mean and most entries per sample, and it goes into the Baseline note. The mean is a few entries, far below the number of guests.

Step 3: Add a "Baseline" note under this task with full.park's medians for windows-release resolution, preview, ticks, and the app's frames, the over-budget count, and the cost check's line. Commit it on main by pathspec with the subject `Plans: Record the good-paths runtime baseline`.

Baseline, taken on main at fdaddc0 with scripts/runtime-report.sh --launches 3 and kept as build/good-paths-baseline.report. full.park medians, in µs, with their ranges:

- windows-release resolution 6101.1 [5773.3 6129.3]
- windows-release preview 9039.0 [8444.7 9055.5]
- windows-release ticks 6830.9 [6811.9 6858.1]
- windows-release-app frames 4451.3 [4424.9 4464.4]
- windows-release-app-overlay frames 5284.4 [5248.0 5553.6]
- over-budget 0 of 25

The cost check on the new full.park: `tick 7831 us, sampling every guest 615 us, 2031 guests, mean 3.7 entries per sample, most 18`. A guest's sample finds a few of the 2,031 guests, so its work grows with the guests near it, which confirms ground-fields' deferred criterion 7.
