# Implementation Plan: Light Preview

## Goal

Add a Light panel and a --light-hour option that light the scene at a chosen time of day without changing the park's clock.

## Approach

A new tooling UI component, light_panel, holds a LightPreview (on, and a second of the day) and draws the Light panel: a Preview checkbox and an integer slider over the minutes of the day, shown as HH:MM. ToolingUi holds the preview, as it holds the shown views and the speed, and draws the panel each frame. The application starts the preview from --light-hour and lights each frame with dayLightAt(preview second) while the preview is on, and with dayLightOf(world) otherwise.

## Placement

Decision 0027 places each behavior this feature adds:

- The preview's state and the Light panel: app, a new tooling UI component src/app/ui/light_panel.h, held by ToolingUi (src/app/ui/tooling_ui.h). The tooling UI owns the panels and the UI state they set, as it owns ShownViews and GameSpeed.
- Reading --light-hour: app, the options component (src/app/options.h), which owns the command line.
- Lighting the frame by the preview or the clock: app, src/app/application.cpp. This is composition only. The application already hands drawFrame the world's light each frame.

## Tasks

### Task 1: Record the before state

Files: none in the repository. Outputs go to build/light-preview/.

Step 1: On the feature branch, before any change, build windows-debug.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings.

Step 2: Record the simulation's output.

Run: `mkdir -p build/light-preview && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/light-preview/scenarios-before.txt`
Expected: exit status 0.

### Task 2: Describe the Light panel and --light-hour in the app's spec

Files:
- Modify: `src/app/SPEC.md`

Step 1: Make the edits that FEATURE.md's Spec changes lists for src/app/SPEC.md, word for word: the last sentence under "## The scene", the new Light panel paragraph after the Hours panel's paragraph under "## Tooling UI", the --light-hour item after the --overlay item under "## Command line", and the new usage reason after the --speed reason.

### Task 3: Add the option's field

Files:
- Modify: `src/app/options.h`

Step 1: Add to Options, after Speed:

    // The hour --light-hour starts the Light panel's preview at, if it was given.
    std::optional<uint64_t> LightHour;

Step 2: In parseOptions's comment, add after the --overlay sentence: "--light-hour H starts with the Light panel's preview at H:00." Add "a --light-hour value that is not a whole hour from 0 to 23" to the list of refusals after the --overlay item, and add --light-hour to the options that --hash refuses.

### Task 4: Read --light-hour

Files:
- Modify: `src/app/options.cpp`

Step 1: In parseOptions's loop, add a branch after the --overlay branch. If argv[i] is "--light-hour" and hasValue, read argv[++i] with parseCount into a uint64_t hour. The option is valid when parseCount succeeds and hour is less than 24. Set options.LightHour to hour.

Step 2: Add `options.LightHour.has_value()` to the conditions that make --hash invalid.

Step 3: Add `[--light-hour H]` to the usage line, after `[--overlay food]`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: builds with no warnings.

### Task 5: Add the Light panel

