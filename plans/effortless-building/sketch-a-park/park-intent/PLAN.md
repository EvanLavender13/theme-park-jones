# Implementation Plan: Park Intent

## Goal

Add src/sim/park with entrance, path, and box intent registered in makeParkSchema, private components behind public queries, ground lines, footprints, the new-park template, and tests/parks/new.park.

## Approach

The intent components are plain structs with visitFields in the module's internal header, registered as intent by addParkIntent, and read by other modules through key-ordered queries that walk world.keys() and return public values. groundLine drops near-repeated points and evaluates each centripetal segment with the Barry and Goldman pyramid, writing the clicked points themselves at segment ends so the line passes through them bit for bit. footprintOf normalizes the facing by its larger magnitude before the square root, so every finite nonzero facing gives a unit direction.

## Tasks

### Task 1: Write the park spec

Files:
- Create: `src/sim/park/SPEC.md`
- Modify: `src/sim/SPEC.md`

Step 1: Create src/sim/park/SPEC.md with the exact text in FEATURE.md's Spec changes, without the surrounding code fence.

Step 2: In src/sim/SPEC.md, replace "It registers the medium's types first (sim/medium/SPEC.md), and so far nothing else." with "It registers the medium's types first (sim/medium/SPEC.md), then park intent (sim/park/SPEC.md)."

### Task 2: Add the intent header

Files:
- Create: `src/sim/park/intent.h`

Step 1: Create the header:

```cpp
#ifndef TPJ_SIM_PARK_INTENT_H
#define TPJ_SIM_PARK_INTENT_H

#include "sim/entity_key.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <array>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

// The park is a square this many meters on a side, centered on the origin of the ground.
inline constexpr double PARK_SIZE = 256.0;

// A position on the ground, in meters.
struct ParkPoint {
  double X = 0.0;
  double Z = 0.0;

  bool operator==(const ParkPoint &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, ParkPoint &point) {
  visitor.field("x", point.X);
  visitor.field("z", point.Z);
}

enum class PathKind : uint8_t { Guest, Backstage };

constexpr std::array<std::string_view, 2> enumNames(PathKind /*value*/) {
  return {"guest", "backstage"};
}

enum class BoxKind : uint8_t { Shop, Depot };

constexpr std::array<std::string_view, 2> enumNames(BoxKind /*value*/) { return {"shop", "depot"}; }

// A position and a facing, a direction on the ground held as given, not normalized.
struct Pose {
  double X = 0.0;
  double Z = 0.0;
  double FacingX = 0.0;
  double FacingZ = -1.0;

  bool operator==(const Pose &) const = default;
};

// A footprint's width, across its facing, and depth, along it, in meters.
struct FootprintSize {
  double Width = 0.0;
  double Depth = 0.0;
};

constexpr double pathWidth(PathKind kind) { return kind == PathKind::Guest ? 3.0 : 2.0; }

constexpr FootprintSize boxSize(BoxKind kind) {
  return kind == BoxKind::Shop ? FootprintSize{8.0, 6.0} : FootprintSize{12.0, 8.0};
}

inline constexpr FootprintSize ENTRANCE_SIZE{10.0, 3.0};

// An entrance's intent, as other modules read it.
struct ParkEntrance {
  EntityKey Key = NULL_KEY;
  Pose At;

  bool operator==(const ParkEntrance &) const = default;
};

// A path's intent, as other modules read it: its kind and the points the player clicked, in order.
struct ParkPath {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;

  bool operator==(const ParkPath &) const = default;
};

// A box's intent, as other modules read it.
struct ParkBox {
  EntityKey Key = NULL_KEY;
  BoxKind Kind = BoxKind::Shop;
  Pose At;

  bool operator==(const ParkBox &) const = default;
};

// Registers the private intent component types entrance, path, and box, in that order.
void addParkIntent(WorldSchema &schema);

// Every entity holding that kind of intent, in ascending key order.
std::vector<ParkEntrance> parkEntrances(const World &world);
std::vector<ParkPath> parkPaths(const World &world);
std::vector<ParkBox> parkBoxes(const World &world);

// The new-park template with makeParkSchema's schema and the seed, resolution pending.
World makeNewPark(uint64_t seed);

} // namespace tpj

#endif
```

