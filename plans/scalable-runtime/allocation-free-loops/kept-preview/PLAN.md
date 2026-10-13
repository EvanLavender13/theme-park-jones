# Implementation Plan: Kept Preview

## Goal

Make the kept preview, the frame's ghost, and the overlay's shape check and shades free no block they allocated in steady play on full.park, and run them in the allocation check, with the scene and the simulation unchanged.

## Approach

SceneSync keeps the parts of the frame's ghost and rebuilds each only when its inputs change: the edit's ghost when the preview is made again, the look's marks when the look or the edit count changes, and the inspected guest's mark when the guests are rebuilt. It then puts the frame's ghost together into kept meshes. It checks the overlay's shape by the edit count, and by the preview's edit only when the preview holds a candidate of an edit that can change paths, and shades again only when the preview, the shape, the need, or the previewed world's tick changes, into kept lists. The sampling and shading below it get filling forms that keep their lists, and keepPreview stops following ticks when the preview holds no candidate.

## Placement

Decision 0027 places each behavior this feature adds:

- A kept field's filling sample and the node rule appending to a list: sim/medium, kept_field.h. It owns kept fields' sampling. Hungry footfall's node rule in sim/guests/footfall.h follows the new signature.
- sampleFieldAtPlaces with a kept FieldSample: sim/medium, field.h, which owns field sampling and FieldSample.
- parkSegmentKeys: sim/park, intent.h, the one place other modules read intent.
- changesPaths: sim/park, edits.h, which owns the edits and what each applies.
- AvailabilityLists and the filling forms of availabilityBends and availabilityValues: legible, availability.h, which owns availability.
- keepPreview's kept footfall sample, and following ticks only with a candidate: legible, preview.h, which owns the kept preview.
- shadeOverlay into kept lists, and appendMesh: render, paving_overlay.h and park_mesh.h, which own the shades and the meshes.
- availabilityShades into kept lists: views, availability_overlay.h, which joins availability to the shades.
- Keeping the ghost's parts and the shades, and checking the shape by edit count and edit: app, SceneSync in app/scene/scene_sync.h (tpj_app_core). SceneSync already owns the kept preview, the overlay's shape, and when the ghost is rebuilt. Building there moves the work out of the tpj_app executable, so the check runs it. uploadPreviewMeshes in app/scene/scene_uploads.cpp only uploads. src/app/application.cpp changes its calls, composition only.
- Running the preview and the overlays in the check: tests/checks/allocations/allocation_check.cpp.

## Tasks

### Task 1: Record the simulation and captures before the change

Files: none.

Step 1: On main, build tpj_scenarios and the app, and record.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `mkdir -p build/kept-preview && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/kept-preview/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/kept-preview/full-before.txt`
Expected: exit status 0.

Run: `for n in 1 2; do build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 600 --speed 0 --overlay food --capture build/kept-preview/capture-before-$n.bmp; done && cmp build/kept-preview/capture-before-1.bmp build/kept-preview/capture-before-2.bmp`
Expected: each run prints `GPU driver: vulkan`, and cmp exits with status 0 and prints nothing. If cmp reports a difference, the capture can't show a drawing change, so treat it as a deviation.

### Task 2: Update the specs

