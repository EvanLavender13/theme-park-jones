#include "scenarios/scenarios.h"
#include "scenarios/synthetic.h"

#include <array>

namespace tpj {

std::span<const Scenario> registeredScenarios() {
  static const std::array<Scenario, 3> SCENARIOS = {walkersScenario(), beaconsScenario(),
                                                    stallsScenario()};
  return SCENARIOS;
}

} // namespace tpj
