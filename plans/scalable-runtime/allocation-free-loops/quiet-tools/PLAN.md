# Implementation Plan: Quiet Tools

## Goal

Make each tool's frame work, and building the look's marks again when the look changes, free no block they allocated on full.park, and run it in the allocation check, with the tools and the simulation unchanged.

## Approach

The Interaction keeps a ToolPark: the park's path points, segments, corners, edges, tapers, boxes, and doors, filled from intent only when the session's edit count or generation changes, and the tools' working lists behind a pointer to a private type. Every tools function reads the park through it, snapping and the frontage search keep their lists in it, and the snap in effect holds its guides and holds in fixed arrays. The Interaction keeps the tentative edit and the measured lengths, and a new fillLook in app/scene fills the app's kept PreviewLook in place. SceneSync builds the look's marks through list forms of render's appendEntity and appendSides, from boxes, path points, and segments it fills again only when the edit count changes.

## Placement

Decision 0027 places each behavior this feature adds:

- appendStopPosition: sim/park, edits.h. It owns stops and their positions, so the simulation and the tools give the same positions.
- ToolPark and keepToolPark, and every tools function reading the park through it: tools, tools.h. The tools own what they read of the park and how they reuse their lists.
- The tools' working lists: tools, the private headers tools/internal/tool_lists.h, tools/internal/snapping.h, and the new tools/internal/frontage.h, so the app sees none of snapping's or the frontage search's types.
- Keeping the ToolPark, the tentative edit, and the measured lengths, and filling the ToolPark again when the edit count or generation changes: app, Interaction in app/input/interaction.h (tpj_app_core). It already owns the tool and follows the session's generation.
- Filling the tool's part of the frame's look: app, fillLook in app/scene/scene_sync.h, beside PreviewLook. cmake/layers.txt puts app/scene above app/input, so it can read the Interaction, and the check can call it.
- appendEntity and appendSides reading kept lists: render, park_mesh.h, which owns the ghost's marks.
- Keeping the boxes, path points, and segments the look's marks read: app, SceneSync in app/scene/scene_sync.h, which already rebuilds the look's marks when the look or the edit count changes.
- src/app/application.cpp and application.h keep the frame's look and call the Interaction and fillLook. src/app/ui/tooling_ui.cpp and snap_guides take spans. Both are composition only.
- Running the tools in the check: tests/checks/allocations/allocation_check.cpp.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, build tpj_scenarios and record.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `mkdir -p build/quiet-tools && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-tools/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-tools/full-before.txt`
Expected: exit status 0.

### Task 2: Update the specs

Files:
- Modify: `src/tools/SPEC.md`, `src/render/SPEC.md`, `src/sim/park/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each edit FEATURE.md's Spec changes lists, with its sentences as written there.

### Task 3: Append one stop's position

Files:
- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`

Step 1: Declare after stopPositions:

    // Appends the stop's ground position to the positions of the path's earlier stops, which
    // positions holds, as stopPositions gives it, reading the corners from a list as parkCorners
    // gives them. Gives false and appends nothing where stopPositions gives none. Allocates nothing
    // once the list has grown.
    bool appendStopPosition(const World &world, std::span<const ParkCorner> corners,
                            const PathStop &stop, std::vector<ParkPoint> &positions);

Step 2: Move the body of stopPositions' loop into appendStopPosition, and stopPosition's OnBend branch into a helper that takes the corner found. appendStopPosition finds the OnBend's corner in the corners given: the one whose Point is the stop's point, with a radius above 0, as roundedCorner finds it. stopPosition keeps finding it through roundedCorner. stopPositions calls appendStopPosition for each stop with the corners parkCorners gives, derived only when a stop is an OnBend, so it gives the same positions and derives no more than today.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 4: Declare the ToolPark

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`
- Create: `src/tools/internal/tool_lists.h`

Step 1: In tools.h, forward-declare `struct ToolLists;` and declare:

    // The park as the tools read it: its path points, segments, corners, edges, tapers, and boxes,
    // in the order sim/park gives them, and the doors serving each kind, as parkDoors gives them,
    // with the lists the tools reuse from call to call. The caller keeps it and fills it again with
    // keepToolPark whenever the world's intent changes. It can be moved but not copied.
    struct ToolPark {
      ToolPark();
      ~ToolPark();
      ToolPark(ToolPark &&) noexcept;
      ToolPark &operator=(ToolPark &&) noexcept;
      ToolPark(const ToolPark &) = delete;
      ToolPark &operator=(const ToolPark &) = delete;

      std::vector<ParkPathPoint> Points;
      std::vector<ParkSegment> Segments;
      std::vector<ParkCorner> Corners;
      std::vector<ParkEdge> Edges;
      std::vector<ParkTaper> Tapers;
      std::vector<ParkBox> Boxes;
      std::vector<ParkDoor> GuestDoors;
      std::vector<ParkDoor> BackstageDoors;
      std::unique_ptr<ToolLists> Lists;
    };

    // Fills the park from the world's intent.
    void keepToolPark(const World &world, ToolPark &park);

Step 2: Create tools/internal/tool_lists.h with `struct ToolLists`, holding for now `std::vector<ParkPoint> Positions`, the stop positions a path tool works out. Tasks 7 and 8 add snapping's and the frontage search's lists. Define ToolPark's constructor, which makes the ToolLists, its destructor, and its moves as defaulted in tools.cpp, and keepToolPark, which assigns each list from parkPathPoints, parkSegments, parkCorners, parkEdges, parkTapers, parkBoxes, and parkDoors for each kind.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 5: Hold a snap's guides and holds in fixed arrays

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/snapping.cpp`

