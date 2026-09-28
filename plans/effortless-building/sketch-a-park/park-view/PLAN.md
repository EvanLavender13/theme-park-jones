# Implementation Plan: Park View

## Goal

Draw the park's intent, flat path ribbons and boxes, through a lit mesh pipeline, start the app from the template or a park file with --park, --ticks, and --hash, and check in tests/parks/sketch.park.

## Approach

src/render/park_mesh.cpp builds a ParkMesh of plain vertices and indices from the park module's public queries, one element at a time, so tests check the geometry without a GPU and box-tools can reuse the same functions for ghosts. The renderer gains a second pipeline with position, normal, and color attributes, uploads the mesh when setParkMesh is called, and draws it after the terrain in the same pass, with the terrain depth-biased away. main.cpp parses the new options, builds the starting world, answers --hash before SDL_Init, and rebuilds the mesh whenever the intent it reads differs from what it last drew.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify the park mesh and its drawing

Files:
- Modify: `src/render/SPEC.md`

Step 1: After the Contract paragraph that begins "createRenderer creates a Vulkan-backed GPU device", insert the paragraph FEATURE.md gives starting "setParkMesh uploads a park mesh".

Step 2: At the end of the file, append the "## Park mesh" section exactly as FEATURE.md gives it.

Run: `grep -c "^## " src/render/SPEC.md`
Expected: `2`

### Task 2: Specify the app's park and options

Files:
- Modify: `src/app/SPEC.md`

Step 1: Between the Main loop section and "## Camera", insert the "## Park" section FEATURE.md gives.

Step 2: Replace the paragraph under "## Command line" with the three paragraphs FEATURE.md gives.

Run: `grep -c "^## " src/app/SPEC.md`
Expected: `5`

### Task 3: Move the translucent pass to box-tools in the milestone

Files:
- Modify: `plans/effortless-building/sketch-a-park/MILESTONE.md`

Step 1: In feature 3's line, replace "boxes, and the entrance from intent, a translucent pass for ghosts and highlights, the app" with "boxes, and the entrance from intent, the app".

Step 2: In feature 4's line, replace "the place, move, and delete tools for boxes, and the delete tool for paths, with ghosts and hover highlights." with "the place, move, and delete tools for boxes, and the delete tool for paths, with ghosts and hover highlights drawn in a translucent pass."

Step 3: In Deepening candidates, append the bullet "- Crossings and junctions drawn cleanly: where ribbons meet or cross they overlap, and ribbons of different kinds tie in depth where they cross."

Run: `grep -c "translucent pass" plans/effortless-building/sketch-a-park/MILESTONE.md`
Expected: `1`

### Task 4: Declare the park mesh

Files:
- Create: `src/render/park_mesh.h`
- Create: `src/render/park_mesh.cpp`
- Modify: `src/render/CMakeLists.txt`

Step 1: Create src/render/park_mesh.h:

