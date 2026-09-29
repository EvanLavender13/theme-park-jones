# Implementation Plan: Route Distance

## Goal

The routes module publishes guest-route-distance and backstage-route-distance, entry fields giving every place its distance along its network to each anchored entity and the next step toward it.

## Approach

A new file pair, src/sim/routes/route_distance.h and .cpp, defines RouteStep, RouteEntry, the RouteDistance field template, and routeEntries. routeEntries runs Dijkstra from a source's anchored nodes over steps built from edges(), then picks each node's next step from every step that achieves its distance by the fixed tie rule. The resolver route-distance publishes each anchored entity's entries at its nodes' places, and addRoutes registers the fields and the resolver after path-networks, so makeParkSchema does not change.

## Tasks

### Task 1: Routes spec

Files:
- Modify: `src/sim/routes/SPEC.md:3`
- Modify: `src/sim/routes/SPEC.md:7`
- Modify: `src/sim/routes/SPEC.md:31` (append after)

Step 1: Make the three edits FEATURE.md's Spec changes gives for src/sim/routes/SPEC.md: the first paragraph's publishing sentence, the addRoutes sentence in Networks, and the new Route distance section, its heading and all five paragraphs verbatim, appended after Stops and nodes.

Step 2: Check

Run: `grep -c "route-distance" src/sim/routes/SPEC.md`
Expected: `2`, the fields paragraph and the sampleEdge paragraph.

### Task 2: Sim spec

Files:
- Modify: `src/sim/SPEC.md:13`

Step 1: Replace "then the routes resolver that derives the park's networks (sim/routes/SPEC.md)" with "then the routes module's resolvers and fields, which derive the park's networks and route distance (sim/routes/SPEC.md)".

### Task 3: Declarations and stubs

Files:
- Create: `src/sim/routes/route_distance.h`
- Create: `src/sim/routes/route_distance.cpp`
- Modify: `src/sim/routes/networks.h:43-44`
- Modify: `src/sim/routes/networks.cpp:1-14,429`
- Modify: `src/sim/CMakeLists.txt:17`

Step 1: Create src/sim/routes/route_distance.h:

```cpp
#ifndef TPJ_SIM_ROUTES_ROUTE_DISTANCE_H
#define TPJ_SIM_ROUTES_ROUTE_DISTANCE_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"

#include <optional>
#include <string_view>
#include <vector>

namespace tpj {

class WorldSchema;

// An edge walked one way: along the carrier from its stop at From to its stop at To. The carrier
// is NULL_KEY at the source itself, where the route has arrived.
struct RouteStep {
  EntityKey Carrier = NULL_KEY;
  double From = 0.0;
  double To = 0.0;

  bool operator==(const RouteStep &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, RouteStep &step) {
  visitor.field("carrier", step.Carrier);
  visitor.field("from", step.From);
  visitor.field("to", step.To);
}

// A source's distance along the network from a place, and the first step of a shortest route.
struct RouteEntry {
  double Distance = 0.0;
  RouteStep Next;

  bool operator==(const RouteEntry &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, RouteEntry &entry) {
  visitor.field("distance", entry.Distance);
  visitor.field("next", entry.Next);
}

// At a place inside an edge, one entry for the source: the better of the edge's two ends, the
// place's offset to each added to that end's distance, ties to the From end. Entries inside the
// edge are ignored.
std::vector<RouteEntry> sampleRouteEdge(const EdgeSample<RouteEntry> &sample);

// The route distance field on the kind's network.
template <PathKind NetworkKind> struct RouteDistance {
  using Entry = RouteEntry;
  static constexpr std::string_view Name =
      NetworkKind == PathKind::Guest ? "guest-route-distance" : "backstage-route-distance";
  static constexpr FieldKind Kind = FieldKind::Entry;

  static std::vector<RouteEntry> sampleEdge(const EdgeSample<RouteEntry> &sample) {
    return sampleRouteEdge(sample);
  }
};

// The source's entry at each node of the network, indexed by node, none where no route joins the
// node to one the source anchors. Distances are least route lengths added from the source's end,
// and each next step is the lowest by carrier key, then toward lower distances, then From, of
// those that achieve its node's distance.
std::vector<std::optional<RouteEntry>> routeEntries(const Network &network, EntityKey source);

// Registers the guest and then the backstage route distance field, and then the resolver
// route-distance, which publishes every anchored entity's entries at its nodes. addRoutes calls
// it after registering path-networks.
void addRouteDistance(WorldSchema &schema);

} // namespace tpj

#endif
```

Step 2: Create src/sim/routes/route_distance.cpp with stubs:

```cpp
#include "sim/routes/route_distance.h"

#include "sim/schema.h"
#include "sim/world.h"

#include <optional>
#include <vector>

namespace tpj {

namespace {

void resolveRouteDistance(World & /*world*/) {}

} // namespace

std::vector<RouteEntry> sampleRouteEdge(const EdgeSample<RouteEntry> & /*sample*/) { return {}; }

std::vector<std::optional<RouteEntry>> routeEntries(const Network &network,
                                                    EntityKey /*source*/) {
  return std::vector<std::optional<RouteEntry>>(network.nodeCount());
}

void addRouteDistance(WorldSchema &schema) {
  addField<RouteDistance<PathKind::Guest>>(schema);
  addField<RouteDistance<PathKind::Backstage>>(schema);
  schema.addResolver("route-distance", &resolveRouteDistance,
                     {"path-networks", "guest-route-distance-field",
                      "backstage-route-distance-field"});
}

} // namespace tpj
```

Step 3: In src/sim/routes/networks.h, change addRoutes's comment to:

```cpp
// Registers the resolver path-networks, and then route distance's fields and resolver.
```

Step 4: In src/sim/routes/networks.cpp, add `#include "sim/routes/route_distance.h"` after `#include "sim/park/geometry.h"`, and replace line 429 with:

```cpp
void addRoutes(WorldSchema &schema) {
  schema.addResolver("path-networks", &resolvePathNetworks);
  addRouteDistance(schema);
}
```

Step 5: In src/sim/CMakeLists.txt, add `    routes/route_distance.cpp` after `    routes/networks.cpp`.

Step 6: Build

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

Step 7: Existing tests still pass with the fields registered.

Run: `ctest --preset linux-debug 2>&1 | tail -3`
Expected: `100% tests passed`.

### Task 4: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, src/sim/routes/SPEC.md, src/sim/medium/SPEC.md, and src/sim/SPEC.md, and the public headers src/sim/routes/route_distance.h, src/sim/routes/networks.h, src/sim/medium/field.h, and src/sim/medium/network.h.

### Task 5: sampleEdge

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Replace the sampleRouteEdge stub with:

```cpp
std::vector<RouteEntry> sampleRouteEdge(const EdgeSample<RouteEntry> &sample) {
  const NetworkEdge &edge = sample.Edge;
  std::optional<RouteEntry> best;
  // Strictly less keeps the earlier candidate, and the From end's come first.
  const auto consider = [&best](const RouteEntry &candidate) {
    if (!best || candidate.Distance < best->Distance) {
      best = candidate;
    }
  };
  for (const RouteEntry &entry : sample.AtFrom) {
    consider({entry.Distance + sample.FromOffset, {edge.Carrier, edge.ToDistance, edge.FromDistance}});
  }
  for (const RouteEntry &entry : sample.AtTo) {
    consider({entry.Distance + sample.ToOffset, {edge.Carrier, edge.FromDistance, edge.ToDistance}});
  }
  if (!best) {
    return {};
  }
  return {*best};
}
```

Step 2: Build and run.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#route_distance_test]"`
Expected: no warnings; the sampleEdge tests pass, and the routeEntries, field, and walk tests still fail.

### Task 6: routeEntries

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Add `#include <functional>`, `#include <queue>`, `#include <stdint.h>`, and `#include <utility>` to the system includes, in sorted order. In the anonymous namespace, before resolveRouteDistance, add:

```cpp
// A step leaving a node, with the node it arrives at and the length of its edge.
struct Leaving {
  RouteStep Step;
  uint32_t Arrives = 0;
  double Length = 0.0;
};

// The steps leaving each node, indexed by node: each edge gives one from each of its ends.
std::vector<std::vector<Leaving>> leavingSteps(const Network &network) {
  std::vector<std::vector<Leaving>> leaving(network.nodeCount());
  for (const NetworkEdge &edge : network.edges()) {
    leaving[edge.From].push_back(
        {{edge.Carrier, edge.FromDistance, edge.ToDistance}, edge.To, edge.length()});
    leaving[edge.To].push_back(
        {{edge.Carrier, edge.ToDistance, edge.FromDistance}, edge.From, edge.length()});
  }
  return leaving;
}

// Whether a step comes before another among those achieving a node's distance: the lower carrier
// key, then toward lower distances, then the lower From.
bool isPreferred(const RouteStep &step, const RouteStep &other) {
  if (step.Carrier != other.Carrier) {
    return step.Carrier < other.Carrier;
  }
  const bool isDown = step.To < step.From;
  const bool isOtherDown = other.To < other.From;
  if (isDown != isOtherDown) {
    return isDown;
  }
  return step.From < other.From;
}
```

Step 2: Replace the routeEntries stub with:

```cpp
std::vector<std::optional<RouteEntry>> routeEntries(const Network &network, EntityKey source) {
  const uint32_t count = network.nodeCount();
  std::vector<std::optional<RouteEntry>> entries(count);
  const std::vector<std::vector<Leaving>> leaving = leavingSteps(network);

  // Dijkstra from every node the source anchors. Rounded addition is monotone, so each distance is
  // the least route length added from the source's end, whatever order ties are settled in.
  using Queued = std::pair<double, uint32_t>;
  std::priority_queue<Queued, std::vector<Queued>, std::greater<>> queue;
  for (const uint32_t node : network.anchoredNodes(source)) {
    entries[node] = RouteEntry{};
    queue.emplace(0.0, node);
  }
  std::vector<bool> settled(count, false);
  while (!queue.empty()) {
    const auto [distance, node] = queue.top();
    queue.pop();
    if (settled[node]) {
      continue;
    }
    settled[node] = true;
    for (const Leaving &step : leaving[node]) {
      const double reached = distance + step.Length;
      std::optional<RouteEntry> &entry = entries[step.Arrives];
      if (!entry || reached < entry->Distance) {
        entry = RouteEntry{reached, {}};
        queue.emplace(reached, step.Arrives);
      }
    }
  }

  // Each next step is chosen from every step achieving its node's distance, so it depends on the
  // network alone.
  for (uint32_t node = 0; node < count; ++node) {
    std::optional<RouteEntry> &entry = entries[node];
    if (!entry || network.nodeAnchor(node) == source) {
      continue;
    }
    std::optional<RouteStep> next;
    for (const Leaving &step : leaving[node]) {
      const std::optional<RouteEntry> &far = entries[step.Arrives];
      if (far && far->Distance + step.Length == entry->Distance &&
          (!next || isPreferred(step.Step, *next))) {
        next = step.Step;
      }
    }
    if (next) {
      entry->Next = *next;
    }
  }
  return entries;
}
```

Step 3: Build and run.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#route_distance_test]"`
Expected: no warnings; the routeEntries and sampleEdge tests pass, including the reference comparison, the tie rule, and walks on synthetic networks. Tests of the fields in a resolved world still fail.

### Task 7: The resolver

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Add `#include "sim/routes/networks.h"` to the project includes and `#include <algorithm>` to the system includes, in sorted order. Replace the resolveRouteDistance stub with:

```cpp
// Publishes each entity anchoring a node of the kind's network, in ascending key order, with its
// entries at its reachable nodes' places, in node order.
template <PathKind Kind> void publishRouteDistance(World &world) {
  const Network &network = parkNetwork(world, Kind);
  std::vector<EntityKey> sources;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const EntityKey anchor = network.nodeAnchor(node);
    if (anchor != NULL_KEY) {
      sources.push_back(anchor);
    }
  }
  std::ranges::sort(sources);
  const auto repeated = std::ranges::unique(sources);
  sources.erase(repeated.begin(), repeated.end());
  for (const EntityKey source : sources) {
    const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, source);
    std::vector<PlacedEntry<RouteEntry>> placed;
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      if (const std::optional<RouteEntry> &entry = entries[node]) {
        placed.push_back({network.nodePlace(node), *entry});
      }
    }
    publishResolved<RouteDistance<Kind>>(world, source, std::move(placed));
  }
}

void resolveRouteDistance(World &world) {
  publishRouteDistance<PathKind::Guest>(world);
  publishRouteDistance<PathKind::Backstage>(world);
}
```

Step 2: Build and run.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#route_distance_test]"`
Expected: no warnings; every route distance test passes.

### Task 8: Confirm

Step 1: Full Linux build and tests.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings, `100% tests passed`.

Step 2: Windows build, tests, and the cross-build check.

Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1 && ctest.exe --preset windows-debug 2>&1 | tail -3 && scripts/cross-build-check.sh 2>&1 | tail -2`
Expected: the build succeeds, `100% tests passed`, and the cross-build check reports that the outputs match.

### Task 9: Review and commit

Step 1: Stage and review as implementing-features describes, with FEATURE.md, src/sim/routes/SPEC.md, src/sim/SPEC.md, src/sim/medium/SPEC.md, and docs/principles.md as context. Present the findings verbatim.

Step 2: Commit once via commit-hygiene, staging the specific paths: src/sim/routes/route_distance.h, src/sim/routes/route_distance.cpp, src/sim/routes/networks.h, src/sim/routes/networks.cpp, src/sim/CMakeLists.txt, src/sim/routes/SPEC.md, src/sim/SPEC.md, and the test pass's files under tests/sim/. Subject: `Routes: Publish route distance fields`.
