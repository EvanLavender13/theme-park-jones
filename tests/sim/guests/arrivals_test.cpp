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
#include <set>
#include <stdint.h>
#include <tuple>
#include <vector>

namespace tpj {
namespace {

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
      .Paths = {test::guestPath(WALK,
                                {{-20.0, test::northGateLineZ()}, {20.0, test::northGateLineZ()}})},
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

// The guests in the world that are not among the guests seen, which it adds to them.
std::vector<EntityKey> newGuests(const World &world, std::set<EntityKey> &seen) {
  std::vector<EntityKey> arrived;
  for (const EntityKey guest : parkGuests(world)) {
    if (seen.insert(guest).second) {
      arrived.push_back(guest);
    }
  }
  return arrived;
}

TEST_CASE("Every entrance with a guest connector admits guests alike, and one with none admits "
          "none") {
  World world = gatesWorld();
  const Place west = test::gatePlace(world, WEST_GATE);
  const Place east = test::gatePlace(world, EAST_GATE);
  std::set<EntityKey> seen;
  uint64_t westAdmitted = 0;
  uint64_t eastAdmitted = 0;
  // Two rounds of arrivals, so that admitting alike is shown beyond the first.
  while (westAdmitted < 2) {
    REQUIRE(world.Tick < 1000);
    stepWorld(world);
    CAPTURE(world.Tick);
    for (const EntityKey guest : newGuests(world, seen)) {
      // A guest first walks in the cycle after it arrives, so it stands where it was admitted.
      const Place at = recordOf(world, guest).At;
      CAPTURE(guest);
      CHECK((at == west || at == east));
      westAdmitted += at == west ? 1 : 0;
      eastAdmitted += at == east ? 1 : 0;
    }
    CHECK(westAdmitted == eastAdmitted);
  }
}

TEST_CASE("Each arrival stands wandering at the node place of its own entrance's lowest anchored "
          "node, on the ground there, with a stay of at least STAY_MIN and below STAY_MAX ticks "
          "from its arrival and a Hunger from 0 to STARTING_HUNGER_MAX") {
  World world = gatesWorld();
  std::set<EntityKey> seen;
  std::vector<EntityKey> arrived;
  uint64_t admitting = 0;
  while (arrived.empty()) {
    REQUIRE(world.Tick < 1000);
    admitting = world.Tick;
    stepWorld(world);
    arrived = newGuests(world, seen);
  }

  // One guest at each entrance with a guest connector, wherever entrance and key order put it.
  std::vector<Place> places;
  for (const EntityKey guest : arrived) {
    const GuestRecord record = recordOf(world, guest);
    CAPTURE(guest);
    places.push_back(record.At);
    CHECK(record.Activity == GuestActivity::Wandering);
    CHECK(record.Position == test::guestNetwork(world).groundPoint(record.At));
    CHECK(record.StayUntil >= admitting + STAY_MIN);
    CHECK(record.StayUntil < admitting + STAY_MAX);
    CHECK(record.Hunger >= 0.0);
    CHECK(record.Hunger <= STARTING_HUNGER_MAX);
  }
  std::vector<Place> gates{test::gatePlace(world, WEST_GATE), test::gatePlace(world, EAST_GATE)};
  const auto byPlace = [](const Place &left, const Place &right) {
    return std::tie(left.Carrier, left.Distance) < std::tie(right.Carrier, right.Distance);
  };
  std::ranges::sort(places, byPlace);
  std::ranges::sort(gates, byPlace);
  CHECK(places == gates);
}

} // namespace
} // namespace tpj
