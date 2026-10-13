# Implementation Plan: Quiet Uploads

## Goal

The renderer keeps its GPU buffers and transfer buffers and grows them only when outgrown, and the allocation check runs uploading and drawing with SDL's allocations counted.

## Approach

Each of the renderer's 16 GPU buffers becomes a KeptBuffer: the buffer, a transfer buffer of its own, and the bytes both hold. Each upload maps the transfer buffer with cycle true, writes, unmaps, and uploads with cycle true in a copy pass on the frame's command buffer, as Dear ImGui's SDL_GPU backend does. A KeptBuffer is replaced by one at least twice as large only when the data outgrows it. The allocation check gives SDL its memory functions, creates a hidden window and a renderer, and uploads, draws, and waits for the GPU after each sync, so the result doesn't depend on how far the GPU lags. If SDL itself frees a block at each frame, stop and report it as a deviation; don't exclude it.

## Placement

Decision 0027 places each behavior this feature adds:

- Keeping the GPU buffers and transfer buffers, growing them, and recording uploads on the frame's command buffer: render, src/render/renderer.h and renderer.cpp. The renderer owns every GPU resource, and no other module touches SDL_GPU.
- Writing the overlay's shape and shades as words into mapped memory: render, src/render/renderer.cpp, private. Only the renderer knows the layout overlay.frag reads.
- uploadWorldMeshes and uploadPreviewMeshes move from the tpj_app executable into tpj_app_core, src/app/scene/scene_uploads.h, unchanged, so the check calls the app's own uploads. tpj_app_core holds the app's components its tests and checks call through their headers. main.cpp gains nothing.
- Counting SDL's allocations: the allocation check, tests/checks/allocations/watch.h and watch.cpp. The watch already owns every replaced allocation function.
- The hidden window, the renderer, and the frames the check draws: the allocation check, tests/checks/allocations/allocation_check.cpp.

## Tasks

### Task 1: Record the frame and the simulation before the change

Files: none.

Step 1: On main, before any edit, build the app and tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_scenarios`
Expected: the build finishes with exit status 0 and prints no warning.

Step 2: Capture the frame and record the scenarios.

Run: `mkdir -p build/quiet-uploads && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 300 --speed 0 --overlay food --frames 5 --capture build/quiet-uploads/before.bmp`
Expected: exit status 0, and build/quiet-uploads/before.bmp exists.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-uploads/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-uploads/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c quiet-uploads`.

### Task 2: Update the specs, the docs, and the milestone

Files:
- Modify: `src/render/SPEC.md`, `src/app/SPEC.md`
- Modify: `docs/testing.md`, `docs/conventions.md`, `docs/decisions/0041-allocation-check.md`
- Modify: `plans/scalable-runtime/allocation-free-loops/MILESTONE.md`

Step 1: Make the SPEC.md edits FEATURE.md's Spec changes gives.

Step 2: In docs/testing.md, in the paragraph under "## The suite", replace "the tick, the scene's sync, the kept preview and the overlays, the tools, and the panels' data (decision 0041)." with "the tick, the scene's sync, the kept preview and the overlays, the tools, the panels' data, and the renderer's uploads and drawing (decision 0041). Only ImGui's own drawing and the previews that resolve a new world stay outside it."

Step 3: In docs/conventions.md, under "## Memory in loops", replace "Only uploading meshes, drawing, ImGui's own drawing, and previews that resolve a new world stay outside it (decision 0041)." with "Only ImGui's own drawing and previews that resolve a new world stay outside it (decision 0041). Code the check doesn't run must follow this rule all the same."

Step 4: Amend docs/decisions/0041-allocation-check.md in place:

- In Decision, replace "Only uploading meshes, drawing, and ImGui's own drawing stay outside it." with "The check also creates a hidden window and a renderer, uploads what the app's frame uploads after each sync, and draws the frame. It counts SDL's allocations as well as operator new, by giving SDL its memory functions, so a GPU buffer made and released is found. Only ImGui's own drawing stays outside it."
- In Consequences, after the first paragraph, add "Code the check doesn't run follows the rule all the same. If a milestone leaves work outside the check, it reads that code before it closes."

Step 5: In MILESTONE.md, replace feature 6's description with "the renderer keeps each GPU buffer with a transfer buffer of its own and grows them only when outgrown, records uploads on the frame's command buffer, and the allocation check uploads and draws in a hidden window, counting SDL's allocations, for criterion 9." In criterion 9, replace "(check, in PLAN.md of quiet-uploads)" with "Drawing a frame without a capture frees no block it allocated either. (ctest)". In the deepening candidate that starts "A frame check of the app itself", replace "including uploading and drawing" with "including ImGui's own drawing".

### Task 3: Give the renderer kept buffers

Files:
- Modify: `src/render/renderer.h:11-62`

