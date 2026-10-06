# Implementation Plan: Edge Radii

## Goal

Give every turn of the paving's outline its own radius, held on the segment ends, set with a handle, and kept through every edit.

## Approach

Corners and junctions become one thing in surface.cpp: at every point with two or more segments, the outline turns from each segment to the next with a fillet, inside or outside, whose radius comes from intent and is clamped to the range that fits. A corner's inside edge takes its radius from the corner's radius, as today, and every other edge from the segment end it starts at, sharp until set. One routine builds the outline ring for a point. Junctions fan it from the point, as today, and corners triangulate it by ear clipping, as the sharp corner does today. The same pass publishes each edge for the tools' handles. Commands carry each segment end's radius through splits and merges.

## Placement

Decision 0027 places each behavior this feature adds:

- Edge radii as intent, and SetEdgeRadius: sim/park, internal/components.h, intent.h, and edits.h and edits.cpp, beside the corner radius and SetCornerRadius. sim/park owns path intent and its commands.
- Fitting edges and building the outline from them, and publishing them as parkEdges: sim/park, surface.h and surface.cpp, which already build the corners and junctions they replace. The validity check and the tools must read the same edges the surface is built from.
- Refusing paving that leaves the park: sim/park, edits.cpp, where the validity check lives.
- Edge handles and their drag: tools, tools.h and tools.cpp, beside the radius handle.
- Drawing edge handles: render, park_mesh.h and park_mesh.cpp, which draw radius handles. The app's scene_uploads.cpp and application.cpp pass them on, as they pass radius handles. That is composition only.
- Random SetEdgeRadius edits: scenarios, park_edits.cpp, which draws every other command.

## Tasks

### Task 1: Update the specs

Files:
- Modify: `src/sim/park/SPEC.md`, `src/tools/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

### Task 2: Add the intent, the command, and the edges, with stubs

Files:
- Modify: `src/sim/park/intent.h`, `src/sim/park/internal/components.h`, `src/sim/park/layout.cpp`
- Modify: `src/sim/park/surface.h`, `src/sim/park/surface.cpp`
- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`
- Modify: every `.park` file in `tests/parks/` and `tests/parks/stress/`

Step 1: In intent.h, after DEFAULT_CORNER_RADIUS, add `inline constexpr double DEFAULT_EDGE_RADIUS = 0.0;` with the comment "The edge radius a new segment end takes, in meters: a sharp edge." Add `double FromEdge = DEFAULT_EDGE_RADIUS;` and `double ToEdge = DEFAULT_EDGE_RADIUS;` to ParkSegment after Line, and say in its comment that it holds its edge radius at each end.

Step 2: In components.h, add the same two fields to PathSegmentIntent after Width, and visit them last as "from-edge-radius" and "to-edge-radius". In layout.cpp, groundOf builds each ParkSegment with both, after its line.

Step 3: In surface.h, declare ParkEdge, parkEdges, and edgeRadiusFor, with comments stating the spec's paragraph on them:

```cpp
struct ParkEdge {
  EntityKey Point = NULL_KEY;
  EntityKey Segment = NULL_KEY;          // the segment whose left side the edge starts from
  std::array<ParkPoint, 2> Along;        // unit directions away from the point: Segment's, then the next one's
  bool Inner = false;
  double Radius = 0.0;
  double MinRadius = 0.0;
  double MaxRadius = 0.0;
  ParkPoint Middle;                      // the curve's middle
  ParkPoint Sharp;                       // where the two sides meet
  ParkPoint Toward;                      // the unit direction Middle moves from Sharp as Radius grows
  bool operator==(const ParkEdge &) const = default;
};
std::vector<ParkEdge> parkEdges(const World &world);
double edgeRadiusFor(const ParkEdge &edge, double distance);
```

In surface.cpp, stub parkEdges to return none and edgeRadiusFor to return 0.

Step 4: In edits.h, add SetEdgeRadius after SetSegmentWidth, with the comment "The segment's edge radius at the point, one of its two points, held exactly as given":

```cpp
struct SetEdgeRadius {
  EntityKey Segment = NULL_KEY;
  EntityKey Point = NULL_KEY;
  double Radius = 0.0;
  bool operator==(const SetEdgeRadius &) const = default;
};
```

