# Implementation Plan: Quiet Edits

## Goal

The app's work after an edit, an undo, or a redo, at a click's pick, at each tool's press and release, and at showing or hiding an overlay refills lists the app keeps, and the allocation check watches it over rounds of undos and redos on full.park.

## Approach

Each reader that this work calls with a returning form gains a form that fills a list the caller keeps. The sim's ground derivation keeps its working lists in TickBuffers, as shopRecord already does, so no private type reaches a public header. Lists whose values hold lists of their own, the segments and their ground lines, keep their extra values in a spare list when they shrink, so no inner list's memory is freed. Each returning form becomes a wrapper over its fill form, so both give the same results by construction, and tpj_scenarios --compare confirms it. The look reads the tool park's points, segments, and boxes instead of deriving its own. The allocation check pauses its watch while the session applies each undo or redo, and watches a second history that follows the same world.

## Placement

Decision 0027 places each behavior this feature adds:

- Keeping a list of values that hold lists, with a spare list: sim, src/sim/tick_buffers.h, next to copyGrowing and fillGrowing. It is the shared helper for kept lists, and sim code is its first user.
- The fill forms of parkPathPoints, parkSegments, parkCorners, segmentLine, and segmentRecords, and the kept ground derivation: sim/park, src/sim/park/intent.h, geometry.h, edits.h, and internal/layout.h. sim/park owns intent and how the ground is derived from it.
- The fill forms of parkEdges and parkTapers: sim/park, src/sim/park/surface.h. surface.cpp owns the edge fitting they run.
- The fill form of parkDoors: sim/routes, src/sim/routes/networks.h, which owns doors.
- The kept form of nearestDepot: sim/operations, src/sim/operations/operations.h, which owns supply routes.
- The fill form of parkFurniture: sim/furniture, src/sim/furniture/furniture.h.
- The kept park mesh, overlay shape, and picking lists: render, src/render/park_mesh.h, paving_overlay.h, and picking.h, which own building them.
- The kept form of pickSubject, and clearing the kept preview's tick: legible, src/legible/inspect.h and preview.h.
- Refilling the tool park, and releasing into a kept edit: tools, src/tools/tools.h and internal/tool_lists.h.
- The history's kept records and change, and takeUndo and takeRedo: tools, src/tools/history.h, which owns the history.
- Keeping the park mesh and the overlay shape's lists, and reading the look's lists from the tool park: app, src/app/scene/scene_sync.h. The scene's sync already owns every mesh the scene draws.
- Keeping the pick's lists and the released edit: app, src/app/input/interaction.h. The interaction already owns the pick and the release.
- Queueing an undo or redo from the history's kept command, on a kept queue: app, src/app/session/park_session.h, which owns applying them.
- application.cpp changes only the calls it composes. main.cpp gains nothing.
- Pausing the watch: the allocation check, tests/checks/allocations/watch.h and watch.cpp.
- The rounds of undos and redos: the allocation check, tests/checks/allocations/allocation_check.cpp.

## Tasks

### Task 1: Record the frame and the simulation before the change

Files: none.

Step 1: On main, before any edit, build the app and tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_scenarios`
Expected: the build finishes with exit status 0 and prints no warning.

Step 2: Capture the frame and record the scenarios.

Run: `mkdir -p build/quiet-edits && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 300 --speed 0 --overlay food --frames 5 --capture build/quiet-edits/before.bmp`
Expected: exit status 0, and build/quiet-edits/before.bmp exists.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-edits/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-edits/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c quiet-edits`.

### Task 2: Update the specs

Files:
- Modify: `src/sim/park/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/operations/SPEC.md`, `src/sim/furniture/SPEC.md`, `src/render/SPEC.md`, `src/legible/SPEC.md`, `src/tools/SPEC.md`, `src/app/SPEC.md`

Step 1: Make the SPEC.md edits FEATURE.md's Spec changes gives.

### Task 3: Update the docs and the milestone

Files:
- Modify: `docs/conventions.md`, `docs/testing.md`, `docs/decisions/0041-allocation-check.md`
- Modify: `plans/scalable-runtime/allocation-free-loops/MILESTONE.md`