```cpp
#ifndef TPJ_RENDER_PARK_MESH_H
#define TPJ_RENDER_PARK_MESH_H

#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// A color with alpha, each channel in [0, 1].
struct Rgba {
  float R = 0.0f;
  float G = 0.0f;
  float B = 0.0f;
  float A = 1.0f;

  bool operator==(const Rgba &) const = default;
};

// Matches the vertex inputs of park.vert.
struct ParkVertex {
  float Position[3] = {};
  float Normal[3] = {};
  Rgba Color;
};

// Triangles as three indices each, wound counter-clockwise seen from the side their normal
// points to.
struct ParkMesh {
  std::vector<ParkVertex> Vertices;
  std::vector<uint32_t> Indices;
};

// A path's ribbon lies this far above the ground.
inline constexpr float PATH_LIFT = 0.02f;
inline constexpr float ENTRANCE_HEIGHT = 5.0f;
inline constexpr Rgba ENTRANCE_COLOR{0.92f, 0.80f, 0.28f, 1.0f};

constexpr float boxHeight(BoxKind kind) { return kind == BoxKind::Shop ? 4.0f : 6.0f; }

constexpr Rgba pathColor(PathKind kind) {
  return kind == PathKind::Guest ? Rgba{0.82f, 0.74f, 0.58f, 1.0f}
                                 : Rgba{0.46f, 0.46f, 0.50f, 1.0f};
}

constexpr Rgba boxColor(BoxKind kind) {
  return kind == BoxKind::Shop ? Rgba{0.84f, 0.42f, 0.30f, 1.0f}
                               : Rgba{0.34f, 0.44f, 0.62f, 1.0f};
}

// The color a box's front face takes: red, green, and blue moved 0.4 of the way to 1.
Rgba lightened(Rgba color);

// Adds the path's flat ribbon along its ground line, or nothing when the line is empty.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points);

// Adds an open-bottomed box of the height over the pose's footprint, or nothing when the pose has
// no footprint.
void appendBox(ParkMesh &mesh, const Pose &pose, FootprintSize size, float height, Rgba color);

// The world's entrances, paths, and boxes, in that order and each in key order.
ParkMesh buildParkMesh(const World &world);

// A rectangle on the ground.
struct GroundBounds {
  float MinX = 0.0f;
  float MinZ = 0.0f;
  float MaxX = 0.0f;
  float MaxZ = 0.0f;
};

// The least rectangle holding every vertex's x and z, or none for a mesh with no vertices.
std::optional<GroundBounds> meshBounds(const ParkMesh &mesh);

} // namespace tpj

#endif
```

Step 2: Create src/render/park_mesh.cpp with stubs:

```cpp
#include "render/park_mesh.h"

namespace tpj {

Rgba lightened(Rgba color) { return color; }

void appendPath(ParkMesh & /*mesh*/, PathKind /*kind*/,
                const std::vector<ParkPoint> & /*points*/) {}

void appendBox(ParkMesh & /*mesh*/, const Pose & /*pose*/, FootprintSize /*size*/,
               float /*height*/, Rgba /*color*/) {}

ParkMesh buildParkMesh(const World & /*world*/) { return {}; }

std::optional<GroundBounds> meshBounds(const ParkMesh & /*mesh*/) { return std::nullopt; }

} // namespace tpj
```

Step 3: In src/render/CMakeLists.txt, add `park_mesh.cpp` after `math.cpp` in add_library, and change the link line to `target_link_libraries(tpj_render PUBLIC tpj_core tpj_sim SDL3::SDL3 tpj_imgui)`.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 5: Declare setParkMesh

Files:
- Modify: `src/render/renderer.h`
- Modify: `src/render/renderer.cpp`

Step 1: In renderer.h, add `#include "render/park_mesh.h"` after `#include "render/math.h"`. In struct Renderer, after `uint32_t TerrainIndexCount = 0;`, add:

```cpp
  SDL_GPUGraphicsPipeline *ParkPipeline = nullptr;
  SDL_GPUBuffer *ParkVertices = nullptr;
  SDL_GPUBuffer *ParkIndices = nullptr;
  uint32_t ParkIndexCount = 0;
```

After the declaration of destroyRenderer, declare:

```cpp
// Uploads the park mesh drawn from now on, replacing the one before. An empty mesh draws
// nothing. Returns false and logs through SDL on failure.
bool setParkMesh(Renderer &renderer, const ParkMesh &mesh);
```

Step 2: In renderer.cpp, after destroyRenderer's definition, add the stub `bool setParkMesh(Renderer & /*renderer*/, const ParkMesh & /*mesh*/) { return true; }`.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 6: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/render/SPEC.md, src/app/SPEC.md, src/sim/park/SPEC.md, and src/scenarios/SPEC.md as specs, and src/render/park_mesh.h, src/render/renderer.h, src/sim/park/intent.h, src/sim/park/geometry.h, and src/sim/park/edits.h as public headers.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `ctest --preset linux-debug`
Expected: the build is clean. The new tests for criteria 1, 2, 4, 6, and 8 fail on the stubs, the missing options, and the missing sketch.park. Those for criteria 3, 5, and 7 may already pass, since empty stubs append nothing and the current app refuses the new options. Every earlier test passes.

