#include "sim/routes/route_distance.h"

#include "sim/routes/internal/route_steps.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <span>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

// A distance in whole millimetres. UNREACHED marks a node no route joins to the source.
constexpr int64_t UNREACHED = INT64_MAX;
// An old node no new node is matched to, or a new node matched to no old one.
constexpr uint32_t NO_NODE = UINT32_MAX;

// The metres rounded to whole millimetres.
int64_t millimetresOf(double metres) { return std::llround(metres * 1000.0); }

double metresOf(int64_t millimetres) { return static_cast<double>(millimetres) / 1000.0; }

// A step leaving a node, with the node it arrives at and the length of its edge in millimetres.
struct Leaving {
  RouteStep Step;
  uint32_t Arrives = 0;
  int64_t Length = 0;
};

// A change to one place of a source's entries: the entry there, or none.
struct RouteWrite {
  Place At;
  std::optional<RouteEntry> Value;
};

// The lists the route-distance resolver keeps in the caller's buffers.
struct RouteLists {
  // The steps leaving each node, indexed by node, and the inner lists not in use.
  std::vector<std::vector<Leaving>> Steps;
  std::vector<std::vector<Leaving>> SpareSteps;
  // A full search: one source's distance and entry at each node, its queue and settled nodes,
  // and the source's anchored nodes.
  std::vector<int64_t> Distances;
  std::vector<std::optional<RouteEntry>> Entries;
  std::vector<std::pair<int64_t, uint32_t>> Queue;
  std::vector<uint8_t> Settled;
  std::vector<uint32_t> Anchored;
  // The sources, and the sources of slots the network no longer anchors.
  std::vector<EntityKey> Sources;
  std::vector<EntityKey> Dropped;
  // The nodes in order of their places.
  std::vector<uint32_t> ByPlace;
  // The comparison with the previous network, once per resolution: the old node matched to each
  // new node, whether each old node is matched, whether each new node is touched, the touched
  // nodes ascending, and the places of the old nodes no new node is matched to, in place order.
  std::vector<uint32_t> OldNode;
  std::vector<uint8_t> OldMatched;
  std::vector<uint8_t> IsTouched;
  std::vector<uint32_t> Touched;
  std::vector<Place> Removed;
  // One source's update: each node's distance, its lookahead, and its old distance, loaded when
  // LoadedIn holds the source's generation, the nodes loaded, the nodes to write and the
  // generation they were added in, the writes in place order, and the entries after the first.
  uint32_t Generation = 0;
  std::vector<int64_t> G;
  std::vector<int64_t> Rhs;
  std::vector<int64_t> Old;
  std::vector<uint32_t> LoadedIn;
  std::vector<uint32_t> Loaded;
  std::vector<uint32_t> WrittenIn;
  std::vector<uint32_t> Written;
  std::vector<RouteWrite> Writes;
  std::vector<PlacedEntry<RouteEntry>> Tail;
};

// Grows a kept list to at least count values, giving new ones the value.
template <typename T> void growTo(std::vector<T> &kept, size_t count, const T &value) {
  if (kept.size() < count) {
    reserveGrowing(kept, count);
    kept.resize(count, value);
  }
}

// Fills the steps leaving each node: each edge gives one from each of its ends. An edge's length is
// its stops' distances, each rounded to whole millimetres, subtracted.
void fillLeavingSteps(const Network &network, RouteLists &lists) {
  resizeKeeping(lists.Steps, network.nodeCount(), lists.SpareSteps);
  for (std::vector<Leaving> &steps : lists.Steps) {
    steps.clear();
  }
  for (const NetworkEdge &edge : network.edges()) {
    const int64_t length = millimetresOf(edge.ToDistance) - millimetresOf(edge.FromDistance);
    lists.Steps[edge.From].push_back(
        {{edge.Carrier, edge.FromDistance, edge.ToDistance}, edge.To, length});
    lists.Steps[edge.To].push_back(
        {{edge.Carrier, edge.ToDistance, edge.FromDistance}, edge.From, length});
  }
}

