#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::recordOf;

// Two entrances with doors at the ends of one guest path 40 m long, and a backstage path well away
// from them, a carrier on another network.
constexpr EntityKey WEST_GATE{1};
constexpr EntityKey EAST_GATE{2};
constexpr EntityKey WALK{3};
constexpr EntityKey BACKSTAGE{4};

constexpr double WALK_LENGTH = 40.0;

test::ParkIntent gatesIntent() {
  return test::ParkIntent{
      .Entrances = {test::northGate(WEST_GATE, -20.0), test::northGate(EAST_GATE, 20.0)},
      .Paths = {test::guestPath(WALK, {{-20.0, 123.0}, {20.0, 123.0}}),
                ParkPath{.Key = BACKSTAGE,
                         .Kind = PathKind::Backstage,
                         .Points = {{-20.0, 100.0}, {20.0, 100.0}}}},
      .Boxes = {}};
}

// A place strictly inside the guest path, a quarter of the way along it.
constexpr Place INSIDE_WALK{WALK, 10.0};

// A tick before the first arrival and above 0, so a draw made at the call's tick differs from one
// made at tick 0.
constexpr uint64_t ADD_TICK = 5;

// A stay that ends long after every test here has finished stepping.
constexpr uint64_t LONG_STAY = 100'000;

World worldAtAddTick() {
  World world = test::resolvedWorld(gatesIntent());
  test::stepUntil(world, ADD_TICK);
  return world;
}

TEST_CASE(
    "addGuest gives the key nextKey gave before the call, a wandering guest at the place with "
    "the stay given, no target, no meals, and no last meal") {
  World world = worldAtAddTick();
  const uint64_t next = world.nextKey();
  const EntityKey guest = addGuest(world, INSIDE_WALK, LONG_STAY);

  CHECK(guest == EntityKey{next});
  const GuestRecord record = recordOf(world, guest);
  CHECK(record.Activity == GuestActivity::Wandering);
  CHECK(record.At == INSIDE_WALK);
  CHECK(record.StayUntil == LONG_STAY);
  CHECK(record.Target == NULL_KEY);
  CHECK(record.MealsEaten == 0);
  CHECK_FALSE(record.LastMeal.has_value());
}

TEST_CASE("An added guest's starting hunger and hunger rate are drawn on its key with the world's "
          "seed and the tick at the call") {
  World world = worldAtAddTick();
  const EntityKey guest = addGuest(world, INSIDE_WALK, LONG_STAY);
  const double startingHunger = test::drawnStartingHunger(world, guest, ADD_TICK);
  CHECK(recordOf(world, guest).Hunger == startingHunger);

  // The record does not show the rate, but the guest's first step adds it to its hunger, and the
  // park has no shop to feed it.
  stepWorld(world);
  const double rate = test::drawnHungerRate(world, guest, ADD_TICK);
  CHECK(recordOf(world, guest).Hunger == std::min(1.0, startingHunger + rate));
}

TEST_CASE("addGuest throws std::invalid_argument for a place that does not resolve on the guest "
          "network, leaving the world and its next key unchanged") {
  SECTION("a place on a carrier of another network") {
    World world = worldAtAddTick();
    const World before = copyWorld(world);
    CHECK_THROWS_AS(addGuest(world, Place{BACKSTAGE, 10.0}, LONG_STAY), std::invalid_argument);
    CHECK(worldsEqual(world, before));
    CHECK(world.nextKey() == before.nextKey());
  }
  SECTION("a place beyond the end of a guest path") {
    World world = worldAtAddTick();
    const World before = copyWorld(world);
    CHECK_THROWS_AS(addGuest(world, Place{WALK, WALK_LENGTH + 1.0}, LONG_STAY),
                    std::invalid_argument);
    CHECK(worldsEqual(world, before));
    CHECK(world.nextKey() == before.nextKey());
  }
  SECTION("a place on a guest path before the world's first resolution, when the guest network is "
          "empty") {
    World world = test::worldOf(gatesIntent());
    REQUIRE(world.isResolvePending());
    const World before = copyWorld(world);
    CHECK_THROWS_AS(addGuest(world, INSIDE_WALK, LONG_STAY), std::invalid_argument);
    CHECK(worldsEqual(world, before));
    CHECK(world.nextKey() == before.nextKey());
  }
}

TEST_CASE("The guests a cycle admits have the records addGuest gives in a copy of the world taken "
          "before that cycle, called in key order with each guest's place and stay") {
  World world = test::resolvedWorld(gatesIntent());
  test::stepUntil(world, test::FIRST_ARRIVAL);
  World copy = copyWorld(world);
  const std::vector<EntityKey> before = parkGuests(world);
  stepWorld(world);

  std::vector<EntityKey> arrived;
  std::ranges::set_difference(parkGuests(world), before, std::back_inserter(arrived));
  // One guest at each entrance, so the order between arrivals is exercised.
  REQUIRE(arrived.size() == 2);
  for (const EntityKey guest : arrived) {
    CAPTURE(guest);
    const GuestRecord record = recordOf(world, guest);
    const EntityKey added = addGuest(copy, record.At, record.StayUntil);
    CHECK(added == guest);
    CHECK(guestRecord(copy, added) == std::optional<GuestRecord>(record));
  }
}

TEST_CASE("addGuest with the same arguments on two equal worlds leaves them equal") {
  World one = worldAtAddTick();
  World other = copyWorld(one);
  REQUIRE(worldsEqual(one, other));
  addGuest(one, INSIDE_WALK, LONG_STAY);
  addGuest(other, INSIDE_WALK, LONG_STAY);
  CHECK(worldsEqual(one, other));
}

TEST_CASE("A world with several added guests, two at one place and one at an entrance's node, is a "
          "working park: it steps, and every added guest walks in the next cycle") {
  World world = worldAtAddTick();
  const std::vector<Place> places = {INSIDE_WALK, INSIDE_WALK, test::gatePlace(world, WEST_GATE)};
  std::vector<EntityKey> guests;
  guests.reserve(places.size());
  for (const Place &place : places) {
    guests.push_back(addGuest(world, place, LONG_STAY));
  }

  stepWorld(world);
  CHECK_NOTHROW(validateWorld(world));
  for (std::size_t index = 0; index < guests.size(); ++index) {
    CAPTURE(index);
    const std::optional<GuestRecord> record = guestRecord(world, guests[index]);
    REQUIRE(record.has_value());
    CHECK(record.value_or(GuestRecord{}).At != places[index]);
  }
}

} // namespace
} // namespace tpj
