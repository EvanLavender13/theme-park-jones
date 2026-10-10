# Implementation Plan: Hour Light

## Goal

Light the park by its time of day, in linear light with a PBR Neutral tonemap, while the food overlay and the editing marks keep their colors.

## Approach

A new render component, day_light, turns a second of the day into a DayLight. It interpolates keyframes at hours of the day, wrapping at midnight, and computes the sun's path and the moon. The renderer draws the terrain, park, and guests into a float target, lit by a shared light.glsl, and a full-screen pass tonemaps that target into the existing 8-bit target. A third pass draws into the 8-bit target against the scene's stored depth. It draws the overlay unlit, the ghost's preview of the park lit and tonemapped as the park is, and the marks unlit. The UI, the blit, and captures are unchanged. The app passes dayLightOf(world) to drawFrame.

## Placement

Decision 0027 places each behavior this feature adds:

- The day's light (keyframes, the sun's path, the moon, dayLightAt, dayLightOf): render, a new component src/render/day_light.h. render owns how things are drawn. The light is a pure function with no GPU state, so the app and light-preview can call it apart from the renderer.
- The float target, the tonemap pass, the light's uniforms, and the pass for the overlay, ghosts, and marks: render, the renderer component (src/render/renderer.h). It owns the GPU targets, pipelines, and passes.
- The light's shading, the tonemap, the lit ghost, and the unlit marks: render's shaders in src/render/shaders. They are part of the renderer.
- Splitting a ghost into what it previews and its marks: render, the park mesh component (src/render/park_mesh.h), which builds the ghost. The app's ghost, which adds the tool's marks, stays in app/scene (src/app/scene/scene_uploads.cpp), which composes the frame's meshes.
- Handing the world's light to drawFrame: app, src/app/application.cpp. This is composition only. The application already hands drawFrame the camera and the UI each frame.

## Tasks

### Task 1: Record the before state

Files: none in the repository. Outputs go to build/hour-light/.

Step 1: On the feature branch, before any change, build windows-debug and windows-release.

Run: `cmake.exe --build --preset windows-debug && cmake.exe --build --preset windows-release`
Expected: both builds finish with no warnings.

Step 2: Record the simulation's output.

Run: `mkdir -p build/hour-light && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/hour-light/scenarios-before.txt`
Expected: exit status 0.

Step 3: Capture noon.park as it looks today.

Run: `build/windows-release/ThemeParkJones.exe --park tests/parks/noon.park --speed 0 --capture build/hour-light/noon-before.bmp`
Expected: exit status 0, and the BMP exists.

Step 4: Record the runtime report.

Run: `scripts/runtime-report.sh build/hour-light/runtime-before.txt`
Expected: it prints the report against the budget and writes the file.

### Task 2: Describe the light in render's spec

Files:
- Modify: `src/render/SPEC.md`

Step 1: Make the edits that FEATURE.md's Spec changes lists for src/render/SPEC.md, word for word: the two Boundary additions, the new drawFrame paragraph opening, the new section "## The light" after "## The renderer", the sentence appended to "## Guests", and the words added to the Paving overlay's coloring sentence.

### Task 3: Describe the lit scene in the app's spec

Files:
- Modify: `src/app/SPEC.md:27`

Step 1: Append to the paragraph under "## The scene": "The scene is lit by the light at the world's time of day (render/SPEC.md, The light)."

### Task 4: Add the day_light interface

Files:
- Create: `src/render/day_light.h`, `src/render/day_light.cpp`
- Modify: `src/render/CMakeLists.txt`

Step 1: Create src/render/day_light.h with include guard TPJ_RENDER_DAY_LIGHT_H. It includes render/math.h and <stdint.h>, and forward-declares `struct World;`. Declare Rgb, DayLight, SUNRISE_SECOND, SUNSET_SECOND, dayLightAt, and dayLightOf exactly as FEATURE.md's Spec changes gives them. Give each a one-line comment stating what it is.

Step 2: Create src/render/day_light.cpp. Each function returns a default-constructed value for now.

Step 3: Add `day_light.cpp` to tpj_render's sources in src/render/CMakeLists.txt, in alphabetical order.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: builds with no warnings.

### Task 5: Compute the day's light

Files:
- Modify: `src/render/day_light.cpp`

Step 1: Add the keyframes, private to the file. Each holds a second of the day and the following values:

- the sun's color, written in sRGB
- the sun's strength
- the moon's strength
- the ambient sky and ground colors, written in sRGB
- the sky's color, written in sRGB
- the exposure

The first values, in order of the second of the day:

| Time | Sun color | Sun | Moon | Ambient sky | Ambient ground | Sky | Exposure |
|---|---|---|---|---|---|---|---|
| 00:00 | 1.00 0.60 0.40 | 0 | 0.10 | 0.30 0.34 0.44 | 0.12 0.12 0.16 | 0.06 0.08 0.15 | 1.6 |
| 05:00 | 1.00 0.60 0.40 | 0 | 0.10 | 0.30 0.34 0.44 | 0.12 0.12 0.16 | 0.06 0.08 0.15 | 1.6 |
| 06:00 | 1.00 0.60 0.38 | 0.60 | 0.04 | 0.52 0.50 0.58 | 0.30 0.26 0.24 | 0.80 0.64 0.60 | 1.2 |
| 08:00 | 1.00 0.90 0.78 | 0.75 | 0 | 0.60 0.64 0.72 | 0.48 0.44 0.38 | 0.62 0.76 0.90 | 1.0 |
| 12:00 | 1.00 0.97 0.92 | 0.75 | 0 | 0.62 0.66 0.74 | 0.50 0.46 0.40 | 0.62 0.76 0.90 | 1.0 |
| 17:00 | 1.00 0.80 0.56 | 0.75 | 0 | 0.62 0.62 0.68 | 0.50 0.44 0.36 | 0.74 0.74 0.80 | 1.0 |
| 19:30 | 1.00 0.52 0.30 | 0.60 | 0.04 | 0.50 0.44 0.52 | 0.30 0.24 0.22 | 0.86 0.54 0.40 | 1.2 |
| 20:30 | 1.00 0.60 0.40 | 0 | 0.10 | 0.30 0.34 0.44 | 0.12 0.12 0.16 | 0.06 0.08 0.15 | 1.6 |

The moon's color is fixed at sRGB 0.62 0.72 1.00.

Step 2: Implement dayLightAt(second):

- Find the two keyframes around the second. After the last keyframe, blend toward the first keyframe of the next day, one DAY_SECONDS later, so 23:59 blends into 0:00 with no jump.
- Convert each keyframe's sRGB colors to linear with the exact sRGB transfer function, then interpolate linearly by the second.
- The colors in DayLight are the interpolated color times its strength. The ambient light and the sky have no separate strength.
- The sun's direction: for a second between SUNRISE_SECOND and SUNSET_SECOND, let f be the fraction of that span passed. The sun's heading on the ground is (cos πf, sin πf) in (X, Z). Its elevation is 60° times sin πf. Its direction is (cos e · cos πf, sin e, cos e · sin πf).
- The sun's color is also multiplied by a smoothstep of the elevation from 0° to 10°, so it fades out at the horizon. Outside the span the sun's color is zero, and its direction is the direction at the nearer end.
- The moon's direction is fixed: elevation 55°, heading (−√½, −√½) in (X, Z).

Step 3: Implement dayLightOf(world) as dayLightAt(parkTime(world.Tick).Second).

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: builds with no warnings.

### Task 6: Share the light and the tonemap between the shaders

Files:
- Create: `src/render/shaders/light.glsl`, `src/render/shaders/tonemap.glsl`
- Modify: `src/render/shaders/park.frag`, `src/render/shaders/terrain.frag`, `src/render/CMakeLists.txt`

Step 1: Create light.glsl:

- The std140 uniform block `Light` at set 3, binding 0, holding vec4s: sunDirection, sun, moonDirection, moon, ambientSky, ambientGround, sky, and settings. settings.x is 1 when the light gives brightness only, and settings.y is the exposure.
- A function that converts an sRGB color to linear with the exact transfer function.
- A function `incoming(vec3 normal)`. It returns the sun's color times the clamped dot of the normal with the sun's direction, plus the same for the moon, plus a mix from the ambient ground color to the ambient sky color by 0.5 · normal.y + 0.5. When settings.x is 1, it returns the luminance of that sum (Rec. 709 weights 0.2126, 0.7152, 0.0722) in all three channels.
- A function `fadeToSky(vec3 color, float viewDistance)`, which mixes toward sky.rgb by smoothstep(250.0, 700.0, viewDistance), as today's shaders do.

Step 2: Create tonemap.glsl. It holds one function, `displayed(vec3 linear, float exposure)`:

1. It multiplies by the exposure.
2. It applies PBR Neutral as in https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl, with startCompression 0.76 and desaturation 0.15.
3. It encodes with the exact sRGB transfer function.

Step 3: In park.frag and terrain.frag, add `#extension GL_GOOGLE_include_directive : require` and `#include "light.glsl"`, and remove the SKY and SUN_DIRECTION constants.

- park.frag: the color is the linear vertex color times incoming(normal), faded to the sky. Alpha is unchanged.
- terrain.frag: GRASS converted to linear, times incoming(normal), then the grid lines darken it as today, then it fades to the sky.