### Task 3: Add the private intent components

Files:
- Create: `src/sim/park/internal/components.h`

Step 1: Create the header, whose types only files under src/sim/park may name (decision 0016):

```cpp
#ifndef TPJ_SIM_PARK_INTERNAL_COMPONENTS_H
#define TPJ_SIM_PARK_INTERNAL_COMPONENTS_H

#include "sim/park/intent.h"

#include <vector>

namespace tpj {

// A path the player drew: its kind and the points they clicked, in order.
struct PathIntent {
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;

  bool operator==(const PathIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, PathIntent &path) {
  visitor.field("kind", path.Kind);
  visitor.field("points", path.Points);
}

// A box the player placed.
struct BoxIntent {
  BoxKind Kind = BoxKind::Shop;
  Pose At;

  bool operator==(const BoxIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, BoxIntent &box) {
  visitor.field("kind", box.Kind);
  visitor.field("x", box.At.X);
  visitor.field("z", box.At.Z);
  visitor.field("facing-x", box.At.FacingX);
  visitor.field("facing-z", box.At.FacingZ);
}

// Where guests arrive and leave.
struct EntranceIntent {
  Pose At;

  bool operator==(const EntranceIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, EntranceIntent &entrance) {
  visitor.field("x", entrance.At.X);
  visitor.field("z", entrance.At.Z);
  visitor.field("facing-x", entrance.At.FacingX);
  visitor.field("facing-z", entrance.At.FacingZ);
}

} // namespace tpj

#endif
```

### Task 4: Add the geometry header

Files:
- Create: `src/sim/park/geometry.h`

Step 1: Create the header:

```cpp
#ifndef TPJ_SIM_PARK_GEOMETRY_H
#define TPJ_SIM_PARK_GEOMETRY_H

#include "sim/medium/network.h"
#include "sim/park/intent.h"

#include <array>
#include <optional>
#include <vector>

namespace tpj {

// A point closer than this to the last kept point of a path is dropped.
inline constexpr double MIN_POINT_SPACING = 0.01;
// A ground line segment takes one point per this many meters of chord, and at least
// MIN_SEGMENT_SAMPLES.
inline constexpr double GROUND_LINE_SPACING = 1.0;
inline constexpr int MIN_SEGMENT_SAMPLES = 8;

// The path's centripetal Catmull-Rom curve through its points as a line with distances, or an empty
// line when a point is not finite or outside the park, or fewer than two points are kept. See
// sim/park/SPEC.md.
std::vector<CarrierPoint> groundLine(const std::vector<ParkPoint> &points);

// The rectangle a box or entrance covers. Forward and Right are unit directions.
struct Footprint {
  ParkPoint Forward;
  ParkPoint Right;
  // Front left, front right, back right, back left.
  std::array<ParkPoint, 4> Corners;
};

// The pose's footprint for the size, or none when the pose is not finite or its facing has zero
// length.
std::optional<Footprint> footprintOf(const Pose &pose, FootprintSize size);

} // namespace tpj

#endif
```

### Task 5: Add stub definitions and register the intent

Files:
- Create: `src/sim/park/intent.cpp`
- Create: `src/sim/park/geometry.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park_schema.cpp`

Step 1: Create src/sim/park/intent.cpp with the real registration and stub queries and template:

```cpp
#include "sim/park/intent.h"

#include "sim/park/internal/components.h"
#include "sim/park_schema.h"

namespace tpj {

void addParkIntent(WorldSchema &schema) {
  schema.addComponent<EntranceIntent>("entrance", DataKind::Intent);
  schema.addComponent<PathIntent>("path", DataKind::Intent);
  schema.addComponent<BoxIntent>("box", DataKind::Intent);
}

std::vector<ParkEntrance> parkEntrances(const World & /*world*/) { return {}; }

std::vector<ParkPath> parkPaths(const World & /*world*/) { return {}; }

std::vector<ParkBox> parkBoxes(const World & /*world*/) { return {}; }

World makeNewPark(uint64_t seed) { return World(makeParkSchema(), seed); }

} // namespace tpj
```

