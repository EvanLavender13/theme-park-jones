#include "support/synthetic_network.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::distanceToCarrier;
using test::groundDistance;
using test::makeSyntheticNetwork;
using test::SyntheticRandom;

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();

constexpr GroundPoint NOWHERE{.X = NOT_A_NUMBER, .Z = NOT_A_NUMBER};

// A straight carrier from one ground position to another, whose distance runs to length, with
// stops only at its two ends.
Carrier straight(EntityKey key, GroundPoint from, GroundPoint to, double length, uint32_t fromNode,
                 uint32_t toNode) {
  return Carrier{
      .Key = key,
      .Points = {{.X = from.X, .Z = from.Z, .Distance = 0},
                 {.X = to.X, .Z = to.Z, .Distance = length}},
      .Stops = {{.Distance = 0, .Node = fromNode}, {.Distance = length, .Node = toNode}}};
}

const Carrier &carrierIn(const Network &network, EntityKey key) {
  const std::vector<Carrier> &carriers = network.carriers();
  const auto found =
      std::ranges::find_if(carriers, [key](const Carrier &carrier) { return carrier.Key == key; });
  REQUIRE(found != carriers.end());
  return *found;
}

// The place is on the carrier, resolves, and no point of the carrier's ground line is nearer the
// position than the place's ground point by more than 1e-9.
void requireNearestOn(const Network &network, EntityKey carrier, GroundPoint position,
                      const std::optional<Place> &nearest) {
  CAPTURE(carrier, position.X, position.Z);
  REQUIRE(nearest.has_value());
  const Place place = nearest.value_or(Place{});
  CAPTURE(place.Carrier, place.Distance);
  REQUIRE(place.Carrier == carrier);
  REQUIRE(network.resolve(place).has_value());
  const std::optional<GroundPoint> ground = network.groundPoint(place);
  REQUIRE(ground.has_value());
  REQUIRE(groundDistance(position, ground.value_or(NOWHERE)) <=
          distanceToCarrier(carrierIn(network, carrier), position) + 1e-9);
}

TEST_CASE("carryOver gives no place when the place did not resolve before or its carrier is gone "
          "after") {
  constexpr EntityKey LINE{1};
  const Network before({straight(LINE, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 0, 1)}, 2, {});

  SECTION("a place that did not resolve before, though it resolves after") {
    // LINE grows to 20 and a carrier NEW appears, so each of these places resolves after.
    constexpr EntityKey NEW{2};
    const Network after({Carrier{.Key = LINE,
                                 .Points = {{.X = 0, .Z = 0, .Distance = 0},
                                            {.X = 10, .Z = 0, .Distance = 10},
                                            {.X = 20, .Z = 0, .Distance = 20}},
                                 .Stops = {{.Distance = 0, .Node = 0},
                                           {.Distance = 10, .Node = 1},
                                           {.Distance = 20, .Node = 2}}},
                         straight(NEW, {.X = 0, .Z = 5}, {.X = 10, .Z = 5}, 10, 3, 4)},
                        5, {});
    for (const Place place :
         std::vector<Place>{{.Carrier = LINE, .Distance = 15}, {.Carrier = NEW, .Distance = 5}}) {
      CAPTURE(place.Carrier, place.Distance);
      REQUIRE(after.resolve(place).has_value());
      REQUIRE_FALSE(carryOver(place, before, after).has_value());
    }
    for (const Place place : std::vector<Place>{{.Carrier = LINE, .Distance = NOT_A_NUMBER},
                                                {.Carrier = LINE, .Distance = -1}}) {
      CAPTURE(place.Carrier, place.Distance);
      REQUIRE_FALSE(carryOver(place, before, after).has_value());
    }
  }
  SECTION("a place whose carrier the network after lacks") {
    // Another carrier lies exactly where LINE was, and the place must not jump onto it.
    const Network after({straight(EntityKey{3}, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 0, 1)}, 2,
                        {});
    REQUIRE_FALSE(carryOver({.Carrier = LINE, .Distance = 5}, before, after).has_value());
  }
}

