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

// tests/parks/supply.park.

} // namespace
} // namespace tpj
