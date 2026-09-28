#include "scenarios/scenarios.h"
#include "scenarios/synthetic.h"

#include <array>

namespace tpj {

std::span<const Scenario> registeredScenarios() {
  static const std::array<Scenario, 4> SCENARIOS = {walkersScenario(), beaconsScenario(),
                                                    stallsScenario(), parkEditsScenario()};
  return SCENARIOS;
}

} // namespace tpj
