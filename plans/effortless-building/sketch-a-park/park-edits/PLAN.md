# Implementation Plan: Park Edits

## Goal

Add the five park commands, the physical-validity check as their only refusal, a whole-world validity query, and a scenario that compares the check's answers across builds.

## Approach

src/sim/park/edits.cpp gathers a world's solids (footprints with their pose and size) and lines (ground lines with half their path's width) through the public queries, and judges each conflict with CONTACT_TOLERANCE: corners and line points against the square's limits, the separating axis test on four axes for solids, and a segment-to-rectangle distance in the solid's own frame for lines. isAccepted builds the command's solid or line and checks it against the others. applyCommand calls isAccepted first and changes nothing when it is false, so the query and the outcome are one decision. The scenario draws commands with keyed draws and queues one every tenth cycle.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify validity and commands in the park spec

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: In the Ground lines section, after the sentence "With fewer than two kept points the line is empty.", add the sentence "keptPoints gives the points it keeps." in the same paragraph.

Step 2: Between the Footprints section and "## The new park", insert the Physical validity and Commands sections exactly as FEATURE.md's Spec changes give them, from "## Physical validity" to the end of the Commands section.

Step 3: In The new park, after "holding exactly two entities.", add the sentence "It is physically valid."

Run: `grep -c "^## " src/sim/park/SPEC.md`
Expected: `6`

### Task 2: Name the commands in the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Replace "then park intent (sim/park/SPEC.md)." with "then park intent and its commands (sim/park/SPEC.md)."

Run: `grep -c "park intent and its commands" src/sim/SPEC.md`
Expected: `1`

### Task 3: Describe the park-edits scenario

Files:
- Modify: `src/scenarios/SPEC.md`

Step 1: In the Contract's second paragraph, after the sentence ending "so tests can audit its world.", add the paragraph text FEATURE.md gives for src/scenarios/SPEC.md, as further sentences of the same paragraph.

Run: `grep -c "park-edits makes its world" src/scenarios/SPEC.md`
Expected: `1`

### Task 4: Make keptPoints public

Files:
- Modify: `src/sim/park/geometry.h`
- Modify: `src/sim/park/geometry.cpp`

Step 1: In geometry.h, after the MIN_SEGMENT_SAMPLES constant, declare:

```cpp
// The points in order, without any closer than MIN_POINT_SPACING to the last kept one.
std::vector<ParkPoint> keptPoints(const std::vector<ParkPoint> &points);
```

Step 2: In geometry.cpp, move the keptPoints definition, unchanged except for its comment, which the header now carries, out of the anonymous namespace to just after `} // namespace` and before groundLine.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 5: Declare the commands and stub them

Files:
- Create: `src/sim/park/edits.h`
- Create: `src/sim/park/edits.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park_schema.cpp`

Step 1: Create src/sim/park/edits.h with include guard TPJ_SIM_PARK_EDITS_H, including "sim/entity_key.h", "sim/park/intent.h", "sim/schema.h", "sim/world.h", and <vector>, and holding, in namespace tpj, the declarations FEATURE.md's public interface gives for edits.h, in that order.

Step 2: Create src/sim/park/edits.cpp:

```cpp
#include "sim/park/edits.h"

namespace tpj {

void addParkEdits(WorldSchema &schema) {
  schema.addCommand<AddPath>();
  schema.addCommand<AddBox>();
  schema.addCommand<MoveBox>();
  schema.addCommand<DeletePath>();
  schema.addCommand<DeleteBox>();
}

bool isAccepted(const World & /*world*/, const AddPath & /*command*/) { return false; }
bool isAccepted(const World & /*world*/, const AddBox & /*command*/) { return false; }
bool isAccepted(const World & /*world*/, const MoveBox & /*command*/) { return false; }
bool isAccepted(const World & /*world*/, const DeletePath & /*command*/) { return false; }
bool isAccepted(const World & /*world*/, const DeleteBox & /*command*/) { return false; }

void applyCommand(World & /*world*/, const AddPath & /*command*/) {}
void applyCommand(World & /*world*/, const AddBox & /*command*/) {}
void applyCommand(World & /*world*/, const MoveBox & /*command*/) {}
void applyCommand(World & /*world*/, const DeletePath & /*command*/) {}
void applyCommand(World & /*world*/, const DeleteBox & /*command*/) {}

bool isPhysicallyValid(const World & /*world*/) { return false; }

} // namespace tpj
```

Step 3: In src/sim/CMakeLists.txt, add `park/edits.cpp` to tpj_sim's sources, before `park/geometry.cpp`.

