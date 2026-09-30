#include "support/synthetic_network.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using test::distanceToCarrier;
using test::groundDistance;
using test::makeSyntheticNetwork;
using test::SyntheticNetwork;
using test::SyntheticRandom;

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITE = std::numeric_limits<double>::infinity();

constexpr EntityKey BEND{10};
constexpr EntityKey LOOP{15};
constexpr EntityKey SPUR{20};
constexpr EntityKey FIRST_OWNER{7};
constexpr EntityKey SECOND_OWNER{8};

// A small network with a junction, a carrier that stops twice at one node, stops between carrier
// points, and distances unlike ground lengths, since a producer measures along its own route.
//
// BEND runs (0,0) to (4,0) over distance 10, then to (4,3) over 2, stopping at nodes 0, 1, and 2 at
// distances 0, 5, and 12. LOOP leaves node 3 at (4,9), passes node 4 at distance 7, and returns to
// node 3 at distance 12. SPUR runs from node 2 at (4,3) to node 3 at (4,9) over distance 6.
// Carriers and anchors are given out of order.
SyntheticNetwork sampleInputs() {
  SyntheticNetwork inputs;
  inputs.Carriers = {
      Carrier{.Key = SPUR,
              .Points = {{.X = 4, .Z = 3, .Distance = 0}, {.X = 4, .Z = 9, .Distance = 6}},
              .Stops = {{.Distance = 0, .Node = 2}, {.Distance = 6, .Node = 3}}},
      Carrier{.Key = LOOP,
              .Points = {{.X = 4, .Z = 9, .Distance = 0},
                         {.X = 8, .Z = 9, .Distance = 4},
                         {.X = 8, .Z = 12, .Distance = 7},
                         {.X = 4, .Z = 9, .Distance = 12}},
              .Stops = {{.Distance = 0, .Node = 3},
                        {.Distance = 7, .Node = 4},
                        {.Distance = 12, .Node = 3}}},
      Carrier{.Key = BEND,
              .Points = {{.X = 0, .Z = 0, .Distance = 0},
                         {.X = 4, .Z = 0, .Distance = 10},
                         {.X = 4, .Z = 3, .Distance = 12}},
              .Stops = {{.Distance = 0, .Node = 0},
                        {.Distance = 5, .Node = 1},
                        {.Distance = 12, .Node = 2}}},
  };
  inputs.NodeCount = 5;
  inputs.Anchors = {{.Node = 3, .Entity = FIRST_OWNER},
                    {.Node = 0, .Entity = SECOND_OWNER},
                    {.Node = 1, .Entity = FIRST_OWNER}};
  return inputs;
}

Carrier &carrierOf(SyntheticNetwork &inputs, EntityKey key) {
  return *std::ranges::find_if(inputs.Carriers,
                               [key](const Carrier &carrier) { return carrier.Key == key; });
}

// A minimal network, a tree with junctions, and a larger one with parallel carriers.
struct RandomCase {
  uint64_t Seed = 0;
  uint32_t NodeCount = 0;
};

const std::vector<RandomCase> RANDOM_CASES = {
    {.Seed = 1, .NodeCount = 2}, {.Seed = 2, .NodeCount = 9}, {.Seed = 3, .NodeCount = 24}};

// Places on the sample network that lie on no carrier of it.
std::vector<Place> unresolvablePlaces() {
  return {
      {.Carrier = EntityKey{99}, .Distance = 1},
      {.Carrier = NULL_KEY, .Distance = 0},
      {.Carrier = BEND, .Distance = NOT_A_NUMBER},
      {.Carrier = BEND, .Distance = -1},
      {.Carrier = BEND, .Distance = std::nextafter(0.0, -1.0)},
      {.Carrier = BEND, .Distance = std::nextafter(12.0, INFINITE)},
      {.Carrier = BEND, .Distance = INFINITE},
      {.Carrier = BEND, .Distance = -INFINITE},
      // Within BEND's length, but beyond SPUR's.
      {.Carrier = SPUR, .Distance = 7},
  };
}

// The position as T, or null when the place resolves to nothing or to the other alternative.
template <typename T> const T *positionAs(const std::optional<NetworkPosition> &position) {
  return position.has_value() ? std::get_if<T>(&position.value()) : nullptr;
}

void requireNode(const Network &network, const Place &place, uint32_t node) {
  CAPTURE(place.Carrier, place.Distance, node);
  const std::optional<NetworkPosition> position = network.resolve(place);
  const auto *at = positionAs<NodePosition>(position);
  REQUIRE(at != nullptr);
  REQUIRE(at->Node == node);
}

