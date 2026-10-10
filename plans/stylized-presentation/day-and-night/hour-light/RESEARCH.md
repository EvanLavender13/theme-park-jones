# Research: hour-light

The milestone's RESEARCH.md settles the approach: a float scene target, a PBR Neutral tonemap into the existing 8-bit target, the overlay and marks drawn after the tonemap, guests lit by brightness only, and keyframes at hours of the day. This file covers the details this feature's plan depends on.

## How does PBR Neutral work?

Khronos publishes the reference in GLSL (https://github.com/KhronosGroup/ToneMapping, PBR_Neutral/pbrNeutral.glsl). It takes linear Rec. 709 color, which is the same primaries as sRGB.

- First it subtracts a small offset from all three channels. The offset is x - 6.25x² when the smallest channel x is under 0.08, and 0.04 otherwise. This lifts the darkest values toward zero.
- If the largest channel is below 0.76, it returns the color. Below that peak, a color passes through with its hue and saturation unchanged.
- Above that peak, it compresses the peak toward 1 along a curve, and it blends toward white by a desaturation of 0.15 as the compression grows.

The output is in [0, 1], ready for sRGB encoding. Scaling by an exposure before the tonemap brightens or darkens the whole frame without changing hues below the compression point.

So a surface lit to about 1.0 at noon keeps its authored color, apart from the small offset. Only the brightest highlights compress. Night needs exposure above 1 to lift its dim values into the range the eye reads, which is the day-for-night lift.

Sources: https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl — the reference function and its constants; https://modelviewer.dev/examples/tone-mapping — how it compares with ACES and AgX.

## How should the light be balanced so today's colors still read?

Today a face lit straight on by the sun shows its full authored color. A face turned away keeps 55% of it, in display values, which is about 26% in linear light. The new light should keep that balance at noon, so the boxes' kind colors still read as themselves:

- A top face at noon receives about 1.0 in linear light, from the sun's term plus the ambient light from the sky.
- A side face turned away from the sun receives about 0.3, from the ambient light alone.

With the sun about 60 degrees up, that is a sun strength near 0.75 and an ambient sky near 0.35 in linear light.

At night a top face should receive about a tenth of its noon light before exposure. An exposure near 1.5 then lifts it. The ambient light at night is cool and less saturated, as the slice's research asks.

## Can the shaders share the light's code?

The shaders compile with glslang-standalone, through src/render/CMakeLists.txt. Its standalone includer (StandAlone/StandAlone.cpp, DirStackFileIncluder) resolves `#include "light.glsl"` relative to the including file, once the shader enables `#extension GL_GOOGLE_include_directive : require`. So the light's uniform block and its shading function can live in one file that terrain.frag and park.frag include. The build must list the included file as a dependency of each shader, or editing it won't rebuild them.

## What does SDL_GPU need for the tonemap pass?

- The float target must be created with both the color-target and sampler usages.
- A sampler comes from SDL_CreateGPUSampler. Nearest filtering is enough, because the pass reads the target at the same size it writes.
- The fragment shader's sampler sits at set 2, and its uniforms at set 3. Its loadShader call must give num_samplers 1.
- The pass draws three vertices with no vertex buffer, and the vertex shader builds a full-screen triangle from gl_VertexIndex.
- Uniform data pushed with SDL_PushGPUFragmentUniformData applies to the draws that follow, so the scene can push the light once, and push it again before the guests with the brightness-only flag set.

The overlay and the marks draw in a later pass into the 8-bit target. That pass needs the scene's depth, so the scene pass must store its depth target, and the later pass loads it.

Sources: https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader — resource sets per stage; https://wiki.libsdl.org/SDL3/SDL_GPUSwapchainComposition — the SDR swapchain the 8-bit target is blitted to.