Step 1: Forward-declare `struct SDL_GPUCommandBuffer;` and `struct SDL_GPUTransferBuffer;` with the others.

Step 2: Before Renderer, add:

- `KeptBuffer`, commented "A GPU buffer the renderer keeps, the transfer buffer that fills it, and the bytes both hold.", with `SDL_GPUBuffer *Buffer = nullptr;`, `SDL_GPUTransferBuffer *Transfer = nullptr;`, and `uint32_t Capacity = 0;`.
- `KeptMesh`, commented "A mesh's kept vertex and index buffers, and the indices last uploaded.", with `KeptBuffer Vertices;`, `KeptBuffer Indices;`, and `uint32_t IndexCount = 0;`.

Step 3: In Renderer, replace each pair of buffer pointers and its index count with a KeptMesh: `Terrain`, `Park`, `Guests`, `Ghost` (the Lit part), `Tints`, `Marks`, and `Overlay`. Replace OverlayShape and OverlayShades with `KeptBuffer OverlayShape;` and `KeptBuffer OverlayShades;`. Add `SDL_GPUCommandBuffer *Commands = nullptr;`, commented "The frame's command buffer, once an upload or drawFrame has acquired it. drawFrame submits it."

Step 4: In the comments of setParkMesh, setOverlayShape, setOverlayShades, setGhostMesh, and setGuestMesh, replace "from now on" with "from the next frame on", and add the sentence "It reuses the buffers the renderer keeps, and makes larger ones only when the data outgrows them."

### Task 4: Upload into kept buffers on the frame's command buffer

Files:
- Modify: `src/render/renderer.cpp:327-432`

Step 1: Replace createBuffer, uploadMesh, and uploadStorage with these, in the anonymous namespace:

- `SDL_GPUCommandBuffer *frameCommands(Renderer &renderer)`: if renderer.Commands is null, acquire one with SDL_AcquireGPUCommandBuffer and keep it there. Return it, or null after logging "Cannot acquire command buffer".
- `bool fitBuffer(Renderer &renderer, KeptBuffer &kept, SDL_GPUBufferUsageFlags usage, uint32_t bytes)`: if bytes is at most kept.Capacity, return true. Otherwise release kept.Buffer and kept.Transfer (SDL_GPU defers the release until the GPU is done with them), and create both at max(bytes, 2 * kept.Capacity) bytes, the transfer buffer with SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD. If either fails, log "Cannot create a kept buffer", release both, reset kept to `KeptBuffer{}`, and return false. Otherwise set kept.Capacity and return true.
- `template <typename Write> bool uploadKept(Renderer &renderer, KeptBuffer &kept, SDL_GPUBufferUsageFlags usage, uint32_t bytes, Write write)`: return true when bytes is 0. Call fitBuffer, then frameCommands. Map kept.Transfer with `SDL_MapGPUTransferBuffer(device, kept.Transfer, true)`, and log "Cannot map a transfer buffer" and return false on null. Call `write(mapped)` and unmap. Begin a copy pass on the frame's command buffer, call `SDL_UploadToGPUBuffer(copy, &source, &destination, true)` for the first bytes of kept.Buffer, end the pass, and return true.
- `bool uploadMesh(Renderer &renderer, KeptMesh &mesh, const void *vertices, uint32_t vertexBytes, std::span<const uint32_t> indices)`: if indices is empty, set mesh.IndexCount to 0 and return true, keeping the buffers. Otherwise upload the vertices with SDL_GPU_BUFFERUSAGE_VERTEX and the indices with SDL_GPU_BUFFERUSAGE_INDEX through uploadKept with memcpy writers, then set mesh.IndexCount.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: errors only where the old helpers and fields are still used, which tasks 5 to 7 replace.

### Task 5: Write the overlay's words into mapped memory

Files:
- Modify: `src/render/renderer.cpp:434-476`

Step 1: Replace shapeWords with `uint32_t shapeWordCount(const OverlayShape &shape)`, which gives 5 + 13 per piece + the line references + 2 per line + 3 per point + the overlaps, and `void writeShapeWords(const OverlayShape &shape, uint32_t *words)`, which writes the same words in the same order as shapeWords does today, through a pointer it advances, filling the five offsets as it goes.

Step 2: Replace shadeWords with `uint32_t shadeWordCount(const OverlayShades &shades)`, which gives 1 + the first knots + 2 per knot, and `void writeShadeWords(const OverlayShades &shades, uint32_t *words)`, which writes the same words in the same order as shadeWords does today.

Step 3: Remove `#include <vector>` if nothing else in the file uses it. createTerrainMesh still does.

### Task 6: Set every mesh and the overlay through the kept buffers

Files:
- Modify: `src/render/renderer.cpp:478-488,617-731`

