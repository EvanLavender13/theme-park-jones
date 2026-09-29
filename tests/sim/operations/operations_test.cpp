#include "support/ledger_writes.h"
#include "support/line_park.h"
#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <stdint.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::carry;
using test::DEPOT;
using test::depotAcross;
using test::depotAt;
using test::FAR_DEPOT;
using test::GONE;
using test::hold;
using test::LINE;
using test::lineWorld;
using test::LOOSE_DEPOT;
using test::LOOSE_SHOP;
using test::looseDepot;
using test::looseShop;
using test::OTHER_SHOP;
using test::packetsFrom;
using test::ParkIntent;
using test::SHOP;
using test::shopAt;
using test::TWIN_DEPOT;
using test::worldOf;

// The least Distance among the source's entries that sampling backstage-route-distance gives at
// the nodePlace of each backstage node anchored to at, or none.
std::optional<double> sampledLength(const World &world, EntityKey at, EntityKey source) {
  const Network &network = parkNetwork(world, PathKind::Backstage);
  std::optional<double> least;
  for (const uint32_t node : network.anchoredNodes(at)) {
    for (const SampledEntry<RouteEntry> &entry :
         sampleField<RouteDistance<PathKind::Backstage>>(world, network, network.nodePlace(node))) {
      if (entry.Source == source) {
        least = least.has_value() ? std::min(least.value(), entry.Value.Distance)
                                  : entry.Value.Distance;
      }
    }
  }
  return least;
}

// Stands in for a sampled length that a REQUIRE has already shown is present.
constexpr double NO_LENGTH = std::numeric_limits<double>::quiet_NaN();

// A packet sent at tick 0 with a delay of 10, or a returning one.
FlowPacket packetOf(EntityKey from, EntityKey to, EntityKey handle, int64_t units,
                    bool returning = false) {
  return FlowPacket{.Arrival = 10,
                    .From = from,
                    .To = to,
                    .Handle = handle,
                    .Units = units,
                    .Delay = 10,
                    .Returning = returning};
}

// Registration.

TEST_CASE("Resolving a world made with makeParkSchema gives it a ledger for each of "
          "supply-orders, supplies, guest-visits, and meals") {
  SECTION("an empty park") {
    World world = worldOf({});
    resolveWorld(world);
    CHECK(ledgerOf<SupplyOrders>(world) != nullptr);
    CHECK(ledgerOf<Supplies>(world) != nullptr);
    CHECK(ledgerOf<GuestVisits>(world) != nullptr);
    CHECK(ledgerOf<Meals>(world) != nullptr);
  }
  SECTION("the new-park template") {
    World world = makeNewPark(5);
    resolveWorld(world);
    CHECK(ledgerOf<SupplyOrders>(world) != nullptr);
    CHECK(ledgerOf<Supplies>(world) != nullptr);
    CHECK(ledgerOf<GuestVisits>(world) != nullptr);
    CHECK(ledgerOf<Meals>(world) != nullptr);
  }
}

TEST_CASE("addOperations registers the four kinds' ledgers and then shop-service, a state "
          "component type") {
  WorldSchema schema;
  addOperations(schema);
  std::vector<std::string> names;
  for (const ComponentType &type : schema.components()) {
    names.push_back(type.Name);
  }
  CHECK(names == std::vector<std::string>{"supply-orders-ledger", "supplies-ledger",
                                          "guest-visits-ledger", "meals-ledger", "shop-service"});
  REQUIRE_FALSE(schema.components().empty());
  CHECK(schema.components().back().Kind == DataKind::State);
}

// Supply routes.

TEST_CASE("supplyRouteLength is the backstage route distance from the node anchored to at to the "
          "source") {
  const World world =
      lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0)});
  REQUIRE(sampledLength(world, SHOP, DEPOT).has_value());
  REQUIRE(sampledLength(world, DEPOT, SHOP).has_value());
  REQUIRE(sampledLength(world, SHOP, OTHER_SHOP).has_value());
  CHECK(supplyRouteLength(world, SHOP, DEPOT) == sampledLength(world, SHOP, DEPOT));
  CHECK(supplyRouteLength(world, DEPOT, SHOP) == sampledLength(world, DEPOT, SHOP));
  // Any source on the backstage network, not only a depot.
  CHECK(supplyRouteLength(world, SHOP, OTHER_SHOP) == sampledLength(world, SHOP, OTHER_SHOP));
}

