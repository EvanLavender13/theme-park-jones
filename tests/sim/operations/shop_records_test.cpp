#include "support/ledger_writes.h"
#include "support/line_park.h"

#include "sim/entity_key.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::carry;
using test::DEPOT;
using test::depotAt;
using test::hold;
using test::LINE;
using test::lineWorld;
using test::LOOSE_SHOP;
using test::looseShop;
using test::OTHER_SHOP;
using test::SHOP;
using test::shopAt;

// A shop on the line with a depot it reaches.
World suppliedWorld() { return lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)}); }

// A shop on the line with no depot anywhere.
World starvedWorld() { return lineWorld({shopAt(SHOP, 0.0)}); }

ShopRecord requireRecord(const World &world, EntityKey shop) {
  const std::optional<ShopRecord> record = shopRecord(world, shop);
  REQUIRE(record.has_value());
  return record.value_or(ShopRecord{});
}

// A guest that has left: its key once named an entity and names none now.
EntityKey leftGuest(World &world) {
  const EntityKey guest = world.createEntity();
  REQUIRE(world.destroyEntity(guest));
  return guest;
}

FlowPacket packetOf(EntityKey from, EntityKey to, EntityKey handle, int64_t units) {
  return FlowPacket{.Arrival = 10,
                    .From = from,
                    .To = to,
                    .Handle = handle,
                    .Units = units,
                    .Delay = 10,
                    .Returning = false};
}

TEST_CASE("shopRecord gives a record for each shop box and none for any other key") {
  World world =
      lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0), looseShop()});
  // A live entity that is no box, as a guest is.
  const EntityKey guest = world.createEntity();

  for (const EntityKey shop : {SHOP, OTHER_SHOP, LOOSE_SHOP}) {
    INFO("shop " << static_cast<uint64_t>(shop));
    CHECK(shopRecord(world, shop).has_value());
  }
  for (const EntityKey other : {LINE, DEPOT, guest, test::GONE, NULL_KEY}) {
    INFO("key " << static_cast<uint64_t>(other));
    CHECK_FALSE(shopRecord(world, other).has_value());
  }
}

TEST_CASE("A record's Stock is the supplies units the shop holds under its own handle") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0)});
  hold<Supplies>(world, SHOP, SHOP, 3);
  // None of these is held by the shop under its own handle.
  hold<Supplies>(world, SHOP, test::GONE, 4);
  hold<Supplies>(world, OTHER_SHOP, OTHER_SHOP, 5);
  carry<Supplies>(world, packetOf(DEPOT, SHOP, SHOP, 6));

  CHECK(requireRecord(world, SHOP).Stock == 3);
}

TEST_CASE("A record's Queue is the guest-visits units the shop holds under handles that name live "
          "entities, counted before the shop's step queues them") {
  World world = lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0)});
  const EntityKey twice = world.createEntity();
  const EntityKey once = world.createEntity();
  const EntityKey elsewhere = world.createEntity();
  const EntityKey coming = world.createEntity();
  const EntityKey left = leftGuest(world);
  hold<GuestVisits>(world, SHOP, twice, 2);
  hold<GuestVisits>(world, SHOP, once, 1);
  // None of these is held by the shop under a live guest's key.
  hold<GuestVisits>(world, SHOP, left, 3);
  hold<GuestVisits>(world, OTHER_SHOP, elsewhere, 1);
  carry<GuestVisits>(world, packetOf(coming, SHOP, coming, 1));

  CHECK(requireRecord(world, SHOP).Queue == 3);
}

TEST_CASE("A record's OnOrder is the shop's inventoryPosition less its Stock") {
  World world = suppliedWorld();
  hold<Supplies>(world, SHOP, SHOP, 3);
  carry<Supplies>(world, packetOf(DEPOT, SHOP, SHOP, 5));
  carry<SupplyOrders>(world, packetOf(SHOP, DEPOT, SHOP, 4));
  hold<SupplyOrders>(world, DEPOT, SHOP, 2);
  REQUIRE(inventoryPosition(world, SHOP) > 3);

  const ShopRecord record = requireRecord(world, SHOP);
  CHECK(record.Stock == 3);
  CHECK(record.OnOrder == inventoryPosition(world, SHOP) - record.Stock);
}

TEST_CASE("A record's Starved is whether the shop has no nearest depot") {
  const World world =
      lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(OTHER_SHOP, 16.0), looseShop()});
  const World alone = starvedWorld();
  SECTION("a shop whose connector reaches a depot") {
    REQUIRE(nearestDepot(world, SHOP).has_value());
    CHECK_FALSE(requireRecord(world, SHOP).Starved);
  }
  SECTION("a shop whose connector reaches no depot") {
    REQUIRE_FALSE(nearestDepot(alone, SHOP).has_value());
    CHECK(requireRecord(alone, SHOP).Starved);
  }
  SECTION("a shop with no backstage connector") {
    REQUIRE_FALSE(nearestDepot(world, LOOSE_SHOP).has_value());
    CHECK(requireRecord(world, LOOSE_SHOP).Starved);
  }
}