Step 4: In src/render/CMakeLists.txt, make each shader's custom command also depend on `${CMAKE_CURRENT_SOURCE_DIR}/shaders/light.glsl` and `${CMAKE_CURRENT_SOURCE_DIR}/shaders/tonemap.glsl`.

Run: `cmake.exe --build --preset windows-debug --target tpj_shaders`
Expected: every shader compiles.

### Task 7: Add the tonemap, ghost, and marks shaders

Files:
- Create: `src/render/shaders/tonemap.vert`, `src/render/shaders/tonemap.frag`, `src/render/shaders/ghost.frag`, `src/render/shaders/marks.frag`
- Modify: `src/render/shaders/overlay.frag`, `src/render/CMakeLists.txt`

Each new fragment shader, and overlay.frag, includes light.glsl and tonemap.glsl. "The displayed sky" below means displayed(sky.rgb, settings.y).

Step 1: tonemap.vert draws a full-screen triangle from gl_VertexIndex, with no inputs, and passes the texture coordinate. Flip it so the float target is read upright. Confirm that with the captures in Task 11.

Step 2: tonemap.frag samples the float target at set 2, binding 0, and writes displayed(color, settings.y), with alpha 1.

Step 3: ghost.frag takes park.vert's outputs. It lights the color as park.frag does, faded to the sky, then writes displayed(that, settings.y) with the vertex alpha. A ghost is then lit and tonemapped exactly as the committed park is.

Step 4: marks.frag takes park.vert's outputs. It shades by today's fixed light, so marks look as they do today: color times 0.55 + 0.45 · max(dot(normal, normalize(vec3(0.4, 1.0, 0.3))), 0). It fades to the displayed sky by smoothstep(250.0, 700.0, viewDistance). Alpha is unchanged.

Step 5: In overlay.frag:

- Move the Ramp block to binding 1. The Light block takes binding 0.
- Remove the SKY and SUN_DIRECTION constants and the light factor.
- The color is the ramp's color, faded to the displayed sky.

Step 6: Add tonemap.vert, tonemap.frag, ghost.frag, and marks.frag to TPJ_SHADER_SOURCES, in alphabetical order.

Run: `cmake.exe --build --preset windows-debug --target tpj_shaders`
Expected: every shader compiles.

### Task 8: Split the ghost into what it previews and its marks

Files:
- Modify: `src/render/park_mesh.h:206-210`, `src/render/park_mesh.cpp:573-640`, `src/render/renderer.h:82`, `src/app/scene/scene_uploads.cpp:15-52`

Step 1: In park_mesh.h, add `struct GhostMesh { ParkMesh Tints; ParkMesh Lit; ParkMesh Marks; };`, with a comment that Tints draw under Lit and Marks over it, that Lit is lit as the park is, and that Tints and Marks keep their colors. Both buildGhostMesh overloads return a GhostMesh.

Step 2: In buildGhostMesh, write each part to the mesh FEATURE.md names:

- An accepted edit's added or moved box, the surface it adds or changes, the reshaped segments of an accepted DeleteSegment, and the candidate's walkways and starved marks go to Lit.
- Anything drawn in INVALID_TINT or DELETE_TINT goes to Tints. That covers a refused edit's box, surface, and straight segments, appendDeleted, and a DeleteSegment's tinted segment. Drawn under Lit, a delete tint stays under the segments the edit reshapes, as the spec says.
- The sides in JOIN_MARK_COLOR and the join marks go to Marks.

The geometry each call adds is unchanged.

Step 3: In renderer.h, setGhostMesh takes `const GhostMesh &`. Add TintVertices, TintIndices, TintIndexCount, MarkVertices, MarkIndices, and MarkIndexCount to Renderer, beside the ghost's buffers. setGhostMesh replaces all three parts: Tints in the tints' buffers, Lit in the ghost's buffers, and Marks in the marks' buffers.

Step 4: In scene_uploads.cpp, ghostMesh returns a GhostMesh. It starts from buildGhostMesh when there is an edit, and appends the path start, the finish mark, the highlight, the point markers and handles, the hovered handle or sides, and the inspected mark to its Marks part. Update its comment.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: builds with no warnings. The app builds after Task 10.

### Task 9: Draw the scene in linear light, tonemap it, then draw the overlay, ghosts, and marks

Files:
- Modify: `src/render/renderer.h`, `src/render/renderer.cpp`

Step 1: In renderer.h:

- Add to Renderer: `SDL_GPUTexture *SceneTarget`, `SDL_GPUSampler *SceneSampler`, `SDL_GPUGraphicsPipeline *TonemapPipeline`, and `SDL_GPUGraphicsPipeline *MarksPipeline`. Forward-declare SDL_GPUSampler.
- Include render/day_light.h.
- Give drawFrame the parameter `const DayLight &light` after the camera, and say in its comment that the scene is lit by that light.