// Fills ByPlace with the network's nodes in order of their places.
void orderNodesByPlace(const Network &network, RouteLists &lists) {
  std::vector<uint32_t> &order = lists.ByPlace;
  order.clear();
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    order.push_back(node);
  }
  std::ranges::sort(order, [&network](uint32_t a, uint32_t b) {
    return placeBefore(network.nodePlace(a), network.nodePlace(b));
  });
}

// The step a node not anchored to the source takes: the first by isPreferredStep of those leaving
// it whose far node's distance plus the step's length is its distance, or none.
template <typename DistanceOf>
const RouteStep *nextStep(std::span<const Leaving> steps, int64_t distance, DistanceOf distanceOf) {
  const RouteStep *next = nullptr;
  for (const Leaving &step : steps) {
    const int64_t far = distanceOf(step.Arrives);
    if (far != UNREACHED && far + step.Length == distance &&
        (next == nullptr || isPreferredStep(step.Step, *next))) {
      next = &step.Step;
    }
  }
  return next;
}

// Fills the lists' Distances and Entries with the source's distance and entry at each node, as
// routeEntries gives them, from the steps fillLeavingSteps filled for the network.
void searchEntries(const Network &network, EntityKey source, RouteLists &lists) {
  const uint32_t count = network.nodeCount();
  std::vector<int64_t> &distances = lists.Distances;
  fillGrowing(distances, count, UNREACHED);
  fillGrowing(lists.Entries, count, std::optional<RouteEntry>{});
  fillGrowing(lists.Settled, count, uint8_t{0});

  // Dijkstra from every node the source anchors, in whole millimetres, so each distance is exactly
  // the least route length whatever order ties are settled in. The queue is a heap on a kept list,
  // popped least first, as std::priority_queue with std::greater pops it.
  using Queued = std::pair<int64_t, uint32_t>;
  std::vector<Queued> &queue = lists.Queue;
  queue.clear();
  const auto push = [&queue](int64_t distance, uint32_t node) {
    queue.emplace_back(distance, node);
    std::push_heap(queue.begin(), queue.end(), std::greater<>{});
  };
  network.anchoredNodes(source, lists.Anchored);
  for (const uint32_t node : lists.Anchored) {
    distances[node] = 0;
    push(0, node);
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
      const int64_t reached = distance + step.Length;
      if (reached < distances[step.Arrives]) {
        distances[step.Arrives] = reached;
        push(reached, step.Arrives);
      }
    }
  }

  // Each next step is chosen from every step achieving its node's distance, so it depends on the
  // network alone.
  for (uint32_t node = 0; node < count; ++node) {
    if (distances[node] == UNREACHED) {
      continue;
    }
    RouteEntry &entry = lists.Entries[node].emplace(RouteEntry{metresOf(distances[node]), {}});
    if (network.nodeAnchor(node) == source) {
      continue;
    }
    if (const RouteStep *next = nextStep(lists.Steps[node], distances[node],
                                         [&distances](uint32_t far) { return distances[far]; })) {
      entry.Next = *next;
    }
  }
}

// Searches from the source over the whole network and replaces its entries with an entry at each
// reached node's place, in place order.
template <PathKind Kind>
void fullSearch(World &world, const Network &network, EntityKey source, RouteLists &lists) {
  searchEntries(network, source, lists);
  std::vector<PlacedEntry<RouteEntry>> &placed = editResolved<RouteDistance<Kind>>(world, source);
  placed.clear();
  for (const uint32_t node : lists.ByPlace) {
    if (const std::optional<RouteEntry> &entry = lists.Entries[node]) {
      placed.push_back({network.nodePlace(node), *entry});
    }
  }
}

// Whether the new node's edges match its old node's: the same edges of the same carriers between
// the same distances, each ending at the matched far node, and the same anchor.
bool sameEdges(const Network &network, const Network &previous, uint32_t node, uint32_t old,
               std::span<const uint32_t> oldNode) {
  if (network.nodeAnchor(node) != previous.nodeAnchor(old)) {
    return false;
  }
  const std::span<const EdgeEnd> ends = network.edgeEnds(node);
  const std::span<const EdgeEnd> oldEnds = previous.edgeEnds(old);
  if (ends.size() != oldEnds.size()) {
    return false;
  }
  for (size_t i = 0; i < ends.size(); ++i) {
    const NetworkEdge &edge = network.edges()[ends[i].Edge];
    const NetworkEdge &oldEdge = previous.edges()[oldEnds[i].Edge];
    const uint32_t far = ends[i].AtFrom ? edge.To : edge.From;
    const uint32_t oldFar = oldEnds[i].AtFrom ? oldEdge.To : oldEdge.From;
    if (ends[i].AtFrom != oldEnds[i].AtFrom || edge.Carrier != oldEdge.Carrier ||
        edge.FromDistance != oldEdge.FromDistance || edge.ToDistance != oldEdge.ToDistance ||
        oldNode[far] != oldFar) {
      return false;
    }
  }
  return true;
}

