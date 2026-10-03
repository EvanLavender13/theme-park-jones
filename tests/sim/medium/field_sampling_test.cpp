#include "support/synthetic_fields.h"
#include "support/synthetic_network.h"

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using test::addSource;
using test::Emits;
using test::Footfall;
using test::makeFieldSchema;
using test::makeFieldWorld;
using test::makeSyntheticNetwork;
using test::Reach;
using test::ReachCall;
using test::reachCalls;
using test::SyntheticNetwork;

using Sampled = std::vector<std::pair<EntityKey, double>>;

constexpr double INFINITE = std::numeric_limits<double>::infinity();
constexpr EntityKey ABSENT_CARRIER{99};

// A minimal network, a tree with junctions, and a larger one with parallel carriers.
struct RandomCase {
  uint64_t Seed = 0;
  uint32_t NodeCount = 0;
};

const std::vector<RandomCase> RANDOM_CASES = {
    {.Seed = 1, .NodeCount = 2}, {.Seed = 2, .NodeCount = 9}, {.Seed = 3, .NodeCount = 24}};

// An entry and where the fixture put it: at a stop of Node, strictly inside the one edge of the
// carrier Inside, or, with neither, at a place that resolves nowhere.
struct Published {
  PlacedEntry<double> Entry;
  std::optional<uint32_t> Node;
  std::optional<EntityKey> Inside;
};

// Entries at every stop of every carrier, strictly inside every edge at its middle and at the
// nearest distances to each end, and at places that resolve nowhere, each with its own value. Each
// synthetic carrier is one edge. Carriers come in the order they were generated, not key order,
// so the source's order is not the network's.
std::vector<Published> publishedOn(const SyntheticNetwork &inputs) {
  std::vector<Published> published;
  auto add = [&](Place at, std::optional<uint32_t> node, std::optional<EntityKey> inside) {
    const double value = static_cast<double>(published.size() + 1);
    published.push_back({.Entry = {.At = at, .Value = value}, .Node = node, .Inside = inside});
  };
  for (const Carrier &carrier : inputs.Carriers) {
    const double length = carrier.Stops.back().Distance;
    for (const CarrierStop &stop : carrier.Stops) {
      add({.Carrier = carrier.Key, .Distance = stop.Distance}, stop.Node, std::nullopt);
    }
    for (const double distance :
         {std::nextafter(0.0, INFINITE), length / 2.0, std::nextafter(length, 0.0)}) {
      add({.Carrier = carrier.Key, .Distance = distance}, std::nullopt, carrier.Key);
    }
  }
  const Carrier &first = inputs.Carriers.front();
  const double length = first.Stops.back().Distance;
  // -0.0 equals 0.0 as a number, so it is exactly the first stop's distance.
  add({.Carrier = first.Key, .Distance = -0.0}, first.Stops.front().Node, std::nullopt);
  // Just outside each end of a carrier, and at a stop's distance on carriers the network lacks.
  add({.Carrier = first.Key, .Distance = -std::numeric_limits<double>::denorm_min()}, std::nullopt,
      std::nullopt);
  add({.Carrier = first.Key, .Distance = std::nextafter(length, INFINITE)}, std::nullopt,
      std::nullopt);
  add({.Carrier = ABSENT_CARRIER, .Distance = 0.0}, std::nullopt, std::nullopt);
  add({.Carrier = NULL_KEY, .Distance = 0.0}, std::nullopt, std::nullopt);
  return published;
}

// The second source's entries: the first's in reverse order, each value raised by 1000.
std::vector<Published> reversedAndRaised(std::vector<Published> published) {
  std::ranges::reverse(published);
  for (Published &entry : published) {
    entry.Entry.Value += 1000.0;
  }
  return published;
}

std::vector<PlacedEntry<double>> entriesOf(const std::vector<Published> &published) {
  std::vector<PlacedEntry<double>> entries;
  entries.reserve(published.size());
  for (const Published &entry : published) {
    entries.push_back(entry.Entry);
  }
  return entries;
}

// Two sources publishing the same places into both footfall and reach, the lower-keyed in the
// order publishedOn gives them and the higher-keyed in reverse. The world's own network is the
// standard layout's, so every sample here is taken on the synthetic network it is given.
struct RandomFieldCase {
  SyntheticNetwork Inputs;
  Network Built;
  std::vector<Published> FirstEntries;
  std::vector<Published> SecondEntries;
  World Resolved;
  EntityKey First = NULL_KEY;
  EntityKey Second = NULL_KEY;
};

EntityKey addBothSource(World &world, const std::vector<Published> &published) {
  const std::vector<PlacedEntry<double>> entries = entriesOf(published);
  const EntityKey key = addSource<Footfall>(world, entries);
  world.Registry.emplace<Emits<Reach>>(world.findEntity(key), Emits<Reach>{.Entries = entries});
  return key;
}