Step 1: In docs/conventions.md, under "## Memory in loops", after "So does a copy, a rebuild, or a new list returned at every tick or frame.", add "Work at a player action, such as an edit, an undo, or a click, runs inside the frame loop too. Refill its lists instead of building new ones." Replace "The allocation check in tests/checks enforces this rule for all the work the app repeats in steady play. Only ImGui's own drawing and previews that resolve a new world stay outside it (decision 0041)." with "The allocation check in tests/checks enforces this rule for all the work the app repeats in steady play, and for its work after an undo or a redo, at a click's pick, and at each tool's press and release. Only ImGui's own drawing, previews that resolve a new world, and applying a command stay outside it (decision 0041)."

Step 2: In docs/testing.md, in the paragraph under "## The suite", replace "the panels' data, and the renderer's uploads and drawing (decision 0041). Only ImGui's own drawing and the previews that resolve a new world stay outside it." with "the panels' data, the renderer's uploads and drawing, and the app's work after an undo or a redo, at a click's pick, and at each tool's press and release (decision 0041). Only ImGui's own drawing, the previews that resolve a new world, and applying a command stay outside it."

Step 3: Amend docs/decisions/0041-allocation-check.md in place. In Decision, after "so a GPU buffer made and released is found.", add "It also undoes and redoes an edit, and watches the app's work after each: the history's part, the tool park, the scene's sync, the overlay's shape, a click's pick, and each tool's press and release. Applying the command resolves the world, so the watch pauses around it." In the paragraph that starts "Previews of an edit that changes every frame", after "Both stay outside the check until a milestone makes resolution reuse its storage.", add "So does applying a command, and a path tool's press that asks the world whether it accepts the path."

Step 4: In MILESTONE.md, replace criterion 11 with "11. The app's work at a player action reuses its memory. After an edit, an undo, or a redo applies, the tool park is filled again, and the park mesh, the overlay's shape, and the look's marks are built, each in lists the app keeps. The look reads the tool park's lists. Taking an undo or redo and noting its change in the history, showing or hiding an overlay, a click's pick, and each tool's press and release keep their memory too. Applying the command resolves the world and stays with changing-previews, and so does a path tool's press that asks the world whether it accepts the path. A plain edit adds a new record to the undo list and empties the redo list, which drops records that can no longer be redone. (ctest)". Replace feature 7's description with "the history, the tool park, the park mesh, the overlay's shape, the look, picking, and each tool's press and release keep their memory when a player acts, and the allocation check watches them over undos and redos, for criterion 11." In Leftovers, replace the three bullets under "Owned by quiet-edits, feature 7." with "- Committing an edit, undo, and redo: the history's records and change (history.cpp noteEdits), the taken edit and its command (history.cpp take), the tool park (tools.cpp keepToolPark), the park mesh (scene_uploads.cpp uploadWorldMeshes), the overlay's shape (scene_sync.cpp syncOverlayShape), the look's points and segments (scene_sync.cpp syncLook), and emptying the kept preview (scene_sync.cpp syncWorld).", "- A click's pick and its subject (picking.cpp entityAtCursor, inspect.cpp inspectorSubject).", and "- Each tool's press and release, apart from a path tool's press that asks the world whether it accepts the path (tools.cpp releasePointer)."

### Task 4: Pause the watch

Files:
- Modify: `tests/checks/allocations/watch.h`, `tests/checks/allocations/watch.cpp`

Step 1: In watch.h, after stopWatching, declare `void pauseWatching();` and `void resumeWatching();`, commented "Pauses and resumes watching. While paused, no block is noted, and freeing a noted block removes its note without finding it. That can hide a free but never finds a false one, so work the check doesn't watch can run between watched work."

Step 2: In watch.cpp, add `bool Paused = false;` with the other state. In note, also return while Paused. In forget, while Paused, find the block's entry as moved does and remove it with removeAt, without reporting; leave LastAllocated empty. moved needs no change: it already removes the entry. pauseWatching sets Paused and clears LastAllocated. resumeWatching clears Paused and LastAllocated. startWatching and stopWatching clear Paused.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check`
Expected: exit status 0, no warning.

### Task 5: Keep lists of lists

Files:
- Modify: `src/sim/tick_buffers.h`

Step 1: After fillGrowing, add `template <typename T> void resizeKeeping(std::vector<T> &kept, size_t count, std::vector<T> &spare);`, commented "Gives a kept list whose values hold lists of their own count values, keeping every inner list's memory: when it shrinks, its extra values move to the spare list, and when it grows, it takes values back from the spare list before making new ones. The values it keeps still hold what they held, for the caller to overwrite." Implement it as the comment says: move each extra value to the back of spare and pop it from kept, then move values from the back of spare to kept, and emplace default values once spare is empty. A moved-from value holds no memory, so popping it frees nothing.