Add it last to ParkEdit, and declare its isAccepted and applyCommand overloads. Update the comments that count ten commands. In edits.cpp, register it last in addParkEdits, and stub both overloads: isAccepted returns false, and applyCommand does nothing. Add a planOf overload for it that returns none, so editedOf's visit compiles.

Step 5: Add both fields, at their default, to every saved segment line in the checked-in parks. Leave the user's untracked parks in parks/ alone.

Run: `sed -i -E 's/^([0-9]+ kind=(guest|backstage) from=.*)$/\1 from-edge-radius=0 to-edge-radius=0/' tests/parks/*.park tests/parks/stress/*.park`

Step 6: Build and run the sim tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: builds with no warnings, and all tests pass.

Step 7: The new fields enter every world's hash, so main's scenario output can't be compared with this feature's. Save this state's output as the baseline for Task 11.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe 2> /dev/null | grep -v '^scenario park-edits ' > build/edge-base.txt`
Expected: the command exits 0.

### Task 3: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/sim/park/SPEC.md, and the public headers src/sim/park/edits.h, src/sim/park/surface.h, and src/sim/park/intent.h. Its chosen edit fails until Tasks 4 and 5 are done, because the stub parkEdges gives no edge.

### Task 4: Keep edge radii through commands

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add `double FromEdge = DEFAULT_EDGE_RADIUS;` and `double ToEdge = DEFAULT_EDGE_RADIUS;` to PlanLine. planMove and planWidth set them from the segment they reshape, which keeps its From and To. planDelete's merged line takes the low segment's radius at its far end, and the high segment's radius at the high segment's far end. Each sits at whichever of From and To that point is.

Step 2: In applyPlan, a split segment keeps its FromEdge, and its ToEdge becomes DEFAULT_EDGE_RADIUS. Its last new part takes its old ToEdge at its To end. A line's first part takes the line's FromEdge, and its last part its ToEdge. A line of one part takes both. Every other end takes DEFAULT_EDGE_RADIUS.

Step 3: Add the plan of SetEdgeRadius: none unless the key holds a segment with a ground line, the point is its From or To, and the radius is finite and at least 0. Add `EntityKey Edged = NULL_KEY;`, `bool EdgedAtFrom = false;`, and `double EdgedTo = 0.0;` to EditPlan, and have applyPlan set that end's radius. Replace the stubs with isAccepted and applyCommand as SetCornerRadius's are written.

Step 4: In pointsChanged, count a segment as changed when either edge radius differs too. In editedSurface, return the edited surface whether or not it has segments.

Step 5: Add `leavesPark(const SurfacePiece &)`: whether a corner lies farther than PARK_SIZE / 2 + CONTACT_TOLERANCE from the origin along x or z. acceptedPlan and isPhysicallyValid refuse a piece that leaves the park, as they refuse one that meets a solid.

Step 6: Build tpj_sim.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: builds with no warnings.

### Task 5: Fit each edge

Files:
- Modify: `src/sim/park/surface.cpp`

This replaces pairOf. Work at a point P with the segments sorted counter-clockwise, as trimJunction sorts them. For each segment A and the next one B, let u and v be their outward directions, hA and hB their half widths, LA and LB their reaches, and SA and SB their starts. A segment's start at a corner is the arc's tangent length, the same for both segments: corner.Radius · sine / (1 − cosine), with sine and cosine taken between the corner's two Along directions in the order that makes sine positive, as arcOf takes them. Elsewhere it is 0. Let sine = ccw(u, v) and cosine = dot(u, v).

Step 1: If |sine| ≤ PARALLEL, the pair keeps today's Around or Step, from pairOf's first branch. It has no radius and makes no edge.

Step 2: Otherwise the edge is inner when sine > 0, with s = 1, and outer when sine < 0, with s = −1. With radius r, its center lies hA + s·r from A's center line, on A's left, and hB + s·r from B's, on B's right. It touches A at tA(r) = (hB + hA·cosine + s·r·(1 + cosine)) / sine from P, and B at tB(r), the same with the halves swapped. Both grow with r. The radius at which A's touch is d is rA(d) = (d·sine − hB − hA·cosine) / (s·(1 + cosine)), and the same for B.

