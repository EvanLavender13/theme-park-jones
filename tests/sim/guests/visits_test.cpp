#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::recordOf;

// The units of guest-visits addressed to the guest, in transit and held.
int64_t visitsAddressedTo(const World &world, EntityKey guest) {
  const FlowAddressed addressed = addressedTo<GuestVisits>(world, guest);
  int64_t units = 0;
  for (const FlowPacket &packet : addressed.Packets) {
    units += packet.Units;
  }
  for (const FlowStock &stock : addressed.Stocks) {
    units += stock.Units;
  }
  return units;
}

// What the checks read of a guest before a cycle.
struct GuestBefore {
  GuestRecord Record;
  int64_t VisitsHeld = 0;
  int64_t MealsHeld = 0;
  int64_t VisitsAddressed = 0;
};

// What the checks read of the world before a cycle: the tick it steps, its guests, the offers,
// and the ledgers' counts.
struct BeforeCycle {
  uint64_t Tick = 0;
  std::map<EntityKey, GuestBefore> Guests;
  test::ParkOffers Offers;
  int64_t VisitsCreated = 0;
  int64_t VisitsFinished = 0;
  int64_t MealsEaten = 0;
};

BeforeCycle beforeCycle(const World &world) {
  BeforeCycle before{.Tick = world.Tick,
                     .Guests = {},
                     .Offers = test::parkOffers(world),
                     .VisitsCreated = unitsCreated<GuestVisits>(world),
                     .VisitsFinished = unitsConsumed<GuestVisits>(world, FINISHED_CAUSE),
                     .MealsEaten = unitsConsumed<Meals>(world, EATEN_CAUSE)};
  for (const EntityKey guest : parkGuests(world)) {
    before.Guests.emplace(guest,
                          GuestBefore{.Record = recordOf(world, guest),
                                      .VisitsHeld = unitsHeld<GuestVisits>(world, guest, guest),
                                      .MealsHeld = unitsHeld<Meals>(world, guest, guest),
                                      .VisitsAddressed = visitsAddressedTo(world, guest)});
  }
  return before;
}

bool isWaiting(const World &world, EntityKey guest) {
  return recordOf(world, guest).Activity == GuestActivity::Waiting;
}

bool anyWaiting(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::any_of(guests, [&](EntityKey guest) { return isWaiting(world, guest); });
}

// Whether a guest is waiting in the cycle stepping the tick at or after its StayUntil.
bool anyWaitingPastStay(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::any_of(guests, [&](EntityKey guest) {
    return isWaiting(world, guest) && world.Tick >= recordOf(world, guest).StayUntil;
  });
}

// Whether a shop holds supplies and visits.
bool anyShopServing(const World &world) {
  const std::vector<ParkBox> boxes = parkBoxes(world);
  return std::ranges::any_of(boxes, [&](const ParkBox &box) {
    const std::optional<ShopRecord> record = shopRecord(world, box.Key);
    return record.has_value() && record.value_or(ShopRecord{}).Stock > 0 &&
           record.value_or(ShopRecord{}).Queue > 0;
  });
}

std::optional<ParkBox> depotOf(const World &world) {
  const std::vector<ParkBox> boxes = parkBoxes(world);
  const auto found = std::ranges::find(boxes, BoxKind::Depot, &ParkBox::Kind);
  return found == boxes.end() ? std::nullopt : std::optional<ParkBox>(*found);
}

std::optional<EntityKey> backstageOf(const World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  const auto found = std::ranges::find(paths, PathKind::Backstage, &ParkPath::Kind);
  return found == paths.end() ? std::nullopt : std::optional<EntityKey>(found->Key);
}

std::size_t guestPathCount(const World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  return static_cast<std::size_t>(std::ranges::count(paths, PathKind::Guest, &ParkPath::Kind));
}

