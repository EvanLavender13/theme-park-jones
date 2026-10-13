# Feature: Kept Preview

## Summary

kept-preview makes the preview's per-frame and per-tick work reuse its memory while the edit stays the same, and adds it to the allocation check. This work covers the kept preview, the frame's ghost, and the overlay's shape check and shades. Today the app builds the whole ghost again at each tick while the Inspector follows a guest. It shades the overlay again whenever it builds the ghost. It derives every segment's line twice a frame to check the overlay's shape. And it reads a shop ghost's footfall into new lists every frame.

After this feature, the scene keeps the edit's ghost until the preview is made again, keeps the look's marks until the look or the park changes, and rebuilds only the inspected guest's mark at each tick. It checks the overlay's shape by the edit count and, when the preview holds a candidate of an edit that can change paths, by the edit, shades again only when the shading's inputs change, and shades into lists it keeps. The ghost and shade building moves from the tpj_app executable into SceneSync in tpj_app_core, so the check can run it.

A preview without a candidate is no longer made again at each tick, because nothing in it changes with a tick. While an overlay is shown and the preview holds a candidate, the preview and its edit's ghost are still made again at each tick, and changing-previews takes that. What the app shows doesn't change. This covers criterion 5 of the milestone.

## Acceptance criteria

1. The allocation check runs the preview and the overlays as the app does. It holds two edits still with no overlay shown: a shop moved onto its own place and a segment deletion. For each, it keeps the preview and builds the frame's ghost after every watched tick, with a look that has a highlight, handles, a path start and finish, a hovered segment's sides, and an inspected guest. It also shows each need's overlay with no edit, and the food overlay with a guest path the world refuses, keeping the preview and the shape and shading at every 60th tick. Freed blocks are reported as `the preview` or `the overlay`, in the report line quiet-scene gave. If nothing is freed, the check prints `allocation check: 600 ticks of full.park, the scene's sync, the preview, and the overlays freed no block they allocated` and exits with status 0. (check: in SceneSync::syncLook, in src/app/scene/scene_sync.cpp, add `const std::vector<uint64_t> held{world.Tick};` and `static_cast<void>(held);`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that start `allocation check: the preview freed` and lines that start `allocation check: the overlay freed`; then revert)
2. With the edit unchanged and no overlay shown, keeping the preview and putting together the frame's ghost free no block they allocated over the check's watched ticks of full.park. While an overlay of any need is shown and the preview holds no candidate, keeping the preview, checking the shape, and shading free none either. (ctest)
3. The scene and the overlay draw what they drew before. A capture of full.park, paused after 600 ticks with the food overlay shown, is byte-identical before and after the feature. Task 1 checks that two such captures from one build are byte-identical. (check: the capture commands in PLAN.md tasks 1 and 16)
4. The ghost, the overlay, and the Inspector's highlight behave as before. (play: open tests/parks/stress/full.park in the windows-debug app. Choose Look and click a guest, then choose Place shop and rest the pointer beside a path: the shop's ghost and the guest's highlight show, and the highlight follows the guest as it walks. Show Food in the Overlay row: the overlay changes as shops serve, and the ghost and the overlay's tooltip agree. Choose Edit path: point markers and handles show, and the handle under the pointer is highlighted. Pause and place a guest path: its ghost goes, the path shows at once, and the overlay shades it.)
5. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md task 15)
6. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: the three commands in PLAN.md task 18)

## Tests

None. The allocation check covers the work, and criterion 1's check proves it sees it.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows. It samples hungry footfall, guest route distance, and the four offers as it does today, through filling forms that keep their lists.

## Spec changes

src/sim/medium/SPEC.md:

- Replace "Kept fields have no such form." with "A kept field's entries at a place can fill a KeptFieldSample the same way, and fieldValue can sum them from one. Neither allocates once the sample's lists have grown."
- After "sampleFieldAtPlaces keeps nothing after it returns.", add "For a field that isn't kept, it can instead take a FieldSample the caller keeps, and then allocates nothing once the sample's lists have grown."

src/sim/park/SPEC.md:

- Replace "Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, parkCorners, and parkBoxes," with "Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, parkSegmentKeys, parkCorners, and parkBoxes,".
- Replace "Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, ParkCorner, ParkBox, ParkEdge, and ParkTaper." with "Each gives every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, ParkCorner, ParkBox, ParkEdge, and ParkTaper. The exception is parkSegmentKeys, which gives only the keys of one kind's segments."
- After "parkEntrances and parkBoxes can also put their values in a list the caller keeps, clearing it first, and then allocate nothing once the list has grown.", add "parkSegmentKeys puts its keys in such a list, without deriving the segments' lines."
- After "Other modules change intent only through commands.", add "changesPaths(edit) says whether applying an edit can change a path point or segment. It is false for AddBox, MoveBox, DeleteBox, SetShopStock, and SetOpeningHours, and true for every other edit."

src/legible/SPEC.md:

- In Previews, replace "If the caller follows ticks, it also makes it again when the world's tick has changed." with "If the caller follows ticks and the kept preview holds a candidate, it also makes it again when the world's tick has changed. A preview without a candidate holds nothing a tick changes, because whether the world accepts an edit depends only on the park's intent."
- After "and reads the shop context's footfall from the world again.", add "Once its lists have grown, keeping the preview allocates nothing."
- In Availability, after "It keeps nothing after it returns.", add "availabilityValues and availabilityBends can also use AvailabilityLists that the caller keeps, and then allocate nothing once its lists have grown."

src/render/SPEC.md:

- In Paving overlay, after the paragraph starting "shadeOverlay(shape, places, values, half) gives the shades", add "shadeOverlay can also fill OverlayShades that the caller keeps, using ShadeLists the caller keeps, and then allocates nothing once they have grown."
- In Ghosts and highlights, after the paragraph starting "appendHandle adds a radius", add "appendMesh adds one mesh's triangles after another's, so a ghost can be put together from parts."

src/views/SPEC.md: after the Availability overlays paragraph, add "availabilityShades can also fill AvailabilityShadeLists that the caller keeps, and then allocates nothing once its lists have grown."

src/app/SPEC.md, Tools and the ghost:

- Replace "The preview is made again whenever the park's intent changes or the edit changes, and, while an overlay is shown, whenever the world ticks." with "The preview is made again whenever the park's intent changes or the edit changes, and, while an overlay is shown and the world accepts the edit, whenever the world ticks. While an overlay is shown, it is shaded again whenever the world ticks."
- After "so the guest's highlight follows it as it walks, sits, and stands up.", add "Once their lists have grown, keeping the preview, the ghost, and the overlay up to date frees no memory they allocated while the edit and the tool's look stay the same, as they do while the pointer rests. The exception is an overlay shown while the world accepts the edit, where the preview is made again at each tick. The allocation check holds a stretch of full.park's ticks to it, with edits held still and with each need's overlay shown."

## Files affected

Modify:

- src/sim/medium/SPEC.md, src/sim/medium/field.h, src/sim/medium/kept_field.h
- src/sim/guests/footfall.h, src/sim/guests/footfall.cpp
- src/sim/park/SPEC.md, src/sim/park/intent.h, src/sim/park/intent.cpp, src/sim/park/edits.h, src/sim/park/edits.cpp
- src/legible/SPEC.md, src/legible/availability.h, src/legible/availability.cpp, src/legible/preview.h, src/legible/preview.cpp
- src/render/SPEC.md, src/render/paving_overlay.h, src/render/paving_overlay.cpp, src/render/park_mesh.h, src/render/park_mesh.cpp
- src/views/SPEC.md, src/views/availability_overlay.h, src/views/availability_overlay.cpp
- src/app/SPEC.md, src/app/CMakeLists.txt, src/app/scene/scene_sync.h, src/app/scene/scene_sync.cpp, src/app/scene/scene_uploads.h, src/app/scene/scene_uploads.cpp, src/app/application.cpp
- tests/checks/allocations/allocation_check.cpp

## Dependencies

- quiet-scene: the check's work names and SceneSync's edit count. Merged.
- tests/parks/stress/full.park.

## Out of scope

- Making the preview again at each tick while an overlay is shown and the preview holds a candidate. changing-previews takes it.
- Building the edit's ghost, which happens whenever the preview is made again, including at each tick while an overlay is shown and the preview holds a candidate. changing-previews takes it.
- Building the look's marks, which happens when the look or the park changes. quiet-tools takes pointer movement.
- The tools' look itself, such as Edit path's handles and the highlight (quiet-tools), and the tooltips, the Inspector, and the Graph view (quiet-panels).
- Uploading the ghost, the shape, and the shades to the GPU.

## Open questions

None.
