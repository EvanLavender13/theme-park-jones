# Implementation Plan: Clean Junctions

## Goal

Path ends meet with round corners and no notch, and guest ribbons draw over backstage ribbons where they cross, without flicker.

## Approach

The internal appendRibbon and appendJoint in src/render/park_mesh.cpp take the height they lie at as a parameter instead of reading PATH_LIFT, and appendPath and appendWalkway pass pathLift(kind). appendPath then adds appendJoint beyond its ground line's first point, facing back along the first segment, and beyond its last point, as appendWalkway already does at its end. The header replaces PATH_LIFT with pathLift(kind) and renames WALKWAY_JOINT_SEGMENTS to JOINT_SEGMENTS.

## Tasks

### Task 1: Render spec, contract and park mesh

Files:
- Modify: `src/render/SPEC.md:11`
- Modify: `src/render/SPEC.md:25-29`

Step 1: Make the Contract and Park mesh edits FEATURE.md's Spec changes gives: the setParkMesh paragraph's last sentence; the ribbon paragraph's first sentence, followed by the two new paragraphs (lifts, then the round joint) after the ribbon paragraph; the appendPath paragraph replaced; and the two walkway paragraph edits.

Step 2: Check

Run: `grep -c "PATH_LIFT\|WALKWAY_JOINT_SEGMENTS" src/render/SPEC.md; grep -c "pathLift" src/render/SPEC.md`
Expected: `0`, then `3`.

### Task 2: Render spec, ghosts