Files:
- Modify: `src/sim/medium/SPEC.md`, `src/sim/park/SPEC.md`, `src/legible/SPEC.md`, `src/render/SPEC.md`, `src/views/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each edit FEATURE.md's Spec changes lists, with its sentences as written there.

### Task 3: Sample a kept field into kept lists

Files:
- Modify: `src/sim/medium/kept_field.h`, `src/sim/guests/footfall.h`, `src/sim/guests/footfall.cpp`

Step 1: Change the node rule to `static void sampleNode(const NodeSample &sample, std::vector<double> &out)`, appending its values to out, as sampleEdge does. Update HasNodeRule and its comment, HungryFootfall's declaration and definition, and sampleKeptSlotAtNode.

Step 2: Declare `KeptFieldSample`: the slots, a SampleLists<double>, a NodeSample, the node ends it isn't using, the node rule's values, and the entries. Comment that a kept field's sampleField fills its entries, and that it allocates nothing once its lists have grown. Let sampleKeptSlotAtNode fill the NodeSample it is given. To reuse an end, move it from the unused ends and clear its Along. When done, move the sample's ends back to the unused ends, as swapSteppedEntries keeps retired slots, so that each end's list keeps its memory.

Step 3: Add `void sampleField(const World &, const Network &, const Place &, KeptFieldSample &)` for kept fields, filling the sample's entries, which it clears first. Add `double fieldValue(const World &, const Network &, const Place &, KeptFieldSample &)`, summing them as fieldValue does. Make the returning forms use their own lists, so their results stay the same.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 4: Sample a field at many places into kept lists

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Add a form of sampleFieldAtPlaces that takes a `FieldSample<typename F::Entry> &` after the visitor, for fields that aren't kept (static_assert as sampleField's filling form does). Fill the sample's Slots with layeredSlots, clearing them when the holder isn't live, and pass its Lists to sampleSlotsAt. Comment: "As sampleFieldAtPlaces, keeping its working lists in the sample. Allocates nothing once they have grown."

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 5: List segment keys, and tell edits that change paths

Files:
- Modify: `src/sim/park/intent.h`, `src/sim/park/intent.cpp`, `src/sim/park/edits.h`, `src/sim/park/edits.cpp`

Step 1: After the filling forms of parkEntrances and parkBoxes, declare `void parkSegmentKeys(const World &world, PathKind kind, std::vector<EntityKey> &keys)`, commented: "The keys of the segments of the kind, ascending, into a list the caller keeps, which it clears first. They are the keys parkSegments gives for that kind. Allocates nothing once the list has grown."

Step 2: Define it with world.forEachWith<PathSegmentIntent>, keeping the keys whose intent has the kind.

Step 3: After isAccepted(world, edit) in edits.h, declare `bool changesPaths(const ParkEdit &edit)`, commented: "Whether applying the edit can change a path point or segment: false for AddBox, MoveBox, DeleteBox, SetShopStock, and SetOpeningHours, and true for every other edit." Define it with std::visit, listing the five false kinds, so a new edit kind counts as changing paths.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 6: Keep the availability's lists

Files:
- Modify: `src/legible/availability.h`, `src/legible/availability.cpp`

Step 1: Declare `AvailabilityLists` in availability.h. It holds the guest segments' keys, where each line's stops begin, each source's least distance at each stop as a nested StopEntry, where each stop's entries begin, the supplied offers found so far, a FieldSample<RouteEntry> for the route distance, and each place's last counted source. Comment: "The lists availabilityBends and availabilityValues reuse. Keep one to compute availabilities many times without allocating once its lists have grown."

Step 2: Declare `void availabilityBends(const World &, const AvailabilityDefinition &, AvailabilityLists &, std::vector<Place> &bends)` and `void availabilityValues(const World &, const AvailabilityDefinition &, std::span<const Place>, AvailabilityLists &, std::vector<double> &values)`. Each clears its output first and gives what the returning form gives. Comment: "As the returning form, into lists the caller keeps. Allocates nothing once they have grown."

Step 3: Rewrite the bodies as these forms, using the lists for every working list. Move StopEntry into the lists. SuppliedOffers takes the kept list of found offers and clears it. addStops takes the segment keys from parkSegmentKeys(world, PathKind::Guest, ...) instead of parkSegments. leastAtStops and availabilityValues sample with the kept FieldSample. Make the returning forms call the filling forms with lists of their own.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 7: Keep the preview without allocating

Files:
- Modify: `src/legible/preview.h`, `src/legible/preview.cpp`

Step 1: Give KeptPreview a member `KeptFieldSample Footfall`, commented as the lists that reading the shop's footfall again reuses. In keepPreview's kept branch, read the footfall with fieldValue's filling form and that sample.

Step 2: In keepPreview, follow ticks only while the kept preview holds a candidate. The condition to keep becomes: something is kept, the edit is the kept one's, and either the tick is the kept one's, or ticks aren't followed, or the kept preview has no candidate. Update keepPreview's comment to match the legible spec: without a candidate the preview holds nothing a tick changes, and keeping the preview allocates nothing once its lists have grown.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 8: Shade into kept lists

Files:
- Modify: `src/render/paving_overlay.h`, `src/render/paving_overlay.cpp`

Step 1: Declare `ShadeLists` (each place's line, and the next free knot of each line) and `void shadeOverlay(const OverlayShape &, std::span<const Place>, std::span<const double>, double half, ShadeLists &, OverlayShades &shades)`, commented: "As the returning form, into shades and lists the caller keeps. Allocates nothing once they have grown." Fill the shades with assign and resize on their existing lists.

Step 2: Replace stable_sort with an insertion sort by distance over each line's knots. It is stable and allocates nothing, and for places already in order it makes one pass. Make the returning form call the filling form.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 9: Append one mesh to another

Files:
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`

