#include "sim/guests/footfall.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/guests/internal/footfall.h"
#include "sim/guests/internal/guest.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// The field's entity, which holds the footfall and is the source of its entries.
constexpr EntityKey FOOTFALL_SOURCE = fieldKey(HungryFootfall::Name);

// The index in edges() of the stretch the place lies in: the one of its own carrier whose
// FromDistance it is at or above and whose ToDistance it is below, or the carrier's last at its
// length. None when the network lacks the carrier or the distance is off it.
std::optional<size_t> stretchOf(const Network &network, const Place &place) {
  const std::vector<NetworkEdge> &edges = network.edges();
  const auto first = std::ranges::lower_bound(edges, place.Carrier, {}, &NetworkEdge::Carrier);
  const auto last =
      std::ranges::upper_bound(first, edges.end(), place.Carrier, {}, &NetworkEdge::Carrier);
  if (first == last || !(place.Distance >= first->FromDistance) ||
      place.Distance > std::prev(last)->ToDistance) {
    return std::nullopt;
  }
  const auto after =
      std::ranges::upper_bound(first, last, place.Distance, {}, &NetworkEdge::FromDistance);
  return static_cast<size_t>(std::prev(after) - edges.begin());
}

// Moves each stretch's value toward the summed hunger of the guests on it, holds the new values
// at the stretches' midpoints, and publishes them with each node's mean of its stretches.
void stepFootfall(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::vector<NetworkEdge> &edges = network.edges();
  Footfall &footfall = world.Registry.get_or_emplace<Footfall>(world.findEntity(FOOTFALL_SOURCE));
  std::vector<double> held(edges.size(), 0.0);
  for (const PlacedEntry<double> &entry : footfall.Stretches) {
    if (const std::optional<size_t> stretch = stretchOf(network, entry.At)) {
      held[*stretch] += entry.Value;
    }
  }
  std::vector<double> hunger(edges.size(), 0.0);
  for (const EntityKey key : parkGuests(world)) {
    const Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (const std::optional<size_t> stretch = stretchOf(network, guest.At)) {
      hunger[*stretch] += guest.Hunger;
    }
  }
  std::vector<PlacedEntry<double>> stretches;
  stretches.reserve(edges.size());
  std::vector<double> nodeSums(network.nodeCount(), 0.0);
  std::vector<uint32_t> nodeEnds(network.nodeCount(), 0);
  for (size_t i = 0; i < edges.size(); ++i) {
    const NetworkEdge &edge = edges[i];
    const double value = held[i] + ((hunger[i] - held[i]) / static_cast<double>(FOOTFALL_TIME));
    stretches.push_back({Place{edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2.0}, value});
    nodeSums[edge.From] += value;
    ++nodeEnds[edge.From];
    nodeSums[edge.To] += value;
    ++nodeEnds[edge.To];
  }
  footfall.Stretches = stretches;
  std::vector<PlacedEntry<double>> entries = std::move(stretches);
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    entries.push_back(
        {network.nodePlace(node), nodeSums[node] / static_cast<double>(nodeEnds[node])});
  }
  publishStepped<HungryFootfall>(world, FOOTFALL_SOURCE, std::move(entries));
}

// Carries each held value's place from the guest network before the resolution to the new one,
// dropping those whose places are retired.
void carryFootfall(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  const entt::entity entity = world.findEntity(FOOTFALL_SOURCE);
  auto *footfall = before == nullptr || entity == entt::null
                       ? nullptr
                       : world.Registry.try_get<Footfall>(entity);
  if (footfall == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  std::vector<PlacedEntry<double>> carried;
  carried.reserve(footfall->Stretches.size());
  for (const PlacedEntry<double> &entry : footfall->Stretches) {
    if (const std::optional<Place> at = carryOver(entry.At, *before, after)) {
      carried.push_back({*at, entry.Value});
    }
  }
  footfall->Stretches = std::move(carried);
}

} // namespace

std::vector<double> HungryFootfall::sampleEdge(const EdgeSample<double> &sample) {
  std::vector<double> values;
  values.reserve(sample.Along.size());
  for (const EdgeEntry<double> &entry : sample.Along) {
    values.push_back(entry.Value);
  }
  return values;
}

void addFootfall(WorldSchema &schema) {
  addField<HungryFootfall>(schema);
  schema.addComponent<Footfall>("footfall", DataKind::State);
  schema.addSystem(&stepFootfall);
  schema.addFinisher(&carryFootfall);
}

} // namespace tpj
