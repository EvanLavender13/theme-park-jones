# Implementation Plan: Path Tool

## Goal

Let the player draw guest and backstage paths by clicking points, with a ghost curve that snaps onto same-kind paths, finishing on a click of the last point and cancelling from the Tools panel.

## Approach

src/tools gains two ToolKinds, the drawn points in ToolState, snapToPath, and a path branch in pressPointer, releasePointer, and tentativeEdit, so the existing release contract carries ghost-equals-commit to paths. buildGhostMesh already draws an AddPath ghost, so the renderer changes only in its spec. The Tools panel adds the two tools, a finishing hint, and a Cancel path button.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify the path tools

Files:
- Modify: `src/tools/SPEC.md`

Step 1: In "## Input", replace the first two sentences and the selectTool sentence, from "A ToolState holds one tool" through "keeping the pointer and the place tools' facing.", with the text FEATURE.md gives for "## Input".

Step 2: At the end of "## Picking", add the paragraph FEATURE.md gives, starting "snapToPath gives".

Step 3: In "## Tools", after the paragraph "None gives no edit and no highlight, and never takes hold.", add the paragraph FEATURE.md gives, starting "GuestPath and BackstagePath draw".

Run: `grep -c "snapToPath\|FINISH_REACH\|any drawn points" src/tools/SPEC.md`
Expected: `3`

### Task 2: Specify the path ghost

Files:
- Modify: `src/render/SPEC.md`

Step 1: In "## Ghosts", after the sentence ending "the box buildParkMesh draws in the world the edit gives.", add the sentence FEATURE.md gives, starting "Likewise, the ghost of an accepted AddPath".

Run: `grep -c "ghost of an accepted AddPath" src/render/SPEC.md`
Expected: `1`

### Task 3: Specify the panel's path tools

Files:
- Modify: `src/app/SPEC.md`

Step 1: In "## Tools", replace the first sentence, "The Tools panel selects the tool (tools/SPEC.md): Look, ... Move box, and Delete.", with the two sentences FEATURE.md gives.

Run: `grep -c "Cancel path" src/app/SPEC.md`
Expected: `1`

### Task 4: Record the milestone's tuning value and note

Files:
- Modify: `plans/effortless-building/sketch-a-park/MILESTONE.md`

Step 1: In Tuning values, after the bullet "- Snapping reach: 2 m from a path's ground line.", add:

```markdown
- Finishing reach: 1 m from a path's last drawn point, where a click finishes the path.
```

Step 2: In Research notes, after the bullet beginning "- A highlight lies exactly on what it marks", add:

```markdown
- A click on a path's last drawn point finishes it, so a double-click does, and the ghost while the cursor rests there is the path that commits. The path tool snaps every point it draws onto the nearest point of a same-kind ground line.
```

Run: `grep -c "Finishing reach\|A click on a path's last drawn point" plans/effortless-building/sketch-a-park/MILESTONE.md`
Expected: `2`

### Task 5: Declare the path tools

Files:
- Modify: `src/tools/tools.h`
- Modify: `src/tools/tools.cpp`

Step 1: In src/tools/tools.h, add `#include <vector>` after `#include <stdint.h>`, and replace the ToolKind line with:

```cpp
enum class ToolKind : uint8_t {
  None,
  GuestPath,
  BackstagePath,
  PlaceShop,
  PlaceDepot,
  MoveBox,
  Delete
};
```

Step 2: After the MIN_FACING_DRAG constant, add:

```cpp
// A path tool snaps the pointer onto a same-kind path's ground line this near it, in meters.
inline constexpr double SNAP_REACH = 2.0;
// A path tool's press this near its last drawn point finishes the path, in meters.
inline constexpr double FINISH_REACH = 1.0;
```

Step 3: In ToolState, after `Pose Target;`, add:

```cpp
  // A path tool's points drawn so far.
  std::vector<ParkPoint> Drawn;
```

Step 4: After the pathAt declaration, add:

```cpp
// The nearest point to the given one on the ground line of a path of the kind, when one lies within
// SNAP_REACH, and the point itself otherwise.
ParkPoint snapToPath(const World &world, PathKind kind, ParkPoint point);
```