Step 1: Declare `void appendMesh(ParkMesh &mesh, const ParkMesh &more)`, commented: "Adds more's triangles after the mesh's, with their indices offset by the mesh's vertex count." Define it.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 10: Shade an availability into kept lists

Files:
- Modify: `src/views/availability_overlay.h`, `src/views/availability_overlay.cpp`

Step 1: Declare `AvailabilityShadeLists`, holding AvailabilityLists, the bends, the values, ShadeLists, and the OverlayShades, and `void availabilityShades(const World &, const OverlayShape &, const AvailabilityDefinition &, AvailabilityShadeLists &lists)`, which leaves the shades in lists.Shades. Comment: "As the returning form, into lists the caller keeps. Allocates nothing once they have grown." Make the returning form call it and return the shades.

Run: `cmake.exe --build --preset windows-debug --target tpj_views tpj_bench`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 11: Check the overlay's shape by the edit count and the edit

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`

Step 1: Replace ShapedPaths with a struct holding what the shape was built for: `std::optional<uint64_t> Edits` and `std::optional<ParkEdit> Edit`. Edit holds the preview's edit only when the preview holds a candidate and changesPaths gives true for its edit, and is empty otherwise. In syncOverlayShape, while shown, compare the kept struct's fields with the current edit count and that edit directly, without building a temporary, so a frame copies no edit. Build the shape only when they differ, and then store them, assigning into the kept struct. Update the comment: the shape is built again when the edit count differs, or the edit of a candidate that can change paths differs, from those it was built for. These fix the previewed world's path points and segments.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 12: Keep the ghost's parts and the shades in SceneSync

Files:
- Modify: `src/app/CMakeLists.txt`, `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`, `src/app/application.cpp`

Step 1: Link tpj_app_core to tpj_views publicly.

Step 2: Declare `struct PreviewMeshes { bool Ghost = false; bool Shades = false; }` with a defaulted operator==, commented as which preview meshes a frame rebuilt. Change syncLook to `PreviewMeshes syncLook(const World &world, bool remade, const PreviewLook &look, bool shapeChanged)`, and add the accessors `ghost()` and `shades()`. Its comment says:
- The edit's ghost is built when the preview was just made again, or at the first call.
- The look's marks are built when the look or the edit count differs from those they were built at.
- The inspected guest's mark is built when either of those is built, or when the last syncWorld rebuilt the guests while the subject is a guest.
- When any part is built, the frame's ghost is put together from them in that order.
- While the look shows an overlay, the shades are built again when the preview was just made again, the shape changed, or the need or the previewed world's tick differs from those they were built for. Otherwise they are forgotten.
- Once its lists have grown, a call frees no memory it allocated.

Step 3: Move ghostMesh's mark code from scene_uploads.cpp into scene_sync.cpp, split into the three parts. The edit's ghost is buildGhostMesh, or empty meshes with no edit. The look's marks are the path start in Lit with its join mark, the finish mark, the highlight, the handles, the hovered handle or sides, and a non-guest subject's appendEntity. The subject mark is appendGuestEntity for a guest subject. Each part clears its meshes and appends into them. Put the frame's ghost together by assigning each of the edit ghost's meshes into the frame's, then appendMesh the look's Lit and Marks and the subject mark into it. Shade with availabilityShades' filling form and needAvailability. Remember the look by assignment.

Step 4: Change uploadPreviewMeshes to `bool uploadPreviewMeshes(Renderer &, const SceneSync &, PreviewMeshes meshes, bool shapeChanged)`. It sets the ghost when meshes.Ghost, the shape when shapeChanged, and the shades when meshes.Shades. Update its comment to say it only uploads what the scene built. In application.cpp, call syncLook with the world and shapeChanged, and upload when the ghost, the shape, or the shades changed.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 13: Run the preview and the overlays in the check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Add a held preview: a SceneSync, an optional edit, and a PreviewLook. Its sync runs as the app's frame does: syncWorld, then syncPreview following ticks while the look shows an overlay, then syncOverlayShape twice on the previewed world, then syncLook with whether either shape call changed it.

Step 2: Before the warm-up, make two held previews with no overlay, one with no edit for each need in NEEDS, its look showing that need, and one with a refused guest path, its look showing food.
- The shop move: MoveBox of the first shop in parkBoxes onto its own pose.
- The point removal: RemovePoint of the first path point the world accepts it for.
- The refused guest path: an AddPath of PathKind::Guest through two NewPoint stops outside the park's square, which the world must refuse. Throw std::runtime_error with "the held guest path is accepted" if isAccepted gives true.
- Each look of the two: the edit's box or point as the highlight, one handle of each of the Point, Radius, Edge, and Taper kinds at that point's or box's place, a Side handle on the first guest segment from parkSegmentKeys as the hovered handle, a guest path start that joins and a finish at the same place, and as the inspected subject the guest in the middle of parkGuests.
After the first sync, if the shop move's preview has no candidate, or no shop context with a connection, throw std::runtime_error with "the held shop move has no connected shop context".

Step 3: Sync both held edits after each warm-up tick, and each overlay's preview at every warm-up tick that is a multiple of 60. In each watched tick, after the scene's sync, call `watchWork("the preview")` and sync both held edits twice. At every watched tick that is a multiple of 60, call `watchWork("the overlay")` and sync each overlay's preview twice. Change the success line to `allocation check: 600 ticks of full.park, the scene's sync, the preview, and the overlays freed no block they allocated`, and update the file's comments.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park, the scene's sync, the preview, and the overlays freed no block they allocated`. If it reports blocks, the report names the work. Fix each loop allocation it finds in this feature (docs/conventions.md).

