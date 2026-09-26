# 0009. SDL3 with SDL_GPU as the rendering layer

Status: Accepted, 2026-09-26

## Context

The alternatives were raylib (fastest start, known from SteelJones) and SDL3 with SDL_GPU (more durable for instancing, custom vertex formats, and custom shaders). The game will need large amounts of instanced foliage and procedurally generated geometry.

## Decision

SDL3 for windowing and input, and SDL_GPU for rendering. Shaders are written in GLSL and compiled to SPIR-V at build time by glslang, which is built from source. Because only SPIR-V is provided, SDL_GPU selects its Vulkan backend on every platform.

## Consequences

There is one shader language and one bytecode format. Supporting D3D12 or Metal later means adding SDL_shadercross to the shader build. Under WSL, Vulkan runs on lavapipe (software), which is fine for checks but not for playing. The scene renders to an offscreen target that is blitted to the swapchain, which makes frame capture possible (--capture).