void requireEdge(const Network &network, const Place &place, uint32_t edge, double fromOffset,
                 double toOffset) {
  CAPTURE(place.Carrier, place.Distance, edge);
  const std::optional<NetworkPosition> position = network.resolve(place);
  const auto *at = positionAs<EdgePosition>(position);
  REQUIRE(at != nullptr);
  REQUIRE(at->Edge == edge);
  REQUIRE(at->FromOffset == fromOffset);
  REQUIRE(at->ToOffset == toOffset);
  REQUIRE(at->FromOffset > 0.0);
  REQUIRE(at->ToOffset > 0.0);
}

void requireSameEdge(const NetworkEdge &actual, const NetworkEdge &expected) {
  REQUIRE(actual.Carrier == expected.Carrier);
  REQUIRE(actual.From == expected.From);
  REQUIRE(actual.To == expected.To);
  REQUIRE(actual.FromDistance == expected.FromDistance);
  REQUIRE(actual.ToDistance == expected.ToDistance);
}

std::shared_ptr<const WorldSchema> makeNetworkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addNetworkComponent(*schema);
  return schema;
}

constexpr uint64_t NETWORK_PURPOSE = hashName("network");

// A world holding each network on an entity derived from key 1, as a producer would.
World holdNetworks(std::shared_ptr<const WorldSchema> schema,
                   const std::vector<Network> &networks) {
  World world(std::move(schema), 77);
  const EntityKey owner = world.createEntity();
  for (size_t i = 0; i < networks.size(); ++i) {
    const EntityKey key = world.createDerivedEntity(owner, NETWORK_PURPOSE, i);
    world.Registry.emplace<Network>(world.findEntity(key), networks[i]);
  }
  return world;
}

// The distance from the point to the nearest point of any carrier's ground line, which is the
// bound the nearest place's ground point must meet.
double distanceToCarriers(const Network &network, GroundPoint point) {
  double nearest = INFINITE;
  for (const Carrier &carrier : network.carriers()) {
    nearest = std::min(nearest, distanceToCarrier(carrier, point));
  }
  return nearest;
}

TEST_CASE("the default network is empty") {
  const Network network;
  REQUIRE(network.carriers().empty());
  REQUIRE(network.edges().empty());
  REQUIRE(network.nodeCount() == 0);
}

TEST_CASE("the constructor accepts inputs the spec does not list as malformed") {
  SECTION("no carriers and no nodes") { REQUIRE_NOTHROW(Network({}, 0, {})); }
  SECTION("junctions, a carrier stopping twice at a node, stops between points, and an entity "
          "anchoring two nodes") {
    const SyntheticNetwork inputs = sampleInputs();
    REQUIRE_NOTHROW(inputs.build());
  }
  SECTION("negative coordinates, and a segment whose points share a ground position") {
    // The producer's distances need not follow the ground, so a segment may have length along its
    // route but none on the ground.
    const std::vector<Carrier> carriers = {
        Carrier{.Key = EntityKey{3},
                .Points = {{.X = -2, .Z = -5, .Distance = 0},
                           {.X = -2, .Z = -5, .Distance = 1},
                           {.X = -6, .Z = -5, .Distance = 5}},
                .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 5, .Node = 1}}}};
    REQUIRE_NOTHROW(Network(carriers, 2, {}));
  }
  SECTION("an edge from a node back to itself") {
    const std::vector<Carrier> carriers = {
        Carrier{.Key = EntityKey{3},
                .Points = {{.X = 0, .Z = 0, .Distance = 0},
                           {.X = 1, .Z = 1, .Distance = 2},
                           {.X = 0, .Z = 0, .Distance = 4}},
                .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 4, .Node = 0}}}};
    REQUIRE_NOTHROW(Network(carriers, 1, {}));
  }
  SECTION("random synthetic networks") {
    for (const RandomCase &random : RANDOM_CASES) {
      CAPTURE(random.Seed);
      REQUIRE_NOTHROW(makeSyntheticNetwork(random.Seed, random.NodeCount).build());
    }
  }
}