### Task 14: Show the check sees the preview and the overlays

Files: none kept.

Step 1: Plant the temporary list criterion 1 gives in SceneSync::syncLook, rebuild, and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with report lines starting `allocation check: the preview freed` and lines starting `allocation check: the overlay freed`. The report lists the first 20 blocks. If they all name one work, see that the count printed covers both.

Step 2: Revert the plant, rebuild, and run again.

Expected: exit status 0.

### Task 15: Show the simulation unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/kept-preview/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/kept-preview/scenarios-before.txt build/kept-preview/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/kept-preview/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/kept-preview/full-before.txt build/kept-preview/full-after.txt`
Expected: exit status 0.

### Task 16: Show the scene and the overlay unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 600 --speed 0 --overlay food --capture build/kept-preview/capture-after.bmp && cmp build/kept-preview/capture-before-1.bmp build/kept-preview/capture-after.bmp`
Expected: exit status 0, with no output from cmp. If cmp reports a difference, treat it as a deviation.

### Task 17: Play the preview

Files: none.

Step 1: The user does criterion 4's play check in the windows-debug app.

### Task 18: Confirm the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play on full.park frees no block it allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 19: Commit

Step 1: Stage the feature's paths by name and commit once with the commit-hygiene skill, subject `App: Keep the preview's ghost and shades and check them`.
