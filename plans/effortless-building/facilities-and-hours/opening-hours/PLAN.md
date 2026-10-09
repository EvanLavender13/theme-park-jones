# Implementation Plan: Opening Hours

## Goal

The park has saved opening hours, 9:00 to 21:00 unless set, which the player previews, applies, undoes, and redoes from a new Hours panel, with every existing park unchanged.

## Approach

A new private component, opening-hours, sits on one entity of its own, which SetOpeningHours creates when it sets hours other than the default and destroys when it sets the default, so every older park saves as before. The history's IntentRecords gain the hours as one optional value beside the keyed lists, so an edit of them is recorded before and after and reversed by a RestoreIntent that sets them. The app's Interaction holds the Hours panel's choice beside the Inspector's chosen stock and gives it as the tentative edit when neither the tool nor the stock gives one. This feature's RESEARCH.md gives the reasons.

## Placement

Decision 0027 places each behavior this feature adds:

- The opening hours as intent, their save, and parkHours: sim/park, src/sim/park/intent.h with intent.cpp and internal/components.h. It owns park intent and its private components.
- SetOpeningHours, and RestoreIntent's hours: sim/park, src/sim/park/edits.h with edits.cpp, which owns every command that changes intent.
- Recording and reversing an hours change: tools, src/tools/history.cpp, which records and reverses every edit.
- The Hours panel's choice and its tentative edit: app input, src/app/input/interaction.h with interaction.cpp. It already holds the Inspector's chosen stock and gives the frame's tentative edit.
- The Hours panel's times, buttons, steps, and Apply: app UI, a new src/app/ui/hours_panel.h with hours_panel.cpp. The Time panel shows the clock and the speed, and explained-needs will add the arrival curve to the Hours panel, so the hours get a panel of their own.
- Passing the park's hours to the panel and the player's choice back to the interaction: app UI, src/app/ui/tooling_ui.cpp, which already draws the panels each frame. It adds calls only.
- Emptying the kept preview when the hours change while paused: app scene, src/app/scene/scene_sync.h, whose DrawnIntent already notices every other intent change.
- A park that holds hours: tests/parks/facilities.park, which the park world checks open.

## Tasks

### Task 1: Describe the hours in the specs

Files:
- Modify: `src/sim/park/SPEC.md` (Boundary, Intent, Commands, editedSegments, editedSurface, ParkEdit)
- Modify: `src/tools/SPEC.md` (History)
- Modify: `src/app/SPEC.md` (Tooling UI)
- Modify: `plans/effortless-building/facilities-and-hours/MILESTONE.md` (Deepening candidates)

Step 1: Make each edit FEATURE.md's Spec changes gives.

Step 2: Add to MILESTONE.md's Deepening candidates: "- Setting hours past midnight or all day from the Hours panel. SetOpeningHours already accepts them."

### Task 2: Declare the hours and SetOpeningHours