TEST_CASE("supplyRouteLength is none when at anchors no backstage node or the source has no entry "
          "there") {
  SECTION("at has no backstage connector") {
    const World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), looseShop()});
    CHECK_FALSE(supplyRouteLength(world, LOOSE_SHOP, DEPOT).has_value());
  }
  SECTION("the source has no backstage connector") {
    const World world = lineWorld({shopAt(SHOP, 0.0), looseDepot()});
    CHECK_FALSE(supplyRouteLength(world, SHOP, LOOSE_DEPOT).has_value());
  }
  SECTION("the source is no entity") {
    const World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
    CHECK_FALSE(supplyRouteLength(world, SHOP, GONE).has_value());
  }
  SECTION("at and the source are joined only by a guest path") {
    // Both shops' front doors are 3 m from the guest path at z = -12, and no backstage line is
    // near their back doors.
    World world = worldOf(ParkIntent{.Entrances = {},
                                     .Paths = {ParkPath{.Key = LINE,
                                                        .Kind = PathKind::Guest,
                                                        .Points = {{-64.0, -12.0}, {64.0, -12.0}}}},
                                     .Boxes = {shopAt(SHOP, 0.0), shopAt(OTHER_SHOP, 16.0)}});
    resolveWorld(world);
    CHECK_FALSE(supplyRouteLength(world, SHOP, OTHER_SHOP).has_value());
  }
}

TEST_CASE("nearestDepot is the depot box with the least supply route length at the shop, with that "
          "length") {
  // The far depot has the lower key, the other shop is nearer than any depot, and the loose depot
  // has no route.
  const World world = lineWorld({shopAt(SHOP, 0.0), depotAt(FAR_DEPOT, -48.0), depotAt(DEPOT, 32.0),
                                 shopAt(OTHER_SHOP, 16.0), looseDepot()});
  const std::optional<double> length = sampledLength(world, SHOP, DEPOT);
  REQUIRE(length.has_value());
  REQUIRE(sampledLength(world, SHOP, FAR_DEPOT) > length);
  CHECK(nearestDepot(world, SHOP) ==
        std::optional<DepotRoute>(DepotRoute{DEPOT, length.value_or(NO_LENGTH)}));
}

TEST_CASE("nearestDepot breaks a tie in supply route length toward the lower key") {
  // Depots facing each other across the line at the same x meet it at the same place, 3 m from
  // each door, so their routes to the shop tie.
  SECTION("the lower key on the far side of the line") {
    const World world =
        lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), depotAcross(TWIN_DEPOT, 32.0)});
    const std::optional<double> length = sampledLength(world, SHOP, DEPOT);
    REQUIRE(length.has_value());
    REQUIRE(sampledLength(world, SHOP, TWIN_DEPOT) == length);
    CHECK(nearestDepot(world, SHOP) ==
          std::optional<DepotRoute>(DepotRoute{DEPOT, length.value_or(NO_LENGTH)}));
  }
  SECTION("the lower key on the shop's side of the line") {
    const World world =
        lineWorld({shopAt(SHOP, 0.0), depotAcross(DEPOT, 32.0), depotAt(TWIN_DEPOT, 32.0)});
    const std::optional<double> length = sampledLength(world, SHOP, DEPOT);
    REQUIRE(length.has_value());
    REQUIRE(sampledLength(world, SHOP, TWIN_DEPOT) == length);
    CHECK(nearestDepot(world, SHOP) ==
          std::optional<DepotRoute>(DepotRoute{DEPOT, length.value_or(NO_LENGTH)}));
  }
}

