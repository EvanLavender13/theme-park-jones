#include "sim/routes/route_distance.h"

#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <queue>
#include <span>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

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

// The entry a place inside the edge takes from an entry at the edge's From end, the place's offset
// from that end added.
RouteEntry viaFrom(const NetworkEdge &edge, const RouteEntry &entry, double offset) {
  return {entry.Distance + offset, {edge.Carrier, edge.ToDistance, edge.FromDistance}};
}

// The entry a place inside the edge takes from an entry at the edge's To end.
RouteEntry viaTo(const NetworkEdge &edge, const RouteEntry &entry, double offset) {
  return {entry.Distance + offset, {edge.Carrier, edge.FromDistance, edge.ToDistance}};
}

// Keeps the candidate when it is the first or strictly less, so the earlier of equals stays.
void keepLeast(std::optional<RouteEntry> &best, const RouteEntry &candidate) {
  if (!best || candidate.Distance < best->Distance) {
    best = candidate;
  }
}

template <PathKind Kind>
std::optional<RouteEntry> routeEntryOn(const World &world, const Network &network,
                                       const Place &place, EntityKey source) {
  using Field = RouteDistance<Kind>;
  const std::optional<NetworkPosition> position = network.resolve(place);
  if (!position) {
    return std::nullopt;
  }
  if (const auto *node = std::get_if<NodePosition>(&*position)) {
    return sourceEntryAtNode<Field>(world, network, node->Node, source);
  }
  const FieldSlot<RouteEntry> *slot = sourceSlot<Field>(world, source);
  if (slot == nullptr) {
    return std::nullopt;
  }
  const EdgePosition &inside = std::get<EdgePosition>(*position);
  const NetworkEdge &edge = network.edges()[inside.Edge];
  std::optional<RouteEntry> best;
  // The source's entries at one end, in its order, as the medium's edge sample lists them: an
  // entry strictly inside the edge is at neither end.
  const auto atEnd = [&edge, slot](std::span<const Place> stops, const auto &take) {
    for (const PlacedEntry<RouteEntry> &entry : slot->Entries) {
      const Place &at = entry.At;
      const bool within = at.Carrier == edge.Carrier && at.Distance > edge.FromDistance &&
                          at.Distance < edge.ToDistance;
      if (!within && std::ranges::find(stops, at) != stops.end()) {
        take(entry.Value);
      }
    }
  };
  atEnd(network.stopPlaces(edge.From),
        [&](const RouteEntry &entry) { keepLeast(best, viaFrom(edge, entry, inside.FromOffset)); });
  atEnd(network.stopPlaces(edge.To),
        [&](const RouteEntry &entry) { keepLeast(best, viaTo(edge, entry, inside.ToOffset)); });
  return best;
}

} // namespace

std::vector<RouteEntry> sampleRouteEdge(const EdgeSample<RouteEntry> &sample) {
  std::optional<RouteEntry> best;
  // The From end's entries come first, so it wins ties.
  for (const RouteEntry &entry : sample.AtFrom) {
    keepLeast(best, viaFrom(sample.Edge, entry, sample.FromOffset));
  }
  for (const RouteEntry &entry : sample.AtTo) {
    keepLeast(best, viaTo(sample.Edge, entry, sample.ToOffset));
  }
  if (!best) {
    return {};
  }
  return {*best};
}

std::optional<RouteEntry> routeEntryAt(const World &world, PathKind kind, const Network &network,
                                       const Place &place, EntityKey source) {
  return kind == PathKind::Guest ? routeEntryOn<PathKind::Guest>(world, network, place, source)
                                 : routeEntryOn<PathKind::Backstage>(world, network, place, source);
}

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

void addRouteDistance(WorldSchema &schema) {
  addField<RouteDistance<PathKind::Guest>>(schema);
  addField<RouteDistance<PathKind::Backstage>>(schema);
  schema.addResolver(
      "route-distance", &resolveRouteDistance,
      {"path-networks", "guest-route-distance-field", "backstage-route-distance-field"});
}

} // namespace tpj
