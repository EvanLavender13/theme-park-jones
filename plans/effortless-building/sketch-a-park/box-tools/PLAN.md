# Implementation Plan: Box Tools

## Goal

Let the player place, move, and delete boxes and delete paths through tools in a new library, with each edit's ghost drawn in a translucent pass before the release commits it.

## Approach

sim/park gains ParkEdit, a variant of its five commands, with isAccepted and queueEdit over it. src/tools holds ToolState and plain functions of the pointer's ground position, presses, releases, and the world, so tests drive them without a window. tpj_render builds ghosts with appendBox and appendPath in ghost colors, draws them through a second pipeline on the park's shaders that blends and tests greater-or-equal without writing depth, and turns the cursor into a ground position. main.cpp wires the Tools panel, the left button, the cursor, and a CommandQueue into the loop.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify ParkEdit

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: At the end of the "## Commands" section, after the paragraph ending "the world it describes is physically valid.", add the paragraph FEATURE.md gives for src/sim/park/SPEC.md, starting "Commands compare equal when their fields do."

Run: `grep -c "queueEdit" src/sim/park/SPEC.md`
Expected: `1`

### Task 2: Specify the tools

Files:
- Create: `src/tools/SPEC.md`

Step 1: Create src/tools/SPEC.md with exactly the content FEATURE.md gives for it.

Run: `grep -c "^## " src/tools/SPEC.md`
Expected: `3`

### Task 3: Specify ghosts, their pass, and cursor picking

Files:
- Modify: `src/render/SPEC.md`

Step 1: After the Contract paragraph beginning "setParkMesh uploads a park mesh", insert the two paragraphs FEATURE.md gives, starting "setGhostMesh uploads" and "groundAtCursor, in picking.h".

Step 2: At the end of the file, append the "## Ghosts" section FEATURE.md gives.

Run: `grep -c "^## " src/render/SPEC.md`
Expected: `3`

### Task 4: Specify the app's tools

Files:
- Modify: `src/app/SPEC.md`

Step 1: Between the "## Park" section and "## Camera", insert the "## Tools" section FEATURE.md gives.

Run: `grep -c "^## " src/app/SPEC.md`
Expected: `6`

### Task 5: Record the milestone's new candidate and note

Files:
- Modify: `plans/effortless-building/sketch-a-park/MILESTONE.md`

Step 1: In Deepening candidates, after the bullet beginning "- Keyboard shortcuts for tools", add:

```markdown
- Turning a placed box, in place or while moving it. box-tools' move keeps a box's facing, so turning one means deleting and placing it again.
```

Step 2: In Research notes, after the bullet beginning "- Tool logic lives in a library", add:

```markdown
- A highlight lies exactly on what it marks, so the translucent pass tests depth greater-or-equal, and park.vert's position is invariant so both pipelines agree on depth.
```

Run: `grep -c "Turning a placed box\|greater-or-equal" plans/effortless-building/sketch-a-park/MILESTONE.md`
Expected: `2`

### Task 6: Declare ParkEdit

Files:
- Modify: `src/sim/park/edits.h`
- Modify: `src/sim/park/edits.cpp`

Step 1: In src/sim/park/edits.h, add `#include "sim/command_queue.h"` after `#include "sim/entity_key.h"`, and `#include <variant>` after `#include <vector>`.

Step 2: In each of AddPath, AddBox, MoveBox, DeletePath, and DeleteBox, add as its last member:

```cpp
  bool operator==(const AddPath &) const = default;
```

with the struct's own name in place of AddPath.

Step 3: After the declaration of addParkEdits, add:

```cpp
// A tentative or committed edit to park intent: one of the five commands.
using ParkEdit = std::variant<AddPath, AddBox, MoveBox, DeletePath, DeleteBox>;
```

Step 4: After the five isAccepted declarations, add:

```cpp
// Whether the command the edit holds would be applied to the world.
bool isAccepted(const World &world, const ParkEdit &edit);

// Pushes the command the edit holds, as its own type, so the cycle applies it as if pushed
// directly.
void queueEdit(CommandQueue &queue, const ParkEdit &edit);
```

Step 5: At the end of namespace tpj in src/sim/park/edits.cpp, add stubs:

```cpp
bool isAccepted(const World & /*world*/, const ParkEdit & /*edit*/) { return false; }

void queueEdit(CommandQueue & /*queue*/, const ParkEdit & /*edit*/) {}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 7: Declare the tools library

Files:
- Create: `src/tools/tools.h`
- Create: `src/tools/tools.cpp`
- Create: `src/tools/CMakeLists.txt`
- Modify: `CMakeLists.txt`

Step 1: Create src/tools/tools.h:

```cpp
#ifndef TPJ_TOOLS_TOOLS_H
#define TPJ_TOOLS_TOOLS_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>

namespace tpj {

enum class ToolKind : uint8_t { None, PlaceShop, PlaceDepot, MoveBox, Delete };

// A place drag nearer than this to where the box lands keeps the facing it had, in meters.
inline constexpr double MIN_FACING_DRAG = 1.0;

// One tool and what it holds. Change it only through selectTool, movePointer, pressPointer, and
// releasePointer. See tools/SPEC.md.
struct ToolState {
  ToolKind Kind = ToolKind::None;
  // Where the pointer meets the ground, if it does.
  std::optional<ParkPoint> Pointer;
  bool Holding = false;
  // The place tools' facing for the next box.
  double FacingX = 0.0;
  double FacingZ = -1.0;
  // Holding with a place tool: where the box lands and how it faces.
  Pose Landing;
  // Holding with MoveBox: the box, its pose at the press, the offset of its position from the
  // pointer then, and where it would move.
  EntityKey Held = NULL_KEY;
  Pose HeldFrom;
  ParkPoint GrabOffset;
  Pose Target;
};

// Sets the tool, dropping any hold without committing.
void selectTool(ToolState &tool, ToolKind kind);
// Where the pointer meets the ground, or none off the ground or over a panel.
void movePointer(ToolState &tool, std::optional<ParkPoint> ground);
// The primary button went down.
void pressPointer(ToolState &tool, const World &world);
// The primary button went up: the edit to commit, which is the tentative edit of the moment
// before when the press took hold.
std::optional<ParkEdit> releasePointer(ToolState &tool, const World &world);

// The edit the ghost shows.
std::optional<ParkEdit> tentativeEdit(const ToolState &tool, const World &world);
// The entity the tool marks.
std::optional<EntityKey> highlightedEntity(const ToolState &tool, const World &world);

// The first box in key order whose footprint holds the point.
std::optional<EntityKey> boxAt(const World &world, ParkPoint point);
// The first path in key order whose ground line passes within half its width of the point.
std::optional<EntityKey> pathAt(const World &world, ParkPoint point);

} // namespace tpj

#endif
```

Step 2: Create src/tools/tools.cpp with stubs:

```cpp
#include "tools/tools.h"

