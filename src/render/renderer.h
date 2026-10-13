#ifndef TPJ_RENDER_RENDERER_H
#define TPJ_RENDER_RENDERER_H

#include "render/day_light.h"
#include "render/math.h"
#include "render/park_mesh.h"
#include "render/paving_overlay.h"

#include <stdint.h>

struct ImDrawData;
struct SDL_GPUBuffer;
struct SDL_GPUCommandBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;
struct SDL_GPUTransferBuffer;
struct SDL_Window;

namespace tpj {

struct CameraView {
  Vec3 Eye;
  Vec3 Target;
  float FovY = 0.9f;
  float NearZ = 0.1f;
  float FarZ = 2000.0f;
};

// A GPU buffer the renderer keeps, the transfer buffer that fills it, and the bytes both hold.
struct KeptBuffer {
  SDL_GPUBuffer *Buffer = nullptr;
  SDL_GPUTransferBuffer *Transfer = nullptr;
  uint32_t Capacity = 0;
};

// A mesh's kept vertex and index buffers, and the indices last uploaded.
struct KeptMesh {
  KeptBuffer Vertices;
  KeptBuffer Indices;
  uint32_t IndexCount = 0;
};

// The scene renders in linear light into a float target, which is tonemapped into an offscreen
// color target. The overlay, ghosts, and UI draw over that, and it is then blitted to the
// swapchain, which lets any frame be read back for captures.
struct Renderer {
  SDL_Window *Window = nullptr;
  SDL_GPUDevice *Device = nullptr;
  // The frame's command buffer, once an upload or drawFrame has acquired it. drawFrame submits it.
  SDL_GPUCommandBuffer *Commands = nullptr;
  SDL_GPUGraphicsPipeline *TerrainPipeline = nullptr;
  KeptMesh Terrain;
  SDL_GPUGraphicsPipeline *ParkPipeline = nullptr;
  KeptMesh Park;
  KeptMesh Guests;
  SDL_GPUGraphicsPipeline *GhostPipeline = nullptr;
  KeptMesh Ghost;
  SDL_GPUGraphicsPipeline *MarksPipeline = nullptr;
  KeptMesh Tints;
  KeptMesh Marks;
  SDL_GPUGraphicsPipeline *OverlayPipeline = nullptr;
  KeptMesh Overlay;
  KeptBuffer OverlayShape;
  KeptBuffer OverlayShades;
  float OverlayHalf = 1.0f;
  SDL_GPUGraphicsPipeline *TonemapPipeline = nullptr;
  SDL_GPUSampler *SceneSampler = nullptr;
  SDL_GPUTexture *SceneTarget = nullptr;
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

// Uploads the park mesh drawn from the next frame on, replacing the one before. An empty mesh
// draws nothing. Returns false and logs through SDL on failure. It reuses the buffers the renderer
// keeps, and makes larger ones only when the data outgrows them.
bool setParkMesh(Renderer &renderer, const ParkMesh &mesh);

// Uploads the paving overlay's shape drawn from the next frame on, replacing the one before, drawn
// opaque over the guest paving and under its edge lines. An empty shape draws nothing. Returns
// false and logs through SDL on failure. It reuses the buffers the renderer keeps, and makes larger
// ones only when the data outgrows them.
bool setOverlayShape(Renderer &renderer, const OverlayShape &shape);
// Uploads the shades the overlay's shape is colored by from the next frame on, replacing the ones
// before. They must be the shades of the shape last set. Returns false and logs through SDL on
// failure. It reuses the buffers the renderer keeps, and makes larger ones only
// when the data outgrows them.
bool setOverlayShades(Renderer &renderer, const OverlayShades &shades);

// Uploads the translucent ghost drawn from the next frame on, its tints, the park it previews, and
// its marks, replacing the one before. An empty part draws nothing. Returns false and logs through
// SDL on failure. It reuses the buffers the renderer keeps, and makes larger ones only
// when the data outgrows them.
bool setGhostMesh(Renderer &renderer, const GhostMesh &ghost);
// Uploads the guests' mesh drawn from the next frame on, replacing the one before. An empty mesh
// draws nothing. Returns false and logs through SDL on failure. It reuses the buffers the renderer
// keeps, and makes larger ones only when the data outgrows them.
bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh);

// Starts the GPU backend's part of an ImGui frame. Call before ImGui::NewFrame.
void beginUiFrame();

// Draws one frame: the scene lit by `light`, then `ui` over it when non-null. When `capturePath` is
// non-null the frame, including the UI, is also written there as a BMP.
bool drawFrame(Renderer &renderer, const CameraView &camera, const DayLight &light, ImDrawData *ui,
               const char *capturePath);

} // namespace tpj

#endif