Files:
- Modify: `src/render/SPEC.md` (the Ghosts section's appendPath and buildGhostMesh paragraphs)

Step 1: Make the two Ghosts edits FEATURE.md's Spec changes gives.

Step 2: Check

Run: `grep -c "ribbon and joints" src/render/SPEC.md`
Expected: `2`

### Task 3: Declarations

Files:
- Modify: `src/render/park_mesh.h:40-41, 65-83`
- Modify: `src/render/park_mesh.cpp:75-116`

Step 1: In park_mesh.h, replace lines 40 and 41 (`// A path's ribbon lies this far above the ground.` and `inline constexpr float PATH_LIFT = 0.02f;`) with:

```cpp
// A path's ribbon lies this far above the ground: guest paths above backstage ones, by as much as
// backstage ones lie above the terrain, so crossing ribbons of the two kinds never tie in depth.
constexpr float pathLift(PathKind kind) { return kind == PathKind::Guest ? 0.04f : 0.02f; }
```

Step 2: In park_mesh.h, replace the appendPath comments:

```cpp
// Adds the path's flat ribbon along its ground line at pathLift(kind), with a round joint beyond
// each end, or nothing when the line is empty.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points);
// The same path in the color given.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points, Rgba color);

// A round joint beyond a ribbon's end is a half disc of this many segments.
inline constexpr uint32_t JOINT_SEGMENTS = 16;
```

and in the first appendWalkway comment, change `the ribbon along the points, pathWidth(kind) wide, in the color, and` to `the ribbon along the points, pathWidth(kind) wide at pathLift(kind), in the color, and`, rewrapping to 100 columns.

Step 3: In park_mesh.cpp, keep the build compiling against the new header without changing behavior yet: replace every `WALKWAY_JOINT_SEGMENTS` with `JOINT_SEGMENTS`, and every `PATH_LIFT` with `pathLift(PathKind::Backstage)`. Task 5 replaces the latter.

Run: `grep -c "PATH_LIFT\|WALKWAY_JOINT_SEGMENTS" src/render/park_mesh.h src/render/park_mesh.cpp`
Expected: `src/render/park_mesh.h:0` and `src/render/park_mesh.cpp:0`.

Step 4: Build the library, not the tests, which still name the old constants until the test pass.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug --target tpj_render 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 4: Test pass

Run the test pass per implementing-features, with FEATURE.md at `plans/effortless-building/sketch-a-park/clean-junctions/FEATURE.md`, specs `src/render/SPEC.md`, `src/sim/park/SPEC.md`, `src/sim/routes/SPEC.md`, and `src/sim/medium/SPEC.md`, and public headers `src/render/park_mesh.h`, `src/sim/park/geometry.h`, `src/sim/park/intent.h`, `src/sim/park/edits.h`, `src/sim/routes/networks.h`, `src/sim/medium/network.h`, and `src/sim/world.h`. The existing tests in `tests/render/park_mesh_test.cpp` and `tests/render/ghost_mesh_test.cpp` that name PATH_LIFT, WALKWAY_JOINT_SEGMENTS, or appendPath's vertex and index counts are the test pass's to update.

### Task 5: Lift per kind

Files:
- Modify: `src/render/park_mesh.cpp:75-116, 175-200`

Step 1: Give appendRibbon a lift parameter. Its comment and signature become:

```cpp
// Adds the flat ribbon of the width along the line, the lift above the ground. The line has at
// least two points, each distinct from the next.
void appendRibbon(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double width, float lift,
                  Rgba color) {
```

and its two addVertex calls pass `lift` where they pass `pathLift(PathKind::Backstage)`.

Step 2: Give appendJoint a lift parameter. Its comment and signature become:

```cpp
// Adds a flat half disc of the radius beyond a ribbon's end at the point, the lift above the
// ground, for the unit direction pointing away from the ribbon there: its center, then
// JOINT_SEGMENTS + 1 vertices on its rim, from the ribbon's corner on the direction's right around
// to the one on its left, and a triangle facing up from the center to each rim vertex and the next.
void appendJoint(ParkMesh &mesh, const CarrierPoint &point, ParkPoint direction, double radius,
                 float lift, Rgba color) {
```

and its two addVertex calls pass `lift`.

Step 3: In the five-argument appendWalkway, pass `pathLift(kind)` to both calls:

```cpp
  appendRibbon(mesh, points, pathWidth(kind), pathLift(kind), color);
```

```cpp
  appendJoint(mesh, points.back(), unitStep(points[points.size() - 2], points.back()),
              0.5 * pathWidth(kind), pathLift(kind), color);
```

Step 4: In the colored appendPath, pass the kind's lift:

```cpp
  appendRibbon(mesh, line, pathWidth(kind), pathLift(kind), color);
```

Step 5: Build and run the render tests.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests`
Expected: no warnings. The tests of criteria 1, 5, and 6 pass, and those of the walkway's joint, flush start, and appendWalkways pass. The tests of appendPath's joints (criteria 3 and 4), and any that count appendPath's vertices or indices, still fail.

### Task 6: Round joints at both ends of a path

Files:
- Modify: `src/render/park_mesh.cpp:175-181`

Step 1: The colored appendPath becomes:

```cpp
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points, Rgba color) {
  const std::vector<CarrierPoint> line = groundLine(points);
  if (line.empty()) {
    return;
  }
  const double width = pathWidth(kind);
  const float lift = pathLift(kind);
  appendRibbon(mesh, line, width, lift, color);
  appendJoint(mesh, line.front(), unitStep(line[1], line[0]), 0.5 * width, lift, color);
  appendJoint(mesh, line.back(), unitStep(line[line.size() - 2], line.back()), 0.5 * width, lift,
              color);
}
```

A non-empty ground line has at least two points, each distinct from the next, so both unitSteps are defined.

Step 2: Build and run the render tests.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests`
Expected: no warnings, and every test passes.

### Task 7: Confirm

Step 1: Full verification

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no warnings, `100% tests passed`.

Run: `cmake.exe --build --preset windows-debug && scripts/cross-build-check.sh`
Expected: both succeed.

Step 2: Capture on Windows, since Linux captures hang under WSLg.

Run: `timeout 60 build/windows-debug/ThemeParkJones.exe --park tests/parks/sketch.park --capture build/windows-debug/junctions.bmp`
Expected: exit 0. Convert the BMP to PNG in the scratchpad and look at it: every path's free end is round, and the paths and walkways are otherwise as before.

Step 3: Check criterion 8 by hand in the running app: two guest paths drawn to meet end to end at an angle join with a round outer corner, and a guest path drawn across a backstage path draws over it without flicker while orbiting and zooming from near to far. This is Evan's to confirm.

### Task 8: Review and commit

Step 1: Review per implementing-features step 7, with FEATURE.md, src/render/SPEC.md, and docs/principles.md as context.

Step 2: Commit once via commit-hygiene, staging the paths the feature changed (never parks/sketch.park), with the subject `Render: Round path ends and lift guest paths over backstage`.
