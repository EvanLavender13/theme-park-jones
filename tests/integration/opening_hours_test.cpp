// Guests arrive only while the park is open, and all of them leave before it opens again.

#include "support/park_files.h"

#include "sim/clock.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

bool isOpenNow(const World &world, const OpeningHours &hours) {
  return isOpenAt(hours, parkTime(world.Tick).Second);
}

TEST_CASE("Opening hours bound the day: guests arrive only while open and leave before reopening") {
  World world = openPark(parkFile("day.park"));
  const OpeningHours hours = parkHours(world);
  REQUIRE(isOpenNow(world, hours));
  REQUIRE(parkGuests(world).empty());

  std::set<EntityKey> admitted;
  bool arrivedWhileClosed = false;
  bool closed = false;
  // Steps through the first day and the night after it, up to the moment the park opens again.
  while (!closed || !isOpenNow(world, hours)) {
    // A hang guard only: the park reopens on the second day.
    REQUIRE(parkTime(world.Tick).Day <= 2);
    stepWorld(world);
    closed = closed || !isOpenNow(world, hours);
    for (const EntityKey guest : parkGuests(world)) {
      if (admitted.insert(guest).second &&
          !isOpenAt(hours, parkTime(recordOf(world, guest).Arrived).Second)) {
        arrivedWhileClosed = true;
      }
    }
  }
  REQUIRE_FALSE(admitted.empty());
  CHECK_FALSE(arrivedWhileClosed);
  CHECK(parkGuests(world).empty());

  const uint64_t reopened = world.Tick;
  while (parkGuests(world).empty()) {
    // A hang guard only: guests arrive while the reopened park is open.
    REQUIRE(isOpenNow(world, hours));
    stepWorld(world);
  }
  for (const EntityKey guest : parkGuests(world)) {
    CHECK_FALSE(admitted.contains(guest));
    CHECK(recordOf(world, guest).Arrived >= reopened);
  }
}

} // namespace
} // namespace tpj