### Task 7: Build path ribbons

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Add `#include <math.h>` and an anonymous namespace holding:

- `void addVertex(ParkMesh &mesh, double x, float y, double z, float normalX, float normalY, float normalZ, Rgba color)`, which pushes a ParkVertex with the position cast to float.
- `ParkPoint unitStep(const CarrierPoint &from, const CarrierPoint &to)`, which returns (to - from) divided by sqrt(dx * dx + dz * dz). Ground line points are distinct, so the length is positive.

Step 2: Define appendPath:

1. `const std::vector<CarrierPoint> line = groundLine(points);` and return if it is empty.
2. `const uint32_t first = static_cast<uint32_t>(mesh.Vertices.size());` and `const double half = 0.5 * pathWidth(kind);`.
3. For each i from 0 to line.size() - 1, find the tangent: at i = 0, unitStep(line[0], line[1]); at the last i, unitStep(line[i - 1], line[i]); otherwise the sum s of unitStep(line[i - 1], line[i]) and unitStep(line[i], line[i + 1]), divided by its length when that length is above 0, else unitStep(line[i - 1], line[i]). With right = (-tangent.Z, tangent.X), add the left vertex at (line[i].X - right.X * half, PATH_LIFT, line[i].Z - right.Z * half), then the right vertex at the same point plus right times half, both with normal (0, 1, 0) and color pathColor(kind).
4. For each i from 0 to line.size() - 2, with l0 = first + 2i, r0 = l0 + 1, l1 = l0 + 2, r1 = l0 + 3, push the indices l0, r0, l1 and then l1, r0, r1. Both triangles then face up.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `ctest --preset linux-debug`
Expected: the build is clean, and the test pass's tests of criterion 1 pass.

### Task 8: Build boxes

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Define lightened: return `{c.R + (1 - c.R) * 0.4f, c.G + (1 - c.G) * 0.4f, c.B + (1 - c.B) * 0.4f, c.A}`.

Step 2: In the anonymous namespace, add `void addQuad(ParkMesh &mesh, const std::array<ParkPoint, 4> &ground, const std::array<float, 4> &heights, float normalX, float normalY, float normalZ, Rgba color)`, which adds the four vertices (ground[k].X, heights[k], ground[k].Z) in order with the normal and color, and the indices v, v + 1, v + 2 and v, v + 2, v + 3, where v is the vertex count before it. Its callers give the four vertices counter-clockwise seen from the normal's side.

Step 3: Define appendBox. Get `const std::optional<Footprint> footprint = footprintOf(pose, size);` and return when it is empty. Name the corners FL, FR, BR, BL = Corners[0] to Corners[3], F = Forward, R = Right, and h = height. Then:

- Top: addQuad with ground {FL, BL, BR, FR}, heights {h, h, h, h}, normal (0, 1, 0), and the color.
- Left side: ground {FL, BL, BL, FL}, heights {0, 0, h, h}, normal (-R.X, 0, -R.Z), the color.
- Back side: ground {BL, BR, BR, BL}, heights {0, 0, h, h}, normal (-F.X, 0, -F.Z), the color.
- Right side: ground {BR, FR, FR, BR}, heights {0, 0, h, h}, normal (R.X, 0, R.Z), the color.
- Front side: ground {FR, FL, FL, FR}, heights {0, 0, h, h}, normal (F.X, 0, F.Z), lightened(color).

Each side's first two vertices run along the footprint's edge counter-clockwise seen from above, and its last two rise above them in reverse, so every face is counter-clockwise seen from outside.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `ctest --preset linux-debug`
Expected: the build is clean, and the test pass's tests of criteria 2 and 3 pass.

