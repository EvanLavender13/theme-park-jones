#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <stdint.h>

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

} // namespace
} // namespace tpj
