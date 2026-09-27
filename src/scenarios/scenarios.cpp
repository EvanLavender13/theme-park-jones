#include "scenarios/scenarios.h"
#include "scenarios/synthetic.h"

#include <array>

namespace tpj {

std::span<const Scenario> registeredScenarios() {
  static const std::array<Scenario, 2> SCENARIOS = {walkersScenario(), beaconsScenario()};
  return SCENARIOS;
}

} // namespace tpj