namespace tpj {

void selectTool(ToolState &tool, ToolKind kind) { tool.Kind = kind; }

void movePointer(ToolState &tool, std::optional<ParkPoint> ground) { tool.Pointer = ground; }

void pressPointer(ToolState & /*tool*/, const World & /*world*/) {}

std::optional<ParkEdit> releasePointer(ToolState & /*tool*/, const World & /*world*/) {
  return std::nullopt;
}

std::optional<ParkEdit> tentativeEdit(const ToolState & /*tool*/, const World & /*world*/) {
  return std::nullopt;
}

std::optional<EntityKey> highlightedEntity(const ToolState & /*tool*/, const World & /*world*/) {
  return std::nullopt;
}

std::optional<EntityKey> boxAt(const World & /*world*/, ParkPoint /*point*/) {
  return std::nullopt;
}

std::optional<EntityKey> pathAt(const World & /*world*/, ParkPoint /*point*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 3: Create src/tools/CMakeLists.txt:

```cmake
# The park's editing tools. They link the simulation alone, so tests drive them without a window
# (principle 10).
add_library(tpj_tools STATIC
    tools.cpp)
target_include_directories(tpj_tools PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_tools PUBLIC tpj_sim)
tpj_configure_target(tpj_tools)
```

Step 4: In the root CMakeLists.txt, after `add_subdirectory(src/scenarios)`, add `add_subdirectory(src/tools)`.

Run: `cmake --preset linux-debug > /dev/null && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 8: Declare ghosts and cursor picking in the renderer

Files:
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`
- Create: `src/render/picking.h`
- Create: `src/render/picking.cpp`
- Modify: `src/render/renderer.h`
- Modify: `src/render/renderer.cpp`
- Modify: `src/render/CMakeLists.txt`

Step 1: In src/render/park_mesh.h, add `#include "sim/entity_key.h"` and `#include "sim/park/edits.h"` to the project includes in sorted order. After the declaration of `lightened`, add:

```cpp
// Ghosts and highlights are translucent.
inline constexpr float GHOST_ALPHA = 0.5f;
inline constexpr Rgba INVALID_TINT{0.95f, 0.15f, 0.15f, 0.5f};
inline constexpr Rgba DELETE_TINT{1.0f, 0.55f, 0.1f, 0.6f};
inline constexpr Rgba HIGHLIGHT_TINT{1.0f, 1.0f, 1.0f, 0.35f};
```

After the declaration of `appendPath`, add:

```cpp
// The same ribbon in the color given.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points, Rgba color);
```

After the declaration of `buildParkMesh`, add:

```cpp
// Adds the box or path the key holds as buildParkMesh draws it, in the color. Nothing when the key
// holds neither.
void appendEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color);

// The ghost of an edit on a world: translucent in its kind's color when accepted, INVALID_TINT when
// not, and DELETE_TINT for a deletion. See render/SPEC.md.
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit);
```

Step 2: At the end of namespace tpj in src/render/park_mesh.cpp, add stubs:

```cpp
void appendPath(ParkMesh & /*mesh*/, PathKind /*kind*/, const std::vector<ParkPoint> & /*points*/,
                Rgba /*color*/) {}

void appendEntity(ParkMesh & /*mesh*/, const World & /*world*/, EntityKey /*key*/,
                  Rgba /*color*/) {}

ParkMesh buildGhostMesh(const World & /*world*/, const ParkEdit & /*edit*/) { return {}; }
```

Step 3: Create src/render/picking.h:

```cpp
#ifndef TPJ_RENDER_PICKING_H
#define TPJ_RENDER_PICKING_H

#include "render/renderer.h"
#include "sim/park/intent.h"

#include <optional>

namespace tpj {

// The point on the ground under a cursor at normalized device coordinates, x from -1 at the left
// to 1 at the right and y from -1 at the bottom to 1 at the top. None when the eye is not above the
// ground or the ray through the cursor does not descend.
std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY);

} // namespace tpj

#endif
```

Step 4: Create src/render/picking.cpp with a stub:

```cpp
#include "render/picking.h"