### Task 6: Derive the ground into kept lists

Files:
- Modify: `src/sim/park/geometry.h:22`, `src/sim/park/geometry.cpp:18-29`
- Modify: `src/sim/park/internal/layout.h`, `src/sim/park/layout.cpp`
- Modify: `src/sim/park/intent.h:214-230`, `src/sim/park/intent.cpp`

Step 1: In geometry.h, after segmentLine, declare `void segmentLine(ParkPoint from, ParkPoint to, std::vector<CarrierPoint> &line);`, commented "As the returning form, into a list the caller keeps, which it clears first. Allocates nothing once the list has grown." Move the body into it, writing its two points into line, and make the returning form call it.

Step 2: In layout.h, declare `void layoutOf(const World &world, PathLayout &layout);`, commented "As the returning form, into a layout the caller keeps, clearing its lists first." It reads the points and segments with forEachWith. Declare `struct GroundLists`, commented "The lists deriving a ground reuses, and the ground it gives: the layout read from the world, each segment's straight line, the arc points, the scratch the roundings are found with, and the ground. The straight lines, the ground's segments, and a caller's segments keep their extra values in spare lists.", holding `PathLayout Layout;`, `std::vector<std::vector<CarrierPoint>> Lines;`, `std::vector<std::vector<CarrierPoint>> SpareLines;`, `std::vector<ParkPoint> Arcs;`, `TickBuffers Scratch;`, `PathGround Ground;`, `std::vector<ParkSegment> SpareSegments;`, and `std::vector<ParkSegment> CallerSpare;`. Declare `void groundOf(const PathLayout &layout, GroundLists &lists);`, commented "As the returning form, into lists.Ground. Allocates nothing once the lists have grown."

Step 3: In layout.cpp, add a struct in the anonymous namespace holding the segment ends, the bends, the paving reaches, the arms, and the roundings, kept in lists.Scratch. Make straightLines, roundingsOf, and pavingReaches fill the kept lists, clearing each first, instead of returning new ones. straightLines sizes Lines with resizeKeeping and fills each with segmentLine's fill form, or clears it where straightLine gives none. In groundOf's fill form, size Ground.Segments with resizeKeeping and SpareSegments. For each segment, set its fields, and build its line in place: copy the straight line with copyGrowing where no rounding touches it, and otherwise clear it and extend it as the code does now. Copy the corners with copyGrowing. Make the returning groundOf and layoutOf call the fill forms on lists of their own.

Step 4: In intent.h, after the existing fill forms, declare `void parkPathPoints(const World &world, std::vector<ParkPathPoint> &points);`, and `void parkSegments(const World &world, std::vector<ParkSegment> &segments, TickBuffers &buffers);` and `void parkCorners(const World &world, std::vector<ParkCorner> &corners, TickBuffers &buffers);`, commented "As the returning forms, into a list the caller keeps, keeping the lists that deriving the segments' lines works with in the buffers. Allocate nothing once the lists have grown." Include sim/tick_buffers.h.

Step 5: In intent.cpp, implement parkPathPoints' fill form with forEachWith, as parkEntrances' does. parkSegments' fill form takes `GroundLists &lists = buffers.get<GroundLists>()`, runs layoutOf and groundOf into it, sizes the caller's list with resizeKeeping against lists.CallerSpare, and copies each segment's fields and its line with copyGrowing, then its furnishing as the returning form sets it. parkCorners' fill form copies lists.Ground.Corners with copyGrowing. Make the three returning forms call the fill forms with lists and buffers of their own.

Step 6: Build the sim and run its tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: exit status 0, all tests passed.

### Task 7: Fit edges and tapers into kept lists

Files:
- Modify: `src/sim/park/surface.h:184-206`, `src/sim/park/surface.cpp:780-900,1411-1500`

Step 1: In surface.h, after parkTapers, declare `void parkEdges(const World &world, std::vector<ParkEdge> &edges, TickBuffers &buffers);` and `void parkTapers(const World &world, std::vector<ParkTaper> &tapers, TickBuffers &buffers);`, commented "As the returning forms, into a list the caller keeps, which they clear first, keeping the lists they work with in the buffers. Allocate nothing once the lists have grown."

