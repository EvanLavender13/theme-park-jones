#include "support/ledger_writes.h"
#include "support/park_worlds.h"
#include "support/route_edits.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::carry;
using test::conserved;

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
// A shipment over routes.park's supply route takes about 500 ticks, so guests take their turns
// this long before the first edit, and shops hold stock and serve while the edits disrupt them.
constexpr uint64_t WARM_UP_CYCLES = 600;
// Before each cycle a guest appears at a chance of 1 in GUEST_ARRIVAL, far faster than a shop
// serves, so queues grow. Each guest leaves at a chance of 1 in GUEST_STAY, so some leave while
// queued or while their visit is on its way.
constexpr uint64_t GUEST_ARRIVAL = 6;
constexpr uint64_t GUEST_STAY = 120;
constexpr uint64_t WALK_TICKS = 30;

// What the synthetic guests do between two cycles: some leave, and a new guest may send one visit
// to a shop box, arriving after a walk.
struct GuestTurn {
  std::vector<EntityKey> Leaving;
  // Each new guest's shop and walking delay.
  std::vector<std::pair<EntityKey, uint32_t>> Visits;
};

// A cycle of the sequence: the guests' turn before it, and the edit it applies, if any.
struct Cycle {
  GuestTurn Turn;
  std::optional<ParkEdit> Edit;
};

GuestTurn drawTurn(test::RouteEditDraws &draws, const World &world,
                   const std::vector<EntityKey> &guests) {
  GuestTurn turn;
  for (const EntityKey guest : guests) {
    if (draws.oneIn(GUEST_STAY)) {
      turn.Leaving.push_back(guest);
    }
  }
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  if (!shops.empty() && draws.oneIn(GUEST_ARRIVAL)) {
    const EntityKey shop = shops[draws.below(shops.size())];
    turn.Visits.emplace_back(shop, static_cast<uint32_t>(1 + draws.below(WALK_TICKS)));
  }
  return turn;
}

// Applies the turn to the world between cycles and gives the guests it created. A guest is an
// entity with no component, and its visit is a packet written into the ledger as a guest's system
// would send it.
std::vector<EntityKey> takeTurn(World &world, const GuestTurn &turn) {
  for (const EntityKey guest : turn.Leaving) {
    REQUIRE(world.destroyEntity(guest));
  }
  std::vector<EntityKey> arrived;
  for (const auto &[shop, delay] : turn.Visits) {
    const EntityKey guest = world.createEntity();
    carry<GuestVisits>(world, FlowPacket{.Arrival = world.Tick + delay,
                                         .From = guest,
                                         .To = shop,
                                         .Handle = guest,
                                         .Units = 1,
                                         .Delay = delay,
                                         .Returning = false});
    arrived.push_back(guest);
  }
  return arrived;
}

// Runs an edit sequence from tests/parks/routes.park, whose shop and depot are joined by a
// backstage path, so orders flow from the start, with synthetic guests taking a turn before every
// cycle. The sequence first warms up with WARM_UP_CYCLES cycles of guests and no edits, then
// applies each edit by one cycle, followed by a drawn number of cycles with no command.
// beforeCycle sees the world after the guests' turn and the cycle about to step it. afterCycle
// sees the resolved starting world and the world after every cycle, with whether that cycle
// applied an edit.
template <typename BeforeCycle, typename AfterCycle>
void runServiceSequence(uint64_t seed, BeforeCycle beforeCycle, AfterCycle afterCycle) {
  test::RouteEditDraws draws(seed);
  // The guests draw apart from the edits, so the edit sequence is the one the seed gives without
  // them.
  test::RouteEditDraws guestDraws(~seed);
  World world = loadWorld(makeParkSchema(), routesText());
  REQUIRE_NOTHROW(resolveWorld(world));
  afterCycle(world, true);
  std::vector<EntityKey> guests;
  // One cycle: the guests' turn, then a step applying a drawn edit when edited.
  const auto runCycle = [&](bool edited) {
    INFO("tick " << world.Tick);
    Cycle next{.Turn = drawTurn(guestDraws, world, guests), .Edit = std::nullopt};
    std::erase_if(guests, [&](EntityKey guest) {
      return std::ranges::find(next.Turn.Leaving, guest) != next.Turn.Leaving.end();
    });
    for (const EntityKey guest : takeTurn(world, next.Turn)) {
      guests.push_back(guest);
    }
    CommandQueue queue;
    if (edited) {
      next.Edit = test::routeEdit(draws, world);
      queueEdit(queue, next.Edit.value());
    }
    beforeCycle(world, next);
    REQUIRE_NOTHROW(stepWorld(world, queue));
    afterCycle(world, edited);
  };
  for (uint64_t cycle = 0; cycle < WARM_UP_CYCLES; ++cycle) {
    runCycle(false);
  }
  for (int edit = 0; edit < EDITS; ++edit) {
    INFO("edit " << edit);
    runCycle(true);
    const uint64_t idle = draws.below(IDLE_CYCLES);
    for (uint64_t cycle = 0; cycle < idle; ++cycle) {
      runCycle(false);
    }
  }
}