TEST_CASE("the constructor refuses each malformed input the spec lists") {
  const std::vector<std::pair<std::string, std::function<void(SyntheticNetwork &)>>> malformed = {
      {"a carrier keyed NULL_KEY",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Key = NULL_KEY; }},
      {"a repeated carrier key",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Key = BEND; }},
      {"a carrier with one point",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Points.pop_back(); }},
      {"a coordinate that is NaN",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].X = NOT_A_NUMBER; }},
      {"a coordinate that is infinite",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].Z = INFINITE; }},
      // NaN compares false, so an ordering test alone would let it through.
      {"a point distance that is NaN",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].Distance = NOT_A_NUMBER; }},
      // Infinity equals itself and exceeds every finite distance, so only finiteness refuses it.
      {"a last point and last stop at infinity",
       [](SyntheticNetwork &inputs) {
         carrierOf(inputs, BEND).Points[2].Distance = INFINITE;
         carrierOf(inputs, BEND).Stops[2].Distance = INFINITE;
       }},
      {"a first point whose distance is not 0",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[0].Distance = 0.5; }},
      {"point distances that repeat",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].Distance = 12; }},
      {"a carrier with one stop",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Stops.pop_back(); }},
      {"a stop distance that is NaN",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Stops[1].Distance = NOT_A_NUMBER; }},
      {"a first stop not at 0",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Stops[0].Distance = 1; }},
      {"a last stop before the carrier's length",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Stops[1].Distance = 5; }},
      {"a last stop beyond the carrier's length",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Stops[1].Distance = 7; }},
      {"stop distances that repeat",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Stops[1].Distance = 0; }},
      {"a stop naming the node count",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, SPUR).Stops[1].Node = 5; }},
      {"a node no carrier stops at", [](SyntheticNetwork &inputs) { inputs.NodeCount = 6; }},
      {"an anchor naming the node count",
       [](SyntheticNetwork &inputs) {
         inputs.Anchors.push_back({.Node = 5, .Entity = SECOND_OWNER});
       }},
      {"an anchor naming NULL_KEY",
       [](SyntheticNetwork &inputs) { inputs.Anchors.push_back({.Node = 2, .Entity = NULL_KEY}); }},
      {"a second anchor on a node",
       [](SyntheticNetwork &inputs) {
         inputs.Anchors.push_back({.Node = 3, .Entity = SECOND_OWNER});
       }},
      // Identical anchors must not be merged silently.
      {"a repeated anchor",
       [](SyntheticNetwork &inputs) {
         inputs.Anchors.push_back({.Node = 3, .Entity = FIRST_OWNER});
       }},
  };

  REQUIRE_NOTHROW(sampleInputs().build());
  for (const auto &[label, apply] : malformed) {
    CAPTURE(label);
    SyntheticNetwork inputs = sampleInputs();
    apply(inputs);
    CHECK_THROWS_AS(inputs.build(), std::invalid_argument);
  }
}

TEST_CASE("a network does not depend on the order its carriers and anchors are given in") {
  const auto schema = makeNetworkSchema();
  const std::vector<SyntheticNetwork> cases = {sampleInputs(), makeSyntheticNetwork(4, 12)};
  for (const SyntheticNetwork &given : cases) {
    SyntheticNetwork reordered = given;
    std::ranges::reverse(reordered.Carriers);
    std::ranges::reverse(reordered.Anchors);
    SyntheticNetwork rotated = given;
    std::ranges::rotate(rotated.Carriers, rotated.Carriers.begin() + 1);
    if (!rotated.Anchors.empty()) {
      std::ranges::rotate(rotated.Anchors, rotated.Anchors.begin() + 1);
    }

    const Network original = given.build();
    for (const SyntheticNetwork &other : {reordered, rotated}) {
      const Network network = other.build();
      const World left = holdNetworks(schema, {original});
      const World right = holdNetworks(schema, {network});
      REQUIRE(worldsEqual(left, right));
      REQUIRE(hashWorld(left) == hashWorld(right));

      REQUIRE(network.edges().size() == original.edges().size());
      for (size_t i = 0; i < network.edges().size(); ++i) {
        requireSameEdge(network.edges()[i], original.edges()[i]);
      }
      for (uint32_t node = 0; node < network.nodeCount(); ++node) {
        CAPTURE(node);
        REQUIRE(network.nodePlace(node) == original.nodePlace(node));
      }
    }
  }
}

