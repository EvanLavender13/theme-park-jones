#include "support/ledger_writes.h"
#include "support/line_park.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::carry;
using test::DEPOT;
using test::depotAt;
using test::GONE;
using test::hold;
using test::lineWorld;
using test::OTHER_SHOP;
using test::packetsFrom;
using test::SHOP;
using test::shopAt;

// A shop on the line with a depot it reaches, at tick 0.
World suppliedWorld() { return lineWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)}); }

// A shop on the line with no depot anywhere, so it has no nearest depot, at tick 0.
World starvedWorld() { return lineWorld({shopAt(SHOP, 0.0)}); }

// Synthetic guests: entities the test creates, so their keys ascend in the order given.
std::vector<EntityKey> addGuests(World &world, std::size_t count) {
  std::vector<EntityKey> guests;
  for (std::size_t index = 0; index < count; ++index) {
    guests.push_back(world.createEntity());
  }
  return guests;
}

// A packet sent at tick 0 that arrives at the tick given.
FlowPacket sentAtZero(EntityKey from, EntityKey to, EntityKey handle, int64_t units,
                      uint64_t arrival) {
  return FlowPacket{.Arrival = arrival,
                    .From = from,
                    .To = to,
                    .Handle = handle,
                    .Units = units,
                    .Delay = static_cast<uint32_t>(arrival),
                    .Returning = false};
}

// What the shop sends a guest it serves while stepping the tick: one unit, addressed to the guest,
// arriving SERVICE_INTERVAL later.
FlowPacket servedPacket(EntityKey guest, uint64_t tick) {
  return FlowPacket{.Arrival = tick + SERVICE_INTERVAL,
                    .From = SHOP,
                    .To = guest,
                    .Handle = guest,
                    .Units = 1,
                    .Delay = SERVICE_INTERVAL,
                    .Returning = false};
}

void stepUntil(World &world, uint64_t tick) {
  while (world.Tick < tick) {
    stepWorld(world);
  }
}

void deleteShop(World &world) {
  CommandQueue queue;
  queueEdit(queue, DeleteBox{SHOP});
  stepWorld(world, queue);
  REQUIRE(world.findEntity(SHOP) == entt::null);
}

// Serving.

TEST_CASE("A shop box that has never held a visit serves the first guest it holds at once") {
  World world = suppliedWorld();
  stepUntil(world, 10);
  const EntityKey guest = world.createEntity();
  hold<GuestVisits>(world, SHOP, guest, 1);
  hold<Supplies>(world, SHOP, SHOP, 1);

  stepWorld(world);

  CHECK(unitsCreated<Meals>(world) == 1);
}

// What serving the guest leaves: one meal created and sent, one supply consumed as served, and the
// guest's other visit still held.
void checkServedStocks(const World &world, EntityKey guest) {
  CHECK(unitsCreated<Meals>(world) == 1);
  CHECK(stockOf<Meals>(world, SHOP).empty());
  CHECK(unitsConsumed<Supplies>(world, SERVED_CAUSE) == 1);
  CHECK(unitsConsumed<Supplies>(world) == 1);
  CHECK(unitsHeld<Supplies>(world, SHOP, SHOP) == 2);
  CHECK(unitsHeld<GuestVisits>(world, SHOP, guest) == 1);
}

// Steps a world whose shop has never served, holding two of one guest's visits and three supplies,
// and checks that it serves the guest.
void checkServes(World world) {
  const EntityKey guest = world.createEntity();
  hold<GuestVisits>(world, SHOP, guest, 2);
  hold<Supplies>(world, SHOP, SHOP, 3);
  const uint64_t tick = world.Tick;

  stepWorld(world);

  CHECK(packetsFrom<GuestVisits>(world, SHOP) ==
        std::vector<FlowPacket>{servedPacket(guest, tick)});
  CHECK(packetsFrom<Meals>(world, SHOP) == std::vector<FlowPacket>{servedPacket(guest, tick)});
  checkServedStocks(world, guest);
}

TEST_CASE("Serving a guest consumes one supply as served and sends the guest one meal and one of "
          "its visits, each one unit under its handle with the delay SERVICE_INTERVAL") {
  CHECK(SERVICE_INTERVAL == 90);
  SECTION("a supplied shop") { checkServes(suppliedWorld()); }
  SECTION("a starved shop serves from its stock") { checkServes(starvedWorld()); }
}