template <typename AfterCycle> void runServiceSequence(uint64_t seed, AfterCycle afterCycle) {
  runServiceSequence(seed, [](const World & /*world*/, const Cycle & /*cycle*/) {}, afterCycle);
}

// Whether every meals packet a shop sent has a guest-visits packet the same shop sent to the same
// guest, with the same handle and arrival. A returning packet was sent back by the ledger, not by
// a shop.
bool mealsTravelWithVisits(const World &world) {
  const Ledger *meals = ledgerOf<Meals>(world);
  const Ledger *visits = ledgerOf<GuestVisits>(world);
  REQUIRE(meals != nullptr);
  REQUIRE(visits != nullptr);
  return std::ranges::all_of(meals->Packets, [&](const FlowPacket &meal) {
    return meal.Returning || std::ranges::any_of(visits->Packets, [&](const FlowPacket &visit) {
             return !visit.Returning && visit.From == meal.From && visit.To == meal.To &&
                    visit.Handle == meal.Handle && visit.Arrival == meal.Arrival;
           });
  });
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, supply-orders, supplies, "
          "guest-visits, and meals each satisfy the ledger's identity") {
  bool sawOrders = false;
  bool sawSupplies = false;
  bool sawMeals = false;
  runServiceSequence(51, [&](const World &world, bool /*edited*/) {
    CHECK(conserved<SupplyOrders>(world));
    CHECK(conserved<Supplies>(world));
    CHECK(conserved<GuestVisits>(world));
    CHECK(conserved<Meals>(world));
    sawOrders = sawOrders || unitsCreated<SupplyOrders>(world) > 0;
    sawSupplies = sawSupplies || unitsCreated<Supplies>(world) > 0;
    sawMeals = sawMeals || unitsCreated<Meals>(world) > 0;
  });
  CHECK(sawOrders);
  CHECK(sawSupplies);
  CHECK(sawMeals);
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, consumed order units "
          "carry only the causes fulfilled, unfilled, cancelled, undeliverable, and discarded, "
          "consumed supplies only returned, served, undeliverable, and discarded, and consumed "
          "visits and meals only abandoned, undeliverable, and discarded") {
  bool sawConsumedOrders = false;
  bool sawServed = false;
  bool sawAbandoned = false;
  runServiceSequence(52, [&](const World &world, bool /*edited*/) {
    CHECK(unitsConsumed<SupplyOrders>(world) ==
          unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE) +
              unitsConsumed<SupplyOrders>(world, UNFILLED_CAUSE) +
              unitsConsumed<SupplyOrders>(world, CANCELLED_CAUSE) +
              unitsConsumed<SupplyOrders>(world, UNDELIVERABLE_CAUSE) +
              unitsConsumed<SupplyOrders>(world, DISCARDED_CAUSE));
    CHECK(unitsConsumed<Supplies>(world) ==
          unitsConsumed<Supplies>(world, RETURNED_CAUSE) +
              unitsConsumed<Supplies>(world, SERVED_CAUSE) +
              unitsConsumed<Supplies>(world, UNDELIVERABLE_CAUSE) +
              unitsConsumed<Supplies>(world, DISCARDED_CAUSE));
    CHECK(unitsConsumed<GuestVisits>(world) ==
          unitsConsumed<GuestVisits>(world, ABANDONED_CAUSE) +
              unitsConsumed<GuestVisits>(world, UNDELIVERABLE_CAUSE) +
              unitsConsumed<GuestVisits>(world, DISCARDED_CAUSE));
    CHECK(unitsConsumed<Meals>(world) == unitsConsumed<Meals>(world, ABANDONED_CAUSE) +
                                             unitsConsumed<Meals>(world, UNDELIVERABLE_CAUSE) +
                                             unitsConsumed<Meals>(world, DISCARDED_CAUSE));
    sawConsumedOrders = sawConsumedOrders || unitsConsumed<SupplyOrders>(world) > 0;
    sawServed = sawServed || unitsConsumed<Supplies>(world, SERVED_CAUSE) > 0;
    sawAbandoned = sawAbandoned || unitsConsumed<GuestVisits>(world, ABANDONED_CAUSE) > 0;
  });
  CHECK(sawConsumedOrders);
  CHECK(sawServed);
  CHECK(sawAbandoned);
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, the supplies created "
          "equal the order units consumed as fulfilled") {
  bool sawFulfilled = false;
  runServiceSequence(53, [&](const World &world, bool /*edited*/) {
    CHECK(unitsCreated<Supplies>(world) == unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE));
    sawFulfilled = sawFulfilled || unitsConsumed<SupplyOrders>(world, FULFILLED_CAUSE) > 0;
  });
  CHECK(sawFulfilled);
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, the meals created equal "
          "the supplies consumed as served") {
  bool sawServed = false;
  runServiceSequence(57, [&](const World &world, bool /*edited*/) {
    CHECK(unitsCreated<Meals>(world) == unitsConsumed<Supplies>(world, SERVED_CAUSE));
    sawServed = sawServed || unitsConsumed<Supplies>(world, SERVED_CAUSE) > 0;
  });
  CHECK(sawServed);
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, every meals packet a shop "
          "sent travels with a visit the shop sent to the same guest, arriving with it") {
  bool sawMealInTransit = false;
  runServiceSequence(58, [&](const World &world, bool /*edited*/) {
    CHECK(mealsTravelWithVisits(world));
    sawMealInTransit = sawMealInTransit || unitsInTransit<Meals>(world) > 0;
  });
  CHECK(sawMealInTransit);
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, no shop's "
          "inventoryPosition exceeds ORDER_UP_TO") {
  bool sawStocked = false;
  runServiceSequence(54, [&](const World &world, bool /*edited*/) {
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

TEST_CASE("No step or edit of a randomized park edit sequence with synthetic guests throws, "
          "whether shops are supplied or starved") {
  bool sawSupplied = false;
  bool sawStarved = false;
  bool sawQueue = false;
  runServiceSequence(55, [&](const World &world, bool /*edited*/) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Kind == BoxKind::Shop) {
        const bool supplied = nearestDepot(world, box.Key).has_value();
        sawSupplied = sawSupplied || supplied;
        sawStarved = sawStarved || !supplied;
        sawQueue = sawQueue || stockOf<GuestVisits>(world, box.Key).size() > 1;
      }
    }
  });
  CHECK(sawSupplied);
  CHECK(sawStarved);
  CHECK(sawQueue);
}

