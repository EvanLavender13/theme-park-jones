# Implementation Plan: Quiet Panels

## Goal

Make the panels' data free no block it allocated from frame to frame on full.park, and run it in the allocation check, with the panels and the simulation unchanged. Keep frame times only while --frame-times is on. Describe the finished check in docs/testing.md and docs/conventions.md.

## Approach

A new PanelData in tpj_app_core works out the panels' data into lists it keeps, and ToolingUi draws what it gives, so the check runs the app's own code. Below it, each function that makes a new list or string each call fills what the caller keeps instead: guestOptions and a new form of shopRecord keep their lists in TickBuffers, and inspectSubject, availabilityAt, overlayPlace, and buildGraphOverlay fill what the caller passes, in place of what they return today, since the panels are their only callers. New lookups by key, parkBox and furniturePiece, let the Inspector, facilityRecord, and a shop's stock setting find one box or piece without copying a list. inspectSubject writes its strings in place and reuses its rows.

## Placement

Decision 0027 places each behavior this feature adds:

- departureTally without a list, and guestOptions filling a kept list: sim/guests, guests.h. The guests module owns the tally and the options, and keeps its walk lists private behind TickBuffers.
- shopRecord keeping its lists, and facilityRecord reading one box: sim/operations, operations.h and facilities.cpp, which own the records and their route lengths.
- parkBox: sim/park, intent.h, which owns boxes and how other modules read them.
- furniturePiece: sim/furniture, furniture.h, which owns the pieces.
- inspectSubject filling a kept Inspection, with InspectLists: legible, inspect.h, which owns the inspection's text.
- availabilityAt filling a kept Availability: legible, availability.h, beside the forms that already take AvailabilityLists.
- overlayPlace using a kept list, and buildGraphOverlay filling a kept GraphOverlay: render, paving_overlay.h and graph_overlay.h, which own them.
- Working out the panels' data into kept lists: app, PanelData in the new app/ui/panel_data.h, compiled into tpj_app_core. The panels own their data, and the check links tpj_app_core but not tpj_app, which needs a window.
- ToolingUi, drawNeedTooltip, and drawGraphView drawing what PanelData gives: app/ui, in tpj_app. Composition only.
- Keeping frame times only with --frame-times: app, Application in application.cpp. Composition only.
- Running the panels' data in the check: tests/checks/allocations/allocation_check.cpp.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, build tpj_scenarios and record.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `mkdir -p build/quiet-panels && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-panels/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-panels/full-before.txt`
Expected: exit status 0.

### Task 2: Update the specs and the docs

Files:
- Modify: `src/sim/guests/SPEC.md`, `src/sim/operations/SPEC.md`, `src/sim/furniture/SPEC.md`, `src/legible/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`
- Modify: `docs/testing.md`, `docs/conventions.md`, `docs/decisions/0041-allocation-check.md`

Step 1: Make the SPEC.md edits FEATURE.md's Spec changes lists, word for word.

Step 2: In docs/testing.md, in The suite, replace "The allocation check in tests/checks finds allocations in the tick and in the display work the app repeats every tick (decision 0041)." with "The allocation check in tests/checks finds allocations in the work the app repeats in steady play: the tick, the scene's sync, the kept preview and the overlays, the tools, and the panels' data (decision 0041)."

Step 3: In docs/testing.md, in What a feature adds, after the paragraph that starts "If a feature registers state", add the paragraph "Don't add an allocation test for a feature. If a feature adds to the tick, the allocation check covers it once full.park holds what the feature adds. If it adds other work the app repeats in steady play, such as a panel's data or a tool's hover, add that work to the check in the same feature."

Step 4: In docs/conventions.md, in Memory in loops, replace "The allocation check in tests/checks enforces this rule for the tick and the display work repeated every tick (decision 0041)." with "The allocation check in tests/checks enforces this rule for all the work the app repeats in steady play. Only uploading meshes, drawing, ImGui's own drawing, and previews that resolve a new world stay outside it (decision 0041)."