### Task 9: Build the whole park's mesh

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Define buildParkMesh: start from an empty ParkMesh, then for each entrance in parkEntrances(world) call appendBox(mesh, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT, ENTRANCE_COLOR), for each path in parkPaths(world) call appendPath(mesh, path.Kind, path.Points), and for each box in parkBoxes(world) call appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), boxColor(box.Kind)). Return the mesh.

Step 2: Define meshBounds: none when mesh.Vertices is empty, and otherwise the least and greatest Position[0] as MinX and MaxX and Position[2] as MinZ and MaxZ over every vertex.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `ctest --preset linux-debug`
Expected: the build is clean, and the test pass's tests of criteria 1 to 5 pass.

### Task 10: Add the park shaders

Files:
- Create: `src/render/shaders/park.vert`
- Create: `src/render/shaders/park.frag`
- Modify: `src/render/CMakeLists.txt`

Step 1: Create park.vert:

```glsl
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outViewDistance;

// SDL_GPU binds vertex-stage uniform buffers at set 1. The same block as terrain.vert.
layout(set = 1, binding = 0) uniform Camera {
    mat4 viewProjection;
    vec4 eye;
} camera;

void main() {
    outNormal = inNormal;
    outColor = inColor;
    outViewDistance = distance(inPosition, camera.eye.xyz);
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
}
```

Step 2: Create park.frag, lit and fogged as terrain.frag is:

```glsl
#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

const vec3 SKY = vec3(0.62, 0.76, 0.90);
const vec3 SUN_DIRECTION = vec3(0.4, 1.0, 0.3);

void main() {
    float light = 0.55 + 0.45 * max(dot(normalize(inNormal), normalize(SUN_DIRECTION)), 0.0);
    vec3 color = inColor.rgb * light;
    color = mix(color, SKY, smoothstep(250.0, 700.0, inViewDistance));
    outColor = vec4(color, inColor.a);
}
```