Step 2: In surface.cpp, add an `EdgeLists` struct in the anonymous namespace holding the shapes, the ends, a Scratch, the asked points, the taper candidates, and the candidates' segment ends. Give shapesOf, endsOf, farEndsOf, and taperCandidates forms that fill a kept list, clearing it first, and make their returning forms, where buildSurface and others still use them, call those. Make edgesOf and tapersAt fill a found list the caller keeps from an EdgeLists. The two fill forms of parkEdges and parkTapers take GroundLists and EdgeLists from the buffers, run layoutOf and groundOf into the GroundLists, and fill the caller's list. Make the returning parkEdges and parkTapers call them with buffers of their own.

Step 3: Build the sim and run its tests, as in task 6 step 6.

### Task 8: Read segment records without their lines

Files:
- Modify: `src/sim/park/edits.h:194-216`, `src/sim/park/edits.cpp`

Step 1: In edits.h, after IntentRecords, declare `void segmentRecords(const World &world, std::vector<SegmentRecord> &records);`, commented "Each segment's record, in ascending key order, into a list the caller keeps, which it clears first: what parkSegments gives for it, without its line. Doesn't derive the lines, and allocates nothing once the list has grown."

Step 2: In edits.cpp, implement it with forEachWith over the segment intent, setting the furnishing from the left and right furnishing intents as parkSegments does.

### Task 9: Fill doors, depots, and furniture into kept lists

Files:
- Modify: `src/sim/routes/networks.h:54-56`, `src/sim/routes/networks.cpp:158-176,557-559`
- Modify: `src/sim/operations/operations.h:166-168`, `src/sim/operations/operations.cpp`
- Modify: `src/sim/furniture/furniture.h:100`, `src/sim/furniture/furniture.cpp`

Step 1: In networks.h, after parkDoors, declare `void parkDoors(const World &world, PathKind kind, std::vector<ParkDoor> &doors, TickBuffers &buffers);`, commented "As the returning form, into a list the caller keeps, which it clears first, keeping the entrances and boxes it reads in the buffers. Allocates nothing once the lists have grown." In networks.cpp, give doorsServing a form that fills a kept list, keep the entrances and boxes in a struct the buffers hold, and make the returning parkDoors call the fill form.

Step 2: In operations.h, after nearestDepot, declare `std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop, TickBuffers &buffers);`, commented "As the returning form, keeping the lists it works with in the buffers. Allocates nothing once they have grown." In operations.cpp, keep the boxes, the depots' keys, and a RouteLengths in a struct the buffers hold, fill the depots' keys from parkBoxes' fill form in key order, and call the existing depotRouteBy that reads through kept lists. Make the returning nearestDepot call the kept form with buffers of its own.

Step 3: In furniture.h, after parkFurniture, declare `void parkFurniture(const World &world, std::vector<FurniturePiece> &pieces);`, commented "As the returning form, into a list the caller keeps, which it clears first. Allocates nothing once the list has grown." Move the body into it and make the returning form call it.

Step 4: Build the sim and run its tests, as in task 6 step 6.

### Task 10: Refill the tool park and release into a kept edit

Files:
- Modify: `src/tools/internal/tool_lists.h`, `src/tools/tools.h`, `src/tools/tools.cpp`

Step 1: In tool_lists.h, add `TickBuffers Buffers;` to ToolLists, commented "The lists the sim's readers keep for the tool park."

Step 2: In keepToolPark, fill Points, Segments, Corners, Edges, Tapers, GuestDoors, and BackstageDoors with their fill forms, passing park.Lists->Buffers where they take buffers. Boxes and the frontage's edge steps already refill.

Step 3: In tools.h, before tentativeEdit, add `struct KeptEdit`, commented "An edit the caller keeps from call to call, and the stops of the last path it held while it holds another edit, so switching between a path and another edit frees no list.", with `std::optional<ParkEdit> Edit;` and `std::vector<PathStop> SpareStops;`. Change tentativeEdit's fill form to take `KeptEdit &kept` in place of the optional, and say in its comment that it puts the edit in kept.Edit. In tools.cpp, add two helpers in the anonymous namespace. One makes kept.Edit an AddPath: if it holds another edit, it sets it to an empty AddPath and swaps its Stops with SpareStops. The other empties kept.Edit or sets it to another edit: if it holds an AddPath, it first swaps that path's Stops with SpareStops. Every place that sets or empties a KeptEdit's edit goes through these, including pathEdit, tentativeEdit, and releasePointer.

