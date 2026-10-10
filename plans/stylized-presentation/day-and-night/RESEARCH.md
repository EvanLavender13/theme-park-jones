# Research: day-and-night

The capability's RESEARCH.md and the slice's RESEARCH.md settle the approach: a sun on an hour angle, a moon at night, a hemispheric ambient light, colors authored as ramps keyed on the time of day, and a night that is cooler and less saturated, not only darker. This file covers how to build that in this renderer.

## What does the renderer do today?

Every scene shader repeats the same two constants: a sky color and a fixed sun direction (src/render/shaders/park.frag, terrain.frag, overlay.frag). Light is 0.55 plus 0.45 times the sun's Lambert term, so a face turned away from the sun keeps 55% of its color. The frame clears to the sky color, and everything fades to it between 250 m and 700 m from the eye.

The scene draws into an R8G8B8A8_UNORM target. The swapchain uses SDL_GPU's default SDR composition. Colors are written as they are authored, and are treated as already sRGB-encoded. No step converts them to or from linear light. Dear ImGui draws into the same target after the scene. The target is then blitted to the swapchain, and captures read it back.

Ghosts and highlights draw through the park's lit shader with blending. The overlay's shader multiplies its ramp color by a fixed light factor of about 0.96 and fades it to the sky color with distance.

## How should the scene move to linear light?

Light adds up correctly only in linear values. The scene should render into a 16-bit float target (R16G16B16A16_FLOAT), with every authored color converted from sRGB to linear before lighting. A full-screen pass then tonemaps the float target and encodes it to sRGB in the 8-bit target that the UI, the blit, and captures already use. Under SDR composition the swapchain stays UNORM, so the shader does the sRGB encoding itself. The SDR_LINEAR composition would do the encoding in hardware, but the UI and the blit would then need changes for no gain.

SDL_GPU binds a fragment shader's samplers at set 2 and its uniforms at set 3. A texture must be created with the sampler usage to be read in a later pass. The full-screen pass draws one triangle with no vertex buffer.

Rejected: rendering the light straight into the 8-bit target — warm and cool light clip and band, and a dim night loses its shades. SDR_LINEAR composition — it changes the swapchain format that the UI backend and the blit expect.

Sources: https://wiki.libsdl.org/SDL3/SDL_GPUSwapchainComposition — SDR and SDR_LINEAR compositions; https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader — resource sets for each stage; https://moonside.games/posts/sdl-gpu-sprite-batcher/ — SDL_GPU sampling and passes in practice.

## Which tonemapper keeps a palette's colors?

ACES, which SteelJones uses, shifts hues and desaturates strongly: bright yellows and cyans become unreachable. AgX shifts hue less, but still desaturates. Khronos PBR Neutral is built to pass the widest range of base colors through unchanged, and it compresses only highlights, with no hue shift. The capability's look rests on a limited palette with consistent colors, and the boxes' kind colors must stay distinct, so a neutral tonemapper fits. If the look later wants a filmic rolloff, grading belongs in retro-post.

Rejected: ACES — it shifts the palette's hues. AgX — it desaturates the palette.

Sources: https://www.khronos.org/news/press/khronos-pbr-neutral-tone-mapper-released-for-true-to-life-color-rendering-of-3d-products — the aims of PBR Neutral; https://modelviewer.dev/examples/tone-mapping — PBR Neutral compared with ACES and AgX; https://dl.acm.org/doi/fullHtml/10.1145/3641233.3664313 — the paper behind it.

## How do the overlay and the editing marks keep their colors?

If the overlay and the marks draw into the float target, the hour's light and the tonemap change them. They should draw after the tonemap, straight into the 8-bit target, unlit, in their own colors. They need the scene's depth so boxes and guests still hide them. The depth target must therefore be stored after the scene pass, and the second pass must load it. Their distance fade goes toward the hour's sky as displayed, which is the tonemapped sky color, so they fade into the same sky as the scene.

The overlay is opaque and lies under the paving's edge lines, boxes, and guests. Drawn later with the depth test, it still lies under them, because they are nearer the eye.

Rejected: lighting the overlay and dividing the light out afterward — the tonemap can't be undone exactly, so colors would drift.

## How do guests keep their need shading?

The user chose brightness only. Guests take the light's luminance, but not its color. Their blue-to-pink shading stays the same hue at every hour and dims with the light. The shader can light a guest by the luminance of the sum of the light terms in place of their color. Guests are boxes standing in for a later form, so nothing more elaborate is needed.

## How should the day's light be authored?

SteelJones blends four keyframes: midnight, sunrise, noon, and sunset. Each holds the sky, ground, key, and fog colors. Here the keyframes sit at hours of the day and wrap at midnight, so 23:59 blends into 0:00 with no jump. Interpolate them piecewise-linearly in linear color. A ramp evaluated at the time of day gives the sun's color and strength, the moon's, the ambient sky and ground colors, the sky color, and the exposure.

The user chose a compressed day: sunrise near 6:00, golden light from about 17:00, sunset near 19:30, and full dark by about 20:30. The last open hour is dusk, and guests walk out after closing at night.

The sun's direction comes from the hour angle between sunrise and sunset. Its elevation peaks well below 90 degrees, so faces stay distinct at noon. Its strength fades to zero at the horizon over a twilight band, as SteelJones fades its sun and moon over 10 degrees. The moon is a fixed, high direction that fades in as the sun fades out, so night light always comes from above.

The renderer may use the C runtime's trigonometry (decision 0022). It reads the time of day through parkTime(world.Tick) (sim/clock.h). The tick moves in whole steps of 2 game seconds, so the light can never jump by more than one tick's change.

Rejected: a physically based sun path from latitude and date — more than the look needs, and it ties the hours to a place. Keyframes at fixed fractions of the opening hours — the slice says the light follows the time of day alone, and the hours are the player's to change.

Sources: /mnt/c/Users/EvanUhhh/source/repos/SteelJones src/render/day_cycle.cpp and src/render/lighting.cpp — four-keyframe day cycle and twilight fades.
