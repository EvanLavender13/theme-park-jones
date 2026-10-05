# Implementation Plan: Paved Surface

## Goal

Derive the paved surface in sim/park, refuse boxes against it, and draw it, with the ghost drawn the same way.

## Approach

A new file, src/sim/park/surface.cpp, builds the surface from a PathLayout: each segment's trims at both ends, then its body, corner halves, junction fans, and edge lines. Validity tests footprints against the pieces with the separating axis test the box check already uses. The edit check compares the surface before and after an edit, grouped by owner, and checks only the owners that changed. The renderer fills the pieces and strokes the edge lines.

## Placement

Decision 0027 places each behavior this feature adds:

- Building the surface: sim/park, src/sim/park/surface.h and surface.cpp. sim/park owns path intent and its derived shapes, and validity and the renderer must read one shape.
- Refusing footprints against the surface: sim/park, edits.cpp, where the validity check already lives.
- The surface an edit changes: sim/park, editedSurface in edits.h, beside editedSegments, because both need the edit's plan.
- Drawing the surface and its edge lines: render, park_mesh.h and park_mesh.cpp, which already draw the park and ghosts.

## Tasks

### Task 1: Update the specs

Files:
- Modify: `src/sim/park/SPEC.md`
- Modify: `src/render/SPEC.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

### Task 2: Add the public interface with stubs

Files:
- Create: `src/sim/park/surface.h`, `src/sim/park/surface.cpp`
- Modify: `src/sim/CMakeLists.txt` (add `park/surface.cpp`), `src/sim/park/edits.h`, `src/sim/park/edits.cpp`

Step 1: In surface.h, declare these, with a comment on each stating the spec's behavior:

```cpp
struct SurfacePiece {
  PathKind Kind = PathKind::Guest;
  EntityKey Owner = NULL_KEY;           // its segment, or a junction's point
  std::vector<ParkPoint> Corners;       // convex, counter-clockwise seen from above
  bool operator==(const SurfacePiece &) const = default;
};
struct EdgeLine {
  PathKind Kind = PathKind::Guest;
  EntityKey Owner = NULL_KEY;           // the point it runs around
  std::vector<ParkPoint> Points;        // paving on the left, seen from above
  bool operator==(const EdgeLine &) const = default;
};
struct PavedSurface {
  std::vector<SurfacePiece> Pieces;
  std::vector<EdgeLine> Edges;
};
PavedSurface parkSurface(const World &world);
```

Step 2: In edits.h, declare `PavedSurface editedSurface(const World &world, const ParkEdit &edit);`. Give both functions stub definitions that return an empty surface.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: builds with no warnings.

### Task 3: Build bodies, square ends, and edge lines along straight paths

Files:
- Modify: `src/sim/park/surface.cpp`, `src/sim/park/internal/layout.h`

Step 1: Declare `PavedSurface surfaceOf(const PathLayout &layout);` in internal/layout.h, and define parkSurface as `surfaceOf(layoutOf(world))`.

Step 2: In surfaceOf, skip segments whose ground line is empty, using segmentsOf. For each remaining segment, keep a trim at each end and on each side: the distance from that end's point, along the segment's straight line, where its body stops. Every trim starts at 0.

Step 3: Emit each segment's body as one four-corner piece between its trims. At a point one segment uses, emit an edge line that starts on the segment's left side at its middle, runs to the point, crosses the end square, and runs back along the right side to the middle. Here a segment's middle is half its straight length from either point, and left and right are taken walking away from the point.

Step 4: Build tpj_sim.

### Task 4: Build corner halves

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: For each corner from cornersOf, set the outer trims of both segments to the arc's tangent length at the corner's radius. Build each segment's half from its tangent point to the arc's middle as two arcs about the corner's center: the outer at the radius plus the half width, and the inner at the radius minus it. Halve both arcs together, as appendArc does, until the outer one lies within ARC_TOLERANCE, so the two arcs have matching points. At radius 0 the center is the point, so the outer edge is an arc of half the width around it.

Step 2: If the radius is less than either half width, intersect the two inner edge lines. Set each segment's inner trim to that point's distance along it, at most half the segment's straight length. Collapse that half's inner points onto the segment's inner trim point.

Step 3: Emit each half as a strip of convex pieces, one for each two neighboring pairs of matching inner and outer points, owned by the segment. Emit two edge lines owned by the point:
- The outer one runs from the first segment's outer middle, along its outer edge and both outer arc halves, to the second segment's outer middle.
- The inner one runs the other way, from the second segment's inner middle to the first's, so the paving lies on its left too.

Where the widths differ, both lines step at the arc's middle.

Step 4: Build tpj_sim. Run the app on tests/parks/stress/winding-path.park with `--capture` and look at the bends.

### Task 5: Build junctions

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: At each point that two or more segments use and that makes no corner, sort the segments by the direction of their straight lines away from the point. Sort counter-clockwise from +x, comparing quadrant and then the cross product, with no trigonometry.

Step 2: For each segment and the next one around the point, find the gap from the first's direction to the second's.

- If the gap is less than half a turn, fit a fillet of radius r, half the narrower width. Its center lies at r plus each half width from each straight line, on the gap's side. Each touching point's distance along its segment is linear in r. Reduce r so that both distances are at most half their segment's straight length, or to 0 if needed. If they still don't fit at r = 0, mark the pair as stepping.
- Otherwise the pair goes around the point.

Raise each segment's trims, on both sides, to the farthest touching distance among its fillets.

Step 3: Emit the junction as triangles fanned from the point, owned by the point. Go around the point:
- across each segment's trim line;
- back along its left edge to the fillet's touching point;
- along the fillet, or around the point for a pair that goes around it (built as in Step 4 below), or straight across at the point for a stepping pair;
- out along the next segment's right edge to its trim.

Skip triangles with no area.

Step 4: Build the outline around the point without trigonometry. Start with the two edge directions, from the point to each edge at distance 0. Halve the turn between them with normalized vector sums, as arcMiddle does, until each piece lies within ARC_TOLERANCE of the curve at the larger half width. If the turn is half a turn or more, first split it at the direction perpendicular to the middle of the gap. Set each point's distance from the point by its fraction of the way around, from one half width to the other.

Step 5: Emit one edge line per pair, owned by the point. It runs from the first segment's left middle, around the pair's inside corner, to the next segment's right middle.

Step 6: Build tpj_sim.

### Task 6: Refuse footprints against the surface

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Replace `meets(const Line &, const Solid &)` with `meets(const SurfacePiece &, const Solid &)`. Use the separating axis test over the footprint's two axes and the piece's edge normals, skipping edges shorter than MIN_SEGMENT_LENGTH. Count an overlap only when it is more than CONTACT_TOLERANCE on every axis, as overlapAlong does.

Step 2: The two `fits` functions and isPhysicallyValid take the surface's pieces where they took lines. Keep leavesPark(const Line &) for the park's edge, and keep the check that every segment has a ground line.

Step 3: Define editedSurface. Apply the plan to a LayoutStore, as editedBy does, and take surfaceOf on the layouts before and after. Group both surfaces' pieces and edge lines by owner. Return every piece and edge line of each owner whose group differs. Return none whenever editedSegments gives none, including when an edited segment has an empty ground line.

Step 4: acceptedPlan also requires every piece from the plan's edited surface to fit the solids. Share the before and after surfaces with editedSurface through one helper.

Step 5: Build, and run the sim tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 7: Draw the surface

Files:
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`