TEST_CASE("nearestDepot is none when no depot box has a supply route length at the shop") {
  SECTION("the shop has no backstage connector") {
    const World world = lineWorld({depotAt(DEPOT, 32.0), looseShop()});
    CHECK_FALSE(nearestDepot(world, LOOSE_SHOP).has_value());
  }
  SECTION("the shop's backstage line reaches no depot") {
    const World world = lineWorld({shopAt(SHOP, 0.0), looseDepot()});
    CHECK_FALSE(nearestDepot(world, SHOP).has_value());
  }
  SECTION("the shop's backstage line reaches only another shop") {
    const World world = lineWorld({shopAt(SHOP, 0.0), shopAt(OTHER_SHOP, 16.0)});
    CHECK_FALSE(nearestDepot(world, SHOP).has_value());
  }
}

// Shipment delay.

TEST_CASE("shipmentDelay is ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS)), SUPPLY_SPEED being "
          "2 m/s") {
  CHECK(SUPPLY_SPEED == 2.0);
  for (const double distance : {38.0, 1.0, 1000.0, 2.0e8}) {
    CAPTURE(distance);
    CHECK(shipmentDelay(distance) ==
          static_cast<uint32_t>(std::ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS))));
  }
}

TEST_CASE("shipmentDelay gives 1 for a result below 1 or a NaN") {
  for (const double distance : {0.0, -0.0, -5.0, -std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN()}) {
    CAPTURE(distance);
    CHECK(shipmentDelay(distance) == 1);
  }
}

TEST_CASE("shipmentDelay gives the largest uint32_t for a result above it") {
  for (const double distance : {1.0e12, std::numeric_limits<double>::infinity()}) {
    CAPTURE(distance);
    CHECK(shipmentDelay(distance) == std::numeric_limits<uint32_t>::max());
  }
}

// Inventory position.

TEST_CASE("inventoryPosition counts the supplies the shop holds, supplies heading to it, and its "
          "orders heading to or held by a depot box, and nothing else addressed to it") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0)});
  REQUIRE(inventoryPosition(world, SHOP) == 0);
  int64_t expected = 0;

  SECTION("supplies the shop holds count") {
    hold<Supplies>(world, SHOP, SHOP, 5);
    expected = 5;
  }
  SECTION("supplies heading to the shop count") {
    carry<Supplies>(world, packetOf(DEPOT, SHOP, SHOP, 4));
    expected = 4;
  }
  SECTION("orders heading to a depot box count") {
    carry<SupplyOrders>(world, packetOf(SHOP, DEPOT, SHOP, 3));
    expected = 3;
  }
  SECTION("orders a depot box holds count") {
    hold<SupplyOrders>(world, DEPOT, SHOP, 6);
    expected = 6;
  }
  SECTION("orders the shop holds do not count") { hold<SupplyOrders>(world, SHOP, SHOP, 7); }
  SECTION("orders a depot sent back to the shop do not count") {
    carry<SupplyOrders>(world, packetOf(DEPOT, SHOP, SHOP, 7));
  }
  SECTION("orders returning from a deleted depot do not count") {
    carry<SupplyOrders>(world, packetOf(GONE, SHOP, SHOP, 7, true));
  }
  SECTION("orders heading to a deleted depot do not count") {
    carry<SupplyOrders>(world, packetOf(SHOP, GONE, SHOP, 7));
  }
  SECTION("orders a deleted depot holds do not count") { hold<SupplyOrders>(world, GONE, SHOP, 7); }
  SECTION("supplies returning to a depot do not count") {
    carry<Supplies>(world, packetOf(SHOP, DEPOT, SHOP, 7, true));
  }
  SECTION("supplies a depot holds do not count") { hold<Supplies>(world, DEPOT, SHOP, 7); }
  SECTION("units addressed to another shop do not count") {
    hold<Supplies>(world, OTHER_SHOP, OTHER_SHOP, 7);
    carry<Supplies>(world, packetOf(DEPOT, OTHER_SHOP, OTHER_SHOP, 7));
    carry<SupplyOrders>(world, packetOf(OTHER_SHOP, DEPOT, OTHER_SHOP, 7));
    hold<SupplyOrders>(world, DEPOT, OTHER_SHOP, 7);
  }
  SECTION("everything at once counts the four that count, added") {
    hold<Supplies>(world, SHOP, SHOP, 5);
    carry<Supplies>(world, packetOf(DEPOT, SHOP, SHOP, 4));
    carry<SupplyOrders>(world, packetOf(SHOP, DEPOT, SHOP, 3));
    hold<SupplyOrders>(world, DEPOT, SHOP, 6);
    hold<SupplyOrders>(world, SHOP, SHOP, 7);
    carry<SupplyOrders>(world, packetOf(SHOP, GONE, SHOP, 7));
    hold<SupplyOrders>(world, GONE, SHOP, 7);
    hold<Supplies>(world, DEPOT, SHOP, 7);
    expected = 18;
  }

  CHECK(inventoryPosition(world, SHOP) == expected);
}