Step 4: Replace releasePointer with `void releasePointer(ToolState &tool, ToolPark &park, const World &world, KeptEdit &kept);`, commented "The primary button went up: puts the edit to commit in kept.Edit, or empties it. The edit is the tentative edit of the moment before when the press took hold." Use tentativeEdit's fill form in place of its returning form. Where it gives no edit, as when nothing is held, empty kept.Edit through step 3's helper, so a path the kept edit held keeps its stops.

Step 5: Build tpj_tools.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: exit status 0, no warning. The app's calls to tentativeEdit and releasePointer are fixed in task 16.

### Task 11: Keep the history's lists

Files:
- Modify: `src/tools/history.h`, `src/tools/history.cpp`

Step 1: In EditHistory, add `IntentRecords Now;` and `IntentChange Change;`, commented "The lists noting an edit reuses: the world's intent as it is now, and the change since the intent the history saw last." After redoEdit, declare `const RestoreIntent *takeUndo(EditHistory &history);` and `const RestoreIntent *takeRedo(EditHistory &history);`, commented "As undoEdit and redoEdit, giving the command the history keeps until its next undo or redo instead of a copy, or null where those give none."

Step 2: In history.cpp, make recordsOf fill a kept IntentRecords with parkPathPoints' fill form, segmentRecords, parkBoxes' fill form, and parkHours. Make changeBetween fill a kept IntentChange, clearing its lists first. Make reversal fill a kept RestoreIntent, clearing its lists first, so take refills history.Sent. take gives a pointer to history.Sent. takeUndo and takeRedo call take, and undoEdit and redoEdit copy what they give.

Step 3: noteEdits fills history.Now with recordsOf and history.Change with changeBetween, then swaps Seen and Now, so both keep their memory. Without a waiting step, if Change isn't empty, it moves Change onto the undo list and empties the redo list, as now. With a waiting step and an empty Change, it moves Taken back where it was, as now. Otherwise it renames as now, swaps Change and Taken, so the taken edit's memory holds the next change, and moves Taken onto the opposite list. clearHistory takes the world's intent into Seen with recordsOf's fill form.

Step 4: Build tpj_tools and run the undo test.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools tpj_integration_tests && build/windows-debug/tpj_integration_tests.exe "Undoing*"`
Expected: exit status 0, all tests passed.

### Task 12: Build the park mesh into kept lists

Files:
- Modify: `src/render/park_mesh.h:197-205`, `src/render/park_mesh.cpp:487-517`

Step 1: In park_mesh.h, before buildParkMesh, add `struct ParkMeshLists`, commented "The lists building the park mesh reuses, and the mesh: the entrances, boxes, and furniture, the buffers for the shops' supply routes, and the mesh.", with `std::vector<ParkEntrance> Entrances;`, `std::vector<ParkBox> Boxes;`, `std::vector<FurniturePiece> Furniture;`, `TickBuffers Buffers;`, and `ParkMesh Mesh;`. Replace buildParkMesh with `void buildParkMesh(const World &world, ParkMeshLists &lists);`, keeping its comment's first sentence and adding "The mesh is put in lists.Mesh. Uses lists the caller keeps, which it clears first, and allocates nothing once they have grown."

Step 2: In park_mesh.cpp, clear the mesh, read the entrances, boxes, and furniture with their fill forms into the lists, and draw the starved marks from lists.Boxes with nearestDepot's kept form, in the same order and colors as appendStarvedMarks. appendStarvedMarks stays for the ghost.

### Task 13: Build the overlay shape into kept lists

Files:
- Modify: `src/render/paving_overlay.h:58-62`, `src/render/paving_overlay.cpp:105-140,180-253`

Step 1: In paving_overlay.h, before buildOverlayShape, add `struct OverlayShapeLists`, commented "The lists building the overlay shape reuses, and the shape: each segment's record, the buffers for the search for overlaps, and the shape.", with `std::vector<SegmentRecord> Segments;`, `TickBuffers Buffers;`, and `OverlayShape Shape;`. Replace buildOverlayShape with `void buildOverlayShape(const World &world, OverlayShapeLists &lists);`, keeping its comment and adding "The shape is put in lists.Shape. Uses lists the caller keeps, which it clears first, and allocates nothing once they have grown."

Step 2: In paving_overlay.cpp, clear the shape's lists first. addLines reads the guest segments' keys and ends from segmentRecords instead of parkSegments, into kept lists. Keep the guest segments' keys, the ends, the owners, and findOverlaps' bounds, order, and pairs in a struct the buffers hold, filled with fillGrowing or cleared and refilled.

### Task 14: Pick into kept lists

Files:
- Modify: `src/render/picking.h:40-46`, `src/render/picking.cpp:89-111`
- Modify: `src/legible/inspect.h:60-70`, `src/legible/inspect.cpp:275-303`, `src/legible/preview.h:57-59`

Step 1: In picking.h, before entityAtCursor, add `struct PickLists`, commented "The lists picking reuses: the entrances, boxes, and guests.", with `std::vector<ParkEntrance> Entrances;`, `std::vector<ParkBox> Boxes;`, and `std::vector<EntityKey> Guests;`. Add `PickLists &lists` as entityAtCursor's last parameter, and add to its comment "Uses lists the caller keeps, and allocates nothing once they have grown." In picking.cpp, read them with parkEntrances', parkBoxes', and parkGuests' fill forms.

Step 2: In inspect.h, add `TickBuffers &buffers` as the last parameter of inspectorSubject and pickSubject, and add to pickSubject's comment "Keeps the lists it works with in the buffers, and allocates nothing once they have grown." In inspect.cpp, inspectorSubject calls shopRecord's form with buffers, and finds a bench or toilet block with parkBox instead of parkBoxes.

Step 3: In preview.h, in keepPreview's comment, replace "empty it when the world is replaced, or changed without a tick, as edits applied while paused change it." with "when the world is changed without a tick, as edits applied while paused change it, clear the kept preview's Tick, so the next call makes it again, keeping its lists. Empty it when the world is replaced."

Step 4: Build tpj_render and tpj_legible.

Run: `cmake.exe --build --preset windows-debug --target tpj_render tpj_legible`
Expected: exit status 0, no warning.

### Task 15: Keep the scene's meshes, and read the look from the tool park

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`
- Modify: `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`