// The stall run, on the eating park. The cycle stepping CLOSE_GATE deletes the gate path, so the
// park admits no more guests than the six it has and steps quickly. Every CHURN_PERIOD cycles the
// depot is deleted and a new one added at its pose. An order takes ORDER_DELAY to reach a depot,
// longer than one lives, so no order is filled, and the shops, supplied but holding no supplies,
// keep every visit they take: their guests wait on, past the end of their stays. The first cycle
// from UNSERVED_CUT with a guest waiting deletes the backstage path, so the shops, starved with
// nothing to cover their queues, send every visit back unserved, and the next draws the path again.
// The cycle stepping JOIN adds a guest path from the spine's end to the cross's east end, closing a
// loop that changes route distance. Once a guest waits past its stay, the depot is left alone and
// the shops' orders are filled. The first cycle in which a shop then holds supplies and visits
// deletes the backstage path again, so the offers say no meals while the shops serve the guests
// their stock covers, and the run ends SERVING_CYCLES later.
constexpr uint64_t CLOSE_GATE = 6 * ARRIVAL_INTERVAL;
constexpr uint64_t CHURN_PERIOD = 20;
static_assert(CHURN_PERIOD < ORDER_DELAY);
constexpr uint64_t UNSERVED_CUT = 600;
constexpr uint64_t JOIN = 1200;
constexpr uint64_t SERVING_CYCLES = 300;
constexpr uint64_t LAST_CYCLE = CLOSE_GATE + STAY_MAX + 1000;

// The edits a stall-run cycle queued, with the depot and the guest path count before it.
struct StallEdits {
  EntityKey Depot = NULL_KEY;
  std::size_t GuestPaths = 0;
  bool Churned = false;
  bool Cutting = false;
  bool Restoring = false;
  bool Closing = false;
  bool Joining = false;
};

// Requires that each edit the cycle queued was applied.
void requireEditsLanded(const StallEdits &edits, const World &world) {
  if (edits.Churned) {
    REQUIRE(depotOf(world).has_value());
    REQUIRE(depotOf(world).value_or(ParkBox{}).Key != edits.Depot);
  }
  if (edits.Cutting) {
    REQUIRE_FALSE(backstageOf(world).has_value());
  }
  if (edits.Restoring) {
    REQUIRE(backstageOf(world).has_value());
  }
  if (edits.Closing) {
    REQUIRE(guestPathCount(world) == edits.GuestPaths - 1);
  }
  if (edits.Joining) {
    REQUIRE(guestPathCount(world) == edits.GuestPaths + 1);
  }
}

// Where the stall run stands: the depot's pose, whether the depot is still being replaced, which
// backstage cuts it has made, and the tick it ends.
struct StallState {
  Pose DepotPose;
  bool Churning = true;
  bool CutUnserved = false;
  bool Restored = false;
  bool CutServing = false;
  uint64_t End = LAST_CYCLE;
};

// Queues the stall run's edits for the cycle about to step, and gives what they are.
StallEdits queueStallEdits(const World &world, StallState &state, CommandQueue &queue) {
  StallEdits edits;
  edits.Depot = depotOf(world).value_or(ParkBox{}).Key;
  edits.GuestPaths = guestPathCount(world);
  edits.Churned = state.Churning && world.Tick > 0 && world.Tick % CHURN_PERIOD == 0;
  if (edits.Churned) {
    queue.push(DeleteBox{edits.Depot});
    queue.push(AddBox{BoxKind::Depot, state.DepotPose});
  }
  const bool cutsUnserved = !state.CutUnserved && world.Tick >= UNSERVED_CUT && anyWaiting(world);
  const bool cutsServing = !state.Churning && !state.CutServing && anyShopServing(world);
  edits.Cutting = cutsUnserved || cutsServing;
  edits.Restoring = state.CutUnserved && !state.Restored;
  state.CutUnserved = state.CutUnserved || cutsUnserved;
  state.CutServing = state.CutServing || cutsServing;
  if (cutsServing) {
    state.End = world.Tick + SERVING_CYCLES;
  }
  if (edits.Cutting) {
    const std::optional<EntityKey> backstage = backstageOf(world);
    REQUIRE(backstage.has_value());
    queue.push(DeletePath{backstage.value_or(NULL_KEY)});
  } else if (edits.Restoring) {
    queue.push(AddPath{PathKind::Backstage, test::eatingBackstagePoints()});
    state.Restored = true;
  }
  edits.Closing = world.Tick == CLOSE_GATE;
  if (edits.Closing) {
    queue.push(DeletePath{test::EATING_GATE_PATH});
  }
  edits.Joining = world.Tick == JOIN;
  if (edits.Joining) {
    queue.push(AddPath{PathKind::Guest, {{0.0, 60.0}, {20.0, 70.0}}});
  }
  if (state.Churning && state.Restored && anyWaitingPastStay(world)) {
    state.Churning = false;
  }
  return edits;
}