Step 4: In src/sim/park_schema.cpp, include "sim/park/edits.h" after "sim/medium/network.h", and call `addParkEdits(*schema);` after `addParkIntent(*schema);`. Change the comment above the calls to end "...in dependency order, starting with park intent and its commands."

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

Run: `ctest --preset linux-debug`
Expected: every existing test passes.

### Task 6: Test pass

Dispatch the test-writer agent on plans/effortless-building/sketch-a-park/park-edits/FEATURE.md. It writes tests under tests/sim/park/ and tests/scenarios/ and adds them to their CMakeLists.txt. Build, and confirm the new tests compile and fail only where the stubs and the missing scenario make them fail.

### Task 7: Gather solids and lines

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Replace the includes with:

```cpp
#include "sim/park/edits.h"

#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>
```

Step 2: Before addParkEdits, add an anonymous namespace holding:

```cpp
constexpr double HALF_PARK = PARK_SIZE / 2.0;

// A footprint the check compares, with the pose and size it came from.
struct Solid {
  EntityKey Key = NULL_KEY;
  Pose At;
  FootprintSize Size;
  Footprint Shape;
};

// A path's ground line, with half the path's width.
struct Line {
  std::vector<CarrierPoint> Points;
  double HalfWidth = 0.0;
};

std::optional<Solid> solidOf(EntityKey key, const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> shape = footprintOf(pose, size);
  if (!shape) {
    return std::nullopt;
  }
  return Solid{key, pose, size, *shape};
}

std::optional<Line> lineOf(PathKind kind, const std::vector<ParkPoint> &points) {
  std::vector<CarrierPoint> line = groundLine(points);
  if (line.empty()) {
    return std::nullopt;
  }
  return Line{std::move(line), pathWidth(kind) / 2.0};
}

// Every entrance's and box's footprint, skipping any with none.
std::vector<Solid> solidsOf(const World &world) {
  std::vector<Solid> solids;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    if (const std::optional<Solid> solid = solidOf(entrance.Key, entrance.At, ENTRANCE_SIZE)) {
      solids.push_back(*solid);
    }
  }
  for (const ParkBox &box : parkBoxes(world)) {
    if (const std::optional<Solid> solid = solidOf(box.Key, box.At, boxSize(box.Kind))) {
      solids.push_back(*solid);
    }
  }
  return solids;
}

// Every path's ground line, skipping any that is empty.
std::vector<Line> linesOf(const World &world) {
  std::vector<Line> lines;
  for (const ParkPath &path : parkPaths(world)) {
    if (std::optional<Line> line = lineOf(path.Kind, path.Points)) {
      lines.push_back(std::move(*line));
    }
  }
  return lines;
}

// The intent T the key holds, or null.
template <typename T> const T *findIntent(const World &world, EntityKey key) {
  const entt::entity entity = world.findEntity(key);
  return entity == entt::null ? nullptr : world.Registry.try_get<T>(entity);
}

// Whether the kind is one of its enum's values.
template <typename Kind> bool isKnown(Kind kind) {
  return static_cast<size_t>(kind) < enumNames(kind).size();
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: warnings only for the helpers not yet used, which Task 9 uses. If clang-tidy or -Werror stops the build on an unused function, continue with Task 8 and build after it.

### Task 8: Judge conflicts

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: At the end of the anonymous namespace, add:

```cpp
bool leavesPark(const Solid &solid) {
  constexpr double LIMIT = HALF_PARK + CONTACT_TOLERANCE;
  return std::ranges::any_of(solid.Shape.Corners, [](const ParkPoint &corner) {
    return std::abs(corner.X) > LIMIT || std::abs(corner.Z) > LIMIT;
  });
}

bool leavesPark(const Line &line) {
  const double limit = (HALF_PARK - line.HalfWidth) + CONTACT_TOLERANCE;
  return std::ranges::any_of(line.Points, [limit](const CarrierPoint &point) {
    return std::abs(point.X) > limit || std::abs(point.Z) > limit;
  });
}

// The least and greatest projections of the footprint's corners onto the axis.
std::pair<double, double> extentAlong(const Footprint &footprint, const ParkPoint &axis) {
  const auto project = [&axis](const ParkPoint &corner) {
    return corner.X * axis.X + corner.Z * axis.Z;
  };
  double least = project(footprint.Corners[0]);
  double greatest = least;
  for (const ParkPoint &corner : footprint.Corners) {
    least = std::min(least, project(corner));
    greatest = std::max(greatest, project(corner));
  }
  return {least, greatest};
}

