#include "sim/routes/route_distance.h"

#include "sim/routes/internal/route_steps.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/world.h"

#include <algorithm>
#include <functional>
#include <optional>
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

// The lists the route-distance resolver keeps in the caller's buffers: the steps leaving each node,
// indexed by node, and the inner lists not in use; one source's entries at each node, the search's
// queue and settled nodes, and the source's anchored nodes; and the sources.
struct RouteLists {
  std::vector<std::vector<Leaving>> Steps;
  std::vector<std::vector<Leaving>> SpareSteps;
  std::vector<std::optional<RouteEntry>> Entries;
  std::vector<std::pair<double, uint32_t>> Queue;
  std::vector<uint8_t> Settled;
  std::vector<uint32_t> Anchored;
  std::vector<EntityKey> Sources;
};

// Fills the steps leaving each node: each edge gives one from each of its ends.
void fillLeavingSteps(const Network &network, RouteLists &lists) {
  resizeKeeping(lists.Steps, network.nodeCount(), lists.SpareSteps);
  for (std::vector<Leaving> &steps : lists.Steps) {
    steps.clear();
  }
  for (const NetworkEdge &edge : network.edges()) {
    lists.Steps[edge.From].push_back(
        {{edge.Carrier, edge.FromDistance, edge.ToDistance}, edge.To, edge.length()});
    lists.Steps[edge.To].push_back(
        {{edge.Carrier, edge.ToDistance, edge.FromDistance}, edge.From, edge.length()});
  }
}

// Fills the lists' Entries with the source's entry at each node, as routeEntries gives them, from
// the steps fillLeavingSteps filled for the network.
void searchEntries(const Network &network, EntityKey source, RouteLists &lists) {
  const uint32_t count = network.nodeCount();
  std::vector<std::optional<RouteEntry>> &entries = lists.Entries;
  fillGrowing(entries, count, std::optional<RouteEntry>{});
  fillGrowing(lists.Settled, count, uint8_t{0});

  // Dijkstra from every node the source anchors. Rounded addition is monotone, so each distance is
  // the least route length added from the source's end, whatever order ties are settled in. The
  // queue is a heap on a kept list, popped least first, as std::priority_queue with std::greater
  // pops it.
  using Queued = std::pair<double, uint32_t>;
  std::vector<Queued> &queue = lists.Queue;
  queue.clear();
  const auto push = [&queue](double distance, uint32_t node) {
    queue.emplace_back(distance, node);
    std::push_heap(queue.begin(), queue.end(), std::greater<>{});
  };
  network.anchoredNodes(source, lists.Anchored);
  for (const uint32_t node : lists.Anchored) {
    entries[node] = RouteEntry{};
    push(0.0, node);
  }
  while (!queue.empty()) {
    std::pop_heap(queue.begin(), queue.end(), std::greater<>{});
    const auto [distance, node] = queue.back();
    queue.pop_back();
    if (lists.Settled[node] != 0) {
      continue;
    }
    lists.Settled[node] = 1;
    for (const Leaving &step : lists.Steps[node]) {
      const double reached = distance + step.Length;
      std::optional<RouteEntry> &entry = entries[step.Arrives];
      if (!entry || reached < entry->Distance) {
        entry = RouteEntry{reached, {}};
        push(reached, step.Arrives);
      }
    }
  }

  // Each next step is chosen from every step achieving its node's distance, so it depends on the
  // network alone.
  for (uint32_t node = 0; node < count; ++node) {
    if (!entries[node] || network.nodeAnchor(node) == source) {
      continue;
    }
    RouteEntry &entry = *entries[node];
    const RouteStep *next = nullptr;
    for (const Leaving &step : lists.Steps[node]) {
      const std::optional<RouteEntry> &far = entries[step.Arrives];
      if (far && far->Distance + step.Length == entry.Distance &&
          (next == nullptr || isPreferredStep(step.Step, *next))) {
        next = &step.Step;
      }
    }
    if (next != nullptr) {
      entry.Next = *next;
    }
  }
}

// Publishes each entity anchoring a node of the kind's network, in ascending key order, with its
// entries at its reachable nodes' places, in node order.
template <PathKind Kind> void publishRouteDistance(World &world, RouteLists &lists) {
  const Network &network = parkNetwork(world, Kind);
  std::vector<EntityKey> &sources = lists.Sources;
  sources.clear();
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const EntityKey anchor = network.nodeAnchor(node);
    if (anchor != NULL_KEY) {
      sources.push_back(anchor);
    }
  }
  std::ranges::sort(sources);
  const auto repeated = std::ranges::unique(sources);
  sources.erase(repeated.begin(), repeated.end());
  fillLeavingSteps(network, lists);
  for (const EntityKey source : sources) {
    searchEntries(network, source, lists);
    std::vector<PlacedEntry<RouteEntry>> &placed =
        publishResolved<RouteDistance<Kind>>(world, source);
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      if (const std::optional<RouteEntry> &entry = lists.Entries[node]) {
        placed.push_back({network.nodePlace(node), *entry});
      }
    }
  }
}

void resolveRouteDistance(World &world, TickBuffers &buffers) {
  RouteLists &lists = buffers.get<RouteLists>();
  publishRouteDistance<PathKind::Guest>(world, lists);
  publishRouteDistance<PathKind::Backstage>(world, lists);
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

void sampleRouteEdge(const EdgeSample<RouteEntry> &sample, std::vector<RouteEntry> &out) {
  std::optional<RouteEntry> best;
  // The From end's entries come first, so it wins ties.
  for (const RouteEntry &entry : sample.AtFrom) {
    keepLeast(best, viaFrom(sample.Edge, entry, sample.FromOffset));
  }
  for (const RouteEntry &entry : sample.AtTo) {
    keepLeast(best, viaTo(sample.Edge, entry, sample.ToOffset));
  }
  if (best) {
    out.push_back(*best);
  }
}

std::optional<RouteEntry> routeEntryAt(const World &world, PathKind kind, const Network &network,
                                       const Place &place, EntityKey source) {
  return kind == PathKind::Guest ? routeEntryOn<PathKind::Guest>(world, network, place, source)
                                 : routeEntryOn<PathKind::Backstage>(world, network, place, source);
}

std::vector<std::optional<RouteEntry>> routeEntries(const Network &network, EntityKey source) {
  RouteLists lists;
  fillLeavingSteps(network, lists);
  searchEntries(network, source, lists);
  return lists.Entries;
}

void addRouteDistance(WorldSchema &schema) {
  addField<RouteDistance<PathKind::Guest>>(schema);
  addField<RouteDistance<PathKind::Backstage>>(schema);
  schema.addResolver(
      "route-distance", &resolveRouteDistance,
      {"path-networks", "guest-route-distance-field", "backstage-route-distance-field"});
}

} // namespace tpj