Step 5: In docs/decisions/0041-allocation-check.md, in Consequences, replace "If a feature adds work to steady play, the check covers it once full.park holds what the feature adds, as keeping full.park current already requires." with "If a feature adds to the tick, the check covers it once full.park holds what the feature adds, as keeping full.park current already requires. If it adds other work to steady play, such as a panel's data or a tool's hover, the feature adds that work to the check."

### Task 3: Read the departure tally without a list

Files:
- Modify: `src/sim/guests/guests.cpp:1280-1286`

Step 1: In departureTally, replace `world.keysWith<Departures>()` with `world.forEachWith<Departures>`, keeping the first entity's ByReason, and give a zero tally when there is none.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0, no warning.

### Task 4: Fill a guest's options into a kept list

Files:
- Modify: `src/sim/guests/guests.h:252-256`, `src/sim/guests/guests.cpp:1229-1248`

Step 1: In guests.h, include `sim/tick_buffers.h` and replace the returning guestOptions, whose only caller is inspectSubject, with:

    // Fills options, which it clears first, with the options the guest would weigh if it chose now,
    // each with its terms, score, and softmax probability, and returns true. False, with options
    // empty, when the key holds no guest or its place does not resolve on the guest network. Keeps
    // the lists it works with in the buffers, and allocates nothing once they have grown. Draws
    // nothing and changes nothing; it costs one choice, so call it for one guest.
    bool guestOptions(const World &world, EntityKey guest, TickBuffers &buffers,
                      std::vector<ChoiceOption> &options);

Step 2: In guests.cpp, add a private struct GuestOptionLists next to GuestTickLists, holding `std::vector<ParkEntrance> EntranceValues`, `std::vector<EntityKey> Entrances`, and `WalkLists Walk`. guestOptions takes `buffers.get<GuestOptionLists>()`, samples route distance into `Walk.Routes` with the filling sampleField, calls parkMoment with the kept entrance lists, scoreOptions into `Walk.Scored`, and softmaxWeights into `Walk.ChoiceWeights`. It then copies `Walk.Scored.Options` into options with copyGrowing.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0, no warning.

### Task 5: Look up one box by its key

Files:
- Modify: `src/sim/park/intent.h:225`, `src/sim/park/intent.cpp:77-84`

Step 1: In intent.h, declare after the filling parkBoxes:

    // The box the key holds, as parkBoxes gives it, or none when the key holds no box. Allocates
    // nothing.
    std::optional<ParkBox> parkBox(const World &world, EntityKey key);

Step 2: In intent.cpp, parkBox reads the key's BoxIntent and ShopStockIntent with tryGet and builds the ParkBox as parkBoxes does.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0, no warning.

### Task 6: Look up one piece of furniture by its key

Files:
- Modify: `src/sim/furniture/furniture.h:99`, `src/sim/furniture/furniture.cpp:1090-1097`

Step 1: In furniture.h, declare after parkFurniture:

    // The piece the key holds, as parkFurniture gives it, or none when the key holds no piece.
    // Allocates nothing.
    std::optional<FurniturePiece> furniturePiece(const World &world, EntityKey key);

Step 2: In furniture.cpp, furniturePiece reads the key's FurniturePieceState with tryGet and builds the FurniturePiece as parkFurniture does.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0, no warning.

### Task 7: Read a facility's record without a list

Files:
- Modify: `src/sim/operations/facilities.cpp:259-285`

Step 1: In facilityRecord, replace the loop over parkBoxes with parkBox(world, facility), and give none when it gives none.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0, no warning.

### Task 8: Keep shopRecord's lists

Files:
- Modify: `src/sim/operations/operations.h:175`, `src/sim/operations/operations.cpp:28-38,786-820`

Step 1: In operations.h, include `sim/tick_buffers.h` and declare after shopRecord:

    // As the returning form, keeping the lists it works with in the buffers. Allocates nothing once
    // they have grown.
    std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop, TickBuffers &buffers);