TEST_CASE("edges() lists each carrier's stretches between consecutive stops, carriers in key "
          "order") {
  const Network network = sampleInputs().build();
  const std::vector<NetworkEdge> expected = {
      {.Carrier = BEND, .From = 0, .To = 1, .FromDistance = 0, .ToDistance = 5},
      {.Carrier = BEND, .From = 1, .To = 2, .FromDistance = 5, .ToDistance = 12},
      {.Carrier = LOOP, .From = 3, .To = 4, .FromDistance = 0, .ToDistance = 7},
      {.Carrier = LOOP, .From = 4, .To = 3, .FromDistance = 7, .ToDistance = 12},
      {.Carrier = SPUR, .From = 2, .To = 3, .FromDistance = 0, .ToDistance = 6},
  };
  REQUIRE(network.edges().size() == expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);
    requireSameEdge(network.edges()[i], expected[i]);
    REQUIRE(network.edges()[i].length() == expected[i].ToDistance - expected[i].FromDistance);
  }
}

TEST_CASE("resolve gives the node at a stop's distance, and between stops the enclosing edge with "
          "its offsets") {
  const Network network = sampleInputs().build();

  SECTION("at a stop") {
    requireNode(network, {.Carrier = BEND, .Distance = 0}, 0);
    requireNode(network, {.Carrier = BEND, .Distance = 5}, 1);
    requireNode(network, {.Carrier = BEND, .Distance = 12}, 2);
    requireNode(network, {.Carrier = SPUR, .Distance = 0}, 2);
    requireNode(network, {.Carrier = SPUR, .Distance = 6}, 3);
    requireNode(network, {.Carrier = LOOP, .Distance = 0}, 3);
    requireNode(network, {.Carrier = LOOP, .Distance = 7}, 4);
    requireNode(network, {.Carrier = LOOP, .Distance = 12}, 3);
  }
  SECTION("between stops") {
    requireEdge(network, {.Carrier = BEND, .Distance = 2}, 0, 2, 3);
    // Past a carrier point that is not a stop.
    requireEdge(network, {.Carrier = BEND, .Distance = 11}, 1, 6, 1);
    requireEdge(network, {.Carrier = LOOP, .Distance = 9}, 3, 2, 3);
    requireEdge(network, {.Carrier = SPUR, .Distance = 1.5}, 4, 1.5, 4.5);
  }
}

TEST_CASE("resolve gives no position for a missing carrier or a distance outside its carrier") {
  const Network network = sampleInputs().build();
  for (const Place &place : unresolvablePlaces()) {
    CAPTURE(place.Carrier, place.Distance);
    CHECK_FALSE(network.resolve(place).has_value());
  }
  CHECK_FALSE(Network().resolve({.Carrier = BEND, .Distance = 0}).has_value());
}

TEST_CASE("on random synthetic networks every stop resolves to its node and every place strictly "
          "inside an edge to that edge") {
  for (const RandomCase &random : RANDOM_CASES) {
    CAPTURE(random.Seed);
    const SyntheticNetwork inputs = makeSyntheticNetwork(random.Seed, random.NodeCount);
    const Network network = inputs.build();

    for (const Carrier &carrier : inputs.Carriers) {
      for (const CarrierStop &stop : carrier.Stops) {
        requireNode(network, {.Carrier = carrier.Key, .Distance = stop.Distance}, stop.Node);
      }
    }

    REQUIRE(network.edges().size() == inputs.Carriers.size());
    for (uint32_t i = 0; i < network.edges().size(); ++i) {
      const NetworkEdge &edge = network.edges()[i];
      // The middle, and the nearest distances to each end that are strictly inside.
      for (const double distance :
           {(edge.FromDistance + edge.ToDistance) / 2.0,
            std::nextafter(edge.FromDistance, INFINITE), std::nextafter(edge.ToDistance, 0.0)}) {
        requireEdge(network, {.Carrier = edge.Carrier, .Distance = distance}, i,
                    distance - edge.FromDistance, edge.ToDistance - distance);
      }
    }
  }
}

TEST_CASE("nodePlace gives a node's stop on its lowest-keyed carrier at the lowest distance there, "
          "which resolves to the node") {
  const Network network = sampleInputs().build();
  // Node 2 is at BEND's 12 and SPUR's 0: the key decides, not the distance. Node 3 is at LOOP's 0
  // and 12 and SPUR's 6.
  const std::vector<Place> expected = {{.Carrier = BEND, .Distance = 0},
                                       {.Carrier = BEND, .Distance = 5},
                                       {.Carrier = BEND, .Distance = 12},
                                       {.Carrier = LOOP, .Distance = 0},
                                       {.Carrier = LOOP, .Distance = 7}};
  REQUIRE(network.nodeCount() == expected.size());
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    CAPTURE(node);
    REQUIRE(network.nodePlace(node) == expected[node]);
    requireNode(network, network.nodePlace(node), node);
  }

  for (const RandomCase &random : RANDOM_CASES) {
    CAPTURE(random.Seed);
    const Network synthetic = makeSyntheticNetwork(random.Seed, random.NodeCount).build();
    for (uint32_t node = 0; node < synthetic.nodeCount(); ++node) {
      requireNode(synthetic, synthetic.nodePlace(node), node);
    }
  }
}