namespace tpj {

std::optional<ParkPoint> groundAtCursor(const CameraView & /*view*/, float /*aspect*/,
                                        float /*ndcX*/, float /*ndcY*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 5: In src/render/CMakeLists.txt, add `picking.cpp` to tpj_render's sources after `park_mesh.cpp`.

Step 6: In src/render/renderer.h, add to Renderer after `uint32_t ParkIndexCount = 0;`:

```cpp
  SDL_GPUGraphicsPipeline *GhostPipeline = nullptr;
  SDL_GPUBuffer *GhostVertices = nullptr;
  SDL_GPUBuffer *GhostIndices = nullptr;
  uint32_t GhostIndexCount = 0;
```

and after the declaration of setParkMesh:

```cpp
// Uploads the translucent mesh of ghosts and highlights drawn from now on, replacing the one
// before. An empty mesh draws nothing. Returns false and logs through SDL on failure.
bool setGhostMesh(Renderer &renderer, const ParkMesh &mesh);
```

Step 7: In src/render/renderer.cpp, after setParkMesh, add a stub:

```cpp
bool setGhostMesh(Renderer & /*renderer*/, const ParkMesh & /*mesh*/) { return true; }
```

Run: `cmake --preset linux-debug > /dev/null && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 9: Run the test pass

Dispatch the test-writer agent with plans/effortless-building/sketch-a-park/box-tools/FEATURE.md, src/sim/park/SPEC.md, src/tools/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, and docs/principles.md. It writes tests under tests/ against the public headers only, with tests/tools/ building tpj_tools_tests linking tpj_tools alone, outside the TPJ_BUILD_APP guard in tests/CMakeLists.txt.

Run: `cmake --preset linux-debug > /dev/null && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: the build gives no output, the new tests of criteria 1, 2, 4, 5, 7, 8, and 9 fail against the stubs, and every earlier test passes. Tests of criteria 3 and 6 may pass trivially while every edit and pick is none, and may fail between Task 11 and Task 12, once picking works and the tools do not.

### Task 10: Implement ParkEdit

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Replace the two stubs with:

```cpp
bool isAccepted(const World &world, const ParkEdit &edit) {
  return std::visit([&world](const auto &command) { return isAccepted(world, command); }, edit);
}

void queueEdit(CommandQueue &queue, const ParkEdit &edit) {
  std::visit([&queue](const auto &command) { queue.push(command); }, edit);
}
```

Step 2: Add `#include <variant>` to the system includes if it is not there.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: the build gives no output, and the tests of criterion 1 pass.

### Task 11: Implement picking

Files:
- Modify: `src/tools/tools.cpp`

Step 1: Add `#include "sim/park/geometry.h"` after the header's own include, and the system includes `<algorithm>`, `<math.h>`, and `<vector>`.

Step 2: Before the first function in namespace tpj, add:

```cpp
namespace {

// The distance from a point to the segment between two distinct ground line points.
double segmentDistance(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double px = point.X - from.X;
  const double pz = point.Z - from.Z;
  const double t = std::clamp((px * dx + pz * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  const double ex = px - dx * t;
  const double ez = pz - dz * t;
  return sqrt(ex * ex + ez * ez);
}

} // namespace
```

Step 3: Replace the boxAt and pathAt stubs with:

```cpp
std::optional<EntityKey> boxAt(const World &world, ParkPoint point) {
  for (const ParkBox &box : parkBoxes(world)) {
    const FootprintSize size = boxSize(box.Kind);
    const std::optional<Footprint> footprint = footprintOf(box.At, size);
    if (!footprint) {
      continue;
    }
    const double dx = point.X - box.At.X;
    const double dz = point.Z - box.At.Z;
    const double along = dx * footprint->Forward.X + dz * footprint->Forward.Z;
    const double across = dx * footprint->Right.X + dz * footprint->Right.Z;
    if (fabs(along) <= 0.5 * size.Depth && fabs(across) <= 0.5 * size.Width) {
      return box.Key;
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> pathAt(const World &world, ParkPoint point) {
  for (const ParkPath &path : parkPaths(world)) {
    const std::vector<CarrierPoint> line = groundLine(path.Points);
    const double half = 0.5 * pathWidth(path.Kind);
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      if (segmentDistance(point, line[i], line[i + 1]) <= half) {
        return path.Key;
      }
    }
  }
  return std::nullopt;
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: the build gives no output, and the tests of criterion 2 pass. Tests of criteria 3 and 6 may fail until Task 12.

### Task 12: Implement the tools

Files:
- Modify: `src/tools/tools.cpp`

Step 1: In the anonymous namespace, after segmentDistance, add:

```cpp
bool isPlaceTool(ToolKind kind) { return kind == ToolKind::PlaceShop || kind == ToolKind::PlaceDepot; }

BoxKind placedKind(ToolKind kind) {
  return kind == ToolKind::PlaceShop ? BoxKind::Shop : BoxKind::Depot;
}

// The pose the box the key holds has, if it holds one.
std::optional<Pose> boxPose(const World &world, EntityKey key) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key) {
      return box.At;
    }
  }
  return std::nullopt;
}
```

Step 2: Replace the selectTool, movePointer, pressPointer, releasePointer, tentativeEdit, and highlightedEntity stubs with:

```cpp
void selectTool(ToolState &tool, ToolKind kind) {
  tool.Kind = kind;
  tool.Holding = false;
  tool.Held = NULL_KEY;
}

void movePointer(ToolState &tool, std::optional<ParkPoint> ground) {
  tool.Pointer = ground;
  if (!tool.Holding || !ground) {
    return;
  }
  if (isPlaceTool(tool.Kind)) {
    const double dx = ground->X - tool.Landing.X;
    const double dz = ground->Z - tool.Landing.Z;
    if (dx * dx + dz * dz >= MIN_FACING_DRAG * MIN_FACING_DRAG) {
      tool.Landing.FacingX = dx;
      tool.Landing.FacingZ = dz;
    }
  } else if (tool.Kind == ToolKind::MoveBox) {
    tool.Target.X = ground->X + tool.GrabOffset.X;
    tool.Target.Z = ground->Z + tool.GrabOffset.Z;
  }
}

