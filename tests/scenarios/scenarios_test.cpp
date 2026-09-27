#include "support/scenario_world.h"

#include "scenarios/scenarios.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <stdint.h>
#include <string_view>

namespace tpj {
namespace {

bool isNameCharacter(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
}

TEST_CASE("Registered scenario names are distinct lowercase letters, digits, and hyphens") {
  REQUIRE_FALSE(registeredScenarios().empty());
  std::set<std::string_view> names;
  for (const Scenario &scenario : registeredScenarios()) {
    INFO("scenario " << scenario.Name);
    CHECK_FALSE(scenario.Name.empty());
    for (const char ch : scenario.Name) {
      CHECK(isNameCharacter(ch));
    }
    CHECK(names.insert(scenario.Name).second);
  }
}

// A scenario whose systems draw gives different worlds for different seeds, beyond the seed itself,
// which is what makes the cross-build check cover the draws made while stepping.
TEST_CASE("Some registered scenario's worlds differ by more than their seed after 100 cycles") {
  constexpr int CYCLES = 100;
  bool anyDiffers = false;
  for (const Scenario &scenario : registeredScenarios()) {
    World first = test::startScenarioWorld(scenario, scenario.Seed);
    World second = test::startScenarioWorld(scenario, scenario.Seed + 1);
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
      test::stepScenarioWorld(scenario, first);
      test::stepScenarioWorld(scenario, second);
    }
    second.Seed = first.Seed;
    if (!worldsEqual(first, second)) {
      anyDiffers = true;
    }
  }
  CHECK(anyDiffers);
}

} // namespace
} // namespace tpj
