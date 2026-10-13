#include "render/renderer.h"

#include "core/profile.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>

#include <algorithm>
#include <bit>
#include <span>
#include <stddef.h>
#include <tuple>
#include <vector>

namespace tpj {
namespace {

constexpr SDL_GPUTextureFormat COLOR_FORMAT = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
// The scene is lit in linear light, which needs more range and precision than COLOR_FORMAT has.
constexpr SDL_GPUTextureFormat SCENE_FORMAT = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
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

// Matches the std140 `Light` block in light.glsl. Settings holds 1 in its first float when the
// light gives brightness only, and the exposure in its second.
struct LightUniforms {
  float SunDirection[4];
  float Sun[4];
  float MoonDirection[4];
  float Moon[4];
  float AmbientSky[4];
  float AmbientGround[4];
  float Sky[4];
  float Settings[4];
};

// Matches the std140 `Ramp` block in overlay.frag: the ramp's colors and the value that shades
// halfway up the ramp, in Middle's first float.
struct RampUniforms {
  Rgba Colors[OVERLAY_RAMP.size()];
  float Middle[4];
};

void logError(const char *what) {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", what, SDL_GetError());
}

SDL_GPUShader *loadShader(SDL_GPUDevice *device, const char *fileName, SDL_GPUShaderStage stage,
                          uint32_t uniformBufferCount, uint32_t storageBufferCount,
                          uint32_t samplerCount) {
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
  info.num_storage_buffers = storageBufferCount;
  info.num_samplers = samplerCount;
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

// The resources a fragment shader reads: its uniform buffers, its storage buffers, and its
// samplers.
struct FragmentBuffers {
  uint32_t Uniforms = 0;
  uint32_t Storage = 0;
  uint32_t Samplers = 0;
};

// The vertex attributes a pipeline reads from one buffer, and the bytes between vertices.
struct VertexLayout {
  const SDL_GPUVertexAttribute *Attributes = nullptr;
  uint32_t AttributeCount = 0;
  uint32_t Pitch = 0;
};

// A depth-tested pipeline with back faces culled, drawing into a target of the given format.
// Depth is reversed: nearer is greater. A translucent one blends by alpha, passes equal depths,
// and writes no depth.
SDL_GPUGraphicsPipeline *createPipeline(const Renderer &renderer, const char *vertexFile,
                                        const char *fragmentFile, VertexLayout layout,
                                        bool translucent, FragmentBuffers fragmentBuffers,
                                        SDL_GPUTextureFormat targetFormat) {
  SDL_GPUShader *vertexShader =
      loadShader(renderer.Device, vertexFile, SDL_GPU_SHADERSTAGE_VERTEX, 1, 0, 0);
  SDL_GPUShader *fragmentShader =
      loadShader(renderer.Device, fragmentFile, SDL_GPU_SHADERSTAGE_FRAGMENT,
                 fragmentBuffers.Uniforms, fragmentBuffers.Storage, fragmentBuffers.Samplers);
  if (vertexShader == nullptr || fragmentShader == nullptr) {
    SDL_ReleaseGPUShader(renderer.Device, vertexShader);
    SDL_ReleaseGPUShader(renderer.Device, fragmentShader);
    return nullptr;
  }

  SDL_GPUVertexBufferDescription vertexBuffer = {};
  vertexBuffer.slot = 0;
  vertexBuffer.pitch = layout.Pitch;
  vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

  SDL_GPUColorTargetDescription colorTarget = {};
  colorTarget.format = targetFormat;
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
  info.vertex_input_state.vertex_attributes = layout.Attributes;
  info.vertex_input_state.num_vertex_attributes = layout.AttributeCount;
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
                                            VertexLayout{attributes, 2, sizeof(TerrainVertex)},
                                            false, FragmentBuffers{.Uniforms = 1}, SCENE_FORMAT);
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
  const VertexLayout layout{attributes, 3, sizeof(ParkVertex)};
  const FragmentBuffers light{.Uniforms = 1};
  renderer.ParkPipeline = createPipeline(renderer, "park.vert.spv", "park.frag.spv", layout, false,
                                         light, SCENE_FORMAT);
  // Ghosts and marks draw after the tonemap, into the color target.
  renderer.GhostPipeline = createPipeline(renderer, "park.vert.spv", "ghost.frag.spv", layout, true,
                                          light, COLOR_FORMAT);
  renderer.MarksPipeline = createPipeline(renderer, "park.vert.spv", "marks.frag.spv", layout, true,
                                          light, COLOR_FORMAT);
  return renderer.ParkPipeline != nullptr && renderer.GhostPipeline != nullptr &&
         renderer.MarksPipeline != nullptr;
}

bool createOverlayPipeline(Renderer &renderer) {
  SDL_GPUVertexAttribute attributes[2] = {};
  attributes[0].location = 0;
  attributes[0].buffer_slot = 0;
  attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attributes[0].offset = offsetof(OverlayVertex, Position);
  attributes[1].location = 1;
  attributes[1].buffer_slot = 0;
  attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
  attributes[1].offset = offsetof(OverlayVertex, Piece);
  renderer.OverlayPipeline =
      createPipeline(renderer, "overlay.vert.spv", "overlay.frag.spv",
                     VertexLayout{attributes, 2, sizeof(OverlayVertex)}, false,
                     FragmentBuffers{.Uniforms = 2, .Storage = 2}, COLOR_FORMAT);
  return renderer.OverlayPipeline != nullptr;
}

// The full-screen pass that tonemaps the scene target into the color target, and the sampler it
// reads the scene with.
bool createTonemapPipeline(Renderer &renderer) {
  SDL_GPUShader *vertexShader =
      loadShader(renderer.Device, "tonemap.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0);
  SDL_GPUShader *fragmentShader =
      loadShader(renderer.Device, "tonemap.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 1);
  if (vertexShader == nullptr || fragmentShader == nullptr) {
    SDL_ReleaseGPUShader(renderer.Device, vertexShader);
    SDL_ReleaseGPUShader(renderer.Device, fragmentShader);
    return false;
  }
  SDL_GPUColorTargetDescription colorTarget = {};
  colorTarget.format = COLOR_FORMAT;
  SDL_GPUGraphicsPipelineCreateInfo info = {};
  info.vertex_shader = vertexShader;
  info.fragment_shader = fragmentShader;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.target_info.color_target_descriptions = &colorTarget;
  info.target_info.num_color_targets = 1;
  renderer.TonemapPipeline = SDL_CreateGPUGraphicsPipeline(renderer.Device, &info);
  SDL_ReleaseGPUShader(renderer.Device, vertexShader);
  SDL_ReleaseGPUShader(renderer.Device, fragmentShader);
  if (renderer.TonemapPipeline == nullptr) {
    logError("Cannot create the tonemap pipeline");
    return false;
  }
  // The pass reads the scene at the size it writes it, so it never filters.
  SDL_GPUSamplerCreateInfo sampler = {};
  sampler.min_filter = SDL_GPU_FILTER_NEAREST;
  sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
  sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  renderer.SceneSampler = SDL_CreateGPUSampler(renderer.Device, &sampler);
  if (renderer.SceneSampler == nullptr) {
    logError("Cannot create the scene sampler");
    return false;
  }
  return true;
}

constexpr uint32_t TERRAIN_VERTICES_PER_SIDE = TERRAIN_CELLS_PER_SIDE + 1;
constexpr uint32_t TERRAIN_VERTEX_COUNT = TERRAIN_VERTICES_PER_SIDE * TERRAIN_VERTICES_PER_SIDE;
constexpr uint32_t TERRAIN_INDEX_COUNT = TERRAIN_CELLS_PER_SIDE * TERRAIN_CELLS_PER_SIDE * 6;
constexpr uint32_t TERRAIN_VERTEX_BYTES = TERRAIN_VERTEX_COUNT * sizeof(TerrainVertex);

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

// The frame's command buffer, acquired by the frame's first upload or by drawFrame. Null after
// logging when none can be acquired.
SDL_GPUCommandBuffer *frameCommands(Renderer &renderer) {
  if (renderer.Commands == nullptr) {
    renderer.Commands = SDL_AcquireGPUCommandBuffer(renderer.Device);
    if (renderer.Commands == nullptr) {
      logError("Cannot acquire command buffer");
    }
  }
  return renderer.Commands;
}

void releaseKept(SDL_GPUDevice *device, KeptBuffer &kept) {
  // SDL_GPU defers the release until the GPU no longer uses the buffers.
  SDL_ReleaseGPUBuffer(device, kept.Buffer);
  SDL_ReleaseGPUTransferBuffer(device, kept.Transfer);
  kept = KeptBuffer{};
}

// Makes the kept buffer hold at least the bytes. It keeps the buffers it holds when they are large
// enough, and otherwise replaces them with ones twice the bytes, so a mesh that grows a little, as
// the guests' does when more arrive, fits without new buffers until it has doubled.
bool fitBuffer(Renderer &renderer, KeptBuffer &kept, SDL_GPUBufferUsageFlags usage,
               uint32_t bytes) {
  if (bytes <= kept.Capacity) {
    return true;
  }
  const uint32_t capacity = 2 * bytes;
  releaseKept(renderer.Device, kept);
  SDL_GPUBufferCreateInfo info = {};
  info.usage = usage;
  info.size = capacity;
  kept.Buffer = SDL_CreateGPUBuffer(renderer.Device, &info);
  SDL_GPUTransferBufferCreateInfo transferInfo = {};
  transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transferInfo.size = capacity;
  kept.Transfer = SDL_CreateGPUTransferBuffer(renderer.Device, &transferInfo);
  if (kept.Buffer == nullptr || kept.Transfer == nullptr) {
    logError("Cannot create a kept buffer");
    releaseKept(renderer.Device, kept);
    return false;
  }
  kept.Capacity = capacity;
  return true;
}

// Uploads the bytes write gives into the start of the kept buffer, on the frame's command buffer.
// Both the transfer buffer and the buffer cycle, so a frame the GPU still draws keeps its data.
template <typename Write>
bool uploadKept(Renderer &renderer, KeptBuffer &kept, SDL_GPUBufferUsageFlags usage, uint32_t bytes,
                Write write) {
  if (bytes == 0) {
    return true;
  }
  if (!fitBuffer(renderer, kept, usage, bytes)) {
    return false;
  }
  SDL_GPUCommandBuffer *commands = frameCommands(renderer);
  if (commands == nullptr) {
    return false;
  }
  void *mapped = SDL_MapGPUTransferBuffer(renderer.Device, kept.Transfer, true);
  if (mapped == nullptr) {
    logError("Cannot map a transfer buffer");
    return false;
  }
  write(mapped);
  SDL_UnmapGPUTransferBuffer(renderer.Device, kept.Transfer);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTransferBufferLocation source = {};
  source.transfer_buffer = kept.Transfer;
  SDL_GPUBufferRegion destination = {};
  destination.buffer = kept.Buffer;
  destination.size = bytes;
  SDL_UploadToGPUBuffer(copy, &source, &destination, true);
  SDL_EndGPUCopyPass(copy);
  return true;
}

// Uploads a mesh's vertices and indices into its kept buffers. An empty mesh keeps the buffers and
// draws nothing.
bool uploadMesh(Renderer &renderer, KeptMesh &mesh, const void *vertices, uint32_t vertexBytes,
                std::span<const uint32_t> indices) {
  mesh.IndexCount = 0;
  if (indices.empty()) {
    return true;
  }
  const auto indexBytes = static_cast<uint32_t>(indices.size_bytes());
  if (!uploadKept(renderer, mesh.Vertices, SDL_GPU_BUFFERUSAGE_VERTEX, vertexBytes,
                  [&](void *to) { SDL_memcpy(to, vertices, vertexBytes); }) ||
      !uploadKept(renderer, mesh.Indices, SDL_GPU_BUFFERUSAGE_INDEX, indexBytes,
                  [&](void *to) { SDL_memcpy(to, indices.data(), indexBytes); })) {
    return false;
  }
  mesh.IndexCount = static_cast<uint32_t>(indices.size());
  return true;
}

bool uploadMesh(Renderer &renderer, KeptMesh &kept, const ParkMesh &mesh) {
  return uploadMesh(renderer, kept, mesh.Vertices.data(),
                    static_cast<uint32_t>(mesh.Vertices.size() * sizeof(ParkVertex)), mesh.Indices);
}

uint32_t wordOf(double value) { return std::bit_cast<uint32_t>(static_cast<float>(value)); }

// The words writeShapeWords writes.
uint32_t shapeWordCount(const OverlayShape &shape) {
  constexpr size_t PIECE_WORDS = 2 * std::tuple_size_v<decltype(OverlayPiece::Corners)> + 5;
  return static_cast<uint32_t>(5 + PIECE_WORDS * shape.Pieces.size() + shape.LineRefs.size() +
                               2 * shape.Lines.size() + 3 * shape.Points.size() +
                               shape.Overlaps.size());
}

// The shape as overlay.frag reads it: five offsets, to the pieces, the line references, the
// lines, the points, and the overlaps, then each of them in turn.
void writeShapeWords(const OverlayShape &shape, uint32_t *words) {
  uint32_t *at = words + 5;
  const auto offset = [&] { return static_cast<uint32_t>(at - words); };
  words[0] = offset();
  for (const OverlayPiece &piece : shape.Pieces) {
    for (const ParkPoint &corner : piece.Corners) {
      *at++ = wordOf(corner.X);
      *at++ = wordOf(corner.Z);
    }
    *at++ = piece.Count;
    *at++ = piece.FirstLine;
    *at++ = piece.LineCount;
    *at++ = piece.FirstOverlap;
    *at++ = piece.OverlapCount;
  }
  words[1] = offset();
  at = std::copy(shape.LineRefs.begin(), shape.LineRefs.end(), at);
  words[2] = offset();
  for (const OverlayLine &line : shape.Lines) {
    *at++ = line.FirstPoint;
    *at++ = line.PointCount;
  }
  words[3] = offset();
  for (const CarrierPoint &point : shape.Points) {
    *at++ = wordOf(point.X);
    *at++ = wordOf(point.Z);
    *at++ = wordOf(point.Distance);
  }
  words[4] = offset();
  std::copy(shape.Overlaps.begin(), shape.Overlaps.end(), at);
}

// The words writeShadeWords writes.
uint32_t shadeWordCount(const OverlayShades &shades) {
  return static_cast<uint32_t>(1 + shades.FirstKnots.size() + 2 * shades.Knots.size());
}

// The shades as overlay.frag reads them: the offset to the knots, where each line's knots begin
// and where the last ends, then each knot's distance and value.
void writeShadeWords(const OverlayShades &shades, uint32_t *words) {
  uint32_t *at = words;
  *at++ = static_cast<uint32_t>(1 + shades.FirstKnots.size());
  at = std::copy(shades.FirstKnots.begin(), shades.FirstKnots.end(), at);
  for (const OverlayKnot &knot : shades.Knots) {
    *at++ = wordOf(knot.Distance);
    *at++ = wordOf(knot.Value);
  }
}

bool createTerrainMesh(Renderer &renderer, float terrainSize) {
  std::vector<TerrainVertex> vertices(TERRAIN_VERTEX_COUNT);
  std::vector<uint32_t> indices(TERRAIN_INDEX_COUNT);
  writeTerrainGeometry(terrainSize, vertices.data(), indices.data());
  // Recorded on the frame's command buffer, so it reaches the GPU with the first frame.
  return uploadMesh(renderer, renderer.Terrain, vertices.data(), TERRAIN_VERTEX_BYTES, indices);
}

// (Re)creates the offscreen color and depth targets when the swapchain size changes.
bool ensureRenderTargets(Renderer &renderer, uint32_t width, uint32_t height) {
  if (renderer.ColorTarget != nullptr && renderer.TargetWidth == width &&
      renderer.TargetHeight == height) {
    return true;
  }
  SDL_ReleaseGPUTexture(renderer.Device, renderer.SceneTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.ColorTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.DepthTarget);

  SDL_GPUTextureCreateInfo info = {};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.width = width;
  info.height = height;
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;

  info.format = SCENE_FORMAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  renderer.SceneTarget = SDL_CreateGPUTexture(renderer.Device, &info);

  info.format = COLOR_FORMAT;
  renderer.ColorTarget = SDL_CreateGPUTexture(renderer.Device, &info);

  info.format = static_cast<SDL_GPUTextureFormat>(renderer.DepthFormat);
  info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  renderer.DepthTarget = SDL_CreateGPUTexture(renderer.Device, &info);

  if (renderer.SceneTarget == nullptr || renderer.ColorTarget == nullptr ||
      renderer.DepthTarget == nullptr) {
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
  if (!SDL_GPUTextureSupportsFormat(renderer.Device, SCENE_FORMAT, SDL_GPU_TEXTURETYPE_2D,
                                    SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                        SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
    logError("The GPU cannot render the scene in a float target");
    return false;
  }
  if (!createTerrainPipeline(renderer) || !createParkPipeline(renderer) ||
      !createOverlayPipeline(renderer) || !createTonemapPipeline(renderer) ||
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
  if (renderer.Commands != nullptr) {
    SDL_SubmitGPUCommandBuffer(renderer.Commands);
  }
  SDL_WaitForGPUIdle(renderer.Device);
  if (renderer.UiInitialized) {
    ImGui_ImplSDLGPU3_Shutdown();
  }
  SDL_ReleaseGPUTexture(renderer.Device, renderer.SceneTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.ColorTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.DepthTarget);
  SDL_ReleaseGPUSampler(renderer.Device, renderer.SceneSampler);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.TonemapPipeline);
  for (KeptMesh *mesh : {&renderer.Terrain, &renderer.Park, &renderer.Guests, &renderer.Ghost,
                         &renderer.Tints, &renderer.Marks, &renderer.Overlay}) {
    releaseKept(renderer.Device, mesh->Vertices);
    releaseKept(renderer.Device, mesh->Indices);
  }
  releaseKept(renderer.Device, renderer.OverlayShape);
  releaseKept(renderer.Device, renderer.OverlayShades);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.TerrainPipeline);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.OverlayPipeline);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.ParkPipeline);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.GhostPipeline);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.MarksPipeline);
  if (renderer.Window != nullptr) {
    SDL_ReleaseWindowFromGPUDevice(renderer.Device, renderer.Window);
  }
  SDL_DestroyGPUDevice(renderer.Device);
  renderer = Renderer{};
}

bool setParkMesh(Renderer &renderer, const ParkMesh &mesh) {
  return uploadMesh(renderer, renderer.Park, mesh);
}

bool setOverlayShape(Renderer &renderer, const OverlayShape &shape) {
  if (!uploadMesh(renderer, renderer.Overlay, shape.Vertices.data(),
                  static_cast<uint32_t>(shape.Vertices.size() * sizeof(OverlayVertex)),
                  shape.Indices)) {
    return false;
  }
  if (shape.Indices.empty()) {
    return true;
  }
  return uploadKept(renderer, renderer.OverlayShape, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                    shapeWordCount(shape) * static_cast<uint32_t>(sizeof(uint32_t)),
                    [&](void *to) { writeShapeWords(shape, static_cast<uint32_t *>(to)); });
}

bool setOverlayShades(Renderer &renderer, const OverlayShades &shades) {
  renderer.OverlayHalf = static_cast<float>(shades.Half);
  return uploadKept(renderer, renderer.OverlayShades, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                    shadeWordCount(shades) * static_cast<uint32_t>(sizeof(uint32_t)),
                    [&](void *to) { writeShadeWords(shades, static_cast<uint32_t *>(to)); });
}

bool setGhostMesh(Renderer &renderer, const GhostMesh &ghost) {
  return uploadMesh(renderer, renderer.Tints, ghost.Tints) &&
         uploadMesh(renderer, renderer.Ghost, ghost.Lit) &&
         uploadMesh(renderer, renderer.Marks, ghost.Marks);
}

bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh) {
  return uploadMesh(renderer, renderer.Guests, mesh);
}