void pressPointer(ToolState &tool, const World &world) {
  if (tool.Holding) {
    return;
  }
  if (tool.Kind == ToolKind::Delete) {
    tool.Holding = true;
  } else if (isPlaceTool(tool.Kind) && tool.Pointer) {
    tool.Holding = true;
    tool.Landing = Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ};
  } else if (tool.Kind == ToolKind::MoveBox && tool.Pointer) {
    const std::optional<EntityKey> key = boxAt(world, *tool.Pointer);
    const std::optional<Pose> pose = key ? boxPose(world, *key) : std::nullopt;
    if (pose) {
      tool.Holding = true;
      tool.Held = *key;
      tool.HeldFrom = *pose;
      tool.Target = *pose;
      tool.GrabOffset = ParkPoint{pose->X - tool.Pointer->X, pose->Z - tool.Pointer->Z};
    }
  }
}

std::optional<ParkEdit> releasePointer(ToolState &tool, const World &world) {
  if (!tool.Holding) {
    return std::nullopt;
  }
  std::optional<ParkEdit> edit = tentativeEdit(tool, world);
  if (isPlaceTool(tool.Kind)) {
    tool.FacingX = tool.Landing.FacingX;
    tool.FacingZ = tool.Landing.FacingZ;
  }
  tool.Holding = false;
  tool.Held = NULL_KEY;
  return edit;
}