TEST_CASE("In every tick of randomized park edits with synthetic guests, sampling food-offer at a "
          "shop's guest anchor gives one entry, from that shop, supplied exactly when the shop has "
          "a nearest depot") {
  bool sawSupplied = false;
  bool sawStarved = false;
  runServiceSequence(60, [&](const World &world, bool /*edited*/) {
    const Network &network = parkNetwork(world, PathKind::Guest);
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Kind != BoxKind::Shop) {
        continue;
      }
      for (const uint32_t node : network.anchoredNodes(box.Key)) {
        const std::vector<SampledEntry<OfferEntry>> sampled =
            sampleField<FoodOffer>(world, network, network.nodePlace(node));
        const bool supplied = nearestDepot(world, box.Key).has_value();
        CAPTURE(box.Key, supplied);
        REQUIRE(sampled.size() == 1);
        CHECK(sampled.front().Source == box.Key);
        CHECK(sampled.front().Value.Supplied == supplied);
        sawSupplied = sawSupplied || supplied;
        sawStarved = sawStarved || !supplied;
      }
    }
  });
  CHECK(sawSupplied);
  CHECK(sawStarved);
}

// A starved mark drawn from intent alone is right only if Starved depends on nothing else.
TEST_CASE("In every tick of randomized park edits with synthetic guests, every shop box has a "
          "record, whose Starved equals Starved for its key in a new world with the same intent, "
          "once resolved") {
  // The new world depends on intent alone, so it is rebuilt only when an edit may have changed it.
  std::optional<World> fresh;
  bool sawSupplied = false;
  bool sawStarved = false;
  runServiceSequence(61, [&](const World &world, bool edited) {
    if (edited || !fresh.has_value()) {
      fresh.emplace(test::worldOf(test::intentOf(world)));
      resolveWorld(fresh.value());
    }
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Kind != BoxKind::Shop) {
        continue;
      }
      CAPTURE(box.Key);
      const std::optional<ShopRecord> record = shopRecord(world, box.Key);
      const std::optional<ShopRecord> expected = shopRecord(fresh.value(), box.Key);
      REQUIRE(record.has_value());
      REQUIRE(expected.has_value());
      CHECK(record.value_or(ShopRecord{}).Starved == expected.value_or(ShopRecord{}).Starved);
      sawSupplied = sawSupplied || !record.value_or(ShopRecord{}).Starved;
      sawStarved = sawStarved || record.value_or(ShopRecord{}).Starved;
    }
  });
  CHECK(sawSupplied);
  CHECK(sawStarved);
}

