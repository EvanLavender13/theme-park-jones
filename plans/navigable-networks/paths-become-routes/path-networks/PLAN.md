# Implementation Plan: Path Networks

## Goal

Derive the park's guest and backstage networks from its paths in a new module, src/sim/routes, with nodes at path ends and wherever same-kind lines meet, and check in tests/parks/routes.park.

## Approach

A resolver registered after park edits copies parkPaths, builds one carrier per path of each kind from its ground line, and puts each kind's Network on a derived entity keyed by the kind. Meetings come from a pairwise test of segments with a bounding-box reject: a proper crossing by the orientation test, and each end within JUNCTION_TOLERANCE of the other segment. Each carrier's meeting distances are grouped greedily into stops, a union-find over stops joins the two stops of each meeting, and nodes are numbered by walking carriers in key order and stops by distance.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify the routes module

Files:
- Create: `src/sim/routes/SPEC.md`

Step 1: Create the file with the text FEATURE.md gives under "Create src/sim/routes/SPEC.md", without the four-space indent.

Run: `grep -c "^## " src/sim/routes/SPEC.md`
Expected: `3`

### Task 2: Name the routes module in the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Replace the sentence "It registers the medium's types first (sim/medium/SPEC.md), then park intent and its commands (sim/park/SPEC.md)." with the sentence FEATURE.md gives.

Run: `grep -c "sim/routes/SPEC.md" src/sim/SPEC.md`
Expected: `1`

### Task 3: Declare the routes module

Files:
- Create: `src/sim/routes/networks.h`
- Create: `src/sim/routes/networks.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park_schema.cpp`

Step 1: Create `src/sim/routes/networks.h`:

```cpp
#ifndef TPJ_SIM_ROUTES_NETWORKS_H
#define TPJ_SIM_ROUTES_NETWORKS_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/intent.h"

#include <stdint.h>

namespace tpj {

class World;
class WorldSchema;

// Two lines of a kind that come this close meet, and a carrier's meetings this close are one stop.
inline constexpr double JUNCTION_TOLERANCE = 0.001;

inline constexpr uint64_t NETWORK_PURPOSE = hashName("network");

// The key of the derived entity holding the kind's network.
constexpr EntityKey networkKey(PathKind kind) {
  return deriveKey(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
}

// The kind's network as the last resolution derived it, or an empty network when the world holds
// none. See sim/routes/SPEC.md.
const Network &parkNetwork(const World &world, PathKind kind);

// Registers the resolver path-networks.
void addRoutes(WorldSchema &schema);

} // namespace tpj

#endif
```

Step 2: Create `src/sim/routes/networks.cpp` with stubs:

```cpp
#include "sim/routes/networks.h"

#include "sim/schema.h"
#include "sim/world.h"

namespace tpj {

namespace {

void resolvePathNetworks(World & /*world*/) {}

} // namespace

const Network &parkNetwork(const World & /*world*/, PathKind /*kind*/) {
  static const Network EMPTY;
  return EMPTY;
}

void addRoutes(WorldSchema &schema) { schema.addResolver("path-networks", &resolvePathNetworks); }

} // namespace tpj
```

Step 3: In `src/sim/CMakeLists.txt`, add `routes/networks.cpp` to tpj_sim's sources after `park_schema.cpp`.

Step 4: In `src/sim/park_schema.cpp`, add `#include "sim/routes/networks.h"` after `#include "sim/park/intent.h"`, add `addRoutes(*schema);` after `addParkEdits(*schema);`, and change the comment's last words "starting with park intent and its commands." to "starting with park intent and its commands, then the routes that derive the park's networks."

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; cmake --build --preset linux-debug --target tpj_sim_tests >/dev/null && build/linux-debug/tpj_sim_tests 2>&1 | tail -2`
Expected: no warnings, and every existing test passes.

### Task 4: Check in routes.park

Files:
- Create: `tests/parks/routes.park`

Step 1: Create the file with exactly this text, ending with a line feed:

```
tpj-park 1
seed 1
tick 0
next-key 9

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path]
2 kind=guest points=[{x=0 z=123} {x=0 z=100}]
3 kind=guest points=[{x=0 z=110} {x=-30 z=110}]
4 kind=guest points=[{x=-15 z=120} {x=-15 z=95}]
5 kind=guest points=[{x=0 z=100} {x=20 z=100} {x=30 z=80}]
6 kind=backstage points=[{x=12 z=122} {x=12 z=87}]

