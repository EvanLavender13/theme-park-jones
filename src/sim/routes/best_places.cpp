#include "sim/routes/best_places.h"

#include "sim/routes/internal/route_steps.h"

#include <algorithm>
#include <cmath>
#include <compare>
#include <functional>
#include <numeric>
#include <optional>
#include <queue>
#include <stddef.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

// A seed whose place resolves on the network, with where it resolves.
struct ResolvedSeed {
  PlaceSeed Seed;
  NetworkPosition Position;
};

// A seed's label kept at a node: the seed's index among the resolved seeds, its value, and its
// distance.
struct Kept {
  uint32_t Seed = 0;
  double Value = 0.0;
  double Distance = 0.0;
};

// A seed's label waiting at a node. A seed's rank is its place in order of higher value, then
// lower key, so labels compare by distance, then rank, then node.
struct Label {
  double Distance = 0.0;
  uint32_t Rank = 0;
  uint32_t Node = 0;

  auto operator<=>(const Label &) const = default;
};

// The seeds whose places resolve, in key order. Throws std::invalid_argument when two seeds share
// a key.
std::vector<ResolvedSeed> resolveSeeds(const Network &network, std::span<const PlaceSeed> seeds) {
  std::vector<PlaceSeed> sorted(seeds.begin(), seeds.end());
  std::ranges::sort(sorted, {}, &PlaceSeed::Key);
  const auto repeated = std::ranges::adjacent_find(sorted, {}, &PlaceSeed::Key);
  if (repeated != sorted.end()) {
    throw std::invalid_argument("two seeds share the key " +
                                std::to_string(static_cast<uint64_t>(repeated->Key)));
  }
  std::vector<ResolvedSeed> resolved;
  for (const PlaceSeed &seed : sorted) {
    if (const std::optional<NetworkPosition> position = network.resolve(seed.At)) {
      resolved.push_back({seed, *position});
    }
  }
  return resolved;
}

// The seed's kept distance at a node, or none when the node doesn't keep it.
std::optional<double> keptDistance(const std::vector<Kept> &kept, uint32_t seed) {
  const auto found = std::ranges::find(kept, seed, &Kept::Seed);
  if (found == kept.end()) {
    return std::nullopt;
  }
  return found->Distance;
}

// Whether the node's kept labels already hold the seed, or keep labels whose seeds' values are at
// least its own. Every label the node keeps came before any label still to come, so either one
// means the node won't keep the seed.
bool isRefused(const std::vector<Kept> &kept, const std::vector<ResolvedSeed> &seeds, uint32_t seed,
               uint32_t keep) {
  if (keptDistance(kept, seed)) {
    return true;
  }
  const double value = seeds[seed].Seed.Value;
  const auto atLeast =
      std::ranges::count_if(kept, [value](const Kept &other) { return other.Value >= value; });
  return !std::cmp_less(atLeast, keep);
}

// The seeds' own labels, in order: a seed at a node starts there at distance 0, and a seed inside
// an edge starts at both its ends at its distance from each.
std::vector<Label> startLabels(const Network &network, const std::vector<ResolvedSeed> &seeds,
                               const std::vector<uint32_t> &rankOf) {
  std::vector<Label> starts;
  for (uint32_t seed = 0; seed < seeds.size(); ++seed) {
    if (const auto *node = std::get_if<NodePosition>(&seeds[seed].Position)) {
      starts.push_back({0.0, rankOf[seed], node->Node});
    } else {
      const auto &inside = std::get<EdgePosition>(seeds[seed].Position);
      const NetworkEdge &edge = network.edges()[inside.Edge];
      starts.push_back({inside.FromOffset, rankOf[seed], edge.From});
      starts.push_back({inside.ToOffset, rankOf[seed], edge.To});
    }
  }
  std::ranges::sort(starts);
  return starts;
}