Step 2: Create src/sim/park/geometry.cpp with stubs:

```cpp
#include "sim/park/geometry.h"

namespace tpj {

std::vector<CarrierPoint> groundLine(const std::vector<ParkPoint> & /*points*/) { return {}; }

std::optional<Footprint> footprintOf(const Pose & /*pose*/, FootprintSize /*size*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 3: In src/sim/CMakeLists.txt, add `park/geometry.cpp` and `park/intent.cpp` to tpj_sim's sources, after `musl_log.cpp` and before `park_schema.cpp`.

Step 4: In src/sim/park_schema.cpp, include `"sim/park/intent.h"` after `"sim/medium/network.h"`, and call `addParkIntent(*schema);` after `addNetworkComponent(*schema);`, replacing the comment above them with:

```cpp
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them, in dependency order, starting with park intent.
```

Step 5: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics, and every test passes.

### Task 6: Update a stale test comment

Files:
- Modify: `tests/scenarios/main_test.cpp:119-120`

Step 1: Replace the comment

```cpp
// Saves makeParkSchema loads while it registers only the network type, which is derived and never
// saved: a header alone, and one listing entities.
```

with

```cpp
// Saves holding no intent or state, which makeParkSchema loads: a header alone, and one listing
// entities.
```

### Task 7: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, src/sim/park/SPEC.md, src/sim/SPEC.md, and src/sim/medium/SPEC.md as the specs, and src/sim/park/intent.h and src/sim/park/geometry.h as the public headers. The tests reach intent only through those headers, loading saves to put intent in a world, since src/sim/park/internal/ is private to the module. Then reconfigure and build, since the test pass adds files to tests/sim/CMakeLists.txt.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics. The tests for criterion 1 pass, since registration is real. The tests for criteria 2 to 7 fail on their assertions, because the stubs give no entities, empty lines, and no footprints, and new.park does not exist yet.

### Task 8: Implement the queries and the template

Files:
- Modify: `src/sim/park/intent.cpp`
- Create: `tests/parks/new.park`

Step 1: In src/sim/park/intent.cpp, add `#include <utility>` and `#include <vector>` after the project includes, and add after the includes, inside namespace tpj:

```cpp
namespace {

// Every entity holding the component T, made public by toValue, in ascending key order.
template <typename T, typename ToValue>
auto collectIntent(const World &world, ToValue toValue) {
  std::vector<decltype(toValue(EntityKey{}, std::declval<const T &>()))> result;
  for (const EntityKey key : world.keys()) {
    if (const T *intent = world.Registry.try_get<T>(world.findEntity(key)); intent != nullptr) {
      result.push_back(toValue(key, *intent));
    }
  }
  return result;
}

} // namespace
```

Step 2: Replace the three query stubs and the template stub with:

```cpp
std::vector<ParkEntrance> parkEntrances(const World &world) {
  return collectIntent<EntranceIntent>(world, [](EntityKey key, const EntranceIntent &entrance) {
    return ParkEntrance{key, entrance.At};
  });
}

std::vector<ParkPath> parkPaths(const World &world) {
  return collectIntent<PathIntent>(world, [](EntityKey key, const PathIntent &path) {
    return ParkPath{key, path.Kind, path.Points};
  });
}

std::vector<ParkBox> parkBoxes(const World &world) {
  return collectIntent<BoxIntent>(world, [](EntityKey key, const BoxIntent &box) {
    return ParkBox{key, box.Kind, box.At};
  });
}

World makeNewPark(uint64_t seed) {
  World world(makeParkSchema(), seed);
  // The entrance's back lies on the edge at z = 128, and it faces into the park.
  const EntityKey entrance = world.createEntity();
  world.Registry.emplace<EntranceIntent>(world.findEntity(entrance),
                                         EntranceIntent{Pose{0.0, 126.5, 0.0, -1.0}});
  // A guest path from 2 m in front of the entrance, 20 m into the park.
  const EntityKey path = world.createEntity();
  world.Registry.emplace<PathIntent>(world.findEntity(path),
                                     PathIntent{PathKind::Guest, {{0.0, 123.0}, {0.0, 103.0}}});
  return world;
}
```