TEST_CASE("Every world a randomized park edit sequence with synthetic guests reaches equals its "
          "save loaded and resolved, and the two stay equal as they step on") {
  std::optional<World> loaded;
  bool sawQueueSaved = false;
  runServiceSequence(
      56,
      [&](const World & /*world*/, const Cycle &cycle) {
        if (!cycle.Edit.has_value()) {
          REQUIRE(loaded.has_value());
          static_cast<void>(takeTurn(loaded.value(), cycle.Turn));
          stepWorld(loaded.value());
        }
      },
      [&](const World &world, bool edited) {
        if (edited) {
          loaded.emplace(loadWorld(makeParkSchema(), saveWorld(world)));
          resolveWorld(loaded.value());
          for (const ParkBox &box : parkBoxes(world)) {
            sawQueueSaved = sawQueueSaved || (box.Kind == BoxKind::Shop &&
                                              !stockOf<GuestVisits>(world, box.Key).empty());
          }
        }
        REQUIRE(worldsEqual(loaded.value(), world));
      });
  CHECK(sawQueueSaved);
}

TEST_CASE("A candidate made with an edit from a world of a randomized park edit sequence with "
          "synthetic guests, once it has stepped a cycle, equals the world that queues the edit "
          "for that cycle") {
  std::optional<World> candidate;
  bool sawChange = false;
  int compared = 0;
  runServiceSequence(
      59,
      [&](const World &world, const Cycle &cycle) {
        if (cycle.Edit.has_value()) {
          World previewed = copyWorld(world);
          stepWorld(previewed);
          CommandQueue queue;
          queueEdit(queue, cycle.Edit.value());
          candidate.emplace(makeCandidate(previewed, queue));
          sawChange = sawChange || !worldsEqual(candidate.value(), previewed);
        }
      },
      [&](const World &world, bool edited) {
        if (edited && candidate.has_value()) {
          REQUIRE(worldsEqual(candidate.value(), world));
          candidate.reset();
          ++compared;
        }
      });
  CHECK(compared == EDITS);
  CHECK(sawChange);
}

} // namespace
} // namespace tpj
