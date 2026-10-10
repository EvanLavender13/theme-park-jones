#include "render/renderer.h"

#include "core/profile.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdlgpu3.h>

#include <bit>
#include <stddef.h>
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

// Matches the std140 `Ramp` block in overlay.frag: the ramp's colors, the color for none, and the
// value that shades halfway up the ramp, in Middle's first float.
struct RampUniforms {
  Rgba Colors[OVERLAY_RAMP.size()];
  Rgba Zero;
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

// Creates a storage buffer the graphics shaders read and uploads the words into it.
bool uploadStorage(SDL_GPUDevice *device, const std::vector<uint32_t> &words,
                   SDL_GPUBuffer *&buffer) {
  const auto bytes = static_cast<uint32_t>(words.size() * sizeof(uint32_t));
  buffer = createBuffer(device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, bytes);
  SDL_GPUTransferBufferCreateInfo transferInfo = {};
  transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transferInfo.size = bytes;
  SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
  if (buffer == nullptr || transfer == nullptr) {
    logError("Cannot create storage buffer");
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (mapped == nullptr) {
    logError("Cannot map storage transfer buffer");
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  SDL_memcpy(mapped, words.data(), bytes);
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
  destination.buffer = buffer;
  destination.size = bytes;
  SDL_UploadToGPUBuffer(copy, &source, &destination, false);
  SDL_EndGPUCopyPass(copy);
  const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  if (!submitted) {
    logError("Cannot submit storage upload");
  }
  return submitted;
}

uint32_t wordOf(double value) { return std::bit_cast<uint32_t>(static_cast<float>(value)); }

// The shape as overlay.frag reads it: five offsets, to the pieces, the line references, the
// lines, the points, and the overlaps, then each of them in turn.
std::vector<uint32_t> shapeWords(const OverlayShape &shape) {
  std::vector<uint32_t> words(5, 0);
  words[0] = static_cast<uint32_t>(words.size());
  for (const OverlayPiece &piece : shape.Pieces) {
    for (const ParkPoint &corner : piece.Corners) {
      words.push_back(wordOf(corner.X));
      words.push_back(wordOf(corner.Z));
    }
    words.insert(words.end(), {piece.Count, piece.FirstLine, piece.LineCount, piece.FirstOverlap,
                               piece.OverlapCount});
  }
  words[1] = static_cast<uint32_t>(words.size());
  words.insert(words.end(), shape.LineRefs.begin(), shape.LineRefs.end());
  words[2] = static_cast<uint32_t>(words.size());
  for (const OverlayLine &line : shape.Lines) {
    words.insert(words.end(), {line.FirstPoint, line.PointCount});
  }
  words[3] = static_cast<uint32_t>(words.size());
  for (const CarrierPoint &point : shape.Points) {
    words.insert(words.end(), {wordOf(point.X), wordOf(point.Z), wordOf(point.Distance)});
  }
  words[4] = static_cast<uint32_t>(words.size());
  words.insert(words.end(), shape.Overlaps.begin(), shape.Overlaps.end());
  return words;
}

// The shades as overlay.frag reads them: the offset to the knots, where each line's knots begin
// and where the last ends, then each knot's distance and value.
std::vector<uint32_t> shadeWords(const OverlayShades &shades) {
  std::vector<uint32_t> words;
  words.reserve(1 + shades.FirstKnots.size() + 2 * shades.Knots.size());
  words.push_back(static_cast<uint32_t>(1 + shades.FirstKnots.size()));
  words.insert(words.end(), shades.FirstKnots.begin(), shades.FirstKnots.end());
  for (const OverlayKnot &knot : shades.Knots) {
    words.push_back(wordOf(knot.Distance));
    words.push_back(wordOf(knot.Value));
  }
  return words;
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
  SDL_WaitForGPUIdle(renderer.Device);
  if (renderer.UiInitialized) {
    ImGui_ImplSDLGPU3_Shutdown();
  }
  SDL_ReleaseGPUTexture(renderer.Device, renderer.SceneTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.ColorTarget);
  SDL_ReleaseGPUTexture(renderer.Device, renderer.DepthTarget);
  SDL_ReleaseGPUSampler(renderer.Device, renderer.SceneSampler);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.TonemapPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.TerrainVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.TerrainIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.TerrainPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayIndices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayShape);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayShades);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.OverlayPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.ParkVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.ParkIndices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GuestVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GuestIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.ParkPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GhostVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GhostIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.GhostPipeline);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.TintVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.TintIndices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.MarkVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.MarkIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.MarksPipeline);
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

