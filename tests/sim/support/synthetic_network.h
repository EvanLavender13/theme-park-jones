#ifndef TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_NETWORK_H
#define TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_NETWORK_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stddef.h>
#include <stdint.h>
#include <vector>

// Random synthetic networks, in which each edge is its own straight carrier, and the ground
// distances that checks of nearest places are bounded by.
namespace tpj::test {

inline double groundDistance(GroundPoint a, GroundPoint b) {
  return std::sqrt(((a.X - b.X) * (a.X - b.X)) + ((a.Z - b.Z) * (a.Z - b.Z)));
}

// The distance from the point to the nearest point of the carrier's ground line.
inline double distanceToCarrier(const Carrier &carrier, GroundPoint point) {
  double nearest = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i + 1 < carrier.Points.size(); ++i) {
    const CarrierPoint &a = carrier.Points[i];
    const CarrierPoint &b = carrier.Points[i + 1];
    const double dx = b.X - a.X;
    const double dz = b.Z - a.Z;
    const double squared = (dx * dx) + (dz * dz);
    const double t =
        squared == 0.0
            ? 0.0
            : std::clamp((((point.X - a.X) * dx) + ((point.Z - a.Z) * dz)) / squared, 0.0, 1.0);
    nearest = std::min(nearest, groundDistance(point, {.X = a.X + (t * dx), .Z = a.Z + (t * dz)}));
  }
  return nearest;
}

// A counter-based generator, so a network depends only on its seed.
class SyntheticRandom {
public:
  explicit SyntheticRandom(uint64_t seed) : Seed(seed) {}

  uint64_t next() {
    Hasher hasher;
    hasher.add(Seed);
    hasher.add(Count++);
    return hasher.value();
  }
  // In [0, 1).
  double uniform() { return static_cast<double>(next() >> 11U) * 0x1p-53; }
  double between(double low, double high) { return low + (uniform() * (high - low)); }
  uint32_t below(uint32_t bound) { return static_cast<uint32_t>(next() % bound); }

private:
  uint64_t Seed;
  uint64_t Count = 0;
};

// A network's inputs, carriers in the order they were generated, which is not key order.
struct SyntheticNetwork {
  std::vector<Carrier> Carriers;
  uint32_t NodeCount = 0;
  std::vector<NodeAnchor> Anchors;

  [[nodiscard]] Network build() const { return {Carriers, NodeCount, Anchors}; }
};

// Nodes at random ground positions, joined by a random spanning tree and then nodeCount / 2 more
// edges between random distinct nodes, so junctions and parallel carriers occur. Each edge is a
// straight carrier whose length is its ground length, running in a random direction. About a
// third of the nodes are anchored, to three entities, so an entity may anchor several nodes.
// nodeCount is at least 2.
inline SyntheticNetwork makeSyntheticNetwork(uint64_t seed, uint32_t nodeCount) {
  SyntheticRandom random(seed);
  std::vector<GroundPoint> positions;
  for (uint32_t node = 0; node < nodeCount; ++node) {
    const double x = random.between(-50.0, 50.0);
    const double z = random.between(-50.0, 50.0);
    positions.push_back({.X = x, .Z = z});
  }

  SyntheticNetwork network;
  network.NodeCount = nodeCount;
  std::set<uint64_t> usedKeys;
  auto addCarrier = [&](uint32_t first, uint32_t second) {
    const bool reversed = (random.next() & 1U) != 0;
    const uint32_t from = reversed ? second : first;
    const uint32_t to = reversed ? first : second;
    uint64_t key = 0;
    while (key == 0 || usedKeys.contains(key)) {
      key = random.next() >> 1U;
    }
    usedKeys.insert(key);
    const GroundPoint a = positions[from];
    const GroundPoint b = positions[to];
    const double length = std::sqrt(((b.X - a.X) * (b.X - a.X)) + ((b.Z - a.Z) * (b.Z - a.Z)));
    network.Carriers.push_back(Carrier{
        .Key = EntityKey{key},
        .Points = {{.X = a.X, .Z = a.Z, .Distance = 0.0}, {.X = b.X, .Z = b.Z, .Distance = length}},
        .Stops = {{.Distance = 0.0, .Node = from}, {.Distance = length, .Node = to}},
    });
  };

  for (uint32_t node = 1; node < nodeCount; ++node) {
    addCarrier(node, random.below(node));
  }
  for (uint32_t extra = 0; extra < nodeCount / 2; ++extra) {
    const uint32_t first = random.below(nodeCount);
    const uint32_t second = (first + 1 + random.below(nodeCount - 1)) % nodeCount;
    addCarrier(first, second);
  }

  for (uint32_t node = 0; node < nodeCount; ++node) {
    if (random.below(3) == 0) {
      network.Anchors.push_back({.Node = node, .Entity = EntityKey{1 + random.below(3)}});
    }
  }
  return network;
}

} // namespace tpj::test

#endif
