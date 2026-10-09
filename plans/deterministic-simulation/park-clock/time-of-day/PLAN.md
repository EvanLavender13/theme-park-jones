# Implementation Plan: Time of Day

## Goal

parkTime gives a tick's day and time of day, and the app shows them in a Time panel, with the simulation unchanged.

## Approach

sim/clock.h declares the clock's constants, ParkTime, and parkTime, and clock.cpp computes the day and second from the tick with unsigned 64-bit sums, a division, and a remainder. A new app UI component, time_panel, draws the Time panel from a ParkTime, and tooling_ui calls it with parkTime of the session world's tick. A scenarios comparison from before and after shows the simulation unchanged. This feature's RESEARCH.md gives the reasons for whole-number seconds and the display.

## Placement

Decision 0027 places each behavior this feature adds:

- The clock: sim root, src/sim/clock.h with clock.cpp. deterministic-simulation owns the tick cycle and World::Tick in the sim root, and the clock is a pure function of the tick that every capability may read. cmake/layers.txt puts sim/* in the lowest sim layer, so every sim module and the app can include it.
- The Time panel: app, a new component src/app/ui/time_panel.h with time_panel.cpp, beside the Debug panel and the Tools panel, which each have their own component. tooling_ui already builds every panel of the frame, so it calls the new one. Nothing is added to src/app/main.cpp.

## Tasks

### Task 1: Record the simulation before the change

Files:
- Create, scratch: `build/time-of-day/before.txt`

Step 1: Build tpj_scenarios on main as it is, and write its output for every checked-in park.

Run: `mkdir -p build/time-of-day && cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/*.park tests/parks/stress/winding-path.park > build/time-of-day/before.txt; echo "status $?"`
Expected: a timing line for each scenario and each park on the terminal, then `status 0`.

### Task 2: Describe the clock in the sim spec

Files:
- Modify: `src/sim/SPEC.md` (Boundary, and a new section after The tick cycle)

Step 1: Make the two edits to src/sim/SPEC.md that FEATURE.md's Spec changes gives.

### Task 3: Describe the Time panel in the app spec

Files:
- Modify: `src/app/SPEC.md` (Tooling UI)

Step 1: Add the paragraph FEATURE.md's Spec changes gives, after the Debug panel's paragraph.

### Task 4: Add the clock

Files:
- Create: `src/sim/clock.h`
- Create: `src/sim/clock.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: In src/sim/clock.h, with the include guard TPJ_SIM_CLOCK_H and `#include <stdint.h>`, in namespace tpj, declare:

- `inline constexpr uint64_t GAME_SECONDS_PER_TICK = 2;` with the comment "The game seconds each tick adds to the clock, so at 1x, SIM_TICK_SECONDS of real time, one game minute passes each real second."
- `inline constexpr uint64_t DAY_SECONDS = 86400;` with the comment "The game seconds in a day."
- `inline constexpr uint64_t DAY_TICKS = DAY_SECONDS / GAME_SECONDS_PER_TICK;` with the comment "The ticks in a day: 43200."
- `inline constexpr uint64_t FIRST_SECOND = 9 * 3600;` with the comment "The second of day 1 that tick 0 falls on: 9:00."
- `struct ParkTime { uint64_t Day = 1; uint64_t Second = 0; bool operator==(const ParkTime &) const = default; };` with the comment "A moment of the park's clock: the day, counted from 1, and the second of that day, from 0 to DAY_SECONDS - 1." Comment each field.
- `ParkTime parkTime(uint64_t tick);` with the comment "The park's time at a tick. Tick 0 is FIRST_SECOND of day 1, each tick adds GAME_SECONDS_PER_TICK, and a new day starts at midnight. Whole numbers only, so every build agrees."

Step 2: In src/sim/clock.cpp, define parkTime: let `seconds = FIRST_SECOND + tick * GAME_SECONDS_PER_TICK`, and return `{seconds / DAY_SECONDS + 1, seconds % DAY_SECONDS}`.

Step 3: Add `clock.cpp` to tpj_sim's sources in src/sim/CMakeLists.txt, in alphabetical order before `cycle.cpp`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 5: Add the Time panel

Files:
- Create: `src/app/ui/time_panel.h`
- Create: `src/app/ui/time_panel.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/ui/tooling_ui.h` (ToolingUi::build's comment)
- Modify: `src/app/ui/tooling_ui.cpp` (drawPanels)

Step 1: In src/app/ui/time_panel.h, with the include guard TPJ_APP_UI_TIME_PANEL_H and `#include "sim/clock.h"`, declare `void drawTimePanel(const ParkTime &time);` with the comment "Draws the Time panel: the park's day and its time of day on a 24-hour clock, in hours and minutes. Call between ImGui::NewFrame and ImGui::Render."

Step 2: In src/app/ui/time_panel.cpp, define it as drawDebugPanel in src/app/ui/debug_panel.cpp is laid out. Place the window, with ImGuiCond_FirstUseEver, at the window's top center: `ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 12.0f)` with pivot `ImVec2(0.5f, 0.0f)`. Begin it as "Time" with ImGuiWindowFlags_AlwaysAutoResize. Inside, write `ImGui::Text("Day %llu  %02llu:%02llu", ...)` with the day, `time.Second / 3600`, and `time.Second % 3600 / 60`, each cast to `unsigned long long`. End the window.

Step 3: Add `ui/time_panel.cpp` to tpj_app's sources in src/app/CMakeLists.txt, in alphabetical order after `ui/snap_guides.cpp`.

Step 4: In src/app/ui/tooling_ui.cpp, include `app/ui/time_panel.h`, and in drawPanels call `drawTimePanel(parkTime(world.Tick));` right after drawDebugPanel. Add "the Time panel," to drawPanels' comment and to ToolingUi::build's comment in tooling_ui.h, where they list the panels.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 6: Check the panel by capture

Files:
- Create, scratch: `build/time-of-day/noon.bmp`, `build/time-of-day/midnight.bmp`, and their PNGs

Step 1: Capture main-street.park at tick 5400 and tick 27000.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --ticks 5400 --capture build/time-of-day/noon.bmp; echo "status $?"; build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --ticks 27000 --capture build/time-of-day/midnight.bmp; echo "status $?"`
Expected: `status 0` twice. The second run steps for a minute or two before its window opens.

Step 2: Convert both to PNG and look at them.

Run: `python3 -c "from PIL import Image; [Image.open(f'build/time-of-day/{n}.bmp').save(f'build/time-of-day/{n}.png') for n in ('noon', 'midnight')]"`
Expected: no output. noon.png's Time panel reads `Day 1  12:00`, and midnight.png's reads `Day 2  00:00`. Each sits at the top center, clear of the Tools and Debug panels. If imgui.ini in the working directory holds a saved Time position, the panel sits there instead. Then move imgui.ini aside for the capture and put it back after.

Step 3: Play main-street.park in the app for a few seconds.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park`
Expected: the Time panel starts at `Day 1  09:00`, and its minutes advance by one about each real second.

### Task 7: Confirm the simulation is unchanged

Files:
- Create, scratch: `build/time-of-day/after.txt`

Step 1: Write the same output with this feature's tpj_scenarios, and compare.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/*.park tests/parks/stress/winding-path.park > build/time-of-day/after.txt && build/windows-debug/tpj_scenarios.exe --compare build/time-of-day/before.txt build/time-of-day/after.txt; echo "status $?"`
Expected: `status 0`.

### Task 8: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 9: Commit and merge

Files: none.

Step 1: Stage only this feature's paths: src/sim/clock.h, src/sim/clock.cpp, src/sim/CMakeLists.txt, src/sim/SPEC.md, src/app/ui/time_panel.h, src/app/ui/time_panel.cpp, src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp, src/app/CMakeLists.txt, src/app/SPEC.md, and plans/deterministic-simulation/park-clock/time-of-day/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Add the park clock and the Time panel`. Merge into main as implementing-features says, and rebuild windows-release.