bool setOverlayShape(Renderer &renderer, const OverlayShape &shape) {
  // SDL_GPU defers the release until the GPU no longer uses the buffers.
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayIndices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayShape);
  renderer.OverlayVertices = nullptr;
  renderer.OverlayIndices = nullptr;
  renderer.OverlayShape = nullptr;
  renderer.OverlayIndexCount = 0;
  if (shape.Indices.empty()) {
    return true;
  }
  const auto vertexBytes = static_cast<uint32_t>(shape.Vertices.size() * sizeof(OverlayVertex));
  const auto indexBytes = static_cast<uint32_t>(shape.Indices.size() * sizeof(uint32_t));
  if (!uploadMesh(renderer.Device, shape.Vertices.data(), vertexBytes, shape.Indices.data(),
                  indexBytes, renderer.OverlayVertices, renderer.OverlayIndices) ||
      !uploadStorage(renderer.Device, shapeWords(shape), renderer.OverlayShape)) {
    return false;
  }
  renderer.OverlayIndexCount = static_cast<uint32_t>(shape.Indices.size());
  return true;
}

bool setOverlayShades(Renderer &renderer, const OverlayShades &shades) {
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayShades);
  renderer.OverlayShades = nullptr;
  renderer.OverlayHalf = static_cast<float>(shades.Half);
  return uploadStorage(renderer.Device, shadeWords(shades), renderer.OverlayShades);
}

bool setGhostMesh(Renderer &renderer, const GhostMesh &ghost) {
  return replaceMesh(renderer, ghost.Tints, renderer.TintVertices, renderer.TintIndices,
                     renderer.TintIndexCount) &&
         replaceMesh(renderer, ghost.Lit, renderer.GhostVertices, renderer.GhostIndices,
                     renderer.GhostIndexCount) &&
         replaceMesh(renderer, ghost.Marks, renderer.MarkVertices, renderer.MarkIndices,
                     renderer.MarkIndexCount);
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

// Draws the paving overlay's pieces, colored from its shape and shades, or nothing without them.
void drawOverlay(SDL_GPURenderPass *pass, const Renderer &renderer) {
  if (renderer.OverlayIndexCount == 0 || renderer.OverlayShape == nullptr ||
      renderer.OverlayShades == nullptr) {
    return;
  }
  SDL_GPUBuffer *storage[2] = {renderer.OverlayShape, renderer.OverlayShades};
  SDL_BindGPUGraphicsPipeline(pass, renderer.OverlayPipeline);
  SDL_BindGPUFragmentStorageBuffers(pass, 0, storage, 2);
  const SDL_GPUBufferBinding vertexBinding = {renderer.OverlayVertices, 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
  const SDL_GPUBufferBinding indexBinding = {renderer.OverlayIndices, 0};
  SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  SDL_DrawGPUIndexedPrimitives(pass, renderer.OverlayIndexCount, 1, 0, 0, 0);
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
  drawMesh(pass, renderer.TerrainPipeline, renderer.TerrainVertices, renderer.TerrainIndices,
           renderer.TerrainIndexCount);
  // The camera and light uniforms pushed before the pass serve these pipelines too. Guests are
  // opaque boxes, drawn as the park's are, but take only the light's brightness.
  drawMesh(pass, renderer.ParkPipeline, renderer.ParkVertices, renderer.ParkIndices,
           renderer.ParkIndexCount);
  const LightUniforms brightness = lightUniforms(light, true);
  SDL_PushGPUFragmentUniformData(commands, 0, &brightness, sizeof(brightness));
  drawMesh(pass, renderer.ParkPipeline, renderer.GuestVertices, renderer.GuestIndices,
           renderer.GuestIndexCount);
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
  ramp.Zero = OVERLAY_ZERO_COLOR;
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
  drawMesh(pass, renderer.MarksPipeline, renderer.TintVertices, renderer.TintIndices,
           renderer.TintIndexCount);
  drawMesh(pass, renderer.GhostPipeline, renderer.GhostVertices, renderer.GhostIndices,
           renderer.GhostIndexCount);
  drawMesh(pass, renderer.MarksPipeline, renderer.MarkVertices, renderer.MarkIndices,
           renderer.MarkIndexCount);
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

  drawLitScene(renderer, commands, camera, light);
  drawTonemap(renderer, commands, light);
  drawOverlayAndGhost(renderer, commands);
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
