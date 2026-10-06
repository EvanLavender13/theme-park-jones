# Implementation Plan: Predictable Editing

## Goal

Make every path edit change only what it names: joins and splits keep curves, a curve can be joined where it lies, Delete removes only what it tints, merging is its own tool, and points are visible.

## Approach

Three changes in sim/park carry the behavior.

- Fitting: corner arcs and paving edges share a segment by what the other end asks for, not by halves.
- Bend splits: an arc place cuts a corner into corners of the same radius whose arcs lie on the old one. Stops, targets, inserts, and meetings can all name such a place.
- Curve keeping: after the plan is tried on a layout copy, a pass compares each point's segment ends before and after. It writes the edge radii and corner radii that keep each curve, and the plan applies those writes last.

The tools then snap to straight parts and arcs, Delete loses points, Remove point is added, and the ghost draws point markers and a bare delete tint.

## Placement

Decision 0027 places each behavior this feature adds:

- Arc places, bendPlace and bendAlong: sim/park, geometry.h and geometry.cpp, beside cornerRadiusFor. The tools and the commands must measure arcs the same way.
- Sharing a segment between its ends' arcs: sim/park, layout.cpp, which rounds corners. Sharing it between paving edges: sim/park, surface.cpp, which fits edges.
- OnBend, RemovePoint, bend splits, and meetings with arcs: sim/park, edits.h and edits.cpp, which plan and apply every path command.
- The order of segment ends around a point: sim/park, a new internal header, internal/around.h, so that surface.cpp's fitting and curves.cpp's keeping sort ends the same way.
- Curve keeping: sim/park, a new internal component, internal/curves.h and curves.cpp. It compares two layouts and gives the writes that keep their curves. That rule is separate from planning any one command, and edits.cpp is already long.
- Remove point, Delete without points, snapping to arcs, and point markers: tools, tools.h and tools.cpp, which own every tool's picking.
- Drawing point markers and the bare delete tint: render, park_mesh.h and park_mesh.cpp. The app's tool_panel.cpp and scene_uploads.cpp pass them on. That is composition only.
- Random RemovePoint and OnBend inserts: scenarios, park_edits.cpp, which draws every other command.

## Tasks

### Task 1: Update the specs

