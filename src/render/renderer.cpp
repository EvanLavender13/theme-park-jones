#include "render/renderer.h"

#include "core/profile.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>

#include <stddef.h>
#include <vector>

namespace tpj {
namespace {

constexpr SDL_GPUTextureFormat COLOR_FORMAT = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
constexpr SDL_FColor SKY_COLOR = {0.62f, 0.76f, 0.90f, 1.0f};
constexpr uint32_t TERRAIN_CELLS_PER_SIDE = 128;

struct TerrainVertex {
  float Position[3];
  float Normal[3];
};

// Matches the std140 `Camera` block in terrain.vert.
struct CameraUniforms {
  Mat4 ViewProjection;
  float Eye[4];
};

void logError(const char *what) {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", what, SDL_GetError());
}

SDL_GPUShader *loadShader(SDL_GPUDevice *device, const char *fileName, SDL_GPUShaderStage stage,
                          uint32_t uniformBufferCount) {
  char path[1024];
  SDL_snprintf(path, sizeof(path), "%sshaders/%s", SDL_GetBasePath(), fileName);
  size_t size = 0;
  void *code = SDL_LoadFile(path, &size);
  if (code == nullptr) {
    logError(path);
    return nullptr;
  }

  SDL_GPUShaderCreateInfo info = {};
  info.code = static_cast<const Uint8 *>(code);
  info.code_size = size;
  info.entrypoint = "main";
  info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = stage;
  info.num_uniform_buffers = uniformBufferCount;
  SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
  SDL_free(code);
  if (shader == nullptr) {
    logError(fileName);
  }
  return shader;
}

SDL_GPUTextureFormat chooseDepthFormat(SDL_GPUDevice *device) {
  const SDL_GPUTextureFormat candidates[] = {SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                             SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                             SDL_GPU_TEXTUREFORMAT_D16_UNORM};
  for (const SDL_GPUTextureFormat format : candidates) {
    if (SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D,
                                     SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
      return format;
    }
  }
  return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
}

// A lit, depth-tested pipeline with back faces culled. Depth is reversed: nearer is greater. A
// translucent one blends by alpha, passes equal depths, and writes no depth.
SDL_GPUGraphicsPipeline *createPipeline(const Renderer &renderer, const char *vertexFile,
                                        const char *fragmentFile,
                                        const SDL_GPUVertexAttribute *attributes,
                                        uint32_t attributeCount, uint32_t pitch, bool translucent) {
  SDL_GPUShader *vertexShader =
      loadShader(renderer.Device, vertexFile, SDL_GPU_SHADERSTAGE_VERTEX, 1);
  SDL_GPUShader *fragmentShader =
      loadShader(renderer.Device, fragmentFile, SDL_GPU_SHADERSTAGE_FRAGMENT, 0);
  if (vertexShader == nullptr || fragmentShader == nullptr) {
    SDL_ReleaseGPUShader(renderer.Device, vertexShader);
    SDL_ReleaseGPUShader(renderer.Device, fragmentShader);
    return nullptr;
  }

  SDL_GPUVertexBufferDescription vertexBuffer = {};
  vertexBuffer.slot = 0;
  vertexBuffer.pitch = pitch;
  vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

  SDL_GPUColorTargetDescription colorTarget = {};
  colorTarget.format = COLOR_FORMAT;
  if (translucent) {
    colorTarget.blend_state.enable_blend = true;
    colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  }

  SDL_GPUGraphicsPipelineCreateInfo info = {};
  info.vertex_shader = vertexShader;
  info.fragment_shader = fragmentShader;
  info.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
  info.vertex_input_state.num_vertex_buffers = 1;
  info.vertex_input_state.vertex_attributes = attributes;
  info.vertex_input_state.num_vertex_attributes = attributeCount;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.depth_stencil_state.enable_depth_test = true;
  info.depth_stencil_state.enable_depth_write = !translucent;
  info.depth_stencil_state.compare_op =
      translucent ? SDL_GPU_COMPAREOP_GREATER_OR_EQUAL : SDL_GPU_COMPAREOP_GREATER;
  info.target_info.color_target_descriptions = &colorTarget;
  info.target_info.num_color_targets = 1;
  info.target_info.depth_stencil_format = static_cast<SDL_GPUTextureFormat>(renderer.DepthFormat);
  info.target_info.has_depth_stencil_target = true;

  SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(renderer.Device, &info);
  SDL_ReleaseGPUShader(renderer.Device, vertexShader);
  SDL_ReleaseGPUShader(renderer.Device, fragmentShader);
  if (pipeline == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot create pipeline for %s: %s", vertexFile,
                 SDL_GetError());
  }
  return pipeline;
}

bool createTerrainPipeline(Renderer &renderer) {
  SDL_GPUVertexAttribute attributes[2] = {};
  attributes[0].location = 0;
  attributes[0].buffer_slot = 0;
  attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attributes[0].offset = offsetof(TerrainVertex, Position);
  attributes[1].location = 1;
  attributes[1].buffer_slot = 0;
  attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attributes[1].offset = offsetof(TerrainVertex, Normal);
  renderer.TerrainPipeline = createPipeline(renderer, "terrain.vert.spv", "terrain.frag.spv",
                                            attributes, 2, sizeof(TerrainVertex), false);
  return renderer.TerrainPipeline != nullptr;
}

// The vertex attributes read the color as four floats.
static_assert(sizeof(Rgba) == 4 * sizeof(float));

bool createParkPipeline(Renderer &renderer) {
  SDL_GPUVertexAttribute attributes[3] = {};
  attributes[0].location = 0;
  attributes[0].buffer_slot = 0;
  attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attributes[0].offset = offsetof(ParkVertex, Position);
  attributes[1].location = 1;
  attributes[1].buffer_slot = 0;
  attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attributes[1].offset = offsetof(ParkVertex, Normal);
  attributes[2].location = 2;
  attributes[2].buffer_slot = 0;
  attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
  attributes[2].offset = offsetof(ParkVertex, Color);
  renderer.ParkPipeline = createPipeline(renderer, "park.vert.spv", "park.frag.spv", attributes, 3,
                                         sizeof(ParkVertex), false);
  renderer.GhostPipeline = createPipeline(renderer, "park.vert.spv", "park.frag.spv", attributes, 3,
                                          sizeof(ParkVertex), true);
  return renderer.ParkPipeline != nullptr && renderer.GhostPipeline != nullptr;
}

constexpr uint32_t TERRAIN_VERTICES_PER_SIDE = TERRAIN_CELLS_PER_SIDE + 1;
constexpr uint32_t TERRAIN_VERTEX_COUNT = TERRAIN_VERTICES_PER_SIDE * TERRAIN_VERTICES_PER_SIDE;
constexpr uint32_t TERRAIN_INDEX_COUNT = TERRAIN_CELLS_PER_SIDE * TERRAIN_CELLS_PER_SIDE * 6;
constexpr uint32_t TERRAIN_VERTEX_BYTES = TERRAIN_VERTEX_COUNT * sizeof(TerrainVertex);
constexpr uint32_t TERRAIN_INDEX_BYTES = TERRAIN_INDEX_COUNT * sizeof(uint32_t);

// Writes a flat grid mesh. Each cell is two triangles wound counter-clockwise when seen from
// above, so their front faces point up.
void writeTerrainGeometry(float terrainSize, TerrainVertex *vertices, uint32_t *indices) {
  constexpr uint32_t side = TERRAIN_VERTICES_PER_SIDE;
  const float cellSize = terrainSize / static_cast<float>(TERRAIN_CELLS_PER_SIDE);
  const float origin = -0.5f * terrainSize;
  for (uint32_t j = 0; j < side; ++j) {
    for (uint32_t i = 0; i < side; ++i) {
      TerrainVertex &vertex = vertices[j * side + i];
      vertex.Position[0] = origin + static_cast<float>(i) * cellSize;
      vertex.Position[1] = 0.0f;
      vertex.Position[2] = origin + static_cast<float>(j) * cellSize;
      vertex.Normal[0] = 0.0f;
      vertex.Normal[1] = 1.0f;
      vertex.Normal[2] = 0.0f;
    }
  }

  uint32_t *index = indices;
  for (uint32_t j = 0; j < TERRAIN_CELLS_PER_SIDE; ++j) {
    for (uint32_t i = 0; i < TERRAIN_CELLS_PER_SIDE; ++i) {
      const uint32_t x0z0 = j * side + i;
      const uint32_t x1z0 = x0z0 + 1;
      const uint32_t x0z1 = x0z0 + side;
      const uint32_t x1z1 = x0z1 + 1;
      *index++ = x0z0;
      *index++ = x0z1;
      *index++ = x1z0;
      *index++ = x1z0;
      *index++ = x0z1;
      *index++ = x1z1;
    }
  }
}

SDL_GPUBuffer *createBuffer(SDL_GPUDevice *device, SDL_GPUBufferUsageFlags usage, uint32_t size) {
  SDL_GPUBufferCreateInfo info = {};
  info.usage = usage;
  info.size = size;
  return SDL_CreateGPUBuffer(device, &info);
}

// Creates a vertex and an index buffer and uploads their contents in one copy pass.
bool uploadMesh(SDL_GPUDevice *device, const void *vertices, uint32_t vertexBytes,
                const void *indices, uint32_t indexBytes, SDL_GPUBuffer *&vertexBuffer,
                SDL_GPUBuffer *&indexBuffer) {
  vertexBuffer = createBuffer(device, SDL_GPU_BUFFERUSAGE_VERTEX, vertexBytes);
  indexBuffer = createBuffer(device, SDL_GPU_BUFFERUSAGE_INDEX, indexBytes);

  SDL_GPUTransferBufferCreateInfo transferInfo = {};
  transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transferInfo.size = vertexBytes + indexBytes;
  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
  if (vertexBuffer == nullptr || indexBuffer == nullptr || transfer == nullptr) {
    logError("Cannot create mesh buffers");
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }

  auto *mapped = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(device, transfer, false));
  if (mapped == nullptr) {
    logError("Cannot map mesh transfer buffer");
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  SDL_memcpy(mapped, vertices, vertexBytes);
  SDL_memcpy(mapped + vertexBytes, indices, indexBytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
  if (commands == nullptr) {
    logError("Cannot acquire command buffer");
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);

  SDL_GPUTransferBufferLocation source = {};
  source.transfer_buffer = transfer;
  SDL_GPUBufferRegion destination = {};
  destination.buffer = vertexBuffer;
  destination.size = vertexBytes;
  SDL_UploadToGPUBuffer(copy, &source, &destination, false);

  source.offset = vertexBytes;
  destination.buffer = indexBuffer;
  destination.size = indexBytes;
  SDL_UploadToGPUBuffer(copy, &source, &destination, false);

  SDL_EndGPUCopyPass(copy);
  const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  if (!submitted) {
    logError("Cannot submit mesh upload");
  }
  return submitted;
}

bool createTerrainMesh(Renderer &renderer, float terrainSize) {
  std::vector<TerrainVertex> vertices(TERRAIN_VERTEX_COUNT);
  std::vector<uint32_t> indices(TERRAIN_INDEX_COUNT);
  writeTerrainGeometry(terrainSize, vertices.data(), indices.data());
  if (!uploadMesh(renderer.Device, vertices.data(), TERRAIN_VERTEX_BYTES, indices.data(),
                  TERRAIN_INDEX_BYTES, renderer.TerrainVertices, renderer.TerrainIndices)) {
    return false;
  }
  renderer.TerrainIndexCount = TERRAIN_INDEX_COUNT;
  return true;
}

// (Re)creates the offscreen color and depth targets when the swapchain size changes.
bool ensureRenderTargets(Renderer &renderer, uint32_t width, uint32_t height) {
  if (renderer.ColorTarget != nullptr && renderer.TargetWidth == width &&
      renderer.TargetHeight == height) {
    return true;
  }
  SDL_ReleaseGPUTexture(renderer.Device, renderer.ColorTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.DepthTarget);

  SDL_GPUTextureCreateInfo info = {};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.width = width;
  info.height = height;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;

  info.format = COLOR_FORMAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  renderer.ColorTarget = SDL_CreateGPUTexture(renderer.Device, &info);

  info.format = static_cast<SDL_GPUTextureFormat>(renderer.DepthFormat);
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  renderer.DepthTarget = SDL_CreateGPUTexture(renderer.Device, &info);

  if (renderer.ColorTarget == nullptr || renderer.DepthTarget == nullptr) {
    logError("Cannot create render targets");
    return false;
  }
  renderer.TargetWidth = width;
  renderer.TargetHeight = height;
  return true;
}

// Queues a readback of the color target, submits, waits, and writes the pixels as a BMP.
bool submitWithCapture(Renderer &renderer, SDL_GPUCommandBuffer *commands, const char *path) {
  const uint32_t width = renderer.TargetWidth;
  const uint32_t height = renderer.TargetHeight;

  SDL_GPUTransferBufferCreateInfo transferInfo = {};
  transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
  transferInfo.size = width * height * 4;
  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(renderer.Device, &transferInfo);
  if (transfer == nullptr) {
    logError("Cannot create capture buffer");
    SDL_SubmitGPUCommandBuffer(commands);
    return false;
  }

  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureRegion source = {};
  source.texture = renderer.ColorTarget;
  source.w = width;
  source.h = height;
  source.d = 1;
  SDL_GPUTextureTransferInfo destination = {};
  destination.transfer_buffer = transfer;
  SDL_DownloadFromGPUTexture(copy, &source, &destination);
  SDL_EndGPUCopyPass(copy);

  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  bool saved = false;
  if (fence != nullptr && SDL_WaitForGPUFences(renderer.Device, true, &fence, 1)) {
    void *pixels = SDL_MapGPUTransferBuffer(renderer.Device, transfer, false);
    SDL_Surface *surface =
        SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height),
                              SDL_PIXELFORMAT_RGBA32, pixels, static_cast<int>(width * 4));
    saved = surface != nullptr && SDL_SaveBMP(surface, path);
    SDL_DestroySurface(surface);
    SDL_UnmapGPUTransferBuffer(renderer.Device, transfer);
  }
  if (!saved) {
    logError("Cannot save capture");
  }
  SDL_ReleaseGPUFence(renderer.Device, fence);
  SDL_ReleaseGPUTransferBuffer(renderer.Device, transfer);
  return saved;
}

} // namespace

