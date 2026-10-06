# Implementation Plan: Width Tapers

## Goal

Where two segments of different widths meet, replace every step in the paving's outline with a smooth curve from the narrower segment's side into the wider one's, over a length held on the point and set with a handle.

## Approach

A path point gains a taper length, set by a new command, SetTaperLength. In surface.cpp, after a point's edges are fitted, each side that would step at a point of two different widths curves. The narrower segment's trim on that side moves out by the curve's length, and the outline runs a cubic from there into the wider segment's side. A held length of 0 means a natural length. layout.cpp's corner sharing leaves room for a held length where a side steps. parkTapers publishes each taper for the tools' new handle, and the app draws it in its own color.

## Changed during implementation

The shape changed three times after the user played it. What's built now:

- Tapers replace steps, and nothing else. In trimPoint, after the normal fit, each pair at a two-segment point of different widths that isn't a fillet would step. On that side, the narrower segment's trim moves the taper length farther out, and outlinePair draws a cubic from there into the wider segment's side (appendCurve). Sides that join already, like the inside of a bend, keep their fit.
- A held length of 0 means the natural length, twice how far the curve moves out (TAPER_SLOPE in surface.cpp). Edit path's handle stays at MIN_TAPER_LENGTH or more.
- The curve's end directions are scaled by how far it travels along each, so short tapers can't loop back.
- Points whose outline curves are cut into triangles, since a fan from the point can miss parts of a curved outline.
- layout.cpp's pavingReaches adds a held taper length to the narrower segment's reach, where a side steps.
- Earlier versions narrowed the wider segment, then widened the narrower one by refitting the whole bend at one width. The refit made knobs, a second curve and a sharp corner at bends, and could close off holes at sharp turns. Both were dropped.
- The app redrew the park only when entrances, segments, or boxes changed, so a committed taper wasn't drawn. scene_sync.cpp now compares path points too.
- Taper handles get their own color, TAPER_HANDLE_COLOR.
- parkTapers first lists the points that exactly two segments of different widths use, straight from the segments' intent. If there are none, it builds nothing, and otherwise it fits only those points and their segments' far ends.

## Placement

Decision 0027 places each behavior this feature adds:

- The taper length and its save field: sim/park, internal/components.h and intent.h, beside the corner radius, since it is point intent.
- SetTaperLength: sim/park, edits.h and edits.cpp, which plan and apply every path command.
- The curves that replace steps, their reach, and parkTapers: sim/park, surface.h and surface.cpp, which fit every edge and trim of the paving.
- A corner arc leaving room for a held taper length at its segment's other end: sim/park, layout.cpp, whose pavingReaches already gives each far end's paving need.
- The taper handle and its drag: tools, tools.h and tools.cpp, which own every Edit path handle.
- The taper handle's color: render, park_mesh.h, beside the other handle colors. The app's scene_uploads.cpp picks it for taper handles. That is composition only.
- Redrawing the park when a path point changes: app, scene/scene_sync.h and scene_sync.cpp, which decide which meshes a frame rebuilds from the intent they were built from.
- Random SetTaperLength edits: scenarios, park_edits.cpp, which draws every other command.

## Tasks

### Task 1: Update the specs

Files:
- Modify: `src/sim/park/SPEC.md`, `src/tools/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

### Task 2: Add the field and the interfaces, with stubs

Files:
- Modify: `src/sim/park/intent.h`, `src/sim/park/intent.cpp`, `src/sim/park/internal/components.h`
- Modify: `src/sim/park/surface.h`, `src/sim/park/surface.cpp`, `src/sim/park/edits.h`, `src/sim/park/edits.cpp`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`
- Modify: every `.park` file in `tests/parks/`, `tests/parks/stress/`, and `parks/`

Step 1: In intent.h, after DEFAULT_EDGE_RADIUS, add `inline constexpr double DEFAULT_TAPER_LENGTH = 0.0;` with the comment "The taper length a new path point takes, in meters: a step." Add `double Taper = DEFAULT_TAPER_LENGTH;` to ParkPathPoint after Radius, and add to its comment "and the length the wider of two segments narrows over where they meet straight on there".

Step 2: In internal/components.h, add `double Taper = DEFAULT_TAPER_LENGTH;` to PathPointIntent after Radius, with the same comment change, and `visitor.field("taper", point.Taper);` last in its visitFields. In intent.cpp, parkPathPoints gives `ParkPathPoint{key, point.At, point.Radius, point.Taper}`.

