# Implementation Plan: Paving Overlay

## Goal

The food overlay draws the guest paving's own pieces, a shader colors each pixel by the exact availability at its overlay place, and each tick uploads only the values at the places where availability bends.

## Approach

render builds an overlay shape from a world: the guest pieces, the guest segments' line points, each piece's lines, and which pieces of different owners overlap. The app keeps it, building it again only when the previewed park's path points or segments change. legible finds the bends of availability along each guest segment's line, views takes availabilityValues at them, and render packs them into shades. The renderer uploads the shape and the shades as two storage buffers, and a fragment shader finds each pixel's place and blends the value between the bends around it. overlayPlace runs the same rule on the CPU for the tooltip.

## Placement

Decision 0027 places each behavior this feature adds:

- The overlay shape, the overlay-place rule, and packing values into shades: render, src/render/paving_overlay.h. render owns the overlay's geometry and drawing, and may read sim's surface and networks. The CPU rule sits beside the shape the shader reads, so the two stay one rule.
- Uploading and drawing the shape and shades: render, src/render/renderer.h, with the shaders in src/render/shaders/. The renderer owns every pipeline and GPU buffer.
- The bends of availability along a line: legible, src/legible/availability.h. legible owns availability and how it is computed from route distance and offers.
- Shading a shape with an availability: views, src/views/availability_overlay.h. views joins render and legible, as it does today.
- Keeping the shape until the previewed park's paths change: app, src/app/scene/scene_sync.h. SceneSync already decides which scene data a frame builds again.
- The tooltip's place: app, src/app/ui/food_tooltip.cpp, composing render's overlayPlace with legible's foodAvailability.
- The bench stages: bench, src/bench/stages.h, which times what the app does.

## Tasks

### Task 1: Baseline timings

Step 1: On main, before any change, take the baseline report.

Run: `mkdir -p build/paving-overlay && scripts/runtime-report.sh --launches 3 build/paving-overlay/before.txt`
Expected: the report prints against the budget and is written to build/paving-overlay/before.txt.

### Task 2: Specs

Files:
- Modify: `src/render/SPEC.md`, `src/legible/SPEC.md`, `src/views/SPEC.md`, `src/app/SPEC.md`, `src/bench/SPEC.md`

Step 1: Make every change FEATURE.md's Spec changes gives, word for word.

### Task 3: Bends of availability

Files:
- Modify: `src/legible/availability.h`, `src/legible/availability.cpp`

Step 1: Declare `std::vector<Place> availabilityBends(const World &world, const AvailabilityDefinition &definition);` with a comment stating what legible/SPEC.md now says of it.

Step 2: Define it in availability.cpp:

- Take the guest network and the keys of the guest segments from parkSegments. For each carrier of the network that is a guest segment, in key order, take a place at each of its stops.
- Sample route distance at all those stop places in one call of sampleFieldAtPlaces<RouteDistance<PathKind::Guest>>, keeping each place's first entry of each source, as availabilityValues counts them.
- For each pair of consecutive stops on a line, with the edge's length L: for each source with an entry at either end whose suppliedOffer exists (found once per source, as availabilityValues finds them), the source's distance at t along the edge is the least of the start's distance plus t and the end's distance plus L less t, over the ends it has an entry at. If it has both and the two meet strictly inside the edge, add that t. For each point of the definition's curve, the distance at which the source's effective time reaches its X is X less the offer's wait in seconds, times REFERENCE_SPEED. On each straight part of the distance, add each t strictly inside the edge where the distance equals it.
- Give the stops and the added places, each place at its stop's distance plus t, sorted by carrier key then distance, with equal places once.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: it builds with no warnings.

### Task 4: Check that availability is linear between bends

Files:
- Create: `build/paving-overlay/linear.cpp`, `build/paving-overlay/build.sh` (scratch, not committed)

