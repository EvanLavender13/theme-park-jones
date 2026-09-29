#include "sim/routes/route_distance.h"

#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <queue>
#include <stdint.h>
#include <utility>
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

} // namespace

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
    consider(
        {entry.Distance + sample.FromOffset, {edge.Carrier, edge.ToDistance, edge.FromDistance}});
  }
  for (const RouteEntry &entry : sample.AtTo) {
    consider(
        {entry.Distance + sample.ToOffset, {edge.Carrier, edge.FromDistance, edge.ToDistance}});
  }
  if (!best) {
    return {};
  }
  return {*best};
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