TEST_CASE("nodeAnchor and anchoredNodes find the anchors given, and nothing where none was given") {
  const Network network = sampleInputs().build();
  REQUIRE(network.nodeAnchor(0) == SECOND_OWNER);
  REQUIRE(network.nodeAnchor(1) == FIRST_OWNER);
  REQUIRE(network.nodeAnchor(2) == NULL_KEY);
  REQUIRE(network.nodeAnchor(3) == FIRST_OWNER);
  REQUIRE(network.nodeAnchor(4) == NULL_KEY);

  // Given as node 3 before node 1.
  REQUIRE(network.anchoredNodes(FIRST_OWNER) == std::vector<uint32_t>{1, 3});
  REQUIRE(network.anchoredNodes(SECOND_OWNER) == std::vector<uint32_t>{0});
  REQUIRE(network.anchoredNodes(EntityKey{9}).empty());
  REQUIRE(network.anchoredNodes(NULL_KEY).empty());
  REQUIRE(Network().anchoredNodes(FIRST_OWNER).empty());
}

TEST_CASE("nodePlace and nodeAnchor refuse a node not below the node count") {
  const Network network = sampleInputs().build();
  REQUIRE_THROWS_AS(network.nodePlace(5), std::out_of_range);
  REQUIRE_THROWS_AS(network.nodeAnchor(5), std::out_of_range);
  REQUIRE_THROWS_AS(Network().nodePlace(0), std::out_of_range);
  REQUIRE_THROWS_AS(Network().nodeAnchor(0), std::out_of_range);
}

bool placeBefore(const Place &left, const Place &right) {
  return left.Carrier < right.Carrier ||
         (left.Carrier == right.Carrier && left.Distance < right.Distance);
}

std::vector<Place> stopPlacesOf(const Network &network, uint32_t node) {
  const std::span<const Place> places = network.stopPlaces(node);
  return {places.begin(), places.end()};
}

TEST_CASE("stopPlaces lists a node's stop places in ascending carrier key and then distance, with "
          "no repeats") {
  const Network network = sampleInputs().build();
  // Node 2 is at BEND's 12 and SPUR's 0, so the key orders them, not the distance. LOOP stops at
  // node 3 twice.
  const std::vector<std::vector<Place>> expected = {
      {{.Carrier = BEND, .Distance = 0}},
      {{.Carrier = BEND, .Distance = 5}},
      {{.Carrier = BEND, .Distance = 12}, {.Carrier = SPUR, .Distance = 0}},
      {{.Carrier = LOOP, .Distance = 0},
       {.Carrier = LOOP, .Distance = 12},
       {.Carrier = SPUR, .Distance = 6}},
      {{.Carrier = LOOP, .Distance = 7}}};
  REQUIRE(network.nodeCount() == expected.size());
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    CAPTURE(node);
    REQUIRE(stopPlacesOf(network, node) == expected[node]);
  }

  for (const RandomCase &random : RANDOM_CASES) {
    CAPTURE(random.Seed);
    const Network synthetic = makeSyntheticNetwork(random.Seed, random.NodeCount).build();
    for (uint32_t node = 0; node < synthetic.nodeCount(); ++node) {
      CAPTURE(node);
      const std::vector<Place> places = stopPlacesOf(synthetic, node);
      for (size_t i = 1; i < places.size(); ++i) {
        REQUIRE(placeBefore(places[i - 1], places[i]));
      }
    }
  }
}

TEST_CASE("a place is among a node's stop places exactly when resolve gives that node for it") {
  std::vector<SyntheticNetwork> cases = {sampleInputs()};
  for (const RandomCase &random : RANDOM_CASES) {
    cases.push_back(makeSyntheticNetwork(random.Seed, random.NodeCount));
  }
  for (size_t c = 0; c < cases.size(); ++c) {
    CAPTURE(c);
    const SyntheticNetwork &inputs = cases[c];
    const Network network = inputs.build();

    // Every stop place resolves to its node.
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      for (const Place &place : network.stopPlaces(node)) {
        requireNode(network, place, node);
      }
    }
    // resolve gives a node only at a stop's distance, so every stop given is among its node's.
    for (const Carrier &carrier : inputs.Carriers) {
      for (const CarrierStop &stop : carrier.Stops) {
        const Place place{.Carrier = carrier.Key, .Distance = stop.Distance};
        CAPTURE(place.Carrier, place.Distance, stop.Node);
        const std::vector<Place> places = stopPlacesOf(network, stop.Node);
        REQUIRE(std::ranges::find(places, place) != places.end());
      }
    }
  }
}

