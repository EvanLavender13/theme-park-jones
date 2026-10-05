// The food loop on a checked-in park: cutting the backstage segments of tests/parks/warm.park,
// which supply its shop, leaves its guests hungrier than the same park uncut. This compares a park
// with itself cut, so it is not a standard the sim's park world checks prove over every park file.

#include "support/park_files.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

// Far more cycles than the uncut park takes to serve a guest the cut one does not, so a cut that
// changes nothing fails rather than stepping for ever.
constexpr uint64_t LONGEST_RUN = 5000;

double meanHunger(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  REQUIRE_FALSE(guests.empty());
  double total = 0.0;
  for (const EntityKey guest : guests) {
    total += recordOf(world, guest).Hunger;
  }
  return total / static_cast<double>(guests.size());
}

// Each guest's MealsEaten, by key, for the guests of the world.
std::map<EntityKey, uint64_t> mealsEatenBy(const World &world) {
  std::map<EntityKey, uint64_t> eaten;
  for (const EntityKey guest : parkGuests(world)) {
    eaten.emplace(guest, recordOf(world, guest).MealsEaten);
  }
  return eaten;
}

// Adds to the set each guest of the world that has eaten more meals than when the run started, a
// guest that arrived since counting as having eaten none.
void addServed(const World &world, const std::map<EntityKey, uint64_t> &atStart,
               std::set<EntityKey> &served) {
  for (const auto &[guest, eaten] : mealsEatenBy(world)) {
    const auto start = atStart.find(guest);
    if (eaten > (start == atStart.end() ? 0 : start->second)) {
      served.insert(guest);
    }
  }
}

// Catches a cut that never reaches the guests: meals made with no supply, or guests fed from an
// offer the cut should have withdrawn.
TEST_CASE("Cutting a shop's supply route leaves guests hungrier than in the same park uncut") {
  World warm = openPark(parkFile("warm.park"));
  World cut = copyWorld(warm);
  CommandQueue cutting;
  for (const ParkSegment &segment : parkSegments(cut)) {
    if (segment.Kind == PathKind::Backstage) {
      cutting.push(DeleteSegment{segment.Key});
    }
  }
  REQUIRE_FALSE(cutting.empty());
  // Both step the cycle that applies the cut, so they stand at the same tick from then on.
  stepWorld(warm);
  stepWorld(cut, cutting);
  const std::vector<ParkSegment> left = parkSegments(cut);
  REQUIRE(std::ranges::none_of(
      left, [](const ParkSegment &segment) { return segment.Kind == PathKind::Backstage; }));

  const std::map<EntityKey, uint64_t> warmAtStart = mealsEatenBy(warm);
  const std::map<EntityKey, uint64_t> cutAtStart = mealsEatenBy(cut);
  std::set<EntityKey> servedInWarm;
  std::set<EntityKey> servedInCut;
  const uint64_t start = warm.Tick;
  const auto servedOnlyInWarm = [&]() {
    return std::ranges::any_of(servedInWarm,
                               [&](EntityKey guest) { return !servedInCut.contains(guest); });
  };
  while (!servedOnlyInWarm()) {
    REQUIRE(warm.Tick < start + LONGEST_RUN);
    stepWorld(warm);
    stepWorld(cut);
    addServed(warm, warmAtStart, servedInWarm);
    addServed(cut, cutAtStart, servedInCut);
  }
  REQUIRE(warm.Tick == cut.Tick);
  CHECK(meanHunger(cut) > meanHunger(warm));
}

} // namespace
} // namespace tpj
