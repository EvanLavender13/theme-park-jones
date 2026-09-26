#ifndef TPJ_RENDER_RENDERER_H
#define TPJ_RENDER_RENDERER_H

#include "render/math.h"

#include <stdint.h>

struct SDL_GPUBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUTexture;
struct SDL_Window;

namespace tpj {

struct CameraView {
  Vec3 Eye;
  Vec3 Target;
  float FovY = 0.9f;
  float NearZ = 0.1f;
  float FarZ = 2000.0f;
};

// The scene renders into an offscreen color target that is then blitted to the swapchain,
// which lets any frame be read back for captures.
struct Renderer {
  SDL_Window *Window = nullptr;
  SDL_GPUDevice *Device = nullptr;
  SDL_GPUGraphicsPipeline *TerrainPipeline = nullptr;
  SDL_GPUBuffer *TerrainVertices = nullptr;
  SDL_GPUBuffer *TerrainIndices = nullptr;
  uint32_t TerrainIndexCount = 0;
  SDL_GPUTexture *ColorTarget = nullptr;
  SDL_GPUTexture *DepthTarget = nullptr;
  int DepthFormat = 0;
  uint32_t TargetWidth = 0;
  uint32_t TargetHeight = 0;
};

// Creates the GPU device and a flat square terrain of `terrainSize` meters centered on the
// origin. Returns false and logs through SDL on failure.
bool createRenderer(Renderer &renderer, SDL_Window *window, float terrainSize);
void destroyRenderer(Renderer &renderer);

// Draws one frame. When `capturePath` is non-null the frame is also written there as a BMP.
bool drawFrame(Renderer &renderer, const CameraView &camera, const char *capturePath);

} // namespace tpj

#endif