[box]
7 kind=shop x=6.5 z=115 facing-x=-1 facing-z=0
8 kind=depot x=12 z=80 facing-x=0 facing-z=1
```

Path 3 starts on path 2, path 4 crosses path 3, and path 5 starts at path 2's end. The backstage path crosses path 5 without joining it. The shop's front door lies 3.5 m from path 2 and its back door 2.5 m from the backstage path, and the depot's front door 3 m from the backstage path's end, for box-connections.

Run: `cmake --build --preset linux-debug --target tpj_scenarios >/dev/null && build/linux-debug/tpj_scenarios tests/parks/routes.park | grep -m1 "file tests/parks/routes.park"`
Expected: `file tests/parks/routes.park tick 0 hash ` followed by a hash.

### Task 5: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/routes/SPEC.md, src/sim/SPEC.md, src/sim/medium/SPEC.md, and src/sim/park/SPEC.md, and the public headers src/sim/routes/networks.h, src/sim/medium/network.h, src/sim/park/intent.h, and src/sim/park/geometry.h. Its tests of criteria 1 to 5 and 8 must build and fail on behavior. Those of criteria 6 and 7 may already pass, since the stub derives nothing.

### Task 6: Derive carriers from paths

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: Replace the file with:

```cpp
#include "sim/routes/networks.h"

#include "sim/park/geometry.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// One carrier for each path of the kind whose ground line is not empty, with no stops yet. parkPaths
// gives paths in ascending key order, so the carriers are in it too.
std::vector<Carrier> pathCarriers(const std::vector<ParkPath> &paths, PathKind kind) {
  std::vector<Carrier> carriers;
  for (const ParkPath &path : paths) {
    if (path.Kind != kind) {
      continue;
    }
    std::vector<CarrierPoint> line = groundLine(path.Points);
    if (line.empty()) {
      continue;
    }
    carriers.push_back(Carrier{.Key = path.Key, .Points = std::move(line), .Stops = {}});
  }
  return carriers;
}

Network deriveNetwork(const std::vector<ParkPath> &paths, PathKind kind) {
  std::vector<Carrier> carriers = pathCarriers(paths, kind);
  uint32_t nodeCount = 0;
  for (Carrier &carrier : carriers) {
    carrier.Stops = {{.Distance = 0.0, .Node = nodeCount},
                     {.Distance = carrier.Points.back().Distance, .Node = nodeCount + 1}};
    nodeCount += 2;
  }
  return Network(std::move(carriers), nodeCount, {});
}

void resolvePathNetworks(World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    Network network = deriveNetwork(paths, kind);
    const EntityKey key =
        world.createDerivedEntity(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
    world.Registry.emplace_or_replace<Network>(world.findEntity(key), std::move(network));
  }
}

} // namespace

const Network &parkNetwork(const World &world, PathKind kind) {
  static const Network EMPTY;
  const entt::entity entity = world.findEntity(networkKey(kind));
  if (entity == entt::null) {
    return EMPTY;
  }
  const Network *network = world.Registry.try_get<Network>(entity);
  return network != nullptr ? *network : EMPTY;
}

void addRoutes(WorldSchema &schema) { schema.addResolver("path-networks", &resolvePathNetworks); }

} // namespace tpj
```

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; for f in tests/sim/routes/*_test.cpp; do build/linux-debug/tpj_sim_tests -# "[#$(basename "$f" .cpp)]" 2>&1 | tail -1; done`
Expected: no warnings. The tests of criteria 1, 6, and 7 pass. Those of criteria 2, 3, and 8 that need a junction or crossing still fail.

### Task 7: Find meetings and join them into nodes

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: Replace the includes after `#include "sim/world.h"` with:

```cpp
#include <algorithm>
#include <numeric>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>
```

Step 2: In the anonymous namespace, after pathCarriers, add:

```cpp
// A meeting's distance on each of two carriers, named by their indices in the network's carriers.
struct Meeting {
  size_t First = 0;
  double FirstDistance = 0.0;
  size_t Second = 0;
  double SecondDistance = 0.0;
};

// The side of p from the segment from a to b: positive to its left, negative to its right.
double sideOf(const CarrierPoint &a, const CarrierPoint &b, const CarrierPoint &p) {
  return (b.X - a.X) * (p.Z - a.Z) - (b.Z - a.Z) * (p.X - a.X);
}

bool oppositeSigns(double first, double second) {
  return (first < 0.0 && second > 0.0) || (first > 0.0 && second < 0.0);
}

// The segment's distance at the fraction along it, clamped to its two distances.
double distanceAt(const CarrierPoint &a, const CarrierPoint &b, double fraction) {
  return std::clamp(a.Distance + fraction * (b.Distance - a.Distance), a.Distance, b.Distance);
}

// The distance of p's projection onto the segment from a to b, when p lies within
// JUNCTION_TOLERANCE of it.
std::optional<double> projectionWithin(const CarrierPoint &a, const CarrierPoint &b,
                                       const CarrierPoint &p) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  const double t =
      std::clamp(((p.X - a.X) * dx + (p.Z - a.Z) * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  CarrierPoint projected{.X = a.X + t * dx, .Z = a.Z + t * dz, .Distance = distanceAt(a, b, t)};
  if (t == 0.0) {
    projected = a;
  } else if (t == 1.0) {
    projected = b;
  }
  const double offX = p.X - projected.X;
  const double offZ = p.Z - projected.Z;
  if (offX * offX + offZ * offZ > JUNCTION_TOLERANCE * JUNCTION_TOLERANCE) {
    return std::nullopt;
  }
  return projected.Distance;
}

// Whether the segments' bounding boxes lie too far apart for them to meet. The margin is twice the
// tolerance, so rounding never rejects a pair the meeting test would accept.
bool farApart(const CarrierPoint &a, const CarrierPoint &b, const CarrierPoint &c,
              const CarrierPoint &d) {
  const double margin = 2.0 * JUNCTION_TOLERANCE;
  return std::max(a.X, b.X) + margin < std::min(c.X, d.X) ||
         std::max(c.X, d.X) + margin < std::min(a.X, b.X) ||
         std::max(a.Z, b.Z) + margin < std::min(c.Z, d.Z) ||
         std::max(c.Z, d.Z) + margin < std::min(a.Z, b.Z);
}

// Adds the meetings of segment i of the first carrier and segment j of the second.
void addMeetings(const std::vector<Carrier> &carriers, size_t first, size_t i, size_t second,
                 size_t j, std::vector<Meeting> &meetings) {
  const CarrierPoint &a = carriers[first].Points[i];
  const CarrierPoint &b = carriers[first].Points[i + 1];
  const CarrierPoint &c = carriers[second].Points[j];
  const CarrierPoint &d = carriers[second].Points[j + 1];
  if (farApart(a, b, c, d)) {
    return;
  }
  const double sideC = sideOf(a, b, c);
  const double sideD = sideOf(a, b, d);
  const double sideA = sideOf(c, d, a);
  const double sideB = sideOf(c, d, b);
  if (oppositeSigns(sideC, sideD) && oppositeSigns(sideA, sideB)) {
    meetings.push_back({.First = first,
                        .FirstDistance = distanceAt(a, b, sideA / (sideA - sideB)),
                        .Second = second,
                        .SecondDistance = distanceAt(c, d, sideC / (sideC - sideD))});
  }
  // Ends are tested even for crossing segments: nearly collinear segments that rounding leaves
  // crossing at a shallow angle still meet at their ends.
  if (const auto on = projectionWithin(c, d, a)) {
    meetings.push_back({first, a.Distance, second, *on});
  }
  if (const auto on = projectionWithin(c, d, b)) {
    meetings.push_back({first, b.Distance, second, *on});
  }
  if (const auto on = projectionWithin(a, b, c)) {
    meetings.push_back({first, *on, second, c.Distance});
  }
  if (const auto on = projectionWithin(a, b, d)) {
    meetings.push_back({first, *on, second, d.Distance});
  }
}

// The meetings of every pair of segments on different carriers, or on one carrier and not
// neighbors along it.
std::vector<Meeting> findMeetings(const std::vector<Carrier> &carriers) {
  std::vector<Meeting> meetings;
  for (size_t first = 0; first < carriers.size(); ++first) {
    const size_t firstSegments = carriers[first].Points.size() - 1;
    for (size_t i = 0; i < firstSegments; ++i) {
      for (size_t second = first; second < carriers.size(); ++second) {
        const size_t secondSegments = carriers[second].Points.size() - 1;
        for (size_t j = second == first ? i + 2 : 0; j < secondSegments; ++j) {
          addMeetings(carriers, first, i, second, j, meetings);
        }
      }
    }
  }
  return meetings;
}

// The first distances of a carrier's stop groups: sorted, each group taking every distance at most
// JUNCTION_TOLERANCE above its first.
std::vector<double> groupStarts(std::vector<double> distances) {
  std::ranges::sort(distances);
  std::vector<double> starts;
  for (const double distance : distances) {
    if (starts.empty() || distance - starts.back() > JUNCTION_TOLERANCE) {
      starts.push_back(distance);
    }
  }
  return starts;
}

// The index of the group holding the distance, which is at least the first start, 0.
size_t groupHolding(const std::vector<double> &starts, double distance) {
  return static_cast<size_t>(std::ranges::upper_bound(starts, distance) - starts.begin()) - 1;
}

// Stops joined into nodes, by union-find.
class StopJoins {
public:
  explicit StopJoins(size_t count) : Parents(count) {
    std::iota(Parents.begin(), Parents.end(), size_t{0});
  }

  size_t root(size_t stop) {
    while (Parents[stop] != stop) {
      Parents[stop] = Parents[Parents[stop]];
      stop = Parents[stop];
    }
    return stop;
  }

  void join(size_t first, size_t second) {
    const size_t firstRoot = root(first);
    const size_t secondRoot = root(second);
    Parents[std::max(firstRoot, secondRoot)] = std::min(firstRoot, secondRoot);
  }

private:
  std::vector<size_t> Parents;
};
```