Step 2: In operations.cpp, add a private struct ShopRecordLists holding `std::vector<ParkBox> Boxes`, `std::vector<EntityKey> Depots`, and `RouteLengths Lengths`. The kept shopRecord gives none unless parkBox gives a shop for the key. It fills Boxes with the filling parkBoxes and Depots with their depots' keys, which positionOf and the nearest depot need. It counts the queue by walking the visits ledger's stocks at the shop with std::ranges::equal_range on FlowStock::Endpoint, as facilities.cpp's stocksAt does, without copying them. It finds the nearest depot with the depotRouteBy form that takes the depots and RouteLengths. The returning form, which inspectorSubject and tests/integration/shop_stock_test.cpp call, makes a TickBuffers and calls the kept form.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0, no warning.

### Task 9: Fill an inspection in place

Files:
- Modify: `src/legible/inspect.h`, `src/legible/inspect.cpp`

Step 1: In inspect.h, include `sim/guests/guests.h` and `sim/tick_buffers.h`. Replace the returning inspectSubject, whose only caller is the app's Inspector, with:

    // The lists inspectSubject reuses from call to call.
    struct InspectLists {
      TickBuffers Buffers;
      std::vector<ChoiceOption> Options;
    };

    // Fills the inspection, which the caller keeps, with the subject's inspector, built from its
    // inspection record and the world's tick, or saying its entity is gone when the world gives no
    // record. Changes nothing. Filling it again for the same subject allocates nothing once the
    // lists and the inspection have grown.
    void inspectSubject(const World &world, const InspectorSubject &subject, InspectLists &lists,
                        Inspection &inspection);

Step 2: In inspect.cpp, write the text in place:

- A setText helper clears a string and writes into it with `std::format_to(std::back_inserter(text), ...)`, and an appendText helper writes without clearing. Literal text is set with `assign`.
- appendDisplayName appends an enum's name and turns each '-' it appended into a space. Capitalizing changes the first character in place.
- appendBoxText appends a box's or piece's kind and key, from parkBox, then furniturePiece, then "box <key>".
- Every helper that returns a std::string today (keyText, twoDecimals, seconds, displayName, boxText, fullPiecesText, capitalized) gives way to one that writes into a string it is given.

Step 3: In inspect.cpp, fill the lists in place:

- A nextRow helper gives the list's next row when the list holds one, and otherwise adds an empty row. After a list is filled, it is resized to the rows used.
- Each fill writes every string of each row it takes, and clears the ones it leaves empty.
- At each fill, reserve Rows to 11, Choices to NEEDS.size() + 2, and HomeTerms to HOME_REASONS.size() + 1, with named constants: the most rows any inspection shows, the choice rows, and the home terms. A list of strings that grows moves short strings to a new block, which the check can't tell from a temporary, and reserving means no list grows after the first fill.
- A guest's choices come from guestOptions with lists.Buffers into lists.Options. A shop's record comes from the kept shopRecord with lists.Buffers.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: exit status 0, no warning.

### Task 10: Fill an availability in place

Files:
- Modify: `src/legible/availability.h:69-75`, `src/legible/availability.cpp:254-276`

Step 1: In availability.h, replace the returning availabilityAt, whose only caller is the app's need tooltip, with a form declared below AvailabilityLists:

    // Fills availability, which the caller keeps, with the availability at a place on the world's
    // guest network: each reachable source whose offer says it is supplied contributes its relief
    // discounted by its effective time, and the value is their terms added in order. Uses lists the
    // caller keeps, and allocates nothing once they have grown. Changes nothing.
    void availabilityAt(const World &world, const AvailabilityDefinition &definition,
                        const Place &place, AvailabilityLists &lists, Availability &availability);

Step 2: In availability.cpp, availabilityAt clears the contributions and sets the value to 0. It samples route distance at the place into lists.Routes with the filling sampleField, and appends contributions as it does today.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: exit status 0, no warning.

### Task 11: Find an overlay place with a kept list

Files:
- Modify: `src/render/paving_overlay.h:71-73`, `src/render/paving_overlay.cpp:256-280`

