#include "support/route_edits.h"

#include "sim/command_queue.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <stdint.h>
#include <string>

namespace tpj {
namespace {

std::string routesText() {
  std::ifstream file(TPJ_PARKS_DIR "/routes.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

constexpr int EDITS = 60;
// Each edit is followed by fewer cycles than this with no command, so orders reach depots and
// shipments reach shops between edits.
constexpr uint64_t IDLE_CYCLES = 20;

// Runs an edit sequence from tests/parks/routes.park, whose shop and depot are joined by a
// backstage path, so orders flow from the start. Each edit is applied by one cycle, followed by a
// drawn number of cycles with no command. afterCycle sees the resolved starting world and the
// world after every cycle, with whether that cycle applied an edit.
template <typename AfterCycle> void runEditSequence(uint64_t seed, AfterCycle afterCycle) {
  test::RouteEditDraws draws(seed);
  World world = loadWorld(makeParkSchema(), routesText());
  REQUIRE_NOTHROW(resolveWorld(world));
  afterCycle(world, true);
  for (int edit = 0; edit < EDITS; ++edit) {
    INFO("edit " << edit);
    CommandQueue queue;
    queueEdit(queue, test::routeEdit(draws, world));
    REQUIRE_NOTHROW(stepWorld(world, queue));
    afterCycle(world, true);
    const uint64_t idle = draws.below(IDLE_CYCLES);
    for (uint64_t cycle = 0; cycle < idle; ++cycle) {
      INFO("tick " << world.Tick);
      REQUIRE_NOTHROW(stepWorld(world));
      afterCycle(world, false);
    }
  }
}

template <FlowDefinition K> bool conserved(const World &world) {
  return unitsCreated<K>(world) ==
         unitsInTransit<K>(world) + unitsHeld<K>(world) + unitsConsumed<K>(world);
}

TEST_CASE("In every tick of randomized park edits, supply-orders, supplies, guest-visits, and "
          "meals each satisfy the ledger's identity") {
  bool sawOrders = false;
  bool sawSupplies = false;
  runEditSequence(51, [&](const World &world, bool /*edited*/) {
    CHECK(conserved<SupplyOrders>(world));
    CHECK(conserved<Supplies>(world));
    CHECK(conserved<GuestVisits>(world));
    CHECK(conserved<Meals>(world));
    sawOrders = sawOrders || unitsCreated<SupplyOrders>(world) > 0;
    sawSupplies = sawSupplies || unitsCreated<Supplies>(world) > 0;
  });
  CHECK(sawOrders);
  CHECK(sawSupplies);
}

TEST_CASE("In every tick of randomized park edits, consumed order units carry only the causes "
          "fulfilled, unfilled, cancelled, undeliverable, and discarded, and consumed supplies "
          "only returned, undeliverable, and discarded") {
  bool sawConsumedOrders = false;
  runEditSequence(52, [&](const World &world, bool /*edited*/) {
    CHECK(unitsConsumed<SupplyOrders>(world) ==
          unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE) +
              unitsConsumed<SupplyOrders>(world, UNFILLED_CAUSE) +
              unitsConsumed<SupplyOrders>(world, CANCELLED_CAUSE) +
              unitsConsumed<SupplyOrders>(world, UNDELIVERABLE_CAUSE) +
              unitsConsumed<SupplyOrders>(world, DISCARDED_CAUSE));
    CHECK(unitsConsumed<Supplies>(world) ==
          unitsConsumed<Supplies>(world, RETURNED_CAUSE) +
              unitsConsumed<Supplies>(world, UNDELIVERABLE_CAUSE) +
              unitsConsumed<Supplies>(world, DISCARDED_CAUSE));
    sawConsumedOrders = sawConsumedOrders || unitsConsumed<SupplyOrders>(world) > 0;
  });
  CHECK(sawConsumedOrders);
}

TEST_CASE("In every tick of randomized park edits, the supplies created equal the order units "
          "consumed as fulfilled") {
  bool sawFulfilled = false;
  runEditSequence(53, [&](const World &world, bool /*edited*/) {
    CHECK(unitsCreated<Supplies>(world) == unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE));
    sawFulfilled = sawFulfilled || unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE) > 0;
  });
  CHECK(sawFulfilled);
}

TEST_CASE("In every tick of randomized park edits, no shop's inventoryPosition exceeds "
          "ORDER_UP_TO") {
  bool sawStocked = false;
  runEditSequence(54, [&](const World &world, bool /*edited*/) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Kind == BoxKind::Shop) {
        const int64_t position = inventoryPosition(world, box.Key);
        CAPTURE(box.Key, position);
        CHECK(position <= ORDER_UP_TO);
        sawStocked = sawStocked || position > REORDER_POINT;
      }
    }
  });
  CHECK(sawStocked);
}

TEST_CASE("No step or edit of a randomized park edit sequence throws, whether shops are supplied "
          "or starved") {
  bool sawSupplied = false;
  bool sawStarved = false;
  runEditSequence(55, [&](const World &world, bool /*edited*/) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Kind == BoxKind::Shop) {
        const bool supplied = nearestDepot(world, box.Key).has_value();
        sawSupplied = sawSupplied || supplied;
        sawStarved = sawStarved || !supplied;
      }
    }
  });
  CHECK(sawSupplied);
  CHECK(sawStarved);
}

TEST_CASE("Every world a randomized park edit sequence reaches equals its save loaded and "
          "resolved, and the two stay equal as they step on") {
  std::optional<World> loaded;
  bool sawInTransit = false;
  runEditSequence(56, [&](const World &world, bool edited) {
    if (edited) {
      loaded.emplace(loadWorld(makeParkSchema(), saveWorld(world)));
      resolveWorld(loaded.value());
    } else {
      stepWorld(loaded.value());
    }
    REQUIRE(worldsEqual(loaded.value(), world));
    sawInTransit = sawInTransit || unitsInTransit<SupplyOrders>(world) > 0 ||
                   unitsInTransit<Supplies>(world) > 0;
  });
  CHECK(sawInTransit);
}

} // namespace
} // namespace tpj
