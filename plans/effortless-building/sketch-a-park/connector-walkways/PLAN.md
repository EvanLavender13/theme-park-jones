# Implementation Plan: Connector Walkways

## Goal

The park mesh and every accepted edit's ghost draw the networks' connectors as walkway ribbons of their kind's width and color.

## Approach

appendPath's ribbon code moves into an internal appendRibbon over a CarrierPoint line and a width, which appendPath and a new appendWalkway both call. appendWalkways walks parkNetwork's carriers for each kind and draws those whose first stop's node is anchored. buildParkMesh calls it between paths and boxes, and buildGhostMesh appends it for the candidate world of an accepted edit. The app's code is unchanged; only its spec says why.

## Tasks

### Task 1: Render spec, park mesh

Files:
- Modify: `src/render/SPEC.md:23-29`

Step 1: Make the three Park mesh edits FEATURE.md's Spec changes gives: the first paragraph's reading sentence, the appendPath paragraph replaced by the three paragraphs given (ribbon, appendPath, connectors and walkways), and the buildParkMesh order.

Step 2: Check

Run: `grep -c "appendWalkways" src/render/SPEC.md`
Expected: `2`

### Task 2: Render spec, ghosts

Files:
- Modify: `src/render/SPEC.md:37`

Step 1: Replace the buildGhostMesh paragraph with the one FEATURE.md's Spec changes gives.

Step 2: Check

Run: `grep -c "appendWalkways" src/render/SPEC.md`
Expected: `3`

### Task 3: App spec

Files:
- Modify: `src/app/SPEC.md:11`

Step 1: In the Park section, after "it builds the park mesh and gives it to the renderer.", insert the sentence FEATURE.md's Spec changes gives.

### Task 4: Declarations and stubs

Files:
- Modify: `src/render/park_mesh.h:68-75`
- Modify: `src/render/park_mesh.cpp:104`

Step 1: In park_mesh.h, after the second appendPath declaration, add:

```cpp
// Adds a connector's walkway: the ribbon along the points, pathWidth(kind) wide, in the color. The
// points are each distinct from the next, as a connector's are. Nothing for fewer than two.
void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   Rgba color);

// Adds a walkway for each connector of the world's guest network and then its backstage network,
// in the kind's path color with the alpha given. A connector is a carrier whose first stop's node
// is anchored.
void appendWalkways(ParkMesh &mesh, const World &world, float alpha);
```

Change the buildParkMesh comment to `// The world's entrances, paths, walkways, and boxes, in that order and each in key order.` and the buildGhostMesh comment's second line to `// not, and DELETE_TINT for a deletion, followed for an accepted edit by its candidate's walkways.` with the first line unchanged.

Step 2: In park_mesh.cpp, before appendBox's definition, add stubs:

```cpp
void appendWalkway(ParkMesh & /*mesh*/, PathKind /*kind*/,
                   const std::vector<CarrierPoint> & /*points*/, Rgba /*color*/) {}

void appendWalkways(ParkMesh & /*mesh*/, const World & /*world*/, float /*alpha*/) {}
```

Step 3: Build

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 5: Test pass

Run the test pass per implementing-features, with FEATURE.md at `plans/effortless-building/sketch-a-park/connector-walkways/FEATURE.md`, specs `src/render/SPEC.md`, `src/app/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/medium/SPEC.md`, `src/sim/park/SPEC.md`, and public headers `src/render/park_mesh.h`, `src/sim/routes/networks.h`, `src/sim/medium/network.h`, `src/sim/world.h`, `src/sim/park/edits.h`.

### Task 6: Extract the ribbon

Files:
- Modify: `src/render/park_mesh.cpp:65-102`

Step 1: In the anonymous namespace, after ghostColor, add appendRibbon, holding appendPath's body from its `first` onward with `line`, `width`, and `color` as parameters:

```cpp
// Adds the flat ribbon of the width along the line, PATH_LIFT above the ground. The line has at
// least two points, each distinct from the next.
void appendRibbon(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double width,
                  Rgba color) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  const double half = 0.5 * width;
  for (size_t i = 0; i < line.size(); ++i) {
    const ParkPoint tangent = tangentAt(line, i);
    const ParkPoint right{-tangent.Z, tangent.X};
    addVertex(mesh, line[i].X - right.X * half, PATH_LIFT, line[i].Z - right.Z * half, {}, 1.0f,
              color);
    addVertex(mesh, line[i].X + right.X * half, PATH_LIFT, line[i].Z + right.Z * half, {}, 1.0f,
              color);
  }
  for (uint32_t i = 0; i + 1 < line.size(); ++i) {
    const uint32_t left0 = first + 2 * i;
    const uint32_t right0 = left0 + 1;
    const uint32_t left1 = left0 + 2;
    const uint32_t right1 = left0 + 3;
    mesh.Indices.insert(mesh.Indices.end(), {left0, right0, left1, left1, right0, right1});
  }
}
```

Step 2: Make the colored appendPath:

```cpp
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points, Rgba color) {
  const std::vector<CarrierPoint> line = groundLine(points);
  if (line.empty()) {
    return;
  }
  appendRibbon(mesh, line, pathWidth(kind), color);
}
```

Step 3: Verify

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#park_mesh_test]"`
Expected: no build output, and every appendPath test passes.

### Task 7: Walkways

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Add `#include "sim/command_queue.h"`, `#include "sim/medium/network.h"`, and `#include "sim/routes/networks.h"` after `#include "render/park_mesh.h"`, in a block separated by a blank line, sorted.

Step 2: Replace the stubs:

```cpp
void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   Rgba color) {
  if (points.size() < 2) {
    return;
  }
  appendRibbon(mesh, points, pathWidth(kind), color);
}

void appendWalkways(ParkMesh &mesh, const World &world, float alpha) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const Network &network = parkNetwork(world, kind);
    const Rgba base = pathColor(kind);
    const Rgba color{base.R, base.G, base.B, alpha};
    for (const Carrier &carrier : network.carriers()) {
      // Each connector's door node is anchored, and nothing else is (sim/routes/SPEC.md).
      if (!carrier.Stops.empty() && network.nodeAnchor(carrier.Stops.front().Node) != NULL_KEY) {
        appendWalkway(mesh, kind, carrier.Points, color);
      }
    }
  }
}
```

Step 3: Verify

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#park_mesh_test]"`, and the same filtered to each file the test pass created under tests/render/.
Expected: no build output. The tests of criterion 1 (appendWalkway's ribbon) pass. Those of criterion 2 (the joint) fail until Task 7b, those of criteria 3 and 4 (the flush start, and appendWalkways) until Task 7c, and those of criteria 5 and 6 (walkways in buildParkMesh and in ghosts) may fail until Tasks 8 and 9.

### Task 7b: The walkway's round joint

Files:
- Modify: `src/render/SPEC.md`, `src/render/park_mesh.h`, `src/render/park_mesh.cpp`

Step 1: Add the round joint sentence FEATURE.md's Spec changes gives to the walkway paragraph of src/render/SPEC.md, and WALKWAY_JOINT_SEGMENTS, 16, to park_mesh.h beside appendWalkway.

Step 2: Rerun the test pass for the revised criteria 1 and 2.

Step 3: In park_mesh.cpp, add appendJoint in the anonymous namespace, taking the last point p, the last segment's unit direction t, and half the width h. It adds the center at p and WALKWAY_JOINT_SEGMENTS + 1 rim vertices at p + h * (cos(πk/N) * r + sin(πk/N) * t) with r = (-t.z, t.x), PATH_LIFT high with upward normals, and the triangles (center, rim k, rim k + 1) for k below N. appendWalkway calls it after appendRibbon with points.back() and unitStep of the last two points.

Step 4: Verify

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#park_mesh_test]"`
Expected: no build output. The tests of criteria 1 and 2 pass; those of criteria 3 and 4 fail until Task 7c, and those of criteria 5 and 6 may fail until Tasks 8 and 9.

### Task 7c: The walkway's flush start

Files:
- Modify: `src/render/SPEC.md`, `src/render/park_mesh.h`, `src/render/park_mesh.cpp`

Step 1: Add the flush-start sentences and the face normal in appendWalkways that FEATURE.md's Spec changes gives to src/render/SPEC.md, and declare in park_mesh.h, after appendWalkway, an overload `void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points, std::optional<ParkPoint> faceNormal, Rgba color);`, the four-argument one calling it with none.

Step 2: Rerun the test pass for the new criterion 3 and the revised criterion 4.

Step 3: In park_mesh.cpp, after appendRibbon in appendWalkway, when a normal is given, compute t = unitStep of the first two points, r = (-t.Z, t.X), and when dot(t, n) is not 0, move = h * dot(r, n) / dot(t, n); when |move| is less than the first segment's length, add t * move to the ribbon's first vertex and subtract it from its second. In appendWalkways, find the anchored entity's pose in parkEntrances (ENTRANCE_SIZE) or parkBoxes (boxSize), take footprintOf's Forward, and pass it.

Step 4: Verify

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#park_mesh_test]"`
Expected: no build output. The tests of criteria 1 to 4 pass; those of criteria 5 and 6 may fail until Tasks 8 and 9.

### Task 8: Walkways in the park mesh

Files:
- Modify: `src/render/park_mesh.cpp:126-138`

Step 1: In buildParkMesh, between the paths loop and the boxes loop, add `appendWalkways(mesh, world, 1.0f);`.

Step 2: Verify

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#park_mesh_test]"`
Expected: no build output, all pass.

### Task 9: Walkways in the ghost

Files:
- Modify: `src/render/park_mesh.cpp:170-190`

Step 1: At the end of buildGhostMesh, before `return mesh;`, add:

```cpp
  if (isAccepted(world, edit)) {
    CommandQueue queue;
    queueEdit(queue, edit);
    appendWalkways(mesh, makeCandidate(world, queue), GHOST_ALPHA);
  }
```

Step 2: Verify

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests`
Expected: no build output, all pass.

### Task 10: Confirm

Step 1: Full verification

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no warnings, `100% tests passed`.

Run: `cmake.exe --build --preset windows-debug && scripts/cross-build-check.sh`
Expected: both succeed.

Step 2: Capture on Windows, since Linux captures hang under WSLg.

Run: `timeout 60 build/windows-debug/ThemeParkJones.exe --park tests/parks/routes.park --capture build/windows-debug/walkways.bmp`
Expected: exit 0. Convert the BMP to PNG in the scratchpad and look at it: four walkways, the entrance's and the shop's front in the guest color, the shop's back and the depot's front in the backstage color, each from its door to its path.

Step 3: Check the ghost by hand in the running app: a shop ghost within 4 m of a guest path shows a translucent walkway, and one beyond reach shows none. This is Evan's to confirm.

### Task 11: Review and commit

Step 1: Review per implementing-features step 7, with FEATURE.md, src/render/SPEC.md, src/app/SPEC.md, src/sim/routes/SPEC.md, and docs/principles.md as context.

Step 2: Commit once via commit-hygiene, staging the paths the feature changed (never parks/sketch.park), with the subject `Render: Draw connectors as walkways`.
