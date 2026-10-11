# Implementation Plan: Day Panels

## Goal

Add a Departures panel of today's departures by reason, and draw the arrival curve for the shown hours in the Hours panel, read from a candidate with only the chosen hours, with the time of day marked.

## Approach

legible gains arrivals.h. keepArrivalDay samples arrivalCurve at each game minute, of the world when it has the hours and otherwise of previewEdit's candidate for a SetOpeningHours. It makes the day again only when the hours change, since the curve reads only the hours. ToolingUi keeps one KeptArrivalDay and passes it to the Hours panel, which draws the curve and the time mark with ImGui's draw list. A new Departures panel draws departureTally's five counts and their total.

## Placement

Decision 0027 places each behavior this feature adds:

- Reading the arrival curve through the day from a candidate with the chosen hours: legible, arrivals.h. legible explains the simulation from what it publishes, and it is the only module that makes candidates for display (previewEdit).
- Keeping the arrival day between frames: app, ToolingUi in tooling_ui.h. It already holds the panels' state across frames, such as the Light panel's preview.
- Drawing the curve and the time mark: app, hours_panel.h. It already draws the Hours panel.
- Drawing the departures: app, a new departures_panel.h. Each panel has its own component in src/app/ui.

## Tasks

### Task 1: Record the simulation before the change

Files: none

Step 1: Build and record. tpj_scenarios runs headless and opens no window.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/day-panels && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/day-panels/scenarios-before.txt; echo $?`
Expected: the build has no warnings, then `0`.

### Task 2: Update the legible spec

Files:
- Modify: `src/legible/SPEC.md:17-21` (Boundary)
- Modify: `src/legible/SPEC.md`, after the Inspectors section

Step 1: Make the two edits FEATURE.md's Spec changes give for src/legible/SPEC.md: the arrivals.h bullet and the Arrivals section.

### Task 3: Update the app spec

Files:
- Modify: `src/app/SPEC.md:69` (the Hours panel's paragraph)

Step 1: Make the two edits FEATURE.md's Spec changes give for src/app/SPEC.md: the sentences in the Hours panel's paragraph, and the Departures panel's paragraph after it.

### Task 4: Add arrivals.h with a stub

Files:
- Create: `src/legible/arrivals.h`
- Create: `src/legible/arrivals.cpp`
- Modify: `src/legible/CMakeLists.txt:3-7`

Step 1: Create src/legible/arrivals.h with the guard TPJ_LEGIBLE_ARRIVALS_H and the declarations FEATURE.md's public interface gives, in namespace tpj. Include `sim/clock.h` (DAY_SECONDS), `sim/park/intent.h` (OpeningHours), `sim/world.h`, `<array>`, `<optional>`, and `<stddef.h>`.

Step 2: Create src/legible/arrivals.cpp, including `legible/arrivals.h`, with keepArrivalDay returning false.

Step 3: Add `arrivals.cpp` to tpj_legible's sources, after `availability.cpp`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: builds with no warnings.

### Task 5: Sample the arrival curve

Files:
- Modify: `src/legible/arrivals.cpp`

Step 1: Include `legible/preview.h`, `sim/guests/guests.h`, and `sim/park/edits.h`.

Step 2: Implement keepArrivalDay.

- If `kept.Hours == hours`, return false.
- Otherwise, if `parkHours(world) == hours`, read from the world. Else make `const Preview preview = previewEdit(world, ParkEdit{SetOpeningHours{hours}})` and read from `previewedWorld(world, preview)`.
- For each sample i from 0 to ARRIVAL_SAMPLES - 1, set `kept.Day[i] = arrivalCurve(read, i * 60)`.
- Set `kept.Hours = hours` and return true.

Check how ParkEdit is built from SetOpeningHours in src/app/input/interaction.cpp:106-107 and use the same form.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: builds with no warnings.

### Task 6: Draw the curve in the Hours panel

Files:
- Modify: `src/app/ui/hours_panel.h`
- Modify: `src/app/ui/hours_panel.cpp`

Step 1: In hours_panel.h, include `legible/arrivals.h` and `<stdint.h>`, and change the declaration to `HoursUse drawHoursPanel(const HoursControls &hours, const ArrivalDay &curve, uint64_t second);`. Add the sentence FEATURE.md gives to its comment, before "Call between".

Step 2: In hours_panel.cpp, include `sim/guests/guests.h` and `<algorithm>`. In the anonymous namespace add `void drawCurve(const ArrivalDay &curve, uint64_t second)`:

- Reserve an area 240 by 60 pixels with `ImGui::Dummy`, and take its corners from `ImGui::GetItemRectMin()` and `ImGui::GetItemRectMax()`.
- Fill it with the color of ImGuiCol_FrameBg, through `ImGui::GetColorU32`.
- The peak is the largest Y of ARRIVAL_CURVE.
- Draw a polyline through ARRIVAL_SAMPLES + 1 points with `AddPolyline`, in the color of ImGuiCol_PlotLines, 1.5 pixels thick. Point i lies at i / ARRIVAL_SAMPLES of the width, and its height is `curve[i % ARRIVAL_SAMPLES] / peak` of the area's height, measured up from its bottom. The last point, at 24:00, takes the 00:00 sample.
- Draw a vertical line across the area at `second / DAY_SECONDS` of the width, in the color of ImGuiCol_PlotLinesHovered.
- Below the area, write "00:00" at its left, "12:00" at its middle, and "24:00" at its right edge, using `ImGui::SameLine(offset)` with offsets from `ImGui::CalcTextSize`.

Above the area, write "Arrivals per hour at each entrance".

Step 3: In drawHoursPanel, after the Apply button and its EndDisabled, call `drawCurve(curve, second)`.

Step 4: Compile the panel alone. tpj_app itself stops building at tooling_ui.cpp's call until Task 8.

Run: `cmake.exe --build --preset windows-debug --target src/app/CMakeFiles/tpj_app.dir/ui/hours_panel.cpp.obj`
Expected: compiles with no warnings.

### Task 7: Add the Departures panel

Files:
- Create: `src/app/ui/departures_panel.h`
- Create: `src/app/ui/departures_panel.cpp`
- Modify: `src/app/CMakeLists.txt:26`

Step 1: Create departures_panel.h with the guard TPJ_APP_UI_DEPARTURES_PANEL_H, including `sim/guests/guests.h`, with the declaration FEATURE.md gives in namespace tpj.

Step 2: Create departures_panel.cpp. drawDeparturesPanel:

- With ImGuiCond_FirstUseEver, set the next window's position to `(display width - 12, 240)` with pivot `(1, 0)`, at the top right below the Debug panel.
- Begin the window "Departures" with ImGuiWindowFlags_AlwaysAutoResize.
- Draw a two-column table, "departures", with ImGuiTableFlags_SizingFixedFit. Give one row for each reason in HOME_REASONS order, labeled "Hunger", "Thirst", "Tiredness", "Toilet", and "Closing" from a constant array in that order, with the count from `tally.ByReason`. Then add a "Total" row with their sum.
- End the window.

Step 3: Add `ui/departures_panel.cpp` to tpj_app's sources, after `ui/debug_panel.cpp`.

Step 4: Compile the panel alone.

Run: `cmake.exe --build --preset windows-debug --target src/app/CMakeFiles/tpj_app.dir/ui/departures_panel.cpp.obj`
Expected: compiles with no warnings.

### Task 8: Keep the arrival day and draw both panels

Files:
- Modify: `src/app/ui/tooling_ui.h`
- Modify: `src/app/ui/tooling_ui.cpp:24-63` (drawPanels)

Step 1: In tooling_ui.h, include `legible/arrivals.h`, and add a private member `KeptArrivalDay Arrivals;` after `LightPreview Light;`. In build's comment, change "the Debug, Time, Hours, and Tools panels" to "the Debug, Time, Hours, Departures, and Tools panels", and after "and its Apply applies them." add "It draws the arrival curve for the hours it shows, kept between frames, and marks the time of day."

Step 2: In tooling_ui.cpp, include `app/ui/departures_panel.h`. Give drawPanels a `KeptArrivalDay &arrivals` parameter after `GameSpeed &speed`, and pass `Arrivals` from build.

Step 3: In drawPanels, after `const OpeningHours hours = parkHours(world);`, take `const OpeningHours shown = interaction.hoursChoice().value_or(hours);`. Call `keepArrivalDay(arrivals, world, shown)`, pass `HoursControls{hours, shown}`, and pass `arrivals.Day` and `parkTime(world.Tick).Second` to drawHoursPanel. After the Hours panel's Apply handling, call `drawDeparturesPanel(departureTally(world))`.

Step 4: Update drawPanels' comment: after "the interaction's chosen hours from the Hours panel and applying them," add "the arrival curve for the hours it shows, the Departures panel,".

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug`
Expected: builds with no warnings.