Files:
- Modify: `src/sim/park/SPEC.md`, `src/tools/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

### Task 2: Change the interfaces, with stubs

Files:
- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/park/geometry.h`, `src/sim/park/geometry.cpp`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`
- Modify: `src/scenarios/park_edits.cpp`, `tests/sim/park_world_checks_test.cpp`

Step 1: In edits.h, add OnBend after OnSegment, with fields `EntityKey Point` and `double Along` and a default operator==. Its comment says it is a place on the arc of the corner at Point, Along of the way along it, as bendPlace gives. Add OnBend to the end of PathStop and of PointTarget. Add `using InsertPlace = std::variant<OnSegment, OnBend>;`, and make InsertPoint hold one field, `InsertPlace At`. Rename DeletePoint to RemovePoint everywhere, with the comment "The path point goes, and its two segments merge into one." Rename the pathCrossings overload too.

Step 2: In geometry.h, declare `ParkPoint bendPlace(const ParkCorner &corner, double along);` and `double bendAlong(const ParkCorner &corner, ParkPoint point);` with comments stating the spec's sentence on each. Stub them in geometry.cpp to return corner.At and 0.0.

Step 3: In tools.h, add RemovePoint to ToolKind between EditPath and Delete. Add Point to the front of HandleKind, and say in EditHandle's comment that a point marker is keyed by its point, at its position.

Step 4: In park_mesh.h, add POINT_MARK_SIZE = 0.35, POINT_MARK_LIFT = 0.065f, and POINT_MARK_COLOR {0.15f, 0.2f, 0.3f, 0.9f} beside the handle constants. Declare `void appendPointMark(ParkMesh &mesh, ParkPoint at, Rgba color);`, stubbed to add nothing.

Step 5: Make every caller build. These are the edits.cpp planning and visiting code, park_mesh.cpp's join marks and deleted tint, tools.cpp's Delete and insert, park_edits.cpp's draws, and the two chosen edits in the world checks. InsertPoint{segment, along} becomes InsertPoint{OnSegment{segment, along}}, and DeletePoint becomes RemovePoint. For now, treat an OnBend stop, target, or place as naming nothing, so the command is refused.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings.

### Task 3: Place points on arcs

Files:
- Modify: `src/sim/park/geometry.cpp`, `src/sim/park/edits.cpp`

Step 1: Implement bendPlace and bendAlong.
- The arc's center lies along Toward from At, at the hypotenuse of the tangent length and the radius. That is how layout.cpp's roundingOf finds it.
- Its touches lie the tangent length along each Along.
- bendPlace takes the chord point that fraction of the way from the first touch to the second, and pushes it out from the center to the radius.
- bendAlong intersects the line from the center through the point with the chord, and clamps the fraction to 0 to 1.
- Use square roots and products only (decision 0022).

Step 2: In edits.cpp, stopPosition gives an OnBend's place. The place exists when the point makes a corner with a radius above 0 and the fraction is finite and strictly between 0 and 1. Otherwise it gives none.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: no warnings.

### Task 4: Share each segment between its ends' arcs

Files:
- Modify: `src/sim/park/layout.cpp:137-227`

Step 1: In roundingsOf, find every corner first, with the tangent length its point's own radius asks for. Then fit each one.
- A segment allows a corner the larger of two lengths: the segment's length less the tangent the corner at its other end asks for (0 if that end makes no corner), or half its length.
- MaxRadius is the radius whose tangent is the smaller of the two segments' allowances.
- The radius is the asked one, clamped to MaxRadius.
- Then build the arcs as today.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: no warnings.

### Task 5: Share each segment between its ends' paving edges

Files:
- Modify: `src/sim/park/surface.cpp:100-132`, `:316-417`, `:545-677`, `:1032-1047`

Step 1: Give Shape an Asked trim per end and side, and replace reach(end) with reach(end, side).
- reach(end, side) is the larger of Length less Asked at the other end on the same physical side, or Length / 2.
- An end's LEFT is the other end's RIGHT.
- Give Arm a reach per side. pairOf limits an inner fillet by a's LEFT reach and b's RIGHT reach.

Step 2: Before the trims pass, run an asked pass. In it, trimPoint uses each arm's whole Length as its reach and stores the trims it finds as Asked.
- In buildSurface with a focus, the asked pass covers the built points and the far ends of every segment at them, so shapesOf loads the segments at those far ends too.
- edgesOf and the full build cover every point.

Step 3: Delete Shape's Free array. With nothing at the other end, Asked is 0, so the reach is the whole length.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`, then `build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: no warnings, and every test case passes.

### Task 6: Make RemovePoint merge only

Files:
- Modify: `src/sim/park/edits.cpp:654-708`

Step 1: planDelete, renamed planRemove, gives no plan unless mergingPair finds a pair. Delete the branch that destroys every segment at the point.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: no warnings.

### Task 7: Split corners at arc places

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add BendSplits to EditPlan. Each entry holds the corner's ParkCorner, its two segment keys, the first's along Along[0], and its places as PlacedPoints with their arc fractions. Put them in order of the corner's point key, like Splits. Replace Moved and MovedTo with a list of moves, so a corner's point can move along with a MovePoint's point.

Step 2: Fill BendSplits:
- planStops adds a point at each OnBend stop and places it on its corner.
- planMove does the same for an OnBend target, after checking that no segment at the moved point belongs to the corner.
- planInsert does the same for an OnBend place.
- In finishPlan, for each split corner, sort its places by fraction.
  - Add a plan point at each place where the touching lines meet: between each two places in a row, and between the last place and the second segment's line.
  - Record a move of the corner's point to where the line touching the first place meets the first segment's line.
  - The line touching the arc at a place is the line through it perpendicular to the radius there. Meet it with a segment's line by the dot product along that radius, as the spec's bullet describes.

Step 3: In applyPlan, apply bend splits before Splits.
- Move the second segment's end at the corner's point to the last new corner point.
- Set the corner's point's radius to the corner's rounded radius, and give each new corner point the same.
- Add the new segments, with the widths and edge radii the spec gives.
- Each new corner's outside edge is the turn whose starting end is a new segment's end, or the second segment's end. Find it with the cross product of the two directions away from the point, the same test askedOf uses.

Step 4: Accept OnBend in isStopOfKind and isTargetOfKind, and accept InsertPoint's OnBend, with the acceptance the spec states.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: no warnings.

### Task 8: Meet straight parts and arcs

Files:
- Modify: `src/sim/park/edits.cpp:453-521`

Step 1: In meetWorldSegment, meet the plan's line with the segment's straight part. It runs between the touches of the corners at its ends, or between its points where an end makes no corner or has radius 0. Convert AlongB back to a fraction of the line between the segment's points. Where an end of the straight part is a touch, not a point, a BEndOnA there splits the segment at the touch, like a crossing.

Step 2: Add meetCorner for each corner of the line's kind, from groundOf, whose two segments are both in kept, whose point the plan doesn't move, and whose point isn't one of the plan's points, as an OnPoint stop or target makes it.
- Intersect the plan's line with the arc's circle.
- Keep each root whose place lies on the arc. That is when the cosine between its radius and the middle's radius is at least the touches'.
- A root within MIN_SEGMENT_LENGTH of the line's end is that end lying on the arc. Any other root is a crossing, which gets a crossingPoint and a place on the line's Through.
- A place within MIN_SEGMENT_LENGTH of a touch splits that segment at the touch instead of the corner.
- Mark each one as a crossing, as meetWorldSegment does.

Step 3: Call meetCorner from addMeetings for every line, after the segments.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: no warnings.

### Task 9: Keep curves through joins

Files:
- Create: `src/sim/park/internal/curves.h`, `src/sim/park/curves.cpp`, `src/sim/park/internal/around.h`
- Modify: `src/sim/park/surface.cpp:361-368`
- Modify: `src/sim/CMakeLists.txt:17`, `src/sim/park/edits.cpp`

Step 1: In curves.h, declare CurveWrites. It holds a list of (point key, radius) and a list of (segment key, whether at From, edge radius). Declare `CurveWrites keptCurves(const PathLayout &before, const PathLayout &after, const std::vector<std::pair<EntityKey, size_t>> &added);`. Here added lists the segment ends, by key and which end (0 for From), that the edit adds at a point that already had two or more segments.

Step 2: Implement it in curves.cpp with the spec's paragraph on curves, in this order.
1. For each point that had exactly two segments making a corner and now has three or more, write the inside edge's held radius. That is on the end where the outline starts the turn whose cross product is positive. Its value is max(radius − narrower half, 0).
2. For each run of added ends, find the nearest end clockwise of it that isn't added, a, and the next one after the run, b. Give the edges between added ends a's edge radius after step 1's writes. Then fit the edge from a into the run, and the edge from the run into b, so that they touch a and b where a's old edge did. Use the touch formulas pairOf uses. Leave a sharp edge sharp.
3. For each point that had three or more segments and now has exactly two making a corner, write radius = inside edge's radius + narrower half. If that edge's radius is 0, write min(radius, narrower half).
4. For each point that had two or more segments and now has one, write DEFAULT_CORNER_RADIUS.

Move surface.cpp's before() into internal/around.h as an inline function, comesBefore, and use it in both surface.cpp's trimPoint and curves.cpp. Add curves.cpp to the park sources in src/sim/CMakeLists.txt.

Step 3: Give PlanLine FromAdded and ToAdded. Set them true:
- in planStops, for a stop at an existing point;
- in planMove, for the ends at an OnPoint target;
- in meetLines and meetWorldSegment, for parts that end at an existing point the line splits at.

A Split's parts at an existing point are added too. applyPlan reports the segment ends it makes from added plan ends, when given a list to fill.

Step 4: Add `withCurves(world, plan)`. It applies the plan to a LayoutStore copy, collects the added ends, calls keptCurves, and stores the writes in the plan. applyPlan applies them last. Every path command's planOf returns its plan through withCurves.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`, then `build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: no warnings, and every test case passes.

### Task 10: Change the tools

Files:
- Modify: `src/tools/tools.cpp`

Step 1: Delete's tentativeEdit no longer tries pointAt.

Step 2: Remove point's tentativeEdit is RemovePoint for pointAt under the pointer, or none. highlightedEntity marks that point when exactly two segments use it. Its press holds, like Delete's, and its release commits the edit.

Step 3: nearestSegmentSnap measures to each segment's straight part, as Task 8 defines it. Its OnSegment fraction stays a fraction of the line between the segment's points. Add nearestBendSnap over every corner of the kind with a radius above 0, excluding those with a segment at the excluded point. It measures to the arc with bendAlong and bendPlace, keeps fractions strictly between 0 and 1, and loses ties to segments. snapStop and snapPoint use both.

Step 4: insertAt finds the nearest place on the ground line of the segment under the pointer. If that place lies on an arc, it gives InsertPoint{OnBend}. Otherwise it gives InsertPoint{OnSegment} along the straight line.

Step 5: editHandles gives each path point as a Point handle first, in key order. It gives them while Edit path or Remove point is chosen and nothing is held, followed by the radius and edge handles for Edit path only.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: no warnings.

### Task 11: Draw the markers, the delete tint, and the new tool

Files:
- Modify: `src/render/park_mesh.cpp`, `src/app/scene/scene_uploads.cpp`, `src/app/ui/tool_panel.cpp`

Step 1: appendPointMark adds a disc of POINT_MARK_SIZE at POINT_MARK_LIFT.

Step 2: For a DeleteSegment, buildGhostMesh draws the delete tint whether or not it is accepted, in INVALID_TINT when refused. It draws only the surface of the segments editedSegments gives, as their own pieces of editedSurface, and not the rest of editedSurface or the straight path. appendDeleted tints RemovePoint's two segments, as it did DeletePoint's. appendJoinMarks marks an inserted point's place for both InsertPlace kinds, and RemovePoint's crossings.

Step 3: scene_uploads draws Point handles with appendPointMark in POINT_MARK_COLOR, before the other handles. The hovered handle is never a Point.

Step 4: tool_panel lists {"Remove point", ToolKind::RemovePoint} before Delete.

Run: `cmake.exe --build --preset windows-debug`
Expected: no warnings.

### Task 12: Draw random RemovePoint and OnBend edits

Files:
- Modify: `src/scenarios/park_edits.cpp`

Step 1: Kind 7 pushes RemovePoint for a drawn key. Kind 6 picks OnSegment or OnBend with one draw: an OnBend names a drawn key as the point. Update the weights comment.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`, then `build/windows-debug/tpj_scenarios.exe | grep -c "scenario park-edits tick"`
Expected: no warnings, and 3001.

### Task 13: Confirm the criteria

Step 1: Run `cmake.exe --build --preset windows-debug`, `ctest.exe --preset windows-debug`, and `scripts/tidy.sh`.
Expected: no warnings, every test passes, and "tidy: clean.".

Step 2: Check criteria 2 to 7 in the app, as each states. Before that, for criteria 2 to 4, run a scratch program under build/ that applies each edit to a park. Plot the paving and ground lines before and after it, top down.

Step 3: Time Edit path's per-frame tool calls on tests/parks/stress/full.park against main, as edge-radii did, and report the change.

### Task 14: Commit

Step 1: Stage only this feature's paths and commit once with the commit-hygiene skill.
