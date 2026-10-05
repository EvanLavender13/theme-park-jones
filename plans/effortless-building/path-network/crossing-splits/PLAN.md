# Implementation Plan: Crossing Splits

## Goal

AddPath joins its segments to every segment of its kind they cross or touch, including its own, and the ghost marks each such join.

## Approach

A new private function in sim/park, meetingOf, decides how two straight segments meet. It follows FEATURE.md's three rules, using MIN_SEGMENT_LENGTH as the tolerance. planPath in edits.cpp is rebuilt around a table of plan points. It places each stop's point, then tests each of the path's segments against every segment of its kind in the world and against the path's earlier segments, and adds each meeting's point to the segments it splits. isAccepted, applyCommand, and the new pathCrossings all read that one plan, so the ghost's marks and the commit agree. If a path meets nothing, the plan, and so the keys and intent it makes, are exactly what they are today.

## Placement

Decision 0027 places each behavior this feature adds:

- How two straight segments meet: sim/park, a new private component, src/sim/park/internal/meetings.h with src/sim/park/meetings.cpp. It is geometry that only AddPath's plan uses now, and point-editing's MovePoint will use next. Keeping it apart from edits.cpp keeps that file about commands.
- Splitting segments where an AddPath meets them, and pathCrossings: sim/park, edits (src/sim/park/edits.h). AddPath's plan already lives there. One plan serves isAccepted, applyCommand, and pathCrossings.
- Marking crossings in the ghost: render, park_mesh (src/render/park_mesh.cpp, appendGhostPath). It already draws an added path's ghost and its stops' join marks.

## Tasks

### Task 1: Update the park spec

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: In Boundary, replace `segmentLine, stopPosition, stopPositions, footprintOf, and the sizes` with `segmentLine, stopPosition, stopPositions, pathCrossings, footprintOf, and the sizes`.

Step 2: In Intent, replace

    Segments share a point where the player joined them. Routes also joins segments of one kind where they cross (sim/routes/SPEC.md).

with

    Segments share a point where the player joined them, and where a path was drawn across them. Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md).

Step 3: In Commands, after the line `- DeleteBox destroys the box.` and before `Everything a command adds takes a key from the key counter.`, insert the paragraphs and list from FEATURE.md's Spec changes, starting at "Where a segment AddPath adds meets another segment of its kind" and ending at "It gives none if the world refuses the path." Copy them without the `>` quote marks, each paragraph and the list separated by blank lines.

Run: `grep -c "pathCrossings" src/sim/park/SPEC.md`
Expected: `2`

### Task 2: Update the tools and render specs

Files:
- Modify: `src/tools/SPEC.md`
- Modify: `src/render/SPEC.md`

Step 1: In src/tools/SPEC.md, Tools, replace `and a segment it joins splits there when the path commits.` with `and a segment it joins splits there when the path commits. Where the path crosses a segment of its kind, or itself, both split and share a point there when it commits.`

Step 2: In the same paragraph, replace `and marks each stop that joins a point or segment.` with `and marks each stop that joins a point or segment, and each place where the path crosses one.`

Step 3: In src/render/SPEC.md, Ghosts and highlights, replace `- a mark at each stop where an added path joins a point or segment, in JOIN_MARK_COLOR` with `- a mark at each stop where an added path joins a point or segment, and at each place pathCrossings gives, in JOIN_MARK_COLOR`.

Run: `grep -c "crosses" src/tools/SPEC.md && grep -c "pathCrossings" src/render/SPEC.md`
Expected:
```
1
1
```

### Task 3: Declare pathCrossings

Files:
- Modify: `src/sim/park/edits.h`
- Modify: `src/sim/park/edits.cpp`

Step 1: In edits.h, after the declaration of stopPositions and its comment, add the declaration and comment of pathCrossings exactly as FEATURE.md's Public interface gives them.

Step 2: In edits.cpp, after the definition of stopPositions, add a stub definition of pathCrossings that returns an empty vector. Task 7 replaces it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with no warnings and no errors.