Step 1: In PointerSnap, replace the two vectors with `std::array<SnapGuide, 2> Guides{}`, `size_t GuideCount = 0`, `std::array<SnapHold, 2> Holds{}`, and `size_t HoldCount = 0`. Comment that no snap has more than two of either, so a snap is a plain value. Every PointerSnap the code builds sets only the slots it uses, so the defaulted equality stays exact.

Step 2: Change snapGuides to return `std::span<const SnapGuide>`: the snap's first GuideCount guides, or an empty span. In resnap, copy the snap's held array and count into a local array before resetting the snap, and give SnapQuery.Held a span of them. In findSnap, copy the Snap's arrays and counts into the PointerSnap. The places that build a PointerSnap with no guides and no holds keep their counts at 0.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 6: Read the park through the ToolPark in every tool

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/snapping.cpp`, `src/tools/frontage.cpp`, `src/tools/internal/snapping.h`

Step 1: Change the declarations to:

    void movePointer(ToolState &tool, ToolPark &park, const World &world, const PointerSample &pointer);
    void pressPointer(ToolState &tool, ToolPark &park, const World &world);
    std::optional<ParkEdit> releasePointer(ToolState &tool, ToolPark &park, const World &world);
    std::optional<EntityKey> highlightedEntity(const ToolState &tool, const ToolPark &park,
                                               const World &world);
    std::optional<EntityKey> boxAt(const ToolPark &park, ParkPoint point);
    std::optional<EntityKey> segmentAt(const ToolPark &park, ParkPoint point);
    std::optional<EntityKey> pointAt(const ToolPark &park, ParkPoint point);
    std::optional<EditHandle> handleAt(const ToolPark &park, ParkPoint point);
    std::optional<EntityKey> sideAt(const ToolPark &park, ParkPoint point);
    std::optional<EditHandle> hoveredHandle(const ToolState &tool, const ToolPark &park);
    std::optional<PathStart> pathStart(const ToolState &tool, ToolPark &park, const World &world);
    std::optional<ParkPoint> pathFinish(const ToolState &tool, ToolPark &park, const World &world);
    // As the three-argument form, keeping its lists in the park's.
    std::optional<Pose> frontagePose(const World &world, ToolPark &park, BoxKind kind,
                                     ParkPoint point);

In internal/snapping.h, change findSnap to `PointerSnap findSnap(const World &world, ToolPark &park, const SnapQuery &query)`. Keep each comment, and say where it reads the park.

Step 2: Make cornerOf, edgeOf, taperOf, boxPose, segmentCount, pointKind, insertAt, and the picking queries scan the park's lists. Make segmentOf and guestSegmentAt give `const ParkSegment *` into the park's segments, or nullptr. Make insertAt keep its places in a `std::array<Snap, 3>` with a count, since it tries one straight part and at most two arcs. Make SnapScene hold a reference to the ToolPark in place of its copies of the points, segments, and corners, and pointSnap read the doors of the query's kind from the park. The frontage form with a park calls the three-argument form's body for now. resnap passes the park to frontagePose and findSnap.

Step 3: Make nextStop work out positions in the park's Positions list: clear it, append each drawn stop's position and then the snapped stop's through appendStopPosition with the park's corners, and compare the last two as today. Make pathStart and pathFinish do the same: pathStart with the snapped stop alone, and pathFinish with the drawn stops. makeScene works out the drawn stops' positions the same way, into its own list for now. acceptsStop keeps building its AddPath for isAccepted, since only a press calls it.

Step 4: Make tentativeEdit, editHandles, and measuredLengths take `ToolPark &park` after the tool, or `const ToolPark &park` for editHandles, and read the park, keeping their returns for now. Task 9 makes them fill.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 7: Snap with kept lists

Files:
- Modify: `src/tools/snapping.cpp`, `src/tools/internal/snapping.h`, `src/tools/internal/tool_lists.h`

Step 1: Move Anchor and Guide from snapping.cpp into internal/snapping.h. Make Anchor hold its position, the index of its first arrival, and its arrival count. Add:

    // The lists one findSnap call fills, kept from call to call: the drawn stops' positions, which
    // segments and drawn segments are left out, the anchors, every anchor's arrivals one after
    // another, and the guides.
    struct SnapLists {
      std::vector<ParkPoint> Drawn;
      std::vector<bool> LeftOut;
      std::vector<bool> DrawnLeftOut;
      std::vector<Anchor> Anchors;
      std::vector<ParkPoint> Arrivals;
      std::vector<Guide> Guides;
    };

Add `SnapLists Snapping` to ToolLists.

Step 2: Make SnapScene hold a reference to the park's SnapLists, and makeScene clear and refill them: the drawn stops' positions through appendStopPosition with the park's corners, and the left-out flags through fillGrowing (src/sim/tick_buffers.h). anchorsOf and guidesFrom fill the kept anchors, arrivals, and guides, clearing them first. An anchor's arrivals are the end of the arrivals list while it is built, so addArrival looks for a repeat only there. An anchor no segment arrives at gets the two axes pushed onto the list. Remove guidesFrom's reserve, which would size the list exactly each call.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 8: Search for frontage with kept lists

Files:
- Modify: `src/tools/frontage.cpp`, `src/tools/internal/tool_lists.h`
- Create: `src/tools/internal/frontage.h`

Step 1: Move EdgeStep, NearPiece, and Candidate from frontage.cpp into the new private header, and declare there:

    // The lists one frontage search fills, kept from call to call: the edge lines' steps, the
    // pieces near the box, and the places it tries.
    struct FrontageLists {
      std::vector<EdgeStep> Steps;
      std::vector<NearPiece> Near;
      std::vector<Candidate> Candidates;
    };

Add `FrontageLists Frontage` to ToolLists.

Step 2: Move the body into the form with a park. Make edgeSteps, piecesNear, and candidatesNear fill the park's kept lists, clearing them first, and Room hold a reference to the near pieces. Make the three-argument form call it with a ToolPark of its own, so frontage_test gives the same poses.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 9: Fill the edit, the handles, and the lengths in place

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`