Step 3: In surface.h, after edgeRadiusFor, add:

```cpp
// Where two segments with ground lines are the only ones at a path point, run away from it in
// opposite directions, and differ in width: the point's key and position, the wider segment's key,
// the unit direction along it away from the point, the taper length it narrows over, and the
// largest that fits.
struct ParkTaper {
  EntityKey Point = NULL_KEY;
  ParkPoint At;
  EntityKey Segment = NULL_KEY;
  ParkPoint Along;
  double Length = 0.0;
  double MaxLength = 0.0;

  bool operator==(const ParkTaper &) const = default;
};

// Each taper, in ascending key order of its point.
std::vector<ParkTaper> parkTapers(const World &world);
```

Stub parkTapers in surface.cpp to return none.

Step 4: In edits.h, after SetEdgeRadius, add SetTaperLength with the comment "The path point's taper length, held exactly as given":

```cpp
struct SetTaperLength {
  EntityKey Point = NULL_KEY;
  double Length = 0.0;

  bool operator==(const SetTaperLength &) const = default;
};
```

Add it last to ParkEdit, and declare its isAccepted and applyCommand overloads. Change "eleven" to "twelve" in ParkEdit's comment, and add SetTaperLength to addParkEdits' comment. In edits.cpp, register it last in addParkEdits. Stub both overloads: isAccepted returns false, and applyCommand does nothing. Add a planOf overload that returns none, so editedOf's visit compiles.

Step 5: In tools.h, add Taper last to HandleKind. Say in EditHandle's comment that a taper handle is keyed by its point and the wider segment, at At. After RADIUS_HANDLE_DEPTH, add `inline constexpr double TAPER_HANDLE_DEPTH = 3.0;` with the comment "Edit path keeps a taper handle at least this far from its point, in meters, unless the taper fits less." After radiusHandleOf, declare `ParkPoint taperHandleOf(const ParkTaper &taper);` with the comment "Where the taper's handle lies." Stub it in tools.cpp to return taper.At.

Step 6: Add the field, at its default, to every saved point line in the checked-in parks and in the user's parks. Leave the user's parks unstaged.

Run: `sed -i -E 's/^([0-9]+ x=[^ ]+ z=[^ ]+ radius=[^ ]+)$/\1 taper=0/' tests/parks/*.park tests/parks/stress/*.park parks/*.park`

Step 7: Build everything and run the sim tests.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/tpj_sim_tests.exe`
Expected: builds with no warnings, and all tests pass.

Step 8: The new field enters every world's hash, so main's scenario output can't be compared with this feature's. Save this state's output as the baseline for Task 9.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null | grep -v '^scenario park-edits ' > build/taper-base.txt`
Expected: the command exits 0.

### Task 3: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/sim/park/SPEC.md, and the public headers src/sim/park/edits.h, src/sim/park/intent.h, and src/sim/park/surface.h. Its chosen edit fails until Task 4 is done, because the stub refuses SetTaperLength.

### Task 4: Apply SetTaperLength

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add `EntityKey Tapered = NULL_KEY;` and `double TaperedTo = 0.0;` to EditPlan after EdgedTo, and say in its comment that if Tapered is not NULL_KEY, that existing point's taper length becomes TaperedTo. Add planTaper after planRadius, shaped like it: none unless the key holds a path point and the length is finite and at least 0, and otherwise a plan with Tapered and TaperedTo set. planOf for SetTaperLength returns it. In applyPlan, after the Rounded write, write `store.point(plan.Tapered).Taper = plan.TaperedTo` when Tapered is set. Fill in isAccepted and applyCommand like SetCornerRadius's.

Step 2: A taper length changes no segment, so pointsChanged doesn't see it. In editedBy, after pointsChanged, add plan.Tapered to points when it is set, and sort and dedupe them, before withFarEnds.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: builds with no warnings, and all tests pass.

### Task 5: Leave room for a held taper at a corner's far end

Files:
- Modify: `src/sim/park/layout.cpp` (pavingReaches)

Step 1: Add stepsBetween, which says whether the outline would step between two arms with all the room it needs: they run straight on, or an outer edge can't touch both sides between the point's own corner and twice the wider half width behind the point. Add taperArm, which gives the narrower arm's index where two arms of different widths meet, a side steps, and the point holds a taper length above 0. In pavingReaches, add the point's taper length to that arm's reach.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: builds with no warnings, and all tests pass.