Step 1: In SceneSync, add `ParkMeshLists ParkLists;` and `[[nodiscard]] const ParkMesh &parkMesh() const { return ParkLists.Mesh; }`, commented "The park as the last syncWorld that needed it drew it." Replace `OverlayShape Shape;` with `OverlayShapeLists ShapeLists;`, and make overlayShape() give ShapeLists.Shape. Remove LookBoxes, LookPoints, and LookSegments.

Step 2: syncWorld builds the park mesh with buildParkMesh into ParkLists whenever it says the park mesh is needed. Where the edit count differs, replace `Kept = {};` with `Kept.Tick.reset();`. Keep `Kept = {};` for a new generation. Update syncWorld's comment: "Whenever the park mesh is needed, it is built. ... When the edit count differs, the kept preview's tick is cleared, so an edit applied without a tick is previewed afresh."

Step 3: syncLook takes `const ToolPark &park` after the world, and builds the look's marks from park.Boxes, park.Points, and park.Segments. Its comment adds "The look's marks read the park as the tools read it, which the caller keeps filled from the same world."

Step 4: syncOverlayShape builds the shape with buildOverlayShape into ShapeLists. While not shown, it clears each of the shape's lists in place instead of assigning an empty shape, and still says the shape changed only when it held pieces.

Step 5: Replace uploadWorldMeshes with `bool uploadWorldMeshes(Renderer &renderer, const SceneSync &scene, const WorldMeshes &meshes, OrbitCamera &camera);`, commented "Gives the renderer the meshes the frame needs, as the scene's sync built them: the park mesh and the guest mesh. Frames the camera on the park mesh's bounds when asked and the mesh has vertices. False if an upload failed." It reads scene.parkMesh() and scene.guestMesh().

### Task 16: Keep the pick, the release, and the undo's command

Files:
- Modify: `src/app/input/interaction.h`, `src/app/input/interaction.cpp`
- Modify: `src/app/input/cursor.h:42-46`, `src/app/input/cursor.cpp:61-68`
- Modify: `src/app/session/park_session.h:80-90`, `src/app/session/park_session.cpp:33-70,110-118`
- Modify: `src/app/application.cpp:72-77,97-100,135-137`

Step 1: In cursor.h, add `PickLists &lists` as entityUnderCursor's last parameter and pass it on.