Step 1: Change the declarations to:

    // Puts the edit the ghost shows in edit. If edit holds an AddPath and the tool's edit is one,
    // it refills that AddPath's stops in place, so they allocate nothing once grown.
    void tentativeEdit(const ToolState &tool, ToolPark &park, const World &world,
                       std::optional<ParkEdit> &edit);
    // The edit the ghost shows.
    std::optional<ParkEdit> tentativeEdit(const ToolState &tool, ToolPark &park, const World &world);
    void editHandles(const ToolState &tool, const ToolPark &park, std::vector<EditHandle> &handles);
    void measuredLengths(const ToolState &tool, ToolPark &park, const World &world,
                         std::vector<MeasuredLength> &lengths);

Keep editHandles' and measuredLengths' comments, adding that each clears the list first.

Step 2: Make pathEdit fill the edit: if fewer than two stops would result, reset it. Otherwise, if the edit doesn't hold an AddPath, assign it an empty one. Set its kind, copy the drawn stops into its stops with copyGrowing (src/sim/tick_buffers.h), and push the next stop when there is one. Every other tool assigns its edit, or resets it, as it returns today. The returning form fills an empty optional and returns it, and releasePointer uses it.

Step 3: Make editHandles and measuredLengths fill the list given, clearing it first. measuredLengths works out the drawn stops' positions in the park's Positions list, as nextStop does.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 10: Keep the ToolPark and the frame's answers in the Interaction

Files:
- Modify: `src/app/input/interaction.h`, `src/app/input/interaction.cpp`