TEST_CASE("nodePlace is the first of a node's stop places") {
  std::vector<Network> networks = {sampleInputs().build()};
  for (const RandomCase &random : RANDOM_CASES) {
    networks.push_back(makeSyntheticNetwork(random.Seed, random.NodeCount).build());
  }
  for (size_t c = 0; c < networks.size(); ++c) {
    CAPTURE(c);
    const Network &network = networks[c];
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      CAPTURE(node);
      const std::span<const Place> places = network.stopPlaces(node);
      REQUIRE_FALSE(places.empty());
      REQUIRE(network.nodePlace(node) == places.front());
    }
  }
}

TEST_CASE("stopPlaces refuses a node not below the node count") {
  const Network network = sampleInputs().build();
  REQUIRE_THROWS_AS(network.stopPlaces(5), std::out_of_range);
  const Network empty;
  REQUIRE_THROWS_AS(empty.stopPlaces(0), std::out_of_range);
}

TEST_CASE("groundPoint gives a carrier point's coordinates at its distance, and between points "
          "interpolates by distance") {
  const Network network = sampleInputs().build();
  auto requireGround = [&](Place place, double x, double z) {
    CAPTURE(place.Carrier, place.Distance);
    const std::optional<GroundPoint> ground = network.groundPoint(place);
    REQUIRE(ground.has_value());
    const GroundPoint at = ground.value_or(GroundPoint{.X = NOT_A_NUMBER, .Z = NOT_A_NUMBER});
    REQUIRE(at.X == x);
    REQUIRE(at.Z == z);
  };

  SECTION("at a point") {
    requireGround({.Carrier = BEND, .Distance = 0}, 0, 0);
    requireGround({.Carrier = BEND, .Distance = 10}, 4, 0);
    requireGround({.Carrier = BEND, .Distance = 12}, 4, 3);
    requireGround({.Carrier = LOOP, .Distance = 4}, 8, 9);
    requireGround({.Carrier = LOOP, .Distance = 7}, 8, 12);
  }
  SECTION("between points") {
    // BEND's segments cover 10 and 2 of distance for 4 and 3 of ground, so interpolating along
    // the whole ground line instead would put these elsewhere.
    requireGround({.Carrier = BEND, .Distance = 7.5}, 3, 0);
    requireGround({.Carrier = BEND, .Distance = 11}, 4, 1.5);
    // At a stop that is not a point.
    requireGround({.Carrier = BEND, .Distance = 5}, 2, 0);
    requireGround({.Carrier = LOOP, .Distance = 5.5}, 8, 10.5);
    requireGround({.Carrier = SPUR, .Distance = 1.5}, 4, 4.5);
  }
}

TEST_CASE("groundPoint gives no position where resolve gives none") {
  const Network network = sampleInputs().build();
  for (const Place &place : unresolvablePlaces()) {
    CAPTURE(place.Carrier, place.Distance);
    CHECK_FALSE(network.groundPoint(place).has_value());
  }
}

TEST_CASE("nearestPlace gives nothing for an empty network or a ground position that is not "
          "finite") {
  CHECK_FALSE(Network().nearestPlace({.X = 0, .Z = 0}).has_value());
  CHECK_FALSE(Network({}, 0, {}).nearestPlace({.X = 1, .Z = 1}).has_value());

  const Network network = sampleInputs().build();
  CHECK_FALSE(network.nearestPlace({.X = NOT_A_NUMBER, .Z = 0}).has_value());
  CHECK_FALSE(network.nearestPlace({.X = 0, .Z = INFINITE}).has_value());
  CHECK_FALSE(network.nearestPlace({.X = -INFINITE, .Z = 3}).has_value());
}