// Compares the network with the previous one: matches each new node to the old node at the same
// place, touches each new node whose edges or anchor differ from its old node's or that has none,
// and lists the places of the old nodes no new node is matched to. An untouched node's distance
// and lookahead are then those of its old node until a neighbour's distance changes.
void compareNetworks(const Network &network, const Network &previous, RouteLists &lists) {
  const uint32_t count = network.nodeCount();
  fillGrowing(lists.OldNode, count, NO_NODE);
  fillGrowing(lists.OldMatched, size_t{previous.nodeCount()}, uint8_t{0});
  fillGrowing(lists.IsTouched, count, uint8_t{0});
  for (uint32_t node = 0; node < count; ++node) {
    const Place place = network.nodePlace(node);
    const std::optional<NetworkPosition> old = previous.resolve(place);
    if (old) {
      if (const auto *at = std::get_if<NodePosition>(&*old)) {
        if (previous.nodePlace(at->Node) == place) {
          lists.OldNode[node] = at->Node;
          lists.OldMatched[at->Node] = 1;
        }
      }
    }
  }
  lists.Touched.clear();
  for (uint32_t node = 0; node < count; ++node) {
    const uint32_t old = lists.OldNode[node];
    if (old == NO_NODE || !sameEdges(network, previous, node, old, lists.OldNode)) {
      lists.IsTouched[node] = 1;
      lists.Touched.push_back(node);
    }
  }
  lists.Removed.clear();
  for (uint32_t old = 0; old < previous.nodeCount(); ++old) {
    if (lists.OldMatched[old] == 0) {
      lists.Removed.push_back(previous.nodePlace(old));
    }
  }
  std::ranges::sort(lists.Removed, placeBefore);
}

// One source's update in place from its entries for the previous network, which LPA* without a
// heuristic carries to the new one: it visits only the touched nodes, the nodes whose distance
// changes, and their neighbours.
class SourceUpdate {
public:
  SourceUpdate(const Network &network, EntityKey source, const FieldSlot<RouteEntry> &slot,
               RouteLists &lists)
      : Net(network), Source(source), Slot(slot), Lists(lists) {
    if (++Lists.Generation == 0) {
      std::ranges::fill(Lists.LoadedIn, 0U);
      std::ranges::fill(Lists.WrittenIn, 0U);
      Lists.Generation = 1;
    }
    const size_t count = Net.nodeCount();
    growTo(Lists.G, count, UNREACHED);
    growTo(Lists.Rhs, count, UNREACHED);
    growTo(Lists.Old, count, UNREACHED);
    growTo(Lists.LoadedIn, count, 0U);
    growTo(Lists.WrittenIn, count, 0U);
    Lists.Loaded.clear();
    Lists.Written.clear();
    Lists.Queue.clear();
  }

  // Settles every node whose distance the edit changes.
  void settle() {
    for (const uint32_t node : Lists.Touched) {
      load(node);
      Lists.Rhs[node] = lookahead(node);
      queueIfInconsistent(node);
    }
    while (!Lists.Queue.empty()) {
      std::pop_heap(Lists.Queue.begin(), Lists.Queue.end(), std::greater<>{});
      const auto [key, node] = Lists.Queue.back();
      Lists.Queue.pop_back();
      int64_t &g = Lists.G[node];
      const int64_t rhs = Lists.Rhs[node];
      if (g == rhs || key != std::min(g, rhs)) {
        continue;
      }
      if (g > rhs) {
        g = rhs;
        for (const Leaving &step : Lists.Steps[node]) {
          const uint32_t far = step.Arrives;
          load(far);
          if (g + step.Length < Lists.Rhs[far] && Net.nodeAnchor(far) != Source) {
            Lists.Rhs[far] = g + step.Length;
            queueIfInconsistent(far);
          }
        }
      } else {
        g = UNREACHED;
        Lists.Rhs[node] = lookahead(node);
        queueIfInconsistent(node);
        for (const Leaving &step : Lists.Steps[node]) {
          const uint32_t far = step.Arrives;
          load(far);
          Lists.Rhs[far] = lookahead(far);
          queueIfInconsistent(far);
        }
      }
    }
  }