Step 1: Change the declarations to:

    // Starts with the tool at start and no subject, having seen the generation and the edit count,
    // with its park filled from the world.
    Interaction(const World &world, uint64_t generation, uint64_t edits);
    // For a generation other than the last one it saw, selects the current tool again, so it drops
    // any hold and drawn points, and forgets the subject. For a generation or an edit count other
    // than the last it saw, fills its park again from the world.
    void follow(const World &world, uint64_t generation, uint64_t edits);
    // The tool's edit, or else the Inspector's chosen stock as a SetShopStock while it differs from
    // the shop's, or else the Hours panel's chosen hours as a SetOpeningHours while they differ from
    // the park's. Kept until the next call, which refills it in place.
    [[nodiscard]] const std::optional<ParkEdit> &tentativeEdit(const World &world);
    [[nodiscard]] std::optional<PathStart> pathStart(const World &world);
    [[nodiscard]] std::optional<ParkPoint> pathFinish(const World &world);
    // Edit path's handles, as editHandles gives them, in a list the caller keeps.
    void editHandles(std::vector<EditHandle> &handles) const;
    [[nodiscard]] std::optional<EditHandle> hoveredHandle() const;
    [[nodiscard]] std::span<const SnapGuide> snapGuides() const;
    // The lengths the ghost shows, as measuredLengths gives them, kept until the next call.
    [[nodiscard]] std::span<const MeasuredLength> measuredLengths(const World &world);

Add members `uint64_t Edits`, `ToolPark Park`, `std::optional<ParkEdit> Edit`, and `std::vector<MeasuredLength> Lengths`.

Step 2: Pass Park to every tools call. Make shopStockOf scan Park.Boxes. tentativeEdit fills Edit through the tools' filling form. If that leaves it empty, it assigns the stock or hours edit as today, and returns Edit.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build finishes with exit status 0 and prints no warning. tpj_app_core doesn't compile src/app/application.cpp or the ui files, which task 11 changes.

### Task 11: Fill the frame's look in place

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/application.h`, `src/app/application.cpp`, `src/app/ui/tooling_ui.cpp`, `src/app/ui/snap_guides.h`, `src/app/ui/snap_guides.cpp`

Step 1: Declare in scene_sync.h, after PreviewLook, including app/input/interaction.h:

    // Puts the frame's look in look: the interaction's highlight, subject, path start and finish,
    // handles, and hovered handle, and the overlay. Refills the handles' list in place.
    void fillLook(Interaction &interaction, const World &world, std::optional<GuestNeed> overlay,
                  PreviewLook &look);

Define it in scene_sync.cpp.

Step 2: In application.h, add a `PreviewLook Look;` member after Interactions, commented as the frame's look, kept so its lists keep their memory. In application.cpp, construct Interactions with the session's world, generation, and edits. Call `Interactions.follow(Session.world(), Session.generation(), Session.edits())` at the top of the frame, as today, and again after the ticks and applyWithoutTick, before Scene.syncWorld, so the tools read the park the ticks left. Replace the look built in the frame with `fillLook(Interactions, Session.world(), Ui.shown().Overlay, Look)`, passed to syncLook.

Step 3: Make drawSnapGuides take `std::span<const SnapGuide>` and `std::span<const MeasuredLength>`. tooling_ui.cpp passes the Interaction's spans as today.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 12: Add list forms of appendEntity and appendSides

Files:
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`

Step 1: Declare after each world form:

    // As appendEntity, reading the boxes and path points from lists as parkBoxes and
    // parkPathPoints give them. Allocates nothing once the mesh has grown.
    void appendEntity(ParkMesh &mesh, const World &world, std::span<const ParkBox> boxes,
                      std::span<const ParkPathPoint> points, EntityKey key, Rgba color);
    // The same strips for the segment the key holds among the segments, as parkSegments gives
    // them.
    void appendSides(ParkMesh &mesh, std::span<const ParkSegment> segments, EntityKey segment,
                     Rgba color);