// The shop's step.

TEST_CASE("A shop consumes every order unit it holds under its own handle as unfilled") {
  SECTION("a supplied shop") {
    World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
    hold<SupplyOrders>(world, SHOP, SHOP, 7);
    stepWorld(world);
    CHECK(unitsHeld<SupplyOrders>(world, SHOP, SHOP) == 0);
    CHECK(unitsConsumed<SupplyOrders>(world, UNFILLED_CAUSE) == 7);
  }
  SECTION("a starved shop") {
    World world = lineWorld({shopAt(SHOP, 0.0)});
    hold<SupplyOrders>(world, SHOP, SHOP, 7);
    stepWorld(world);
    CHECK(unitsHeld<SupplyOrders>(world, SHOP, SHOP) == 0);
    CHECK(unitsConsumed<SupplyOrders>(world, UNFILLED_CAUSE) == 7);
  }
}

TEST_CASE("A supplied shop at or below the reorder point sends one packet of ORDER_UP_TO minus its "
          "position to its nearest depot, under its own handle, with the delay ORDER_DELAY") {
  CHECK(REORDER_POINT == 8);
  CHECK(ORDER_UP_TO == 24);
  CHECK(ORDER_DELAY == 30);
  // The far depot has the lower key, so the order goes to the nearest, not the first.
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(FAR_DEPOT, -48.0), depotAt(DEPOT, 32.0)});
  int64_t position = 0;
  SECTION("an empty shop") {}
  SECTION("a shop at the reorder point") {
    hold<Supplies>(world, SHOP, SHOP, REORDER_POINT);
    position = REORDER_POINT;
  }
  REQUIRE(inventoryPosition(world, SHOP) == position);
  const uint64_t tick = world.Tick;

  stepWorld(world);

  CHECK(packetsFrom<SupplyOrders>(world, SHOP) ==
        std::vector<FlowPacket>{{.Arrival = tick + ORDER_DELAY,
                                 .From = SHOP,
                                 .To = DEPOT,
                                 .Handle = SHOP,
                                 .Units = ORDER_UP_TO - position,
                                 .Delay = ORDER_DELAY,
                                 .Returning = false}});
}

TEST_CASE("A shop above the reorder point sends no orders") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
  hold<Supplies>(world, SHOP, SHOP, REORDER_POINT + 1);
  stepWorld(world);
  CHECK(packetsFrom<SupplyOrders>(world, SHOP).empty());
  CHECK(unitsCreated<SupplyOrders>(world) == 0);
}

TEST_CASE("A starved shop sends no orders") {
  SECTION("a shop with no backstage connector") {
    World world = lineWorld({depotAt(DEPOT, 32.0), looseShop()});
    stepWorld(world);
    CHECK(packetsFrom<SupplyOrders>(world, LOOSE_SHOP).empty());
    CHECK(unitsCreated<SupplyOrders>(world) == 0);
  }
  SECTION("a shop whose backstage line reaches no depot") {
    World world = lineWorld({shopAt(SHOP, 0.0), looseDepot()});
    stepWorld(world);
    CHECK(packetsFrom<SupplyOrders>(world, SHOP).empty());
    CHECK(unitsCreated<SupplyOrders>(world) == 0);
  }
}

// The depot's step.

TEST_CASE("A depot consumes every supplies unit it holds as returned") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
  hold<Supplies>(world, DEPOT, SHOP, 5);
  hold<Supplies>(world, DEPOT, GONE, 2);
  stepWorld(world);
  CHECK(stockOf<Supplies>(world, DEPOT).empty());
  CHECK(unitsConsumed<Supplies>(world, RETURNED_CAUSE) == 7);
}