// The labels each node keeps, in the order kept. A node keeps a seed's first label while fewer
// than keep labels it already keeps have at least the seed's value, and only kept labels spread.
std::vector<std::vector<Kept>> searchSeeds(const Network &network,
                                           const std::vector<ResolvedSeed> &seeds, uint32_t keep) {
  // The seeds by rank: higher value first, then lower key, which is their order already.
  std::vector<uint32_t> byRank(seeds.size());
  std::iota(byRank.begin(), byRank.end(), 0U);
  std::ranges::stable_sort(byRank, std::greater<>{},
                           [&seeds](uint32_t seed) { return seeds[seed].Seed.Value; });
  std::vector<uint32_t> rankOf(seeds.size());
  for (uint32_t rank = 0; rank < byRank.size(); ++rank) {
    rankOf[byRank[rank]] = rank;
  }
  std::vector<std::vector<Kept>> kept(network.nodeCount());
  // The seeds' own labels wait in order apart from the queue, so the queue holds only the labels
  // spreading from kept ones and stays small.
  const std::vector<Label> starts = startLabels(network, seeds, rankOf);
  auto nextStart = starts.begin();
  std::priority_queue<Label, std::vector<Label>, std::greater<>> queue;
  while (nextStart != starts.end() || !queue.empty()) {
    Label label;
    if (queue.empty() || (nextStart != starts.end() && *nextStart < queue.top())) {
      label = *nextStart;
      ++nextStart;
    } else {
      label = queue.top();
      queue.pop();
    }
    const uint32_t seed = byRank[label.Rank];
    if (isRefused(kept[label.Node], seeds, seed, keep)) {
      continue;
    }
    kept[label.Node].push_back({seed, seeds[seed].Seed.Value, label.Distance});
    for (const EdgeEnd &end : network.edgeEnds(label.Node)) {
      const NetworkEdge &edge = network.edges()[end.Edge];
      const uint32_t far = end.AtFrom ? edge.To : edge.From;
      if (!isRefused(kept[far], seeds, seed, keep)) {
        queue.push({label.Distance + edge.length(), label.Rank, far});
      }
    }
  }
  return kept;
}

// The next step from the node toward the seed kept there at the distance: arrival at the seed's
// own node, or else the first by isPreferredStep of the steps that achieve the distance, along an
// edge to a node keeping the seed or into the edge holding the seed's place.
RouteStep nextStep(const Network &network, const std::vector<std::vector<Kept>> &kept,
                   const ResolvedSeed &seed, uint32_t seedIndex, uint32_t node, double distance) {
  if (const auto *at = std::get_if<NodePosition>(&seed.Position);
      at != nullptr && at->Node == node && distance == 0.0) {
    return {};
  }
  const auto *inside = std::get_if<EdgePosition>(&seed.Position);
  std::optional<RouteStep> next;
  const auto consider = [&next](const RouteStep &step) {
    if (!next || isPreferredStep(step, *next)) {
      next = step;
    }
  };
  for (const EdgeEnd &end : network.edgeEnds(node)) {
    const NetworkEdge &edge = network.edges()[end.Edge];
    const double here = end.AtFrom ? edge.FromDistance : edge.ToDistance;
    const double there = end.AtFrom ? edge.ToDistance : edge.FromDistance;
    const std::optional<double> far =
        keptDistance(kept[end.AtFrom ? edge.To : edge.From], seedIndex);
    if (far && *far + edge.length() == distance) {
      consider({edge.Carrier, here, there});
    }
    if (inside != nullptr && inside->Edge == end.Edge &&
        (end.AtFrom ? inside->FromOffset : inside->ToOffset) == distance) {
      consider({edge.Carrier, here, seed.Seed.At.Distance});
    }
  }
  return next.value_or(RouteStep{});
}

// The seed as found at the distance with the step.
BestPlace foundAs(const PlaceSeed &seed, double distance, const RouteStep &next) {
  return {seed.Key, seed.Owner, seed.At, seed.Value, distance, next};
}

// Whether a place comes before another in a sample's order: distance, then higher value, then
// lower key.
bool isBetterPlace(const BestPlace &place, const BestPlace &other) {
  if (place.Distance != other.Distance) {
    return place.Distance < other.Distance;
  }
  if (place.Value != other.Value) {
    return place.Value > other.Value;
  }
  return place.Key < other.Key;
}

