# Research: park-view

## Where is the park's mesh built, so that it can be tested?

The renderer's GPU work needs a device and a window, which tests do not have. Building the mesh is separate from drawing it: a function that reads intent through the park module's public queries and writes vertices and indices into plain vectors needs no GPU at all, so tests can check the ribbon and the boxes against the ground line and the footprints directly. The renderer only uploads what it is given. Building per element, one path or one box at a time, also lets box-tools and path-tool build a ghost from a tentative command's intent with the same functions, so a ghost is drawn by exactly the code that draws the committed result.

Rejected: building vertices inside the renderer while uploading, which leaves the geometry untestable. A mesh kept in the world as derived state, which puts presentation into tpj_sim (principle 10) and gains nothing, since the renderer can rebuild it whenever intent changes.

## How does a flat ribbon lie on flat terrain without flickering?

A ribbon on the ground is coplanar with the terrain, so their depths tie and the rasterizer picks between them per pixel, which shows as flicker (z-fighting). Lifting the ribbon a little helps only where depth precision is finer than the lift, and precision falls with the square of distance: with a 0.1 m near plane and a 32-bit float depth buffer, the smallest resolvable step is a few millimeters at the default 90 m view and about a decimeter at the camera's 400 m limit. The standard remedy for decals is depth bias (polygon offset), a per-pipeline offset of a constant number of depth steps plus a factor of the triangle's depth slope, so grazing views get more offset. SDL_GPU exposes it in the rasterizer state. Pushing the terrain away with a small bias, and lifting the ribbon 2 cm, keeps paths above the grass at every distance without biasing boxes, whose bottoms are never drawn.

The ribbon's edges come from the ground line's samples, each offset by half the width to either side of the tangent. Averaging the unit directions of the two segments meeting at a sample keeps the edges joined. The ribbon narrows slightly at a bend, by the cosine of half the turn, and since the ground line takes at least 8 samples per segment, turns between samples stay small and the narrowing is not visible.

Rejected: a mitered join, which keeps the width exact but spikes at sharp turns. Raising the near plane, which changes the camera for everything to fix one layer. A thick slab for each path, which still ties with the terrain along its bottom and looks like a curb rather than ground.

## How does --hash stay equal to the scenario runner's hash?

tpj_scenarios' runSave loads a save with makeParkSchema, resolves, and steps with no commands, writing tick and hash after each. The app does the same before its first frame, and with --hash prints that line's tick and hash and exits before creating a window. That needs no display, so ctest can run the app next to tpj_scenarios on both builds and compare, which checks that linking the renderer changes nothing in the simulation (principle 10). A window is never opened for it, so the check does not depend on a GPU.

Rejected: printing the hash while the app keeps running, which needs a display and a GPU in the tests. Reusing runSave in the app, which links tpj_scenarios_lib and its generated tables into the player-facing executable.

Sources: https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias — depth bias as a constant plus slope-scaled offset; https://docs.vulkan.org/guide/latest/depth.html — Vulkan's depth bias factors and depth precision; https://wiki.libsdl.org/SDL3/SDL_GPURasterizerState — the bias fields SDL_GPU exposes per pipeline.