TEST_CASE("A depot consumes as cancelled the order units it holds under a handle that names no "
          "shop box") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(FAR_DEPOT, -48.0), depotAt(DEPOT, 32.0)});
  EntityKey handle = NULL_KEY;
  SECTION("a handle no entity holds") { handle = GONE; }
  SECTION("a handle naming a depot box") { handle = FAR_DEPOT; }
  hold<SupplyOrders>(world, DEPOT, handle, 4);

  stepWorld(world);

  CHECK(unitsHeld<SupplyOrders>(world, DEPOT, handle) == 0);
  CHECK(unitsConsumed<SupplyOrders>(world, CANCELLED_CAUSE) == 4);
  CHECK(packetsFrom<SupplyOrders>(world, DEPOT).empty());
  CHECK(unitsCreated<Supplies>(world) == 0);
}

// Steps a world whose depot holds 4 order units of a shop it has no supply route to, and checks
// that it sends them back.
void checkSentBack(World world, EntityKey depot, EntityKey shop) {
  hold<SupplyOrders>(world, depot, shop, 4);
  const uint64_t tick = world.Tick;

  stepWorld(world);

  CHECK(packetsFrom<SupplyOrders>(world, depot) ==
        std::vector<FlowPacket>{{.Arrival = tick + ORDER_DELAY,
                                 .From = depot,
                                 .To = shop,
                                 .Handle = shop,
                                 .Units = 4,
                                 .Delay = ORDER_DELAY,
                                 .Returning = false}});
  CHECK(unitsHeld<SupplyOrders>(world, depot, shop) == 0);
  CHECK(unitsConsumed<SupplyOrders>(world) == 0);
  CHECK(unitsCreated<Supplies>(world) == 0);
}

TEST_CASE("A depot sends back to the shop, under its handle and with the delay ORDER_DELAY, the "
          "order units of a shop it has no supply route to") {
  SECTION("the shop has no backstage connector") {
    checkSentBack(lineWorld({depotAt(DEPOT, 32.0), looseShop()}), DEPOT, LOOSE_SHOP);
  }
  SECTION("the depot has no backstage connector") {
    checkSentBack(lineWorld({shopAt(SHOP, 0.0), looseDepot()}), LOOSE_DEPOT, SHOP);
  }
}

TEST_CASE("A depot fulfils the order units of each shop it reaches, shipping as many supplies to "
          "the shop under its handle with the shipmentDelay of the supply route length") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0)});
  hold<SupplyOrders>(world, DEPOT, SHOP, 10);
  hold<SupplyOrders>(world, DEPOT, OTHER_SHOP, 12);
  const std::optional<double> toShop = sampledLength(world, DEPOT, SHOP);
  const std::optional<double> toOther = sampledLength(world, DEPOT, OTHER_SHOP);
  REQUIRE(toShop.has_value());
  REQUIRE(toOther.has_value());
  const uint32_t shopDelay = shipmentDelay(toShop.value_or(NO_LENGTH));
  const uint32_t otherDelay = shipmentDelay(toOther.value_or(NO_LENGTH));
  const uint64_t tick = world.Tick;

  stepWorld(world);

  CHECK(unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE) == 22);
  CHECK(unitsCreated<Supplies>(world) == 22);
  CHECK(stockOf<SupplyOrders>(world, DEPOT).empty());
  CHECK(stockOf<Supplies>(world, DEPOT).empty());
  std::vector<FlowPacket> expected{{.Arrival = tick + shopDelay,
                                    .From = DEPOT,
                                    .To = SHOP,
                                    .Handle = SHOP,
                                    .Units = 10,
                                    .Delay = shopDelay,
                                    .Returning = false},
                                   {.Arrival = tick + otherDelay,
                                    .From = DEPOT,
                                    .To = OTHER_SHOP,
                                    .Handle = OTHER_SHOP,
                                    .Units = 12,
                                    .Delay = otherDelay,
                                    .Returning = false}};
  std::ranges::sort(expected);
  CHECK(packetsFrom<Supplies>(world, DEPOT) == expected);
}

} // namespace
} // namespace tpj
