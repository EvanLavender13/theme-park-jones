# Milestone: Allocation-Free Loops

Slice: none

Status: planned

## Summary

allocation-free-loops makes docs/conventions.md's rule against allocating in per-tick and per-frame work hold while the park plays, and builds the check that decision 0041 calls for, so it keeps holding. Steady play is the park running with nothing changing but the clock, the pointer's hover, and the views. In steady play the app repeats the same work every frame and every tick, and RESEARCH.md lists all of it from the app's frame loop. An audit and this milestone's reviews found that work allocating and freeing memory in dozens of places (RESEARCH.md lists them). The milestone makes each part of that work reuse its memory, and puts each part in the allocation check. Each feature adds its area to the check and makes that area pass in the same change, so the check never fails on main. Previews of an edit that changes every frame stay outside, for the changing-previews milestone, and so does the preview an overlay makes again at each tick. The milestone comes before every feature in other capabilities, as decision 0041 says.

## Acceptance criteria

1. tests/checks holds the allocation check, an executable of its own that counts every heap allocation and free. Its source has a place in cmake/check_layers.cmake, as tests/integration does, so the layer check passes. It loads tests/parks/stress/full.park and warms each piece of work up. It then repeats the work with the same buffers and fails if a heap block is both allocated and freed during the repeats, naming the work and, for ticks, the tick. It runs in `ctest.exe --preset windows-debug`. (ctest; check: inside stepGuests' loop over guests, add a temporary std::vector that holds the guest's key, confirm that the check fails and names a tick, and revert)
2. Adding and removing components and entities during play reuses the world's storage, so a stretch of ticks frees no block it allocated for them. That includes a component's own heap members: seat service holds its free seats in a fixed-size array, since a piece has at most a few seats. Saves, hashes, equality, and the order of every walk are unchanged. (ctest: the check's ticks)
3. Stepping full.park through a stretch of ticks frees no block it allocated. The check steps through ParkSession::step, the app's own stepping. tpj_bench and tpj_scenarios keep their buffers across ticks too, so tpj_scenarios's comparison runs the path the app runs. (ctest; check: run tpj_bench and tpj_scenarios, and read where each keeps its buffers)
4. The scene's sync at each tick frees no block it allocated: finding whether intent changed, the seated guests, and the guest mesh. (ctest)
5. With the edit unchanged and no overlay shown, the kept preview at each tick frees no block it allocated, and neither does putting together the frame's ghost with its highlight, handles, and the inspected guest's mark. While an overlay of any need is shown and the preview holds no candidate, shading it at each tick frees no block it allocated, and neither does checking its shape at each frame. That work moves out of the tpj_app executable into tpj_app_core, so the check can run it. While an overlay is shown and the preview holds a candidate, the preview is made again at each tick, and changing-previews takes that. (ctest)
6. With the pointer held at fixed places, a frame's tool work frees no block it allocated. That covers the pointer's snapping and frontage search, the tentative edit, the highlight, a path tool's marks, and Edit path's handles and hovered handle, for each tool. (ctest)
7. The panels' data, the Inspector's inspection of a guest, the overlay tooltip's availability, the shop ghost's context, and the Graph view's overlay, repeated, free no block they allocated. The panels' data, such as the guest count, moves out of the tpj_app executable into tpj_app_core, so the check can run it. The app keeps frame times only while --frame-times is on, so ordinary play keeps no list that grows every frame. (ctest)
8. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after each feature shows no difference. The runtime report on the stress parks is compared before and after the milestone. Each stage's change is reported, and no stage is marked result-changed. `--frame-times` on full.park with the window idle is reported before and after. (check: tpj_scenarios --compare; scripts/runtime-report.sh and its --compare)
9. Uploading to the GPU in steady play reuses its buffers: the guest mesh at each tick, the overlay's shades at each tick, and the ghost when the look changes. Each upload writes into the buffers the renderer keeps, and makes larger ones only when the data outgrows them. It allocates and frees no heap block and no GPU buffer while the sizes stay within what they have held. Drawing a frame without a capture frees no block it allocated either. (ctest)
10. Every SPEC.md whose public functions take kept buffers says so. src/sim/SPEC.md says that stepWorld's caller keeps its buffers. docs/testing.md and docs/conventions.md describe the check as decision 0041 does. The windows-debug build has no warnings, ctest passes, and tidy is clean.
11. The app's work at a player action reuses its memory: after an edit, an undo, or a redo applies, the park mesh, the overlay's shape, and the scene's lists of boxes, path points, and segments are refilled in the lists the app keeps, and a click's picking and a press's own work keep theirs. Resolving the world at that action stays with changing-previews. (check, in PLAN.md of quiet-edits)

## Medium

None. The milestone adds, samples, and publishes no fields or flows, and changes no answer the simulation or the display gives. Kept buffers are never part of a world: they aren't walked, hashed, saved, or compared.

## Dependencies

- Decision 0041.
- Decision 0039, keyed component storage: its key order stays.
- measured-runtime: full.park, tpj_bench, and the runtime report. Complete.
- The private-header and layer checks in tests/checks. Complete.

## Core feature

quiet-tick. It builds the check, and makes the world's storage and the tick pass it, criteria 1 to 3. The tick runs whenever the park runs, whatever the player is doing, and its worst allocation, a guest's choice, grows with the guests. Once it lands, no change can add an allocation to the tick without failing ctest.

## Features

1. `quiet-tick`: the allocation check, the world's storage reused as components and entities come and go, and the tick with kept buffers, for criteria 1 to 3. Depends on: none.
2. `quiet-scene`: the scene's sync at each tick, for criterion 4. Depends on: feature 1.
3. `kept-preview`: the kept preview, the overlay's shades and shape, and the frame's ghost with the edit unchanged, for criterion 5. Depends on: feature 1.
4. `quiet-tools`: each tool's per-frame work with the pointer held still, for criterion 6. Depends on: feature 1.
5. `quiet-panels`: the panels' data, the Inspector, the tooltips, the Graph view, and the frame-time log, for criterion 7; and the edits to docs/testing.md and docs/conventions.md in criterion 10. Depends on: features 1 to 4.
6. `quiet-uploads`: the renderer keeps each GPU buffer with a transfer buffer of its own and grows them only when outgrown, records uploads on the frame's command buffer, and the allocation check uploads and draws in a hidden window, counting SDL's allocations, for criterion 9. Depends on: none.
7. `quiet-edits`: the park mesh, the overlay's shape, the scene's lists, picking, and presses keep their memory when a player action applies, for criterion 11. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

- A frame check of the app itself: run the app hidden with scripted pointer input and count allocations per frame, including ImGui's own drawing. Gated on: scripted input for the app.

## Leftovers

What the milestone left outside its criteria and the allocation check, with the code read for each.

Owned by changing-previews, the capability's milestone 6, still to be planned. Each of these allocates today, in ordinary play. They run at every frame the tool's edit changes: while the pointer moves with a place tool (tools.cpp otherEdit poses the box at the pointer), while it moves with a path tool that has stops drawn (pathEdit adds the pointer's stop), and while anything is dragged. They also run at each tick while an overlay is shown and the preview holds a candidate:

- Making the preview again: resolving a new candidate world (scene_sync.cpp syncPreview, legible/preview.cpp keepPreview and previewEdit).
- Building the edit's ghost whenever the preview is made again (scene_sync.cpp syncLook). buildGhostMesh allocates as a whole: it returns a new GhostMesh, and among its lists it copies the park's furniture twice in appendFurnitureChanges, the boxes for a MoveBox, and the segments in appendDeleted, and makes temporary lists in appendJoinMarks, ownersOf, appendWalkways, appendStraightPath, appendStarvedMarks, and the Remove point ghost's appendEntity per segment (park_mesh.cpp).
- Building the overlay's shape again, while an overlay is shown and a path edit changes: buildOverlayShape returns a new OverlayShape (scene_sync.cpp syncOverlayShape).
- The need tooltip on a candidate. Its own lists are kept, but the candidate it reads is made again at each tick.
- Applying a command at a tick, which resolves.

Owned by quiet-edits, feature 7. Each runs once per player action and rebuilds what it could refill:

- Committing an edit, undo, and redo: buildParkMesh returns a new ParkMesh (scene_uploads.cpp uploadWorldMeshes), buildOverlayShape returns a new OverlayShape (scene_sync.cpp syncOverlayShape), and the scene reads the path points and segments into new lists (scene_sync.cpp syncLook).
- Presses and releases: a press's own work apart from asking the world whether it accepts a path, which resolves and stays with changing-previews (tools.cpp).
- Picking a subject on a click (picking.cpp entityAtCursor).

Left as they are, since each makes something new by nature and runs once per player action:

- Opening and saving a park, and the file dialogs (park_session.cpp useFileRequest). Each frame takes an empty request, which moves an empty string and allocates nothing.
- --capture's readback, which runs once for the last frame (renderer.cpp submitWithCapture).

Unchecked while the pointer or the view moves, owned by the deepening candidate above, a frame check of the app with scripted input. The allocation check holds the pointer and the camera still. The code keeps its lists for a moving pointer and view too, but nothing runs it moving:

- The tools' own work at a moving pointer: snapping, the frontage search, Edit path's handles and hovered handle, and the look's marks, which fill the lists ToolLists and SceneSync keep (tools/tools.cpp, scene_sync.cpp syncLook).
- The Graph view at a moving camera, which fills the GraphOverlay and keys PanelData keeps, and the need tooltip at a moving point, which fills PanelData's overlay lines and availability lists (panel_data.cpp).

Outside the check, read and following the rule:

- ImGui's own drawing. Its SDL_GPU backend keeps its vertex, index, and transfer buffers and grows them only when outgrown (imgui_impl_sdlgpu3.cpp CreateOrResizeBuffers). ImGui reuses its draw lists from frame to frame. The deepening candidate above would put it in a check.
- The app's own per-frame code that only the app runs: gatherInput, readCursor, ToolingUi::build and each panel's drawing, and the snap guides' labels. They format into stack buffers or pass literals and allocate nothing. The Debug panel copied each need's name into a std::string every frame. The names fit libstdc++'s short-string buffer, so it reached no heap, but it was a copy at every frame, and it now formats into a stack buffer.
- The GPU driver's own memory, which neither hook sees. SDL suballocates device memory itself, and each buffer or region it makes goes through SDL_malloc, which the check counts.

Fixed at closing: tpj_bench's food-overlay and guest-mesh stages timed the returning forms, which made new lists at each repetition, while the app keeps them. The stages now keep their lists across repetitions as the app does, and the returning forms of availabilityShades and buildGuestMesh are gone, since nothing else called them (bench/stages.cpp).

Owned by the capability's backlog:

- stepShops samples backstage route distance at every tick for each shop's nearest depot, which changes only at a resolution. It allocates nothing, but it repeats work ("Depot routes kept per resolution").
- Drawing guests from one shared mesh instead of rebuilding and uploading a mesh of all guests at every tick ("Instanced guests").

## Open questions

None.

## Research notes

- The work steady play repeats comes from the app's frame loop, so the check covers what the player's machine actually does each frame.
- The check fails if a heap block is both allocated and freed during repeated work. That catches temporary lists and lists rebuilt every time. A list moving to a larger block passes, since the conventions let a list grow.
- A kept candidate can't follow ticks without resolving, so the preview an overlay makes again at each tick goes to changing-previews.
- The world's storage allocates a node for each component added. On full.park, seat service comes and goes as pieces fill and empty, so the storage must reuse its nodes.

The details are in RESEARCH.md.
