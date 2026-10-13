# Feature: Quiet Panels

## Summary

quiet-panels makes the panels' data reuse its memory from frame to frame, and adds it to the allocation check. That data is the Debug panel's guest count, the Departures panel's tally, the Hours panel's arrival curve, the Inspector's inspection of a guest, shop, bench, or toilet block, with a shop's stock setting, the need tooltip's availability, and the Graph view's lines and nodes. Today the app works most of it out again each frame into new lists and new strings.

After this feature, a new PanelData in tpj_app_core works out that data into lists it keeps, and ToolingUi draws what it gives. guestOptions keeps its lists in TickBuffers the caller keeps, and shopRecord gains a form that does. inspectSubject, availabilityAt, overlayPlace, and buildGraphOverlay fill what the caller keeps, in place of what they return today, since the panels are their only callers. sim/park gains parkBox and furniture gains furniturePiece, which look up one box or piece by its key, so the Inspector, facilityRecord, and a shop's stock setting copy no list of boxes or pieces. departureTally stops making a list. The app keeps frame times only while --frame-times is on. The shop ghost's tooltip only reads the kept preview's shop context, which kept-preview already made allocation-free and the check already holds. What the player sees doesn't change. This covers criterion 7 of the milestone, and criterion 9's edits to docs/testing.md and docs/conventions.md, which land with this last feature.

## Acceptance criteria

1. The allocation check runs the panels' data as the app works it out, at every warm-up and watched tick, for four held subjects: the guest in the middle of the park's guests, the first shop, the first bench, and the first toilet block. For each, it gives the guest count, the departure tally, the arrival day, for the park's hours with the guest and the bench and for hours that open an hour later with the shop and the toilet block, the subject's inspection, the shop's stock setting, each need's availability at the middle of the first guest segment on the food overlay's shape, and the Graph view's overlay for the default camera in a 1280 by 720 window. Freed blocks are reported as `the panels`. If nothing is freed, the check prints `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, and the panels freed no block they allocated` and exits with status 0. (check: in PanelData::guestCount, in src/app/ui/panel_data.cpp, add `const std::vector<uint64_t> held{world.Tick};` and `static_cast<void>(held);`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that start `allocation check: the panels freed`; then revert)
2. Once its lists have grown, the panels' data frees no block it allocated over the check's watched ticks of full.park, for each kind of subject. (ctest)
3. Without --frame-times, the app keeps no frame times. With it, the line at exit is as before. (check: read Application::runFrames in src/app/application.cpp and see FrameTimes::add called only while --frame-times is on; run `build/windows-debug/ThemeParkJones.exe --frames 10 --frame-times` and see one line, `frame-times` followed by 8 numbers)
4. The panels and tooltips show what they showed before. (play, in the windows-release app on tests/parks/stress/full.park: see the Debug panel's guest count and the Departures panel's counts change as the park runs; step the Hours panel's Opens and see the curve follow; inspect a guest and see its rows and choices update as it walks, sits, and eats; inspect a shop, choose Drink, and see Apply enabled; inspect a bench and a toilet block; check Graph and see the networks' lines and nodes; choose the Food overlay and hover paving to see the tooltip's facilities; with Place shop, hover beside guest paving and see the footfall and supply route tooltip)
5. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md task 18)
6. docs/testing.md and docs/conventions.md describe the check as decision 0041 does: it covers all the work the app repeats in steady play except uploading meshes, drawing, ImGui's own drawing, and the previews that resolve a new world. Decision 0041 says what a feature adds to the check. (check: read the three files)
7. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: the three commands in PLAN.md task 20)

## Tests

None. The allocation check covers the work, and criterion 1's check proves it sees it.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows. The panels read the world's records, route distance, and offers as they do today.

## Spec changes

src/sim/guests/SPEC.md:

- At the end of the paragraph that starts "departureTally(world) gives the departure tally", add "It allocates nothing."
- Replace "guestOptions(world, key) returns the options the guest would weigh if it chose now." with "guestOptions(world, key, buffers, options) fills options, a list the caller keeps, with the options the guest would weigh if it chose now, and returns true. If the key holds no guest or its place doesn't resolve on the guest network, it leaves options empty and returns false. It keeps the lists it works with in TickBuffers the caller keeps, and allocates nothing once they have grown."

src/sim/operations/SPEC.md:

- After the paragraph that starts "shopRecord(world, key) returns a shop's inspection record", add "shopRecord can also keep the lists it works with in TickBuffers the caller keeps, and then allocates nothing once they have grown."
- At the end of the paragraph that starts "facilityRecord(world, key) returns a facility's inspection record", add "It allocates nothing."

src/sim/park/SPEC.md:

- At the end of the paragraph that starts "Other modules read intent only through", add "Other modules also read one box through parkBox, which gives the box a key holds, as parkBoxes gives it, or none when the key holds no box. It allocates nothing."

src/sim/furniture/SPEC.md:

- In Boundary, replace "Other modules read the furniture through parkFurniture and furnitureSize," with "Other modules read the furniture through parkFurniture, furniturePiece, and furnitureSize,".
- After "parkFurniture gives every piece, in key order.", add "furniturePiece gives the piece a key holds, as parkFurniture gives it, or none when the key holds no piece. It allocates nothing."

src/legible/SPEC.md:

- Replace "availabilityAt(world, definition, place) gives the value and each source's contribution:" with "availabilityAt(world, definition, place, lists, availability) fills an Availability the caller keeps with the value and each source's contribution:", and at the end of that paragraph add "It uses AvailabilityLists the caller keeps, and allocates nothing once they have grown."
- After "inspectSubject reads the subject's record afresh at each call, so if an inspector is shown every frame, it follows every tick. It says when the subject is gone.", add "It fills an Inspection the caller keeps, using InspectLists the caller keeps. Once they have grown, filling it again for the same subject allocates nothing while the park only ticks."

src/render/SPEC.md:

- Replace "which overlayPlace(shape, point) gives," with "which overlayPlace(shape, point, lines) gives,", and at the end of that paragraph add "overlayPlace uses a list the caller keeps, and allocates nothing once it has grown."
- At the end of the paragraph that starts "buildGraphOverlay gives both networks' carriers and nodes", add "It fills a GraphOverlay the caller keeps, using a list of keys the caller keeps, and allocates nothing once they have grown."

src/app/SPEC.md:

- After the paragraph that starts "The Inspector shows its subject's title and rows.", add "Once their lists have grown, the panels' data frees no memory it allocated from frame to frame: the guest count, the departure tally, the arrival curve, the Inspector's inspection and a shop's stock setting, the need tooltip's availability, and the Graph view's lines and nodes. That holds for each kind of subject. Drawing the panels with ImGui is outside it. The allocation check holds a stretch of full.park's ticks to it, with a guest, a shop, a bench, and a toilet block inspected."
- At the end of the --frame-times item, add "Without --frame-times, the app keeps no frame times."

docs/testing.md, docs/conventions.md, and docs/decisions/0041-allocation-check.md: see PLAN.md task 2.

## Files affected

- Modify: `src/sim/guests/guests.h`, `src/sim/guests/guests.cpp`, `src/sim/guests/SPEC.md`
- Modify: `src/sim/operations/operations.h`, `src/sim/operations/operations.cpp`, `src/sim/operations/facilities.cpp`, `src/sim/operations/SPEC.md`
- Modify: `src/sim/park/intent.h`, `src/sim/park/intent.cpp`, `src/sim/park/SPEC.md`
- Modify: `src/sim/furniture/furniture.h`, `src/sim/furniture/furniture.cpp`, `src/sim/furniture/SPEC.md`
- Modify: `src/legible/inspect.h`, `src/legible/inspect.cpp`, `src/legible/availability.h`, `src/legible/availability.cpp`, `src/legible/SPEC.md`
- Modify: `src/render/paving_overlay.h`, `src/render/paving_overlay.cpp`, `src/render/graph_overlay.h`, `src/render/graph_overlay.cpp`, `src/render/SPEC.md`
- Create: `src/app/ui/panel_data.h`, `src/app/ui/panel_data.cpp`
- Modify: `src/app/CMakeLists.txt`, `src/app/ui/tooling_ui.h`, `src/app/ui/tooling_ui.cpp`, `src/app/ui/need_tooltip.h`, `src/app/ui/need_tooltip.cpp`, `src/app/ui/graph_view.h`, `src/app/ui/graph_view.cpp`, `src/app/application.cpp`, `src/app/SPEC.md`
- Modify: `tests/checks/allocations/allocation_check.cpp`
- Modify: `docs/testing.md`, `docs/conventions.md`, `docs/decisions/0041-allocation-check.md`

## Dependencies

- quiet-tick: the allocation check and TickBuffers. Merged.
- kept-preview: the kept preview's shop context and the check's held overlays, whose shape the tooltip reads. Merged.
- quiet-tools: the check's held tools, which run before the panels. Merged.

## Out of scope

- ImGui's own drawing of the panels, as decision 0041 says.
- Picking a subject, which runs on a click, not in steady play.
- The need tooltip on a preview's candidate while an overlay is shown. The candidate is made again at each tick, which changing-previews takes. The tooltip's own lists are reused on it all the same.

## Open questions

None.