void beginUiFrame() { ImGui_ImplSDLGPU3_NewFrame(); }

namespace {

// Draws a mesh's triangles through a pipeline, or nothing for an empty mesh.
void drawMesh(SDL_GPURenderPass *pass, SDL_GPUGraphicsPipeline *pipeline, const KeptMesh &mesh) {
  if (mesh.IndexCount == 0) {
    return;
  }
  SDL_BindGPUGraphicsPipeline(pass, pipeline);
  const SDL_GPUBufferBinding vertexBinding = {mesh.Vertices.Buffer, 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
  const SDL_GPUBufferBinding indexBinding = {mesh.Indices.Buffer, 0};
  SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  SDL_DrawGPUIndexedPrimitives(pass, mesh.IndexCount, 1, 0, 0, 0);
}

// Draws the paving overlay's pieces, colored from its shape and shades, or nothing without them.
void drawOverlay(SDL_GPURenderPass *pass, const Renderer &renderer) {
  if (renderer.Overlay.IndexCount == 0 || renderer.OverlayShape.Buffer == nullptr ||
      renderer.OverlayShades.Buffer == nullptr) {
    return;
  }
  SDL_GPUBuffer *storage[2] = {renderer.OverlayShape.Buffer, renderer.OverlayShades.Buffer};
  SDL_BindGPUGraphicsPipeline(pass, renderer.OverlayPipeline);
  SDL_BindGPUFragmentStorageBuffers(pass, 0, storage, 2);
  const SDL_GPUBufferBinding vertexBinding = {renderer.Overlay.Vertices.Buffer, 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
  const SDL_GPUBufferBinding indexBinding = {renderer.Overlay.Indices.Buffer, 0};
  SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  SDL_DrawGPUIndexedPrimitives(pass, renderer.Overlay.IndexCount, 1, 0, 0, 0);
}

void copyRgb(Rgb color, float (&to)[4]) {
  to[0] = color.R;
  to[1] = color.G;
  to[2] = color.B;
  to[3] = 1.0f;
}

void copyDirection(Vec3 direction, float (&to)[4]) {
  to[0] = direction.X;
  to[1] = direction.Y;
  to[2] = direction.Z;
  to[3] = 0.0f;
}

// The light's uniforms, with its exposure, giving brightness only when asked.
LightUniforms lightUniforms(const DayLight &light, bool brightnessOnly) {
  LightUniforms uniforms = {};
  copyDirection(light.SunDirection, uniforms.SunDirection);
  copyRgb(light.Sun, uniforms.Sun);
  copyDirection(light.MoonDirection, uniforms.MoonDirection);
  copyRgb(light.Moon, uniforms.Moon);
  copyRgb(light.AmbientSky, uniforms.AmbientSky);
  copyRgb(light.AmbientGround, uniforms.AmbientGround);
  copyRgb(light.Sky, uniforms.Sky);
  uniforms.Settings[0] = brightnessOnly ? 1.0f : 0.0f;
  uniforms.Settings[1] = light.Exposure;
  return uniforms;
}

// Draws the terrain, the park, and the guests into the scene target, lit by the light, keeping the
// depth for the passes after the tonemap.
void drawLitScene(const Renderer &renderer, SDL_GPUCommandBuffer *commands,
                  const CameraView &camera, const DayLight &light) {
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
  const LightUniforms lit = lightUniforms(light, false);
  SDL_PushGPUFragmentUniformData(commands, 0, &lit, sizeof(lit));

  SDL_GPUColorTargetInfo colorTarget = {};
  colorTarget.texture = renderer.SceneTarget;
  colorTarget.clear_color = {light.Sky.R, light.Sky.G, light.Sky.B, 1.0f};
  colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
  colorTarget.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depthTarget = {};
  depthTarget.texture = renderer.DepthTarget;
  // Reversed depth clears to the far plane, 0.
  depthTarget.clear_depth = 0.0f;
  depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
  depthTarget.store_op = SDL_GPU_STOREOP_STORE;
  depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depthTarget.cycle = true;

  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, &depthTarget);
  drawMesh(pass, renderer.TerrainPipeline, renderer.Terrain);
  // The camera and light uniforms pushed before the pass serve these pipelines too. Guests are
  // opaque boxes, drawn as the park's are, but take only the light's brightness.
  drawMesh(pass, renderer.ParkPipeline, renderer.Park);
  const LightUniforms brightness = lightUniforms(light, true);
  SDL_PushGPUFragmentUniformData(commands, 0, &brightness, sizeof(brightness));
  drawMesh(pass, renderer.ParkPipeline, renderer.Guests);
  SDL_EndGPURenderPass(pass);
}

// Tonemaps the scene target into the color target.
void drawTonemap(const Renderer &renderer, SDL_GPUCommandBuffer *commands, const DayLight &light) {
  const LightUniforms lit = lightUniforms(light, false);
  SDL_PushGPUFragmentUniformData(commands, 0, &lit, sizeof(lit));
  SDL_GPUColorTargetInfo colorTarget = {};
  colorTarget.texture = renderer.ColorTarget;
  colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
  colorTarget.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, nullptr);
  SDL_BindGPUGraphicsPipeline(pass, renderer.TonemapPipeline);
  const SDL_GPUTextureSamplerBinding scene = {renderer.SceneTarget, renderer.SceneSampler};
  SDL_BindGPUFragmentSamplers(pass, 0, &scene, 1);
  SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
  SDL_EndGPURenderPass(pass);
}

// Draws the overlay, then the ghost's tints, the park it previews, and its marks, into the color
// target over the tonemapped scene, depth tested against the scene's depth. The light's uniforms
// pushed for the tonemap still serve them.
void drawOverlayAndGhost(const Renderer &renderer, SDL_GPUCommandBuffer *commands) {
  RampUniforms ramp = {};
  for (size_t i = 0; i < OVERLAY_RAMP.size(); ++i) {
    ramp.Colors[i] = OVERLAY_RAMP[i];
  }
  ramp.Middle[0] = renderer.OverlayHalf;
  SDL_PushGPUFragmentUniformData(commands, 1, &ramp, sizeof(ramp));

  SDL_GPUColorTargetInfo colorTarget = {};
  colorTarget.texture = renderer.ColorTarget;
  colorTarget.load_op = SDL_GPU_LOADOP_LOAD;
  colorTarget.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPUDepthStencilTargetInfo depthTarget = {};
  depthTarget.texture = renderer.DepthTarget;
  depthTarget.load_op = SDL_GPU_LOADOP_LOAD;
  depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
  depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

  // The overlay lies just over the guest paving and under its edge lines, boxes, and guests, which
  // the scene's depth holds. Ghosts come after it, so they blend over it.
  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, &depthTarget);
  drawOverlay(pass, renderer);
  drawMesh(pass, renderer.MarksPipeline, renderer.Tints);
  drawMesh(pass, renderer.GhostPipeline, renderer.Ghost);
  drawMesh(pass, renderer.MarksPipeline, renderer.Marks);
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

bool drawFrame(Renderer &renderer, const CameraView &camera, const DayLight &light, ImDrawData *ui,
               const char *capturePath) {
  TPJ_PROFILE_ZONE();
  // The frame's uploads are already recorded on its command buffer. The next upload acquires the
  // next frame's.
  SDL_GPUCommandBuffer *commands = frameCommands(renderer);
  renderer.Commands = nullptr;
  if (commands == nullptr) {
    return false;
  }

  SDL_GPUTexture *swapchain = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, renderer.Window, &swapchain, &width,
                                             &height)) {
    logError("Cannot acquire swapchain texture");
    // Submitted rather than cancelled, so the uploads recorded on it still reach the GPU.
    SDL_SubmitGPUCommandBuffer(commands);
    return false;
  }
  // A hidden window has no swapchain texture, but its frames are still drawn, at its size, for
  // runs that time or capture them without showing a window.
  const bool hidden = (SDL_GetWindowFlags(renderer.Window) & SDL_WINDOW_HIDDEN) != 0;
  if (swapchain == nullptr && hidden) {
    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(renderer.Window, &pixelWidth, &pixelHeight)) {
      logError("Cannot read the window's size");
      SDL_SubmitGPUCommandBuffer(commands);
      return false;
    }
    width = static_cast<uint32_t>(pixelWidth);
    height = static_cast<uint32_t>(pixelHeight);
  }
  // A null swapchain texture on a shown window means it is minimized; skip the frame.
  if ((swapchain == nullptr && !hidden) || !ensureRenderTargets(renderer, width, height)) {
    return SDL_SubmitGPUCommandBuffer(commands) && swapchain == nullptr;
  }

  drawLitScene(renderer, commands, camera, light);
  drawTonemap(renderer, commands, light);
  drawOverlayAndGhost(renderer, commands);
  if (ui != nullptr && renderer.UiInitialized) {
    drawUi(renderer, commands, ui);
  }
  if (swapchain != nullptr) {
    blitToSwapchain(renderer, commands, swapchain);
  }

  if (capturePath != nullptr) {
    return submitWithCapture(renderer, commands, capturePath);
  }
  return SDL_SubmitGPUCommandBuffer(commands);
}

} // namespace tpj