bool createRenderer(Renderer &renderer, SDL_Window *window, float terrainSize) {
#ifdef NDEBUG
  constexpr bool debugMode = false;
#else
  constexpr bool debugMode = true;
#endif
  renderer.Window = window;
  renderer.Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debugMode, nullptr);
  if (renderer.Device == nullptr) {
    logError("Cannot create GPU device");
    return false;
  }
  SDL_Log("GPU driver: %s", SDL_GetGPUDeviceDriver(renderer.Device));
  if (!SDL_ClaimWindowForGPUDevice(renderer.Device, window)) {
    logError("Cannot claim window for GPU device");
    return false;
  }
  renderer.DepthFormat = chooseDepthFormat(renderer.Device);
  if (!createTerrainPipeline(renderer) || !createParkPipeline(renderer) ||
      !createTerrainMesh(renderer, terrainSize)) {
    return false;
  }

  ImGui_ImplSDLGPU3_InitInfo uiInfo = {};
  uiInfo.Device = renderer.Device;
  uiInfo.ColorTargetFormat = COLOR_FORMAT;
  uiInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  renderer.UiInitialized = ImGui_ImplSDLGPU3_Init(&uiInfo);
  if (!renderer.UiInitialized) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot initialize the ImGui GPU backend");
  }
  return renderer.UiInitialized;
}