Step 3: In src/render/CMakeLists.txt, set TPJ_SHADER_SOURCES to `park.vert park.frag terrain.vert terrain.frag`, one per line.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ls build/linux-debug/shaders`
Expected: no warnings or errors, and the listing shows park.frag.spv, park.vert.spv, terrain.frag.spv, and terrain.vert.spv.

### Task 11: Create the park pipeline and reverse depth

Files:
- Modify: `src/render/renderer.cpp`

Step 1: Generalize createTerrainPipeline into `SDL_GPUGraphicsPipeline *createPipeline(const Renderer &renderer, const char *vertexFile, const char *fragmentFile, const SDL_GPUVertexAttribute *attributes, uint32_t attributeCount, uint32_t pitch)`, keeping its shader loading, vertex buffer description, and states, with the depth test's compare op SDL_GPU_COMPAREOP_GREATER. In drawScene, clear depth to 0. In src/render/math.cpp, reverse perspective's depth: M[10] = nearZ / (farZ - nearZ) and M[14] = nearZ * farZ / (farZ - nearZ), so the near plane maps to 1 and the far plane to 0, and say so in math.h's comment. (Amended after Evan's zoomed-out check: a terrain depth bias left paths flickering seen from above.) It logs "Cannot create pipeline" with the vertex file's name and returns null on failure.

Step 2: createTerrainPipeline keeps its two attributes and calls createPipeline with "terrain.vert.spv", "terrain.frag.spv", sizeof(TerrainVertex). Add createParkPipeline with three attributes at locations 0, 1, and 2: FLOAT3 at offsetof(ParkVertex, Position), FLOAT3 at offsetof(ParkVertex, Normal), and FLOAT4 at offsetof(ParkVertex, Color), calling createPipeline with "park.vert.spv", "park.frag.spv", sizeof(ParkVertex). Add `static_assert(sizeof(Rgba) == 4 * sizeof(float));` beside it.

Step 3: In createRenderer, create the park pipeline after the terrain pipeline, returning false when it fails. In destroyRenderer, release ParkVertices, ParkIndices, and ParkPipeline beside the terrain's.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 12: Upload and draw the park mesh

Files:
- Modify: `src/render/renderer.cpp`

Step 1: Generalize createTerrainMesh's upload into `bool uploadMesh(SDL_GPUDevice *device, const void *vertices, uint32_t vertexBytes, const void *indices, uint32_t indexBytes, SDL_GPUBuffer *&vertexBuffer, SDL_GPUBuffer *&indexBuffer)`: create both buffers and one upload transfer buffer of vertexBytes + indexBytes, copy the vertices then the indices into it, upload both in one copy pass, submit, and release the transfer buffer. It logs and returns false on any failure. createTerrainMesh writes the terrain geometry into two local vectors and calls it.

Step 2: Define setParkMesh: release ParkVertices and ParkIndices (SDL_GPU defers the release until the GPU is done with them), set them to null and ParkIndexCount to 0, and return true when mesh.Indices is empty. Otherwise call uploadMesh with the mesh's data and byte sizes and, on success, set ParkIndexCount to mesh.Indices.size().

Step 3: In drawScene, after the terrain's draw call and before SDL_EndGPURenderPass, when ParkIndexCount is above 0, bind ParkPipeline, bind ParkVertices at slot 0 and ParkIndices as 32-bit indices, and draw ParkIndexCount indices. The camera uniforms pushed before the pass serve both pipelines.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `build/linux-debug/ThemeParkJones --capture build/linux-debug/terrain.bmp`
Expected: no warnings or errors, and the capture shows the terrain as before, since the app draws no park yet.

### Task 13: Parse --park, --ticks, and --hash

Files:
- Modify: `src/app/main.cpp`

Step 1: Add to Options `const char *ParkPath = nullptr;`, `uint64_t Ticks = 0;`, and `bool PrintHash = false;`. In parseOptions, accept `--park PATH`, `--ticks N`, parsed with std::from_chars over the whole value and refused when empty, not decimal, or not wholly consumed, and `--hash`. Add `bool FramesGiven = false;` to Options, set when --frames is parsed. After the loop, refuse --hash when FramesGiven is true or CapturePath is set, whatever their values. Every refusal logs the usage `Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH]` and returns false. Update the comment above parseOptions to name the new options.

Step 2: Add `std::optional<tpj::World> startingWorld(const Options &options)`: when ParkPath is null, `tpj::makeNewPark(1)`. Otherwise read the file with SDL_LoadFile, writing "Cannot read <path>: <SDL error>" to standard error and returning none on failure, then `tpj::loadWorld(tpj::makeParkSchema(), text)`, catching tpj::LoadError to write "Cannot load <path>: <what>" to standard error and return none, and SDL_free the text. Then call `tpj::resolveWorld(world)` and `tpj::stepWorld(world)` Ticks times, and return it.

Step 3: In main, after parseOptions and before SDL_Init, call startingWorld and return EXIT_FAILURE when it gives none. When PrintHash is set, write `tick <t> hash <h>` with `printf("tick %llu hash %016llx\n", ...)`, casting Tick and hashWorld's value to unsigned long long, and return EXIT_SUCCESS. Otherwise pass the world into runLoop by reference, replacing the default-constructed world there.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `build/linux-debug/ThemeParkJones --park tests/parks/new.park --ticks 5 --hash; build/linux-debug/tpj_scenarios --ticks 5 tests/parks/new.park 2>/dev/null | grep "^file" | tail -1`
Expected: no warnings or errors, and the two lines' tick and hash agree: `tick 5 hash <h>` and `file tests/parks/new.park tick 5 hash <h>`. The test pass's tests of criteria 6 and 7 pass under `ctest --preset linux-debug`.

### Task 14: Draw the world's intent in the app

Files:
- Modify: `src/app/main.cpp`
- Modify: `src/app/orbit_camera.h`
- Modify: `src/app/orbit_camera.cpp`

Step 1: Include "render/park_mesh.h" and "sim/park/intent.h". Add a struct in the anonymous namespace:

```cpp
// The intent a park mesh was built from, so the mesh is rebuilt only when intent changes.
struct DrawnIntent {
  std::vector<tpj::ParkEntrance> Entrances;
  std::vector<tpj::ParkPath> Paths;
  std::vector<tpj::ParkBox> Boxes;