Step 1: createTerrainMesh uploads through `uploadMesh(renderer, renderer.Terrain, ...)`. Its upload is recorded on renderer.Commands and reaches the GPU with the first frame.

Step 2: Remove replaceMesh. setParkMesh, setGuestMesh, and setGhostMesh call uploadMesh with their KeptMesh: Park, Guests, and Tints, Ghost, and Marks.

Step 3: setOverlayShape calls uploadMesh for renderer.Overlay. When the shape has indices, it then calls uploadKept for renderer.OverlayShape with SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, shapeWordCount(shape) * 4 bytes, and a writer that calls writeShapeWords. setOverlayShades sets OverlayHalf and does the same for renderer.OverlayShades with shadeWordCount and writeShadeWords. Neither releases a buffer.

Step 4: destroyRenderer submits renderer.Commands when it isn't null, before SDL_WaitForGPUIdle. It then releases each KeptMesh's and KeptBuffer's buffer and transfer buffer, in place of the old buffer releases.

### Task 7: Draw on the frame's command buffer

Files:
- Modify: `src/render/renderer.cpp:737-765,798-892,922-973`

Step 1: drawMesh takes a `const KeptMesh &` in place of the two buffers and the count, and its callers pass the KeptMesh. drawOverlay draws nothing unless renderer.Overlay.IndexCount is above 0 and both renderer.OverlayShape.Buffer and renderer.OverlayShades.Buffer are set, and binds those two buffers.

Step 2: drawFrame takes the frame's command buffer from frameCommands and sets renderer.Commands back to null, so the next upload acquires the next frame's. Where it cancels the command buffer today, after the swapchain or the window's size can't be read, it submits it instead, so the uploads recorded on it still reach the GPU, and returns false.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 8: Move the scene's uploads into tpj_app_core

Files:
- Modify: `src/app/CMakeLists.txt`

Step 1: Move `scene/scene_uploads.cpp` from tpj_app's sources to tpj_app_core's. Replace tpj_app_core's comment with "# The app's components that need no window of their own, so the app's tests and checks call them through their headers."

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 9: Count SDL's allocations in the watch

Files:
- Modify: `tests/checks/allocations/watch.h`, `tests/checks/allocations/watch.cpp`

Step 1: In watch.h, declare in tpj::check, commented "Allocation functions with the C library's signatures, which watch the blocks they allocate and free as operator new and delete do. Give them to SDL with SDL_SetMemoryFunctions before SDL allocates anything. A realloc counts as its block growing, as a list moving to a larger block does, so it is never found.":

    void *watchedMalloc(size_t size);
    void *watchedCalloc(size_t count, size_t size);
    void *watchedRealloc(void *block, size_t size);
    void watchedFree(void *block);

Update the namespace comment's last sentence to "watch.cpp replaces the global operator new and operator delete, and gives SDL its allocation functions, to see every allocation in the program and in SDL."

Step 2: In watch.cpp, define them. watchedMalloc calls allocate. watchedCalloc allocates count * size bytes through allocate and zeroes them. watchedFree calls release. watchedRealloc calls `std::realloc(block, size == 0 ? 1 : size)`. On success it removes the old block's entry without reporting it, using a new `void moved(void *block)` beside forget that finds and removes the entry only, then notes the new block with note. Set LastAllocated to a null address after a realloc, as a free does.

### Task 10: Run the renderer in the check

Files:
- Modify: `tests/checks/allocations/CMakeLists.txt`, `tests/checks/allocations/allocation_check.cpp`

Step 1: In CMakeLists.txt, link `SDL3::SDL3` to tpj_allocation_check privately, after tpj_sim.

Step 2: In allocation_check.cpp, include `app/scene/scene_uploads.h`, `render/day_light.h`, `render/renderer.h`, `<SDL3/SDL.h>`, and `<imgui.h>`. Add `HiddenFrame`, commented "A hidden window and the renderer that draws into it, as the app makes them for --capture.", holding `SDL_Window *Window`, `tpj::Renderer Gpu`, and `tpj::OrbitCamera Camera`. Its constructor calls SDL_Init(SDL_INIT_VIDEO), creates a hidden 1280 by 720 window, calls ImGui::CreateContext, and calls createRenderer with 256 m of terrain, as the app does. If any step fails, it throws "cannot create the hidden window and renderer: " followed by SDL_GetError(). Its destructor calls destroyRenderer, ImGui::DestroyContext, SDL_DestroyWindow, and SDL_Quit for what it made. Delete its copy and move.

Step 3: syncScene returns the WorldMeshes syncWorld gives. syncPreview returns a `SyncedPreview {tpj::PreviewMeshes Meshes; bool ShapeChanged;}` from what syncLook and the two syncOverlayShape calls give.

