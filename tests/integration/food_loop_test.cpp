// The food loop on the checked-in parks: tests/parks/fed.park feeds its guests, and cutting the
// backstage path, which turns warm.park into cut.park, withdraws the shop's offer, stops guests
// picking it, empties its queue, and leaves guests hungrier. These name parks and compare runs, so
// they are not standards the sim's park world checks prove over every park file.

#include "support/park_files.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

// Ticks the fed park runs: past its first meal, which waits for its shop's first shipment.
// Ticks cut.park's offer is watched: its stepped offer is republished every tick.
// Ticks warm.park and cut.park each run when their picks are compared: long enough for a guest of
// warm.park to pick its shop.
// The most ticks cut.park runs for its queue to empty, one guest per SERVICE_INTERVAL.
// Ticks warm.park and cut.park each run, from the same tick, when their guests' hunger is
// compared: long enough for warm.park to serve a guest that cut.park does not.
constexpr uint64_t HUNGER_TICKS = 540;

World openParkFile(std::string_view name) { return openPark(parkFile(name)); }

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
    // Every key parkGuests gives holds a guest, so it has a record.
    eaten.emplace(guest, guestRecord(world, guest).value_or(GuestRecord{}).MealsEaten);
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
TEST_CASE("Once warm.park has served a guest that cut.park does not, mean guest hunger is higher "
          "in cut.park than in warm.park at the same tick") {
  World warm = openParkFile("warm.park");
  World cut = openParkFile("cut.park");
  // cut.park is warm.park after one more cycle, so warm.park steps that cycle first and both end
  // at the same tick, where cut.park's guests have not had a tick longer to grow hungry.
  stepWorld(warm);
  REQUIRE(warm.Tick == cut.Tick);
  const std::map<EntityKey, uint64_t> warmAtStart = mealsEatenBy(warm);
  const std::map<EntityKey, uint64_t> cutAtStart = mealsEatenBy(cut);
  std::set<EntityKey> servedInWarm;
  std::set<EntityKey> servedInCut;
  for (uint64_t cycle = 0; cycle < HUNGER_TICKS; ++cycle) {
    stepWorld(warm);
    stepWorld(cut);
    addServed(warm, warmAtStart, servedInWarm);
    addServed(cut, cutAtStart, servedInCut);
  }
  REQUIRE(warm.Tick == cut.Tick);
  CHECK(std::ranges::any_of(servedInWarm,
                            [&](EntityKey guest) { return !servedInCut.contains(guest); }));
  CHECK(meanHunger(cut) > meanHunger(warm));
}

} // namespace
} // namespace tpj
