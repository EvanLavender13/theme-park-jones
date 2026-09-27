# Implementation Plan: Networks and Places

## Goal

Add the medium's Network type in src/sim/medium, with carriers as polylines, stops that tile each carrier into edges, and anchors. Add place resolution, ground points, and the nearest-place query, register the network as derived data, and have makeParkSchema register it.

## Approach

Network keeps its inputs (carriers, node count, anchors) as private members listed by visitFields, and its constructor sorts and validates them. It then builds two caches: the edge list, with each carrier's first edge index, and each node's place. The walk lists only the inputs. The caches are functions of them, so equal inputs give equal networks, and a copy copies the caches with the rest. Every lookup is a binary search over sorted vectors, and nearestPlace scans every segment in key order, keeping a candidate only when it is strictly nearer, which gives the tie rule. Formulas are fixed as the spec states, so every build computes the same bits (RESEARCH.md).

## Tasks

### Task 1: Write the medium spec

Files:
- Create: `src/sim/medium/SPEC.md`

Step 1: Create the file with the text in FEATURE.md's "Spec changes" section for src/sim/medium/SPEC.md, exactly, without the surrounding quotation marks.

### Task 2: Update the sim spec and the milestone

Files:
- Modify: `src/sim/SPEC.md` (the paragraph beginning "Component types are registered with a WorldSchema")
- Modify: `plans/shared-medium/first-field-and-flow/MILESTONE.md` (Open questions)

Step 1: In src/sim/SPEC.md, replace "It registers nothing yet." with "It registers the medium's types first (sim/medium/SPEC.md), and so far nothing else."

Step 2: In MILESTONE.md, delete the open question beginning "How a carrier's ground geometry is held". It is resolved: a polyline with the producer's arc lengths, as networks-and-places' RESEARCH.md records.

### Task 3: Declare the network type

Files:
- Create: `src/sim/medium/network.h`

Step 1: Create the header.

