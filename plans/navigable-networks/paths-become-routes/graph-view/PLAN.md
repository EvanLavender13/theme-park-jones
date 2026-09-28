# Implementation Plan: Graph View

## Goal

Build the graph overlay of the park's networks on the CPU in render/graph_overlay, and draw it in the app behind a Graph checkbox and a --graph option.

## Approach

windowPoint projects a ground point with drawFrame's matrices and maps it to window coordinates, refusing points whose view depth is not positive. buildGraphOverlay walks each kind's network through its public queries, clips each carrier segment to the part at view depth at least NearZ on the ground before projecting, and projects each node's ground position. The app keeps a showGraph flag, set by --graph and the Debug panel's checkbox, and hands the overlay to ImGui's background draw list each frame it is set.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify the graph overlay

Files:
- Modify: `src/render/SPEC.md`

Step 1: Append the "## Graph overlay" section FEATURE.md gives for src/render/SPEC.md at the end of the file, after the Ghosts section, without the four-space indent.

Run: `grep -c "^## Graph overlay" src/render/SPEC.md`
Expected: `1`

### Task 2: Specify the Graph checkbox and --graph

Files:
- Modify: `src/app/SPEC.md`

Step 1: Replace the Tooling UI section's paragraph with the one FEATURE.md gives.

Step 2: In the Command line section, add the --graph paragraph FEATURE.md gives after the paragraph starting "--frames N exits", and replace "or --hash given with --frames or --capture, whatever their values," with "or --hash given with --frames, --capture, or --graph, whatever their values,".

Run: `grep -c "\-\-graph" src/app/SPEC.md`
Expected: `3`

### Task 3: Declare the graph overlay

Files:
- Create: `src/render/graph_overlay.h`
- Create: `src/render/graph_overlay.cpp`
- Modify: `src/render/CMakeLists.txt`

Step 1: Create `src/render/graph_overlay.h`:

```cpp
#ifndef TPJ_RENDER_GRAPH_OVERLAY_H
#define TPJ_RENDER_GRAPH_OVERLAY_H

#include "render/park_mesh.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// The graph view's colors, opaque and distinct from each other and from the paths' ribbons.
inline constexpr Rgba GRAPH_NODE_COLOR{1.0f, 1.0f, 1.0f, 1.0f};

constexpr Rgba graphColor(PathKind kind) {
  return kind == PathKind::Guest ? Rgba{0.10f, 0.85f, 1.0f, 1.0f} : Rgba{1.0f, 0.30f, 0.80f, 1.0f};
}

// In window units.
inline constexpr float GRAPH_LINE_THICKNESS = 2.0f;
inline constexpr float GRAPH_NODE_RADIUS = 4.0f;

// A position in a window, from its top left corner.
struct WindowPoint {
  float X = 0.0f;
  float Y = 0.0f;
};

// A carrier segment, from the carrier's point at Segment to the next, as drawn: clipped to the
// near plane and projected.
struct GraphLine {
  PathKind Kind = PathKind::Guest;
  EntityKey Carrier = NULL_KEY;
  uint32_t Segment = 0;
  WindowPoint From;
  WindowPoint To;
};

struct GraphNode {
  PathKind Kind = PathKind::Guest;
  uint32_t Node = 0;
  WindowPoint At;
};

struct GraphOverlay {
  std::vector<GraphLine> Lines;
  std::vector<GraphNode> Nodes;
};

// Where drawFrame's projection puts the ground point in a window of the width and height. None
// when the point's view depth is not positive, or the width or height is not. See render/SPEC.md.
std::optional<WindowPoint> windowPoint(const CameraView &view, float width, float height,
                                       GroundPoint point);

// Both networks' segments and nodes in a window of the width and height, with segments clipped to
// the near plane. See render/SPEC.md.
GraphOverlay buildGraphOverlay(const World &world, const CameraView &view, float width,
                               float height);

} // namespace tpj

#endif
```