// Steps the stall run, handing check what it read of the world before each cycle and the world
// after it.
template <typename Check> void runStallPark(Check check) {
  World world = test::eatingWorld();
  const std::optional<ParkBox> firstDepot = depotOf(world);
  REQUIRE(firstDepot.has_value());
  StallState state;
  state.DepotPose = firstDepot.value_or(ParkBox{}).At;
  while (world.Tick < state.End) {
    INFO("stepping " << world.Tick);
    CommandQueue queue;
    const StallEdits edits = queueStallEdits(world, state, queue);
    const BeforeCycle before = beforeCycle(world);
    stepWorld(world, queue);
    requireEditsLanded(edits, world);
    check(before, world);
  }
  REQUIRE(state.CutServing);
}

TEST_CASE("A guest creates guest-visits units only in a cycle that ends with it waiting at its "
          "Target's guest anchor, and then exactly one, under its own key as handle, which its "
          "Target holds at the cycle's end") {
  CHECK(VISIT_DELAY == 1);
  int sent = 0;
  runStallPark([&](const BeforeCycle &before, const World &after) {
    int64_t senders = 0;
    for (const EntityKey guest : parkGuests(after)) {
      const GuestRecord now = recordOf(after, guest);
      const auto was = before.Guests.find(guest);
      // A guest ends the cycle waiting afresh when it was not waiting, or when its visit came back
      // and it chose the same shop again at its anchor.
      const bool waitsAfresh =
          now.Activity == GuestActivity::Waiting &&
          (was == before.Guests.end() || was->second.Record.Activity != GuestActivity::Waiting ||
           was->second.VisitsHeld > 0);
      if (!waitsAfresh) {
        continue;
      }
      CAPTURE(guest);
      ++senders;
      CHECK(now.At == test::anchorPlace(after, now.Target));
      const FlowAddressed addressed = addressedTo<GuestVisits>(after, guest);
      CHECK(addressed.Packets.empty());
      REQUIRE(addressed.Stocks.size() == 1);
      CHECK(addressed.Stocks.front().Endpoint == now.Target);
      CHECK(addressed.Stocks.front().Units == 1);
    }
    CHECK(unitsCreated<GuestVisits>(after) - before.VisitsCreated == senders);
    sent += static_cast<int>(senders);
  });
  CHECK(sent > 0);
}

TEST_CASE("No guest ever has more than one guest-visits unit addressed to it") {
  bool sawOutstanding = false;
  runStallPark([&](const BeforeCycle & /*before*/, const World &after) {
    for (const EntityKey guest : parkGuests(after)) {
      CAPTURE(guest);
      const int64_t units = visitsAddressedTo(after, guest);
      CHECK(units <= 1);
      sawOutstanding = sawOutstanding || units == 1;
    }
  });
  CHECK(sawOutstanding);
}

TEST_CASE("A waiting guest whose visit has not come back keeps its place, Activity, and Target, "
          "and sends nothing, whatever its Target's offer, route distance, or stay") {
  bool sawNoMeals = false;
  bool sawStayOver = false;
  bool sawAfterJoin = false;
  runStallPark([&](const BeforeCycle &before, const World &after) {
    const uint64_t tick = before.Tick;
    for (const auto &[guest, was] : before.Guests) {
      if (was.Record.Activity != GuestActivity::Waiting || was.VisitsHeld > 0) {
        continue;
      }
      CAPTURE(guest);
      // No edit in the stall run moves a shop's guest anchor, so every waiting guest's place
      // resolves.
      const std::optional<GuestRecord> now = guestRecord(after, guest);
      REQUIRE(now.has_value());
      const GuestRecord record = now.value_or(GuestRecord{});
      CHECK(record.At == was.Record.At);
      CHECK(record.Activity == GuestActivity::Waiting);
      CHECK(record.Target == was.Record.Target);
      CHECK(visitsAddressedTo(after, guest) == was.VisitsAddressed);
      sawNoMeals =
          sawNoMeals ||
          !test::offerAmong(before.Offers, was.Record.Target).value_or(OfferEntry{}).Supplied;
      sawStayOver = sawStayOver || tick >= was.Record.StayUntil;
      sawAfterJoin = sawAfterJoin || tick > JOIN;
    }
  });
  CHECK(sawNoMeals);
  CHECK(sawStayOver);
  CHECK(sawAfterJoin);
}