Step 3: Create tests/parks/new.park with exactly these lines, each ending with a line feed:

```
tpj-park 1
seed 1
tick 0
next-key 3

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path]
2 kind=guest points=[{x=0 z=123} {x=0 z=103}]
```

Step 4: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics. The tests for criteria 1 and 2 pass. The tests for criterion 7 that need only the template and new.park pass, and those that measure its geometry still fail. Those for criteria 3 to 6 still fail.

### Task 9: Implement footprints

Files:
- Modify: `src/sim/park/geometry.cpp`

Step 1: Add `#include <algorithm>` and `#include <cmath>` after the matching header, and replace the footprintOf stub with:

```cpp
std::optional<Footprint> footprintOf(const Pose &pose, FootprintSize size) {
  if (!std::isfinite(pose.X) || !std::isfinite(pose.Z) || !std::isfinite(pose.FacingX) ||
      !std::isfinite(pose.FacingZ)) {
    return std::nullopt;
  }
  // Dividing by the larger magnitude first keeps the sum of squares between 1 and 2, so it neither
  // overflows nor underflows.
  const double larger = std::max(std::abs(pose.FacingX), std::abs(pose.FacingZ));
  if (larger == 0.0) {
    return std::nullopt;
  }
  const double x = pose.FacingX / larger;
  const double z = pose.FacingZ / larger;
  const double length = std::sqrt(x * x + z * z);
  const ParkPoint forward{x / length, z / length};
  const ParkPoint right{-forward.Z, forward.X};
  const double halfWidth = size.Width / 2.0;
  const double halfDepth = size.Depth / 2.0;
  const auto corner = [&](double along, double across) {
    return ParkPoint{pose.X + forward.X * along + right.X * across,
                     pose.Z + forward.Z * along + right.Z * across};
  };
  return Footprint{forward,
                   right,
                   {corner(halfDepth, -halfWidth), corner(halfDepth, halfWidth),
                    corner(-halfDepth, halfWidth), corner(-halfDepth, -halfWidth)}};
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics. The tests for criteria 1, 2, and 6 pass. Those for criteria 3 to 5, and criterion 7's ground line measurements, still fail.

### Task 10: Implement ground lines

Files:
- Modify: `src/sim/park/geometry.cpp`

Step 1: Add `#include <stddef.h>` to the system includes, and add after them, inside namespace tpj:

```cpp
namespace {

bool isInsidePark(const ParkPoint &point) {
  constexpr double HALF = PARK_SIZE / 2.0;
  return std::isfinite(point.X) && std::isfinite(point.Z) && point.X >= -HALF &&
         point.X <= HALF && point.Z >= -HALF && point.Z <= HALF;
}

double chord(const ParkPoint &from, const ParkPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// The points in order, without any closer than MIN_POINT_SPACING to the last kept one.
std::vector<ParkPoint> keptPoints(const std::vector<ParkPoint> &points) {
  constexpr double MIN_SQUARED = MIN_POINT_SPACING * MIN_POINT_SPACING;
  std::vector<ParkPoint> kept;
  for (const ParkPoint &point : points) {
    if (!kept.empty()) {
      const double dx = point.X - kept.back().X;
      const double dz = point.Z - kept.back().Z;
      if (dx * dx + dz * dz < MIN_SQUARED) {
        continue;
      }
    }
    kept.push_back(point);
  }
  return kept;
}

// The neighbor beyond an end of the path: the end's other neighbor reflected through it.
ParkPoint phantom(const ParkPoint &end, const ParkPoint &neighbor) {
  return {2.0 * end.X - neighbor.X, 2.0 * end.Z - neighbor.Z};
}

ParkPoint blend(double fromWeight, const ParkPoint &from, double toWeight, const ParkPoint &to) {
  return {fromWeight * from.X + toWeight * to.X, fromWeight * from.Z + toWeight * to.Z};
}

// The curve between p[1] and p[2] at t, from t1 to t2: the Barry and Goldman pyramid.
ParkPoint evaluateSegment(const std::array<ParkPoint, 4> &p, const std::array<double, 4> &knots,
                          double t) {
  const auto [t0, t1, t2, t3] = knots;
  const ParkPoint a1 = blend((t1 - t) / (t1 - t0), p[0], (t - t0) / (t1 - t0), p[1]);
  const ParkPoint a2 = blend((t2 - t) / (t2 - t1), p[1], (t - t1) / (t2 - t1), p[2]);
  const ParkPoint a3 = blend((t3 - t) / (t3 - t2), p[2], (t - t2) / (t3 - t2), p[3]);
  const ParkPoint b1 = blend((t2 - t) / (t2 - t0), a1, (t - t0) / (t2 - t0), a2);
  const ParkPoint b2 = blend((t3 - t) / (t3 - t1), a2, (t - t1) / (t3 - t1), a3);
  return blend((t2 - t) / (t2 - t1), b1, (t - t1) / (t2 - t1), b2);
}

// Appends the point with its distance: the previous distance plus the step's length.
void appendPoint(std::vector<CarrierPoint> &line, const ParkPoint &point) {
  double distance = 0.0;
  if (!line.empty()) {
    const CarrierPoint &last = line.back();
    const double dx = point.X - last.X;
    const double dz = point.Z - last.Z;
    distance = last.Distance + std::sqrt(dx * dx + dz * dz);
  }
  line.push_back(CarrierPoint{point.X, point.Z, distance});
}

} // namespace
```

Step 2: Replace the groundLine stub with:

```cpp
std::vector<CarrierPoint> groundLine(const std::vector<ParkPoint> &points) {
  if (!std::ranges::all_of(points, isInsidePark)) {
    return {};
  }
  const std::vector<ParkPoint> kept = keptPoints(points);
  if (kept.size() < 2) {
    return {};
  }
  std::vector<CarrierPoint> line;
  const size_t last = kept.size() - 1;
  for (size_t i = 0; i < last; ++i) {
    const std::array<ParkPoint, 4> p{i == 0 ? phantom(kept[0], kept[1]) : kept[i - 1], kept[i],
                                     kept[i + 1],
                                     i + 1 == last ? phantom(kept[last], kept[last - 1])
                                                   : kept[i + 2]};
    std::array<double, 4> knots{};
    for (size_t j = 1; j < knots.size(); ++j) {
      knots[j] = knots[j - 1] + std::sqrt(chord(p[j - 1], p[j]));
    }
    const int samples = static_cast<int>(
        std::max(static_cast<double>(MIN_SEGMENT_SAMPLES), std::ceil(chord(p[1], p[2]) / GROUND_LINE_SPACING)));
    // The segment's first point is the kept point itself, so the line passes through it exactly.
    appendPoint(line, kept[i]);
    for (int k = 1; k < samples; ++k) {
      const double t = knots[1] + (knots[2] - knots[1]) * static_cast<double>(k) /
                                      static_cast<double>(samples);
      appendPoint(line, evaluateSegment(p, knots, t));
    }
  }
  appendPoint(line, kept[last]);
  return line;
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics, and every test passes.

### Task 11: Verify both builds

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
Expected: no output.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics, and every test passes, the sim symbol check and the private header check included.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds and every test passes.

Run: `scripts/cross-build-check.sh`
Expected: both builds run every scenario and tests/parks/new.park, and it reports that they wrote the same lines.

### Task 12: Commit

Via the commit-hygiene skill, stage everything and commit once with the subject `Sim: Add park intent, ground lines, and footprints` and the Co-Authored-By trailer.