std::optional<ParkEdit> tentativeEdit(const ToolState &tool, const World &world) {
  if (isPlaceTool(tool.Kind)) {
    if (tool.Holding) {
      return AddBox{placedKind(tool.Kind), tool.Landing};
    }
    if (tool.Pointer) {
      return AddBox{placedKind(tool.Kind),
                    Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ}};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::MoveBox) {
    if (tool.Holding && !(tool.Target == tool.HeldFrom)) {
      return MoveBox{tool.Held, tool.Target};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::Delete && tool.Pointer) {
    if (const std::optional<EntityKey> box = boxAt(world, *tool.Pointer)) {
      return DeleteBox{*box};
    }
    if (const std::optional<EntityKey> path = pathAt(world, *tool.Pointer)) {
      return DeletePath{*path};
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> highlightedEntity(const ToolState &tool, const World &world) {
  if (tool.Kind == ToolKind::MoveBox && !tool.Holding && tool.Pointer) {
    return boxAt(world, *tool.Pointer);
  }
  return std::nullopt;
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: the build gives no output, and the tests of criteria 3 to 6 pass.

### Task 13: Build ghosts

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Rename the body of the existing three-argument appendPath into the new four-argument one, using the `color` parameter where it used `pathColor(kind)`, and make the three-argument one:

```cpp
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points) {
  appendPath(mesh, kind, points, pathColor(kind));
}
```

Step 2: In the anonymous namespace, after addQuad, add:

```cpp
// An edit's ghost color for a kind's color: translucent when accepted, the invalid tint when not.
Rgba ghostColor(const World &world, const ParkEdit &edit, Rgba color) {
  return isAccepted(world, edit) ? Rgba{color.R, color.G, color.B, GHOST_ALPHA} : INVALID_TINT;
}
```

Step 3: Replace the appendEntity and buildGhostMesh stubs with:

```cpp
void appendEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key) {
      appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), color);
      return;
    }
  }
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Key == key) {
      appendPath(mesh, path.Kind, path.Points, color);
      return;
    }
  }
}

ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit) {
  ParkMesh mesh;
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    appendBox(mesh, add->At, boxSize(add->Kind), boxHeight(add->Kind),
              ghostColor(world, edit, boxColor(add->Kind)));
  } else if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Key == move->Box) {
        appendBox(mesh, move->At, boxSize(box.Kind), boxHeight(box.Kind),
                  ghostColor(world, edit, boxColor(box.Kind)));
      }
    }
  } else if (const auto *path = std::get_if<AddPath>(&edit)) {
    appendPath(mesh, path->Kind, path->Points, ghostColor(world, edit, pathColor(path->Kind)));
  } else if (isAccepted(world, edit)) {
    const EntityKey key = std::holds_alternative<DeletePath>(edit) ? std::get<DeletePath>(edit).Path
                                                                   : std::get<DeleteBox>(edit).Box;
    appendEntity(mesh, world, key, DELETE_TINT);
  }
  return mesh;
}
```

Step 4: Add `#include <variant>` to the system includes.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: the build gives no output, and the tests of criteria 7 and 8 pass, with park-view's park mesh tests still passing.

### Task 14: Implement cursor picking

Files:
- Modify: `src/render/picking.cpp`

Step 1: Add `#include <math.h>` to the system includes, and replace the stub with:

```cpp
std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY) {
  const Vec3 forward = normalize(view.Target - view.Eye);
  const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  const Vec3 up = cross(right, forward);
  const float tanHalf = tanf(0.5f * view.FovY);
  const Vec3 direction = forward + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf);
  if (view.Eye.Y <= 0.0f || direction.Y >= 0.0f) {
    return std::nullopt;
  }
  const float t = -view.Eye.Y / direction.Y;
  return ParkPoint{view.Eye.X + direction.X * t, view.Eye.Z + direction.Z * t};
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: the build gives no output, and the tests of criterion 9 pass.

### Task 15: Draw the translucent pass

Files:
- Modify: `src/render/shaders/park.vert`
- Modify: `src/render/renderer.cpp`

Step 1: In src/render/shaders/park.vert, after the three `out` declarations, add:

```glsl
// The ghost pipeline shares this shader and tests depth greater-or-equal against what the park
// pipeline wrote, so both must compute the same position for the same vertex.
invariant gl_Position;
```

Step 2: In src/render/renderer.cpp, give createPipeline a last parameter `bool translucent`, update its comment to "A lit, depth-tested pipeline with back faces culled. Depth is reversed: nearer is greater. A translucent one blends by alpha, passes equal depths, and writes no depth.", and replace its three depth_stencil_state lines and the colorTarget setup with:

```cpp
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
```

```cpp
  info.depth_stencil_state.enable_depth_test = true;
  info.depth_stencil_state.enable_depth_write = !translucent;
  info.depth_stencil_state.compare_op =
      translucent ? SDL_GPU_COMPAREOP_GREATER_OR_EQUAL : SDL_GPU_COMPAREOP_GREATER;
```

Pass `false` in createTerrainPipeline's call.

Step 3: In createParkPipeline, replace the final two statements with:

```cpp
  renderer.ParkPipeline = createPipeline(renderer, "park.vert.spv", "park.frag.spv", attributes, 3,
                                         sizeof(ParkVertex), false);
  renderer.GhostPipeline = createPipeline(renderer, "park.vert.spv", "park.frag.spv", attributes,
                                          3, sizeof(ParkVertex), true);
  return renderer.ParkPipeline != nullptr && renderer.GhostPipeline != nullptr;
```

Step 4: Replace the body of setParkMesh with a helper in the anonymous namespace before it, and make both setters call it:

```cpp
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
```

```cpp
bool setParkMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.ParkVertices, renderer.ParkIndices,
                     renderer.ParkIndexCount);
}

bool setGhostMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.GhostVertices, renderer.GhostIndices,
                     renderer.GhostIndexCount);
}
```

If replaceMesh must sit in a different anonymous-namespace block to precede setParkMesh, open one just before it.

Step 5: In destroyRenderer, after the three ParkPipeline and park buffer releases, add:

```cpp
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GhostVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GhostIndices);
  SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.GhostPipeline);
```

Step 6: In drawScene, after the park mesh's `if` block and before `SDL_EndGPURenderPass(pass);`, add:

```cpp
  // Ghosts and highlights come after everything opaque, so they blend over it.
  if (renderer.GhostIndexCount > 0) {
    SDL_BindGPUGraphicsPipeline(pass, renderer.GhostPipeline);
    const SDL_GPUBufferBinding ghostVertexBinding = {renderer.GhostVertices, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &ghostVertexBinding, 1);
    const SDL_GPUBufferBinding ghostIndexBinding = {renderer.GhostIndices, 0};
    SDL_BindGPUIndexBuffer(pass, &ghostIndexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, renderer.GhostIndexCount, 1, 0, 0, 0);
  }
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/ThemeParkJones --park tests/parks/sketch.park --capture build/linux-debug/sketch.bmp; echo $?`
Expected: no warnings, and `0`, with the capture unchanged from park-view's: an empty ghost mesh draws nothing.

### Task 16: Add the Tools panel

Files:
- Create: `src/app/tool_panel.h`
- Create: `src/app/tool_panel.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Create src/app/tool_panel.h:

```cpp
#ifndef TPJ_APP_TOOL_PANEL_H
#define TPJ_APP_TOOL_PANEL_H

#include "tools/tools.h"

namespace tpj {

// Draws the Tools panel, with a choice for each tool. Returns true when the player chose a
// different tool, which it writes to kind. Call between ImGui::NewFrame and ImGui::Render.
bool drawToolPanel(ToolKind &kind);

} // namespace tpj

#endif
```

Step 2: Create src/app/tool_panel.cpp:

```cpp
#include "app/tool_panel.h"

#include <imgui.h>

namespace tpj {
namespace {

struct ToolChoice {
  const char *Label;
  ToolKind Kind;
};

constexpr ToolChoice TOOL_CHOICES[] = {{"Look", ToolKind::None},
                                       {"Place shop", ToolKind::PlaceShop},
                                       {"Place depot", ToolKind::PlaceDepot},
                                       {"Move box", ToolKind::MoveBox},
                                       {"Delete", ToolKind::Delete}};

} // namespace

bool drawToolPanel(ToolKind &kind) {
  bool changed = false;
  ImGui::SetNextWindowPos(ImVec2(12.0f, 140.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (const ToolChoice &choice : TOOL_CHOICES) {
      if (ImGui::RadioButton(choice.Label, kind == choice.Kind) && kind != choice.Kind) {
        kind = choice.Kind;
        changed = true;
      }
    }
  }
  ImGui::End();
  return changed;
}

} // namespace tpj
```

Step 3: In src/app/CMakeLists.txt, add `tool_panel.cpp` to tpj_app's sources after `orbit_camera.cpp`, and `tpj_tools` to its target_link_libraries after `tpj_sim`.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 17: Wire the tools into the loop

Files:
- Modify: `src/app/main.cpp`

Step 1: Add `#include "app/tool_panel.h"`, `#include "render/picking.h"`, `#include "sim/command_queue.h"`, `#include "sim/park/edits.h"`, and `#include "tools/tools.h"` to the project includes in sorted order.

Step 2: After the DrawnIntent struct, add:

```cpp
// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// The edit and highlight a ghost mesh was built from.
struct DrawnGhost {
  std::optional<tpj::ParkEdit> Edit;
  std::optional<tpj::EntityKey> Highlight;

  bool operator==(const DrawnGhost &) const = default;
};
```

Step 3: Give updateParkMesh a last parameter `bool &rebuilt`, set `rebuilt = false;` at its start, and `rebuilt = true;` just before its final `return tpj::setParkMesh(renderer, mesh);`. Update its comment to end "Returns false if the upload failed, and sets rebuilt when it built a mesh."

Step 4: After updateParkMesh, add:

```cpp
// Rebuilds the ghost mesh when the tool's edit or highlight differs from what was last drawn, or
// the park mesh was rebuilt. Returns false if the upload failed.
bool updateGhostMesh(tpj::Renderer &renderer, const tpj::World &world, const tpj::ToolState &tool,
                     std::optional<DrawnGhost> &drawn, bool parkRebuilt) {
  DrawnGhost current{tpj::tentativeEdit(tool, world), tpj::highlightedEntity(tool, world)};
  if (drawn && *drawn == current && !parkRebuilt) {
    return true;
  }
  tpj::ParkMesh mesh;
  if (current.Edit) {
    mesh = tpj::buildGhostMesh(world, *current.Edit);
  }
  if (current.Highlight) {
    tpj::appendEntity(mesh, world, *current.Highlight, tpj::HIGHLIGHT_TINT);
  }
  drawn = std::move(current);
  return tpj::setGhostMesh(renderer, mesh);
}

// The ground under the cursor, or none while ImGui wants the mouse or the cursor meets no ground.
std::optional<tpj::ParkPoint> groundUnderCursor(SDL_Window *window, const tpj::CameraView &view) {
  if (ImGui::GetIO().WantCaptureMouse) {
    return std::nullopt;
  }
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0) {
    return std::nullopt;
  }
  float x = 0.0f;
  float y = 0.0f;
  SDL_GetMouseState(&x, &y);
  const float ndcX = 2.0f * x / static_cast<float>(width) - 1.0f;
  const float ndcY = 1.0f - 2.0f * y / static_cast<float>(height);
  return tpj::groundAtCursor(view, static_cast<float>(width) / static_cast<float>(height), ndcX,
                             ndcY);
}
```