```cpp
#ifndef TPJ_SIM_MEDIUM_NETWORK_H
#define TPJ_SIM_MEDIUM_NETWORK_H

#include "sim/entity_key.h"

#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <variant>
#include <vector>

namespace tpj {

class WorldSchema;

// A point of a carrier's ground line: its position on the ground, and its arc length from the
// carrier's start as the carrier's producer measured it.
struct CarrierPoint {
  double X = 0.0;
  double Z = 0.0;
  double Distance = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, CarrierPoint &point) {
  visitor.field("x", point.X);
  visitor.field("z", point.Z);
  visitor.field("distance", point.Distance);
}

// A distance along a carrier where it meets a node.
struct CarrierStop {
  double Distance = 0.0;
  uint32_t Node = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, CarrierStop &stop) {
  visitor.field("distance", stop.Distance);
  visitor.field("node", stop.Node);
}

// A line with a stable key that edges are stretches of, such as a drawn path or a box's connector.
// Its stops cut it into edges, the first at 0 and the last at its length.
struct Carrier {
  EntityKey Key = NULL_KEY;
  std::vector<CarrierPoint> Points;
  std::vector<CarrierStop> Stops;
};

template <typename Visitor> void visitFields(Visitor &visitor, Carrier &carrier) {
  visitor.field("key", carrier.Key);
  visitor.field("points", carrier.Points);
  visitor.field("stops", carrier.Stops);
}

// A node tied to an entity, so the entity finds where it meets the network.
struct NodeAnchor {
  uint32_t Node = 0;
  EntityKey Entity = NULL_KEY;
};

template <typename Visitor> void visitFields(Visitor &visitor, NodeAnchor &anchor) {
  visitor.field("node", anchor.Node);
  visitor.field("entity", anchor.Entity);
}

// The stretch of a carrier between two consecutive stops.
struct NetworkEdge {
  EntityKey Carrier = NULL_KEY;
  uint32_t From = 0;
  uint32_t To = 0;
  double FromDistance = 0.0;
  double ToDistance = 0.0;

  [[nodiscard]] double length() const { return ToDistance - FromDistance; }
};

// A carrier and a distance along it. It names the same ground position however the carrier is cut
// into edges, so holders keep places, never edges.
struct Place {
  EntityKey Carrier = NULL_KEY;
  double Distance = 0.0;

  bool operator==(const Place &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, Place &place) {
  visitor.field("carrier", place.Carrier);
  visitor.field("distance", place.Distance);
}

struct GroundPoint {
  double X = 0.0;
  double Z = 0.0;
};

// A place at a node.
struct NodePosition {
  uint32_t Node = 0;
};

// A place strictly inside an edge, with its distances along the carrier to the edge's two ends.
struct EdgePosition {
  uint32_t Edge = 0;
  double FromOffset = 0.0;
  double ToOffset = 0.0;
};

using NetworkPosition = std::variant<NodePosition, EdgePosition>;

// Carriers, nodes, and anchors, as a value. Derived data: producers build networks in resolution.
class Network {
public:
  Network() = default;
  // Sorts carriers by key and anchors by node. Throws std::invalid_argument for the malformed
  // inputs sim/medium/SPEC.md lists.
  Network(std::vector<Carrier> carriers, uint32_t nodeCount, std::vector<NodeAnchor> anchors);

  // In ascending key order.
  [[nodiscard]] const std::vector<Carrier> &carriers() const { return Carriers; }
  [[nodiscard]] uint32_t nodeCount() const { return NodeCount; }
  // Carriers in key order, and each carrier's edges in order of distance.
  [[nodiscard]] const std::vector<NetworkEdge> &edges() const { return Edges; }

  // The node at exactly the place's distance, or the edge enclosing it. None when the carrier is
  // not in the network, or the distance is NaN, below 0, or above the carrier's length.
  [[nodiscard]] std::optional<NetworkPosition> resolve(const Place &place) const;
  // The node's stop on its lowest-keyed carrier, at the lowest distance there. Throws
  // std::out_of_range for a node not below nodeCount().
  [[nodiscard]] Place nodePlace(uint32_t node) const;
  // The node's anchored entity, or NULL_KEY. Throws std::out_of_range as nodePlace does.
  [[nodiscard]] EntityKey nodeAnchor(uint32_t node) const;
  // The nodes anchored to the entity, ascending.
  [[nodiscard]] std::vector<uint32_t> anchoredNodes(EntityKey entity) const;
  // The place's ground position, interpolated by distance between carrier points. None where
  // resolve gives none.
  [[nodiscard]] std::optional<GroundPoint> groundPoint(const Place &place) const;
  // The place whose ground position is nearest the point, ties to the lower carrier key and then
  // the lower distance. None for an empty network or a point that is not finite.
  [[nodiscard]] std::optional<Place> nearestPlace(GroundPoint point) const;

  // Lists the inputs. The edges and node places are rebuilt from them only by the constructor, so
  // Network is never loaded: it is derived, and saves never hold it.
  template <typename Visitor> friend void visitFields(Visitor &visitor, Network &network) {
    visitor.field("carriers", network.Carriers);
    visitor.field("nodes", network.NodeCount);
    visitor.field("anchors", network.Anchors);
  }

private:
  // The index in Carriers of the carrier holding the place, when the place lies on it.
  [[nodiscard]] std::optional<size_t> findCarrierOf(const Place &place) const;

  std::vector<Carrier> Carriers;
  uint32_t NodeCount = 0;
  std::vector<NodeAnchor> Anchors;

  // Built by the constructor from the members above.
  std::vector<NetworkEdge> Edges;
  // Parallel to Carriers: the index in Edges of each carrier's first edge.
  std::vector<uint32_t> FirstEdges;
  std::vector<Place> NodePlaces;
};

// Registers Network as the derived component type network.
void addNetworkComponent(WorldSchema &schema);

} // namespace tpj

#endif
```

### Task 4: Stub the network and register it

Files:
- Create: `src/sim/medium/network.cpp`
- Modify: `src/sim/CMakeLists.txt:3-11`
- Modify: `src/sim/park_schema.cpp`

Step 1: Create the stub source. addNetworkComponent is its final form.