### Task 4: Run the test pass

Step 1: Run the test pass that implementing-features describes, for the chosen edit in FEATURE.md's Tests section. The edit makes only AddPath and NewPoint values, which exist already, so the test builds and passes before the implementation. The implementation then makes the same edit split the segment.

### Task 5: Decide how two segments meet

Files:
- Create: `src/sim/park/internal/meetings.h`
- Create: `src/sim/park/meetings.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: Create src/sim/park/internal/meetings.h with the include guard `TPJ_SIM_PARK_INTERNAL_MEETINGS_H`, including `sim/park/intent.h`, `<optional>`, `<stddef.h>`, and `<stdint.h>`, declaring in namespace tpj:

```cpp
// How two straight segments, A and B, meet. Each runs from its first end to its second.
enum class MeetingKind : uint8_t {
  // An end of A lies on B, away from B's ends, so B splits at that end's point.
  AEndOnB,
  // An end of B lies on A, away from A's ends, so A splits at that end's point.
  BEndOnA,
  // They cross away from all four ends, so both split at a new point there.
  Crossing
};

struct Meeting {
  MeetingKind Kind = MeetingKind::Crossing;
  // For AEndOnB and BEndOnA, the end that lies on the other segment: 0 for its first, 1 for its
  // second.
  size_t End = 0;
  // Where the segments meet along each, as a fraction from its first end to its second.
  double AlongA = 0.0;
  double AlongB = 0.0;
  // Where they meet: the end's own position, or the crossing.
  ParkPoint At;
};

// How two segments meet, each with two ends at least MIN_SEGMENT_LENGTH apart. None when each end
// of each lies within MIN_SEGMENT_LENGTH of the line through the other, when an end of each lies
// within MIN_SEGMENT_LENGTH of an end of the other, or when they don't meet.
std::optional<Meeting> meetingOf(ParkPoint a0, ParkPoint a1, ParkPoint b0, ParkPoint b1);
```

Step 2: Create src/sim/park/meetings.cpp, including `sim/park/internal/meetings.h`, `sim/park/geometry.h`, `<algorithm>`, and `<cmath>`. In an anonymous namespace, define:

- `double crossOf(ParkPoint origin, ParkPoint a, ParkPoint b)`: the cross product of a minus origin with b minus origin, `(a.X - o.X) * (b.Z - o.Z) - (a.Z - o.Z) * (b.X - o.X)`.
- `double distanceBetween(ParkPoint a, ParkPoint b)`: the straight distance, with std::sqrt.
- `double lineDistance(ParkPoint p, ParkPoint q0, ParkPoint q1)`: `std::abs(crossOf(q0, q1, p)) / distanceBetween(q0, q1)`.
- `double alongOf(ParkPoint p, ParkPoint q0, ParkPoint q1)`: the projection of p minus q0 onto q1 minus q0, divided by the squared length, clamped to [0, 1].
- `double segmentDistance(ParkPoint p, ParkPoint q0, ParkPoint q1)`: the distance from p to the place alongOf gives on q0 to q1.

Then define meetingOf in namespace tpj, with `constexpr double TOLERANCE = MIN_SEGMENT_LENGTH;`, testing in this order:

1. If lineDistance of a0 and a1 to the line b0 to b1, and of b0 and b1 to the line a0 to a1, are all at most TOLERANCE, return none.
2. If any of a0 and a1 lies within TOLERANCE of either of b0 and b1, by distanceBetween, return none.
3. For a0 then a1: if its segmentDistance to b0 to b1 is at most TOLERANCE, return `Meeting{MeetingKind::AEndOnB, end, end == 0 ? 0.0 : 1.0, alongOf(that end, b0, b1), that end}`.
4. For b0 then b1: if its segmentDistance to a0 to a1 is at most TOLERANCE, return `Meeting{MeetingKind::BEndOnA, end, alongOf(that end, a0, a1), end == 0 ? 0.0 : 1.0, that end}`.
5. Let `d0 = crossOf(a0, a1, b0)`, `d1 = crossOf(a0, a1, b1)`, `e0 = crossOf(b0, b1, a0)`, `e1 = crossOf(b0, b1, a1)`. If `d0 * d1 < 0.0` and `e0 * e1 < 0.0`, the segments cross. Let `alongA = e0 / (e0 - e1)` and `alongB = d0 / (d0 - d1)`. Return `Meeting{MeetingKind::Crossing, 0, alongA, alongB, ParkPoint{a0.X + alongA * (a1.X - a0.X), a0.Z + alongA * (a1.Z - a0.Z)}}`.
6. Otherwise return none.

Step 3: In src/sim/CMakeLists.txt, add `park/meetings.cpp` to tpj_sim's sources, between `park/intent.cpp` and `park_schema.cpp`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with no warnings and no errors.

### Task 6: Split segments where an AddPath meets them

Files:
- Modify: `src/sim/park/edits.cpp:230-370` (Split, PathPlan, planDrawnSplits, planPath)
- Modify: `src/sim/park/edits.cpp:465-513` (applyCommand for AddPath)

Step 1: Include `sim/park/internal/meetings.h` in edits.cpp.

Step 2: Replace the structs Split and PathPlan with these, keeping their place in the anonymous namespace:

```cpp
// A point an AddPath's segments run through: an existing path point, or, when Existing is
// NULL_KEY, a point the path adds.
struct PlanPoint {
  ParkPoint At;
  EntityKey Existing = NULL_KEY;
};

