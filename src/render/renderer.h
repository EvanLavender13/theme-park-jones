#ifndef TPJ_RENDER_RENDERER_H
#define TPJ_RENDER_RENDERER_H

#include "render/math.h"
#include "render/park_mesh.h"

#include <stdint.h>

struct ImDrawData;
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
  SDL_GPUBuffer *OverlayVertices = nullptr;
  SDL_GPUBuffer *OverlayIndices = nullptr;
  uint32_t OverlayIndexCount = 0;
  SDL_GPUGraphicsPipeline *ParkPipeline = nullptr;
  SDL_GPUBuffer *ParkVertices = nullptr;
  SDL_GPUBuffer *ParkIndices = nullptr;
  uint32_t ParkIndexCount = 0;
  SDL_GPUBuffer *GuestVertices = nullptr;
  SDL_GPUBuffer *GuestIndices = nullptr;
  uint32_t GuestIndexCount = 0;
  SDL_GPUGraphicsPipeline *GhostPipeline = nullptr;
  SDL_GPUBuffer *GhostVertices = nullptr;
  SDL_GPUBuffer *GhostIndices = nullptr;
  uint32_t GhostIndexCount = 0;
  SDL_GPUTexture *ColorTarget = nullptr;
  SDL_GPUTexture *DepthTarget = nullptr;
  int DepthFormat = 0;
  uint32_t TargetWidth = 0;
  uint32_t TargetHeight = 0;
  bool UiInitialized = false;
};

// Creates the GPU device, a flat square terrain of `terrainSize` meters centered on the origin,
// and the Dear ImGui GPU backend. An ImGui context must already exist. Returns false and logs
// through SDL on failure.
bool createRenderer(Renderer &renderer, SDL_Window *window, float terrainSize);
void destroyRenderer(Renderer &renderer);

// Uploads the park mesh drawn from now on, replacing the one before. An empty mesh draws
// nothing. Returns false and logs through SDL on failure.
bool setParkMesh(Renderer &renderer, const ParkMesh &mesh);

// Uploads the food overlay's mesh drawn from now on, replacing the one before, drawn opaque over
// the terrain and under the park. An empty mesh draws nothing. Returns false and logs through SDL
// on failure.
bool setOverlayMesh(Renderer &renderer, const ParkMesh &mesh);

// Uploads the translucent mesh of ghosts and highlights drawn from now on, replacing the one
// before. An empty mesh draws nothing. Returns false and logs through SDL on failure.
bool setGhostMesh(Renderer &renderer, const ParkMesh &mesh);
// Uploads the guests' mesh drawn from now on, replacing the one before. An empty mesh draws
// nothing. Returns false and logs through SDL on failure.
bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh);

// Starts the GPU backend's part of an ImGui frame. Call before ImGui::NewFrame.
void beginUiFrame();

// Draws one frame: the scene, then `ui` over it when non-null. When `capturePath` is non-null
// the frame, including the UI, is also written there as a BMP.
bool drawFrame(Renderer &renderer, const CameraView &camera, ImDrawData *ui,
               const char *capturePath);

} // namespace tpj

#endif