TEST_CASE("A supplied shop serves one guest in a cycle, sending visits and meals for that guest "
          "only, however many are queued and however much it holds") {
  World world = suppliedWorld();
  for (const EntityKey guest : addGuests(world, 3)) {
    hold<GuestVisits>(world, SHOP, guest, 1);
  }
  hold<Supplies>(world, SHOP, SHOP, 5);

  stepWorld(world);

  const std::vector<FlowPacket> visits = packetsFrom<GuestVisits>(world, SHOP);
  const std::vector<FlowPacket> meals = packetsFrom<Meals>(world, SHOP);
  REQUIRE(visits.size() == 1);
  REQUIRE(meals.size() == 1);
  CHECK(visits.front().To == meals.front().To);
  CHECK(unitsConsumed<Supplies>(world, SERVED_CAUSE) == 1);
}

TEST_CASE("A shop that served while stepping tick t serves no other guest before tick t + "
          "SERVICE_INTERVAL, and serves the next one then") {
  World world = suppliedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  hold<GuestVisits>(world, SHOP, guests[0], 1);
  hold<GuestVisits>(world, SHOP, guests[1], 1);
  hold<Supplies>(world, SHOP, SHOP, 3);

  stepWorld(world);
  REQUIRE(unitsCreated<Meals>(world) == 1);
  stepUntil(world, SERVICE_INTERVAL);
  CHECK(unitsCreated<Meals>(world) == 1);

  stepWorld(world);
  CHECK(unitsCreated<Meals>(world) == 2);
  CHECK(packetsFrom<Meals>(world, SHOP) ==
        std::vector<FlowPacket>{servedPacket(guests[1], SERVICE_INTERVAL)});
}

TEST_CASE("A shop that holds no supplies unit under its own handle serves no one") {
  World world = suppliedWorld();
  const EntityKey guest = world.createEntity();
  hold<GuestVisits>(world, SHOP, guest, 1);
  SECTION("it holds no supplies") {}
  SECTION("it holds supplies only under another handle") { hold<Supplies>(world, SHOP, GONE, 3); }
  SECTION("its supplies are still on their way") {
    carry<Supplies>(world, sentAtZero(DEPOT, SHOP, SHOP, 3, 10));
  }

  stepWorld(world);

  CHECK(unitsCreated<Meals>(world) == 0);
  CHECK(unitsConsumed<Supplies>(world, SERVED_CAUSE) == 0);
}

TEST_CASE("A supplied shop returns no visit, however long its queue") {
  World world = suppliedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 4);
  for (const EntityKey guest : guests) {
    hold<GuestVisits>(world, SHOP, guest, 1);
  }

  stepWorld(world);

  CHECK(packetsFrom<GuestVisits>(world, SHOP).empty());
  for (const EntityKey guest : guests) {
    CHECK(unitsHeld<GuestVisits>(world, SHOP, guest) == 1);
  }
}

// The queue.

TEST_CASE("A shop serves the guest whose visit reached it at an earlier swap first, whatever the "
          "guests' keys") {
  World world = suppliedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  const EntityKey low = guests[0];
  const EntityKey high = guests[1];
  hold<GuestVisits>(world, SHOP, high, 1);
  carry<GuestVisits>(world, sentAtZero(low, SHOP, low, 1, 1));
  // The shop can serve only once this arrives, when it holds both visits.
  carry<Supplies>(world, sentAtZero(DEPOT, SHOP, SHOP, 1, 1));

  stepUntil(world, 2);

  const std::vector<FlowPacket> meals = packetsFrom<Meals>(world, SHOP);
  REQUIRE(meals.size() == 1);
  CHECK(meals.front().To == high);
}

TEST_CASE("A shop serves visits that reached it at the same swap in ascending guest key order") {
  World world = suppliedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  const EntityKey low = guests[0];
  const EntityKey high = guests[1];
  carry<GuestVisits>(world, sentAtZero(high, SHOP, high, 1, 1));
  carry<GuestVisits>(world, sentAtZero(low, SHOP, low, 1, 1));
  hold<Supplies>(world, SHOP, SHOP, 1);

  stepUntil(world, 2);

  const std::vector<FlowPacket> meals = packetsFrom<Meals>(world, SHOP);
  REQUIRE(meals.size() == 1);
  CHECK(meals.front().To == low);
}

TEST_CASE("A shop's queue holds an entry for each visit unit it holds, so a guest with two visits "
          "takes two places") {
  // A starved shop keeps as many entries as its cover, here 2, which the first guest's two visits
  // fill, so the second guest's visit goes back.
  World world = starvedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  hold<GuestVisits>(world, SHOP, guests[0], 2);
  hold<GuestVisits>(world, SHOP, guests[1], 1);
  carry<Supplies>(world, sentAtZero(GONE, SHOP, SHOP, 2, 10));

  stepWorld(world);

  CHECK(unitsHeld<GuestVisits>(world, SHOP, guests[0]) == 2);
  CHECK(unitsHeld<GuestVisits>(world, guests[1], guests[1]) == 1);
}