// A plan point on a segment, Along of the way from the segment's first end to its second.
struct PlacedPoint {
  size_t Point = 0;
  double Along = 0.0;
};

// A segment of the world that an AddPath splits, and the plan points it splits at.
struct Split {
  EntityKey Segment = NULL_KEY;
  std::vector<PlacedPoint> At;
};

// What an accepted AddPath makes. Points holds each stop's point in stop order, a RepeatStop
// sharing its earlier stop's, then each point a meeting adds, in the order found. Stops gives each
// stop's entry in Points. Splits holds the world's segments the path splits, in ascending key
// order. Drawn holds, for each of the path's own segments from stop i to stop i + 1, the points it
// runs through between them. Both list their points in order of Along. Crossings holds the entries
// in Points where a meeting splits a segment, in the order found.
struct PathPlan {
  std::vector<PlanPoint> Points;
  std::vector<size_t> Stops;
  std::vector<Split> Splits;
  std::vector<std::vector<PlacedPoint>> Drawn;
  std::vector<size_t> Crossings;
};
```

Step 3: Delete planDrawnSplits. Its checks move into Step 7's partsFit. Nothing else used it.

Step 4: Add these helpers above planPath, in the anonymous namespace:

- `size_t pointEntry(PathPlan &plan, EntityKey key, ParkPoint at)`: the index of the entry in plan.Points whose Existing is key, appending `PlanPoint{at, key}` first if there is none.
- `bool place(std::vector<PlacedPoint> &on, size_t point, double along)`: appends `PlacedPoint{point, along}` and returns true, unless `on` already holds an entry with that Point. Then it returns false.
- `std::vector<PlacedPoint> &splitOf(PathPlan &plan, EntityKey segment)`: the At list of the Split for the segment in plan.Splits, appending `Split{segment, {}}` first if there is none.
- `void markCrossing(PathPlan &plan, size_t point)`: appends point to plan.Crossings unless it holds it already.

Step 5: Add `PathPlan planStops(const AddPath &command, const std::vector<ParkPoint> &positions)` above planPath. It returns a plan with Drawn sized to `command.Stops.size() - 1`, and for each stop i in order:

- OnPoint: Stops gets `pointEntry(plan, on.Point, positions[i])`.
- RepeatStop: Stops gets `plan.Stops[repeat.Stop]`.
- Any other stop: Points gets `PlanPoint{positions[i], NULL_KEY}`, and Stops gets its index.
- Then an OnSegment also does `place(splitOf(plan, on.Segment), plan.Stops[i], on.Along)`, and an OnDrawnSegment does `place(plan.Drawn[on.Stop], plan.Stops[i], on.Along)`.

Step 6: Add `void addMeetings(PathPlan &plan, const std::vector<ParkSegment> &segments, PathKind kind)` above planPath. For each of the path's own segments k, from 0 to `plan.Drawn.size() - 1`, let a0 and a1 be the At of `plan.Points[plan.Stops[k]]` and `plan.Points[plan.Stops[k + 1]]`.

First, for each segment s in `segments`, in their ascending key order, whose Kind is kind and whose Line is not empty, take `meetingOf(a0, a1, b0, b1)`, where b0 and b1 are the positions of `s.Line.front()` and `s.Line.back()`. On a meeting:

- AEndOnB: let p be `plan.Stops[k + meeting.End]`. Place p on `splitOf(plan, s.Key)` at AlongB.
- BEndOnA: let p be `pointEntry(plan, meeting.End == 0 ? s.From : s.To, meeting.At)`. Place p on `plan.Drawn[k]` at AlongA.
- Crossing: append `PlanPoint{meeting.At, NULL_KEY}` to plan.Points, and let p be its index. Place p on `plan.Drawn[k]` at AlongA and on `splitOf(plan, s.Key)` at AlongB.

Then, for each earlier own segment m, from 0 to k - 1, take `meetingOf(m's a0, m's a1, k's a0, k's a1)`, with m's ends found as k's are. On a meeting:

- AEndOnB: let p be `plan.Stops[m + meeting.End]`. Place p on `plan.Drawn[k]` at AlongB.
- BEndOnA: let p be `plan.Stops[k + meeting.End]`. Place p on `plan.Drawn[m]` at AlongA.
- Crossing: append a new PlanPoint at meeting.At as above. Place it on `plan.Drawn[m]` at AlongA and on `plan.Drawn[k]` at AlongB.

After each meeting, if any of its place calls returned true, call markCrossing(plan, p). A meeting at an OnSegment or OnDrawnSegment stop finds its point already placed by planStops, so it marks nothing, and the stop's own join mark shows it. The order of Crossings is the order meetings are found.

Step 7: Add `bool partsFit(const World &world, const PathPlan &plan, const std::vector<ParkSegment> &segments, const std::vector<Solid> &solids, PathKind kind)` above planPath. It returns false unless every part has a ground line and fits the solids:

- For each Split, find its segment with findSegment. Its parts run from the At of its From point, found with `findIntent<PathPointIntent>`, through the At of each placed point in order, to the At of its To point. Each part's `segmentLine` must not be empty, and `fits(Line{part line, segment.Width / 2.0}, solids)` must hold.
- For each own segment k, its parts run from `plan.Points[plan.Stops[k]].At`, through each placed point of `plan.Drawn[k]` in order, to `plan.Points[plan.Stops[k + 1]].At`. Each part's segmentLine must not be empty, and must fit the solids with half of `pathWidth(kind)`.

A part bends at most MIN_SEGMENT_LENGTH away from its segment's line where an end lies on it, so the parts are what the check tests, not the whole lines.

Step 8: Rewrite planPath to:

1. Refuse, as now, an unknown kind, fewer than two stops, a stop with no position, or a stop isStopOfKind rejects.
2. Make `PathPlan plan = planStops(command, *positions)`.
3. Refuse when any of the path's own segments, from stop i's position to stop i + 1's, has an empty segmentLine. meetingOf needs two distinct ends.
4. `addMeetings(plan, segments, command.Kind)`.
5. Sort each Split's At and each Drawn list by Along with `std::ranges::sort` and the projection `&PlacedPoint::Along`. Sort plan.Splits by `&Split::Segment`.
6. Return none unless `partsFit(world, plan, segments, solidsOf(world), command.Kind)`. Otherwise return the plan.

Remove planPath's old loops that checked each stop-to-stop line against the solids, gathered OnSegment stops into Splits, and checked the split parts. Steps 5 and 7 replace them.

Step 9: Rewrite applyCommand for AddPath to read the new plan:

1. Make `std::vector<EntityKey> keys`. For each entry of plan->Points in order, push its Existing key, or, when Existing is NULL_KEY, the key addPoint makes for its At.
2. For each Split in order, the segment keeps its key, kind, and width and now ends at `keys[split.At.front().Point]`. A new segment of its kind and width runs from each placed point to the next, and the last runs to the segment's old To point, as now.
3. For each own segment k, in order, segments of the path's kind and pathWidth run from `keys[plan->Stops[k]]` through `keys[p.Point]` for each p in `plan->Drawn[k]` to `keys[plan->Stops[k + 1]]`.

This keeps today's key order: the stops' new points in stop order, then the meetings' new points, then the split parts in ascending segment key, then the path's own segments.

Step 10: Build the sim tests and run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: the build has no warnings, and the run ends with a line starting `All tests passed`.

### Task 7: Give the meetings' positions

Files:
- Modify: `src/sim/park/edits.cpp` (pathCrossings)

Step 1: Replace the stub of pathCrossings. It makes the plan with planPath. If there is none, it returns an empty vector. Otherwise it returns `plan->Points[i].At` for each i in plan->Crossings, in order.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with no warnings and no errors.

### Task 8: Mark crossings in the ghost

Files:
- Modify: `src/render/park_mesh.cpp:208-224` (appendGhostPath)
- Modify: `src/render/park_mesh.h:71-72` (the comment above JOIN_MARK_RADIUS)

Step 1: At the end of appendGhostPath, after the loop that marks stops, call `appendJoinMark(mesh, at)` for each `at` in `pathCrossings(world, path)`. Change the function's comment to end: `then a join mark at each stop that joins a point, a segment, or an earlier stop, and at each place the path crosses one. Nothing when a stop has no position.`

Step 2: In park_mesh.h, change the comment above JOIN_MARK_RADIUS to:

```cpp
// A ghost marks each stop where an added path joins a point or segment, and each place where it
// crosses one, with a flat disc of this radius, this high, in this color.
```

Run: `cmake.exe --build --preset windows-debug --target ThemeParkJones`
Expected: the build finishes with no warnings and no errors.

### Task 9: Make the full park again

Files:
- Modify: `tests/parks/stress/full.park`

The full park's builder draws its grid with AddPath, so its crossings now split. It steps a minute of play with 2,000 guests, so it takes a few minutes in windows-debug.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench && build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park && grep -c "kind=guest" tests/parks/stress/full.park && grep -c "kind=backstage" tests/parks/stress/full.park && grep -c "radius=" tests/parks/stress/full.park`
Expected:
```
222
11
154
```

