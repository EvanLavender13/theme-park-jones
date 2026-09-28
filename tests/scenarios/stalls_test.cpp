#include "support/scenario_world.h"

#include "scenarios/scenarios.h"
#include "scenarios/stalls.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

namespace tpj {
namespace {

const Scenario *findStalls() {
  const auto scenarios = registeredScenarios();
  const auto found = std::ranges::find_if(
      scenarios, [](const Scenario &scenario) { return scenario.Name == "stalls"; });
  return found == scenarios.end() ? nullptr : &*found;
}

template <typename Entry> bool anyEntries(const std::vector<FieldSlot<Entry>> &slots) {
  return std::ranges::any_of(slots,
                             [](const FieldSlot<Entry> &slot) { return !slot.Entries.empty(); });
}

// Fields are public medium components, so a test may read both layers directly.
template <FieldDefinition F> bool holdsResolvedEntries(const World &world) {
  const entt::entity holder = world.findEntity(fieldKey(F::Name));
  const auto *resolved =
      holder == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(holder);
  return resolved != nullptr && anyEntries(resolved->Slots);
}

template <FieldDefinition F> bool holdsReadableSteppedEntries(const World &world) {
  const entt::entity holder = world.findEntity(fieldKey(F::Name));
  const auto *stepped =
      holder == entt::null ? nullptr : world.Registry.try_get<SteppedEntries<F>>(holder);
  return stepped != nullptr && anyEntries(stepped->Readable);
}

template <FlowDefinition K> bool conserved(const World &world) {
  return unitsCreated<K>(world) ==
         unitsInTransit<K>(world) + unitsHeld<K>(world) + unitsConsumed<K>(world);
}

TEST_CASE("tpj_scenarios registers a scenario named stalls") { REQUIRE(findStalls() != nullptr); }

// What stalls must exercise for the cross-build check to cover the medium.
struct Exercised {
  bool OfferResolved = false;
  bool OfferStepped = false;
  bool CrowdResolved = false;
  bool CrowdStepped = false;
  bool GoodsInTransit = false;
  bool GoodsConsumed = false;
  bool VisitsInTransit = false;
  bool VisitsConsumed = false;

  void observe(const World &world) {
    OfferResolved = OfferResolved || holdsResolvedEntries<StallOffer>(world);
    OfferStepped = OfferStepped || holdsReadableSteppedEntries<StallOffer>(world);
    CrowdResolved = CrowdResolved || holdsResolvedEntries<Crowd>(world);
    CrowdStepped = CrowdStepped || holdsReadableSteppedEntries<Crowd>(world);
    GoodsInTransit = GoodsInTransit || unitsInTransit<Goods>(world) > 0;
    GoodsConsumed = GoodsConsumed || unitsConsumed<Goods>(world) > 0;
    VisitsInTransit = VisitsInTransit || unitsInTransit<Visits>(world) > 0;
    VisitsConsumed = VisitsConsumed || unitsConsumed<Visits>(world) > 0;
  }
};

TEST_CASE("in its first 3000 cycles, stalls holds both layers of both fields and units of both "
          "kinds in transit and consumed, and conserves each kind after every cycle") {
  const Scenario *stalls = findStalls();
  REQUIRE(stalls != nullptr);
  World world = test::startScenarioWorld(*stalls, stalls->Seed);
  Exercised exercised;

  for (int cycle = 0; cycle < 3000; ++cycle) {
    test::stepScenarioWorld(*stalls, world);
    if (!conserved<Goods>(world) || !conserved<Visits>(world)) {
      CAPTURE(world.Tick);
      REQUIRE(conserved<Goods>(world));
      REQUIRE(conserved<Visits>(world));
    }
    exercised.observe(world);
  }

  CHECK(exercised.OfferResolved);
  CHECK(exercised.OfferStepped);
  CHECK(exercised.CrowdResolved);
  CHECK(exercised.CrowdStepped);
  CHECK(exercised.GoodsInTransit);
  CHECK(exercised.GoodsConsumed);
  CHECK(exercised.VisitsInTransit);
  CHECK(exercised.VisitsConsumed);
}

} // namespace
} // namespace tpj