```cpp
#include "sim/medium/network.h"

#include "sim/schema.h"

#include <utility>

namespace tpj {

Network::Network(std::vector<Carrier> carriers, uint32_t nodeCount, std::vector<NodeAnchor> anchors)
    : Carriers(std::move(carriers)), NodeCount(nodeCount), Anchors(std::move(anchors)) {}

std::optional<size_t> Network::findCarrierOf(const Place &) const { return std::nullopt; }

std::optional<NetworkPosition> Network::resolve(const Place &) const { return std::nullopt; }

Place Network::nodePlace(uint32_t) const { return {}; }

EntityKey Network::nodeAnchor(uint32_t) const { return NULL_KEY; }

std::vector<uint32_t> Network::anchoredNodes(EntityKey) const { return {}; }

std::optional<GroundPoint> Network::groundPoint(const Place &) const { return std::nullopt; }

std::optional<Place> Network::nearestPlace(GroundPoint) const { return std::nullopt; }

void addNetworkComponent(WorldSchema &schema) {
  schema.addComponent<Network>("network", DataKind::Derived);
}

} // namespace tpj
```

Step 2: Add `medium/network.cpp` to the add_library list in src/sim/CMakeLists.txt, after `draw.cpp`.

Step 3: In src/sim/park_schema.cpp, include `"sim/medium/network.h"`, and replace the comment in makeParkSchema with:

```cpp
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them as they land, in dependency order.
  addNetworkComponent(*schema);
```

Step 4: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and all 116 existing tests pass. If clang-tidy objects to a stub's unused parameter, leave the parameter unnamed, as above.

### Task 5: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/sim/medium/SPEC.md, src/sim/SPEC.md, and the public headers src/sim/medium/network.h and src/sim/park_schema.h. It creates tests/sim/medium/network_test.cpp and a random synthetic network builder in tests/sim/support/, and it adds the test file to tpj_sim_tests in tests/sim/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 116 existing tests pass. The new tests of validation, edges, resolution, node places, anchors, ground points, and nearest places fail against the stubs. Tests that hold vacuously may pass: registration, save omission, the out-of-range throws if written as "does not return", and "no position" cases.

### Task 6: Validate and build the network

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: Add `<algorithm>`, `<cmath>`, `<stdexcept>`, and `<string>` to the includes. In an anonymous namespace, add the checks.

```cpp
std::string carrierName(EntityKey key) {
  return "carrier " + std::to_string(static_cast<uint64_t>(key));
}

bool isFinite(double value) { return std::isfinite(value); }

void requireValidPoints(const Carrier &carrier) {
  const std::string name = carrierName(carrier.Key);
  if (carrier.Points.size() < 2) {
    throw std::invalid_argument(name + " has fewer than two points");
  }
  for (const CarrierPoint &point : carrier.Points) {
    if (!isFinite(point.X) || !isFinite(point.Z) || !isFinite(point.Distance)) {
      throw std::invalid_argument(name + " has a coordinate or distance that is not finite");
    }
  }
  if (carrier.Points.front().Distance != 0.0) {
    throw std::invalid_argument(name + " does not start at distance 0");
  }
  for (size_t i = 1; i < carrier.Points.size(); ++i) {
    if (carrier.Points[i].Distance <= carrier.Points[i - 1].Distance) {
      throw std::invalid_argument(name + " has point distances that do not strictly increase");
    }
  }
}

void requireValidStops(const Carrier &carrier, uint32_t nodeCount) {
  const std::string name = carrierName(carrier.Key);
  if (carrier.Stops.size() < 2) {
    throw std::invalid_argument(name + " has fewer than two stops");
  }
  for (const CarrierStop &stop : carrier.Stops) {
    if (!isFinite(stop.Distance)) {
      throw std::invalid_argument(name + " has a stop distance that is not finite");
    }
    if (stop.Node >= nodeCount) {
      throw std::invalid_argument(name + " stops at node " + std::to_string(stop.Node) +
                                  ", which is not below the node count");
    }
  }
  if (carrier.Stops.front().Distance != 0.0) {
    throw std::invalid_argument(name + " has its first stop away from 0");
  }
  if (carrier.Stops.back().Distance != carrier.Points.back().Distance) {
    throw std::invalid_argument(name + " has its last stop away from its length");
  }
  for (size_t i = 1; i < carrier.Stops.size(); ++i) {
    if (carrier.Stops[i].Distance <= carrier.Stops[i - 1].Distance) {
      throw std::invalid_argument(name + " has stop distances that do not strictly increase");
    }
  }
}

void requireValidAnchors(const std::vector<NodeAnchor> &anchors, uint32_t nodeCount) {
  for (size_t i = 0; i < anchors.size(); ++i) {
    const NodeAnchor &anchor = anchors[i];
    const std::string name = "node " + std::to_string(anchor.Node);
    if (anchor.Node >= nodeCount) {
      throw std::invalid_argument("an anchor names " + name + ", which is not below the node count");
    }
    if (anchor.Entity == NULL_KEY) {
      throw std::invalid_argument("an anchor on " + name + " names the null key");
    }
    if (i > 0 && anchors[i - 1].Node == anchor.Node) {
      throw std::invalid_argument(name + " is anchored twice");
    }
  }
}
```

