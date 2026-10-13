# Research: quiet-uploads

## What the renderer does at each upload today

Every setter in src/render/renderer.cpp releases its GPU buffers and creates new ones at each call. setParkMesh, setGuestMesh, and setGhostMesh go through replaceMesh. It releases the vertex and index buffers, and uploadMesh then creates new ones, plus a new transfer buffer that it releases after one copy. setOverlayShape and setOverlayShades release their storage buffers, and uploadStorage creates new ones, also with a new transfer buffer. shapeWords and shadeWords return a new std::vector of words at each call. Each upload also acquires its own command buffer and submits it alone.

In steady play this runs:

- setGuestMesh at every tick: 2 buffers, a transfer buffer, and a command buffer.
- setOverlayShades at every tick while an overlay is shown: a buffer, a transfer buffer, a command buffer, and a list of words.
- setGhostMesh whenever the look changes, as when the pointer moves onto another box, point, or handle: 6 buffers, 3 transfer buffers, and 3 command buffers.

Creating an SDL_GPUBuffer allocates SDL's own bookkeeping with SDL_malloc, a Vulkan buffer, and a region of device memory. Releasing it frees them once the GPU is done with it. All of this breaks docs/conventions.md's Memory in loops rule, which forbids a rebuild at every tick or frame.

## How engines and SDL_GPU handle buffers that change every frame

SDL_gpu.h's performance notes say not to churn resources, to create what you need up front and keep it, and to perform data uploads as early as possible in the frame. Its note on cycling explains how a kept buffer can be rewritten while an earlier frame still reads it. Each SDL_GPUBuffer and SDL_GPUTransferBuffer is a ring of internal resources. When a write passes cycle as true and the resource is still bound to a command buffer in flight, SDL moves to the next unbound internal resource, creating one only if none is free. Once there are as many as there are frames in flight, no more are made.

Dear ImGui's SDL_GPU backend, which this app already runs, does exactly this for its per-frame vertex and index data (imgui_impl_sdlgpu3.cpp, CreateOrResizeBuffers and ImGui_ImplSDLGPU3_PrepareDrawData). It keeps one GPU buffer and one transfer buffer for each, and makes larger ones only when the frame's data outgrows them. Each frame it maps the transfer buffer with cycle true, writes, unmaps, and uploads with cycle true, in a copy pass on the frame's command buffer. Its changelog says it began reusing transfer buffers because recreating them every frame was unusually slow.

So the renderer should keep, for each of its 16 buffers, the GPU buffer, its own transfer buffer, and their capacity. It should write each upload into them with cycling, and make larger ones only when the data outgrows them. If a buffer grows by exactly the bytes needed, a guest mesh that grows by one guest at a time would grow again at every arrival. Doubling the capacity when it grows, as std::vector does, keeps growth to a few steps over a whole day. Uploads should be recorded in a copy pass on the frame's command buffer, which drawFrame then submits with the frame.

The overlay's shape and shades can be written straight into the mapped transfer buffer, since their word counts follow from their list sizes. Then no list of words is needed at all.

Rejected: one staging buffer shared by every upload — each upload in a frame would cycle it again, and every internal buffer would take the size of the largest upload. A transfer buffer per GPU buffer is simpler, and it is what the ImGui backend does. Keeping a list of words in the renderer — it copies every word twice, and the mapped memory can take them directly. Growing a buffer to exactly the bytes needed — a growing guest mesh would recreate it at every arrival.

## How the allocation check can see GPU buffer churn

The check replaces operator new and delete, which SDL never calls. SDL allocates through SDL_malloc, SDL_calloc, SDL_realloc, and SDL_free. SDL_SetMemoryFunctions replaces them, if it is called before SDL allocates anything. The project links SDL statically (CMakeLists.txt, SDL_STATIC ON), so the check's executable can route SDL's allocations through the same watch as operator new. Creating and releasing a GPU buffer, a transfer buffer, or a fence allocates and frees SDL bookkeeping, so the watch sees each one.

SDL_realloc moves or resizes a block in one call. SDL grows its pools with it, such as a command pool's list of command buffers. The watch can treat a realloc as the block growing, so it passes, while a block that SDL_free frees is still found.

To reach the renderer, the check needs a window and a GPU device. The app already draws whole frames in a hidden window for --capture and --frame-times. The check can create a hidden window and a renderer the same way, then upload and draw after each sync, as the app's frame does. The GPU driver allocates its own memory through its own C runtime, which neither hook sees. SDL suballocates device memory itself, and every buffer or region it makes goes through SDL_malloc, so that churn is still seen.

On Linux, scripts/release-check.sh runs ctest on linux-debug. WSL has a display (WSLg) and Mesa's lavapipe Vulkan driver, so the check can create its window and device there too.

Rejected: counting GPU buffer creations with a wrapper around the renderer's own calls — it would see only what the renderer chose to count, not SDL's own churn. Leaving uploads and drawing outside the check, as decision 0041 first did — the renderer has broken the rule since it was written, and nothing noticed.

## What stays outside

ImGui's own drawing stays outside the check, because the check doesn't build the app's panels with ImGui. Its backend keeps its buffers and grows them only when outgrown, as above, and ImGui reuses its draw lists from frame to frame. --capture's readback runs once, for the last frame, so it isn't steady play.

Sources: .cpm-cache/sdl3/45cb/include/SDL3/SDL_gpu.h — performance notes and "A Note On Cycling"; https://moonside.games/posts/sdl-gpu-concepts-cycling/ — cycling explained by SDL_GPU's author; .cpm-cache/imgui/5839/backends/imgui_impl_sdlgpu3.cpp — the backend's kept buffers and transfer buffers; .cpm-cache/sdl3/45cb/include/SDL3/SDL_stdinc.h — SDL_SetMemoryFunctions and when it may be called; .cpm-cache/sdl3/45cb/src/gpu/vulkan/SDL_gpu_vulkan.c — where SDL allocates for buffers, fences, and command pools.
