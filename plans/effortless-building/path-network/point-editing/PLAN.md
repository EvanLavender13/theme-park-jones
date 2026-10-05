# Implementation Plan: Point Editing

## Goal

The commands MovePoint, InsertPoint, and DeletePoint, an Edit path tool that drags and inserts points, a Delete tool that deletes points, and their ghosts.

## Approach

AddPath's plan in src/sim/park/edits.cpp becomes a general edit plan. Its drawn segments become a list of lines, each a segment the command adds or reshapes, and the plan gains the keys the command destroys and a moved point. MovePoint, InsertPoint, and DeletePoint each build such a plan, and the existing meeting search, fit check, and apply step serve all four commands. AddPath's plan builds exactly what it builds today, which a scenarios comparison shows before the new commands enter the random edits. The tools pick and snap points with new queries, and the renderer draws the new edits' ghosts from sim/park's public movedSegments, mergedSegment, and pathCrossings.

## Placement

Decision 0027 places each behavior this feature adds:

- MovePoint, InsertPoint, and DeletePoint, their acceptance, and movedSegments, mergedSegment, and their pathCrossings: sim/park, edits (src/sim/park/edits.h). AddPath's plan, which they share, lives there. One plan per command serves isAccepted, applyCommand, and pathCrossings, so the ghost and the commit agree. edits.cpp grows by about 250 lines. It stays one component about commands, so it isn't split.
- Picking a point and snapping a dragged point, the Edit path tool, and Delete deleting points: tools (src/tools/tools.h). It owns picking, snapping, and what each tool's pointer does.
- The new edits' ghosts and a point's highlight: render, park_mesh (src/render/park_mesh.h). buildGhostMesh and appendEntity already draw every edit's ghost and every highlight.
- The Edit path button: app, tool_panel (src/app/ui/tool_panel.cpp). It lists the tools.
- Random MovePoint, InsertPoint, and DeletePoint edits: scenarios, park_edits (src/scenarios/park_edits.cpp). It draws the random edits of every command.

## Tasks

### Task 1: Record the scenarios output before any change

Files: none.

Step 1: On the current main build, write the scenarios output that Task 6 compares against.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null > build/windows-debug/scenarios-before.txt && grep -c "scenario park-edits" build/windows-debug/scenarios-before.txt`
Expected: the build succeeds, then `1001`.

### Task 2: Update the park spec

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: Make each src/sim/park/SPEC.md change in FEATURE.md's Spec changes, in order. Copy the quoted text without the `>` quote marks, keeping blank lines between paragraphs and lists. The joins paragraph and list replace the existing text from "Where a segment AddPath adds meets another segment of its kind" to "It gives none if the world refuses the path." The movedSegments paragraph follows them.

Run: `grep -c "MovePoint" src/sim/park/SPEC.md && grep -c "five commands" src/sim/park/SPEC.md`
Expected:
```
8
0
```

If the first count differs, check each MovePoint sentence against FEATURE.md before going on.

### Task 3: Update the tools, render, and app specs

Files:
- Modify: `src/tools/SPEC.md`
- Modify: `src/render/SPEC.md`
- Modify: `src/app/SPEC.md`

Step 1: Make each change in FEATURE.md's Spec changes to these three files, in order.

Run: `grep -c "pointAt" src/tools/SPEC.md && grep -c "mergedSegment\|DeletePoint" src/render/SPEC.md && grep -c "Edit path\|path point the tool marks" src/app/SPEC.md`
Expected:
```
4
1
2
```

The render count is 1 because "DeletePoint" appears on one line only. If it differs, check the render list against FEATURE.md.

### Task 4: Declare the new commands and queries

Files:
- Modify: `src/sim/park/edits.h`
- Modify: `src/sim/park/edits.cpp`
- Modify: `src/tools/tools.h`
- Modify: `src/tools/tools.cpp`

Step 1: In edits.h, after the DeleteBox struct, add PointTarget, MovePoint, InsertPoint, and DeletePoint as FEATURE.md's Public interface gives them. Replace addParkEdits' comment and the ParkEdit alias with FEATURE.md's. After the AddPath pathCrossings declaration, add the two new pathCrossings overloads, movedSegments, and mergedSegment, with their comments. Add the three isAccepted declarations after the DeleteBox one, and the three applyCommand declarations after the DeleteBox one.

Step 2: In edits.cpp, in addParkEdits, register the three after DeleteBox:

```cpp
  schema.addCommand<MovePoint>();
  schema.addCommand<InsertPoint>();
  schema.addCommand<DeletePoint>();