Step 1: In paving_overlay.h, replace overlayPlace, whose only caller is the app's need tooltip, with:

    // The overlay place of a ground point: the nearest place on the lines of the pieces that hold
    // it, ties to the lower segment key, or none when no piece holds it or it is not finite. Uses a
    // list the caller keeps, and allocates nothing once it has grown.
    std::optional<Place> overlayPlace(const OverlayShape &shape, GroundPoint point,
                                      std::vector<uint32_t> &lines);

Step 2: In paving_overlay.cpp, clear lines and use it in place of the local list.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: exit status 0, no warning.

### Task 12: Build the graph overlay into kept lists

Files:
- Modify: `src/render/graph_overlay.h:66-71`, `src/render/graph_overlay.cpp:33-110`

Step 1: In graph_overlay.h, replace buildGraphOverlay, whose only caller is the app's graph view, with:

    // Fills overlay, which it clears first, with both networks' segments and nodes in a window of
    // the width and height, with segments clipped to the near plane. A line is marked as a
    // connector when its carrier is not a path's, and a node holds its anchor. Uses a list of keys
    // the caller keeps, and allocates nothing once they have grown.
    void buildGraphOverlay(const World &world, const CameraView &view, float width, float height,
                           std::vector<EntityKey> &pathKeys, GraphOverlay &overlay);

Step 2: In graph_overlay.cpp, clear the overlay's lines and nodes. For each kind, fill pathKeys with parkSegmentKeys for that kind, then append that network's lines and nodes. A network's carriers are its kind's segments or connectors, so the keys of its own kind mark the same connectors as all segments' keys did. appendLines takes a span of keys.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: exit status 0, no warning.

### Task 13: Add PanelData

Files:
- Create: `src/app/ui/panel_data.h`, `src/app/ui/panel_data.cpp`
- Modify: `src/app/CMakeLists.txt:2-13`

Step 1: Create panel_data.h:

    // The data the frame's panels and tooltips show, worked out into lists kept between frames, so
    // working it out again allocates nothing once they have grown.
    class PanelData {
    public:
      // The number of guests in the park.
      uint64_t guestCount(const World &world);
      // The arrival day for the hours, made again only when the hours change.
      const ArrivalDay &arrivalDay(const World &world, OpeningHours hours);
      // The subject's inspection.
      const Inspection &inspection(const World &world, const InspectorSubject &subject);
      // The stock setting of the shop the key holds, or none when it holds no shop.
      static std::optional<ShopStock> shopStock(const World &world, EntityKey key);
      // The need's availability at the overlay place of the point in the shape, or null when there
      // is no point or the shape has no overlay place there.
      const Availability *tooltipAvailability(const World &world, GuestNeed need,
                                              const OverlayShape &shape,
                                              std::optional<ParkPoint> point);
      // The Graph view's lines and nodes for the view in a window of the width and height.
      const GraphOverlay &graph(const World &world, const CameraView &view, float width,
                                float height);

    private:
      std::vector<EntityKey> Guests;
      KeptArrivalDay Arrivals;
      InspectLists InspectKept;
      Inspection Inspected;
      std::vector<uint32_t> OverlayLines;
      AvailabilityLists AvailabilityKept;
      Availability Tooltip;
      std::vector<EntityKey> GraphKeys;
      GraphOverlay Graph;
    };

Step 2: Create panel_data.cpp. guestCount fills Guests with the filling parkGuests and gives its size. arrivalDay calls keepArrivalDay and gives Arrivals.Day. inspection calls inspectSubject. shopStock gives the stock of the box parkBox gives when it is a shop. tooltipAvailability gives null for no point, finds the overlay place with overlayPlace and OverlayLines, gives null when there is none, and otherwise fills Tooltip with availabilityAt and `needAvailability(need)`. graph calls buildGraphOverlay.

Step 3: In src/app/CMakeLists.txt, add `ui/panel_data.cpp` to tpj_app_core's sources.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: exit status 0, no warning.

### Task 14: Draw the panels from PanelData