TEST_CASE("carryOver keeps a place whose carrier's points are unchanged, whatever else differs") {
  constexpr EntityKey LINE{1};
  constexpr EntityKey LOOP{2};
  // LOOP ends where it starts, at (20,0), so its distances 0 and 12 share a ground position.
  const std::vector<CarrierPoint> loopPoints = {{.X = 20, .Z = 0, .Distance = 0},
                                                {.X = 24, .Z = 0, .Distance = 4},
                                                {.X = 24, .Z = 3, .Distance = 7},
                                                {.X = 20, .Z = 0, .Distance = 12}};
  const Network before(
      {straight(LINE, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 0, 1),
       Carrier{.Key = LOOP,
               .Points = loopPoints,
               .Stops = {{.Distance = 0, .Node = 2}, {.Distance = 12, .Node = 2}}}},
      3, {{.Node = 0, .Entity = EntityKey{7}}});
  // After, a new carrier meets LINE at a node that splits its edge at distance 4, LOOP gains a
  // node, and the node count and anchors differ.
  const Network after({Carrier{.Key = LINE,
                               .Points = carrierIn(before, LINE).Points,
                               .Stops = {{.Distance = 0, .Node = 0},
                                         {.Distance = 4, .Node = 3},
                                         {.Distance = 10, .Node = 1}}},
                       Carrier{.Key = LOOP,
                               .Points = loopPoints,
                               .Stops = {{.Distance = 0, .Node = 2},
                                         {.Distance = 7, .Node = 4},
                                         {.Distance = 12, .Node = 2}}},
                       straight(EntityKey{3}, {.X = 4, .Z = 5}, {.X = 4, .Z = 0}, 5, 5, 3)},
                      6,
                      {{.Node = 3, .Entity = EntityKey{9}}, {.Node = 5, .Entity = EntityKey{7}}});

  // At the new node, inside a split edge, and at LOOP's end, where snapping to the nearest place
  // would give distance 0 instead.
  for (const Place place : std::vector<Place>{{.Carrier = LINE, .Distance = 4},
                                              {.Carrier = LINE, .Distance = 7},
                                              {.Carrier = LOOP, .Distance = 12}}) {
    CAPTURE(place.Carrier, place.Distance);
    REQUIRE(carryOver(place, before, after) == place);
    const std::optional<GroundPoint> was = before.groundPoint(place);
    const std::optional<GroundPoint> is = after.groundPoint(place);
    REQUIRE(was.has_value());
    REQUIRE(is.has_value());
    REQUIRE(is.value_or(NOWHERE).X == was.value_or(NOWHERE).X);
    REQUIRE(is.value_or(NOWHERE).Z == was.value_or(NOWHERE).Z);
  }
}

TEST_CASE("carryOver moves a place whose carrier's points changed to the nearest place on the same "
          "carrier to its old ground position") {
  constexpr EntityKey LINE{5};
  auto requireMoved = [](const Place &place, const Network &before, const Network &after) {
    CAPTURE(place.Carrier, place.Distance);
    const std::optional<GroundPoint> was = before.groundPoint(place);
    REQUIRE(was.has_value());
    const GroundPoint old = was.value_or(NOWHERE);
    const std::optional<Place> moved = carryOver(place, before, after);
    REQUIRE(moved == after.nearestPlaceOn(place.Carrier, old));
    requireNearestOn(after, place.Carrier, old, moved);
  };

  SECTION("the carrier moved aside, and another carrier now lies nearer its old line") {
    const Network before({straight(LINE, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 20, 0, 1)}, 2, {});
    const Network after({straight(LINE, {.X = 0, .Z = 2}, {.X = 8, .Z = 2}, 8, 0, 1),
                         straight(EntityKey{2}, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 2, 3)},
                        4, {});
    requireMoved({.Carrier = LINE, .Distance = 8}, before, after);
  }
  SECTION("the carrier shortened past the place") {
    const Network before({straight(LINE, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 0, 1)}, 2, {});
    const Network after({straight(LINE, {.X = 0, .Z = 0}, {.X = 6, .Z = 0}, 6, 0, 1)}, 2, {});
    requireMoved({.Carrier = LINE, .Distance = 9}, before, after);
  }
  SECTION("the carrier's ground line is the same but its producer measures it differently") {
    const Network before({straight(LINE, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 0, 1)}, 2, {});
    const Network after({straight(LINE, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 20, 0, 1)}, 2, {});
    requireMoved({.Carrier = LINE, .Distance = 5}, before, after);
  }
  SECTION("the carrier bent differently, with distances unlike ground lengths before") {
    // Before, distance 11 lies halfway along the second segment, at (4, 1.5), which only the
    // network before can say.
    const Network before(
        {Carrier{.Key = LINE,
                 .Points = {{.X = 0, .Z = 0, .Distance = 0},
                            {.X = 4, .Z = 0, .Distance = 10},
                            {.X = 4, .Z = 3, .Distance = 12}},
                 .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 12, .Node = 1}}}},
        2, {});
    const Network after(
        {Carrier{.Key = LINE,
                 .Points = {{.X = 0, .Z = 0, .Distance = 0},
                            {.X = 4, .Z = 0, .Distance = 4},
                            {.X = 4, .Z = 6, .Distance = 10}},
                 .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 10, .Node = 1}}}},
        2, {});
    requireMoved({.Carrier = LINE, .Distance = 11}, before, after);
  }
}