Step 5: In src/tools/tools.cpp, at the end of namespace tpj, add the stub:

```cpp
ParkPoint snapToPath(const World & /*world*/, PathKind /*kind*/, ParkPoint point) {
  return point;
}
```

Run: `cmake --build --preset linux-debug --target tpj_tools_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 6: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/tools/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, and src/sim/park/SPEC.md, and the public headers src/tools/tools.h and src/render/park_mesh.h. Its tests of criteria 1 to 4 must build and fail on behavior. Those of criteria 5 to 7 may already pass, since the stubs give no drawn points and buildGhostMesh already draws AddPath.

### Task 7: Implement snapToPath

Files:
- Modify: `src/tools/tools.cpp`

Step 1: In the anonymous namespace, after segmentDistance, add:

```cpp
// The nearest point to a point on the segment between two distinct ground line points.
ParkPoint nearestOnSegment(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double t = std::clamp(
      ((point.X - from.X) * dx + (point.Z - from.Z) * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  return ParkPoint{from.X + dx * t, from.Z + dz * t};
}
```

Step 2: Replace the snapToPath stub with:

```cpp
ParkPoint snapToPath(const World &world, PathKind kind, ParkPoint point) {
  ParkPoint snapped = point;
  std::optional<double> nearest;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind != kind) {
      continue;
    }
    const std::vector<CarrierPoint> line = groundLine(path.Points);
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      const ParkPoint candidate = nearestOnSegment(point, line[i], line[i + 1]);
      const double dx = candidate.X - point.X;
      const double dz = candidate.Z - point.Z;
      const double distance = sqrt(dx * dx + dz * dz);
      if (distance <= SNAP_REACH && (!nearest || distance < *nearest)) {
        snapped = candidate;
        nearest = distance;
      }
    }
  }
  return snapped;
}
```

Run: `cmake --build --preset linux-debug --target tpj_tools_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_tools_tests "<criterion 1's test names from the test pass>" 2>&1 | tail -2`
Expected: no warnings, and `All tests passed`. The tests of criteria 2 to 4 still fail until Task 8.

### Task 8: Implement the path tools

Files:
- Modify: `src/tools/tools.cpp`

Step 1: In the anonymous namespace, after placedKind, add:

```cpp
bool isPathTool(ToolKind kind) {
  return kind == ToolKind::GuestPath || kind == ToolKind::BackstagePath;
}

PathKind drawnKind(ToolKind kind) {
  return kind == ToolKind::GuestPath ? PathKind::Guest : PathKind::Backstage;
}

// The point a path tool's press would append: the snapped pointer, unless it lies within
// FINISH_REACH of the last drawn point. See tools/SPEC.md.
std::optional<ParkPoint> nextPoint(const ToolState &tool, const World &world) {
  if (!tool.Pointer) {
    return std::nullopt;
  }
  const ParkPoint point = snapToPath(world, drawnKind(tool.Kind), *tool.Pointer);
  if (!tool.Drawn.empty()) {
    const double dx = point.X - tool.Drawn.back().X;
    const double dz = point.Z - tool.Drawn.back().Z;
    if (dx * dx + dz * dz <= FINISH_REACH * FINISH_REACH) {
      return std::nullopt;
    }
  }
  return point;
}

// A path tool's edit: the drawn points, followed by the next point while not holding.
std::optional<ParkEdit> pathEdit(const ToolState &tool, const World &world) {
  std::vector<ParkPoint> points = tool.Drawn;
  if (!tool.Holding && !points.empty()) {
    if (const std::optional<ParkPoint> next = nextPoint(tool, world)) {
      points.push_back(*next);
    }
  }
  if (points.size() < 2) {
    return std::nullopt;
  }
  return AddPath{drawnKind(tool.Kind), std::move(points)};
}
```

Add `#include <utility>` after `#include <math.h>`.

Step 2: In selectTool, after `tool.Held = NULL_KEY;`, add `tool.Drawn.clear();`.

Step 3: In pressPointer, before the branch `} else if (isPlaceTool(tool.Kind) && tool.Pointer) {`, add:

```cpp
  } else if (isPathTool(tool.Kind) && tool.Pointer) {
    if (const std::optional<ParkPoint> next = nextPoint(tool, world)) {
      tool.Drawn.push_back(*next);
    } else {
      tool.Holding = true;
    }
```

Step 4: In releasePointer, after the block that copies the landing facing for a place tool, add:

```cpp
  if (isPathTool(tool.Kind)) {
    tool.Drawn.clear();
  }
```

Step 5: At the start of tentativeEdit, add:

```cpp
  if (isPathTool(tool.Kind)) {
    return pathEdit(tool, world);
  }
```

Run: `cmake --build --preset linux-debug --target tpj_tools_tests tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_tools_tests 2>&1 | tail -2; build/linux-debug/tpj_render_tests 2>&1 | tail -2`
Expected: no warnings, and every test passes, covering criteria 1 to 7.

### Task 9: Add the path tools to the panel

Files:
- Modify: `src/app/tool_panel.h`
- Modify: `src/app/tool_panel.cpp`

Step 1: In src/app/tool_panel.h, add `#include <optional>` after the tools include, and replace the drawToolPanel declaration and its comment with:

```cpp
// Draws the Tools panel, with a choice for each tool, and while the tool is drawing a path, a hint
// and a Cancel path button. Returns the tool to select: a different one the player chose, or the
// current one again when they cancelled. Call between ImGui::NewFrame and ImGui::Render.
std::optional<ToolKind> drawToolPanel(ToolKind current, bool drawing);
```

Step 2: In src/app/tool_panel.cpp, add `{"Guest path", ToolKind::GuestPath}` and `{"Backstage path", ToolKind::BackstagePath}` to TOOL_CHOICES after Look, and replace drawToolPanel with:

```cpp
std::optional<ToolKind> drawToolPanel(ToolKind current, bool drawing) {
  std::optional<ToolKind> chosen;
  ImGui::SetNextWindowPos(ImVec2(12.0f, 140.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (const ToolChoice &choice : TOOL_CHOICES) {
      if (ImGui::RadioButton(choice.Label, current == choice.Kind) && current != choice.Kind) {
        chosen = choice.Kind;
      }
    }
    if (drawing) {
      ImGui::Separator();
      ImGui::TextUnformatted("Click the last point again to finish.");
      if (ImGui::Button("Cancel path")) {
        chosen = current;
      }
    }
  }
  ImGui::End();
  return chosen;
}
```

Step 3: In src/app/main.cpp, in drawPanels, replace the three lines from `tpj::ToolKind kind = tool.Kind;` through the closing brace of its if with:

```cpp
  if (const std::optional<tpj::ToolKind> kind = tpj::drawToolPanel(tool.Kind, !tool.Drawn.empty())) {
    tpj::selectTool(tool, *kind);
  }
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 10: Verify

Step 1: Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
Expected: no output.

Step 2: Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings, and `100% tests passed`.

Step 3: Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1; ctest.exe --preset windows-debug 2>&1 | tail -3`
Expected: the build finishes, and `100% tests passed`.

Step 4: Run: `scripts/cross-build-check.sh`
Expected: it passes.

Step 5: Run: `build/linux-debug/ThemeParkJones --park tests/parks/sketch.park --capture build/linux-debug/sketch.bmp`, convert the capture to PNG, and view it.
Expected: the Tools panel lists Look, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete.

Step 6: Dispatch the reviewer agent on the staged diff via the reviewing skill, with FEATURE.md, src/tools/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, and docs/principles.md.
Expected: Evan decides on each finding.

Step 7: Ask Evan to run build/windows-debug/ThemeParkJones.exe --park tests/parks/sketch.park and check criterion 8.
Expected: Evan confirms it.

### Task 11: Commit

Step 1: Commit via the commit-hygiene skill, staging the paths this plan and the test pass touched by name, never image.png, with the message:

```
Tools: Draw paths by clicking points, snapping onto paths
```

and a body saying that the path tools draw a guest or backstage path through clicked points, each snapped onto a same-kind ground line within 2 m, that clicking the last point again finishes with exactly the ghost's AddPath, and that choosing a tool or Cancel path drops the drawing, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

Run: `git log -1 --format=%s`
Expected: `Tools: Draw paths by clicking points, snapping onto paths`