- Inner: MinRadius = max(0, rA(SA), rB(SB)), and MaxRadius = min(rA(LA), rB(LB)).
- Outer: let W be twice the larger half width. MinRadius = max(0, rA(−W), rB(−W)), and MaxRadius = min(rA(SA), rB(SB)).
- If MinRadius > MaxRadius, the pair is a Step with no edge.

Step 3: Find the radius asked for. At a corner, for its inner edge, it is max(0, corner.Radius − min(hA, hB)). Every other edge asks for A's edge radius at P, which is FromEdge or ToEdge as P is A's From or To, or 0 if that is below zero or not finite. shapesOf reads it from the ParkSegment it already iterates, and carries it in Shape as `std::array<double, 2> Edge`. Clamp the radius to [MinRadius, MaxRadius]. Pair keeps the radius, the inner flag, and both touches.

Step 4: Set trims per side. A's left trim at P is max(0, tA) of the edge from A, and its right trim is max(0, tB) of the edge into A from the segment before it. A side whose pair has no radius stops at 0.

Step 5: Build tpj_sim.

### Task 6: Build every point from its edges

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: Generalize outlinePair. From A's trimmed left corner, the outline runs along A's left side to its touch, which lies behind P when the touch is negative. For an inner edge, it then follows the arc about the center clockwise, from rightOf(u) to leftOf(v), as today's fillet does. For an outer edge, it follows the arc counter-clockwise, from leftOf(u) to rightOf(v), as today's Around does. Then it runs along B's right side to B's trimmed right corner. Around and Step pairs stay as they are.

Step 2: Build every point that two or more segments use the way buildJunction does: for each segment in order, its trimmed right corner, then the outline of its pair. Each pair's edge line runs from A's left middle, along the outline, to B's right middle, owned by the point, as today.

Step 3: At a corner, triangulate that ring with triangulate. Its first segment is the one the other lies counter-clockwise of by less than half a turn, as buildCorner picks it now. A triangle belongs to the first when ccw(corner.Toward, centroid − corner.At) < 0, and to the second otherwise. Elsewhere, fan it from the point, owned by the point, as today.

Step 4: Remove what this replaces: CornerArc, trimCorner, innerTrims, outlineCorner, and buildCorner. Remove Scratch's Outer and Inner if nothing uses them. Keep the two-pass order: every trim is set before any edge line is written.

Step 5: Make reachOf hold every point's outline. An outline lies within its farthest touch, plus the widest half width, of its point. An inner edge touches no farther than a segment's reach, and an outer edge no farther behind than twice the widest half width. Give each point a reach of the longest reach of any segment there, or twice the widest half width if that is more, plus the widest half width. reachOf joins the segment's box, grown by its half width, with a square that far around each of its two points. Correct its comment.

Step 6: Build tpj_sim and the app. Capture the parks.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/ThemeParkJones.exe --park tests/parks/fed.park --capture build/windows-debug/edge-fed.bmp && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --capture build/windows-debug/edge-full.bmp`
Expected: every bend has a sharp inside and a square outside, and every junction's corners are sharp.

### Task 7: Publish the edges

Files:
- Modify: `src/sim/park/surface.cpp`, `src/sim/park/internal/layout.h`

Step 1: Give buildSurface's first pass an optional list of ParkEdge to fill, one for each pair with a radius, other than a corner's inner edge. Fill a ParkEdge from the pair:

- Along is {u, v}. Inner, Radius, MinRadius, and MaxRadius come from the pair.
- Sharp is P + u·tA(0) + leftOf(u)·hA.
- h is halfway between the arc's two end directions, in the arc's sense, as Step 1 of Task 6 runs it. Middle is the center + h·Radius. Toward is −h.

Step 2: Declare `std::vector<ParkEdge> edgesOf(const PathLayout &layout, const PathGround &ground);` in internal/layout.h. Define it with the first pass only, sorted by point and then by segment. Define parkEdges as edgesOf on the world's layout and ground.

Step 3: Define edgeRadiusFor. The middle moves k = 1 / sqrt((1 − cosine) / 2) − 1 for each meter of radius, where cosine = dot(Along[0], Along[1]). Return distance / k, or 0 when the distance is 0 or less, or k is not above 0.

Step 4: Build and run the sim tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass, including the test pass's chosen edit.

### Task 8: Add edge handles to Edit path

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`

