# Implementation Plan: Points and Segments

## Goal

Replace path curves with path points and straight segments in intent, commands, saves, and every reader, and rewrite every checked-in park to match.

## Approach

sim/park registers two intent types, path-point and path-segment, and publishes them as ParkPathPoint and ParkSegment, each segment with its straight ground line. AddPath takes stops that name new points, existing points, or places on existing segments. One private function in edits.cpp works out the stops' positions and the segments to split, and both isAccepted and applyCommand use it, so the check and the commit can't disagree. Every reader of parkPaths and groundLine moves to parkSegments and each segment's Line, keeping what it does. The hand-written parks are rewritten by hand, and the made parks are made again by their generators.

## Placement

Decision 0027 places each behavior this feature adds:

- Path points and segments as intent, their save lines, and their published values: sim/park, in internal/components.h and intent.h. Park intent is this module's, and other modules read it only through its public values.
- A segment's straight ground line: sim/park, segmentLine in geometry.h. The renderer, the validity check, and routes must measure one shape, and geometry.h is where the park's shared shapes live.
- AddPath's stops, splits, and acceptance, DeleteSegment, and validity against segments: sim/park, edits.h. This module makes the game's one refusal and owns the commands that change intent.
- One carrier per segment: sim/routes, networks.cpp. Routes derives the networks from intent.
- Snapping a click to a stop, and picking a segment: tools, tools.h. The tools turn the pointer into edits.
- Drawing segments and join marks: render, park_mesh.h. The park mesh draws intent and ghosts.
- Reading segments in path places, previews, overlays, scene sync, the scenarios, and the tests: each in the file that read paths, with no new behavior.

## Tasks

### Task 1: Update the park spec

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: Make the changes FEATURE.md's Spec changes section lists for src/sim/park/SPEC.md, using its exact sentences. They change the Boundary, Intent, Ground lines, Physical validity, Commands, and The new park sections.

Step 2: Confirm that no old name is left.

Run: `grep -c "parkPaths\|groundLine\|keptPoints\|MIN_POINT_SPACING\|DeletePath\|ParkPath\b" src/sim/park/SPEC.md`
Expected: `0`

### Task 2: Update the routes, tools, render, and legible specs

Files:
- Modify: `src/sim/routes/SPEC.md`
- Modify: `src/tools/SPEC.md`
- Modify: `src/render/SPEC.md`
- Modify: `src/legible/SPEC.md`

Step 1: Make the changes FEATURE.md's Spec changes section lists for these four files, using its exact sentences.

Step 2: Confirm that no old name is left.

Run: `grep -c "parkPaths\|groundLine\|pathAt\|snapToPath\|doubles back" src/sim/routes/SPEC.md src/tools/SPEC.md src/render/SPEC.md src/legible/SPEC.md`
Expected:
```
src/sim/routes/SPEC.md:0
src/tools/SPEC.md:0
src/render/SPEC.md:0
src/legible/SPEC.md:0
```

### Task 3: Update the capability brief

Files:
- Modify: `plans/effortless-building/CAPABILITY.md:24-26,36,40,48,70,85,125`

Step 1: Make these replacements.

- Line 24: replace "The path is a centripetal Catmull-Rom curve through those points." with "The path is a run of straight segments between those points."
- Line 26: replace the paragraph with "If a point is within snapping reach of a point or segment of the same kind, the path joins it there, and a segment splits where it is joined. The junction that navigable-networks derives is then the one the player meant."
- Line 36: replace "- a footprint or curve leaves the park's bounds." with "- a footprint or a segment's width leaves the park's bounds."
- Line 40: replace the paragraph with "Bounds are checked across each segment's whole width, not only at its points."
- Line 48: replace "Intent is saved as the clicked points and box poses" with "Intent is saved as path points, the segments between them, and box poses".
- Line 70: replace "- path curves by kind (guest, backstage);" with "- path points, and segments between them by kind (guest, backstage);".
- Line 85: replace "- path curves;" with "- segments' ground lines and widths;".
- Line 125: replace "simplifies and smooths it into the same curve intent" with "simplifies it into the same points and segments".

Leave the Foundation, the sketch-a-park entry, and the Research notes as they are. They record what the foundation built.

### Task 4: Declare path points and segments as intent

Files:
- Modify: `src/sim/park/internal/components.h:9-21` (PathIntent)
- Modify: `src/sim/park/intent.h`

Step 1: In src/sim/park/internal/components.h, replace PathIntent and its visitFields with:

```cpp
// A point that path segments share: its ground position, and the radius of the corner it makes
// when exactly two segments meet there.
struct PathPointIntent {
  ParkPoint At;
  double Radius = DEFAULT_CORNER_RADIUS;

  bool operator==(const PathPointIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, PathPointIntent &point) {
  visitor.field("x", point.At.X);
  visitor.field("z", point.At.Z);
  visitor.field("radius", point.Radius);
}

// A straight piece of path between two path points.
struct PathSegmentIntent {
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;

  bool operator==(const PathSegmentIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, PathSegmentIntent &segment) {
  visitor.field("kind", segment.Kind);
  visitor.field("from", segment.From);
  visitor.field("to", segment.To);
  visitor.field("width", segment.Width);
}
```

Add `#include "sim/entity_key.h"` to its includes.

Step 2: In src/sim/park/intent.h:

- Add `#include "sim/medium/network.h"` to the includes.
- Replace the line declaring pathWidth with:

```cpp
// The width a new segment of the kind takes, in meters. Until navigable-networks' wide-routes,
// routes gives every carrier of the kind this width.
constexpr double pathWidth(PathKind kind) { return kind == PathKind::Guest ? 3.0 : 2.0; }

// The corner radius a new path point takes, in meters.
inline constexpr double DEFAULT_CORNER_RADIUS = 3.0;
```

- Replace ParkPath and its comment with:

```cpp
// A path point's intent, as other modules read it: its ground position, and the radius of the
// corner it makes when exactly two segments meet there.
struct ParkPathPoint {
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  double Radius = 0.0;

  bool operator==(const ParkPathPoint &) const = default;
};

// A path segment's intent, as other modules read it: its kind, its two points, its width, and its
// ground line, which is empty when the segment has no physical shape.
struct ParkSegment {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;
  std::vector<CarrierPoint> Line;

  bool operator==(const ParkSegment &) const = default;
};
```

- Replace the comment on addParkIntent with `// Registers the private intent component types entrance, path-point, path-segment, and box, in that order.`
- Replace `std::vector<ParkPath> parkPaths(const World &world);` with:

```cpp
std::vector<ParkPathPoint> parkPathPoints(const World &world);
std::vector<ParkSegment> parkSegments(const World &world);
```

- Replace the comment on makeNewPark with `// The new-park template with the schema and the seed, resolution pending: an entrance on the park's edge facing in, and a guest path of one segment running into the park from in front of it.`

### Task 5: Give segments a straight ground line

Files:
- Modify: `src/sim/park/geometry.h:11-26`
- Modify: `src/sim/park/geometry.cpp:1-112`

Step 1: In src/sim/park/geometry.h, replace MIN_POINT_SPACING, GROUND_LINE_SPACING, MIN_SEGMENT_SAMPLES, keptPoints, and groundLine, with their comments, by:

```cpp
// Two positions closer together than this, in meters, make no ground line.
inline constexpr double MIN_SEGMENT_LENGTH = 0.01;

// The straight ground line from one position to the other: two CarrierPoints, at distance 0 and
// at the length between them. Empty when either position is not finite or lies outside the park,
// or the two lie closer together than MIN_SEGMENT_LENGTH.
std::vector<CarrierPoint> segmentLine(ParkPoint from, ParkPoint to);
```

Step 2: In src/sim/park/geometry.cpp, delete chord, phantom, blend, evaluateSegment, appendPoint, keptPoints, and groundLine. Keep isInsidePark and footprintOf. Add segmentLine after the anonymous namespace. It returns `{}` unless both positions pass isInsidePark. It computes dx and dz from `from` to `to` and `length = std::sqrt(dx * dx + dz * dz)`. It returns `{}` when `length < MIN_SEGMENT_LENGTH`. Otherwise it returns `{CarrierPoint{from.X, from.Z, 0.0}, CarrierPoint{to.X, to.Z, length}}`. Remove includes that are no longer used.

### Task 6: Publish path points and segments

Files:
- Modify: `src/sim/park/intent.cpp`

Step 1: Add `#include "sim/park/geometry.h"`.

Step 2: In addParkIntent, replace the path line with two lines, so the order is entrance, path-point, path-segment, box:

```cpp
  schema.addComponent<PathPointIntent>("path-point", DataKind::Intent);
  schema.addComponent<PathSegmentIntent>("path-segment", DataKind::Intent);
```

Step 3: Replace parkPaths with parkPathPoints and parkSegments, both through collectIntent:

- parkPathPoints maps each PathPointIntent to `ParkPathPoint{key, point.At, point.Radius}`.
- parkSegments maps each PathSegmentIntent to a ParkSegment with its key, kind, From, To, and width, and the Line below. Find each end with `world.Registry.try_get<PathPointIntent>(world.findEntity(key))`, checking findEntity against entt::null first. The Line is `segmentLine(from->At, to->At)` when both ends are path points, From differs from To, and the width is finite and above zero. Otherwise it is empty.

Step 4: In makeNewPark, replace the path's creation with two points and a segment, in this order, so the keys are 2, 3, and 4:

```cpp
  // A guest path of one segment from 2 m in front of the entrance, 20 m into the park.
  const EntityKey near = world.createEntity();
  world.Registry.emplace<PathPointIntent>(world.findEntity(near), PathPointIntent{{0.0, 123.0}});
  const EntityKey far = world.createEntity();
  world.Registry.emplace<PathPointIntent>(world.findEntity(far), PathPointIntent{{0.0, 103.0}});
  const EntityKey segment = world.createEntity();
  world.Registry.emplace<PathSegmentIntent>(
      world.findEntity(segment),
      PathSegmentIntent{PathKind::Guest, near, far, pathWidth(PathKind::Guest)});
```

### Task 7: Declare the new commands

Files:
- Modify: `src/sim/park/edits.h`

Step 1: Add `#include <optional>`. Replace AddPath and DeletePath, with their comments, by:

```cpp
// A drawn path's stop at a new point.
struct NewPoint {
  ParkPoint At;

  bool operator==(const NewPoint &) const = default;
};

// A drawn path's stop at an existing path point.
struct OnPoint {
  EntityKey Point = NULL_KEY;

  bool operator==(const OnPoint &) const = default;
};

// A drawn path's stop on an existing segment, Along of the way from its From point to its To point.
struct OnSegment {
  EntityKey Segment = NULL_KEY;
  double Along = 0.0;

  bool operator==(const OnSegment &) const = default;
};

using PathStop = std::variant<NewPoint, OnPoint, OnSegment>;

// A path of the kind through the stops, in order, joining the points and segments they name.
struct AddPath {
  PathKind Kind = PathKind::Guest;
  std::vector<PathStop> Stops;

  bool operator==(const AddPath &) const = default;
};

struct DeleteSegment {
  EntityKey Segment = NULL_KEY;

  bool operator==(const DeleteSegment &) const = default;
};
```

Step 2: Replace DeletePath with DeleteSegment in addParkEdits' comment, in ParkEdit, in isAccepted, and in applyCommand. The comment on addParkEdits reads `// Registers AddPath, AddBox, MoveBox, DeleteSegment, and DeleteBox, in that order.` In the comment above the isAccepted overloads, replace "no path comes within half its width of a footprint" with "no segment comes within half its width of a footprint".

Step 3: After the ParkEdit alias, add:

```cpp
// The stop's ground position: a NewPoint's own, an OnPoint's point's, or the place Along of the way
// from an OnSegment's segment's From point to its To point. None when the stop names no path point,
// or no segment whose two ends are path points.
std::optional<ParkPoint> stopPosition(const World &world, const PathStop &stop);
```

### Task 8: Apply and check the new commands

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Make validity read segments. Replace lineOf with a function that makes a Line from a ParkSegment: none when its Line is empty, and otherwise `Line{segment.Line, segment.Width / 2.0}`. linesOf builds the lines from parkSegments. In isPhysicallyValid, compare the count of lines with `parkSegments(world).size()`, and update its comment to "An entrance or box with no footprint, or a segment with an empty ground line, is not valid."

Step 2: Add stopPosition. A NewPoint gives its At. An OnPoint gives its point's At through findIntent<PathPointIntent>, or none. An OnSegment finds the segment through findIntent<PathSegmentIntent>, then both of its points the same way, and gives `from + Along * (to - from)` in each coordinate, or none if any is missing.

Step 3: In the anonymous namespace, add the function that works out an AddPath, used by both isAccepted and applyCommand:

```cpp
// A segment an AddPath splits: its key, and the indices of the stops on it in order of Along.
struct Split {
  EntityKey Segment = NULL_KEY;
  std::vector<size_t> Stops;
};

// What an accepted AddPath makes: each stop's position, and the segments it splits in ascending key
// order.
struct PathPlan {
  std::vector<ParkPoint> Positions;
  std::vector<Split> Splits;
};

// The plan of an AddPath the world accepts, or none when it refuses it.
std::optional<PathPlan> planPath(const World &world, const AddPath &command);
```

planPath returns none when any of these holds, and the plan otherwise:

- The kind is not known, or there are fewer than two stops.
- An OnPoint names a key with no PathPointIntent, or no segment names that point as From or To, or a segment that does has another kind.
- An OnSegment's Along is not finite or not strictly between 0 and 1, or it names a key whose segment in parkSegments(world) has another kind or an empty Line.
- stopPosition gives none for a stop.
- For two consecutive stops, segmentLine of their positions is empty, or the Line it makes with half of pathWidth(kind) fails `fits(line, solidsOf(world))`.
- For a split segment, segmentLine is empty between its From point's position and its first stop's position, between consecutive stops' positions, or between its last stop's position and its To point's position.

Build Splits by grouping OnSegment stops by segment key, ascending, and ordering each group's stop indices by Along. Two stops at the same Along on one segment make an empty segmentLine between them, so the last rule refuses them.

Step 4: isAccepted for AddPath returns `planPath(world, command).has_value()`. isAccepted for DeleteSegment returns whether findIntent<PathSegmentIntent> finds the segment.

Step 5: applyCommand for AddPath calls planPath and returns when it gives none. Otherwise it creates entities in this order, so the keys follow it:

1. For each stop in order, a NewPoint or an OnSegment creates a point, `PathPointIntent{position}`, and an OnPoint uses its point's key. Keep each stop's point key.
2. For each Split in order, read the segment's PathSegmentIntent, keep its old To, and set its To to the first stop's point. Then create a segment, with the split segment's kind and width, from each of its stops' points to the next, and from its last stop's point to the old To.
3. For each two consecutive stops, create `PathSegmentIntent{command.Kind, first, second, pathWidth(command.Kind)}`.

Step 6: applyCommand for DeleteSegment returns unless the command is accepted. It reads the segment's From and To, destroys the segment, and then destroys From and To if each still holds a PathPointIntent and no PathSegmentIntent in `world.Registry.view<PathSegmentIntent>()` names it as From or To.

Step 7: Build the simulation library. Routes still reads paths, so expect its errors and no others.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -o "src/sim/[a-z_/]*\.cpp" | sort -u`
Expected: `src/sim/routes/networks.cpp`

### Task 9: Build one carrier per segment

Files:
- Modify: `src/sim/routes/networks.cpp:20-37,381,419`

Step 1: Make pathCarriers take `const std::vector<ParkSegment> &segments`. For each segment of the kind with a Line that is not empty, push `Carrier{.Key = segment.Key, .Width = pathWidth(kind), .Points = segment.Line, .Stops = {}}`. Update its comment: "One carrier for each segment of the kind with a ground line, with no stops yet. parkSegments gives segments in ascending key order, so the carriers are in it too."

Step 2: Make deriveNetwork take `const std::vector<ParkSegment> &segments`, and make resolvePathNetworks pass `parkSegments(world)`.

Step 3: Remove `#include "sim/park/geometry.h"` only if nothing else in the file uses it. footprintOf comes from it, so check with grep first.

Step 4: Build the simulation library.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with no warnings and no errors.

### Task 10: Read segments in path places and previews

Files:
- Modify: `src/legible/path_place.cpp:11-34`
- Modify: `src/legible/preview.cpp:45-66`

Step 1: In nearestGuestPathPlace, loop over parkSegments(world) instead of parkPaths, skipping segments that are not Guest, and change the comment to say segments. Everything else stays.

Step 2: In connectionPlace, read `parkSegments(candidate)` into a `std::vector<ParkSegment>`, and test `segment.Kind == PathKind::Guest && segment.Key == place.Carrier`. Change the comment's "a guest path's carrier" to "a guest segment's carrier".

Step 3: Build the library.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build finishes with no warnings and no errors.

### Task 11: Snap a path tool's clicks to stops

Files:
- Modify: `src/tools/tools.h:30-79`
- Modify: `src/tools/tools.cpp`

Step 1: In src/tools/tools.h:

- Replace SNAP_REACH's comment with `// A path tool snaps a click onto a point or segment of its kind this near it, in meters.`
- Replace FINISH_REACH's comment with `// A path tool's press this near its last stop finishes the path, in meters.`
- Replace `std::vector<ParkPoint> Drawn;` and its comment with `// A path tool's stops so far.` and `std::vector<PathStop> Drawn;`.
- Replace pathAt and snapToPath, with their comments, by:

```cpp
// The first segment in key order whose ground line passes within half its width of the point.
std::optional<EntityKey> segmentAt(const World &world, ParkPoint point);
// The stop a click at the point makes for a path of the kind: the nearest path point of the kind
// within SNAP_REACH, or else the nearest place within SNAP_REACH on a segment of the kind, or the
// segment's end point when that place lies within MIN_SEGMENT_LENGTH of it, or else a new point at
// the click. A point is the kind's when every segment at it has the kind.
PathStop snapStop(const World &world, PathKind kind, ParkPoint point);
```

Step 2: In src/tools/tools.cpp, replace pathAt with segmentAt. It loops over parkSegments(world) and uses the segment's Line and `0.5 * segment.Width` where pathAt used the path's ground line and pathWidth.

Step 3: Replace snapToPath with snapStop:

- Read parkPathPoints and parkSegments once.
- A point is the kind's when at least one segment names it as From or To and every such segment has the kind.
- Go through the kind's points in key order and keep the nearest whose distance to the click is at most SNAP_REACH. Only a strictly nearer point replaces it, so ties go to the lower key. If there is one, return `OnPoint{key}`.
- Otherwise go through the segments of the kind with a Line that is not empty, in key order. Take its From and To points' positions from the points read, and skip it if either is missing. Project the click onto the line between them, with t clamped to [0, 1]. Replace nearestOnSegment, whose only caller was snapToPath, with `double alongSegment(ParkPoint point, ParkPoint from, ParkPoint to)`, which returns that clamped t, and use it here. Keep the nearest projection within SNAP_REACH, replacing only on strictly nearer. If there is one, with length L between the two positions, return `OnPoint{From}` when `t * L < MIN_SEGMENT_LENGTH`, `OnPoint{To}` when `(1 - t) * L < MIN_SEGMENT_LENGTH`, and `OnSegment{key, t}` otherwise.
- Otherwise return `NewPoint{point}`.

Step 4: Replace nextPoint with nextStop, returning `std::optional<PathStop>`. It snaps the pointer with snapStop. When Drawn is not empty, it takes stopPosition of the snapped stop and of Drawn.back(). If both have positions and lie within FINISH_REACH of each other, it returns none. pressPointer pushes the stop nextStop gives. pathEdit builds `std::vector<PathStop> stops` the same way it built points, and returns `AddPath{drawnKind(tool.Kind), std::move(stops)}`.

Step 5: In tentativeEdit's Delete branch, call segmentAt and return `DeleteSegment{*segment}`.

Step 6: Build the library.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with no warnings and no errors.

### Task 12: Draw segments and join marks

Files:
- Modify: `src/render/park_mesh.h:64-70` (appendPath)
- Modify: `src/render/park_mesh.cpp:197-210,280-282,313-318,334-339`
- Modify: `src/render/band_overlay.cpp:147-152`
- Modify: `src/render/graph_overlay.cpp:100-104`

Step 1: In src/render/park_mesh.h, replace both appendPath declarations and their comment with:

```cpp
// Adds a segment's flat ribbon along its ground line, the width wide, at pathLift(kind), with a
// round joint beyond each end, or nothing when the line has fewer than two points.
void appendSegment(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &line,
                   double width, Rgba color);

// A ghost marks each stop where an added path joins a point or segment with a flat disc of this
// radius, this high, in this color.
inline constexpr double JOIN_MARK_RADIUS = 1.0;
inline constexpr float JOIN_MARK_LIFT = 0.06f;
inline constexpr Rgba JOIN_MARK_COLOR{0.15f, 0.85f, 0.95f, 0.9f};

// Adds a join mark centered on the point.
void appendJoinMark(ParkMesh &mesh, ParkPoint at);
```

Replace "or path" with "or segment" in appendEntity's comment.

Step 2: In src/render/park_mesh.cpp, replace both appendPath definitions with appendSegment. It returns when the line has fewer than two points. Otherwise it draws what appendPath drew from its ground line, with `width` in place of pathWidth(kind). appendJoinMark calls appendJoint twice at `CarrierPoint{at.X, at.Z, 0.0}`, with directions {1, 0} and {-1, 0}, radius JOIN_MARK_RADIUS, lift JOIN_MARK_LIFT, and JOIN_MARK_COLOR.

Step 3: In buildParkMesh, loop over parkSegments(world) and call `appendSegment(mesh, segment.Kind, segment.Line, segment.Width, pathColor(segment.Kind))`. In appendEntity, find the key among parkSegments(world) and draw it the same way in the color given.