These counts come from the builder's layout. Each of the 10 guest lines along z is crossed by the 10 along x and splits into 11 parts, 110 in all. Each line along x splits into 11 parts too, and the one at z = 90 splits once more where the entrance's path ends on it, 111 in all. The entrance's path stays whole. The backstage spine splits where each of the 5 rows starts on it, into 6 parts, and the rows stay whole. The park's 54 points gain the 100 grid crossings. If a count differs, or the command prints an error, stop and report it.

### Task 10: Confirm the acceptance criteria

Step 1: Build everything.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings and no errors.

Step 2: Run the test suite. This confirms criterion 6.

Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run tidy.

Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Build the release app, which is the one the user plays.

Run: `cmake.exe --build --preset windows-release --target ThemeParkJones`
Expected: the build finishes with no warnings and no errors.

Step 5: Play criteria 1 to 4 in `build/windows-debug/ThemeParkJones.exe`, as FEATURE.md describes them, and note what you saw for the report. Task 9's output confirms criterion 5.

### Task 11: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only the files FEATURE.md's Files affected section lists, and plans/effortless-building/path-network/crossing-splits/. Use the subject `Park: Join paths where they cross`. In the body, say that full.park was made again with its grid split at every crossing, so runtime reports from before this commit don't compare with later ones.