  // Fills the lists' Writes with the entry, or none, at each place whose entry may differ: the
  // touched nodes, the nodes whose distance changed and their neighbours, and the removed places,
  // in place order. Gives whether any differs from the source's entries.
  bool fillWrites() {
    for (const uint32_t node : Lists.Touched) {
      addWritten(node);
    }
    // Loaded grows while neighbours load, so it is walked by index over the nodes loaded so far.
    const size_t loaded = Lists.Loaded.size();
    for (size_t i = 0; i < loaded; ++i) {
      const uint32_t node = Lists.Loaded[i];
      if (Lists.G[node] != Lists.Old[node]) {
        addWritten(node);
        for (const Leaving &step : Lists.Steps[node]) {
          addWritten(step.Arrives);
        }
      }
    }
    Lists.Writes.clear();
    bool differs = false;
    for (const uint32_t node : Lists.Written) {
      const Place at = Net.nodePlace(node);
      std::optional<RouteEntry> value = entryAt(node);
      const PlacedEntry<RouteEntry> *held = heldAt(at);
      if (held == nullptr ? !value.has_value() : value.has_value() && held->Value == *value) {
        continue;
      }
      differs = true;
      Lists.Writes.push_back({at, value});
    }
    for (const Place &at : Lists.Removed) {
      if (heldAt(at) != nullptr) {
        differs = true;
        Lists.Writes.push_back({at, std::nullopt});
      }
    }
    std::ranges::sort(Lists.Writes, placeBefore, &RouteWrite::At);
    return differs;
  }

private:
  // The source's entry at the place before this resolution, or none.
  [[nodiscard]] const PlacedEntry<RouteEntry> *heldAt(const Place &place) const {
    const auto at =
        std::ranges::lower_bound(Slot.Entries, place, placeBefore, &PlacedEntry<RouteEntry>::At);
    return at != Slot.Entries.end() && at->At == place ? &*at : nullptr;
  }

  // Loads the node's old distance as its distance and lookahead, once per source: the source's
  // entry at its place, when its old node has one.
  void load(uint32_t node) {
    if (Lists.LoadedIn[node] == Lists.Generation) {
      return;
    }
    Lists.LoadedIn[node] = Lists.Generation;
    Lists.Loaded.push_back(node);
    int64_t old = UNREACHED;
    if (Lists.OldNode[node] != NO_NODE) {
      if (const PlacedEntry<RouteEntry> *held = heldAt(Net.nodePlace(node))) {
        old = millimetresOf(held->Value.Distance);
      }
    }
    Lists.G[node] = old;
    Lists.Rhs[node] = old;
    Lists.Old[node] = old;
  }

  // The least distance the node's neighbours give it, or 0 at a node the source anchors.
  int64_t lookahead(uint32_t node) {
    if (Net.nodeAnchor(node) == Source) {
      return 0;
    }
    int64_t least = UNREACHED;
    for (const Leaving &step : Lists.Steps[node]) {
      load(step.Arrives);
      const int64_t far = Lists.G[step.Arrives];
      if (far != UNREACHED) {
        least = std::min(least, far + step.Length);
      }
    }
    return least;
  }

  void queueIfInconsistent(uint32_t node) {
    const int64_t g = Lists.G[node];
    const int64_t rhs = Lists.Rhs[node];
    if (g != rhs) {
      Lists.Queue.emplace_back(std::min(g, rhs), node);
      std::push_heap(Lists.Queue.begin(), Lists.Queue.end(), std::greater<>{});
    }
  }

  void addWritten(uint32_t node) {
    if (Lists.WrittenIn[node] != Lists.Generation) {
      Lists.WrittenIn[node] = Lists.Generation;
      Lists.Written.push_back(node);
    }
  }