Step 2: In renderer.cpp, add SCENE_FORMAT = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, and remove SKY_COLOR.

- createRenderer refuses, with a logged error, a device on which SCENE_FORMAT doesn't support both the color-target and sampler usages.
- ensureRenderTargets also creates SceneTarget at the swapchain's size, with both usages, and releases it with the others.

Step 3: Give createPipeline a target format parameter, and give FragmentBuffers a Samplers count that loadShader passes as num_samplers.

- The terrain and park pipelines target SCENE_FORMAT, with one fragment uniform buffer, the Light block.
- The ghost pipeline uses park.vert and ghost.frag. MarksPipeline uses park.vert and marks.frag. Both are translucent, with one fragment uniform buffer, and target COLOR_FORMAT.
- The overlay pipeline targets COLOR_FORMAT with two fragment uniform buffers.

Step 4: Create TonemapPipeline: tonemap.vert and tonemap.frag, with one fragment sampler and one fragment uniform buffer. It has no vertex input, no depth target, no culling, and targets COLOR_FORMAT. Create SceneSampler with nearest filtering and clamped addressing.

Step 5: destroyRenderer also releases SceneTarget, SceneSampler, TonemapPipeline, MarksPipeline, TintVertices, TintIndices, MarkVertices, and MarkIndices.

Step 6: Rewrite the drawing in three passes. Each pass pushes the Light block, built from the DayLight, at fragment slot 0, with settings.x 0 and settings.y the exposure, except where a step says otherwise.

1. The scene pass, into SceneTarget:
   - Clear it to light.Sky.
   - Clear the depth target as today, with store op STORE.
   - Draw the terrain and the park mesh.
   - Push the Light block again with settings.x 1, then draw the guests through the park pipeline.
2. The tonemap pass, into ColorTarget:
   - Load op DONT_CARE.
   - Bind SceneTarget with SceneSampler, and draw three vertices.
3. The overlay and marks pass, into ColorTarget, with the depth target loaded and not cycled:
   - Load op LOAD.
   - Push the overlay's Ramp at fragment slot 1.
   - Draw the overlay, then the ghost's Tints part through MarksPipeline, then its Lit part through the ghost pipeline, then its Marks part through MarksPipeline.

drawUi, the blit, and the capture are unchanged.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: builds with no warnings.

### Task 10: Hand the world's light to the renderer

Files:
- Modify: `src/app/application.cpp:129`

Step 1: Include render/day_light.h. Pass `dayLightOf(Session.world())` to drawFrame after `view`.

Run: `cmake.exe --build --preset windows-debug`
Expected: builds with no warnings.

### Task 11: Capture the day and check the light

Files: none, unless tuning (Step 4).

Step 1: Rebuild windows-release, then make each capture with it:

- `--park tests/parks/noon.park --speed 0 --capture build/hour-light/noon.bmp`
- `--park tests/parks/day.park --ticks 5400 --speed 0 --capture build/hour-light/day-noon.bmp`
- `--park tests/parks/day.park --ticks 16200 --speed 0 --capture build/hour-light/day-1800.bmp`
- `--park tests/parks/day.park --ticks 25200 --speed 0 --capture build/hour-light/day-2300.bmp`
- `--park tests/parks/day.park --ticks 25200 --speed 0 --overlay food --capture build/hour-light/day-2300-food.bmp`

Expected: each run exits 0. Convert each capture to PNG for viewing.

Step 2: Check criteria 1 to 5 and the overlay half of criterion 7 against the captures:

- noon.bmp against noon-before.bmp for criterion 2.
- noon.bmp against day-noon.bmp for criterion 3. They should match everywhere but the Debug panel's frame rate.

Step 3: Play day.park in the windows-release app at 4x through midnight, for criterion 6. At 23:00, hover a shop placement, a refused placement, a highlight, and a path's handles, for the rest of criterion 7.

Step 4: If a capture or play shows a criterion failing, adjust the keyframes in day_light.cpp toward the balance in RESEARCH.md: a top face near 1.0 at noon, a face turned from the sun near 0.3, and night near a tenth of noon before exposure. Then capture again.

### Task 12: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports no findings.

Step 2: Compare the simulation with Task 1's output.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/hour-light/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/hour-light/scenarios-before.txt build/hour-light/scenarios-after.txt; echo $?`
Expected: `0`

Step 3: Time the frames.

Run: `scripts/runtime-report.sh build/hour-light/runtime-after.txt && scripts/runtime-report.sh --compare build/hour-light/runtime-before.txt build/hour-light/runtime-after.txt`
Expected: full.park's frames stage has a median under 33 ms. Report the before and after medians.

### Task 13: Commit

Step 1: Stage only this feature's paths and commit with the commit-hygiene skill, for example "Render: Light the park by its time of day".