TEST_CASE("A waiting guest holding guest-visits units under its own key consumes them all as "
          "finished") {
  bool sawServed = false;
  bool sawUnserved = false;
  runStallPark([&](const BeforeCycle &before, const World &after) {
    int64_t returned = 0;
    for (const auto &[guest, was] : before.Guests) {
      if (was.Record.Activity != GuestActivity::Waiting || was.VisitsHeld == 0) {
        continue;
      }
      CAPTURE(guest);
      returned += was.VisitsHeld;
      CHECK(unitsHeld<GuestVisits>(after, guest, guest) == 0);
      const bool served = was.MealsHeld > 0;
      sawServed = sawServed || served;
      sawUnserved = sawUnserved || !served;
    }
    CHECK(unitsConsumed<GuestVisits>(after, FINISHED_CAUSE) - before.VisitsFinished == returned);
  });
  CHECK(sawServed);
  CHECK(sawUnserved);
}

TEST_CASE("A served guest consumes its one meal as eaten, its MealsEaten rises by 1, and its "
          "LastMeal gives the tick, its Hunger after the cycle's rise, and that less MEAL_RELIEF "
          "but at least 0, which is its Hunger after the cycle") {
  // Each guest's hunger rate, drawn in the cycle that admitted it.
  std::map<EntityKey, double> rates;
  int served = 0;
  runStallPark([&](const BeforeCycle &before, const World &after) {
    const uint64_t tick = before.Tick;
    int64_t eaten = 0;
    int64_t rose = 0;
    for (const auto &[guest, was] : before.Guests) {
      const std::optional<GuestRecord> now = guestRecord(after, guest);
      const bool fed =
          now.has_value() && now.value_or(GuestRecord{}).MealsEaten > was.Record.MealsEaten;
      rose += fed ? 1 : 0;
      if (was.Record.Activity != GuestActivity::Waiting || was.VisitsHeld == 0) {
        CHECK_FALSE(fed);
        continue;
      }
      const int64_t meals = was.MealsHeld;
      CAPTURE(guest, meals);
      REQUIRE(now.has_value());
      const GuestRecord record = now.value_or(GuestRecord{});
      if (meals == 0) {
        CHECK(record.MealsEaten == was.Record.MealsEaten);
        CHECK(record.LastMeal == was.Record.LastMeal);
        continue;
      }
      REQUIRE(rates.contains(guest));
      eaten += meals;
      ++served;
      const double hungerBefore = std::min(1.0, was.Record.Hunger + rates.at(guest));
      const double hungerAfter = std::max(0.0, hungerBefore - MEAL_RELIEF);
      CHECK(unitsHeld<Meals>(after, guest, guest) == 0);
      CHECK(record.MealsEaten == was.Record.MealsEaten + 1);
      CHECK(record.LastMeal ==
            GuestMeal{.Tick = tick, .Before = hungerBefore, .After = hungerAfter});
      CHECK(record.Hunger == hungerAfter);
      CHECK(hungerAfter < hungerBefore);
    }
    const int64_t consumed = unitsConsumed<Meals>(after, EATEN_CAUSE) - before.MealsEaten;
    CHECK(consumed == eaten);
    CHECK(consumed == rose);
    for (const EntityKey guest : parkGuests(after)) {
      if (!rates.contains(guest)) {
        rates.emplace(guest, test::drawnHungerRate(after, guest, tick));
      }
    }
  });
  CHECK(served > 0);
}

} // namespace
} // namespace tpj
