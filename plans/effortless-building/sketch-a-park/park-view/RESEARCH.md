# Research: park-view

## Where is the park's mesh built, so that it can be tested?

The renderer's GPU work needs a device and a window, which tests do not have. Building the mesh is separate from drawing it: a function that reads intent through the park module's public queries and writes vertices and indices into plain vectors needs no GPU at all, so tests can check the ribbon and the boxes against the ground line and the footprints directly. The renderer only uploads what it is given. Building per element, one path or one box at a time, also lets box-tools and path-tool build a ghost from a tentative command's intent with the same functions, so a ghost is drawn by exactly the code that draws the committed result.

Rejected: building vertices inside the renderer while uploading, which leaves the geometry untestable. A mesh kept in the world as derived state, which puts presentation into tpj_sim (principle 10) and gains nothing, since the renderer can rebuild it whenever intent changes.

## How does a flat ribbon lie on flat terrain without flickering?

A ribbon on the ground is coplanar with the terrain, so their depths tie and the rasterizer picks between them per pixel, which shows as flicker (z-fighting). A small lift separates them only where depth precision is finer than the lift. With the conventional mapping, near at 0 and far at 1, a float depth buffer's precision is spent near the camera, and the smallest resolvable step grows with the square of distance: with a 0.1 m near plane it is a few millimeters at 90 m and about 7 cm at 340 m, so a 2 cm lift fights the grass when the camera is zoomed out. Reversing the mapping, near at 1 and far at 0, pairs the float format's fine steps near 0 with the distant range where perspective needs them, so the relative precision is nearly even: a few hundredths of a millimeter at 340 m. A 2 cm lift then separates ribbon and grass at every distance the camera allows. It costs a changed projection, a greater-than depth test, and a depth clear to 0, and the renderer already prefers a 32-bit float depth buffer.

The ribbon's edges come from the ground line's samples, each offset by half the width to either side of the tangent. Averaging the unit directions of the two segments meeting at a sample keeps the edges joined. The ribbon narrows slightly at a bend, by the cosine of half the turn, and since the ground line takes at least 8 samples per segment, turns between samples stay small and the narrowing is not visible.

Rejected: depth bias on the terrain, tried first. Its slope-scaled part vanishes when the camera looks straight down, and its constant part, one depth step, is smaller than the lift's depth difference at a distance, so paths flickered seen from above. A mitered join, which keeps the width exact but spikes at sharp turns. Raising the near plane, which changes the camera for everything to fix one layer. A thick slab for each path, which still ties with the terrain along its bottom and looks like a curb rather than ground.

## How does --hash stay equal to the scenario runner's hash?

tpj_scenarios' runSave loads a save with makeParkSchema, resolves, and steps with no commands, writing tick and hash after each. The app does the same before its first frame, and with --hash prints that line's tick and hash and exits before creating a window. That needs no display, so ctest can run the app next to tpj_scenarios on both builds and compare, which checks that linking the renderer changes nothing in the simulation (principle 10). A window is never opened for it, so the check does not depend on a GPU.

Rejected: printing the hash while the app keeps running, which needs a display and a GPU in the tests. Reusing runSave in the app, which links tpj_scenarios_lib and its generated tables into the player-facing executable.

Sources: https://developer.nvidia.com/content/depth-precision-visualized — reversed-Z with a float depth buffer and its even precision; https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias — depth bias as a constant plus slope-scaled offset; https://docs.vulkan.org/guide/latest/depth.html — Vulkan's depth bias factors and depth precision; https://wiki.libsdl.org/SDL3/SDL_GPURasterizerState — the bias fields SDL_GPU exposes per pipeline.