Step 2: Move the body of appendSurface's loop over pieces into a helper that adds one piece. The list form of appendEntity adds each piece of resolvedSurface(world) whose owner is the key with that helper, GHOST_LIFT up, in the color, in place of copying them into a PavedSurface. The copy held no edge lines, so the mesh is the same. The world forms call the list forms with parkBoxes, parkPathPoints, and parkSegments.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 13: Build the look's marks from kept lists

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`

Step 1: Add SceneSync members `LookBoxes`, `LookPoints`, and `LookSegments`, commented as the boxes, path points, and segments the look's marks read, filled when the edit count changes. In syncLook, when LookEdits differs from Edits, fill them from parkBoxes (its filling form), parkPathPoints, and parkSegments before building the look's marks. Pass them to buildLookMarks, which calls the list forms of appendEntity and appendSides.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 14: Run the tools in the check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Add a held tool: an Interaction, a PreviewLook, and the PointerSample it holds. Its frame runs as the app's does: follow with the session's world, generation, and edits; movePointer with the sample; tentativeEdit; fillLook with no overlay; snapGuides; and measuredLengths. Each sample has the place as its ground, 0.05 meters per pixel, and snapping on.

Step 2: Before anything else is held, add a curve in open ground, since full.park has none. Queue an AddPath of PathKind::Guest through NewPoint stops at (-120, -80), (-105, -80), and (-105, -65) with queueEdit on session.commands(), and call session.applyWithoutTick(). Its middle point joins exactly two segments, so parkCorners gives a corner there. Queue SetCornerRadius of that corner's point to 4 and SetSegmentWidth of the lower-keyed of its two segments to 5, and apply them the same way. Throw std::runtime_error with "the held curve has no rounded corner and taper" unless parkCorners gives a corner with a radius above 0 and parkTapers gives a taper.

Step 3: Find these places from the park's intent, throwing std::runtime_error naming what is missing if one isn't there:
- The guest segment: the first in parkSegments of PathKind::Guest whose ground line has at least two points. Its middle is halfway between the line's first and last points. Its normal is the unit direction from the first to the last, turned a quarter turn counter-clockwise. Its side lies half its width out along the normal from its middle, and beside it lies half its width plus 2 m out.
- Its point: the position parkPathPoints gives for its From point.
- The backstage segment, its middle, and the place beside it, found the same way for PathKind::Backstage.
- The shop: the first shop in parkBoxes, at its pose's position.
- The edge handle: the Middle of the first edge parkEdges gives.
- Open ground at (-120, -120) and (-110, -120).
- The curve's radius handle, at radiusHandleOf of its corner, its taper handle, at taperHandleOf of its taper, and its arc's middle, at bendPlace of its corner at 0.5.

Step 4: Make the twenty-three held tools, each with its own Interaction. A press is a movePointer to the place, then useButtons with Pressed set and session.commands(), before the warm-up.
- Look at the guest segment's middle.
- Guest path at the point, and at the curve's arc's middle, with nothing drawn.
- Guest path with presses at (-120, -120), held at (-110, -120). Throw unless one stop is drawn.
- Guest path with presses at (-120, -120) and (-110, -120), held at (-110, -120). Throw unless two stops are drawn.
- Backstage path at the backstage segment's middle.
- Place shop, Place bench, and Place toilet beside the guest segment, and Place depot beside the backstage segment.
- Move box at the shop.
- Move box with a press at the shop, held 1 m from it along x. Throw unless the tool holds a box.
- Edit path at the point, at the edge handle, at the guest segment's side, at its middle, at the curve's radius handle, and at its taper handle.
- Edit path with a press at the point, held 1 m from it along x. Throw unless the tool holds a point.
- Remove point at the point.
- Delete at the shop and at the guest segment's middle.
- Furnish at the guest segment's middle.

Step 5: Add a changing look: a held preview with no edit and no overlay, as the existing held previews are, and two looks. The first highlights the shop, has Point handles at the shop and at the point, hovers the guest segment's sides, and inspects the shop as SubjectKind::Shop. The second highlights the guest segment, has one Point handle at the point, hovers the backstage segment's sides, and inspects nothing. Before each sync, give it the first look at even ticks and the second at odd ticks, as a pointer moving between them would.

Step 6: Run each held tool's frame and sync the changing look after each warm-up tick. In each watched tick, after the overlays, call `watchWork("the tools")`, run each held tool's frame, and sync the changing look. Change the success line to `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, and the tools freed no block they allocated`, and update the file's comments.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, and the tools freed no block they allocated`. If it reports blocks, the report names the work. Fix each loop allocation it finds in this feature (docs/conventions.md).

### Task 15: Show the check sees the tools

Files: none kept.

Step 1: Plant the temporary list criterion 1 gives in Interaction::movePointer, rebuild, and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with report lines starting `allocation check: the tools freed`.

Step 2: Revert the plant, rebuild, and run again.

Expected: exit status 0.

### Task 16: Show the frontage test unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_integration_tests && build/windows-debug/tpj_integration_tests.exe -# "[#frontage_test]"`
Expected: `All tests passed (19 assertions in 2 test cases)`.

### Task 17: Show the simulation unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-tools/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-tools/scenarios-before.txt build/quiet-tools/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-tools/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-tools/full-before.txt build/quiet-tools/full-after.txt`
Expected: exit status 0.

### Task 18: Play the tools

Files: none.

Step 1: Build the whole windows-release preset.

Run: `cmake.exe --build --preset windows-release`
Expected: the build finishes with exit status 0 and prints no warning.

Step 2: The user does criterion 3's play check in `build/windows-release/ThemeParkJones.exe`.

### Task 19: Confirm the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play on full.park frees no block it allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 20: Commit

Step 1: Stage the feature's paths by name and commit once with the commit-hygiene skill, subject `Tools: Keep the tools' park and lists and check them`.
