#ifndef TPJ_SIM_ROUTES_BEST_PLACES_H
#define TPJ_SIM_ROUTES_BEST_PLACES_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/routes/route_distance.h"

#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

// A place a search starts from, such as a piece of furniture: its key, the key of the entity it
// belongs to, its place, and its value, higher being better.
struct PlaceSeed {
  EntityKey Key = NULL_KEY;
  EntityKey Owner = NULL_KEY;
  Place At;
  double Value = 0.0;
};

// A seed as found from a place: the seed, its route distance, and the first step toward it.
struct BestPlace {
  EntityKey Key = NULL_KEY;
  EntityKey Owner = NULL_KEY;
  Place At;
  double Value = 0.0;
  double Distance = 0.0;
  RouteStep Next;

  bool operator==(const BestPlace &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, BestPlace &place) {
  visitor.field("key", place.Key);
  visitor.field("owner", place.Owner);
  visitor.field("at", place.At);
  visitor.field("value", place.Value);
  visitor.field("distance", place.Distance);
  visitor.field("next", place.Next);
}

// At each node, the seeds that fewer than keep others dominate there, at the node's nodePlace,
// nodes in order and each node's seeds in order of distance, then higher value, then lower key.
// Then one arrival entry for each seed strictly inside an edge, at its own place, in key order.
// Throws std::invalid_argument when two seeds share a key.
std::vector<PlacedEntry<BestPlace>> bestPlaces(const Network &network,
                                               std::span<const PlaceSeed> seeds, uint32_t keep);

// Appends to out the seeds that fewer than keep others dominate at the sample's place, in the
// order bestPlaces gives a node's. Allocates nothing but out's growth.
void sampleBestPlaceEdge(const EdgeSample<BestPlace> &sample, uint32_t keep,
                         std::vector<BestPlace> &out);

} // namespace tpj

#endif