TEST_CASE("A starved shop's limiting factor is NoSupplyRoute whatever its stock and queue") {
  World world = starvedWorld();
  SECTION("nothing held, which a supplied shop would call Demand") {}
  SECTION("stock for every guest queued, which a supplied shop would call ServiceRate") {
    hold<Supplies>(world, SHOP, SHOP, 5);
    hold<GuestVisits>(world, SHOP, world.createEntity(), 2);
  }
  SECTION("fewer supplies than guests, which a supplied shop would call Supply") {
    hold<GuestVisits>(world, SHOP, world.createEntity(), 2);
  }

  const ShopRecord record = requireRecord(world, SHOP);
  REQUIRE(record.Starved);
  CHECK(record.Limit == LimitingFactor::NoSupplyRoute);
}

TEST_CASE("A supplied shop's limiting factor is Demand when its Queue is 0, Supply when its Stock "
          "is below its Queue, and ServiceRate otherwise") {
  World world = suppliedWorld();
  int64_t stock = 0;
  int64_t queue = 0;
  LimitingFactor expected = LimitingFactor::Demand;
  SECTION("an empty shop waits on demand") {}
  SECTION("a stocked shop with no guest waits on demand") {
    hold<Supplies>(world, SHOP, SHOP, 5);
    stock = 5;
  }
  SECTION("one supply for two guests waits on supply") {
    hold<Supplies>(world, SHOP, SHOP, 1);
    hold<GuestVisits>(world, SHOP, world.createEntity(), 2);
    stock = 1;
    queue = 2;
    expected = LimitingFactor::Supply;
  }
  SECTION("as many supplies as guests waits on the service rate") {
    hold<Supplies>(world, SHOP, SHOP, 2);
    hold<GuestVisits>(world, SHOP, world.createEntity(), 1);
    hold<GuestVisits>(world, SHOP, world.createEntity(), 1);
    stock = 2;
    queue = 2;
    expected = LimitingFactor::ServiceRate;
  }
  // The visits of a guest that has left are not in the Queue, so one supply covers it.
  SECTION("one supply for one live guest and a left guest's visits waits on the service rate") {
    hold<Supplies>(world, SHOP, SHOP, 1);
    hold<GuestVisits>(world, SHOP, world.createEntity(), 1);
    hold<GuestVisits>(world, SHOP, leftGuest(world), 3);
    stock = 1;
    queue = 1;
    expected = LimitingFactor::ServiceRate;
  }

  const ShopRecord record = requireRecord(world, SHOP);
  REQUIRE_FALSE(record.Starved);
  REQUIRE(record.Stock == stock);
  REQUIRE(record.Queue == queue);
  CHECK(record.Limit == expected);
}

TEST_CASE("limitingFactorName names each factor for display") {
  CHECK(limitingFactorName(LimitingFactor::Demand) == "demand");
  CHECK(limitingFactorName(LimitingFactor::Supply) == "supply");
  CHECK(limitingFactorName(LimitingFactor::ServiceRate) == "service rate");
  CHECK(limitingFactorName(LimitingFactor::NoSupplyRoute) == "no supply route");
}

// tests/parks/supply.park.

std::string supplyText() {
  std::ifstream file(TPJ_PARKS_DIR "/supply.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

TEST_CASE("tests/parks/supply.park loads into a physically valid world holding two shop boxes, one "
          "depot box, and no other box") {
  const World world = loadWorld(makeParkSchema(), supplyText());
  CHECK(isPhysicallyValid(world));
  std::size_t shops = 0;
  std::size_t depots = 0;
  const std::vector<ParkBox> boxes = parkBoxes(world);
  for (const ParkBox &box : boxes) {
    shops += box.Kind == BoxKind::Shop ? 1 : 0;
    depots += box.Kind == BoxKind::Depot ? 1 : 0;
  }
  CHECK(boxes.size() == 3);
  CHECK(shops == 2);
  CHECK(depots == 1);
}

TEST_CASE("Once resolved, exactly one of tests/parks/supply.park's shops is starved") {
  World world = loadWorld(makeParkSchema(), supplyText());
  resolveWorld(world);
  std::size_t shops = 0;
  std::size_t starved = 0;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      ++shops;
      starved += requireRecord(world, box.Key).Starved ? 1 : 0;
    }
  }
  REQUIRE(shops == 2);
  CHECK(starved == 1);
}

} // namespace
} // namespace tpj