// Whether one way to the same seed comes before another: by key, then the shorter, then arrival,
// then isPreferredStep.
bool isEarlierWay(const BestPlace &way, const BestPlace &other) {
  if (way.Key != other.Key) {
    return way.Key < other.Key;
  }
  if (way.Distance != other.Distance) {
    return way.Distance < other.Distance;
  }
  const bool isArrived = way.Next.Carrier == NULL_KEY;
  const bool isOtherArrived = other.Next.Carrier == NULL_KEY;
  if (isArrived != isOtherArrived) {
    return isArrived;
  }
  return isPreferredStep(way.Next, other.Next);
}

// Appends to out each way the sample offers to a seed: along the edge to a seed on it, and
// through either end to the seeds found there.
void appendWays(const EdgeSample<BestPlace> &sample, std::vector<BestPlace> &out) {
  const NetworkEdge &edge = sample.Edge;
  for (const EdgeEntry<BestPlace> &along : sample.Along) {
    const double distance = std::abs(along.FromOffset - sample.FromOffset);
    RouteStep next;
    if (distance != 0.0) {
      const double behind =
          along.FromOffset > sample.FromOffset ? edge.FromDistance : edge.ToDistance;
      next = {edge.Carrier, behind, along.Value.At.Distance};
    }
    out.push_back(foundAs({along.Value.Key, along.Value.Owner, along.Value.At, along.Value.Value},
                          distance, next));
  }
  for (const BestPlace &atFrom : sample.AtFrom) {
    out.push_back(foundAs({atFrom.Key, atFrom.Owner, atFrom.At, atFrom.Value},
                          sample.FromOffset + atFrom.Distance,
                          {edge.Carrier, edge.ToDistance, edge.FromDistance}));
  }
  for (const BestPlace &atTo : sample.AtTo) {
    out.push_back(foundAs({atTo.Key, atTo.Owner, atTo.At, atTo.Value},
                          sample.ToOffset + atTo.Distance,
                          {edge.Carrier, edge.FromDistance, edge.ToDistance}));
  }
}

} // namespace

std::vector<PlacedEntry<BestPlace>> bestPlaces(const Network &network,
                                               std::span<const PlaceSeed> seeds, uint32_t keep) {
  const std::vector<ResolvedSeed> resolved = resolveSeeds(network, seeds);
  const std::vector<std::vector<Kept>> kept = searchSeeds(network, resolved, keep);
  std::vector<PlacedEntry<BestPlace>> entries;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const Place at = network.nodePlace(node);
    for (const Kept &label : kept[node]) {
      const ResolvedSeed &seed = resolved[label.Seed];
      const RouteStep next = nextStep(network, kept, seed, label.Seed, node, label.Distance);
      entries.push_back({at, foundAs(seed.Seed, label.Distance, next)});
    }
  }
  for (const ResolvedSeed &seed : resolved) {
    if (std::holds_alternative<EdgePosition>(seed.Position)) {
      entries.push_back({seed.Seed.At, foundAs(seed.Seed, 0.0, RouteStep{})});
    }
  }
  return entries;
}

void sampleBestPlaceEdge(const EdgeSample<BestPlace> &sample, uint32_t keep,
                         std::vector<BestPlace> &out) {
  const auto start = static_cast<ptrdiff_t>(out.size());
  appendWays(sample, out);
  // Each seed's least way, then the seeds in the sample's order. std::sort allocates nothing.
  std::sort(out.begin() + start, out.end(), isEarlierWay);
  const auto repeats =
      std::unique(out.begin() + start, out.end(), [](const BestPlace &way, const BestPlace &other) {
        return way.Key == other.Key;
      });
  out.erase(repeats, out.end());
  std::sort(out.begin() + start, out.end(), isBetterPlace);
  // Each seed moves forward while fewer than keep seeds already moved forward have at least its
  // value.
  auto write = out.begin() + start;
  for (auto read = write; read != out.end(); ++read) {
    const auto atLeast = std::count_if(out.begin() + start, write, [&](const BestPlace &other) {
      return other.Value >= read->Value;
    });
    if (std::cmp_less(atLeast, keep)) {
      *write = *read;
      ++write;
    }
  }
  out.erase(write, out.end());
}

} // namespace tpj