  bool operator==(const DrawnIntent &) const = default;
};
```

Step 2: In src/app/orbit_camera.h, include "render/park_mesh.h" and declare, after orbitCameraEye:

```cpp
// Centers the focus on the bounds and sets the distance at which a sphere around them fits the
// vertical field of view, within the camera's distance limits.
void frameOrbitCamera(OrbitCamera &camera, const GroundBounds &bounds, float fovY);
```

In orbit_camera.cpp, define it: Focus becomes ((MinX + MaxX) / 2, 0, (MinZ + MaxZ) / 2), and Distance becomes clampf(radius / sinf(0.5f * fovY), MIN_DISTANCE, MAX_DISTANCE), where radius is half of sqrtf(width * width + depth * depth) of the bounds.

Step 3: In runLoop, hold `std::optional<DrawnIntent> drawn;`. Each frame, after the simulation's ticks, build `DrawnIntent current{tpj::parkEntrances(world), tpj::parkPaths(world), tpj::parkBoxes(world)}`. When drawn is empty or differs from current, build `const tpj::ParkMesh mesh = tpj::buildParkMesh(world);`. If drawn was empty and `tpj::meshBounds(mesh)` gives bounds, call `tpj::frameOrbitCamera(camera, *bounds, tpj::CameraView{}.FovY)`. Then call `tpj::setParkMesh(renderer, mesh)`, returning false when it fails, and set drawn to current. Do this before the camera's update for the frame, so the first frame is already framed.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` then `build/linux-debug/ThemeParkJones --capture build/linux-debug/new.bmp`
Expected: no warnings or errors, and the capture frames the template's entrance and its short guest path near the middle of the view.

### Task 15: Check in the sketch park

Files:
- Create: `tests/parks/sketch.park`

Step 1: Create tests/parks/sketch.park with exactly this text, ending in a line feed:

```
tpj-park 1
seed 1
tick 0
next-key 7

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path]
2 kind=guest points=[{x=0 z=123} {x=0 z=100} {x=-20 z=80} {x=-20 z=50} {x=10 z=30}]
3 kind=backstage points=[{x=75 z=-20} {x=75 z=10} {x=45 z=25}]

[box]
4 kind=shop x=-35 z=60 facing-x=1 facing-z=0
5 kind=shop x=25 z=40 facing-x=-1 facing-z=0
6 kind=depot x=75 z=-30 facing-x=0 facing-z=1
```

The shops face the guest path, the depot faces the backstage path's start 6 m in front of it, and every box stands well clear of every path.

Run: `ctest --preset linux-debug` then `build/linux-debug/ThemeParkJones --park tests/parks/sketch.park --capture build/linux-debug/sketch.bmp`
Expected: the test pass's tests of criterion 8 pass. Read the capture: the whole park is in frame, the paths lie flat on the ground, curving smoothly, the boxes and the entrance stand as boxes with lighter fronts, guest and backstage paths and shops and the depot differ in color, and nothing flickers through the grass (criterion 9). The far view is not scriptable, so when reporting the feature, ask Evan to zoom out to 400 m in the Windows build and look for flicker.

### Task 16: Verify both builds and the cross-build check

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`, then `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, `ctest --preset linux-debug`, `cmake.exe --build --preset windows-debug`, `ctest.exe --preset windows-debug`, and `scripts/cross-build-check.sh`.
Expected: no warnings or errors, every test passes on both builds, and the cross-build check passes over new.park and sketch.park.

### Task 17: Commit

Stage everything with `git add -A` and commit once via the commit-hygiene skill, with the subject "Render: Draw the park's paths and boxes from intent".