Step 1: Copy build/drawn-edges/build.sh, adding build/<preset>/src/legible/libtpj_legible.a before the sim library. Write a scratch program built with it. It loads tests/parks/main-street.park and tests/parks/warm.park, resolves each, and steps 3000 ticks. For each, it takes availabilityBends with FOOD_AVAILABILITY. For each pair of consecutive bends on one line, it takes the places a quarter, half, and three quarters of the way between them, and compares availabilityValues there with the straight blend of the two bends' values. It prints the number of bends and the largest difference.

Run: `build/paving-overlay/build.sh linear windows-debug && build/paving-overlay/linear.exe`
Expected: for each park, a bend count and a largest difference below 1e-9. If the difference is larger, the bends miss a case; find it before going on.

### Task 5: Overlay shape and places

Files:
- Rename: `src/render/band_overlay.h` to `src/render/paving_overlay.h`, `src/render/band_overlay.cpp` to `src/render/paving_overlay.cpp` (git mv)
- Modify: `src/render/CMakeLists.txt:4`

Step 1: In the header, rename the guard to TPJ_RENDER_PAVING_OVERLAY_H. Remove OVERLAY_BAND, OVERLAY_SPACING, OVERLAY_TOP_LIFT, OVERLAY_EDGE_LIFT, OverlayValues, and buildBandOverlay. Keep OVERLAY_RAMP. Set OVERLAY_ZERO_COLOR to {0.96f, 0.96f, 0.96f, 1.0f}, a near white the ramp and both kinds of paving never give. Add OVERLAY_HEIGHT, pathLift(PathKind::Guest) + 0.0025f: how far above the ground the shading lies, over the guest paving and below its edge lines, which lie EDGE_LIFT above it.

Step 2: Declare, each with a comment saying what it holds or gives:

- OverlayVertex: a position and the index of its piece.
- OverlayPiece: up to four corners counter-clockwise and their count, and the first and count of its lines in the shape's line references and of its overlaps in the shape's overlaps.
- OverlayLine: a guest segment's key and the first and count of its points in the shape's points.
- OverlayShape: Vertices and Indices of the pieces' triangles, Pieces, LineRefs (indices of lines, ascending, for each piece), Lines (ascending key), Points (CarrierPoints), and Overlaps (indices of pieces, for each piece).
- `OverlayShape buildOverlayShape(const World &world);`
- `std::optional<Place> overlayPlace(const OverlayShape &shape, GroundPoint point);`
- OverlayShades: for each line of the shape, the first and count of its knots, the knots as (distance, value) pairs, and Full.
- `OverlayShades shadeOverlay(const OverlayShape &shape, std::span<const Place> places, std::span<const double> values, double full);`

Step 3: Define buildOverlayShape. Lines come from the guest network's carriers that are guest segments, in key order, with their points copied. Pieces come from parkSurface(world) without walkways, guest pieces only, in order. A piece owned by a guest segment refers to that segment's line. A piece owned by a point refers to the lines of each guest segment with that point as From or To. Two pieces of different owners overlap when their convex polygons share positive area: no edge of either separates them, by more than nothing, along its normal. Find the pairs by sorting the pieces by the least X of their bounds and testing only pieces whose bounds overlap. Each piece adds the triangles of a fan from its first corner, its vertices at OVERLAY_HEIGHT, each with the piece's index, in the corners' order.

Step 4: Define overlayPlace. Give none when the point is not finite. For each piece that holds the point, on or inside its outline, take each of its lines. On each line, the nearest place is found segment by segment along its points: the point's projection clamped to the segment, and a segment strictly nearer replaces the one before. Among lines, a strictly nearer place replaces the one before, taking lines in ascending index, so ties go to the lower key. Give that line's key and the place's distance along it.

Step 5: Define shadeOverlay. For each line, take the places on it, in the order given, with their values. Places on keys the shape has no line for are left out.

Step 6: In CMakeLists.txt, list paving_overlay.cpp in place of band_overlay.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: tpj_render builds with no warnings.

### Task 6: The overlay pipeline and its shaders