Step 4: In buildGhostMesh, replace the AddPath branch. Take each stop's stopPosition. If every stop has one, draw a segment between each two consecutive positions, with `segmentLine(first, second)`, pathWidth(path->Kind), and the ghost color the branch used. Then call appendJoinMark at the position of each stop that is an OnPoint or an OnSegment. In the deletion branch, read `std::get<DeleteSegment>(edit).Segment` in place of the DeletePath key.

Step 5: In buildBandOverlay, collect the keys of Guest segments from parkSegments(world). In buildGraphOverlay, collect every segment's key from parkSegments(world). Change both comments from paths to segments.

Step 6: Build the library.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with no warnings and no errors.

### Task 13: Watch segments in the scene sync

Files:
- Modify: `src/app/scene/scene_sync.h:55-62`
- Modify: `src/app/scene/scene_sync.cpp:16`

Step 1: In DrawnIntent, replace `std::vector<ParkPath> Paths;` with `std::vector<ParkSegment> Segments;`. In syncWorld, build it from `parkSegments(world)`. A segment's Line changes whenever a point it uses moves, so watching segments catches every change to what the park mesh draws.

Step 2: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with no warnings and no errors.

### Task 14: Build the scenarios' parks from stops

Files:
- Modify: `src/scenarios/food_shop.cpp:97-125`
- Modify: `src/scenarios/full_park.cpp:22-23,41-46,59`
- Modify: `src/scenarios/park_edits.cpp:24,49-63,80-81`
- Modify: `src/scenarios/slice_parks.cpp:21-27`
- Modify: `src/scenarios/slice_parks.h:22`

Step 1: In food_shop.cpp, supplyPath returns `AddPath{PathKind::Backstage, {NewPoint{{12.0, 122.0}}, NewPoint{{12.0, 87.0}}}}`, and its comment says "routes.park's backstage segment". queueCut collects the keys of Backstage segments from parkSegments and pushes `DeleteSegment{backstage.front()}`. Change "backstage path" to "backstage segment" in its comments.

Step 2: In full_park.cpp, replace TEMPLATE_PATH with `constexpr EntityKey TEMPLATE_SEGMENT{4};` and its comment with "The template's guest segment, which the full park replaces with its own paths." addPath applies `AddPath{kind, {NewPoint{from}, NewPoint{to}}}`. layOut applies `DeleteSegment{TEMPLATE_SEGMENT}` with the text "deleting the template's segment". Until crossing-splits, grid lines that cross share no point, and routes joins them from geometry as before.

Step 3: In park_edits.cpp, drawPath builds `path.Stops` of `NewPoint{point}` in the same draws and order. Change the repeat's comment to "One point in eight repeats the last, which makes a segment too short, so the command is refused." Kind 3 pushes `DeleteSegment{drawGivenKey(world, index)}`, and the weights' comment names DeleteSegment.

Step 4: In slice_parks.cpp, push `DeleteSegment{segment.Key}` for each Backstage segment of parkSegments(world). In slice_parks.h, replace "with a DeletePath" with "with a DeleteSegment".

Step 5: Build the runner.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build finishes with no warnings and no errors.

### Task 15: Move the tests to segments

Files:
- Modify: `tests/integration/food_loop_test.cpp:66-82`
- Modify: `tests/sim/park_world_checks_test.cpp:53-58,133-200`

Step 1: In food_loop_test.cpp, cut warm.park's Backstage segments from parkSegments with DeleteSegment, and check that no Backstage segment is left, where it did the same with paths. Change "backstage paths" to "backstage segments" in its comments. Don't change what it asserts.

Step 2: In park_world_checks_test.cpp:

- Rename guestPathUnderGuestHeadingToShop to guestSegmentUnderGuestHeadingToShop, reading parkSegments. Rename deletingGuestPath to deletingGuestSegment, returning `{"deleting a guest segment guests stand on, heading to the shop", DeleteSegment{segment}}`.
- In SECOND_SHOP's comment, replace "guest path 2 along x = 0" with "the guest segments along x = 0", "backstage path 6 along x = 12" with "the backstage segment along x = 12", "the guest path" with "the guest segments", "the backstage path" with "the backstage segment", and "guest path 5, which leaves guest path 2's end at z = 100" with "the guest segment along z = 100".

Step 3: Build both test executables. The parks aren't rewritten yet, so don't run them.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests`
Expected: the build finishes with no warnings and no errors.

### Task 16: Rewrite the hand-written parks

Files:
- Modify: `tests/parks/new.park`, `tests/parks/fed.park`, `tests/parks/routes.park`, `tests/parks/supply.park`, `tests/parks/sketch.park`, `tests/parks/degenerate-intent.park`, `tests/parks/stress/winding-path.park`

Each path becomes its points and the segments between them. Where two paths met, they share a point. The geometry is the same, except that curves become straight runs between their clicked points. Every rewritten park keeps its boxes clear of its segments.

Step 1: Write tests/parks/new.park as:

```
tpj-park 1
seed 1
tick 0
next-key 5

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path-point]
2 x=0 z=123 radius=3
3 x=0 z=103 radius=3