```

Add stub definitions after the existing definitions of the same names, leaving out the names of the parameters a stub doesn't use, so -Wunused-parameter stays quiet: both new pathCrossings return `{}`, movedSegments returns `{}`, mergedSegment returns `std::nullopt`, the three isAccepted return `false`, and the three applyCommand do nothing. Tasks 6 to 8 replace them.

Step 3: In tools.h, add EditPath to ToolKind between MoveBox and Delete, POINT_REACH after FINISH_REACH, the three ToolState fields after Target, and pointAt and snapPoint after snapStop, as FEATURE.md's Public interface gives them. In tools.cpp, add stubs, leaving out the unused parameters' names: pointAt returns `std::nullopt`, and snapPoint returns `NewPoint{at}`. Task 11 replaces them.

Step 4: In src/render/park_mesh.cpp, buildGhostMesh's last branch reads `std::get<DeleteBox>(edit)` for any edit that is not AddBox, MoveBox, or AddPath. It would throw for the new commands. Change `} else if (isAccepted(world, edit)) {` to `} else if ((std::holds_alternative<DeleteSegment>(edit) || std::holds_alternative<DeleteBox>(edit)) && isAccepted(world, edit)) {`. Task 12 adds the new commands' ghosts.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings and no errors.

### Task 5: Run the test pass

Step 1: Run the test pass that implementing-features describes, for the three chosen edits in FEATURE.md's Tests section. While the stubs stand, the tests fail at their REQUIRE that the world accepts each edit. That is the expected failure.

### Task 6: Turn AddPath's plan into an edit plan

Files:
- Modify: `src/sim/park/edits.cpp`

This task changes no behavior. It reshapes the plan so the new commands can build one.

Step 1: Replace the PathPlan struct and its comment with these two structs:

```cpp
// A segment an edit adds or reshapes: the one Key holds, or, when Key is NULL_KEY, a new one. It
// runs from plan point From to plan point To and splits at the points it runs Through, in order of
// Along.
struct PlanLine {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  double Width = 0.0;
  size_t From = 0;
  size_t To = 0;
  std::vector<PlacedPoint> Through;
};

// What an accepted edit makes. Points holds the points its lines and splits run through: for
// AddPath, each stop's point in stop order, a RepeatStop sharing its earlier stop's, then each
// point a meeting adds, in the order found. Stops gives each AddPath stop's entry in Points.
// Splits holds the segments the edit leaves in place but splits, in ascending key order, each
// with its points in order of Along. Lines holds the segments it adds or reshapes: for AddPath,
// the one from stop i to stop i + 1 at index i. Crossings holds the entries in Points where a
// meeting splits a segment, in the order found. Destroyed holds the keys it destroys, segments
// first. If Moved is not NULL_KEY, that existing point moves to MovedTo.
struct EditPlan {
  std::vector<PlanPoint> Points;
  std::vector<size_t> Stops;
  std::vector<Split> Splits;
  std::vector<PlanLine> Lines;
  std::vector<size_t> Crossings;
  std::vector<EntityKey> Destroyed;
  EntityKey Moved = NULL_KEY;
  ParkPoint MovedTo;
};
```

Rename PathPlan to EditPlan everywhere in edits.cpp.

Step 2: In planStops, replace `plan.Drawn.resize(command.Stops.size() - 1);` with a loop after the stop loop that adds one line per pair of stops:

```cpp
  for (size_t i = 0; i + 1 < command.Stops.size(); ++i) {
    plan.Lines.push_back(PlanLine{NULL_KEY, command.Kind, pathWidth(command.Kind), plan.Stops[i],
                                  plan.Stops[i + 1], {}});
  }
```

The lines are made after the stop loop, because they need every stop's entry. Before the stop loop, make a local `std::vector<std::vector<PlacedPoint>> drawn(command.Stops.size() - 1)`. The OnDrawnSegment branch places onto `drawn[onDrawn->Stop]`, and line i takes `Through = std::move(drawn[i])`.

Step 3: Replace drawnEnds with lineEnds, which reads the line's own points:

```cpp
// The ends of the plan's line k.
std::pair<ParkPoint, ParkPoint> lineEnds(const EditPlan &plan, size_t k) {
  return {plan.Points[plan.Lines[k].From].At, plan.Points[plan.Lines[k].To].At};
}
```

Step 4: In meetWorldSegment, use lineEnds. For AEndOnB, the point is `meeting->End == 0 ? plan.Lines[k].From : plan.Lines[k].To`. Replace each `plan.Drawn[k]` with `plan.Lines[k].Through`.

Step 5: Rename meetDrawnSegment to meetLines, with the same comment for "the plan's earlier line m" and "line k". Use lineEnds. For AEndOnB, the point is line m's end: `meeting->End == 0 ? plan.Lines[m].From : plan.Lines[m].To`. For BEndOnA, it is line k's end, the same way. Replace `plan.Drawn[m]` and `plan.Drawn[k]` with the lines' Through.

Step 6: Change addMeetings to take the segments the edit leaves in place, and to read each line's kind:

```cpp
// Adds the points where each of the plan's lines meets a segment of its kind that the edit leaves
// in place, in ascending key order, and then each of the plan's earlier lines.
void addMeetings(EditPlan &plan, const std::vector<ParkSegment> &kept) {
  for (size_t k = 0; k < plan.Lines.size(); ++k) {
    for (const ParkSegment &segment : kept) {
      if (segment.Kind == plan.Lines[k].Kind && !segment.Line.empty()) {
        meetWorldSegment(plan, k, segment);
      }
    }
    for (size_t m = 0; m < k; ++m) {
      meetLines(plan, m, k);
    }
  }
}
```

Step 7: Change partsFit to drop its kind parameter and check each line at its own width: `runFits(plan, from, line.Through, to, line.Width, solids)` with `const auto [from, to] = lineEnds(plan, k);`.

Step 8: Add finishPlan, which every command's plan ends with, and use it in planPath after the segmentLine check of each line. planPath calls `addMeetings(plan, segments)` and then `return finishPlan(world, std::move(plan), segments);`.

```cpp
// The plan with its meetings sorted, or none when a part of a segment it splits, adds, or
// reshapes has no ground line or meets a solid.
std::optional<EditPlan> finishPlan(const World &world, EditPlan plan,
                                   const std::vector<ParkSegment> &kept) {
  for (Split &split : plan.Splits) {
    std::ranges::sort(split.At, {}, &PlacedPoint::Along);
  }
  for (PlanLine &line : plan.Lines) {
    std::ranges::sort(line.Through, {}, &PlacedPoint::Along);
  }
  std::ranges::sort(plan.Splits, {}, &Split::Segment);
  if (!partsFit(world, plan, kept, solidsOf(world))) {
    return std::nullopt;
  }
  return plan;
}
```

partsFit finds each split's segment in `kept` with findSegment, as it does now in `segments`.

Step 9: Replace the body of applyCommand(World &, const AddPath &) after its plan check with a call to a new applyPlan, and write applyPlan from the current body:

```cpp
// Makes what the plan describes. Destroyed keys go first. Each plan point's key is its existing
// point's, or a new point's, in the plan's order. Each split segment ends at its first point, and
// new parts run on through the rest to its old end. Each line runs through its points: a reshaped
// line keeps its key for its first part, and every other part takes a new key.
void applyPlan(World &world, const EditPlan &plan) {
```

In order, it:
- destroys each key in plan.Destroyed, with world.destroyEntity;
- makes the keys list as the current body does;
- if plan.Moved is not NULL_KEY, sets `world.Registry.get<PathPointIntent>(world.findEntity(plan.Moved)).At = plan.MovedTo;`
- splits plan.Splits as the current body does;
- for each line: starts at `from = keys[line.From]`. For each part, from `from` to the next point in Through and finally to `keys[line.To]`, the first part of a line whose Key is not NULL_KEY replaces that segment's intent with `PathSegmentIntent{line.Kind, from, to, line.Width}`. Every other part is added with addSegment.

For AddPath, every line's Key is NULL_KEY, so applyPlan makes the same keys in the same order as today.

Step 10: pathCrossings(world, const AddPath &) reads `plan->Crossings` as before.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null > build/windows-debug/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/windows-debug/scenarios-before.txt build/windows-debug/scenarios-after.txt; echo "status $?"`
Expected: the build succeeds with no warnings, then `status 0`. This is criterion 8.

### Task 7: MovePoint

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add helpers in the anonymous namespace, after findSegment:

```cpp
// The segments that end at the point, in ascending key order.
std::vector<ParkSegment> segmentsAt(const std::vector<ParkSegment> &segments, EntityKey point);
// The segments that don't end at the point, in ascending key order.
std::vector<ParkSegment> segmentsNotAt(const std::vector<ParkSegment> &segments, EntityKey point);
// The segment's end that is not the point.
EntityKey farEnd(const ParkSegment &segment, EntityKey point);
// The target as a stop, for stopPosition.
PathStop stopOf(const PointTarget &target);
```

stopOf is `std::visit([](const auto &stop) { return PathStop{stop}; }, target)`.

Step 2: Add planMove:

```cpp
// The plan of a MovePoint the world accepts, or none when it refuses it.
std::optional<EditPlan> planMove(const World &world, const MovePoint &command);
```

It does these steps in order, and returns none where a step says so:
1. `segments = parkSegments(world)`, `at = segmentsAt(segments, command.Point)`. If the key holds no PathPointIntent, `at` is empty, or any segment in `at` has a kind other than `at.front().Kind`, return none. Call that kind `kind`.
2. Check the target. An OnPoint target needs `Point != command.Point` and `isPointOfKind(world, segments, Point, kind)`. An OnSegment target needs `isStopOfKind(world, segments, target, kind)`, and the segment's From and To both differ from command.Point. A NewPoint target needs nothing here. Then `position = stopPosition(world, stopOf(command.To))`. If a check fails or position is none, return none.
3. The moved point's entry. For an OnPoint target, `moved = pointEntry(plan, target.Point, *position)`. Otherwise, push `PlanPoint{*position, command.Point}`, set `moved` to its index, and set `plan.Moved = command.Point` and `plan.MovedTo = *position`.
4. For each segment s in `at`: `far = farEnd(s, command.Point)`. If the target is OnPoint and `far == target.Point`, push s.Key to plan.Destroyed and go on to the next. Otherwise `farEntry = pointEntry(plan, far, <far's PathPointIntent At>)`, and push `PlanLine{s.Key, s.Kind, s.Width, s.From == command.Point ? moved : farEntry, s.To == command.Point ? moved : farEntry, {}}`.
5. For an OnPoint target, push command.Point to plan.Destroyed. If no line was made and no segment in `segmentsNotAt(segments, command.Point)` ends at the target point, push the target point to plan.Destroyed too, so it isn't left with no segments.
6. For an OnSegment target, `place(splitOf(plan, target.Segment), moved, target.Along)`.
7. If any line's ends have no segmentLine, return none.
8. `kept = segmentsNotAt(segments, command.Point)`. Call `addMeetings(plan, kept)` and return `finishPlan(world, std::move(plan), kept)`.

If a far end holds no PathPointIntent, as only a hand-written save can make, return none.

Step 3: Define isAccepted(MovePoint) as `planMove(world, command).has_value()`, applyCommand(MovePoint) as applyPlan of the plan when there is one, and pathCrossings(world, const MovePoint &) as the positions of `plan->Crossings`, like AddPath's.

Step 4: Define movedSegments. If the key holds no PathPointIntent, or `stopPosition(world, stopOf(move.To))` is none, return `{}`. Otherwise, for each segment s in segmentsAt, in order: skip it when the target is OnPoint and its far end is the target point. Copy s, replace its end at the point with the target point's key for an OnPoint target, and set its Line to segmentLine of its two ends' positions, with the moved end at the target position. Skip a segment whose far end has no PathPointIntent.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with no warnings and no errors.

### Task 8: InsertPoint

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add planInsert:

```cpp
// The plan of an InsertPoint the world accepts, or none when it refuses it.
std::optional<EditPlan> planInsert(const World &world, const InsertPoint &command);
```

It takes `segments = parkSegments(world)` and `segment = findSegment(segments, command.Segment)`. If segment is null, its Line is empty, or `command.Along` is not finite and strictly between 0 and 1, it returns none. Otherwise it pushes `PlanPoint{*stopPosition(world, OnSegment{command.Segment, command.Along}), NULL_KEY}`, places entry 0 on `splitOf(plan, command.Segment)` at command.Along, and returns `finishPlan(world, std::move(plan), segments)`. It finds no meetings. Both parts lie along the segment, which has met everything it meets already.

Step 2: Define isAccepted(InsertPoint) and applyCommand(InsertPoint) from planInsert, as for MovePoint.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with no warnings and no errors.

### Task 9: DeletePoint

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add planDelete:

```cpp
// The plan of a DeletePoint the world accepts, or none when it refuses it.
std::optional<EditPlan> planDelete(const World &world, const DeletePoint &command);
```

1. If the key holds no PathPointIntent, return none. Take `segments = parkSegments(world)` and `at = segmentsAt(segments, command.Point)`.
2. If `at` has exactly two segments, and their far ends differ, merge. Let `low = at[0]` and `high = at[1]`, which are in ascending key order. Push entries for low's far end and high's far end with pointEntry, taking their positions from their PathPointIntent. If either has none, return none. Push `PlanLine{low.Key, low.Kind, std::max(low.Width, high.Width), From, To, {}}`, where low's end at the point becomes high's far end's entry and its other end stays low's far end's entry. Push high.Key, then command.Point, to plan.Destroyed. If the line's ends have no segmentLine, return none. With `kept = segmentsNotAt(segments, command.Point)`, call addMeetings and return finishPlan.
3. Otherwise, push each segment of `at`, then command.Point, to plan.Destroyed. Then, in ascending key order and once each, push each far end of a segment in `at` that is not command.Point and that no segment outside `at` uses. Return the plan without finishPlan, since it adds nothing that must fit.

Step 2: Define isAccepted(DeletePoint), applyCommand(DeletePoint), and pathCrossings(world, const DeletePoint &) from planDelete, as for MovePoint.

Step 3: Define mergedSegment. Take segmentsAt the point. If there are not exactly two, or their far ends are equal, or either far end has no PathPointIntent, return none. Otherwise return a copy of the lower-keyed one, with its end at the point replaced by the other's far end, Width the wider of the two, and Line the segmentLine of its two ends' positions.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]" | tail -1`
Expected: the build finishes with no warnings, then `All tests passed (<n> assertions in 5 test cases)`. The three chosen edits from Task 5 now pass.

### Task 10: Draw the new commands in the random edits

Files:
- Modify: `src/scenarios/park_edits.cpp`

Step 1: Replace the weights' comment and array with:

```cpp
// Weights of AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, and
// DeletePoint.
constexpr std::array<uint64_t, 8> EDIT_WEIGHTS{3, 4, 3, 1, 1, 2, 1, 1};
```

Step 2: In queueEdit, replace the final `} else {` branch for DeleteBox with `} else if (kind == 4) {`, and add three branches after it:

```cpp
  } else if (kind == 5) {
    const EntityKey point = drawGivenKey(world, index);
    const ParkPoint to{drawAround(world, index, REACH), drawAround(world, index, REACH)};
    commands.push(MovePoint{point, NewPoint{to}});
  } else if (kind == 6) {
    const EntityKey segment = drawGivenKey(world, index);
    commands.push(InsertPoint{segment, drawEdit(world, index)});
  } else {
    commands.push(DeletePoint{drawGivenKey(world, index)});
  }
```

The two declarations of `point` and `to` keep the draws in a fixed order.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null | grep -c "scenario park-edits"`
Expected: the build succeeds with no warnings, then `1001`.

### Task 11: Pick and snap points, and the Edit path and Delete tools

Files:
- Modify: `src/tools/tools.cpp`

Step 1: Give nearestPointSnap and nearestSegmentSnap a last parameter `EntityKey excluded`. nearestPointSnap skips a candidate whose key is excluded. nearestSegmentSnap skips a segment whose From or To is excluded. snapStop passes NULL_KEY to both.

Step 2: Define pointAt. Over parkPathPoints, in ascending key order, keep the nearest point whose distance from the given point is at most POINT_REACH, replacing it only with a strictly nearer one.

Step 3: Define snapPoint. Find the first segment in parkSegments that ends at the point. If there is none, return `NewPoint{at}`. Its kind is the point's kind. Take `onPoint = nearestPointSnap(points, segments, kind, at, point)`. If there is one, return its stop as an OnPoint. Otherwise take `onSegment = nearestSegmentSnap(points, segments, kind, at, point)`, and return its stop, which is an OnPoint or an OnSegment, when there is one. Otherwise return `NewPoint{at}`. Convert each Snap's PathStop to a PointTarget with std::get_if on OnPoint and OnSegment.

Step 4: In selectTool, also reset `tool.Inserting`.

Step 5: In movePointer, after the MoveBox branch, add: when the tool is EditPath, holding, and `tool.Held != NULL_KEY`, set `tool.PointTo = ParkPoint{ground->X + tool.GrabOffset.X, ground->Z + tool.GrabOffset.Z}`.

Step 6: Add a helper in the anonymous namespace:

```cpp
// The InsertPoint a click at the point makes on the segment under it, or none.
std::optional<InsertPoint> insertAt(const World &world, ParkPoint point);
```

It takes segmentAt. It returns none if there is none, or if either end's position is none (stopPosition of OnPoint). Otherwise it returns `InsertPoint{segment, alongSegment(point, from, to)}`.

Step 7: In pressPointer, after the MoveBox branch, add an `else if (tool.Kind == ToolKind::EditPath && tool.Pointer)` branch:
- If pointAt gives a point, hold it: Holding true, Held the point, PointFrom its position (stopPosition of OnPoint), PointTo the same, and GrabOffset its position less the pointer.
- Otherwise, if insertAt gives an edit, hold it: Holding true, Inserting the edit.

Step 8: In releasePointer, reset `tool.Inserting` with the other fields.

Step 9: In tentativeEdit, add a branch for EditPath:
- Holding a point: none if `tool.PointTo == tool.PointFrom`. Otherwise `MovePoint{tool.Held, snapPoint(world, tool.Held, tool.PointTo)}`.
- Holding an insert: `*tool.Inserting`.
- Not holding, with a pointer: none if pointAt gives a point. Otherwise insertAt's edit, or none.

Step 10: In tentativeEdit's Delete branch, between the box and the segment, return `DeletePoint{*point}` when pointAt gives a point.

Step 11: In highlightedEntity, return pointAt's point when the tool is EditPath, not holding, with a pointer.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with no warnings and no errors.

### Task 12: Draw the new edits' ghosts and a point's highlight

Files:
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`

Step 1: In park_mesh.h, replace appendEntity's comment with: "Adds the box, segment, or path point the key holds, in the color: a box or segment shaped as the park draws it, and a path point as a flat disc of JOIN_MARK_RADIUS at JOIN_MARK_LIFT. Nothing when the key holds none of these."

Step 2: In appendEntity, after the segment loop, add a loop over parkPathPoints. For the point with the key, add a disc as appendJoinMark does, with JOIN_MARK_RADIUS and JOIN_MARK_LIFT, in the given color. Add a helper `appendDisc(ParkMesh &, ParkPoint at, double radius, float lift, Rgba color)` with appendJoinMark's two appendJoint calls, and use it in appendJoinMark, appendFinishMark, and appendEntity.

Step 3: In buildGhostMesh, add three branches before the delete branch:
- MovePoint: take movedSegments. If there are any, draw each with appendGhostSegment in `ghostColor(world, edit, pathColor(segments.front().Kind))`. When the target is OnPoint or OnSegment and `stopPosition(world, <the target as a PathStop>)` gives a position, add a join mark there. Add a join mark at each place pathCrossings(world, move) gives.
- InsertPoint: when `stopPosition(world, OnSegment{insert.Segment, insert.Along})` gives a position, add a join mark there.
- DeletePoint: if the world accepts it, draw each segment that ends at the point with appendEntity in DELETE_TINT. If mergedSegment gives a segment, draw it with appendGhostSegment in `ghostColor(world, edit, pathColor(merged.Kind))`. Add a join mark at each place pathCrossings(world, del) gives.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with no warnings and no errors.

### Task 13: Add Edit path to the Tools panel

Files:
- Modify: `src/app/ui/tool_panel.cpp`

Step 1: In TOOL_CHOICES, after `{"Move box", ToolKind::MoveBox},`, add `{"Edit path", ToolKind::EditPath},`.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with no warnings and no errors.

### Task 14: Confirm the criteria

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, `100% tests passed`, and `tidy: clean.` This is criterion 7.

Step 2: Play criteria 1 to 6 in build/windows-debug/ThemeParkJones.exe, opening warm.park for criterion 6 with `--park tests/parks/warm.park`. Report what each showed.

### Task 15: Review and commit

Step 1: Review and commit as implementing-features describes, with one commit for the feature, written with commit-hygiene. Stage only this feature's files: the files in FEATURE.md's Files affected and plans/effortless-building/path-network/point-editing/. Leave parks/ untracked.
