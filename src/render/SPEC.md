# render

Draws views of the park through SDL_GPU (decision 0009). It reads what it needs to draw and never changes simulation state.

## Contract

World space is right-handed and measured in meters, with +Y up. The terrain is a flat square of a size given at creation, centered on the origin in the XZ plane, drawn with 1 m and 10 m grid lines that fade with distance.

createRenderer creates a Vulkan-backed GPU device, claims the window, and uploads the terrain; it logs through SDL and returns false on failure. destroyRenderer releases everything it created and is safe to call after a failed create.

drawFrame renders one CameraView into an offscreen RGBA8 color target with a depth buffer, then blits it to the swapchain. When the window is minimized the frame is skipped and counts as success. When a capture path is given, the rendered frame is also read back and saved as a BMP before drawFrame returns.

Shaders are GLSL in shaders/, compiled to SPIR-V at build time and loaded from shaders/ next to the executable. Following SDL_GPU's SPIR-V layout, vertex-stage uniform buffers use set 1 and fragment-stage uniform buffers use set 3.