### Task 6: Curve each step

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: Add a TaperFit record to Scratch: whether the point tapers, the narrower end's index in the group, which of the two pairs curve, each curve's length, how far the narrower sides would stop without curves, the longest curve, and the longest any curve can be. Add the constants TAPER_SLOPE = 2.0 and MAX_CURVE_PIECES = 64.

Step 2: Give trimPoint the point's held taper length, made 0 when not finite or below zero. After the normal fit at a point with two ends of different half widths, for each pair that isn't a fillet: find the narrower end's side in it, its room (its reach on that side less its trim), and how far the wider segment's side, where it begins, lies out from the narrower side's line. The curve's length is the held length, or TAPER_SLOPE times that distance when the held length is 0, at most the room and at least 0. Move the narrower end's trim on that side out by the length, and keep it as Asked when asking.

Step 3: Add appendCurve, the cubic from one place to another that leaves along one unit direction and arrives along another, each scaled by how far the far place lies along it, and never less than 0. Cut it into n pieces, with n from the bound M / (8n²) ≤ ARC_TOLERANCE, where M is the larger of the second derivatives at its two ends, at most MAX_CURVE_PIECES. In outlinePair, a pair that curves draws this curve from the first segment's trimmed left corner, leaving toward the point, to the second's trimmed right corner, arriving along it.

Step 4: In buildPoint, cut a point whose outline curves into triangles with triangulate, owned by the point, or by the corner's two segments as before at a corner.

Step 5: parkTapers first lists the points that exactly two segments of different widths use, straight from the segments' intent. If there are none, it builds nothing. Otherwise it fits only those points and their segments' far ends, and pushes a ParkTaper for each that tapers.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/ThemeParkJones.exe`
Expected: builds with no warnings. Where widths differ, nothing steps.

### Task 7: Add the taper handle

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/render/park_mesh.h`, `src/app/scene/scene_uploads.cpp`, `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`

Step 1: taperHandleOf returns the point `max(taper.Length, min(TAPER_HANDLE_DEPTH, taper.MaxLength))` from taper.At along taper.Along. handleAt and editHandles add each taper's handle after the edge handles. A press on it takes hold with GrabFrom = taper.Length − along(pointer). Dragging gives `SetTaperLength{point, clamp(along(pointer) + grabFrom, MIN_TAPER_LENGTH, max(MaxLength, MIN_TAPER_LENGTH))}`, with MIN_TAPER_LENGTH = 0.5 in tools.h.

Step 2: Add TAPER_HANDLE_COLOR {0.30f, 0.85f, 0.90f, 0.8f} to park_mesh.h, and draw taper handles in it in scene_uploads.cpp. In scene_sync, compare parkPathPoints too when deciding whether the park mesh needs rebuilding.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/ThemeParkJones.exe`
Expected: builds with no warnings. Criteria 2 to 5 play as FEATURE.md describes.

### Task 8: Draw random taper lengths in the scenarios

Files:
- Modify: `src/scenarios/park_edits.cpp`

Step 1: Add SetTaperLength to EDIT_WEIGHTS' comment and a weight of 1 last in the array, which becomes 12 long. In queueEdit, kind 10 pushes drawEdgeRadius, and the last branch pushes `SetTaperLength{point, drawEdit(world, index) * 12.0}` for a drawn given key, with the comment "Lengths up to 12 m, so some don't fit and are made smaller."

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe | grep -c "scenario park-edits tick"`
Expected: `3001`

### Task 9: Confirm the criteria

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, `100% tests passed`, and `tidy: clean.`

Step 2: Compare the scenarios without park-edits with Task 2's baseline, for criterion 6.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null | grep -v '^scenario park-edits ' > build/taper-branch.txt; build/windows-debug/tpj_scenarios.exe --compare build/taper-base.txt build/taper-branch.txt; echo $?`
Expected: `0`, with no difference printed.

Step 3: Build windows-release here, and in a worktree of main at ../TPJ-baseline. Open tests/parks/stress/full.park in each, pick Edit path, and compare the Debug panel's frame times for criterion 7. Leave the release build here for the user to play.

Step 4: Play criteria 2 to 5 in the release build, and report what each showed.

### Task 10: Review and commit

Step 1: Review the staged diff as implementing-features describes. Stage only this feature's paths, never `git add -A`. Leave image.png and parks/*.park unstaged.

Step 2: Commit with commit-hygiene, as `Park: Narrow a wider segment where widths meet straight on`.