Step 2: In Interaction, add `[[nodiscard]] const ToolPark &park() const { return Park; }`, commented "The park as the tools read it, filled again whenever the generation or the edit count changes." Replace pick with `void pick(const World &world, const SeatedGuests &seated, const std::optional<CursorNdc> &cursor, const CameraView &view);`, commented "Sets the subject from the entity under the cursor, as entityUnderCursor finds it and pickSubject keeps it, with lists the interaction keeps." Replace the private member `std::optional<ParkEdit> Edit;` with `KeptEdit Edit;`, and add `KeptEdit Released;`, `PickLists Picks;`, and `TickBuffers PickBuffers;`. tentativeEdit fills Edit and gives Edit.Edit. useButtons releases into Released and queues Released.Edit when it holds an edit.

Step 3: In ParkSession, replace `std::optional<RestoreIntent> Restore;` with `const RestoreIntent *Restore = nullptr;`, commented "The command the history keeps for the undo or redo waiting to apply, or null.", and add `CommandQueue Alone;`, commented "The queue an undo or redo applies alone on, kept between them." undo and redo take the command with takeUndo and takeRedo. step and applyWithoutTick queue *Restore on Alone, set Restore to null, and apply Alone, which the cycle clears. replace sets Restore to null.

Step 4: In application.cpp, call Interactions.pick with the world, Scene.seated(), readCursor(Window.get()), and orbitCameraView(Camera). Call uploadWorldMeshes with Scene. Call Scene.syncLook with Interactions.park() after the world.

Step 5: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: exit status 0, no warning.

### Task 17: Time the kept forms in the bench

Files:
- Modify: `src/bench/stages.cpp:70-75,108-118`

Step 1: The food-overlay-shape and park-mesh stages build into lists kept across repetitions, as the app keeps them, and report the vertex count of the shape and of the mesh. The food-overlay stage shades the shape from those lists.

Step 2: Build tpj_bench.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench`
Expected: exit status 0, no warning.

### Task 18: Watch the work after undos and redos

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`, `tests/checks/allocations/CMakeLists.txt`

Step 1: SteadyPlay gains `tpj::ToolPark Park;`, filled with keepToolPark after addCurve. syncPreview and syncAndDraw take it and pass it to syncLook. drawScene's call to uploadWorldMeshes passes the scene.

Step 2: Add `constexpr uint64_t WARM_ROUNDS = 4;` and `constexpr uint64_t WATCHED_ROUNDS = 20;`, commented "Rounds of undos and redos to warm up, and to watch." Add `struct EditRounds`, commented "The work after an undo or a redo: an interaction that picks and presses each tool, a scene that shows the Food overlay and inspects a shop, a second history that follows the session's world, the queue the releases put their edits on, and the places they press and pick at.", holding `tpj::Interaction Tools`, `tpj::SceneSync Scene`, `tpj::PreviewLook Look`, `tpj::EditHistory History`, `tpj::CommandQueue Queued`, the ToolPlaces, and the shop's and the first bench's positions.

Step 3: Add setup that, after the steady ticks, clears EditRounds' history on the session's world, queues an AddPath of a guest path from (-120, -100) to (-110, -100) with two NewPoint stops on session.commands(), applies it with session.applyWithoutTick, and notes it on the second history. It throws "the open-ground path was refused" when the edit count doesn't change. The look shows the Hunger overlay, inspects the first shop, highlights the guest segment's From point, and hovers the guest segment's sides.

Step 4: Add `void editRound(EditRounds &rounds, HiddenFrame &frame, tpj::ParkSession &session, bool undo)`, commented "One undo or redo and the app's work after it. The watch pauses while the session takes and applies the command, and watches the second history take and note the same one." In order, it:

- under "the history", takes an undo or redo from the second history with takeUndo or takeRedo, throwing "the history has nothing to undo or redo" on null;
- pauses the watch, calls session.undo() or session.redo() and session.applyWithoutTick(), and resumes it, throwing "the undo or redo did not apply" when the edit count doesn't change;
- under "the history", notes the change with noteEdits on the second history;
- under "the tool park", calls rounds.Tools.follow;
- under "the scene's sync", syncs the scene's world and draws it with drawScene, then three frames with the overlay shown, hidden, and shown again, each as the app runs them: syncPreview with no edit, following ticks while shown, syncOverlayShape twice, syncLook with rounds.Tools.park(), then uploadPreviewMeshes and drawWaited;
- under "the picking", picks at the screen's middle, with a CursorNdc at (0, 0) and aspect 16/9, from a CameraView whose eye lies 60 m above and 30 m south of the shop when undoing and of the bench when redoing, looking at it;
- under "the presses", for each press in the list below: selects the tool, moves the pointer to the place, takes the frame's tentative edit as the app's frame does, presses, and releases, with useButtons on rounds.Queued, then pauses the watch, clears rounds.Queued, and resumes it. For the guest path, the first press is at the ToolPlaces' Start; then, with the watch paused, it moves to Along and presses, which asks the world whether it accepts the path; then, watched, it presses at Along again, which finishes the path, and releases. Last, it selects the Look tool.

