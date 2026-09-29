#include "scenarios/scenarios.h"
#include "scenarios/synthetic.h"

#include <array>

namespace tpj {

std::span<const Scenario> registeredScenarios() {
  static const std::array<Scenario, 5> SCENARIOS = {walkersScenario(), beaconsScenario(),
                                                    stallsScenario(), parkEditsScenario(),
                                                    foodShopScenario()};
  return SCENARIOS;
}

} // namespace tpj