Step 3: Replace deriveNetwork with:

```cpp
Network deriveNetwork(const std::vector<ParkPath> &paths, PathKind kind) {
  std::vector<Carrier> carriers = pathCarriers(paths, kind);
  const std::vector<Meeting> meetings = findMeetings(carriers);

  std::vector<std::vector<double>> distances(carriers.size());
  for (size_t c = 0; c < carriers.size(); ++c) {
    distances[c] = {0.0, carriers[c].Points.back().Distance};
  }
  for (const Meeting &meeting : meetings) {
    distances[meeting.First].push_back(meeting.FirstDistance);
    distances[meeting.Second].push_back(meeting.SecondDistance);
  }

  // Every carrier's stops, numbered in one sequence so a meeting can join stops of two carriers.
  std::vector<std::vector<double>> starts(carriers.size());
  std::vector<size_t> firstStops(carriers.size());
  size_t stopCount = 0;
  for (size_t c = 0; c < carriers.size(); ++c) {
    starts[c] = groupStarts(std::move(distances[c]));
    firstStops[c] = stopCount;
    stopCount += starts[c].size();
  }
  StopJoins joins(stopCount);
  for (const Meeting &meeting : meetings) {
    joins.join(firstStops[meeting.First] + groupHolding(starts[meeting.First], meeting.FirstDistance),
               firstStops[meeting.Second] +
                   groupHolding(starts[meeting.Second], meeting.SecondDistance));
  }

  // Nodes are numbered in order of first stop, carriers in key order and stops by distance.
  constexpr uint32_t UNNUMBERED = UINT32_MAX;
  std::vector<uint32_t> numbers(stopCount, UNNUMBERED);
  uint32_t nodeCount = 0;
  for (size_t c = 0; c < carriers.size(); ++c) {
    const double length = carriers[c].Points.back().Distance;
    for (size_t k = 0; k < starts[c].size(); ++k) {
      const size_t root = joins.root(firstStops[c] + k);
      if (numbers[root] == UNNUMBERED) {
        numbers[root] = nodeCount;
        ++nodeCount;
      }
      const bool last = k + 1 == starts[c].size();
      carriers[c].Stops.push_back({.Distance = last ? length : starts[c][k], .Node = numbers[root]});
    }
  }
  return Network(std::move(carriers), nodeCount, {});
}
```

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; for f in tests/sim/routes/*_test.cpp; do build/linux-debug/tpj_sim_tests -# "[#$(basename "$f" .cpp)]" 2>&1 | tail -1; done`
Expected: no warnings, and every test under tests/sim/routes passes.

### Task 8: Verify

Step 1: Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
Expected: no output.

Step 2: Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings, and `100% tests passed`.

Step 3: Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1; ctest.exe --preset windows-debug 2>&1 | tail -3`
Expected: the build finishes, and `100% tests passed`.

Step 4: Run: `scripts/cross-build-check.sh`
Expected: it passes, running the scenarios and three park files.

Step 5: Dispatch the reviewer agent on the staged diff via the reviewing skill, with FEATURE.md, src/sim/routes/SPEC.md, src/sim/SPEC.md, src/sim/medium/SPEC.md, src/sim/park/SPEC.md, and docs/principles.md.
Expected: Evan decides on each finding.

### Task 9: Commit

Step 1: Commit via the commit-hygiene skill, staging the paths this plan and the test pass touched by name, never parks/sketch.park or image.png, with the message:

```
Routes: Derive guest and backstage networks from paths
```

and a body saying that a new routes module derives one network per path kind in resolution, with a carrier per path along its ground line and nodes at path ends and wherever same-kind lines meet within 1 mm, that nearby meetings group into one stop and nodes are numbered by a fixed walk, and that tests/parks/routes.park joins the cross-build check, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

Run: `git log -1 --format=%s`
Expected: `Routes: Derive guest and backstage networks from paths`