// Whether the two footprints' extents along the axis share more than CONTACT_TOLERANCE.
bool overlapAlong(const Footprint &a, const Footprint &b, const ParkPoint &axis) {
  const auto [aLeast, aGreatest] = extentAlong(a, axis);
  const auto [bLeast, bGreatest] = extentAlong(b, axis);
  return std::min(aGreatest, bGreatest) - std::max(aLeast, bLeast) > CONTACT_TOLERANCE;
}

// The separating axis test: no axis of either footprint separates them.
bool overlaps(const Solid &a, const Solid &b) {
  return overlapAlong(a.Shape, b.Shape, a.Shape.Forward) &&
         overlapAlong(a.Shape, b.Shape, a.Shape.Right) &&
         overlapAlong(a.Shape, b.Shape, b.Shape.Forward) &&
         overlapAlong(a.Shape, b.Shape, b.Shape.Right);
}

// The point in the solid's frame: X along its Forward and Z along its Right, from its position.
ParkPoint toLocal(const Solid &solid, const CarrierPoint &point) {
  const double dx = point.X - solid.At.X;
  const double dz = point.Z - solid.At.Z;
  return {dx * solid.Shape.Forward.X + dz * solid.Shape.Forward.Z,
          dx * solid.Shape.Right.X + dz * solid.Shape.Right.Z};
}

// Whether the segment from a to b enters the box |x| <= halfX, |z| <= halfZ: Liang and Barsky's
// clip against both slabs.
bool entersBox(const ParkPoint &a, const ParkPoint &b, double halfX, double halfZ) {
  double enter = 0.0;
  double leave = 1.0;
  const auto clip = [&enter, &leave](double start, double delta, double half) {
    if (delta == 0.0) {
      return std::abs(start) <= half;
    }
    double low = (-half - start) / delta;
    double high = (half - start) / delta;
    if (low > high) {
      std::swap(low, high);
    }
    enter = std::max(enter, low);
    leave = std::min(leave, high);
    return enter <= leave;
  };
  return clip(a.X, b.X - a.X, halfX) && clip(a.Z, b.Z - a.Z, halfZ);
}

double squaredDistanceToBox(const ParkPoint &point, double halfX, double halfZ) {
  const double dx = std::max(std::abs(point.X) - halfX, 0.0);
  const double dz = std::max(std::abs(point.Z) - halfZ, 0.0);
  return dx * dx + dz * dz;
}

double squaredDistanceToSegment(const ParkPoint &point, const ParkPoint &a, const ParkPoint &b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  const double length = dx * dx + dz * dz;
  double t = 0.0;
  if (length > 0.0) {
    t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / length, 0.0, 1.0);
  }
  const double ex = a.X + dx * t - point.X;
  const double ez = a.Z + dz * t - point.Z;
  return ex * ex + ez * ez;
}

// Whether some segment of the line lies nearer the solid's rectangle than its half width less
// CONTACT_TOLERANCE. A segment that enters the rectangle lies at distance zero; otherwise the
// least distance is from an end to the rectangle or from a corner to the segment.
bool meets(const Line &line, const Solid &solid) {
  const double halfDepth = solid.Size.Depth / 2.0;
  const double halfWidth = solid.Size.Width / 2.0;
  const double reach = line.HalfWidth - CONTACT_TOLERANCE;
  const std::array<ParkPoint, 4> corners{ParkPoint{halfDepth, -halfWidth},
                                         ParkPoint{halfDepth, halfWidth},
                                         ParkPoint{-halfDepth, halfWidth},
                                         ParkPoint{-halfDepth, -halfWidth}};
  for (size_t i = 1; i < line.Points.size(); ++i) {
    const ParkPoint a = toLocal(solid, line.Points[i - 1]);
    const ParkPoint b = toLocal(solid, line.Points[i]);
    if (entersBox(a, b, halfDepth, halfWidth)) {
      return true;
    }
    double nearest = std::min(squaredDistanceToBox(a, halfDepth, halfWidth),
                              squaredDistanceToBox(b, halfDepth, halfWidth));
    for (const ParkPoint &corner : corners) {
      nearest = std::min(nearest, squaredDistanceToSegment(corner, a, b));
    }
    if (nearest < reach * reach) {
      return true;
    }
  }
  return false;
}

// Whether the solid stays in the park and conflicts with no line and no other solid. A solid
// holding the same key is the one being moved, so it is skipped.
bool fits(const Solid &solid, const std::vector<Solid> &solids, const std::vector<Line> &lines) {
  if (leavesPark(solid)) {
    return false;
  }
  if (std::ranges::any_of(solids, [&solid](const Solid &other) {
        return other.Key != solid.Key && overlaps(solid, other);
      })) {
    return false;
  }
  return std::ranges::none_of(lines, [&solid](const Line &line) { return meets(line, solid); });
}

