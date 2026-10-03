#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/guests/footfall.h"
#include "sim/guests/guests.h"
#include "sim/medium/kept_field.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::FIRST_ARRIVAL;
using test::recordOf;

// Two entrances with doors on one guest path, and between them in key order an entrance far from
// any path, whose door has no connector.
constexpr EntityKey WEST_GATE{1};
constexpr EntityKey LOOSE_GATE{2};
constexpr EntityKey EAST_GATE{3};
constexpr EntityKey WALK{4};

World gatesWorld() {
  return test::resolvedWorld(test::ParkIntent{
      .Entrances = {test::northGate(WEST_GATE, -20.0), test::northGate(LOOSE_GATE, -100.0),
                    test::northGate(EAST_GATE, 20.0)},
      .Paths = {test::guestPath(WALK, {{-20.0, 123.0}, {20.0, 123.0}})},
      .Boxes = {}});
}

EntityKey keyAt(uint64_t value) { return EntityKey{value}; }

TEST_CASE("parkGuests gives the guests' keys ascending, and guestRecord gives a record exactly for "
          "those keys") {
  World world = gatesWorld();
  test::stepUntil(world, (2 * ARRIVAL_INTERVAL) + 1);
  const std::vector<EntityKey> guests = parkGuests(world);
  REQUIRE(guests.size() == 4);
  CHECK(std::ranges::is_sorted(guests));
  CHECK(std::ranges::adjacent_find(guests) == guests.end());
  for (const EntityKey key : world.keys()) {
    CAPTURE(key);
    CHECK(guestRecord(world, key).has_value() == (std::ranges::find(guests, key) != guests.end()));
  }
  CHECK_FALSE(guestRecord(world, NULL_KEY).has_value());
  CHECK_FALSE(guestRecord(world, keyAt(world.nextKey())).has_value());
}

TEST_CASE("Guests arrive exactly in the cycles stepping a tick one below a multiple of "
          "ARRIVAL_INTERVAL, one at each entrance anchoring a node of the guest network") {
  World world = gatesWorld();
  // The loose entrance anchors no node, so each arrival cycle admits two guests, not three.
  for (uint64_t cycle = 0; cycle < (2 * ARRIVAL_INTERVAL) + 10; ++cycle) {
    const uint64_t stepped = world.Tick;
    stepWorld(world);
    CAPTURE(stepped);
    CHECK(parkGuests(world).size() == 2 * ((stepped + 1) / ARRIVAL_INTERVAL));
  }
}

TEST_CASE("A cycle's arrivals take keys from the counter in entrance key order, each at the node "
          "place of its entrance's lowest anchored node") {
  World world = gatesWorld();
  for (const uint64_t arrival : {FIRST_ARRIVAL, FIRST_ARRIVAL + ARRIVAL_INTERVAL}) {
    CAPTURE(arrival);
    test::stepUntil(world, arrival);
    const std::vector<EntityKey> before = parkGuests(world);
    const uint64_t next = world.nextKey();
    stepWorld(world);

    std::vector<EntityKey> expected = before;
    expected.push_back(keyAt(next));
    expected.push_back(keyAt(next + 1));
    CHECK(parkGuests(world) == expected);
    CHECK(world.nextKey() == next + 2);
    CHECK(recordOf(world, keyAt(next)).At == test::gatePlace(world, WEST_GATE));
    CHECK(recordOf(world, keyAt(next + 1)).At == test::gatePlace(world, EAST_GATE));
  }
}

TEST_CASE("A new guest's record shows it wandering at its entrance's node place, on the ground "
          "there, with the StayUntil and Hunger drawn from its key and arrival tick, no target, no "
          "meals, and no last meal") {
  World world = gatesWorld();
  test::stepUntil(world, FIRST_ARRIVAL);
  const EntityKey guest = keyAt(world.nextKey());
  stepWorld(world);

  const GuestRecord record = recordOf(world, guest);
  const Place place = test::gatePlace(world, WEST_GATE);
  CHECK(record.Activity == GuestActivity::Wandering);
  CHECK(record.At == place);
  CHECK(record.Position == test::guestNetwork(world).groundPoint(place));
  CHECK(record.StayUntil == test::drawnStayUntil(world, guest, FIRST_ARRIVAL));
  CHECK(record.Hunger == test::drawnStartingHunger(world, guest, FIRST_ARRIVAL));
  CHECK(record.Target == NULL_KEY);
  CHECK(record.MealsEaten == 0);
  CHECK_FALSE(record.LastMeal.has_value());
}

} // namespace
} // namespace tpj