Files:
- Create: `src/render/shaders/overlay.vert`, `src/render/shaders/overlay.frag`
- Modify: `src/render/CMakeLists.txt` (TPJ_SHADER_SOURCES), `src/render/renderer.h`, `src/render/renderer.cpp`

Step 1: overlay.vert takes the position and the piece index. It passes the world X and Z, the piece index unblended, and the view distance, and computes gl_Position with the camera block park.vert uses.

Step 2: overlay.frag reads two read-only storage buffers in set 2, the shape's at binding 0 and the shades' at binding 1, each a header of offsets and then the arrays FEATURE.md's render spec describes, as 32-bit words. Its uniform block in set 3 holds Full, OVERLAY_ZERO_COLOR, and the five OVERLAY_RAMP colors. For the pixel's X and Z, it finds the nearest place on its piece's lines by Task 5 Step 4's rule. For each overlapping piece that holds the pixel, it finds that piece's nearest place the same way, and discards the pixel if that place is strictly nearer, or as near on a lower line. It finds the two knots around the place's distance on the line by binary search and blends their values. A value not above 0 gives OVERLAY_ZERO_COLOR. Any other gives the ramp at the value over Full, capped at 1, blended between its two nearest colors as rampColor did. It lights and fogs the color as park.frag does, with the normal straight up.

Step 3: In renderer.cpp, give loadShader a count of storage buffers, and let createPipeline take the fragment shader's counts of uniform and storage buffers. Create OverlayPipeline, opaque, with OverlayVertex's two attributes: FLOAT3 position and UINT piece. Replace the overlay's mesh buffers in Renderer with the shape's vertex, index, and storage buffers and the shades' storage buffer and Full. Release them in destroyRenderer.

Step 4: Declare and define `bool setOverlayShape(Renderer &renderer, const OverlayShape &shape);` and `bool setOverlayShades(Renderer &renderer, const OverlayShades &shades);`, replacing setOverlayMesh. Each packs its value into one array of 32-bit words, with floats for coordinates, distances, and values, and uploads it as a GRAPHICS_STORAGE_READ buffer. An empty shape draws nothing.

Step 5: In drawScene, push the fragment uniform block before the pass. Draw the overlay after the park mesh, when it has a shape and shades: bind OverlayPipeline, its vertex and index buffers, and the two storage buffers with SDL_BindGPUFragmentStorageBuffers. Change the comment before the draws to say the overlay lies over the guest paving and under its edge lines, boxes, and guests.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: tpj_render and its shaders build with no warnings.

### Task 7: Shades from availability

Files:
- Modify: `src/views/availability_overlay.h`, `src/views/availability_overlay.cpp`

Step 1: Replace buildAvailabilityOverlay with `OverlayShades availabilityShades(const World &world, const OverlayShape &shape, const AvailabilityDefinition &definition);`. It takes availabilityBends, availabilityValues at them, and shadeOverlay with the definition's Full. Update the header comment to match views/SPEC.md.

Run: `cmake.exe --build --preset windows-debug --target tpj_views`
Expected: tpj_views builds with no warnings.