The presses: Place shop beside the guest segment; Move box on the shop; Edit path on the guest segment's point, the corner's radius handle, the taper's handle, the first edge's handle, the guest segment's side, and the guest segment's middle; Remove point on the point; Delete on the shop; Furnish on the guest segment's middle; and the guest path.

Step 5: Make SteadyPlay in main and pass it to checkTicks, so its HiddenFrame, with the renderer's buffers the ticks grew, outlives the ticks. Add `int checkEdits(SteadyPlay &play, tpj::ParkSession &session)`, commented "Undoes and redoes a guest path on the session, running the app's work after each in the play's hidden frame, and reports the blocks the watched rounds freed.", that makes an EditRounds, runs the setup, WARM_ROUNDS rounds, throws "the first pick picked nothing" when the interaction has no subject after them, starts watching, runs WATCHED_ROUNDS rounds alternating undo and redo, starting with an undo, and stops watching.

Step 6: Split report into `int reportFreed(const char *during)`, which prints the overflow line or the freed blocks as now, with "during the watched ticks" or "during the watched rounds", and gives 1, or prints nothing and gives 0. main runs checkTicks, then checkEdits with the same SteadyPlay only when the ticks freed nothing, then prints `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, the panels, the uploads, and the drawing, and 20 undos and redos with the app's work after each, freed no block they allocated` and returns 0 when both freed nothing.

Step 7: In the file's header comment, add "After the ticks, it undoes and redoes a guest path, and watches the app's work after each, with the watch paused while the session applies the command."

Step 8: In CMakeLists.txt, rename the test to "allocation check: steady play and edits on full.park free no block they allocated", in both places.

Step 9: Build and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing step 6's line. If a block is freed, fix the code that frees it, then run again. If the check finds a free in the sim's or SDL's own code that runs at an action, stop and report it as a deviation; don't exclude it.

### Task 19: Prove the rounds see the history

Files:
- Modify, then revert: `src/tools/history.cpp`

Step 1: Make criterion 1's edit at the start of noteEdits.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines starting `allocation check: ` that say a block `the history` allocated was freed during the watched rounds.

Step 2: Revert the edit and rebuild, and see task 18's line again.

### Task 20: Confirm the frame, the simulation, the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 300 --speed 0 --overlay food --frames 5 --capture build/quiet-edits/after.bmp && cmp build/quiet-edits/before.bmp build/quiet-edits/after.bmp`
Expected: exit status 0, and cmp prints nothing.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-edits/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-edits/scenarios-before.txt build/quiet-edits/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-edits/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-edits/full-before.txt build/quiet-edits/full-after.txt`
Expected: exit status 0.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play and edits on full.park free no block they allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 21: Compare the runtime report

Files: none.

Step 1: main hasn't changed since build/allocation-free-loops-after.report was written, so it serves as the report before.

Run: `scripts/runtime-report.sh build/quiet-edits-after.report > build/quiet-edits-after.log 2>&1 && scripts/runtime-report.sh --compare build/allocation-free-loops-after.report build/quiet-edits-after.report`
Expected: exit status 0. The comparison marks no stage result-changed. Report each stage's change. food-overlay-shape and park-mesh now reuse their lists.

### Task 22: Play

Files: none.

Step 1: Build the whole windows-release preset: `cmake.exe --build --preset windows-release`. Expected: exit status 0 and no warning.

Step 2: Ask the user to play criterion 6's play check in build/windows-release/ThemeParkJones.exe on tests/parks/stress/full.park. Fix what the play finds directly.

### Task 23: Review and commit

Step 1: Stage the feature's paths by name, never with `git add -A`. Dispatch the reviewer on the staged diff with FEATURE.md, the changed SPEC.md files, and docs/principles.md.

Step 2: Commit once with the commit-hygiene skill, subject `App: Keep the work after edits, picks, and presses`.