Step 2: Replace the constructor's empty body. Sort, validate, then build the caches in one pass. Since carriers run in ascending key and stops in ascending distance, the first stop seen at a node is its place.

```cpp
Network::Network(std::vector<Carrier> carriers, uint32_t nodeCount, std::vector<NodeAnchor> anchors)
    : Carriers(std::move(carriers)), NodeCount(nodeCount), Anchors(std::move(anchors)) {
  std::ranges::sort(Carriers, {}, &Carrier::Key);
  std::ranges::sort(Anchors, {}, &NodeAnchor::Node);
  for (size_t i = 0; i < Carriers.size(); ++i) {
    if (Carriers[i].Key == NULL_KEY) {
      throw std::invalid_argument("a carrier has the null key");
    }
    if (i > 0 && Carriers[i - 1].Key == Carriers[i].Key) {
      throw std::invalid_argument(carrierName(Carriers[i].Key) + " is repeated");
    }
    requireValidPoints(Carriers[i]);
    requireValidStops(Carriers[i], NodeCount);
  }
  requireValidAnchors(Anchors, NodeCount);

  std::vector<std::optional<Place>> places(NodeCount);
  for (const Carrier &carrier : Carriers) {
    FirstEdges.push_back(static_cast<uint32_t>(Edges.size()));
    for (size_t i = 0; i < carrier.Stops.size(); ++i) {
      const CarrierStop &stop = carrier.Stops[i];
      if (!places[stop.Node]) {
        places[stop.Node] = Place{carrier.Key, stop.Distance};
      }
      if (i + 1 < carrier.Stops.size()) {
        const CarrierStop &next = carrier.Stops[i + 1];
        Edges.push_back({carrier.Key, stop.Node, next.Node, stop.Distance, next.Distance});
      }
    }
  }
  NodePlaces.reserve(NodeCount);
  for (uint32_t node = 0; node < NodeCount; ++node) {
    const std::optional<Place> &place = places[node];
    if (!place) {
      throw std::invalid_argument("node " + std::to_string(node) + " has no carrier stopping at it");
    }
    NodePlaces.push_back(*place);
  }
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the validation, order-independence, and edge tests pass.

### Task 7: Resolve places and find nodes

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: Replace the stubs of findCarrierOf, resolve, nodePlace, nodeAnchor, and anchoredNodes.

```cpp
std::optional<size_t> Network::findCarrierOf(const Place &place) const {
  const auto found = std::ranges::lower_bound(Carriers, place.Carrier, {}, &Carrier::Key);
  if (found == Carriers.end() || found->Key != place.Carrier) {
    return std::nullopt;
  }
  // Written so that a NaN distance fails too.
  if (!(place.Distance >= 0.0 && place.Distance <= found->Points.back().Distance)) {
    return std::nullopt;
  }
  return static_cast<size_t>(found - Carriers.begin());
}

