// A toilet block relieves its park's guests, so taking it away sends them home sooner and more
// often for the toilet.

#include "support/park_files.h"

#include "sim/clock.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

// What a park's guests did over a day: the departure tally, and how many guests left and the sum
// of their stays in ticks.
struct DayOut {
  DepartureTally Tally;
  size_t Left = 0;
  uint64_t TotalStay = 0;
};

double meanStay(const DayOut &day) {
  REQUIRE(day.Left > 0);
  return static_cast<double>(day.TotalStay) / static_cast<double>(day.Left);
}

uint64_t leftFor(const DayOut &day, HomeReason reason) {
  return day.Tally.ByReason.at(static_cast<size_t>(reason));
}

bool isOpenNow(const World &world, const OpeningHours &hours) {
  return isOpenAt(hours, parkTime(world.Tick).Second);
}

// Steps an open park until it has closed and every guest has left, recording each guest's stay
// from the tick it arrived to the tick it is gone. The park reopening first fails the run, since
// every guest of a day leaves before the next opening.
DayOut runDay(World world) {
  const OpeningHours hours = parkHours(world);
  REQUIRE(isOpenNow(world, hours));
  DayOut day;
  std::map<EntityKey, uint64_t> arrivedAt;
  for (const EntityKey guest : parkGuests(world)) {
    arrivedAt.emplace(guest, recordOf(world, guest).Arrived);
  }
  bool closed = false;
  while (!closed || !arrivedAt.empty()) {
    stepWorld(world);
    const bool open = isOpenNow(world, hours);
    REQUIRE_FALSE((closed && open));
    closed = closed || !open;

    const std::vector<EntityKey> guests = parkGuests(world);
    const std::set<EntityKey> present(guests.begin(), guests.end());
    for (auto it = arrivedAt.begin(); it != arrivedAt.end();) {
      if (present.contains(it->first)) {
        ++it;
        continue;
      }
      day.TotalStay += world.Tick - it->second;
      ++day.Left;
      it = arrivedAt.erase(it);
    }
    for (const EntityKey guest : guests) {
      if (!arrivedAt.contains(guest)) {
        arrivedAt.emplace(guest, recordOf(world, guest).Arrived);
      }
    }
  }
  day.Tally = departureTally(world);
  return day;
}

double meanBladder(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  REQUIRE_FALSE(guests.empty());
  double total = 0.0;
  for (const EntityKey guest : guests) {
    total += recordOf(world, guest).Bladder;
  }
  return total / static_cast<double>(guests.size());
}

TEST_CASE("Removing the toilet block makes more guests leave for the toilet over a day") {
  const DayOut withToilet = runDay(openPark(parkFile("day.park")));
  const DayOut withoutToilet = runDay(openPark(parkFile("no-toilet.park")));
  CHECK(leftFor(withoutToilet, HomeReason::Toilet) > leftFor(withToilet, HomeReason::Toilet));
}

TEST_CASE("Removing the toilet block shortens guests' stays over a day") {
  const DayOut withToilet = runDay(openPark(parkFile("day.park")));
  const DayOut withoutToilet = runDay(openPark(parkFile("no-toilet.park")));
  CHECK(meanStay(withoutToilet) < meanStay(withToilet));
}

TEST_CASE("A toilet block leaves its park's guests with a lower mean bladder at closing") {
  World withToilet = openPark(parkFile("day.park"));
  World withoutToilet = openPark(parkFile("no-toilet.park"));
  REQUIRE(withToilet.Tick == withoutToilet.Tick);
  const OpeningHours hours = parkHours(withToilet);
  REQUIRE(parkHours(withoutToilet) == hours);
  REQUIRE(isOpenNow(withToilet, hours));
  // A hang guard only: an open park closes within a day.
  const uint64_t start = withToilet.Tick;
  while (isOpenNow(withToilet, hours)) {
    REQUIRE(withToilet.Tick < start + DAY_TICKS);
    stepWorld(withToilet);
    stepWorld(withoutToilet);
  }
  CHECK(meanBladder(withToilet) < meanBladder(withoutToilet));
}

} // namespace
} // namespace tpj