void destroyRenderer(Renderer &renderer) {
  if (renderer.Device == nullptr) {
    return;
  }
  SDL_WaitForGPUIdle(renderer.Device);
  if (renderer.UiInitialized) {
    ImGui_ImplSDLGPU3_Shutdown();
  }
  SDL_ReleaseGPUTexture(renderer.Device, renderer.ColorTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.DepthTarget);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.TerrainVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.TerrainIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.TerrainPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayIndices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.ParkVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.ParkIndices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GuestVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GuestIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.ParkPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GhostVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GhostIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.GhostPipeline);
  if (renderer.Window != nullptr) {
    SDL_ReleaseWindowFromGPUDevice(renderer.Device, renderer.Window);
  }
  SDL_DestroyGPUDevice(renderer.Device);
  renderer = Renderer{};
}

namespace {

// Replaces a mesh's buffers with the mesh's, or with none when it is empty.
bool replaceMesh(Renderer &renderer, const ParkMesh &mesh, SDL_GPUBuffer *&vertices,
                 SDL_GPUBuffer *&indices, uint32_t &indexCount) {
  // SDL_GPU defers the release until the GPU no longer uses the buffers.
  SDL_ReleaseGPUBuffer(renderer.Device, vertices);
  SDL_ReleaseGPUBuffer(renderer.Device, indices);
  vertices = nullptr;
  indices = nullptr;
  indexCount = 0;
  if (mesh.Indices.empty()) {
    return true;
  }
  const auto vertexBytes = static_cast<uint32_t>(mesh.Vertices.size() * sizeof(ParkVertex));
  const auto indexBytes = static_cast<uint32_t>(mesh.Indices.size() * sizeof(uint32_t));
  if (!uploadMesh(renderer.Device, mesh.Vertices.data(), vertexBytes, mesh.Indices.data(),
                  indexBytes, vertices, indices)) {
    return false;
  }
  indexCount = static_cast<uint32_t>(mesh.Indices.size());
  return true;
}

} // namespace