Step 2: Create `src/render/graph_overlay.cpp` with stubs:

```cpp
#include "render/graph_overlay.h"

namespace tpj {

std::optional<WindowPoint> windowPoint(const CameraView & /*view*/, float /*width*/,
                                       float /*height*/, GroundPoint /*point*/) {
  return std::nullopt;
}

GraphOverlay buildGraphOverlay(const World & /*world*/, const CameraView & /*view*/,
                               float /*width*/, float /*height*/) {
  return {};
}

} // namespace tpj
```

Step 3: In `src/render/CMakeLists.txt`, add `graph_overlay.cpp` to `add_library(tpj_render STATIC ...)` between `math.cpp` and `park_mesh.cpp`.

Run: `cmake --build --preset linux-debug --target tpj_render 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 4: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, src/render/SPEC.md, src/app/SPEC.md, src/sim/routes/SPEC.md, and src/sim/medium/SPEC.md, and the public headers src/render/graph_overlay.h, src/render/picking.h, and src/sim/routes/networks.h.

### Task 5: Project ground points

Files:
- Modify: `src/render/graph_overlay.cpp`

Step 1: Replace the file with windowPoint implemented and buildGraphOverlay still a stub:

```cpp
#include "render/graph_overlay.h"

#include "render/math.h"

namespace tpj {

namespace {

constexpr Vec3 UP{0.0f, 1.0f, 0.0f};

// dot(p - Eye, f), with p the point at height 0 and f the unit direction from Eye to Target.
float viewDepth(const CameraView &view, GroundPoint point) {
  const Vec3 ground{static_cast<float>(point.X), 0.0f, static_cast<float>(point.Z)};
  return dot(ground - view.Eye, normalize(view.Target - view.Eye));
}

} // namespace

std::optional<WindowPoint> windowPoint(const CameraView &view, float width, float height,
                                       GroundPoint point) {
  if (!(width > 0.0f) || !(height > 0.0f) || !(viewDepth(view, point) > 0.0f)) {
    return std::nullopt;
  }
  const Mat4 viewProjection = multiply(perspective(view.FovY, width / height, view.NearZ, view.FarZ),
                                       lookAt(view.Eye, view.Target, UP));
  const auto &m = viewProjection.M;
  const auto x = static_cast<float>(point.X);
  const auto z = static_cast<float>(point.Z);
  // Column-major: element (row, col) is M[col * 4 + row], and the point is (x, 0, z, 1).
  const auto row = [&m, x, z](int r) { return (m[r] * x) + (m[8 + r] * z) + m[12 + r]; };
  const float w = row(3);
  return WindowPoint{((row(0) / w) + 1.0f) * 0.5f * width, (1.0f - (row(1) / w)) * 0.5f * height};
}

GraphOverlay buildGraphOverlay(const World & /*world*/, const CameraView & /*view*/,
                               float /*width*/, float /*height*/) {
  return {};
}

} // namespace tpj
```

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#graph_overlay_test]"`
Expected: no build output. The tests of criteria 1 and 2 pass; those of criteria 3 to 5 fail on missing lines or nodes. If the test pass named its file differently, use its name in the filter.

### Task 6: Clip and project segments

Files:
- Modify: `src/render/graph_overlay.cpp`

Step 1: Add these includes after `#include "render/math.h"`:

```cpp
#include "sim/routes/networks.h"

#include <cstddef>
```

Step 2: Add to the anonymous namespace, after viewDepth:

```cpp
// The point of the segment from a to b at view depth nearZ, where a lies below it and b does not.
GroundPoint pointAtDepth(GroundPoint a, GroundPoint b, float depthA, float depthB, float nearZ) {
  const double t = static_cast<double>(nearZ - depthA) / static_cast<double>(depthB - depthA);
  return {a.X + (t * (b.X - a.X)), a.Z + (t * (b.Z - a.Z))};
}

// Adds a line for each segment of the network's carriers with a part in front of the near plane.
void appendLines(GraphOverlay &overlay, const Network &network, PathKind kind,
                 const CameraView &view, float width, float height) {
  for (const Carrier &carrier : network.carriers()) {
    for (std::size_t index = 0; index + 1 < carrier.Points.size(); ++index) {
      GroundPoint a{carrier.Points[index].X, carrier.Points[index].Z};
      GroundPoint b{carrier.Points[index + 1].X, carrier.Points[index + 1].Z};
      const float depthA = viewDepth(view, a);
      const float depthB = viewDepth(view, b);
      if (depthA < view.NearZ && depthB < view.NearZ) {
        continue;
      }
      if (depthA < view.NearZ) {
        a = pointAtDepth(a, b, depthA, depthB, view.NearZ);
      } else if (depthB < view.NearZ) {
        b = pointAtDepth(b, a, depthB, depthA, view.NearZ);
      }
      const std::optional<WindowPoint> from = windowPoint(view, width, height, a);
      const std::optional<WindowPoint> to = windowPoint(view, width, height, b);
      if (from && to) {
        overlay.Lines.push_back(
            GraphLine{kind, carrier.Key, static_cast<uint32_t>(index), *from, *to});
      }
    }
  }
}
```

Step 3: Replace the buildGraphOverlay stub:

```cpp
GraphOverlay buildGraphOverlay(const World &world, const CameraView &view, float width,
                               float height) {
  GraphOverlay overlay;
  if (!(width > 0.0f) || !(height > 0.0f)) {
    return overlay;
  }
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    appendLines(overlay, parkNetwork(world, kind), kind, view, width, height);
  }
  return overlay;
}
```

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#graph_overlay_test]"`
Expected: no build output. The tests of criteria 1 to 4 and 6 pass; those of criterion 5 fail on missing nodes.

### Task 7: Project nodes

Files:
- Modify: `src/render/graph_overlay.cpp`

Step 1: Add to the anonymous namespace, after appendLines:

```cpp
// Adds a node for each of the network's nodes whose ground position lies in front of the near
// plane.
void appendNodes(GraphOverlay &overlay, const Network &network, PathKind kind,
                 const CameraView &view, float width, float height) {
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const std::optional<GroundPoint> ground = network.groundPoint(network.nodePlace(node));
    if (!ground || viewDepth(view, *ground) < view.NearZ) {
      continue;
    }
    if (const std::optional<WindowPoint> at = windowPoint(view, width, height, *ground)) {
      overlay.Nodes.push_back(GraphNode{kind, node, *at});
    }
  }
}
```

Step 2: In buildGraphOverlay's loop, after the appendLines call, add:

```cpp
    appendNodes(overlay, parkNetwork(world, kind), kind, view, width, height);
```

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests`
Expected: no build output, and all tpj_render_tests pass.

### Task 8: Accept --graph

Files:
- Modify: `src/app/main.cpp`

Step 1: In `struct Options`, add after `bool PrintHash = false;`:

```cpp
  bool ShowGraph = false;
```

Step 2: In parseOptions, add a branch after the `--hash` branch:

```cpp
    } else if (strcmp(argv[i], "--graph") == 0) {
      options.ShowGraph = true;
```

Step 3: Change the refusal condition to:

```cpp
  if (options.PrintHash && (options.FramesGiven || options.CapturePath != nullptr ||
                            options.ShowGraph)) {
```

Step 4: Change the usage line to:

```cpp
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH] [--graph]",
            argv[0]);
```

Step 5: Extend the comment above parseOptions: after "--frames N exits after N frames." add " --graph starts with the graph view on."

Run: `cmake --build --preset linux-debug --target tpj_app_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_app_tests -# "[#command_line_test]"`
Expected: no build output, and all command-line tests pass.

### Task 9: Add the Graph checkbox