[path-segment]
4 kind=guest from=2 to=3 width=3
```

Step 2: Write tests/parks/fed.park and tests/parks/routes.park, with the same text, as:

```
tpj-park 1
seed 1
tick 0
next-key 24

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path-point]
2 x=0 z=123 radius=3
3 x=0 z=110 radius=3
4 x=0 z=100 radius=3
5 x=-15 z=110 radius=3
6 x=-30 z=110 radius=3
7 x=-15 z=120 radius=3
8 x=-15 z=95 radius=3
9 x=20 z=100 radius=3
10 x=30 z=80 radius=3
11 x=12 z=122 radius=3
12 x=12 z=87 radius=3

[path-segment]
13 kind=guest from=2 to=3 width=3
14 kind=guest from=3 to=4 width=3
15 kind=guest from=3 to=5 width=3
16 kind=guest from=5 to=6 width=3
17 kind=guest from=7 to=5 width=3
18 kind=guest from=5 to=8 width=3
19 kind=guest from=4 to=9 width=3
20 kind=guest from=9 to=10 width=3
21 kind=backstage from=11 to=12 width=2

[box]
22 kind=shop x=6.5 z=115 facing-x=-1 facing-z=0
23 kind=depot x=12 z=80 facing-x=0 facing-z=1
```

Step 3: Write tests/parks/supply.park as fed.park's text with next-key 25, and with this line added at the end of [box]:

```
24 kind=shop x=-25 z=116 facing-x=0 facing-z=-1
```

Step 4: Write tests/parks/sketch.park as:

```
tpj-park 1
seed 1
tick 0
next-key 19

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path-point]
2 x=0 z=123 radius=3
3 x=0 z=100 radius=3
4 x=-20 z=80 radius=3
5 x=-20 z=50 radius=3
6 x=10 z=30 radius=3
7 x=75 z=-20 radius=3
8 x=75 z=10 radius=3
9 x=45 z=25 radius=3

[path-segment]
10 kind=guest from=2 to=3 width=3
11 kind=guest from=3 to=4 width=3
12 kind=guest from=4 to=5 width=3
13 kind=guest from=5 to=6 width=3
14 kind=backstage from=7 to=8 width=2
15 kind=backstage from=8 to=9 width=2

[box]
16 kind=shop x=-35 z=60 facing-x=1 facing-z=0
17 kind=shop x=25 z=40 facing-x=-1 facing-z=0
18 kind=depot x=75 z=-30 facing-x=0 facing-z=1
```

Step 5: Write tests/parks/degenerate-intent.park as below. It holds every malformed shape a hand-written save can give: points outside the park, two points at one place, a segment from a point to itself, a segment naming a missing key or a box, a zero width, a point no segment uses, a point used by both kinds, and today's malformed entrance and boxes.

```
tpj-park 1
seed 99
tick 12
next-key 18

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1
2 x=-0 z=-129 facing-x=0 facing-z=0

[path-point]
3 x=0.1 z=-3.25 radius=3
4 x=17 z=4 radius=3
5 x=17 z=4 radius=-1
6 x=300 z=-128 radius=1e+300
7 x=5e-324 z=-0 radius=0
8 x=40 z=40 radius=3

[path-segment]
9 kind=guest from=3 to=4 width=3
10 kind=guest from=4 to=5 width=3
11 kind=guest from=5 to=6 width=3
12 kind=backstage from=7 to=7 width=2
13 kind=backstage from=3 to=99 width=2
14 kind=guest from=3 to=15 width=0

[box]
15 kind=shop x=-20.5 z=60 facing-x=3 facing-z=4
16 kind=depot x=1e+300 z=-128 facing-x=0 facing-z=0
17 kind=shop x=-0 z=0 facing-x=-1.7976931348623157e+308 facing-z=1e-310
```

Step 6: Rewrite tests/parks/stress/winding-path.park at tick 0, keeping its intent and dropping its play state. Its guests stand on today's path keys, so they can't carry over. The runtime report steps it into play. Run this, which reads the old file's paths and boxes and writes the new one:

```
python3 - <<'EOF'
import re
path = 'tests/parks/stress/winding-path.park'
lines = open(path).read().split('\n')
def points(line):
    return re.findall(r'\{x=([^ ]+) z=([^}]+)\}', line)