std::optional<NetworkPosition> Network::resolve(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  const Carrier &carrier = Carriers[*index];
  // The first stop at or beyond the distance. It exists because the last stop is at the length.
  const auto stop = std::ranges::lower_bound(carrier.Stops, place.Distance, {}, &CarrierStop::Distance);
  if (stop->Distance == place.Distance) {
    return NodePosition{stop->Node};
  }
  // The distance is above the first stop, at 0, so the stop before this one exists.
  const auto edge = FirstEdges[*index] + static_cast<uint32_t>(stop - carrier.Stops.begin()) - 1;
  return EdgePosition{edge, place.Distance - Edges[edge].FromDistance,
                      Edges[edge].ToDistance - place.Distance};
}

Place Network::nodePlace(uint32_t node) const { return NodePlaces.at(node); }

EntityKey Network::nodeAnchor(uint32_t node) const {
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  const auto found = std::ranges::lower_bound(Anchors, node, {}, &NodeAnchor::Node);
  return found != Anchors.end() && found->Node == node ? found->Entity : NULL_KEY;
}

std::vector<uint32_t> Network::anchoredNodes(EntityKey entity) const {
  std::vector<uint32_t> nodes;
  for (const NodeAnchor &anchor : Anchors) {
    if (anchor.Entity == entity) {
      nodes.push_back(anchor.Node);
    }
  }
  return nodes;
}
```

nodePlace throws std::out_of_range through vector::at.

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the resolution, node place, and anchor tests pass as well.

### Task 8: Ground points

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: Replace groundPoint's stub.

```cpp
std::optional<GroundPoint> Network::groundPoint(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  const std::vector<CarrierPoint> &points = Carriers[*index].Points;
  // The first point at or beyond the distance. It exists because the last point is at the length.
  const auto after = std::ranges::lower_bound(points, place.Distance, {}, &CarrierPoint::Distance);
  if (after->Distance == place.Distance) {
    return GroundPoint{after->X, after->Z};
  }
  const CarrierPoint &a = *(after - 1);
  const CarrierPoint &b = *after;
  const double t = (place.Distance - a.Distance) / (b.Distance - a.Distance);
  return GroundPoint{a.X + t * (b.X - a.X), a.Z + t * (b.Z - a.Z)};
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the ground point tests pass as well.

### Task 9: Nearest places

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: Replace nearestPlace's stub. A candidate replaces the best only when strictly nearer, so the scan's order, carriers by key and segments by distance, gives the tie rule.

```cpp
std::optional<Place> Network::nearestPlace(GroundPoint point) const {
  if (!isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  std::optional<Place> best;
  double bestSquared = 0.0;
  for (const Carrier &carrier : Carriers) {
    for (size_t i = 0; i + 1 < carrier.Points.size(); ++i) {
      const CarrierPoint &a = carrier.Points[i];
      const CarrierPoint &b = carrier.Points[i + 1];
      const double dx = b.X - a.X;
      const double dz = b.Z - a.Z;
      const double lengthSquared = dx * dx + dz * dz;
      double t = 0.0;
      if (lengthSquared > 0.0) {
        t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / lengthSquared, 0.0, 1.0);
      }
      // A segment's ends are exact, so a junction is equally near on every carrier stopping there
      // and the tie rule decides between them.
      CarrierPoint projected{a.X + t * dx, a.Z + t * dz, a.Distance + t * (b.Distance - a.Distance)};
      if (t == 0.0) {
        projected = a;
      } else if (t == 1.0) {
        projected = b;
      }
      const double offX = point.X - projected.X;
      const double offZ = point.Z - projected.Z;
      const double squared = offX * offX + offZ * offZ;
      if (!best || squared < bestSquared) {
        best = Place{carrier.Key, projected.Distance};
        bestSquared = squared;
      }
    }
  }
  return best;
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and every test passes.

### Task 10: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `scripts/cross-build-check.sh`
Expected: the script prints its stages and passes. The medium adds no scenario yet, and the check confirms that the scenarios' output is unchanged.

### Task 11: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Medium: Add the network type with carriers and places`.
