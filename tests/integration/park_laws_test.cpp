// Laws that hold with every capability running together, checked over every park file in
// tests/parks/. Each module's tests prove its laws on synthetic worlds; these run the park as the
// player and the cross-build check see it, with intent, networks, shops, depots, and guests all
// interacting, and a park checked in to tests/parks/ is covered with no code change.

#include "support/park_files.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::ParkFile;
using test::parkFiles;

// Ticks each determinism run steps every park: long enough for a guest of warm.park to eat.
constexpr uint64_t DETERMINISM_TICKS = 60;

// Catches a park type whose text does not read back as it was written, such as a guest's last
// choice, a ledger's causes, or a field's stepped entries, which no synthetic save holds.
TEST_CASE("Every park file loads and saves back to identical text") {
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    CHECK(saveWorld(loadWorld(makeParkSchema(), park.Text)) == park.Text);
  }
}

// Whether the world holds supplies in transit to a shop that has no supply route, as a shipment
// does when its backstage path is deleted on the way.
bool holdsStrandedShipment(const World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind != BoxKind::Shop || nearestDepot(world, box.Key).has_value()) {
      continue;
    }
    for (const FlowPacket &packet : addressedTo<Supplies>(world, box.Key).Packets) {
      if (packet.To == box.Key) {
        return true;
      }
    }
  }
  return false;
}

// Catches state a module keeps that no save holds, and a resolver or finisher that changes state
// when a park is reopened, such as one that carries guests, clears stepped offers, or drops
// shipments whose route is gone, so the reopened park differs from the one saved.
TEST_CASE("A world stepped from every park file equals the world regenerated from its save, "
          "shipments in transit to a shop with no supply route included") {
  bool sawStrandedShipment = false;
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    World world = openPark(park);
    // A world the cycle has stepped, not one a load has just resolved.
    stepWorld(world);
    const World regenerated = openPark(saveWorld(world));
    CHECK(worldsEqual(regenerated, world));
    sawStrandedShipment = sawStrandedShipment || holdsStrandedShipment(world);
  }
  CHECK(sawStrandedShipment);
}

// Catches a result that depends on something outside the world: an uninitialized value, a
// container ordered by address, or a cache one module keeps between worlds, which two worlds
// stepped side by side in one process would not share.
TEST_CASE("Two runs of every park file, stepped side by side, end with the same state hash") {
  bool sawGuestsEat = false;
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    World first = openPark(park);
    World second = openPark(park);
    const int64_t eatenAtStart = unitsConsumed<Meals>(first, EATEN_CAUSE);
    for (uint64_t cycle = 0; cycle < DETERMINISM_TICKS; ++cycle) {
      stepWorld(first);
      stepWorld(second);
    }
    CHECK(hashWorld(first) == hashWorld(second));
    sawGuestsEat = sawGuestsEat || unitsConsumed<Meals>(first, EATEN_CAUSE) > eatenAtStart;
  }
  CHECK(sawGuestsEat);
}

} // namespace
} // namespace tpj