// BEND runs (0,0) to (4,0) over distance 10 and then to (4,3) over 2. RULER, with a lower key, runs
// alongside it at z = 2, so points near one are often nearer the other.
constexpr EntityKey BEND{10};
constexpr EntityKey RULER{5};

Network bendAndRuler() {
  return {{Carrier{.Key = BEND,
                   .Points = {{.X = 0, .Z = 0, .Distance = 0},
                              {.X = 4, .Z = 0, .Distance = 10},
                              {.X = 4, .Z = 3, .Distance = 12}},
                   .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 12, .Node = 1}}},
           straight(RULER, {.X = 0, .Z = 2}, {.X = 10, .Z = 2}, 10, 2, 3)},
          4,
          {}};
}

TEST_CASE("nearestPlaceOn gives a place on its carrier that resolves, and no point of that carrier "
          "is nearer") {
  const Network network = bendAndRuler();
  struct Query {
    EntityKey Carrier;
    GroundPoint Position;
  };
  // Nearer the other carrier, beside BEND's corner, beyond BEND's end, before BEND's start, and on
  // BEND itself while asking for RULER.
  for (const Query query : std::vector<Query>{{.Carrier = BEND, .Position = {.X = 2, .Z = 1.5}},
                                              {.Carrier = BEND, .Position = {.X = 5, .Z = -0.5}},
                                              {.Carrier = BEND, .Position = {.X = 4, .Z = 7}},
                                              {.Carrier = BEND, .Position = {.X = -3, .Z = -4}},
                                              {.Carrier = RULER, .Position = {.X = 4, .Z = 1}}}) {
    requireNearestOn(network, query.Carrier, query.Position,
                     network.nearestPlaceOn(query.Carrier, query.Position));
  }
}

TEST_CASE("nearestPlaceOn gives the lowest distance among equally near places") {
  constexpr EntityKey U_TURN{4};
  constexpr EntityKey LOOP{2};
  // U_TURN's sides pass one unit either side of (5,1), at distances 5 and 17. LOOP ends where it
  // starts, at (20,0), so that point is at distances 0 and 12.
  const Network network(
      {Carrier{.Key = U_TURN,
               .Points = {{.X = 0, .Z = 0, .Distance = 0},
                          {.X = 10, .Z = 0, .Distance = 10},
                          {.X = 10, .Z = 2, .Distance = 12},
                          {.X = 0, .Z = 2, .Distance = 22}},
               .Stops = {{.Distance = 0, .Node = 0}, {.Distance = 22, .Node = 1}}},
       Carrier{.Key = LOOP,
               .Points = {{.X = 20, .Z = 0, .Distance = 0},
                          {.X = 24, .Z = 0, .Distance = 4},
                          {.X = 24, .Z = 3, .Distance = 7},
                          {.X = 20, .Z = 0, .Distance = 12}},
               .Stops = {{.Distance = 0, .Node = 2}, {.Distance = 12, .Node = 2}}}},
      3, {});
  REQUIRE(network.nearestPlaceOn(U_TURN, {.X = 5, .Z = 1}) ==
          Place{.Carrier = U_TURN, .Distance = 5});
  REQUIRE(network.nearestPlaceOn(LOOP, {.X = 19, .Z = -1}) ==
          Place{.Carrier = LOOP, .Distance = 0});
}

TEST_CASE("nearestPlace's result is nearestPlaceOn of its own carrier and the same position") {
  auto requireAgrees = [](const Network &network, GroundPoint position) {
    CAPTURE(position.X, position.Z);
    const std::optional<Place> nearest = network.nearestPlace(position);
    REQUIRE(nearest.has_value());
    const EntityKey carrier = nearest.value_or(Place{}).Carrier;
    REQUIRE(network.nearestPlaceOn(carrier, position) == nearest);
  };

  SECTION("at a junction, where nearestPlace's tie goes to the lower key at its higher distance") {
    const Network network({straight(EntityKey{7}, {.X = 0, .Z = 0}, {.X = 10, .Z = 0}, 10, 0, 1),
                           straight(EntityKey{3}, {.X = 0, .Z = 10}, {.X = 0, .Z = 0}, 10, 2, 0)},
                          3, {});
    requireAgrees(network, {.X = -1, .Z = -1});
  }
  SECTION("on a random synthetic network with junctions and parallel carriers") {
    const Network network = makeSyntheticNetwork(2, 9).build();
    SyntheticRandom queries(1002);
    for (int i = 0; i < 4; ++i) {
      const double x = queries.between(-60.0, 60.0);
      const double z = queries.between(-60.0, 60.0);
      requireAgrees(network, {.X = x, .Z = z});
    }
  }
}

} // namespace
} // namespace tpj