Step 1: Add these constants and functions:

- `EDGE_LINE_WIDTH = 0.15` and `EDGE_LIFT = 0.005f`.
- `Rgba edgeColor(Rgba color)`, which scales red, green, and blue by 0.6.
- `appendSurface(ParkMesh &, const PavedSurface &, float lift, Rgba fill)`. It adds each piece as a fan at pathLift(kind) + lift. Then it adds each edge line at EDGE_LIFT higher in edgeColor(fill), as a quad on each step's left that is EDGE_LINE_WIDTH wide. Where the line turns right, it also adds the triangle that closes the gap between the two quads.

The fill color applies to every piece. buildParkMesh calls it per kind with pathColor.

Step 2: buildParkMesh draws parkSurface in place of the ribbons. buildGhostMesh draws editedSurface at GHOST_LIFT in ghostColor, in place of the edited segments' ribbons. appendEntity draws a segment's own pieces from parkSurface at GHOST_LIFT. Remove the public appendSegment, and keep the ribbon only for appendStraightPath.

Step 3: Build tpj_app.

### Task 8: Confirm the criteria

Step 1: Capture each park and look at the images against criterion 1.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/fed.park --capture fed.bmp` and the same for tests/parks/stress/full.park.

Step 2: Play criteria 2 to 6 in the app.

Step 3: Build main in a temporary worktree at ../TPJ-baseline, and run `tpj_scenarios.exe` there and on this branch, saving each output. `--compare` stops at the first difference, so remove the park-edits lines from both outputs with `grep -v '^scenario park-edits '`, then compare the two filtered files.

Run: `build/windows-debug/tpj_scenarios.exe --compare base.txt branch.txt; echo $?`
Expected: `0`.

Step 4: Run the full build, `ctest.exe --preset windows-debug`, and `scripts/tidy.sh`.

Expected: no warnings, all tests pass, and "tidy: clean."

### Task 9: Commit

Step 1: Review the staged diff with the reviewer. Then commit once with the commit-hygiene skill, staging only this feature's paths.