### Task 8: The app keeps the shape and uploads shades

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`, `src/app/application.cpp:90-110`

Step 1: SceneSync gains `bool syncOverlayShape(const World &previewed, bool shown)` and `const OverlayShape &overlayShape() const`. While shown, it builds the shape again when the previewed world's parkPathPoints or parkSegments differ from those it last built from, or when it holds no shape. While not shown, it empties the shape and forgets what it was built from. It returns whether the shape changed. A new generation, in syncWorld, also empties it.

Step 2: In Application's frame, call syncOverlayShape with previewedWorld(world, preview) and Ui.shown().FoodOverlay after syncPreview, and again after Ui.build, so a checkbox changed in the panels shows in the same frame. Keep whether either call changed the shape.

Step 3: uploadPreviewMeshes takes the shape and whether it changed. When the shape changed, it calls setOverlayShape. While the look shows the food overlay, it calls setOverlayShades with availabilityShades of the previewed world, the shape, and FOOD_AVAILABILITY. Update its comment. Call uploadPreviewMeshes when syncLook says or the shape changed.

Task 9's build checks this task.

### Task 9: The tooltip on the paving

Files:
- Modify: `src/render/picking.h`, `src/render/picking.cpp`, `src/app/input/cursor.h`, `src/app/input/cursor.cpp`
- Modify: `src/app/ui/food_tooltip.h`, `src/app/ui/food_tooltip.cpp`, `src/app/ui/tooling_ui.h`, `src/app/ui/tooling_ui.cpp:73`, `src/app/application.cpp`

Step 1: In picking, add `std::optional<ParkPoint> pointAtCursor(const CameraView &view, float aspect, float ndcX, float ndcY, float height);`: where the cursor's ray meets the level at the height, or none when the eye is not above it or the ray does not descend. groundAtCursor calls it with 0. In cursor.h, add pointUnderCursor(cursor, view, height) beside groundUnderCursor, giving pointAtCursor for the cursor, or none without a cursor. Comment both as render/SPEC.md says.

Step 2: drawFoodTooltip takes the overlay shape. It takes overlayPlace at the ground point, returns when it gives none, and otherwise attributes foodAvailability at the place, as now. Its header comment says so.

Step 3: ToolingUi::build takes the overlay shape and passes it to drawFoodTooltip, with pointUnderCursor at OVERLAY_HEIGHT in place of groundUnderCursor, so the tooltip explains the pixel the cursor is over at any camera pitch. Application passes Scene.overlayShape().

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the app builds with no warnings.

### Task 10: Remove foodNear and path places

Files:
- Modify: `src/legible/food.h`, `src/legible/food.cpp`, `src/legible/CMakeLists.txt:7`
- Delete: `src/legible/path_place.h`, `src/legible/path_place.cpp`

Step 1: Remove foodNear's declaration and definition and food.h's include of legible/path_place.h. Delete path_place.h and path_place.cpp with git rm, and remove path_place.cpp from CMakeLists.txt.

### Task 11: Bench stages

Files:
- Modify: `src/bench/stages.h`, `src/bench/stages.cpp`, `src/bench/report/internal/lines.cpp`

Step 1: Add ResultKind::Values, written and read as `values`, and say so in the comments and the refusal message that list the kinds.

Step 2: Replace the food-overlay stage with two: food-overlay-shape, a mesh stage of buildOverlayShape, and food-overlay, which builds the shape once before timing and times availabilityShades of it with FOOD_AVAILABILITY, its result the number of knots as values. Update stages.h's list of stages.

Run: `cmake.exe --build --preset windows-debug`
Expected: everything builds with no warnings.

### Task 12: Confirm the criteria

Step 1: Take the report after the change, and compare it with Task 1's.

Run: `scripts/runtime-report.sh --launches 3 build/paving-overlay/after.txt && scripts/runtime-report.sh --compare build/paving-overlay/before.txt build/paving-overlay/after.txt`
Expected: each stage before and after. The food-overlay median is lower on each stress park, and the app's frame times with the overlay shown are no worse. Report the food-overlay-shape stage too.

Step 2: Captures with windows-debug:

- `build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --ticks 3000 --overlay food --capture build/paving-overlay/street-overlay.bmp`
- `build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --ticks 3000 --capture build/paving-overlay/street.bmp`
- `build/windows-debug/ThemeParkJones.exe --park tests/parks/new.park --overlay food --capture build/paving-overlay/new-overlay.bmp`

Look at each for criteria 1 and 4.

Step 3: Run `git grep -n -i "band overlay\|OVERLAY_BAND\|foodNear" -- src`.
Expected: no output.

Step 4: Run Task 4's check again.

Step 5: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports nothing.

Step 6: Rebuild windows-release with `cmake.exe --build --preset windows-release`, and ask the user to play criteria 1, 2, 5, and 6 in main-street.park.

### Task 13: Commit

Step 1: Stage only this feature's paths. Commit with the commit-hygiene skill, subsystem Render, for example "Render: Shade the guest paving in the food overlay".