### Task 9: Capture the panels

Files: none

Step 1: Build the release.

Run: `cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

Step 2: Capture.

Run: `build/windows-release/ThemeParkJones.exe --park tests/parks/noon.park --speed 0 --capture build/day-panels/noon.bmp; echo $?; build/windows-release/ThemeParkJones.exe --park tests/parks/evening.park --ticks 3000 --speed 0 --capture build/day-panels/evening.bmp; echo $?`
Expected: `0` twice.

Step 3: Look at both captures. In noon.bmp, the Hours panel shows the curve at zero before 09:00, a hump, and zero again before 21:00, with the mark at 12:00 inside the hump. In evening.bmp, the Departures panel shows a count under Closing and a Total equal to the sum of the rows, and the mark sits past the hump.

### Task 10: Play the criteria

Files: none

Step 1: Play criteria 1 to 3 on `build/windows-release/ThemeParkJones.exe --park tests/parks/day.park`.

- At 4x through closing, watch the counts rise and the mark move.
- Press + on Closes and see the curve change before Apply.
- Choose the bench tool and hover a placement, and see the curve stay on the chosen hours.
- Press Apply, and see the curve stay where it is.

### Task 11: Confirm the checks

Files: none

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports no findings.

Step 2: Compare the simulation.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/day-panels/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/day-panels/scenarios-before.txt build/day-panels/scenarios-after.txt; echo $?`
Expected: `0`

The feature adds no per-tick work. Its candidate is made once for each change of the shown hours. Run no runtime report.

### Task 12: Commit

Files: every file above, and this feature's plan files.

Step 1: Commit with the commit-hygiene skill, staging only this feature's paths. Subject: `App: Add the Departures panel and an arrival curve`.
