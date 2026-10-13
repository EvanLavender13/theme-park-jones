# Feature: Quiet Uploads

## Summary

quiet-uploads makes the renderer keep its GPU memory, and puts uploading and drawing in the allocation check. Today every call that sets a mesh, the overlay's shape, or its shades releases the GPU buffers it set before and creates new ones. It also creates a transfer buffer and a command buffer for each upload and builds a new list of words for the overlay. In steady play that happens to the guest mesh at every tick, to the overlay's shades at every tick while an overlay is shown, and to the ghost's six buffers whenever the look changes. This breaks docs/conventions.md's Memory in loops rule.

After this feature, the renderer keeps each of its 16 GPU buffers with a transfer buffer of its own. It writes each upload into them with SDL_GPU's cycling, and makes larger ones, twice as large, only when the data outgrows them. It writes the overlay's words straight into the transfer buffer. Uploads are recorded on the frame's command buffer, which drawFrame submits. The allocation check creates a hidden window and a renderer, and counts SDL's own allocations as well as operator new. After each sync it runs, it uploads what the app's frame would upload and draws the frame. What the player sees doesn't change. This covers criterion 9 of the milestone.

## Acceptance criteria

1. The allocation check creates a hidden window and a renderer, and counts every allocation SDL makes as well as every operator new. After each sync of the scene and of each held preview, it uploads what the app's frame uploads, draws the frame, and waits for the GPU to finish it, at every warm-up and watched tick. The warm-up runs the same work at each tick as the watched ticks. Freed blocks are reported as `the uploads` or `the drawing`. If nothing is freed, the check prints `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, the panels, the uploads, and the drawing freed no block they allocated` and exits with status 0. If it can't create the window or the renderer, it exits with status 2. (check: at the start of setGuestMesh, in src/render/renderer.cpp, add `SDL_GPUTransferBufferCreateInfo probe = {}; probe.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; probe.size = 16; SDL_ReleaseGPUTransferBuffer(renderer.Device, SDL_CreateGPUTransferBuffer(renderer.Device, &probe));`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that say a block `the uploads` allocated was freed; then revert)
2. Once the renderer's buffers have grown, uploading the guest mesh at each tick, the overlay's shades at each tick, and the ghost when the look changes frees no block it allocated and creates no GPU buffer. (ctest)
3. Drawing a frame without a capture frees no block it allocated. (ctest)
4. The renderer draws what it drew before. (check: capture full.park paused after 300 ticks with the food overlay, before and after the feature, and see the two files are identical, commands in PLAN.md tasks 1 and 13; play, in the windows-release app on tests/parks/stress/full.park: see guests walk and sit, choose Food and see the overlay change as the park runs, move Place shop over the park and see the ghost follow, choose Edit path and see the handles and the hovered side, and draw a guest path that grows past the first ghost's size)
5. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md tasks 1 and 14)
6. docs/testing.md, docs/conventions.md, and decision 0041 say the check runs the renderer's uploads and drawing, and that only ImGui's own drawing and the previews that resolve a new world stay outside it. docs/conventions.md says that code the check doesn't run still follows the rule. (check: read the three files)
7. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: the three commands in PLAN.md task 15)

## Tests

None. The allocation check covers the work, and criterion 1's check proves it sees GPU buffers being made and released.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows.

## Spec changes

src/render/SPEC.md:

- After the paragraph that starts "Each of setParkMesh, setGuestMesh, and setGhostMesh replaces the mesh set by its previous call", add "A mesh, shape, or shades set reaches the GPU in the next frame drawFrame draws. The renderer keeps the GPU memory each of them was last uploaded into. Once the renderer's memory has grown to the frames in flight, setting one that fits that memory frees nothing it allocated and creates no GPU buffer. Once the renderer's memory has grown, drawing a frame without a capture frees no memory it allocated."

src/app/SPEC.md:

- In the paragraph that starts "The park's walkways and shop marks are rebuilt", after "The allocation check holds a stretch of full.park's ticks to it.", add "Uploading the guests, the ghost, and the overlay to the renderer, and drawing the frame, also free no memory they allocated once the renderer's buffers have grown. The allocation check holds them to it too."

docs/testing.md, docs/conventions.md, and docs/decisions/0041-allocation-check.md: see PLAN.md task 2.

## Files affected

- Modify: `src/render/renderer.h`, `src/render/renderer.cpp`, `src/render/SPEC.md`
- Modify: `src/app/CMakeLists.txt`, `src/app/SPEC.md`
- Modify: `tests/checks/allocations/watch.h`, `tests/checks/allocations/watch.cpp`, `tests/checks/allocations/allocation_check.cpp`, `tests/checks/allocations/CMakeLists.txt`
- Modify: `docs/testing.md`, `docs/conventions.md`, `docs/decisions/0041-allocation-check.md`
- Modify: `plans/scalable-runtime/allocation-free-loops/MILESTONE.md`

## Dependencies

- quiet-tick: the allocation check. Merged.
- quiet-scene, kept-preview, quiet-tools, and quiet-panels: the syncs the check runs, after which it now uploads and draws. Merged.

## Out of scope

- ImGui's own drawing. The check doesn't build the app's panels with ImGui. ImGui's SDL_GPU backend keeps its buffers and transfer buffers and grows them only when outgrown (imgui_impl_sdlgpu3.cpp, CreateOrResizeBuffers), and ImGui reuses its draw lists from frame to frame. The milestone's deepening candidate, a frame check of the app itself, would cover it.
- --capture's readback, which runs once, for the last frame.
- Building the park mesh again after an edit, which runs on an edit, not in steady play.
- The previews that resolve a new world, which changing-previews takes.

## Open questions

None.