TEST_CASE("A starved shop keeps the visits that reached it earliest and returns the later ones") {
  World world = starvedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  const EntityKey low = guests[0];
  const EntityKey high = guests[1];
  hold<GuestVisits>(world, SHOP, high, 1);
  carry<GuestVisits>(world, sentAtZero(low, SHOP, low, 1, 1));
  carry<Supplies>(world, sentAtZero(GONE, SHOP, SHOP, 1, 20));

  stepUntil(world, 2);

  CHECK(unitsHeld<GuestVisits>(world, SHOP, high) == 1);
  CHECK(unitsHeld<GuestVisits>(world, low, low) == 1);
}

// Starved shops.

TEST_CASE("A starved shop keeps queued only its first c visits, c being the supplies it holds "
          "under its own handle and those in packets under its handle heading to it, and returns "
          "every other visit to its guest at once with no meal") {
  CHECK(RETURN_DELAY == 1);
  World world = lineWorld({shopAt(SHOP, 0.0), shopAt(OTHER_SHOP, 16.0)});
  const std::vector<EntityKey> guests = addGuests(world, 6);
  for (const EntityKey guest : guests) {
    hold<GuestVisits>(world, SHOP, guest, 1);
  }
  std::size_t cover = 0;
  SECTION("nothing held or coming") {}
  SECTION("supplies it holds under its own handle count") {
    hold<Supplies>(world, SHOP, SHOP, 2);
    cover = 2;
  }
  SECTION("supplies under its handle heading to it count") {
    carry<Supplies>(world, sentAtZero(GONE, SHOP, SHOP, 3, 10));
    cover = 3;
  }
  SECTION("supplies it holds under another handle do not count") {
    hold<Supplies>(world, SHOP, GONE, 4);
  }
  SECTION("supplies under its handle heading elsewhere do not count") {
    carry<Supplies>(world, FlowPacket{.Arrival = 10,
                                      .From = SHOP,
                                      .To = GONE,
                                      .Handle = SHOP,
                                      .Units = 4,
                                      .Delay = 10,
                                      .Returning = true});
  }
  SECTION("supplies addressed to another shop do not count") {
    hold<Supplies>(world, OTHER_SHOP, OTHER_SHOP, 4);
    carry<Supplies>(world, sentAtZero(GONE, OTHER_SHOP, OTHER_SHOP, 4, 10));
  }
  SECTION("everything at once counts the two that count, added") {
    hold<Supplies>(world, SHOP, SHOP, 2);
    carry<Supplies>(world, sentAtZero(GONE, SHOP, SHOP, 3, 10));
    hold<Supplies>(world, SHOP, GONE, 4);
    hold<Supplies>(world, OTHER_SHOP, OTHER_SHOP, 4);
    cover = 5;
  }

  stepWorld(world);

  // A visit returned with the delay RETURN_DELAY reaches its guest at the swap that ends the cycle
  // returning it, and one kept or served does not.
  for (std::size_t index = 0; index < guests.size(); ++index) {
    CAPTURE(index, cover);
    const EntityKey guest = guests[index];
    CHECK(unitsHeld<GuestVisits>(world, guest, guest) == (index >= cover ? 1 : 0));
    if (index >= cover) {
      const FlowAddressed meals = addressedTo<Meals>(world, guest);
      CHECK(meals.Packets.empty());
      CHECK(meals.Stocks.empty());
    }
  }
}

// Guests that are gone.

TEST_CASE("A shop consumes every meals unit it holds as abandoned and sends none") {
  World world = suppliedWorld();
  const EntityKey guest = world.createEntity();
  hold<Meals>(world, SHOP, GONE, 1);
  hold<Meals>(world, SHOP, guest, 1);

  stepWorld(world);

  CHECK(stockOf<Meals>(world, SHOP).empty());
  CHECK(unitsConsumed<Meals>(world, ABANDONED_CAUSE) == 2);
  CHECK(unitsInTransit<Meals>(world) == 0);
}

// Steps a world whose shop holds two visits of a guest that is gone, and checks that it consumes
// them as abandoned, neither serving nor returning them.
void checkAbandons(World world) {
  const EntityKey left = world.createEntity();
  REQUIRE(world.destroyEntity(left));
  hold<GuestVisits>(world, SHOP, left, 2);

  stepWorld(world);

  CHECK(unitsConsumed<GuestVisits>(world, ABANDONED_CAUSE) == 2);
  CHECK(unitsHeld<GuestVisits>(world) == 0);
  CHECK(unitsInTransit<GuestVisits>(world) == 0);
  CHECK(unitsCreated<Meals>(world) == 0);
  CHECK(unitsConsumed<Supplies>(world, SERVED_CAUSE) == 0);
}