bool setParkMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.ParkVertices, renderer.ParkIndices,
                     renderer.ParkIndexCount);
}

bool setOverlayMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.OverlayVertices, renderer.OverlayIndices,
                     renderer.OverlayIndexCount);
}

bool setGhostMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.GhostVertices, renderer.GhostIndices,
                     renderer.GhostIndexCount);
}

bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.GuestVertices, renderer.GuestIndices,
                     renderer.GuestIndexCount);
}

void beginUiFrame() { ImGui_ImplSDLGPU3_NewFrame(); }

namespace {

// Draws a mesh's triangles through a pipeline, or nothing for an empty mesh.
void drawMesh(SDL_GPURenderPass *pass, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUBuffer *vertices,
              SDL_GPUBuffer *indices, uint32_t indexCount) {
  if (indexCount == 0) {
    return;
  }
  SDL_BindGPUGraphicsPipeline(pass, pipeline);
  const SDL_GPUBufferBinding vertexBinding = {vertices, 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
  const SDL_GPUBufferBinding indexBinding = {indices, 0};
  SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  SDL_DrawGPUIndexedPrimitives(pass, indexCount, 1, 0, 0, 0);
}

void drawScene(const Renderer &renderer, SDL_GPUCommandBuffer *commands, const CameraView &camera) {
  const float aspect =
      static_cast<float>(renderer.TargetWidth) / static_cast<float>(renderer.TargetHeight);
  CameraUniforms uniforms = {};
  uniforms.ViewProjection = multiply(perspective(camera.FovY, aspect, camera.NearZ, camera.FarZ),
                                     lookAt(camera.Eye, camera.Target, {0.0f, 1.0f, 0.0f}));
  uniforms.Eye[0] = camera.Eye.X;
  uniforms.Eye[1] = camera.Eye.Y;
  uniforms.Eye[2] = camera.Eye.Z;
  uniforms.Eye[3] = 1.0f;
  SDL_PushGPUVertexUniformData(commands, 0, &uniforms, sizeof(uniforms));

  SDL_GPUColorTargetInfo colorTarget = {};
  colorTarget.texture = renderer.ColorTarget;
  colorTarget.clear_color = SKY_COLOR;
  colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
  colorTarget.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depthTarget = {};
  depthTarget.texture = renderer.DepthTarget;
  // Reversed depth clears to the far plane, 0.
  depthTarget.clear_depth = 0.0f;
  depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
  depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
  depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depthTarget.cycle = true;

  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, &depthTarget);
  SDL_BindGPUGraphicsPipeline(pass, renderer.TerrainPipeline);
  const SDL_GPUBufferBinding vertexBinding = {renderer.TerrainVertices, 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
  const SDL_GPUBufferBinding indexBinding = {renderer.TerrainIndices, 0};
  SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  SDL_DrawGPUIndexedPrimitives(pass, renderer.TerrainIndexCount, 1, 0, 0, 0);

  // The camera uniforms pushed before the pass serve these pipelines too. The overlay lies just
  // over the terrain, and the park's paths, boxes, and guests over it. Guests are opaque boxes,
  // drawn as the park's are. Ghosts and highlights come after everything opaque, so they blend over
  // it.
  drawMesh(pass, renderer.ParkPipeline, renderer.OverlayVertices, renderer.OverlayIndices,
           renderer.OverlayIndexCount);
  drawMesh(pass, renderer.ParkPipeline, renderer.ParkVertices, renderer.ParkIndices,
           renderer.ParkIndexCount);
  drawMesh(pass, renderer.ParkPipeline, renderer.GuestVertices, renderer.GuestIndices,
           renderer.GuestIndexCount);
  drawMesh(pass, renderer.GhostPipeline, renderer.GhostVertices, renderer.GhostIndices,
           renderer.GhostIndexCount);
  SDL_EndGPURenderPass(pass);
}

// The UI draws into the offscreen target over the scene, so captures include it.
void drawUi(const Renderer &renderer, SDL_GPUCommandBuffer *commands, ImDrawData *ui) {
  ImGui_ImplSDLGPU3_PrepareDrawData(ui, commands);
  SDL_GPUColorTargetInfo uiTarget = {};
  uiTarget.texture = renderer.ColorTarget;
  uiTarget.load_op = SDL_GPU_LOADOP_LOAD;
  uiTarget.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &uiTarget, 1, nullptr);
  ImGui_ImplSDLGPU3_RenderDrawData(ui, commands, pass);
  SDL_EndGPURenderPass(pass);
}

void blitToSwapchain(const Renderer &renderer, SDL_GPUCommandBuffer *commands,
                     SDL_GPUTexture *swapchain) {
  SDL_GPUBlitInfo blit = {};
  blit.source.texture = renderer.ColorTarget;
  blit.source.w = renderer.TargetWidth;
  blit.source.h = renderer.TargetHeight;
  blit.destination.texture = swapchain;
  blit.destination.w = renderer.TargetWidth;
  blit.destination.h = renderer.TargetHeight;
  blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
  blit.filter = SDL_GPU_FILTER_NEAREST;
  SDL_BlitGPUTexture(commands, &blit);
}

} // namespace

bool drawFrame(Renderer &renderer, const CameraView &camera, ImDrawData *ui,
               const char *capturePath) {
  TPJ_PROFILE_ZONE();
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(renderer.Device);
  if (commands == nullptr) {
    logError("Cannot acquire command buffer");
    return false;
  }

  SDL_GPUTexture *swapchain = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, renderer.Window, &swapchain, &width,
                                             &height)) {
    logError("Cannot acquire swapchain texture");
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  // A null swapchain texture means the window is minimized; skip the frame.
  if (swapchain == nullptr || !ensureRenderTargets(renderer, width, height)) {
    return SDL_SubmitGPUCommandBuffer(commands) && swapchain == nullptr;
  }

  drawScene(renderer, commands, camera);
  if (ui != nullptr && renderer.UiInitialized) {
    drawUi(renderer, commands, ui);
  }
  blitToSwapchain(renderer, commands, swapchain);

  if (capturePath != nullptr) {
    return submitWithCapture(renderer, commands, capturePath);
  }
  return SDL_SubmitGPUCommandBuffer(commands);
}

} // namespace tpj