guest = points(next(l for l in lines if l.startswith('2 kind=guest')))
guest += points(next(l for l in lines if l.startswith('19 kind=guest')))[1:]
back = points(next(l for l in lines if l.startswith('36 kind=backstage')))
shop = next(l for l in lines if l.startswith('26 kind=shop')).split(' ', 1)[1]
depot = next(l for l in lines if l.startswith('32 kind=depot')).split(' ', 1)[1]
key = 2
point_lines, segment_lines = [], []
def add_points(found):
    global key
    keys = []
    for x, z in found:
        point_lines.append(f'{key} x={x} z={z} radius=3')
        keys.append(key)
        key += 1
    return keys
guest_keys = add_points(guest)
back_keys = add_points(back)
for kind, width, keys in (('guest', 3, guest_keys), ('backstage', 2, back_keys)):
    for a, b in zip(keys, keys[1:]):
        segment_lines.append((kind, width, a, b))
segments = []
for kind, width, a, b in segment_lines:
    segments.append(f'{key} kind={kind} from={a} to={b} width={width}')
    key += 1
boxes = [f'{key} {shop}', f'{key + 1} {depot}']
key += 2
text = ['tpj-park 1', 'seed 1', 'tick 0', f'next-key {key}', '',
        '[entrance]', '1 x=0 z=126.5 facing-x=0 facing-z=-1', '',
        '[path-point]', *point_lines, '', '[path-segment]', *segments, '',
        '[box]', *boxes, '']
open(path, 'w').write('\n'.join(text))
EOF
```

Step 7: Check what it wrote.

Run: `head -4 tests/parks/stress/winding-path.park && grep -c "kind=guest" tests/parks/stress/winding-path.park && grep -c "radius=3" tests/parks/stress/winding-path.park`
Expected:
```
tpj-park 1
seed 1
tick 0
next-key 76
34
37
```

Step 8: Confirm that every hand-written park loads, by running the scenarios over them for one tick.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 1 tests/parks/new.park tests/parks/fed.park tests/parks/routes.park tests/parks/supply.park tests/parks/sketch.park tests/parks/degenerate-intent.park tests/parks/stress/winding-path.park 2> /dev/null | grep -c "^file "`
Expected: `14`

### Task 17: Make the generated parks again

Files:
- Modify: `tests/parks/fed.park`, `tests/parks/warm.park`, `tests/parks/cut.park`
- Modify: `tests/parks/stress/full.park`

Step 1: Make the slice parks from the rewritten fed.park.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park && grep -c "kind=backstage" tests/parks/warm.park tests/parks/cut.park`
Expected:
```
tests/parks/warm.park:1
tests/parks/cut.park:0
```

Step 2: Make the full park. It steps a minute of play with 2,000 guests, so it takes a few minutes in windows-debug.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench && build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park && grep -c "kind=guest" tests/parks/stress/full.park`
Expected: `21`

If either command throws or prints an error, a rewritten layout or builder refuses an edit. Stop and report it.

### Task 18: Run the test pass

The test pass comes after the parks, not before the implementation, because the chosen edit is made from warm.park, and no test builds until every reader has moved to segments.

Step 1: Run the test pass that implementing-features describes, for the chosen edit in FEATURE.md's Tests section.

### Task 19: Confirm the acceptance criteria

Step 1: Build everything.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings and no errors.

Step 2: Run the test suite. This confirms criterion 1.

Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run tidy.

Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Confirm that no reader of the old names is left.

Run: `grep -rn "parkPaths\|groundLine\|keptPoints\|DeletePath\|std::vector<ParkPath>\|snapToPath\|pathAt\|appendPath\|MIN_POINT_SPACING" src tests | wc -l`
Expected: `0`

Step 5: Take the capture for criterion 5, and look at it.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --graph --capture build/windows-debug/points-and-segments.bmp && ls build/windows-debug/points-and-segments.bmp`
Expected: `build/windows-debug/points-and-segments.bmp`. The image shows a ribbon along each segment with round ends, walkways to the shop and the depot, and a graph node wherever segments meet.

Step 6: Play criteria 2, 3, and 4 in `build/windows-debug/ThemeParkJones.exe`, as FEATURE.md describes them, and note what you saw for the report.

### Task 20: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only the files FEATURE.md's Files affected section lists, and plans/effortless-building/path-network/points-and-segments/. Use the subject `Park: Hold paths as points and segments`.
