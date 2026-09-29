#include "support/scenario_world.h"

#include "scenarios/scenarios.h"
#include "sim/entity_key.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// The runner's default run.
constexpr int CYCLES = 3000;

const Scenario *findScenario(std::string_view name) {
  const auto scenarios = registeredScenarios();
  const auto found = std::ranges::find_if(
      scenarios, [name](const Scenario &scenario) { return scenario.Name == name; });
  return found == scenarios.end() ? nullptr : &*found;
}

template <FlowDefinition K> bool conserved(const World &world) {
  return unitsCreated<K>(world) ==
         unitsInTransit<K>(world) + unitsHeld<K>(world) + unitsConsumed<K>(world);
}

std::vector<EntityKey> shopKeys(const World &world) {
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  return shops;
}

// Whether some guest holds a visit a shop sent back with no meal. Only guests hold visits outside
// shops, and a served visit reaches its guest at the same swap as its meal.
bool someVisitReturnedWithoutMeal(const World &world) {
  const Ledger *visits = ledgerOf<GuestVisits>(world);
  REQUIRE(visits != nullptr);
  const std::vector<EntityKey> shops = shopKeys(world);
  return std::ranges::any_of(visits->Stocks, [&](const FlowStock &stock) {
    return std::ranges::find(shops, stock.Endpoint) == shops.end() &&
           unitsHeld<Meals>(world, stock.Endpoint, stock.Handle) == 0;
  });
}

// What food-shop did over the runner's default run.
struct Run {
  std::vector<int> UnconservedCycles;
  bool ReturnedWithoutMeal = false;
  int64_t MealsCreated = 0;
  int64_t VisitsAbandoned = 0;
};

const Run &foodShopRun() {
  static const Run RUN = [] {
    Run run;
    const Scenario *scenario = findScenario("food-shop");
    if (scenario == nullptr) {
      return run;
    }
    World world = test::startScenarioWorld(*scenario, scenario->Seed);
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
      test::stepScenarioWorld(*scenario, world);
      if (!conserved<SupplyOrders>(world) || !conserved<Supplies>(world) ||
          !conserved<GuestVisits>(world) || !conserved<Meals>(world)) {
        run.UnconservedCycles.push_back(cycle);
      }
      run.ReturnedWithoutMeal = run.ReturnedWithoutMeal || someVisitReturnedWithoutMeal(world);
    }
    run.MealsCreated = unitsCreated<Meals>(world);
    run.VisitsAbandoned = unitsConsumed<GuestVisits>(world, ABANDONED_CAUSE);
    return run;
  }();
  return RUN;
}

TEST_CASE("registeredScenarios lists food-shop after park-edits") {
  const auto scenarios = registeredScenarios();
  const auto indexOf = [&](std::string_view name) {
    return std::distance(scenarios.begin(),
                         std::ranges::find_if(scenarios, [name](const Scenario &scenario) {
                           return scenario.Name == name;
                         }));
  };
  const auto parkEdits = indexOf("park-edits");
  const auto foodShop = indexOf("food-shop");
  REQUIRE(static_cast<std::size_t>(parkEdits) < scenarios.size());
  REQUIRE(static_cast<std::size_t>(foodShop) < scenarios.size());
  CHECK(foodShop > parkEdits);
}

TEST_CASE("food-shop's world starts with a shop that reaches a depot over a backstage path and a "
          "shop that reaches none") {
  const Scenario *scenario = findScenario("food-shop");
  REQUIRE(scenario != nullptr);
  const World world = test::startScenarioWorld(*scenario, scenario->Seed);
  bool supplied = false;
  bool starved = false;
  for (const EntityKey shop : shopKeys(world)) {
    const bool reaches = nearestDepot(world, shop).has_value();
    supplied = supplied || reaches;
    starved = starved || !reaches;
  }
  CHECK(supplied);
  CHECK(starved);
}

TEST_CASE("In food-shop's default run, each of supply-orders, supplies, guest-visits, and meals "
          "satisfies the ledger's identity after every cycle") {
  REQUIRE(findScenario("food-shop") != nullptr);
  CHECK(foodShopRun().UnconservedCycles.empty());
}

TEST_CASE("In food-shop's default run, shops create meals") {
  REQUIRE(findScenario("food-shop") != nullptr);
  CHECK(foodShopRun().MealsCreated > 0);
}

TEST_CASE("In food-shop's default run, shops return visits without meals") {
  REQUIRE(findScenario("food-shop") != nullptr);
  CHECK(foodShopRun().ReturnedWithoutMeal);
}

TEST_CASE("In food-shop's default run, shops consume visits as abandoned") {
  REQUIRE(findScenario("food-shop") != nullptr);
  CHECK(foodShopRun().VisitsAbandoned > 0);
}

} // namespace
} // namespace tpj