// Whether the line stays in the park and meets no solid.
bool fits(const Line &line, const std::vector<Solid> &solids) {
  return !leavesPark(line) &&
         std::ranges::none_of(solids, [&line](const Solid &solid) { return meets(line, solid); });
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: only unused-function diagnostics, which Task 9 clears.

### Task 9: Accept, apply, and validate

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Replace the stubbed isAccepted, applyCommand, and isPhysicallyValid definitions with:

```cpp
bool isAccepted(const World &world, const AddPath &command) {
  if (!isKnown(command.Kind)) {
    return false;
  }
  const std::optional<Line> line = lineOf(command.Kind, keptPoints(command.Points));
  return line && fits(*line, solidsOf(world));
}

bool isAccepted(const World &world, const AddBox &command) {
  if (!isKnown(command.Kind)) {
    return false;
  }
  const std::optional<Solid> solid = solidOf(NULL_KEY, command.At, boxSize(command.Kind));
  return solid && fits(*solid, solidsOf(world), linesOf(world));
}

bool isAccepted(const World &world, const MoveBox &command) {
  const BoxIntent *box = findIntent<BoxIntent>(world, command.Box);
  if (box == nullptr) {
    return false;
  }
  const std::optional<Solid> solid = solidOf(command.Box, command.At, boxSize(box->Kind));
  return solid && fits(*solid, solidsOf(world), linesOf(world));
}

bool isAccepted(const World &world, const DeletePath &command) {
  return findIntent<PathIntent>(world, command.Path) != nullptr;
}

bool isAccepted(const World &world, const DeleteBox &command) {
  return findIntent<BoxIntent>(world, command.Box) != nullptr;
}

void applyCommand(World &world, const AddPath &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  const EntityKey key = world.createEntity();
  world.Registry.emplace<PathIntent>(world.findEntity(key),
                                     PathIntent{command.Kind, keptPoints(command.Points)});
}

void applyCommand(World &world, const AddBox &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  const EntityKey key = world.createEntity();
  world.Registry.emplace<BoxIntent>(world.findEntity(key), BoxIntent{command.Kind, command.At});
}

void applyCommand(World &world, const MoveBox &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  world.Registry.get<BoxIntent>(world.findEntity(command.Box)).At = command.At;
}

void applyCommand(World &world, const DeletePath &command) {
  if (isAccepted(world, command)) {
    world.destroyEntity(command.Path);
  }
}

void applyCommand(World &world, const DeleteBox &command) {
  if (isAccepted(world, command)) {
    world.destroyEntity(command.Box);
  }
}

bool isPhysicallyValid(const World &world) {
  const std::vector<Solid> solids = solidsOf(world);
  const std::vector<Line> lines = linesOf(world);
  // An entrance or box with no footprint, or a path with an empty ground line, is not valid.
  if (solids.size() != parkEntrances(world).size() + parkBoxes(world).size() ||
      lines.size() != parkPaths(world).size()) {
    return false;
  }
  for (size_t i = 0; i < solids.size(); ++i) {
    if (leavesPark(solids[i])) {
      return false;
    }
    for (size_t j = i + 1; j < solids.size(); ++j) {
      if (overlaps(solids[i], solids[j])) {
        return false;
      }
    }
  }
  return std::ranges::all_of(lines, [&solids](const Line &line) { return fits(line, solids); });
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

Run: `ctest --preset linux-debug`
Expected: every test passes except the tests of the park-edits scenario.

### Task 10: Add the park-edits scenario

Files:
- Create: `src/scenarios/park_edits.cpp`
- Modify: `src/scenarios/synthetic.h`
- Modify: `src/scenarios/scenarios.cpp`
- Modify: `src/scenarios/CMakeLists.txt`

Step 1: Create src/scenarios/park_edits.cpp:

```cpp
#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/world.h"

#include <array>
#include <stddef.h>
#include <stdint.h>

namespace tpj {
namespace {

constexpr uint64_t EDIT_PURPOSE = hashName("park-edits");
constexpr uint64_t EDIT_INTERVAL = 10;
// Coordinates reach 8 m past the park's edge, so some edits leave it.
constexpr double REACH = PARK_SIZE / 2.0 + 8.0;
// A path's next point lies at most this far from its last along each axis.
constexpr double PATH_STEP = 40.0;
// Weights of AddPath, AddBox, MoveBox, DeletePath, and DeleteBox.
constexpr std::array<uint64_t, 5> EDIT_WEIGHTS{3, 4, 3, 1, 1};

// The cycle's next draw, in [0, 1).
double drawEdit(const World &world, uint64_t &index) {
  return drawUniform(drawKey(world, NULL_KEY, EDIT_PURPOSE, index++));
}

// A draw from -reach to reach.
double drawAround(const World &world, uint64_t &index, double reach) {
  return (2.0 * drawEdit(world, index) - 1.0) * reach;
}

// Any key the counter has given, live or not.
EntityKey drawGivenKey(const World &world, uint64_t &index) {
  const double given = static_cast<double>(world.nextKey() - 1);
  return static_cast<EntityKey>(1 + static_cast<uint64_t>(drawEdit(world, index) * given));
}

// A braced list evaluates its elements in order, so the draws are made in a fixed order.
Pose drawPose(const World &world, uint64_t &index) {
  return Pose{drawAround(world, index, REACH), drawAround(world, index, REACH),
              drawAround(world, index, 1.0), drawAround(world, index, 1.0)};
}

AddPath drawPath(const World &world, uint64_t &index) {
  AddPath path{drawEdit(world, index) < 0.5 ? PathKind::Guest : PathKind::Backstage, {}};
  const size_t count = 2 + static_cast<size_t>(drawEdit(world, index) * 4.0);
  ParkPoint point{drawAround(world, index, REACH), drawAround(world, index, REACH)};
  path.Points.push_back(point);
  while (path.Points.size() < count) {
    // One point in eight repeats the last, which the command drops.
    if (drawEdit(world, index) >= 0.125) {
      point = ParkPoint{point.X + drawAround(world, index, PATH_STEP),
                        point.Z + drawAround(world, index, PATH_STEP)};
    }
    path.Points.push_back(point);
  }
  return path;
}

// Before every tenth cycle, one drawn edit of any kind.
void queueEdit(const World &world, CommandQueue &commands) {
  if (world.Tick % EDIT_INTERVAL != 0) {
    return;
  }
  uint64_t index = 0;
  const size_t kind = drawPick(drawKey(world, NULL_KEY, EDIT_PURPOSE, index++), EDIT_WEIGHTS);
  if (kind == 0) {
    commands.push(drawPath(world, index));
  } else if (kind == 1) {
    const BoxKind box = drawEdit(world, index) < 0.75 ? BoxKind::Shop : BoxKind::Depot;
    commands.push(AddBox{box, drawPose(world, index)});
  } else if (kind == 2) {
    const EntityKey box = drawGivenKey(world, index);
    commands.push(MoveBox{box, drawPose(world, index)});
  } else if (kind == 3) {
    commands.push(DeletePath{drawGivenKey(world, index)});
  } else {
    commands.push(DeleteBox{drawGivenKey(world, index)});
  }
}

} // namespace

Scenario parkEditsScenario() {
  return Scenario{
      .Name = "park-edits",
      .Seed = 4004,
      .MakeSchema = makeParkSchema,
      .Populate = [](World &world) { world = makeNewPark(world.Seed); },
      .QueueCommands = queueEdit,
  };
}

} // namespace tpj
```

Step 2: In src/scenarios/synthetic.h, declare `Scenario parkEditsScenario();` after `Scenario stallsScenario();`.

Step 3: In src/scenarios/scenarios.cpp, make the array `std::array<Scenario, 4>` and append `parkEditsScenario()` after `stallsScenario()`.

Step 4: In src/scenarios/CMakeLists.txt, add `park_edits.cpp` to tpj_scenarios_lib's sources, after `beacons.cpp`.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

Run: `ctest --preset linux-debug`
Expected: every test passes. If the scenario's test finds no accepted move or no refused add over the run, raise EDIT_WEIGHTS' MoveBox weight rather than changing the check.

### Task 11: Verify both builds

Step 1: Format.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`

Step 2: Linux.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"` and `ctest --preset linux-debug`
Expected: no diagnostics, and every test passes.

Step 3: Windows.

Run: `cmake.exe --build --preset windows-debug` and `ctest.exe --preset windows-debug`
Expected: builds, and every test passes.

Step 4: Cross-build.

Run: `scripts/cross-build-check.sh`
Expected: ends with "both builds wrote the same N lines; passed.", N being about 3001 more than before.

### Task 12: Commit

Commit every change of this feature, plans excluded (they were committed at approval), as one commit through the commit-hygiene skill:

```
Sim: Add park edit commands and the physical-validity check
```

with the body naming the five commands, isAccepted and isPhysicallyValid, the contact tolerance, and the park-edits scenario, and the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