Files:
- Create: `src/app/ui/light_panel.h`
- Create: `src/app/ui/light_panel.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: In light_panel.h, under the include guard TPJ_APP_UI_LIGHT_PANEL_H, include <stdint.h> and declare in namespace tpj:

- `struct LightPreview { bool On = false; uint64_t Second = 0; };`, with the comment "The Light panel's preview: whether it lights the scene, and the second of the day it shows."
- `void drawLightPanel(uint64_t clockSecond, LightPreview &preview);`, with the comment "Draws the Light panel: a Preview checkbox, which sets preview.On, and an Hour slider showing preview.Second's time of day as HH:MM, in whole minutes. While the preview is off, the slider follows clockSecond. Dragging the slider turns the preview on. Call between ImGui::NewFrame and ImGui::Render."

Step 2: In light_panel.cpp, define drawLightPanel, following time_panel.cpp's shape:

- Place the window at the bottom right on first use: position (DisplaySize.x - 12, DisplaySize.y - 12) with pivot (1, 1) and ImGuiCond_FirstUseEver, with a comment that it is clear of the Inspector at the bottom left.
- Begin "Light" with ImGuiWindowFlags_AlwaysAutoResize.
- If the preview is off, set preview.Second to clockSecond, so turning it on starts from the light on screen.
- Draw `ImGui::Checkbox("Preview", &preview.On)`.
- Take the minute of the day, preview.Second / 60, as an int. Write it as `%02d:%02d` hours and minutes into a small char array with snprintf. Draw `ImGui::SliderInt("Hour", &minute, 0, DAY_SECONDS / 60 - 1, text, ImGuiSliderFlags_NoInput)`, with DAY_SECONDS from sim/clock.h. Comment that ImGui shows a format with no conversion as it is. NoInput keeps Ctrl+click from parsing that text.
- If the slider changed, set preview.On to true and preview.Second to the minute times 60.
- End the window.

Step 3: Add `ui/light_panel.cpp` to tpj_app's sources in src/app/CMakeLists.txt, after `ui/inspector_window.cpp`.

### Task 6: Hold and draw the preview in the tooling UI

Files:
- Modify: `src/app/ui/tooling_ui.h`
- Modify: `src/app/ui/tooling_ui.cpp`

Step 1: In tooling_ui.h, include app/ui/light_panel.h. Give the constructor a third parameter, `LightPreview light`, stored in a new member `LightPreview Light;` after Speed. Add `[[nodiscard]] const LightPreview &lightPreview() const { return Light; }` after speed(). Add the Light panel to the class comment ("and the Light panel's preview, which its checkbox and slider set") and to build's comment ("the Light panel, which sets the preview").

Step 2: In ToolingUi::build, right after the drawPanels call, call `drawLightPanel(parkTime(session.world().Tick).Second, Light);`.

### Task 7: Light the frame by the preview

Files:
- Modify: `src/app/application.cpp`

Step 1: In the Application constructor's initializer list, construct Ui with a third argument, `LightPreview{options.LightHour.has_value(), options.LightHour.value_or(0) * 3600}`.

Step 2: In runFrames, before the drawFrame call, after `lastFrame`, take the frame's light, with the comment "While the Light panel previews a time, it lights the frame in place of the clock.":

    const LightPreview &preview = Ui.lightPreview();
    const DayLight light = preview.On ? dayLightAt(preview.Second) : dayLightOf(Session.world());

Pass `light` to drawFrame in place of `dayLightOf(Session.world())`.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: builds with no warnings.

### Task 8: Check the option's refusals

Step 1: Check each refusal.

Run: `build/windows-debug/ThemeParkJones.exe --light-hour 24; echo $?; build/windows-debug/ThemeParkJones.exe --light-hour x; echo $?; build/windows-debug/ThemeParkJones.exe --park tests/parks/noon.park --light-hour 5 --hash; echo $?`
Expected: three times, the usage line ending in `[--frame-times]` and including `[--light-hour H]`, then `1`.

### Task 9: Capture the hours and compare them

Files: none in the repository. Outputs go to build/light-preview/.

Step 1: Build the whole release preset.

Run: `cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

Step 2: Make each capture with build/windows-release/ThemeParkJones.exe and `--park tests/parks/noon.park --speed 0`:

- no --light-hour: build/light-preview/noon.bmp
- --light-hour 12: build/light-preview/noon-12.bmp
- --light-hour 18: build/light-preview/noon-18.bmp
- --light-hour 23: build/light-preview/noon-23.bmp
- --overlay food --light-hour 12: build/light-preview/food-12.bmp
- --overlay food --light-hour 23: build/light-preview/food-23.bmp

Expected: each exits with status 0.

Step 3: For criterion 3, write a mask of the pixels where noon.bmp and noon-12.bmp differ, with a scratch python3 script using PIL in the scratchpad, to build/light-preview/diff-noon.png. Read the mask and noon-23.bmp.
Expected: the differing pixels lie in the Debug panel's frame-rate line and the Light panel's checkbox. noon-23.bmp shows the noon world at night.

Step 4: For criterion 5, take the overlay's shape as the pixels where food-12.bmp differs from noon-12.bmp. Within it, write a mask of the pixels where food-12.bmp differs from food-23.bmp to build/light-preview/diff-food.png, and print their count. Read the mask.
Expected: the differing pixels lie only in the Debug and Light panels, or on paving farther than 250 m from the eye.

Step 5: For criterion 6, read noon-12.bmp and noon-18.bmp around the guests.
Expected: each guest keeps its hue at 18:00 and is darker.

### Task 10: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports no findings.

Step 2: Compare the simulation with Task 1's output.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/light-preview/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/light-preview/scenarios-before.txt build/light-preview/scenarios-after.txt; echo $?`
Expected: `0`

Step 3: Rebuild the whole release preset, then ask the user to play criteria 1 and 2 in build/windows-release/ThemeParkJones.exe on day.park.

Run: `cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

### Task 11: Commit

Step 1: Stage only this feature's paths and commit with the commit-hygiene skill, for example "App: Add a Light panel that previews any hour".
