#ifndef TPJ_TESTS_SIM_SUPPORT_SAME_NETWORKS_H
#define TPJ_TESTS_SIM_SUPPORT_SAME_NETWORKS_H

#include "support/park_worlds.h"

#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <algorithm>
#include <cstddef>
#include <stdint.h>

namespace tpj::test {

// Networks are equal when their carriers, stops, and anchors are, doubles bit for bit.
inline bool sameNetwork(const Network &left, const Network &right) {
  if (left.nodeCount() != right.nodeCount() || left.carriers().size() != right.carriers().size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.carriers().size(); ++index) {
    const Carrier &a = left.carriers()[index];
    const Carrier &b = right.carriers()[index];
    const bool samePoints =
        std::ranges::equal(a.Points, b.Points, [](const CarrierPoint &p, const CarrierPoint &q) {
          return sameBits(p.X, q.X) && sameBits(p.Z, q.Z) && sameBits(p.Distance, q.Distance);
        });
    const bool sameStops =
        std::ranges::equal(a.Stops, b.Stops, [](const CarrierStop &p, const CarrierStop &q) {
          return sameBits(p.Distance, q.Distance) && p.Node == q.Node;
        });
    if (a.Key != b.Key || !samePoints || !sameStops) {
      return false;
    }
  }
  for (uint32_t node = 0; node < left.nodeCount(); ++node) {
    if (left.nodeAnchor(node) != right.nodeAnchor(node)) {
      return false;
    }
  }
  return true;
}

inline bool sameNetworks(const World &left, const World &right) {
  return sameNetwork(parkNetwork(left, PathKind::Guest), parkNetwork(right, PathKind::Guest)) &&
         sameNetwork(parkNetwork(left, PathKind::Backstage),
                     parkNetwork(right, PathKind::Backstage));
}

} // namespace tpj::test

#endif