Files:
- Modify: `src/sim/park/intent.h` (after ParkBox, the addParkIntent comment, after parkBoxes' declaration)
- Modify: `src/sim/park/intent.cpp` (a stub parkHours)
- Modify: `src/sim/park/edits.h` (after SetShopStock, IntentRecords, RestoreIntent's comment, addParkEdits' comment, ParkEdit, isAccepted, applyCommand)
- Modify: `src/sim/park/edits.cpp` (stub definitions, planOf, addParkEdits)

Step 1: In intent.h, add after ParkBox:

    // The park's opening hours: the second of the day it opens and the second it closes, each less
    // than DAY_SECONDS. If closing is at or before opening, the hours run past midnight, and if the
    // two are equal, the park is open all day.
    struct OpeningHours {
      uint64_t Opening = 9ULL * 3600;
      uint64_t Closing = 21ULL * 3600;

      bool operator==(const OpeningHours &) const = default;
    };

    // The hours of a park with none set: 9:00 to 21:00.
    inline constexpr OpeningHours DEFAULT_HOURS{};

Change addParkIntent's comment to name entrance, path-point, path-segment, box, shop-stock, and opening-hours. After parkBoxes' declaration, declare `OpeningHours parkHours(const World &world);` with the comment "The park's opening hours, or DEFAULT_HOURS when none are set." In intent.cpp, define it to return DEFAULT_HOURS for now.

Step 2: In edits.h, after SetShopStock, add:

    // The park's opening hours.
    struct SetOpeningHours {
      OpeningHours Hours;

      bool operator==(const SetOpeningHours &) const = default;
    };

Give IntentRecords a last member `std::optional<OpeningHours> Hours;`, and change its comment to "Path points, segments, and boxes, each list in ascending key order unless its user says otherwise, and the park's opening hours, when the records hold them." Add to RestoreIntent's comment: "When Set holds hours, they become the park's hours, set after the adds. Add holds none." Add SetOpeningHours to ParkEdit between SetShopStock and RestoreIntent, and to addParkEdits' comment in the same place. Change ParkEdit's comment to "one of the fifteen commands". Declare `bool isAccepted(const World &world, const SetOpeningHours &command);` and `void applyCommand(World &world, const SetOpeningHours &command);` after SetShopStock's.

Step 3: In edits.cpp, define both as stubs: isAccepted returns false, and applyCommand does nothing. Add `std::optional<EditPlan> planOf(const World & /*world*/, const SetOpeningHours & /*command*/)` returning std::nullopt, after SetShopStock's planOf, and register the command in addParkEdits after SetShopStock.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

### Task 3: Write the test

Files: none by hand.

Step 1: Dispatch the test-writer as implementing-features says, with FEATURE.md, src/sim/park/SPEC.md and src/tools/SPEC.md, and the public headers src/sim/park/intent.h and src/sim/park/edits.h.

Step 2: Build and run the world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: the new chosen edit fails because the stub refuses it, and nothing fails to build.

### Task 4: Hold the hours as intent

Files:
- Modify: `src/sim/park/internal/components.h` (after ShopStockIntent's visitFields)
- Modify: `src/sim/park/intent.cpp` (addParkIntent, parkHours)

Step 1: In components.h, add after ShopStockIntent's visitFields:

    // The park's opening hours, held by one entity only when they are not DEFAULT_HOURS.
    struct OpeningHoursIntent {
      OpeningHours Hours;

      bool operator==(const OpeningHoursIntent &) const = default;
    };

with visitFields writing two fields, "opening" for Hours.Opening and "closing" for Hours.Closing.

Step 2: In addParkIntent, register `OpeningHoursIntent` as "opening-hours", DataKind::Intent, after shop-stock.

Step 3: parkHours gives the Hours of the first entity in key order that holds an OpeningHoursIntent, using collectIntent, or DEFAULT_HOURS when none does.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 5: Apply SetOpeningHours and restore hours

Files:
- Modify: `src/sim/park/edits.cpp` (include sim/clock.h, a helper in the anonymous namespace after setStock, namesValid, isAccepted and applyCommand for SetOpeningHours, applyCommand for RestoreIntent)

Step 1: Add after setStock, in the anonymous namespace:

- `bool isTimeOfDay(uint64_t second)`, true when second < DAY_SECONDS, and `bool areHoursValid(const OpeningHours &hours)`, true when both its seconds are times of day.
- `void setHours(World &world, const OpeningHours &hours)`, with the comment "Gives the park the hours: destroys the entity holding them for DEFAULT_HOURS, so a park at the default saves as one never set, and otherwise sets them on that entity, creating it if none holds them." It finds the first entity in key order holding an OpeningHoursIntent. For DEFAULT_HOURS it destroys that entity, if any. Otherwise it replaces that entity's hours, or creates an entity with world.createEntity() and emplaces an OpeningHoursIntent with the hours.

Step 2: isAccepted for SetOpeningHours: areHoursValid(command.Hours). applyCommand: when accepted, setHours(world, command.Hours).

Step 3: In namesValid, also return false when command.Add.Hours holds a value, or when command.Set.Hours holds hours that are not valid by areHoursValid.

Step 4: At the end of applyCommand for RestoreIntent, after the added boxes, call setHours(world, *command.Set.Hours) when Set holds hours. Setting them last keeps the keys of the records it adds where isRestoreAccepted and the history expect them.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 6: Record and reverse an hours change

Files:
- Modify: `src/tools/history.cpp` (recordsOf, changeBetween, isEmpty for IntentRecords, reversal)

Step 1: recordsOf also sets records.Hours to parkHours(world), and its comment becomes "The world's path points, segments, and boxes, each in key order, and its opening hours."

Step 2: changeBetween sets change.Before.Hours to was.Hours and change.After.Hours to is.Hours when the two differ, and leaves them empty otherwise.

Step 3: isEmpty for IntentRecords also requires Hours to be empty.

Step 4: reversal sets command.Set.Hours to change.Before.Hours.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build succeeds with no warnings.

### Task 7: Hold the Hours panel's choice

Files:
- Modify: `src/app/input/interaction.h`
- Modify: `src/app/input/interaction.cpp`

Step 1: Give Interaction a private `std::optional<OpeningHours> HoursChoice;`, and change the class comment to "The player's hold on the park: the tool, the guest or shop the Inspector shows, the stock chosen for that shop, and the hours chosen in the Hours panel, both not yet applied. It follows the park session's generation, so a replaced world drops the tool's hold, the subject, and both choices."

Step 2: Add these public members after settleStock, each with its comment:

- `[[nodiscard]] const std::optional<OpeningHours> &hoursChoice() const { return HoursChoice; }` — "The hours chosen in the Hours panel and not yet applied."
- `void chooseHours(OpeningHours hours) { HoursChoice = hours; }` — "Chooses the park's hours. They are applied only by applyHours."
- `void applyHours(CommandQueue &commands) const;` — "Queues a SetOpeningHours of the chosen hours, when hours are chosen. The choice stays until settleHours finds the park has them."
- `void settleHours(const World &world);` — "Drops the chosen hours once the park has them, so the choice stays shown until its SetOpeningHours applies."

Step 3: In interaction.cpp, follow drops HoursChoice. applyHours queues SetOpeningHours{*HoursChoice} when HoursChoice holds hours. settleHours resets HoursChoice when it equals parkHours(world).

Step 4: tentativeEdit gives the tool's edit, then the stock's, as now. Otherwise, when HoursChoice holds hours other than parkHours(world), it gives SetOpeningHours{*HoursChoice}. Otherwise none. Change its declaration's comment to "The tool's edit, or else the Inspector's chosen stock as a SetShopStock while it differs from the shop's, or else the Hours panel's chosen hours as a SetOpeningHours while they differ from the park's."

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 8: Draw the Hours panel

Files:
- Create: `src/app/ui/hours_panel.h`
- Create: `src/app/ui/hours_panel.cpp`
- Modify: `src/app/CMakeLists.txt` (tpj_app's sources, after ui/graph_view.cpp)

Step 1: In hours_panel.h, with the include guard TPJ_APP_UI_HOURS_PANEL_H and includes sim/park/intent.h and <optional>, declare:

    // The Hours panel's controls: the park's hours, and the hours shown, which the player chose.
    struct HoursControls {
      OpeningHours Current;
      OpeningHours Chosen;
    };

    // What the player did in the Hours panel: the hours they chose this frame, and whether they
    // pressed Apply.
    struct HoursUse {
      std::optional<OpeningHours> Chosen;
      bool Apply = false;
    };

    // Draws the Hours panel: Opens and Closes, Chosen's two times on a 24-hour clock, each with -
    // and + buttons that move it to the quarter hour before or after, and an Apply button below,
    // disabled while Chosen is Current. Opens runs from 00:00 to 23:45 and Closes from 00:15 to
    // 24:00, which is a closing second of 0. A + on Opens that brings it to or past Closes moves
    // Closes to a quarter hour after it, and a - on Closes that brings it to or before Opens moves
    // Opens to a quarter hour before it. Call between ImGui::NewFrame and ImGui::Render.
    HoursUse drawHoursPanel(const HoursControls &hours);

Step 2: In hours_panel.cpp, work in seconds with a quarter hour of 900. Treat a closing second of 0 as DAY_SECONDS, the end of the day, while stepping and showing, and turn DAY_SECONDS back into 0 for the result. The quarter hour before a time is the largest multiple of 900 below it, and the one after is the smallest multiple of 900 above it, so a time off the steps moves onto them.

- Opens -: disabled at 0. Otherwise Opens moves to the quarter hour before.
- Opens +: disabled when the quarter hour after is DAY_SECONDS or later. Otherwise Opens moves there, and when Closes, as an end time, is at or before the new Opens, Closes moves to Opens + 900.
- Closes -: disabled when the quarter hour before is below 900. Otherwise Closes moves there, and when the new Closes is at or before Opens, Opens moves to Closes - 900.
- Closes +: disabled when the quarter hour after is above DAY_SECONDS. Otherwise Closes moves there.

Draw at the window's top center, below the Time panel: `ImGui::SetNextWindowPos(ImVec2(DisplaySize.x * 0.5f, 100.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f))`, in a window "Hours" with ImGuiWindowFlags_AlwaysAutoResize. Each row is the label, the time as `%02llu:%02llu`, then the - and + buttons on the same line, each pair inside ImGui::PushID("opens") or PushID("closes") and wrapped in BeginDisabled and EndDisabled as above. A press sets use.Chosen to the stepped hours. Below the rows, Apply inside BeginDisabled(hours.Chosen == hours.Current) sets use.Apply.

Step 3: Add `ui/hours_panel.cpp` to tpj_app's sources in src/app/CMakeLists.txt, after ui/graph_view.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 9: Show the Hours panel each frame

Files:
- Modify: `src/app/ui/tooling_ui.cpp` (drawPanels)
- Modify: `src/app/ui/tooling_ui.h` (build's comment)

Step 1: In drawPanels, after drawTimePanel, call interaction.settleHours(world). Then draw the panel with HoursControls{parkHours(world), interaction.hoursChoice().value_or(parkHours(world))}. When the use has Chosen, call interaction.chooseHours with it. When it has Apply, call interaction.applyHours(session.commands()). Add "the Hours panel" to drawPanels' comment, which then reads: "Builds the Debug, Time, Hours, and Tools panels, setting the speed from the Time panel's buttons, the interaction's chosen hours from the Hours panel and applying them, the shown views from their checkboxes, selecting the tool ...", keeping the rest.

Step 2: In tooling_ui.h, change build's comment from "the Debug, Time, and Tools panels," to "the Debug, Time, Hours, and Tools panels,", and add after "the Time panel's speed buttons, which set the speed." the sentence "The Hours panel's buttons set the interaction's chosen hours, and its Apply applies them."

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 10: Empty the kept preview when the hours change

Files:
- Modify: `src/app/scene/scene_sync.h:69-77` (DrawnIntent)
- Modify: `src/app/scene/scene_sync.cpp` (syncWorld)

Step 1: Give DrawnIntent a last member `OpeningHours Hours;`, and change its comment to "The intent a park mesh was built from, and the opening hours, so a change of them while paused also empties the kept preview."

Step 2: In syncWorld, build current with parkHours(world) as its Hours.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 11: Add hours to facilities.park

Files:
- Modify: `tests/parks/facilities.park`

Step 1: Set next-key to 10, and add after the shop-stock section:

    [opening-hours]
    9 opening=36000 closing=79200

Step 2: Run the world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 12: Confirm the remade parks are unchanged

Files:
- Create, scratch: `build/opening-hours/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`

Step 1: Remake the slice parks with this feature's tpj_scenarios into a scratch directory, from a scratch copy of fed.park, which it writes back as loaded and saved again, and compare them all with the checked-in ones.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/opening-hours && cp tests/parks/fed.park build/opening-hours/fed.park && cd build/opening-hours && ../windows-debug/tpj_scenarios.exe --slice-parks fed.park warm.park cut.park ../../tests/parks/main-street.park warm-main-street.park && for f in fed warm cut warm-main-street; do cmp $f.park ../../tests/parks/$f.park && echo "$f same"; done; cd ../..`
Expected: `fed same`, `warm same`, `cut same`, and `warm-main-street same`.

### Task 13: Play the hours

Files:
- Create, scratch: `build/opening-hours/hours.bmp`

Step 1: Capture the app on facilities.park, and look at the capture.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/facilities.park --capture build/opening-hours/hours.bmp`
Expected: the Hours panel stands below the Time panel at the top center, apart from it, and shows Opens 10:00 and Closes 22:00 with Apply disabled. If it overlaps the Time panel, move its y in hours_panel.cpp and capture again.

Step 2: Play warm-main-street.park.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm-main-street.park`
Expected: The Hours panel shows Opens 09:00 and Closes 21:00 with Apply disabled. Pressing + on Opens shows 09:15 and enables Apply. Apply keeps 09:15 shown, and Apply is disabled once it applies. Ctrl+Z puts 09:00 back and Ctrl+Y 09:15 again. Pressing - on Closes down to 09:30 and once more moves Opens to 09:00 with Closes at 09:15. Pressing + on Closes up to 24:00 disables it. Pause with Space and repeat the Apply, which shows at once. Saving the park and opening the file shows the hours as they were, and starting a new park shows 09:00 and 21:00. Report what was seen, and leave the full check to the user's play.

### Task 14: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 15: Commit and merge

Files: none.

Step 1: Stage only this feature's paths: src/sim/park/, src/tools/history.cpp, src/tools/SPEC.md, src/app/input/interaction.h, src/app/input/interaction.cpp, src/app/ui/hours_panel.h, src/app/ui/hours_panel.cpp, src/app/CMakeLists.txt, src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp, src/app/scene/scene_sync.h, src/app/scene/scene_sync.cpp, src/app/SPEC.md, tests/parks/facilities.park, tests/sim/park_world_checks_test.cpp, plans/effortless-building/facilities-and-hours/MILESTONE.md, and plans/effortless-building/facilities-and-hours/opening-hours/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Let the player set the park's opening hours`. If the format hook rewraps a file after the commit, add it and amend the commit before merging. Merge into main as implementing-features says, and rebuild windows-release.