Step 1: In tools.h, HandleKind becomes `{ Radius, Edge, Side }`. EditHandle gains `EntityKey Segment = NULL_KEY;`. An edge handle's Key is its point and Segment is its segment. Replace radiusHandleAt with `std::optional<EditHandle> handleAt(const World &world, ParkPoint point);`. It gives the nearest radius or edge handle within HANDLE_REACH. On a tie, a radius handle comes first, then the lower point key, then the lower segment key. Replace radiusHandles with `std::vector<EditHandle> editHandles(const ToolState &tool, const World &world);`. It gives every radius handle in key order of its point, then every edge handle in parkEdges' order, while EditPath is chosen and nothing is held. Update the comments to match the tools spec.

Step 2: In tools.cpp, every use of radiusHandleAt becomes handleAt. pressEditPath takes an edge handle as it takes a radius handle. Set GrabFrom to dot(Middle − Sharp, Toward) − dot(pointer − Sharp, Toward). handleEdit for an edge finds it in parkEdges by point and segment. It gives `SetEdgeRadius{segment, point, std::clamp(edgeRadiusFor(edge, dot(pointer − Sharp, Toward) + grabFrom), MinRadius, MaxRadius)}`, or none when the edge is gone. hoveredHandle gives handleAt's handle before checking points and sides.

Step 3: Build tpj_tools.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 9: Draw edge handles

Files:
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`
- Modify: `src/app/application.cpp`, `src/app/input/interaction.h`, `src/app/input/interaction.cpp`, `src/app/scene/scene_sync.h`, `src/app/scene/scene_uploads.cpp`

Step 1: Rename appendRadiusHandle to appendHandle. Add `inline constexpr Rgba EDGE_HANDLE_COLOR{0.95f, 0.70f, 0.25f, 0.8f};` beside HANDLE_COLOR.

Step 2: Interaction::radiusHandles becomes editHandles, returning tools' editHandles. PreviewLook::Handles becomes `std::vector<EditHandle>`. application.cpp passes editHandles. ghostMesh draws each handle with appendHandle at its At, in HANDLE_COLOR for a radius handle and EDGE_HANDLE_COLOR for an edge handle. It draws the hovered radius or edge handle in HIGHLIGHT_TINT, and the hovered sides as today.

Step 3: Build the app and play criteria 1 to 4.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/ThemeParkJones.exe`

### Task 10: Draw random SetEdgeRadius edits

Files:
- Modify: `src/scenarios/park_edits.cpp`

Step 1: Add SetEdgeRadius to the weights' comment and give it weight 1, so EDIT_WEIGHTS has eleven entries. In queueEdit, kind 9 stays SetSegmentWidth, and the last branch draws SetEdgeRadius. Draw a given key as the segment. Draw one more value, and pick the segment's From point when it is below 0.5 and its To point otherwise, through parkSegments. If the key holds no segment, use NULL_KEY, so the command is refused. Draw the radius as `drawEdit(world, index) * 12.0`, so some don't fit and are made smaller.

Step 2: Build the scenarios and run them.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null | grep -c "scenario park-edits"`
Expected: `1001`

### Task 11: Confirm the criteria

Step 1: Capture both parks as in Task 6, and look at the images against criterion 5.

Step 2: Play criteria 1 to 4 in the app.

Step 3: Build windows-release here, and in a worktree of main at ../TPJ-baseline. Open tests/parks/stress/full.park in each, pick Edit path, and compare the Debug panel's frame rates for criterion 7. If they differ, time parkEdges, editedSurface, and isAccepted on full.park in both with a scratch program under build/, as paved-surface did, and report the times.

Step 4: Run the scenarios on this branch, remove the park-edits lines, and compare with Task 2's baseline.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null | grep -v '^scenario park-edits ' > build/edge-branch.txt; build/windows-debug/tpj_scenarios.exe --compare build/edge-base.txt build/edge-branch.txt; echo $?`
Expected: `0`

Step 5: Run the full build, `ctest.exe --preset windows-debug`, and `scripts/tidy.sh`.

Expected: no warnings, 0 tests failed, and "tidy: clean."

### Task 12: Commit

Step 1: Review the staged diff with the reviewer, staging only this feature's paths. Then commit once with the commit-hygiene skill, and tell the user that their untracked parks in parks/ need the Task 2 sed to load.