Files:
- Modify: `src/app/ui/tooling_ui.h:62`, `src/app/ui/tooling_ui.cpp:32-122`
- Modify: `src/app/ui/need_tooltip.h`, `src/app/ui/need_tooltip.cpp`, `src/app/ui/graph_view.h`, `src/app/ui/graph_view.cpp`

Step 1: In tooling_ui.h, replace the `KeptArrivalDay Arrivals` member with `PanelData Panels`, and update the comment on build to say the data comes from it.

Step 2: In tooling_ui.cpp, drawPanels takes the PanelData in place of the KeptArrivalDay. The Debug panel's guest count comes from `guestCount`, and the Hours panel's curve from `arrivalDay`. In build, the Inspector draws `inspection(world, *subject)`, and its stock controls read `shopStock(world, subject->Key)` in place of the loop over parkBoxes. The graph view draws `graph(world, view, size.x, size.y)` with ImGui's display size. The need tooltip draws when `tooltipAvailability` on the previewed world gives an availability.

Step 3: drawNeedTooltip becomes `void drawNeedTooltip(GuestNeed need, const Availability &availability)`, drawing what it draws today from the availability it is given. drawGraphView becomes `void drawGraphView(const GraphOverlay &overlay)`, drawing the overlay it is given. Update their comments.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: exit status 0, no warning.

### Task 15: Keep frame times only with --frame-times

Files:
- Modify: `src/app/application.cpp:69`

Step 1: Call `Frames.add(step.Nanoseconds)` only when WriteFrameTimes is true.

Step 2: Build and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --frames 10 --frame-times`
Expected: one line, `frame-times` followed by 8 numbers.

### Task 16: Run the panels in the check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Include `app/ui/panel_data.h`, `app/input/orbit_camera.h`, and `legible/arrivals.h`. Add a HeldPanel struct holding a tpj::PanelData, an InspectorSubject, and the OpeningHours its arrival day asks for.

Step 2: Add holdPanels, which holds four panels: the guest in the middle of parkGuests, then the first shop, the first bench, and the first toilet block of parkBoxes. It throws std::runtime_error naming any subject full.park lacks. The guest's and the bench's panels ask for the park's hours, and the shop's and the toilet block's for hours that open an hour later and close as the park's do.

Step 3: Add panelFrame, which runs one panel's data as the app works it out: guestCount, departureTally, arrivalDay, inspection, shopStock of the subject's key, tooltipAvailability for each of NEEDS on the food overlay's shape at the middle of the first guest segment, and graph for `orbitCameraView(OrbitCamera{})` at 1280 by 720. The food overlay's shape is `play.Overlays[0].Scene.overlayShape()`, and the place is `segmentPlaces(world, PathKind::Guest).Middle`, worked out once before the warm-up.

Step 4: Add the panels to SteadyPlay. Run each panel's frame after syncTools in warmUp and in the watched ticks, the latter under `tpj::check::watchWork("the panels")`. Change report's success line to `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, and the panels freed no block they allocated`, and add the panels to the file's opening comment.

Step 5: Build and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, and the panels freed no block they allocated`, exit status 0.

If it names a block the panels freed, fix that allocation in the code that made it, as the conventions require, and run it again.

### Task 17: Show the check sees the panels

Files: none.

Step 1: Do criterion 1's check: add the temporary list to PanelData::guestCount, build tpj_allocation_check, run it, and see it exit with status 1, printing lines that start `allocation check: the panels freed`. Revert the list.

### Task 18: Show the simulation unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-panels/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-panels/scenarios-before.txt build/quiet-panels/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-panels/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-panels/full-before.txt build/quiet-panels/full-after.txt`
Expected: exit status 0.

### Task 19: Play the panels

Files: none.

Step 1: Build the whole windows-release preset.

Run: `cmake.exe --build --preset windows-release`
Expected: the build finishes with exit status 0 and prints no warning.

Step 2: The user does criterion 4's play check in `build/windows-release/ThemeParkJones.exe`.

### Task 20: Confirm the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play on full.park frees no block it allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 21: Commit

Step 1: Stage the feature's paths by name and commit once with the commit-hygiene skill, subject `App: Keep the panels' data and check it`.
