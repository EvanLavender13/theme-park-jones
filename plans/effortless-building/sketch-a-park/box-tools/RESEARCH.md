# Research: box-tools

## How does the cursor become a position on the ground?

The camera is a perspective view from an eye toward a target, so every pixel is a ray from the eye. Building it from the camera's own basis needs no matrix inverse: the forward direction toward the target, the right direction across it, and the up direction that completes them, with the cursor's normalized device coordinates scaled by the tangent of half the vertical field of view, and by the aspect ratio horizontally. The ground is the plane y = 0, so the ray meets it at the parameter that brings the eye's height to zero, and only when the ray descends. The orbit camera's pitch stays above about 10 degrees, but half the field of view is wider than that, so the top of the screen can show sky, and there the cursor has no ground position. The same projection the renderer uses maps the point back to the cursor, which gives a property test that needs no GPU.

Rejected: reading the depth buffer under the cursor, which needs a GPU readback each frame and would pick the top of a box rather than the ground the box stands on. Unprojecting near and far points through the inverted view-projection matrix, which is the same ray with a 4x4 inverse and more rounding.

## How do the tools stay testable, and what do they hand over?

A tool's state changes only through three inputs: where the pointer meets the ground, if anywhere, a press of the primary button, and its release. Given those and the world, a tool gives its tentative edit, the command a ghost stands for, and on release gives the edit to commit. Keeping these as plain functions of values in a library linking tpj_sim alone lets tests drive every tool with synthetic positions, and the property principle 8 asks for is direct: what the release commits is the tentative edit shown just before it, and the ghost's validity is isAccepted on that edit. The edit is one of the park's five commands, so a variant of them lets the tools, the ghost builder, and the app's queueing share one type, and path-tool adds its AddPath through the same variant.

Rejected: tools reading SDL events, which ties their logic to a window and leaves the ghost-equals-commit property untestable. Tools that queue commands themselves, which hides the committed value from tests and the app.

## How are a box's position and facing set with one button?

Games that allow free rotation commonly place on press and set the facing by dragging from the placement point before release, so one gesture sets both, with the facing unchanged when the drag is too short to give a stable direction. A facing is a ground direction, so the drag vector itself is the facing, with no angle and no trigonometry, matching how intent holds it. Keeping the last facing for the next ghost lets a row of shops be placed with single clicks. Moving picks the box under the cursor on press and follows the cursor with the grab offset kept, so the box does not jump to center on the cursor. A press and release without movement commits nothing, which avoids queueing an edit that changes nothing.

Rejected: rotating with a key or the wheel in fixed steps, which quantizes the facing that the gridless vision keeps free, and the wheel already zooms. Rotating a box while moving it, which needs a second gesture; a box can be deleted and placed again until reshaping comes.

## How are ghosts and highlights drawn over coincident geometry?

The translucent pass draws after the opaque park mesh, blending by alpha, with a depth test and no depth writes, so ghosts never hide what is behind them. With reversed depth the test is greater-than. A deletion highlight lies exactly on the box or ribbon it marks, so its depth ties with the opaque surface, and a strict greater-than test would fail it. The translucent pipeline therefore tests greater-or-equal. A tie passes only if both pipelines compute the same position bit for bit. Vulkan guarantees repeatability only for identical pipelines, and without the invariant qualifier a compiler may contract or reorder the same expression differently in two pipelines, which shows as patches where the later pass loses by one step. Using the same vertex shader for both pipelines and declaring gl_Position invariant in it makes the depths equal.

Rejected: inflating the highlight slightly, which changes its shape and still ties at the ribbon's flat top. A depth bias on the translucent pipeline, which the park-view research found unreliable when looking straight down. Drawing highlights with no depth test, which shows them through boxes in front.

Sources: https://antongerdelan.net/opengl/raycasting.html — building a pick ray from the cursor and camera; https://learn.microsoft.com/en-us/previous-versions/windows/xna/bb203905(v=xnagamestudio.10) — the unproject alternative through near and far points; https://docs.vulkan.org/spec/latest/appendices/invariance.html — repeatability only for identical pipelines; https://github.com/ZihanWG/VulkanEngine/pull/46 — an equal-depth second pass failing without invariant gl_Position; https://wiki.libsdl.org/SDL3/SDL_GPUColorTargetBlendState — per-pipeline blending.