Step 4: Add two functions, commented "The frame after a sync, as the app's frame runs it: the uploads, then the drawing. Sets the watch's work back to the work given.":

- `void drawScene(HiddenFrame &frame, const tpj::SceneSync &scene, const tpj::ParkSession &session, const tpj::WorldMeshes &meshes, const char *work)`: watchWork("the uploads"), uploadWorldMeshes(frame.Gpu, session.world(), meshes, scene.guestMesh(), frame.Camera), watchWork("the drawing"), drawFrame(frame.Gpu, orbitCameraView(frame.Camera), dayLightOf(session.world()), nullptr, nullptr), SDL_WaitForGPUIdle(frame.Gpu.Device), then watchWork(work). The wait keeps one frame in flight at a time. Without it, nothing limits a hidden window's frames in flight, so SDL's cycling would make new internal buffers whenever the GPU fell further behind than in the warm-up, and the result would depend on GPU timing. Throws "the hidden frame failed to upload or draw" if either returns false.
- `void drawPreview(HiddenFrame &frame, const HeldPreview &held, const tpj::ParkSession &session, const SyncedPreview &synced, const char *work)`: the same, with uploadPreviewMeshes(frame.Gpu, held.Scene, synced.Meshes, synced.ShapeChanged).

Step 5: SteadyPlay gains `HiddenFrame Frame;`. Move the body of checkTicks' watched loop, from the tick's watchWork("the tick") to syncPanels, into `void steadyTick(SteadyPlay &play, tpj::ParkSession &session, uint64_t tick)`, commented "One tick of steady play and the frames after it, as the warm-up and the watched ticks both run it." After every syncScene and syncPreview call in it, and in syncTools, call drawScene or drawPreview with the work label that call runs under: "the scene's sync", "the preview", "the overlay", or "the tools". checkTicks calls watchTick and then steadyTick for each watched tick. warmUp calls steadyTick for each warm-up tick, keeping its tick-0 checks after it, so the warm-up runs the same syncs and frames as the watched ticks and the pools reach their size before watching starts. The watch functions do nothing while not watching, so steadyTick's watchWork calls are harmless in the warm-up.

Step 6: In report, make the success line `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, the panels, the uploads, and the drawing freed no block they allocated`.

Step 7: At the start of main, before anything else, call `SDL_SetMemoryFunctions(tpj::check::watchedMalloc, tpj::check::watchedCalloc, tpj::check::watchedRealloc, tpj::check::watchedFree)`. If it returns false, print "allocation check: cannot give SDL the watched memory functions" and return 2.

Step 8: In the file's header comment, add after the panels' sentence: "After each sync of the scene or of a held preview, the check uploads what the app's frame uploads and draws the frame, in a hidden window, with SDL's allocations counted too."

Step 9: Build and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, the panels, the uploads, and the drawing freed no block they allocated`.

### Task 11: Prove the check sees GPU buffer churn

Files:
- Modify, then revert: `src/render/renderer.cpp`

Step 1: Make criterion 1's edit at the start of setGuestMesh.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines starting `allocation check: ` that say a block `the uploads` allocated was freed.

Step 2: Revert the edit and rebuild.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, with task 10's line.

### Task 12: Prove the check sees the old uploads

Files:
- Modify, then revert: `src/render/renderer.cpp`

Step 1: At the start of uploadMesh, release mesh's two KeptBuffers and reset them to `KeptBuffer{}`, as replaceMesh did before.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines starting `allocation check: ` that say a block `the uploads` allocated was freed.

Step 2: Revert the edit and rebuild, and see task 10's line again.

### Task 13: Capture the frame after the change

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 300 --speed 0 --overlay food --frames 5 --capture build/quiet-uploads/after.bmp && cmp build/quiet-uploads/before.bmp build/quiet-uploads/after.bmp`
Expected: exit status 0, and cmp prints nothing.

### Task 14: Compare the simulation

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-uploads/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-uploads/scenarios-before.txt build/quiet-uploads/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-uploads/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-uploads/full-before.txt build/quiet-uploads/full-after.txt`
Expected: exit status 0.

### Task 15: Confirm the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play on full.park frees no block it allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 16: Play the frame

Files: none.

Step 1: Build the whole windows-release preset: `cmake.exe --build --preset windows-release`. Expected: exit status 0 and no warning.

Step 2: Ask the user to play criterion 4's play check in build/windows-release/ThemeParkJones.exe on tests/parks/stress/full.park. Fix what the play finds directly.

### Task 17: Review and commit

Step 1: Stage the feature's paths by name, never with `git add -A`. Dispatch the reviewer on the staged diff with FEATURE.md, src/render/SPEC.md, src/app/SPEC.md, and docs/principles.md.

Step 2: Commit once with the commit-hygiene skill, subject `Render: Keep the GPU buffers and check uploads and drawing`.