TEST_CASE("A shop consumes as abandoned the visits it holds for a guest that is gone, and neither "
          "serves nor returns them") {
  SECTION("a supplied shop holding supplies") {
    World world = suppliedWorld();
    hold<Supplies>(world, SHOP, SHOP, 2);
    checkAbandons(std::move(world));
  }
  SECTION("a starved shop with nothing to cover them") { checkAbandons(starvedWorld()); }
}

TEST_CASE("A queued guest that leaves is never served, and the guest queued behind it is") {
  World world = suppliedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  const EntityKey left = guests[0];
  const EntityKey staying = guests[1];
  hold<GuestVisits>(world, SHOP, left, 1);
  hold<GuestVisits>(world, SHOP, staying, 1);
  stepWorld(world);
  REQUIRE(unitsCreated<Meals>(world) == 0);
  REQUIRE(world.destroyEntity(left));
  hold<Supplies>(world, SHOP, SHOP, 1);

  stepWorld(world);

  CHECK(unitsConsumed<GuestVisits>(world, ABANDONED_CAUSE) == 1);
  const std::vector<FlowPacket> meals = packetsFrom<Meals>(world, SHOP);
  REQUIRE(meals.size() == 1);
  CHECK(meals.front().To == staying);
}

// Deleted shops.

TEST_CASE("When a shop box is deleted, the visits it held reach their guests' stocks with no "
          "meal") {
  World world = suppliedWorld();
  const std::vector<EntityKey> guests = addGuests(world, 2);
  hold<GuestVisits>(world, SHOP, guests[0], 2);
  hold<GuestVisits>(world, SHOP, guests[1], 1);

  deleteShop(world);
  stepWorld(world);

  CHECK(unitsHeld<GuestVisits>(world, guests[0], guests[0]) == 2);
  CHECK(unitsHeld<GuestVisits>(world, guests[1], guests[1]) == 1);
  CHECK(unitsConsumed<GuestVisits>(world) == 0);
  CHECK(unitsCreated<Meals>(world) == 0);
}

TEST_CASE("When a shop box is deleted, the supplies it held are consumed as discarded") {
  World world = suppliedWorld();
  hold<Supplies>(world, SHOP, SHOP, 5);

  deleteShop(world);
  stepWorld(world);

  CHECK(unitsConsumed<Supplies>(world, DISCARDED_CAUSE) == 5);
  CHECK(unitsConsumed<Supplies>(world) == 5);
}

TEST_CASE("A visit and meal a shop sent to a guest that is gone come back to it and are consumed "
          "as abandoned") {
  World world = suppliedWorld();
  const EntityKey guest = world.createEntity();
  hold<GuestVisits>(world, SHOP, guest, 1);
  hold<Supplies>(world, SHOP, SHOP, 1);
  stepWorld(world);
  REQUIRE(unitsCreated<Meals>(world) == 1);
  REQUIRE(world.destroyEntity(guest));

  // They reach the gone guest at SERVICE_INTERVAL, come back taking as long again, and the shop
  // steps once more.
  stepUntil(world, (2 * SERVICE_INTERVAL) + 1);

  CHECK(unitsConsumed<GuestVisits>(world, ABANDONED_CAUSE) == 1);
  CHECK(unitsConsumed<Meals>(world, ABANDONED_CAUSE) == 1);
  CHECK(unitsInTransit<GuestVisits>(world) == 0);
  CHECK(unitsInTransit<Meals>(world) == 0);
}

TEST_CASE("A visit and meal a shop sent to a guest that is gone are consumed as undeliverable "
          "when the shop is gone too") {
  World world = suppliedWorld();
  const EntityKey guest = world.createEntity();
  hold<GuestVisits>(world, SHOP, guest, 1);
  hold<Supplies>(world, SHOP, SHOP, 1);
  stepWorld(world);
  REQUIRE(unitsCreated<Meals>(world) == 1);
  REQUIRE(world.destroyEntity(guest));
  deleteShop(world);

  stepUntil(world, SERVICE_INTERVAL + 1);

  CHECK(unitsConsumed<GuestVisits>(world, UNDELIVERABLE_CAUSE) == 1);
  CHECK(unitsConsumed<Meals>(world, UNDELIVERABLE_CAUSE) == 1);
}

} // namespace
} // namespace tpj