Step 5: Give gatherInput a second parameter `PointerButtons &buttons`, update its comment to "Drains pending events into ImGui, one frame of camera input, and the left button. Input ImGui is using does not reach the camera, nor a press the tool; a release always reaches it. Returns false when the app should quit.", and replace its event loop's body after `ImGui_ImplSDL3_ProcessEvent(&event);` with:

```cpp
    if (event.type == SDL_EVENT_QUIT) {
      keepRunning = false;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
      buttons.Released = true;
    } else if (!io.WantCaptureMouse) {
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        buttons.Pressed = true;
      }
      addMouseInput(event, input);
    }
```

Step 6: In runLoop, after `std::optional<DrawnIntent> drawn;`, add:

```cpp
  std::optional<DrawnGhost> drawnGhost;
  tpj::ToolState tool;
  tpj::CommandQueue commands;
```

Replace `tpj::CameraInput input;` and `if (!gatherInput(input)) {` with:

```cpp
    tpj::CameraInput input;
    PointerButtons buttons;
    if (!gatherInput(input, buttons)) {
```

Replace `tpj::stepWorld(world);` in the tick loop with `tpj::stepWorld(world, commands);`, and update the loop's comment to "The simulation advances in fixed ticks regardless of frame rate, and queued edits apply at the next (principle 10)."

Replace `if (!updateParkMesh(renderer, world, drawn, camera)) {` with:

```cpp
    bool parkRebuilt = false;
    if (!updateParkMesh(renderer, world, drawn, camera, parkRebuilt)) {
```

Step 7: After `view.Target = camera.Focus;`, add the following. The press and release come before the pointer moves, so they act on the pointer the ghost on screen was built from, and a release commits the edit that ghost showed.

```cpp
    if (buttons.Pressed) {
      tpj::pressPointer(tool, world);
    }
    if (buttons.Released) {
      if (const std::optional<tpj::ParkEdit> edit = tpj::releasePointer(tool, world)) {
        tpj::queueEdit(commands, *edit);
      }
    }
    tpj::movePointer(tool, groundUnderCursor(renderer.Window, view));
    if (!updateGhostMesh(renderer, world, tool, drawnGhost, parkRebuilt)) {
      return false;
    }
```

Step 8: After `tpj::drawDebugPanel(stats);`, add:

```cpp
    tpj::ToolKind kind = tool.Kind;
    if (tpj::drawToolPanel(kind)) {
      tpj::selectTool(tool, kind);
    }
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/ThemeParkJones --park tests/parks/sketch.park --capture build/linux-debug/sketch.bmp; echo $?`
Expected: no warnings, and `0`, with the capture showing the Tools panel below the Debug panel.

Deviation, fixed inline: clang-tidy refused runLoop's size and an exception escaping main through the std::variant the loop now holds. The press, release, and pointer calls moved into useTool, the two panels into drawPanels, and main calls runLoopLogged, which runs runLoop and logs a std::exception it throws before the window and GPU are released.

Review fix: the press and release moved into useButtons, called before the tick loop, so a release is judged against the world the displayed ghost was built from; movePointer stays after the camera update.

### Task 18: Verify

Step 1: Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
Expected: no output.

Step 2: Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings, and `100% tests passed`.

Step 3: Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1; ctest.exe --preset windows-debug 2>&1 | tail -3`
Expected: the build finishes, and `100% tests passed`.

Step 4: Run: `scripts/cross-build-check.sh`
Expected: it passes.

Step 5: Dispatch the reviewer agent on the diff, with FEATURE.md, the four SPEC.md files, and docs/principles.md.
Expected: no findings that affect correctness or stated requirements, or each fixed.

Step 6: Ask Evan to run build/windows-debug/ThemeParkJones.exe --park tests/parks/sketch.park and check criterion 10.
Expected: Evan confirms it.

### Task 19: Commit

Step 1: Commit via the commit-hygiene skill, staging the paths this plan and the test pass touched by name, never image.png, with the message:

```
Tools: Place, move, and delete boxes with ghosts
```

and a body saying that src/tools gives tentative edits from ground positions and presses, a release commits exactly the ghost's edit, and ghosts and highlights draw in a translucent pass, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

Run: `git log -1 --format=%s`
Expected: `Tools: Place, move, and delete boxes with ghosts`