RandomFieldCase randomFieldCase(const RandomCase &random) {
  SyntheticNetwork inputs = makeSyntheticNetwork(random.Seed, random.NodeCount);
  Network built = inputs.build();
  std::vector<Published> firstEntries = publishedOn(inputs);
  std::vector<Published> secondEntries = reversedAndRaised(firstEntries);
  RandomFieldCase result{.Inputs = std::move(inputs),
                         .Built = std::move(built),
                         .FirstEntries = std::move(firstEntries),
                         .SecondEntries = std::move(secondEntries),
                         .Resolved = makeFieldWorld(makeFieldSchema())};
  result.First = addBothSource(result.Resolved, result.FirstEntries);
  result.Second = addBothSource(result.Resolved, result.SecondEntries);
  REQUIRE(result.First < result.Second);
  resolveWorld(result.Resolved);
  return result;
}

template <typename F>
Sampled sampledOn(const World &world, const Network &network, const Place &at) {
  Sampled result;
  for (const SampledEntry<double> &entry : sampleField<F>(world, network, at)) {
    result.emplace_back(entry.Source, entry.Value);
  }
  return result;
}

std::vector<uint64_t> bitsOf(const std::vector<SampledEntry<double>> &entries) {
  std::vector<uint64_t> bits;
  for (const SampledEntry<double> &entry : entries) {
    bits.push_back(static_cast<uint64_t>(entry.Source));
    bits.push_back(std::bit_cast<uint64_t>(entry.Value));
  }
  return bits;
}

// Everything sampleEdge was given in each call, as bits, so two runs compare exactly.
std::vector<uint64_t> callBits(const std::vector<ReachCall> &calls) {
  std::vector<uint64_t> bits;
  for (const ReachCall &call : calls) {
    const EdgeSample<double> &sample = call.Sample;
    bits.push_back(static_cast<uint64_t>(sample.Edge.Carrier));
    bits.push_back(sample.Edge.From);
    bits.push_back(sample.Edge.To);
    bits.push_back(std::bit_cast<uint64_t>(sample.FromOffset));
    bits.push_back(std::bit_cast<uint64_t>(sample.ToOffset));
    bits.push_back(sample.AtFrom.size());
    for (const double value : sample.AtFrom) {
      bits.push_back(std::bit_cast<uint64_t>(value));
    }
    bits.push_back(sample.AtTo.size());
    for (const double value : sample.AtTo) {
      bits.push_back(std::bit_cast<uint64_t>(value));
    }
    bits.push_back(sample.Along.size());
    for (const EdgeEntry<double> &entry : sample.Along) {
      bits.push_back(std::bit_cast<uint64_t>(entry.FromOffset));
      bits.push_back(std::bit_cast<uint64_t>(entry.ToOffset));
      bits.push_back(std::bit_cast<uint64_t>(entry.Value));
    }
  }
  return bits;
}

// Every place the fixture sampled at: each stop, each entry's place inside an edge, and a place
// inside each edge that no entry names.
std::vector<Place> samplePlaces(const RandomFieldCase &fieldCase) {
  std::vector<Place> places;
  for (const Carrier &carrier : fieldCase.Inputs.Carriers) {
    for (const CarrierStop &stop : carrier.Stops) {
      places.push_back({.Carrier = carrier.Key, .Distance = stop.Distance});
    }
  }
  for (const Published &entry : fieldCase.FirstEntries) {
    if (entry.Inside.has_value()) {
      places.push_back(entry.Entry.At);
    }
  }
  for (const NetworkEdge &edge : fieldCase.Built.edges()) {
    places.push_back(
        {.Carrier = edge.Carrier, .Distance = edge.FromDistance + (edge.length() / 4.0)});
  }
  return places;
}

// Distance is measured along routes, so where a carrier lies on the ground cannot change a sample.
TEST_CASE("sampling depends on carriers' distances and stops, not their ground positions") {
  for (const RandomCase &random : RANDOM_CASES) {
    CAPTURE(random.Seed);
    const RandomFieldCase fieldCase = randomFieldCase(random);
    // Every point moved, turned, and stretched unevenly, so no ground length is kept.
    SyntheticNetwork moved = fieldCase.Inputs;
    for (Carrier &carrier : moved.Carriers) {
      for (CarrierPoint &point : carrier.Points) {
        const double x = point.X;
        point.X = (3.0 * point.Z) + 700.0;
        point.Z = (-0.5 * x) - 40.0;
      }
    }
    const Network movedNetwork = moved.build();
    const World &world = fieldCase.Resolved;

    for (const Place &at : samplePlaces(fieldCase)) {
      CAPTURE(at.Carrier, at.Distance);
      REQUIRE(bitsOf(sampleField<Footfall>(world, movedNetwork, at)) ==
              bitsOf(sampleField<Footfall>(world, fieldCase.Built, at)));

      reachCalls().clear();
      const std::vector<uint64_t> original = bitsOf(sampleField<Reach>(world, fieldCase.Built, at));
      const std::vector<uint64_t> originalCalls = callBits(reachCalls());
      reachCalls().clear();
      REQUIRE(bitsOf(sampleField<Reach>(world, movedNetwork, at)) == original);
      REQUIRE(callBits(reachCalls()) == originalCalls);
    }
  }
}

} // namespace
} // namespace tpj