  // The node's entry after the update, as searchEntries gives it.
  std::optional<RouteEntry> entryAt(uint32_t node) {
    load(node);
    const int64_t distance = Lists.G[node];
    if (distance == UNREACHED) {
      return std::nullopt;
    }
    RouteEntry entry{metresOf(distance), {}};
    if (Net.nodeAnchor(node) == Source) {
      return entry;
    }
    const RouteStep *next = nextStep(Lists.Steps[node], distance, [this](uint32_t far) {
      load(far);
      return Lists.G[far];
    });
    if (next != nullptr) {
      entry.Next = *next;
    }
    return entry;
  }

  const Network &Net;
  EntityKey Source;
  const FieldSlot<RouteEntry> &Slot;
  RouteLists &Lists;
};

// Merges the writes, in place order, into the entries, which are in place order: each write's entry
// replaces or joins the entries at its place, or removes them. Only the entries after the first
// write move.
void mergeWrites(std::vector<PlacedEntry<RouteEntry>> &entries, RouteLists &lists) {
  const std::vector<RouteWrite> &writes = lists.Writes;
  const auto first = std::ranges::lower_bound(entries, writes.front().At, placeBefore,
                                              &PlacedEntry<RouteEntry>::At);
  copyGrowing(lists.Tail, std::span<const PlacedEntry<RouteEntry>>(first, entries.end()));
  entries.erase(first, entries.end());
  auto held = lists.Tail.begin();
  for (const RouteWrite &write : writes) {
    while (held != lists.Tail.end() && placeBefore(held->At, write.At)) {
      entries.push_back(*held);
      ++held;
    }
    if (held != lists.Tail.end() && held->At == write.At) {
      ++held;
    }
    if (write.Value) {
      entries.push_back({write.At, *write.Value});
    }
  }
  entries.insert(entries.end(), held, lists.Tail.end());
}

// Updates the source's entries in place from the previous network's.
template <PathKind Kind>
void updateSource(World &world, const Network &network, EntityKey source, RouteLists &lists) {
  using Field = RouteDistance<Kind>;
  const std::span<const FieldSlot<RouteEntry>> slots = resolvedSlots<Field>(world);
  const auto slot = std::ranges::lower_bound(slots, source, {}, &FieldSlot<RouteEntry>::Source);
  SourceUpdate update(network, source, *slot, lists);
  update.settle();
  if (update.fillWrites()) {
    mergeWrites(editResolved<Field>(world, source), lists);
  }
}

// Whether the source anchors a node the comparison touched.
bool anchorTouched(const Network &network, EntityKey source, RouteLists &lists) {
  network.anchoredNodes(source, lists.Anchored);
  return std::ranges::any_of(lists.Anchored,
                             [&lists](uint32_t node) { return lists.IsTouched[node] != 0; });
}

// Gives each entity anchoring a node of the kind's network its entries at its reachable nodes'
// places, in place order, and drops the entries of every other source. With a previous network, a
// source that has entries and anchors no touched node is updated in place from them; every other
// source is searched in full.
template <PathKind Kind> void publishRouteDistance(World &world, RouteLists &lists) {
  using Field = RouteDistance<Kind>;
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

  // Slots are in ascending source order, as the sources are.
  lists.Dropped.clear();
  for (const FieldSlot<RouteEntry> &slot : resolvedSlots<Field>(world)) {
    if (!std::ranges::binary_search(sources, slot.Source)) {
      lists.Dropped.push_back(slot.Source);
    }
  }
  for (const EntityKey source : lists.Dropped) {
    dropResolved<Field>(world, source);
  }

  fillLeavingSteps(network, lists);
  orderNodesByPlace(network, lists);
  const Network *previous = previousNetwork(world, Kind);
  if (previous != nullptr) {
    compareNetworks(network, *previous, lists);
  }
  for (const EntityKey source : sources) {
    const bool held = std::ranges::binary_search(resolvedSlots<Field>(world), source, {},
                                                 &FieldSlot<RouteEntry>::Source);
    if (previous == nullptr || !held || anchorTouched(network, source, lists)) {
      fullSearch<Kind>(world, network, source, lists);
    } else {
      updateSource<Kind>(world, network, source, lists);
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
