# Implementation Plan: Frontage Snap

## Goal

Make Place shop and Place depot turn a box to face a nearby segment of the kind its front door serves, with its front face FRONTAGE_SETBACK beyond the paving, and turn that off while Ctrl is held.

## Approach

A new public tools query, frontagePose, finds the nearest place on each segment's ground line and picks the segment whose paving lies nearest the pointer, within the box's reach. It returns the box's pose there. The tool finds the frontage whenever the pointer moves while nothing is held, keeps it in ToolState as resnap keeps the path snap, and uses it for the ghost and the press. The app needs no change, because movePointer already carries SnapsOff.

## Placement

Decision 0027 places each behavior this feature adds:

- Finding a fronting box's pose from the published segments: tools, a new file src/tools/frontage.cpp, declared in src/tools/tools.h. It is public so the slice's integration test can pose a shop by it. It has its own file because it is one concern apart from the tool state machine, as snapping is. It uses roundedAt from src/tools/internal/snapping.h, the tools' private helper for rounded corners.
- Keeping the frontage in effect and acting on it for the ghost, the press, and the drag: tools, tools.h and tools.cpp, which own ToolState and the place tools.

## Tasks

### Task 1: Save the scenarios baseline

Before any change, with the windows-debug build of main at 0756d54 or later in build/windows-debug.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null > build/frontage-base.txt; wc -l < build/frontage-base.txt`
Expected: `15336`

### Task 2: Update the spec

Files:
- Modify: `src/tools/SPEC.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

### Task 3: Declare the interface

Files:
- Modify: `src/tools/tools.h`
- Create: `src/tools/frontage.cpp`
- Modify: `src/tools/CMakeLists.txt`

Step 1: In tools.h, after DOOR_CLEARANCE, add:

```cpp
// A box that fronts a segment has its front face this far beyond the segment's paving, in meters.
inline constexpr double FRONTAGE_SETBACK = 1.0;
```

Step 2: In ToolState, after Landing, add:

```cpp
  // The place tools' box pose while it fronts a segment, found when the pointer moves while not
  // holding, and kept through a hold. None while it fronts none.
  std::optional<Pose> Fronted;
```

Step 3: After sideAt's declaration, add:

```cpp
// The pose of a box of the kind that fronts the segment near the point, as tools/SPEC.md's Place
// shop and Place depot state, or none when no segment of the kind its front door serves is near.
std::optional<Pose> frontagePose(const World &world, BoxKind kind, ParkPoint point);
```

Step 4: Create src/tools/frontage.cpp, including tools/tools.h, tools/internal/snapping.h, sim/park/intent.h, and sim/park/geometry.h for MIN_SEGMENT_LENGTH, with a frontagePose that returns std::nullopt. Leave the stub's parameters unnamed, because -Wextra makes an unused parameter an error.

Step 5: Add frontage.cpp to tpj_tools's sources in src/tools/CMakeLists.txt, before snapping.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 4: Find the fronting pose

Files:
- Modify: `src/tools/frontage.cpp`

Step 1: In an anonymous namespace, add a function that gives the place on a ground line nearest a point: the place, its distance from the point, the unit direction of the piece it lies on, and whether it is the line's first or last point. Walk the pieces in order. On each, clamp the projection's fraction to 0 to 1, and keep a piece only when its distance is strictly less than the best so far, so the earlier piece wins a tie. The place is the line's first point when the best is piece 0 at fraction 0, and its last point when the best is the last piece at fraction 1.

Step 2: Implement frontagePose:

1. The served kind is PathKind::Guest for BoxKind::Shop and PathKind::Backstage for BoxKind::Depot. The box's depth is boxSize(kind).Depth.
2. For each segment of parkSegments(world), in its order, which is key order: skip it if its kind isn't the served kind or its Line has fewer than two points. Find the nearest place on its Line. Skip it if the place is the line's first point and roundedAt(corners, segment.From) is null, or the line's last point and roundedAt(corners, segment.To) is null, with corners from parkCorners(world), read once before the loop. Also skip it if the distance is more than 0.5 * Width + FRONTAGE_SETBACK + depth.
3. Keep the segment whose distance less 0.5 * Width is strictly smallest, so the lower key wins a tie. Keep its place, distance, piece direction, and width.
4. If none is kept, return none. Otherwise, the direction out from the ground line is the pointer less the place, divided by the distance, when the distance is more than MIN_SEGMENT_LENGTH. Otherwise it is the piece direction turned left, (-Z, X).
5. Return the Pose whose position is the place plus the out direction times 0.5 * Width + FRONTAGE_SETBACK + 0.5 * depth, and whose facing is the out direction negated.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 5: Keep and use the frontage in the place tools