Files:
- Modify: `src/app/debug_panel.h`
- Modify: `src/app/debug_panel.cpp`
- Modify: `src/app/main.cpp`

Step 1: In debug_panel.h, replace the declaration and its comment with:

```cpp
// Draws the tooling panel with frame rate, simulation tick, camera state, and the Graph checkbox,
// which sets showGraph. Call between ImGui::NewFrame and ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph);
```

Step 2: In debug_panel.cpp, change the definition's signature to match, and add after the Distance line:

```cpp
    ImGui::Checkbox("Graph", &showGraph);
```

Step 3: In main.cpp, change drawPanels to take `bool &showGraph` after `const tpj::OrbitCamera &camera`, pass it to `tpj::drawDebugPanel(stats, showGraph);`, and extend its comment: "Builds the Debug and Tools panels, setting showGraph from the Graph checkbox, selecting the tool the player chose and starting the park action they pressed."

Step 4: In runLoop, add after `tpj::CommandQueue commands;`:

```cpp
  bool showGraph = options.ShowGraph;
```

and change the drawPanels call to `drawPanels(renderer.Window, world, camera, showGraph, tool);`.

Run: `cmake --build --preset linux-debug --target tpj_app 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 10: Draw the overlay

Files:
- Modify: `src/app/main.cpp`

Step 1: Add `#include "render/graph_overlay.h"` after `#include "core/profile.h"`.

Step 2: Add before drawPanels:

```cpp
ImU32 imColor(tpj::Rgba color) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(color.R, color.G, color.B, color.A));
}

// Draws the networks over the scene and behind every panel: lines in their kind's graph color,
// then nodes.
void drawGraph(const tpj::World &world, const tpj::CameraView &view) {
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  const tpj::GraphOverlay overlay = tpj::buildGraphOverlay(world, view, size.x, size.y);
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();
  for (const tpj::GraphLine &line : overlay.Lines) {
    drawList->AddLine(ImVec2(line.From.X, line.From.Y), ImVec2(line.To.X, line.To.Y),
                      imColor(tpj::graphColor(line.Kind)), tpj::GRAPH_LINE_THICKNESS);
  }
  for (const tpj::GraphNode &node : overlay.Nodes) {
    drawList->AddCircleFilled(ImVec2(node.At.X, node.At.Y), tpj::GRAPH_NODE_RADIUS,
                              imColor(tpj::GRAPH_NODE_COLOR));
  }
}
```

Step 3: In runLoop, after the drawPanels call and before `ImGui::Render();`, add:

```cpp
    if (showGraph) {
      drawGraph(world, view);
    }
```

Run: `cmake --build --preset linux-debug --target tpj_app 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 11: Verify

Step 1: Format, then build and test linux-debug in full.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i; cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings or errors, and "100% tests passed".

Step 2: Build and test windows-debug.

Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1; ctest.exe --preset windows-debug 2>&1 | tail -3`
Expected: the build finishes, and "100% tests passed".

Step 3: Capture routes.park with the graph on, convert it, and look at it.

Run: `build/linux-debug/ThemeParkJones --park tests/parks/routes.park --graph --capture build/linux-debug/graph.bmp`
Expected: exit status 0. Converted to PNG with PIL and viewed, the capture shows cyan lines along the guest paths and a magenta line along the backstage path, with white dots at the path ends and at the guest paths' junctions and crossings, drawn behind the Debug and Tools panels, and the Debug panel's Graph checkbox checked.

Step 4: Capture it again without --graph.

Run: `build/linux-debug/ThemeParkJones --park tests/parks/routes.park --capture build/linux-debug/nograph.bmp`
Expected: exit status 0, and the capture shows no graph and the Graph checkbox unchecked.

### Task 12: Commit

Stage the changed paths by name (src/render, src/app, tests/render, tests/app, and plans/navigable-networks/paths-become-routes/graph-view), never parks/sketch.park, and commit via the commit-hygiene skill with the subject "Render: Draw the park's networks over the scene".