TEST_CASE("nearestPlace gives a place that resolves, and no carrier point is nearer") {
  auto requireNearest = [](const Network &network, GroundPoint point) {
    CAPTURE(point.X, point.Z);
    const std::optional<Place> nearest = network.nearestPlace(point);
    REQUIRE(nearest.has_value());
    const Place place = nearest.value_or(Place{});
    CAPTURE(place.Carrier, place.Distance);
    REQUIRE(network.resolve(place).has_value());
    const std::optional<GroundPoint> ground = network.groundPoint(place);
    REQUIRE(ground.has_value());
    const GroundPoint at = ground.value_or(GroundPoint{.X = NOT_A_NUMBER, .Z = NOT_A_NUMBER});
    REQUIRE(groundDistance(point, at) <= distanceToCarriers(network, point) + 1e-9);
  };

  SECTION("on the sample network, whose distances are not ground lengths") {
    const Network network = sampleInputs().build();
    // Beside BEND's first segment, near its corner, before its start, beside LOOP, and on SPUR.
    for (const GroundPoint point : std::vector<GroundPoint>{{.X = 2, .Z = -1},
                                                            {.X = 5, .Z = -0.5},
                                                            {.X = -3, .Z = -4},
                                                            {.X = 9, .Z = 10},
                                                            {.X = 4, .Z = 5}}) {
      requireNearest(network, point);
    }
  }
  SECTION("on random synthetic networks") {
    for (const RandomCase &random : RANDOM_CASES) {
      CAPTURE(random.Seed);
      const Network network = makeSyntheticNetwork(random.Seed, random.NodeCount).build();
      SyntheticRandom queries(random.Seed + 1000);
      // Within the nodes' square, and far outside it.
      for (int i = 0; i < 4; ++i) {
        const double x = queries.between(-60.0, 60.0);
        const double z = queries.between(-60.0, 60.0);
        requireNearest(network, {.X = x, .Z = z});
      }
      requireNearest(network, {.X = 500, .Z = -300});
    }
  }
}

TEST_CASE("nearestPlace breaks ties by lower carrier key, then lower distance") {
  SECTION("a junction is equally near on each carrier that stops there") {
    // Key 7 leaves the junction at distance 0 and key 3 arrives there at distance 10, so the key
    // rule and a distance-first rule disagree.
    const std::vector<Carrier> carriers = {
        Carrier{.Key = EntityKey{7},
                .Points = {{.X = 0, .Z = 0, .Distance = 0}, {.X = 10, .Z = 0, .Distance = 10}},
                .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 10, .Node = 1}}},
        Carrier{.Key = EntityKey{3},
                .Points = {{.X = 0, .Z = 10, .Distance = 0}, {.X = 0, .Z = 0, .Distance = 10}},
                .Stops = {{.Distance = 0, .Node = 2}, {.Distance = 10, .Node = 0}}}};
    const Network network(carriers, 3, {});
    REQUIRE(network.nearestPlace({.X = -1, .Z = -1}) ==
            Place{.Carrier = EntityKey{3}, .Distance = 10});
  }
  SECTION("one carrier passing equally near twice") {
    // A U whose sides pass one unit either side of the point, at distances 5 and 17.
    const std::vector<Carrier> carriers = {
        Carrier{.Key = EntityKey{4},
                .Points = {{.X = 0, .Z = 0, .Distance = 0},
                           {.X = 10, .Z = 0, .Distance = 10},
                           {.X = 10, .Z = 2, .Distance = 12},
                           {.X = 0, .Z = 2, .Distance = 22}},
                .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 22, .Node = 1}}}};
    const Network network(carriers, 2, {});
    REQUIRE(network.nearestPlace({.X = 5, .Z = 1}) ==
            Place{.Carrier = EntityKey{4}, .Distance = 5});
  }
}

TEST_CASE("nearestPlace projects onto a segment whose points share a ground position at its first "
          "point") {
  // The carrier's only segment has no ground length, so projecting onto it divides zero by zero
  // unless its first point is taken.
  const std::vector<Carrier> carriers = {
      Carrier{.Key = EntityKey{6},
              .Points = {{.X = 3, .Z = 3, .Distance = 0}, {.X = 3, .Z = 3, .Distance = 2}},
              .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 2, .Node = 1}}}};
  const Network network(carriers, 2, {});
  REQUIRE(network.nearestPlace({.X = 5, .Z = 7}) == Place{.Carrier = EntityKey{6}, .Distance = 0});
}

TEST_CASE("addNetworkComponent registers Network as the derived type network") {
  WorldSchema schema;
  addNetworkComponent(schema);
  REQUIRE(schema.components().size() == 1);
  const ComponentType &type = schema.components()[0];
  REQUIRE(type.Name == "network");
  REQUIRE(type.Kind == DataKind::Derived);
  REQUIRE(type.TypeId == entt::type_id<Network>().hash());
}