Files:
- Modify: `src/tools/tools.cpp`

Step 1: In selectTool, reset tool.Fronted.

Step 2: In resnap, after `tool.Snapped.reset();`, add a branch for the place tools that returns before the path snap. While not holding, set tool.Fronted to frontagePose(world, placedKind(tool.Kind), *tool.Pointer) when the pointer is on the ground and snapping is on, and to none otherwise. While holding, leave it as it is.

Step 3: In movePointer, in the place tools' branch, turn tool.Landing to face the drag only when tool.Fronted is none.

Step 4: In pressPointer, the place tools' branch sets tool.Landing to *tool.Fronted when it has a value, and to the pointer's pose with the tool's facing otherwise, as today.

Step 5: In tentativeEdit, while a place tool is not holding, return AddBox with *tool.Fronted when it has a value, before the pointer's pose.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 6: Check frontage freehand

Files: none in the repository. Write a scratch check under build/frontage-check, which git ignores.

Step 1: Write build/frontage-check/check.cpp, a small program that links tpj_tools. Open tests/parks/new.park, select PlaceShop, and move the pointer along hand-made paths with small random offsets of up to 0.3 m: toward the entrance path from each side, along it, across it, onto its paving, past its end, and away. At each step, print tentativeEdit's pose. Add a backstage path with AddPath and repeat with PlaceDepot, and check that PlaceShop ignores it. Add a guest path with a bend, round the bend with SetCornerRadius, and move along the outside and inside of the curve. Widen a segment to 12 m with SetSegmentWidth and check the front face's distance from the paving's edge. Press, drag 3 m, and release while fronted, and check the committed pose equals the pose before the press. Repeat with SnapsOff set.

Step 2: Have the program check each step and count failures. A fronting pose must face square to its segment's ground line at the nearest place, within 1e-9. Its front face's middle must lie 1 m beyond the paving, within 1e-9 m. Its side must be the pointer's side. Along the outside of the rounded bend, every step must front a segment. Past the path's free end, no step may front it. The press, drag, and release must commit the pose from before the press. With SnapsOff, no step may front. The program prints each failure, then a last line `failures: N`.

Step 3: Copy build/snap-check/build.sh to build/frontage-check/build.sh. It compiles the named .cpp with MSYS2's g++.exe against src/ and EnTT, and links build/windows-debug's libtpj_tools.a and libtpj_sim.a.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools tpj_sim && build/frontage-check/build.sh check && build/frontage-check/check.exe | tail -1`
Expected: `failures: 0`

### Task 7: Confirm the criteria

Step 1: Build, test, and tidy.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -3`
Expected: `0`, then `100% tests passed, 0 tests failed out of 107`, then `tidy: clean.`

Step 2: Compare the scenarios against the baseline (criterion 5).

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null > build/frontage-branch.txt; build/windows-debug/tpj_scenarios.exe --compare build/frontage-base.txt build/frontage-branch.txt; echo $?`
Expected: `0`

Step 3: Build windows-release on the branch and on main, and compare the frame cost of Place shop on full.park (criterion 6). Use a baseline worktree for main.

Run: `git worktree add ../TPJ-baseline main && cmake.exe --preset windows-release && cmake.exe --build --preset windows-release && (cd ../TPJ-baseline && cmake.exe --preset windows-release && cmake.exe --build --preset windows-release)`
Expected: both builds finish with no warnings.

Step 4: Play criteria 1 to 4 in build/windows-release/ThemeParkJones.exe by hand, as each one says. Then compare the frame time on full.park in both builds with the same steps.

Step 5: Remove the baseline worktree.

Run: `git worktree remove ../TPJ-baseline`
Expected: no output.

### Task 8: Review and commit

Step 1: Stage only this feature's paths: src/tools/frontage.cpp, src/tools/tools.h, src/tools/tools.cpp, src/tools/CMakeLists.txt, src/tools/SPEC.md, and the frontage-snap plan directory. Dispatch the reviewer with the reviewing skill, and present its findings.

Step 2: Commit once with the commit-hygiene skill, subject `Tools: Turn placed boxes to face the path they front`.