TEST_CASE("makeParkSchema registers the network type first, as derived") {
  const std::shared_ptr<const WorldSchema> schema = makeParkSchema();
  REQUIRE_FALSE(schema->components().empty());
  const ComponentType &first = schema->components()[0];
  REQUIRE(first.Name == "network");
  REQUIRE(first.Kind == DataKind::Derived);
  REQUIRE(first.TypeId == entt::type_id<Network>().hash());
}

TEST_CASE("a world holding networks copies equal and hashes equal") {
  const World world =
      holdNetworks(makeNetworkSchema(),
                   {sampleInputs().build(), makeSyntheticNetwork(5, 10).build(), Network()});
  const World copy = copyWorld(world);
  REQUIRE(worldsEqual(copy, world));
  REQUIRE(worldsEqual(world, copy));
  REQUIRE(hashWorld(copy) == hashWorld(world));
}

TEST_CASE("changing a carrier point, stop, or anchor of a held network changes the world's hash") {
  const std::vector<std::pair<std::string, std::function<void(SyntheticNetwork &)>>> changes = {
      {"a point's x", [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].X = 5; }},
      {"a point's z", [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].Z = 1; }},
      {"a point's distance",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Points[1].Distance = 9; }},
      {"a stop's distance",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, BEND).Stops[1].Distance = 6; }},
      {"a stop's node",
       [](SyntheticNetwork &inputs) { carrierOf(inputs, LOOP).Stops[2].Node = 2; }},
      {"an anchor's entity",
       [](SyntheticNetwork &inputs) { inputs.Anchors[1].Entity = EntityKey{9}; }},
      {"an anchor's node", [](SyntheticNetwork &inputs) { inputs.Anchors[1].Node = 4; }},
  };

  const auto schema = makeNetworkSchema();
  const World reference = holdNetworks(schema, {sampleInputs().build()});
  const uint64_t referenceHash = hashWorld(reference);
  for (const auto &[label, apply] : changes) {
    CAPTURE(label);
    SyntheticNetwork inputs = sampleInputs();
    apply(inputs);
    const World changed = holdNetworks(schema, {inputs.build()});
    CHECK_FALSE(worldsEqual(changed, reference));
    CHECK(hashWorld(changed) != referenceHash);
  }
}

// A holder keeping a place in its own state, as guests will.
struct Holder {
  Place Where;
};

template <typename Visitor> void visitFields(Visitor &visitor, Holder &holder) {
  visitor.field("where", holder.Where);
}

std::shared_ptr<const WorldSchema> makeHolderSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addNetworkComponent(*schema);
  schema->addComponent<Holder>("holder", DataKind::State);
  return schema;
}

TEST_CASE("a save holds no network") {
  World world = holdNetworks(makeHolderSchema(), {sampleInputs().build()});
  world.Registry.emplace<Holder>(world.findEntity(EntityKey{1}),
                                 Holder{.Where = {.Carrier = BEND, .Distance = 2}});
  const std::string text = saveWorld(world);
  REQUIRE(text.find("[network]") == std::string::npos);

  const EntityKey held = deriveKey(EntityKey{1}, NETWORK_PURPOSE, 0);
  world.Registry.remove<Network>(world.findEntity(held));
  REQUIRE(saveWorld(world) == text);
}

TEST_CASE("a place in a registered state component saves and loads back equal") {
  const auto schema = makeHolderSchema();
  World world(schema, 3);
  // A derived carrier key uses the top bit, and 0.1 has no short binary form.
  const Place far{.Carrier = deriveKey(EntityKey{2}, hashName("path"), 1), .Distance = 0.1};
  const Place near{.Carrier = EntityKey{2}, .Distance = 1e-300};
  const EntityKey first = world.createEntity();
  const EntityKey second = world.createEntity();
  world.Registry.emplace<Holder>(world.findEntity(first), Holder{.Where = far});
  world.Registry.emplace<Holder>(world.findEntity(second), Holder{.Where = near});

  const World loaded = loadWorld(schema, saveWorld(world));
  REQUIRE(worldsEqual(loaded, world));
  REQUIRE(loaded.Registry.get<Holder>(loaded.findEntity(first)).Where == far);
  REQUIRE(loaded.Registry.get<Holder>(loaded.findEntity(second)).Where == near);
}

} // namespace
} // namespace tpj
